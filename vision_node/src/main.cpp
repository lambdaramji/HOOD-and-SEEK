#include "vision.hpp"
#include "link.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#ifdef HOODSEEK_ECOAI
// The ATLAS device image only ships opencv-core/imgproc; videoio and
// imgcodecs need GStreamer/FFmpeg/libgphoto2/libtiff, none of which are
// installed, so this build reads the camera via V4L2 and drops the
// file-codec-only CLI modes instead of linking those two modules.
#include "ecoai_detector.hpp"
#include "latest_frame.hpp"
#include "preview.hpp"
#include "v4l2_camera.hpp"
#else
#include <opencv2/imgcodecs.hpp>
#endif

namespace {
using namespace hoodseek;
// steady_clock is CLOCK_MONOTONIC on Linux. The Pi wall clock is wrong without NTP (design §1.2).
using Clock = std::chrono::steady_clock;
volatile std::sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
int64_t now_ms() { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count(); }
int number(const std::string& s, int low, int high) {
  size_t consumed = 0; int n = std::stoi(s, &consumed);
  if (consumed != s.size() || n < low || n > high) throw std::runtime_error("invalid integer option: " + s);
  return n;
}
Json fault(const std::string& reason) { return {{"event", "fault"}, {"reason", reason}}; }
const char* usage = "hoodseek_vision --camera /dev/video0 --model yolox.tflite [--backend cpu|gpu] [--threads 2] [--rotate 0|180] [--config config.json] [--output observations.jsonl] [--dump-dir DIR --dump-every N] [--max-frames N] [--stall-ms 10000] [--core IPv4:7420 --roi config/roi.json] [--preview-port 7431]\n"
                    "hoodseek_vision --replay proposals.jsonl|- [--config config.json] [--output observations.jsonl] [--max-frames N] [--core IPv4:7420 --roi roi.json]   (PC test path)\n"
                    "hoodseek_vision --export-input photo.jpg --input-width 640 --input-height 640 --tensor-output rgb.f32 [--layout nhwc|nchw] [--resize-mode stretch|letterbox]   (PC only)\n";
}

