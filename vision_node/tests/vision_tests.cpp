#include "latest_frame.hpp"
#include "vision.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace hoodseek;
void check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
template <class F> void rejects(F action) { bool rejected = false; try { action(); } catch (const std::exception&) { rejected = true; } check(rejected, "invalid input was accepted"); }
Frame frame() {
  Frame f; f.width = 200; f.height = 100; f.model_width = 100; f.model_height = 100; f.sequence = 1; f.timestamp_ms = 0;
  return f;
}
int main() {
  try {
    Config c;
    // Ported checks: classwise NMS, model→frame scaling, letterbox inverse, clipping, quantization, preprocessing.
    auto f = frame();
    f.proposals = {{0, .9F, {10, 10, 60, 80}}, {15, .8F, {10, 10, 60, 80}}, {0, .7F, {11, 11, 60, 80}}};
    auto detections = postprocess(f, c);
    check(detections.size() == 2 && detections[0].class_id == 0 && detections[1].class_id == 15, "classwise NMS lost overlapping person/cat or retained duplicate person");
    check(detections[0].box.width == 120, "model/frame scaling");
    auto padded = frame(); padded.resize_mode = "letterbox";
    padded.proposals = {{15, .9F, {0, 25, 100, 50}}, {0, .9F, {0, 0, 10, 20}}};
    auto unpadded = postprocess(padded, c);
    check(unpadded.size() == 1 && unpadded[0].box == cv::Rect2f(0, 0, 200, 100), "letterbox inverse transform/padding-only box");
    auto odd = input_transform(333, 1000, 640, 640, "letterbox");
    check(odd.left == 213 && odd.top == 0 && odd.size.width == 213 && odd.size.height == 640, "odd-dimension letterbox padding");
    rejects([] { input_transform(100, 100, 640, 640, "typo"); });
    f.proposals = {{16, .9F, {-10, -10, 120, 120}}, {15, .9F, {2, 2, -1, 3}}};
    detections = postprocess(f, c);
    check(detections.size() == 1 && detections[0].box == cv::Rect2f(0, 0, 200, 100), "clip or invalid box handling");
    check(std::abs(overlap({0, 0, 100, 100}, {{50, 0}, {150, 0}, {150, 100}, {50, 100}}) - .5F) < 1e-5, "ROI is intersection / box area, pixel polygon");
    check(overlap({200, 200, 10, 10}, {{50, 0}, {150, 0}, {150, 100}, {50, 100}}) == 0, "disjoint box has no overlap");
    check(decode_integer(15, false, 0, 0, true) == 15, "raw uint8 cat class became person");
    check(std::abs(decode_integer(100, true, .01F, 10, false) - .9F) < 1e-5, "dequantization");
    rejects([] { decode_integer(15, false, 0, 0, false); });
    rejects([] { decode_integer(15, true, 0, 0, true); });
    check(quantize(.25F, .5F, 0, -128, 127) == 1, "quantization rounds half away from zero");
    check(quantize(1, .001F, 0, 0, 255) == 255, "quantization saturation");
    cv::Mat bgr(1, 2, CV_8UC3); bgr.at<cv::Vec3b>(0, 0) = {0, 0, 255}; bgr.at<cv::Vec3b>(0, 1) = {0, 255, 0};
    check(preprocess(bgr, 2, 1, false) == std::vector<float>({1, 0, 0, 0, 1, 0}), "RGB NHWC preprocessing");
    check(preprocess(bgr, 2, 1, true) == std::vector<float>({1, 0, 0, 1, 0, 0}), "RGB NCHW preprocessing");
    f.proposals = {{15, std::numeric_limits<float>::quiet_NaN(), {0, 0, 1, 1}}};
    rejects([&] { postprocess(f, c); });

    // The node reports raw scores: no presence state machine, no person threshold, no debounce settings.
    rejects([] { Config::from_json({{"present_frames", 2}}); });
    rejects([] { Config::from_json({{"person_score", .35}}); });
    rejects([] { Config::from_json({{"absent_ms", 3000}}); });
    rejects([] { Config::from_json({{"typo", 1}}); });
    rejects([] { Config::from_json({{"detection_floor", 0}}); });
    // The floor only trims noise; raising it would apply a judgment threshold in the node (#52: fixed 0.10).
    rejects([] { Config::from_json({{"detection_floor", .35}}); });
    rejects([] { Config::from_json({{"watch_classes", {0, 80}}}); });
    check(std::abs(c.detection_floor - .10F) < 1e-6, "detection floor default is 0.10");
    check(c.watch_classes == std::vector<int>({0, 15, 16, 77}), "watch person, cat, dog, teddy bear by default");

    // A low-confidence candidate above the floor must survive so SafetyCore can apply its own threshold.
    f.proposals = {{15, .12F, {10, 10, 20, 20}}};
    check(postprocess(f, c).size() == 1, "0.12 candidate dropped below a judgment threshold");
    f.proposals = {{15, .08F, {10, 10, 20, 20}}};
    check(postprocess(f, c).empty(), "candidate below detection floor kept");

    // Integer xyxy inside the frame; degenerate boxes are not reported.
    auto box = pixel_box({10.4F, 5.6F, 20.2F, 10.0F}, 640, 480);
    check(box && *box == std::array<int, 4>({10, 5, 31, 16}), "pixel box rounds outward to integers");
    box = pixel_box({630.0F, 470.0F, 50.0F, 50.0F}, 640, 480);
    check(box && (*box)[2] == 640 && (*box)[3] == 480, "pixel box clipped to frame");
    check(!pixel_box({700.0F, 10.0F, 5.0F, 5.0F}, 640, 480), "box outside frame reported");

    // Best candidate per watched class; absent classes are null; unwatched classes ignored.
    std::vector<Proposal> found = {{15, .30F, {10, 10, 20, 20}}, {15, .55F, {100, 10, 20, 20}}, {77, .60F, {100, 10, 22, 22}}, {2, .90F, {0, 0, 5, 5}}};
    auto best = best_by_class(found, c.watch_classes, 640, 480);
    check(best.size() == 4 && best.contains("cat") && best.contains("teddy bear"), "watched classes keyed by COCO label");
    check(std::abs(best["cat"]["score"].get<float>() - .55F) < 1e-6 && best["cat"]["bbox"] == Json({100, 10, 120, 30}), "highest-score cat kept");
    check(best["person"].is_null() && best["dog"].is_null(), "missing class must be null");
    check(!best.contains("car"), "unwatched class reported");

    // Raw frame dump without an image codec library (Pi 5 has no imgcodecs).
    const std::string path = "vision_tests_dump.ppm";
    write_ppm(path, bgr);
    std::ifstream in(path, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::remove(path.c_str());
    check(bytes == std::string("P6\n2 1\n255\n") + std::string("\xff\x00\x00\x00\xff\x00", 6), "PPM header and RGB byte order");

    // config/roi.json: pixel polygon calibrated for one resolution and rotation (tools/roi_click.py writes it).
    const Json roi_json = {{"frame_w", 640}, {"frame_h", 480}, {"rotate", 0}, {"points", {{300, 200}, {640, 200}, {640, 480}, {300, 480}}}, {"note", "test"}};
    auto roi = load_roi(roi_json);
    check(roi.frame_w == 640 && roi.frame_h == 480 && roi.rotate == 0 && roi.points.size() == 4, "ROI file fields");
    for (const auto& bad : {Json{{"frame_w", 640}, {"frame_h", 480}, {"rotate", 0}, {"points", {{0, 0}, {100, 100}, {0, 100}, {100, 0}}}},   // bow-tie
                            Json{{"frame_w", 640}, {"frame_h", 480}, {"rotate", 0}, {"points", {{0, 0}, {700, 0}, {700, 100}}}},            // outside frame
                            Json{{"frame_w", 640}, {"frame_h", 480}, {"rotate", 0}, {"points", {{0, 0}, {10.5, 0}, {10, 10}}}},             // not integer pixels
                            Json{{"frame_w", 640}, {"frame_h", 480}, {"rotate", 90}, {"points", {{0, 0}, {10, 0}, {10, 10}}}},              // rotation
                            Json{{"frame_w", 5000}, {"frame_h", 480}, {"rotate", 0}, {"points", {{0, 0}, {10, 0}, {10, 10}}}},              // contract frame_w ≤ 4096
                            Json{{"frame_h", 480}, {"rotate", 0}, {"points", {{0, 0}, {10, 0}, {10, 10}}}}})                                 // missing size
      rejects([&] { load_roi(bad); });

    // Contract v1 vision payload (contracts/vision.schema.json): seen ⇔ bbox array, raw score, cat slot classes from config.
    check(c.cat_classes == std::vector<int>({15, 16}), "pet slot is COCO cat + dog: #49 doll face-on comes out as dog");
    rejects([] { Config::from_json({{"cat_classes", Json::array()}}); });
    rejects([] { Config::from_json({{"cat_classes", {80}}}); });
    Frame shot; shot.width = 640; shot.height = 480;
    std::vector<Proposal> seen_now = {{0, .81F, {120, 40, 180, 440}}, {15, .30F, {10, 10, 50, 50}}, {77, .67F, {350, 260, 120, 140}}};
    auto p = vision_payload(shot, seen_now, 313, {15}, roi);
    check(p["frame_w"] == 640 && p["frame_h"] == 480 && p["infer_ms"] == 313, "payload frame fields");
    check(p["person"] == Json({{"seen", true}, {"score", .81}, {"bbox", {120, 40, 300, 480}}}), "person best candidate");
    check(p["cat"]["seen"] == true && p["cat"]["bbox"] == Json({10, 10, 60, 60}) && p["cat"]["roi_overlap"] == 0.0, "cat-only slot ignores teddy bear; outside ROI");
    p = vision_payload(shot, seen_now, 313, {15, 77}, roi);
    check(p["cat"] == Json({{"seen", true}, {"score", .67}, {"bbox", {350, 260, 470, 400}}, {"roi_overlap", 1.0}}), "mapped teddy bear wins the cat slot, fully in ROI");
    p = vision_payload(shot, {{16, .78F, {350, 260, 120, 140}}}, 313, c.cat_classes, roi);
    check(p["cat"]["seen"] == true && p["cat"]["score"] == .78, "default classes: a dog detection fills the pet slot");
    p = vision_payload(shot, {}, 290, {15}, roi);
    check(p["person"] == Json({{"seen", false}, {"score", 0.0}, {"bbox", nullptr}}), "no person: seen false, bbox null");
    check(p["cat"] == Json({{"seen", false}, {"score", 0.0}, {"bbox", nullptr}, {"roi_overlap", 0.0}}), "no cat: seen false, bbox null");
    Frame other = shot; other.width = 320;
    rejects([&] { vision_payload(other, {}, 290, {15}, roi); });   // ROI calibrated for another resolution

    auto hello = envelope("hello", 0, 0, {{"node", "vision-node"}, {"ver", "0.1.0"}});
    check(hello == Json::parse(R"({"v":1,"type":"hello","src":"vision","seq":0,"t_ms":0,"p":{"node":"vision-node","ver":"0.1.0"}})"), "hello matches contracts/examples/hello.json");
    auto line = encode_line(envelope("vision", 1, 5, p));
    check(line.back() == '\n' && line.find('\n') == line.size() - 1, "one message per line");
    rejects([] { encode_line(Json{{"pad", std::string(5000, 'x')}}); });   // 4096-byte line limit (design §2.1)

    // Camera reader -> inference loop (#148 follow-up): the newest frame only, buffers never shared, a failed read ends the loop.
    LatestFrame newest;
    cv::Mat taken;
    check(!newest.take(taken, 10), "no frame yet: take times out");
    cv::Mat camera(2, 2, CV_8UC1, cv::Scalar(1));
    newest.put(camera);
    camera.create(2, 2, CV_8UC1); camera.setTo(2);
    newest.put(camera);
    check(newest.take(taken, 10) && taken.at<uchar>(0, 0) == 2, "inference takes the newest frame, not a queue");
    check(!newest.take(taken, 10), "a frame is taken once");
    camera.create(2, 2, CV_8UC1); camera.setTo(9);  // the reader reuses what put handed back
    check(taken.at<uchar>(0, 0) == 2, "the reader must never write into the frame inference holds");
    newest.put(camera);  // the next put hands back what take left behind
    camera.create(2, 2, CV_8UC1); camera.setTo(7);
    check(taken.at<uchar>(0, 0) == 2, "take must not leave the frame it handed out behind for the reader");
    // A failed read wakes a waiting take at once, with its reason.
    const auto waited_from = std::chrono::steady_clock::now();
    std::thread failing([&] { std::this_thread::sleep_for(std::chrono::milliseconds(20)); newest.fail("camera read failed"); });
    std::string reason;
    try { while (newest.take(taken, 1000)) {} } catch (const std::runtime_error& e) { reason = e.what(); }
    failing.join();
    check(reason == "camera read failed", "a reader failure must reach the inference loop with its reason");
    check(std::chrono::steady_clock::now() - waited_from < std::chrono::milliseconds(500), "fail must wake a waiting take");

    std::cout << "vision core checks passed\n";
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