int main(int argc, char** argv) {
  using namespace hoodseek;
  std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
  std::ofstream file;
  std::ostream* output = &std::cout;
  std::mutex output_mutex;
  std::map<std::string, std::string> options;
  std::string kind;
  auto emit = [&](const Json& event) {
    std::lock_guard<std::mutex> lock(output_mutex);
    *output << event.dump() << '\n'; output->flush();
    if (!*output) throw std::runtime_error("cannot write observation output");
  };
  Config configuration;
  // One record per inference with raw per-class best candidates. No state carries over between frames.
  auto record = [&](const Frame& frame, const std::vector<Proposal>& found, const Json& infer_ms) {
    return Json{{"seq", frame.sequence}, {"t_ms", frame.timestamp_ms}, {"frame_w", frame.width}, {"frame_h", frame.height},
                {"infer_ms", infer_ms}, {"best", best_by_class(found, configuration.watch_classes, frame.width, frame.height)}};
  };
  std::atomic<bool> finished{false};
  std::atomic<int64_t> progress{now_ms()};
  std::thread watchdog;
  auto finish_watchdog = [&] { finished = true; if (watchdog.joinable()) watchdog.join(); };
  try {
    const std::set<std::string> allowed{"--replay", "--camera", "--model", "--backend", "--threads", "--config", "--output", "--stall-ms", "--export-input",
                                        "--input-width", "--input-height", "--tensor-output", "--layout", "--resize-mode", "--rotate", "--dump-dir", "--dump-every", "--max-frames", "--core", "--roi", "--preview-port"};
    for (int i = 1; i < argc; ++i) {
      std::string key = argv[i];
      if (key == "--help") { std::cout << usage; return 0; }
      if (!allowed.count(key) || i + 1 >= argc || options.count(key)) throw std::runtime_error("unknown, duplicate or incomplete option: " + key);
      options[key] = argv[++i];
    }
    int modes = 0;
    for (const auto* key : {"--replay", "--camera", "--export-input"}) if (options.count(key)) { ++modes; kind = std::string(key).substr(2); }
    if (modes != 1) throw std::runtime_error("choose exactly one input mode; use --help");
    if (options.count("--resize-mode") && kind != "export-input") throw std::runtime_error("--resize-mode is for tensor export; set resize_mode in the runtime config");
    if (options.count("--backend") && options.at("--backend") != "cpu" && options.at("--backend") != "gpu") throw std::runtime_error("backend must be cpu or gpu");
    if (options.count("--rotate") && options.at("--rotate") != "0" && options.at("--rotate") != "180") throw std::runtime_error("rotate must be 0 or 180");
    for (const auto* key : {"--rotate", "--dump-dir", "--dump-every", "--model", "--preview-port"})
      if (options.count(key) && kind != "camera") throw std::runtime_error(std::string(key) + " applies to --camera only");
    if (options.count("--dump-dir") != options.count("--dump-every")) throw std::runtime_error("--dump-dir and --dump-every go together");
    // roi_overlap is part of every vision message, so publishing without a calibrated ROI is refused.
    if (options.count("--core") && !options.count("--roi")) throw std::runtime_error("--core requires --roi (config/roi.json from tools/roi_click.py)");
    if (options.count("--output")) {
      // Avoid destroying the input/model/config through an accidentally reused path.
      for (const auto* key : {"--replay", "--camera", "--model", "--config"})
        if (options.count(key) && std::filesystem::exists(options.at(key)) && std::filesystem::exists(options.at("--output")) && std::filesystem::equivalent(options.at(key), options.at("--output"))) throw std::runtime_error("output must differ from input files");
      file.open(options.at("--output")); if (!file) throw std::runtime_error("cannot open output file"); output = &file;
    }
    if (options.count("--config")) {
      std::ifstream config(options.at("--config")); if (!config) throw std::runtime_error("cannot open config");
      Json j; config >> j; configuration = Config::from_json(j);
    }
    std::optional<Roi> roi;
    if (options.count("--roi")) {
      std::ifstream in(options.at("--roi")); if (!in) throw std::runtime_error("cannot open ROI file");
      Json j; in >> j; roi = load_roi(j);
    }
    std::unique_ptr<CoreLink> link;
    if (options.count("--core")) link = std::make_unique<CoreLink>(options.at("--core"));
    if (kind == "export-input") {
#ifdef HOODSEEK_ECOAI
      throw std::runtime_error("--export-input needs an image codec library this ATLAS build does not link; run it on a PC build instead");
#else
      if (options.count("--resize-mode")) configuration.resize_mode = options.at("--resize-mode");
      int w = number(options.at("--input-width"), 1, 4096), h = number(options.at("--input-height"), 1, 4096);
      std::string layout = options.count("--layout") ? options.at("--layout") : "nhwc";
      if (layout != "nhwc" && layout != "nchw") throw std::runtime_error("layout must be nhwc or nchw");
      const auto& path = options.at("--tensor-output");
      if (std::filesystem::exists(path) && std::filesystem::equivalent(path, options.at("--export-input"))) throw std::runtime_error("tensor output must differ from image");
      cv::Mat image = cv::imread(options.at("--export-input"), cv::IMREAD_COLOR | cv::IMREAD_IGNORE_ORIENTATION);
      auto data = preprocess(image, w, h, layout == "nchw", configuration.resize_mode);
      std::ofstream tensor(path, std::ios::binary); tensor.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(float)));
      if (!tensor) throw std::runtime_error("cannot write float input");
      std::cout << Json{{"width", image.cols}, {"height", image.rows}, {"model_width", w}, {"model_height", h}, {"resize_mode", configuration.resize_mode}}.dump() << '\n'; return 0;
#endif
    }
    int timeout = number(options.count("--stall-ms") ? options.at("--stall-ms") : "10000", 200, 120000);
    // 0 = run until SIGINT/SIGTERM. Measurement runs (#49) use a fixed count per condition.
    const int64_t max_frames = options.count("--max-frames") ? number(options.at("--max-frames"), 1, 1000000) : 0;
    bool reached_max = false;
    // A blocked camera read or inference must end the process instead of going silent forever;
    // SafetyCore then expires vision after 2 s (design §2.4, F7) and systemd restarts the node.
    // Started once the source is open, so model load and camera setup do not count against the limit.
    auto start_watchdog = [&] {
      progress = now_ms();
      watchdog = std::thread([&, timeout] {  // timeout by value: it is a try-block local the catch path outlives
        while (!finished) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          if (!finished && now_ms() - progress.load() > timeout) {
            // Any write can block (a full pipe or a stalled ssh channel carries stdout and stderr alike),
            // so the record is left to a helper thread and the exit never waits more than 200 ms for it.
            std::thread([&] {
              std::fputs("hoodseek_vision: source_stalled\n", stderr);
              if (output_mutex.try_lock()) try { *output << fault("source_stalled").dump() << '\n'; output->flush(); } catch (...) {}
            }).detach();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::_Exit(3);
          }
        }
      });
    };
    if (kind == "replay") {
      start_watchdog();  // before the first write: the start event itself can block
      emit({{"event", "start"}, {"input", "replay"}});
      std::ifstream replay_file;
      std::istream* input = &std::cin;
      if (options.at("--replay") != "-") { replay_file.open(options.at("--replay")); if (!replay_file) throw std::runtime_error("cannot open replay input"); input = &replay_file; }
      std::string line;
      int64_t processed = 0;
      while (!stopping && std::getline(*input, line)) {
        if (line.empty()) continue;
        if (line.size() > 8 * 1024 * 1024) throw std::runtime_error("replay line too large");
        auto frame = Frame::from_json(Json::parse(line));
        auto found = postprocess(frame, configuration);
        emit(record(frame, found, nullptr));
        if (link) link->publish(vision_payload(frame, found, 0, configuration.cat_classes, *roi), now_ms());
        progress = now_ms();
        if (max_frames && ++processed >= max_frames) { reached_max = true; break; }
      }
      if (input->bad()) throw std::runtime_error("replay read failed");
    } else {
#ifdef HOODSEEK_ECOAI
      if (!options.count("--model")) throw std::runtime_error("--camera requires --model");
      const auto model_path = std::filesystem::canonical(options.at("--model")).string();
      const auto backend = options.count("--backend") ? options.at("--backend") : "cpu";
      int threads = number(options.count("--threads") ? options.at("--threads") : "2", 1, 16);
      bool rotate180 = options.count("--rotate") && options.at("--rotate") == "180";
      if (roi && roi->rotate != (rotate180 ? 180 : 0)) throw std::runtime_error("ROI was calibrated with a different --rotate; recalibrate config/roi.json");
      int dump_every = options.count("--dump-every") ? number(options.at("--dump-every"), 1, 100000) : 0;
      if (dump_every) std::filesystem::create_directories(options.at("--dump-dir"));
      // Camera first: while it is unplugged (F7) each systemd restart fails here without reloading the model.
      V4L2Camera capture(options.at("--camera"), 640, 480, rotate180);
      // The driver may negotiate another size; refuse here instead of failing on the first frame.
      if (roi && (capture.width() != roi->frame_w || capture.height() != roi->frame_h))
        throw std::runtime_error("camera size " + std::to_string(capture.width()) + "x" + std::to_string(capture.height()) +
                                 " differs from the ROI calibration; recalibrate config/roi.json");
      EcoDetector detector(model_path, backend, threads, configuration);
      std::unique_ptr<PreviewServer> preview;
      // Display only (UI design v1 §3.2): a bad port value, a missing JPEG codec or a taken port turns the video off,
      // never the observer.
      if (options.count("--preview-port")) try {
        preview = std::make_unique<PreviewServer>(number(options.at("--preview-port"), 1024, 65535));
      } catch (const std::exception& e) {
        std::cerr << "hoodseek_vision: preview off: " << e.what() << '\n';
      }
      start_watchdog();  // after model/camera setup, before the first write
      emit({{"event", "start"}, {"input", "camera"}, {"device", options.at("--camera")}, {"model", model_path}, {"backend", backend},
            {"threads", threads}, {"rotate", rotate180 ? 180 : 0}, {"detection_floor", configuration.detection_floor},
            {"core", options.count("--core") ? Json(options.at("--core")) : Json(nullptr)}, {"preview_port", preview ? Json(preview->port()) : Json(nullptr)}});
      // The camera is read at its own rate on a second thread so the card is not limited to one frame per inference
      // (~3 Hz); inference takes only the newest frame. A failed read still ends the process through the loop below.
      LatestFrame newest;
      std::atomic<bool> reading{true};
      std::thread reader([&] {
        std::string failure = "camera read failed";
        try {
          for (cv::Mat shot; reading;) {
            if (!capture.read(shot) || shot.empty()) break;
            if (preview) preview->offer(shot);
            newest.put(shot);
          }
        } catch (const std::exception& e) {
          failure = e.what();
        } catch (...) {
          failure = "camera reader: unknown exception";
        }
        if (reading) newest.fail(failure);
      });
      // Joined on every way out of the loop, before the camera and the preview it uses are destroyed.
      struct JoinReader {
        std::atomic<bool>& reading;
        std::thread& thread;
        ~JoinReader() { reading = false; thread.join(); }
      } join_reader{reading, reader};
      int64_t sequence = 0;
      cv::Mat image;
      while (!stopping) {
        if (!newest.take(image, 100)) continue;  // no new frame yet; the stall watchdog covers a camera that stops
        const auto begin = now_ms();
        Frame frame = detector.infer(image);
        frame.sequence = ++sequence; frame.timestamp_ms = begin;
        auto found = postprocess(frame, configuration);
        const auto infer_ms = now_ms() - begin;
        emit(record(frame, found, infer_ms));
        if (link) link->publish(vision_payload(frame, found, infer_ms, configuration.cat_classes, *roi), now_ms());
        if (dump_every && sequence % dump_every == 0) {
          char name[32]; std::snprintf(name, sizeof name, "frame_%06lld.ppm", static_cast<long long>(sequence));
          write_ppm((std::filesystem::path(options.at("--dump-dir")) / name).string(), image);
        }
        progress = now_ms();
        if (max_frames && sequence >= max_frames) { reached_max = true; break; }
      }
#else
      throw std::runtime_error("camera inference is not enabled; build with -DHOODSEEK_ECOAI=ON in the ATLAS SDK");
#endif
    }
    emit({{"event", "stop"}, {"reason", reached_max ? "max_frames" : stopping ? "stopped" : "end_of_input"}}); finish_watchdog(); return 0;
  } catch (const std::exception& e) {
    finish_watchdog();
    std::cerr << "hoodseek_vision: " << e.what() << '\n';
    try { emit(fault(e.what())); } catch (...) {}
    return 2;
  }
}
