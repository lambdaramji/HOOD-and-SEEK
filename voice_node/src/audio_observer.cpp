#include "audio_pipeline.h"
#include "command_parser.h"
#include "voice_tcp.h"
#include "common-whisper.h"
#include "whisper.h"

#ifdef HOOD_SEEK_WITH_SENSEVOICE
#include "sherpa-onnx/c-api/c-api.h"
#include "hi_lg_classifier.h"
#else
struct SherpaOnnxOfflineRecognizer;
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef HOOD_SEEK_WITH_PULSE
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <pulse/pulseaudio.h>
#endif

using namespace hoodseek::voice;
namespace {

// 신호 처리 함수와 추론 작업 스레드가 함께 읽으므로 lock-free atomic으로 종료를 공유한다.
std::atomic<bool> stopping{false};
std::atomic<bool> engine_error{false};
static_assert(std::atomic<bool>::is_always_lock_free, "signal stop flag must be lock-free");
void request_stop(int) { stopping.store(true, std::memory_order_relaxed); }
using Clock = std::chrono::steady_clock;
bool abort_inference(void* user) {
  const auto* deadline = static_cast<const Clock::time_point*>(user);
  return stopping.load(std::memory_order_relaxed) || (deadline && Clock::now() >= *deadline);
}

struct Stats {
  int frames = 0;
  int asr_calls = 0;
  int utterances = 0;
  int retry_calls = 0;
  int too_long = 0;
  int candidates = 0;
  int transmitted = 0;
  std::int64_t vad_us = 0;
  std::int64_t max_vad_us = 0;
  float peak_dbfs = -std::numeric_limits<float>::infinity();
};

struct Quality {
  float normal = std::numeric_limits<float>::quiet_NaN();
  float raising = std::numeric_limits<float>::quiet_NaN();
  bool publish = false;
};

const char* publish_name(PublishResult result) {
  switch (result) {
    case PublishResult::Sent: return "sent";
    case PublishResult::Invalid: return "invalid";
    case PublishResult::QualityRejected: return "quality_rejected";
    case PublishResult::Expired: return "expired";
    case PublishResult::Duplicate: return "duplicate";
    case PublishResult::Disconnected: return "disconnected";
    case PublishResult::IoError: return "io_error";
  }
  return "invalid_result";
}

const char* intent_name(Intent intent) {
  switch (intent) {
    case Intent::FAN_SET: return "FAN_SET";
    case Intent::FAN_OFF: return "FAN_OFF";
    case Intent::IND_OFF: return "IND_OFF";
    case Intent::IND_ON: return "IND_ON";
    case Intent::IND_SET: return "IND_SET";
    case Intent::IND_UP: return "IND_UP";
    case Intent::IND_DOWN: return "IND_DOWN";
    case Intent::NONE: return "NONE";
  }
  throw std::runtime_error("invalid internal intent");
}

bool consume_frame(const AudioFrame& frame, SpeechVad& vad, whisper_vad_context* vad_ctx, whisper_context* ctx,
                   whisper_context* fallback_ctx, const SherpaOnnxOfflineRecognizer* sensevoice,
                   Stats& stats, bool live, VoiceTcpClient* link, const Quality& quality,
                   Clock::time_point frame_end) {
  if (link) link->service();
  ++stats.frames;
  const float dbfs = frame_dbfs(frame);
  stats.peak_dbfs = std::max(stats.peak_dbfs, dbfs);
  const auto vad_started = Clock::now();
  engine_error.store(false, std::memory_order_relaxed);
  if (!whisper_vad_detect_speech_no_reset(vad_ctx, frame.data(), kFrameSamples) ||
      engine_error.load(std::memory_order_relaxed) ||
      whisper_vad_n_probs(vad_ctx) != 1 || !whisper_vad_probs(vad_ctx))
    throw std::runtime_error("streaming VAD inference failed");
  const auto vad_us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - vad_started).count();
  stats.vad_us += vad_us;
  stats.max_vad_us = std::max(stats.max_vad_us, static_cast<std::int64_t>(vad_us));
  const float probability = whisper_vad_probs(vad_ctx)[0];
  const auto segment = vad.push(frame, probability);
  if (live && stats.frames % 10 == 0) {
    std::cout << "event=audio_level frame=" << stats.frames << " dbfs=" << dbfs
              << " speech_probability=" << probability;
    if (const auto noise = vad.noise_dbfs())
      std::cout << " noise_dbfs=" << *noise << " relative_db=" << dbfs - *noise;
    std::cout << '\n';
  }
  if (!segment) return false;
  if (segment->too_long) {
    ++stats.too_long;
    std::cout << "event=vad_drop reason=max4s\n";
    return false;
  }
  const auto utterance_end = frame_end - std::chrono::milliseconds(
      kEndSilenceFrames * kFrameSamples * 1000 / kSampleRate);
  auto deadline = utterance_end + std::chrono::seconds(5);
  if (sensevoice) {
#ifdef HOOD_SEEK_WITH_SENSEVOICE
    // SenseVoice에는 Whisper token p/no_speech가 없다. 자모 유사도를 ASR conf로
    // 꾸미지 않고 wire conf=0(미제공)을 사용한다. 실행 허용은 Core 소관이다.
    const auto id = ++stats.utterances;
    ++stats.asr_calls;
    std::cout << "event=asr_started index=" << id << " model=sensevoice samples="
              << segment->samples.size() << '\n';
    std::string text;
    std::int64_t asr_ms = 0;
    const auto infer = [&] {
      const auto started = Clock::now();
      std::unique_ptr<const SherpaOnnxOfflineStream, decltype(&SherpaOnnxDestroyOfflineStream)> stream(
          SherpaOnnxCreateOfflineStream(sensevoice), SherpaOnnxDestroyOfflineStream);
      if (!stream) throw std::runtime_error("SenseVoice stream creation failed");
      SherpaOnnxAcceptWaveformOffline(stream.get(), kSampleRate, segment->samples.data(),
                                    static_cast<int>(segment->samples.size()));
      SherpaOnnxDecodeOfflineStream(sensevoice, stream.get());
      std::unique_ptr<const SherpaOnnxOfflineRecognizerResult,
                      decltype(&SherpaOnnxDestroyOfflineRecognizerResult)> result(
          SherpaOnnxGetOfflineStreamResult(stream.get()), SherpaOnnxDestroyOfflineRecognizerResult);
      if (!result || !result->text) throw std::runtime_error("SenseVoice result missing");
      text = result->text;
      asr_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
      return 0;
    };
    if (link) {
      auto task = std::async(std::launch::async, infer);
      link->wait_with_service(task);
    } else {
      infer();
    }
    if (live) {
      vad.discard_until_silence();
      whisper_vad_reset_state(vad_ctx);
    }
    if (stopping) return true;
    // 호출어·문장 경계·대상·동작을 확인하고 같은 규칙으로 송신 직전에 재검사한다.
    const auto command = hi_lg::classify(text);
    const bool valid = command.intent != "DROP";
    if (valid) ++stats.candidates;
    std::cout << "event=asr index=" << id << " model=sensevoice samples=" << segment->samples.size()
              << " asr_ms=" << asr_ms << " initial_text=" << std::quoted(text)
              << " text=" << std::quoted(text)
              << " conf=unavailable wire_conf=0 quality=asr_unscored retry_status=not_applicable"
              << " intent=" << command.intent
              << " parser_reason=" << command.reason
              << " activation_similarity=" << command.activation
              << " target_similarity=" << command.target
              << " action_similarity=" << command.action
              << " level_similarity=" << command.level_similarity;
    if (valid && (command.intent == "FAN_SET" || command.intent == "IND_SET")) std::cout << " value=" << command.value;
    if (!command.normalized.empty())
      std::cout << " normalized_text=" << std::quoted(command.normalized);
    if (link) std::cout << " link_connected=" << link->connected() << " link_lines=" << link->received_lines();
    std::cout << " end_to_intent_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - utterance_end).count()
              << " intent_monotonic_ns=" << std::chrono::duration_cast<std::chrono::nanoseconds>(
                     Clock::now().time_since_epoch()).count()
              << " expired=" << (Clock::now() >= deadline);
    if (!valid) {
      std::cout << " disposition=drop_text";
    } else if (quality.publish) {
      const auto result = link->publish(id, text, 0, true, deadline);
      if (result == PublishResult::Sent) ++stats.transmitted;
      std::cout << " disposition=" << publish_name(result)
                << " wire_intent=" << command.intent
                << " publish_policy=hi_lg_v1 fan_on_default=" << command.default_fan_on
                << " end_to_send_ms="
                << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - utterance_end).count();
    } else {
      std::cout << " disposition=observation_only";
    }
    std::cout << '\n';
    return true;
#else
    throw std::runtime_error("SenseVoice support was not enabled at build time");
#endif
  }
  auto params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
  params.n_threads = 4;
  params.audio_ctx = 256;
  params.language = "ko";
  params.no_context = true;
  params.no_timestamps = false;
  params.print_special = params.print_progress = params.print_realtime = params.print_timestamps = false;
  params.initial_prompt = "후드, 인덕션, 강풍, 중풍, 약풍, 팬, 화력. 꺼, 켜.";
  params.beam_search.beam_size = 5;
  params.abort_callback = abort_inference;
  const auto started = Clock::now();
  const auto id = ++stats.utterances;
  struct Recognition {
    std::string text;
    std::optional<float> conf;
    float no_speech = 0;
    bool valid = true;
  };
  auto* active_ctx = ctx;
  const auto read_result = [&] {
    Recognition result;
    std::vector<float> scores;
    for (int i = 0; i < whisper_full_n_segments(active_ctx); ++i) {
      if (!result.text.empty()) result.text += ' ';
      result.text += whisper_full_get_segment_text(active_ctx, i);
      const float no_speech = whisper_full_get_segment_no_speech_prob(active_ctx, i);
      result.valid &= std::isfinite(no_speech) && no_speech >= 0 && no_speech <= 1;
      result.no_speech = std::max(result.no_speech, no_speech);
      for (int token = 0; token < whisper_full_n_tokens(active_ctx, i); ++token)
        if (whisper_full_get_token_id(active_ctx, i, token) < whisper_token_eot(active_ctx))
          scores.push_back(whisper_full_get_token_p(active_ctx, i, token));
    }
    result.conf = mean_token_score(scores);
    return result;
  };
  const bool ready = std::isfinite(quality.normal) && std::isfinite(quality.raising);
  const auto minimum_for = [&](const Command& command) {
    // 현재 팬 상태를 판단하지 않으므로 모든 FAN_SET에 올림 문턱을 적용한다.
    return command.intent == Intent::FAN_SET || command.intent == Intent::IND_ON ||
           command.intent == Intent::IND_UP ? quality.raising : quality.normal;
  };
  const auto passes = [&](const Recognition& result, const Command& command) {
    return ready && result.valid && result.conf && *result.conf >= minimum_for(command) &&
           result.no_speech <= 0.6f;
  };
  Recognition initial, selected;
  std::optional<Command> retry_command;
  bool damaged_fan = false;
  std::string retry_reason;
  std::string retry_status = "not_needed";
  std::int64_t asr_ms = 0;
  std::int64_t retry_ms = 0;
  for (int attempt = 0; attempt < 2; ++attempt) {
    ++stats.asr_calls;
    if (attempt) {
      ++stats.retry_calls;
      active_ctx = damaged_fan ? fallback_ctx : ctx;
      params.audio_ctx = damaged_fan ? 256 : 512;
      // 반복 전사만 beam3에서 회귀 없이 빨라졌다. 다른 재전사는 beam5를 유지한다.
      params.beam_search.beam_size = retry_reason == "equivalent_repeated_command" ? 3 : 5;
      params.abort_callback_user_data = &deadline;
    }
    std::cout << "event=asr_started index=" << id << " attempt=" << attempt + 1
              << " audio_ctx=" << params.audio_ctx << " beam=" << params.beam_search.beam_size
              << " model_role=" << (damaged_fan && attempt ? "fallback" : "primary") << '\n';
    std::int64_t elapsed = 0;
    const auto infer = [&] {
      const auto inference_started = Clock::now();
      const int rc = whisper_full(active_ctx, params, segment->samples.data(), static_cast<int>(segment->samples.size()));
      elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - inference_started).count();
      return rc;
    };
    int rc;
    if (link) {
      // get()까지 context·samples를 보존하며 두 번째 추론 중에도 TCP를 처리한다.
      auto task = std::async(std::launch::async, infer);
      rc = link->wait_with_service(task);
    } else {
      rc = infer();
    }
    asr_ms += elapsed;
    if (attempt) retry_ms = elapsed;
    if (stopping) return true;
    if (rc != 0) {
      if (!attempt) throw std::runtime_error("whisper_full failed: " + std::to_string(rc));
      retry_status = Clock::now() >= deadline ? "expired" : "asr_error";
      std::cout << "event=asr_retry_result index=" << id << " status=" << retry_status
                << " rc=" << rc << " retry_ms=" << elapsed << '\n';
      break;  // 불완전한 재전사로 원래 NONE을 명령으로 바꾸지 않는다.
    }
    auto result = read_result();
    if (!attempt) {
      initial = selected = std::move(result);
      if (!ready || !initial.valid || !initial.conf || initial.no_speech > 0.6f) break;
      const auto first_command = parse_command(initial.text);
      retry_command = repeated_command_candidate(initial.text);
      if (first_command && first_command->intent != Intent::NONE) {
        if (passes(initial, *first_command)) break;
        retry_command = first_command;
        retry_reason = "supported_below_threshold";
      } else if (retry_command) {
        retry_reason = "equivalent_repeated_command";
      } else if (fallback_ctx && is_damaged_fan_text(initial.text)) {
        damaged_fan = true;
        retry_reason = "damaged_fan_original";
      } else if (const auto power = power_retry_candidate(initial.text)) {
        retry_command = power;  // 모호한 전원 발화에서 켜기로 승격하지 않는다.
        retry_reason = "ambiguous_power_text";
      } else break;  // 정상 NONE·제외 문장을 모두 실패로 보고 재전사하지 않는다.
      retry_status = Clock::now() >= deadline ? "expired_before_retry" : "started";
      if (retry_status != "started") break;
      std::cout << "event=asr_retry index=" << id << " reason=" << retry_reason
                << " initial_text=" << std::quoted(initial.text)
                << " initial_conf=" << *initial.conf << " next_audio_ctx=" << (damaged_fan ? 256 : 512) << '\n';
      continue;
    }
    const auto command = parse_command(result.text);
    const bool same = command && (damaged_fan ?
        (command->intent == Intent::FAN_OFF || command->intent == Intent::FAN_SET) :
        (retry_command && command->intent == retry_command->intent && command->value == retry_command->value));
    retry_status = Clock::now() >= deadline ? "expired" : !same ? "command_mismatch" :
                   !passes(result, *command) ? "quality_rejected" : "selected";
    std::cout << "event=asr_retry_result index=" << id << " status=" << retry_status
              << " text=" << std::quoted(result.text) << " retry_ms=" << elapsed;
    if (result.conf) std::cout << " conf=" << *result.conf;
    std::cout << " no_speech_max=" << result.no_speech << '\n';
    if (retry_status == "selected") selected = std::move(result);
    break;
  }
  if (live) {
    vad.discard_until_silence();
    whisper_vad_reset_state(vad_ctx);  // 추론 중 누락된 마이크 구간을 이어 붙이지 않는다.
  }
  const auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
  const auto& text = selected.text;
  const auto& conf = selected.conf;
  std::cout << "event=asr index=" << id << " samples=" << segment->samples.size()
            << " asr_ms=" << asr_ms << " wait_ms=" << wait_ms << " retry_ms=" << retry_ms
            << " initial_conf=" << (initial.conf ? *initial.conf : -1)
            << " retry_reason=" << (retry_reason.empty() ? "none" : retry_reason)
            << " retry_status=" << retry_status << " initial_text=" << std::quoted(initial.text)
            << " text=" << std::quoted(text);
  if (link) std::cout << " link_connected=" << link->connected() << " link_lines=" << link->received_lines();
  if (!conf || !selected.valid) {
    std::cout << " disposition=drop_invalid_asr_metrics\n";
    return true;
  }
  std::cout << " conf=" << *conf << " no_speech_max=" << selected.no_speech;
  const auto normalized = normalize_command_text(text);
  if (normalized && *normalized != text)
    std::cout << " normalized_text=" << std::quoted(*normalized);
  const auto command = parse_command(text);
  if (!command || !is_valid_command(*command)) {
    std::cout << " disposition=drop_text\n";
    return true;
  }
  ++stats.candidates;
  const float minimum = minimum_for(*command);
  const bool passed = passes(selected, *command);
  std::cout << " quality=" << (ready ? (passed ? "passed_operator_thresholds" : "rejected") : "uncalibrated")
            << " intent=" << intent_name(command->intent);
  if (command->value) std::cout << " value=" << *command->value;
  if (ready) std::cout << " min_score=" << minimum;
  if (quality.publish) {
    const auto result = link->publish(id, text, *conf, passed, deadline);
    if (result == PublishResult::Sent) ++stats.transmitted;
    std::cout << " disposition=" << publish_name(result) << " end_to_send_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - utterance_end).count();
  } else {
    std::cout << " disposition=observation_only";
  }
  std::cout << '\n';
  return true;
}

#ifdef HOOD_SEEK_WITH_PULSE
bool capture_source_matches(GstElement* mic, const std::string& requested) {
  gchar* actual = nullptr;
  g_object_get(mic, "current-device", &actual, nullptr);
  const bool matches = actual && requested == actual;
  g_free(actual);
  return matches;
}

// ATLAS may redirect a newly created stream despite pulsesrc.device. Move only
// this pulsesrc's source-output, then verify the actual source before any PCM is used.
void pin_capture_source(GstElement* mic, const std::string& requested) {
  const auto deadline = Clock::now() + std::chrono::seconds(3);
  std::unique_ptr<pa_mainloop, decltype(&pa_mainloop_free)> loop(pa_mainloop_new(), pa_mainloop_free);
  if (!loop) throw std::runtime_error("PulseAudio control loop creation failed");
  auto wait = [&](const auto& ready) {
    while (!ready()) {
      if (stopping || Clock::now() >= deadline || pa_mainloop_iterate(loop.get(), 0, nullptr) < 0)
        throw std::runtime_error("capture source verification interrupted or timed out");
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  };
  guint index = PA_INVALID_INDEX;
  wait([&] {
    g_object_get(mic, "source-output-index", &index, nullptr);
    return index != PA_INVALID_INDEX;
  });
  auto disconnect = [](pa_context* context) {
    pa_context_disconnect(context);
    pa_context_unref(context);
  };
  std::unique_ptr<pa_context, decltype(disconnect)> context(
      pa_context_new(pa_mainloop_get_api(loop.get()), "hoodseek-input-control"), disconnect);
  if (!context || pa_context_connect(context.get(), nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0)
    throw std::runtime_error("PulseAudio control connection failed");
  wait([&] {
    const auto state = pa_context_get_state(context.get());
    return state == PA_CONTEXT_READY || !PA_CONTEXT_IS_GOOD(state);
  });
  if (pa_context_get_state(context.get()) != PA_CONTEXT_READY)
    throw std::runtime_error("PulseAudio control connection unavailable");
  int moved = -1;
  std::unique_ptr<pa_operation, decltype(&pa_operation_unref)> operation(
      pa_context_move_source_output_by_name(context.get(), index, requested.c_str(),
        [](pa_context*, int success, void* result) { *static_cast<int*>(result) = success; }, &moved),
      pa_operation_unref);
  if (!operation) throw std::runtime_error("capture source move could not start");
  wait([&] { return moved != -1; });
  if (!moved) throw std::runtime_error("requested capture source unavailable; no fallback allowed");
  wait([&] { return capture_source_matches(mic, requested); });
  std::cout << "event=capture_source_verified requested=" << std::quoted(requested)
            << " actual=" << std::quoted(requested) << " source_output_index=" << index << '\n';
}

void observe_microphone(const std::string& source, int seconds, const std::string& capture_raw, SpeechVad& vad,
                        whisper_vad_context* vad_ctx, whisper_context* ctx, whisper_context* fallback_ctx,
                        const SherpaOnnxOfflineRecognizer* sensevoice,
                        Stats& stats, VoiceTcpClient* link, const Quality& quality) {
  gst_init(nullptr, nullptr);
  // 시험 원음은 ASR가 읽는 것과 같은 16 kHz mono PCM이며 기존 파일을 덮어쓰지 않는다.
  std::unique_ptr<FILE, decltype(&std::fclose)> raw(
      capture_raw.empty() ? nullptr : std::fopen(capture_raw.c_str(), "wbx"), std::fclose);
  if (!capture_raw.empty() && !raw) throw std::runtime_error("capture output exists or cannot be created");
  struct Origin {
    GstElement* pipeline = nullptr;
    GstElement* mic = nullptr;
    const std::string* requested = nullptr;
    std::atomic<bool> source_verified{false};
    std::atomic<bool> source_changed{false};
    std::atomic<std::int64_t> sample_zero_ns{0};
  } origin;
  GError* error = nullptr;
  auto cleanup = [](GstElement* p) {
    gst_element_set_state(p, GST_STATE_NULL);
    gst_object_unref(p);
  };
  const std::string pipeline_text =
      "pulsesrc name=mic ! audioconvert ! audioresample ! "
      "audio/x-raw,format=S16LE,rate=16000,channels=1,layout=interleaved ! " +
      std::string(raw ? "tee name=t t. ! queue ! fdsink name=raw sync=false async=false t. ! queue ! " : "") +
      "appsink name=audio sync=false max-buffers=2 drop=true";
  std::unique_ptr<GstElement, decltype(cleanup)> pipeline(
      gst_parse_launch(pipeline_text.c_str(), &error), cleanup);
  if (error) {
    const std::string message = error->message;
    g_error_free(error);
    throw std::runtime_error(message);
  }
  if (!pipeline) throw std::runtime_error("GStreamer pipeline creation failed");
  origin.pipeline = pipeline.get();
  auto unref = [](auto* p) { gst_object_unref(p); };
  if (raw) {
    std::unique_ptr<GstElement, decltype(unref)> output(
        gst_bin_get_by_name(GST_BIN(pipeline.get()), "raw"), unref);
    if (!output) throw std::runtime_error("capture output missing");
    g_object_set(output.get(), "fd", fileno(raw.get()), nullptr);
    std::unique_ptr<GstPad, decltype(unref)> pad(gst_element_get_static_pad(output.get(), "sink"), unref);
    if (!pad) throw std::runtime_error("capture output pad missing");
    gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_BUFFER,
      [](GstPad* pad, GstPadProbeInfo* info, gpointer data) -> GstPadProbeReturn {
        auto& o = *static_cast<Origin*>(data);
        const auto pts = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
        GstEvent* event = gst_pad_get_sticky_event(pad, GST_EVENT_SEGMENT, 0);
        const GstSegment* segment = nullptr;
        if (event) gst_event_parse_segment(event, &segment);
        const auto running = segment && segment->format == GST_FORMAT_TIME && segment->rate == 1.0
            ? gst_segment_to_running_time(segment, GST_FORMAT_TIME, pts) : GST_CLOCK_TIME_NONE;
        if (event) gst_event_unref(event);
        GstClock* clock = gst_element_get_clock(o.pipeline);
        const auto base = gst_element_get_base_time(o.pipeline);
        const auto now = Clock::now();
        const auto time = clock ? gst_clock_get_time(clock) : GST_CLOCK_TIME_NONE;
        if (clock) gst_object_unref(clock);
        if (GST_CLOCK_TIME_IS_VALID(running) && GST_CLOCK_TIME_IS_VALID(base) &&
            GST_CLOCK_TIME_IS_VALID(time) && time >= base && running <= time - base) {
          o.sample_zero_ns.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
              now.time_since_epoch()).count() - static_cast<std::int64_t>(time - base - running));
        }
        return GST_PAD_PROBE_REMOVE;
      }, &origin, nullptr);
  }
  std::unique_ptr<GstElement, decltype(unref)> mic(gst_bin_get_by_name(GST_BIN(pipeline.get()), "mic"), unref);
  std::unique_ptr<GstElement, decltype(unref)> sink(gst_bin_get_by_name(GST_BIN(pipeline.get()), "audio"), unref);
  std::unique_ptr<GstBus, decltype(unref)> bus(gst_element_get_bus(pipeline.get()), unref);
  if (!mic || !sink || !bus) throw std::runtime_error("GStreamer elements missing");
  origin.mic = mic.get();
  origin.requested = &source;
  std::unique_ptr<GstPad, decltype(unref)> input_pad(gst_element_get_static_pad(mic.get(), "src"), unref);
  if (!input_pad) throw std::runtime_error("capture source pad missing");
  gst_pad_add_probe(input_pad.get(), GST_PAD_PROBE_TYPE_BUFFER,
    [](GstPad*, GstPadProbeInfo*, gpointer data) -> GstPadProbeReturn {
      auto& o = *static_cast<Origin*>(data);
      if (!o.source_verified.load()) return GST_PAD_PROBE_DROP;
      if (!capture_source_matches(o.mic, *o.requested)) {
        o.source_changed.store(true);
        stopping.store(true);  // Cancel an in-flight result before its existing stop/publish checks.
        return GST_PAD_PROBE_DROP;
      }
      return GST_PAD_PROBE_OK;
    }, &origin, nullptr);
  g_object_set(mic.get(), "device", source.c_str(), nullptr);
  if (gst_element_set_state(pipeline.get(), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    throw std::runtime_error("PulseAudio capture failed to start");
  pin_capture_source(mic.get(), source);
  origin.source_verified.store(true);
  std::cout << "event=capture_ready source=" << std::quoted(source) << " rate=16000 channels=1\n";
  const auto started = Clock::now();
  std::vector<float> pending;
  bool capture_started = false;
  bool origin_logged = false;
  bool conservative_time_logged = false;
  while (!stopping && (seconds == 0 || Clock::now() - started < std::chrono::seconds(seconds))) {
    if (origin.source_changed.load() || !capture_source_matches(mic.get(), source))
      throw std::runtime_error("capture source changed; processing and publication stopped");
    if (link) link->service();
    std::unique_ptr<GstMessage, decltype(&gst_message_unref)> message(
        gst_bus_pop_filtered(bus.get(), static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)), gst_message_unref);
    if (message) {
      if (GST_MESSAGE_TYPE(message.get()) == GST_MESSAGE_EOS) break;
      GError* gst_error = nullptr;
      gchar* detail = nullptr;
      gst_message_parse_error(message.get(), &gst_error, &detail);
      const std::string reason = gst_error ? gst_error->message : "capture error";
      if (gst_error) g_error_free(gst_error);
      g_free(detail);
      throw std::runtime_error(reason);
    }
    std::unique_ptr<GstSample, decltype(&gst_sample_unref)> sample(
        gst_app_sink_try_pull_sample(GST_APP_SINK(sink.get()), 100 * GST_MSECOND), gst_sample_unref);
    if (!sample) continue;
    if (raw && !origin_logged && origin.sample_zero_ns.load() != 0) {
      std::cout << "event=capture_audio_origin sample0_monotonic_ns=" << origin.sample_zero_ns.load()
                << " rate=16000 channels=1 format=S16LE\n";
      origin_logged = true;
    }
    GstMapInfo map{};
    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    if (!buffer || !gst_buffer_map(buffer, &map, GST_MAP_READ))
      throw std::runtime_error("capture buffer mapping failed");
    if (map.size % 2 != 0 || map.size > 2 * kSampleRate) {
      gst_buffer_unmap(buffer, &map);
      throw std::runtime_error("invalid or oversized PCM capture buffer");
    }
    auto buffer_end = Clock::now();
    std::unique_ptr<GstClock, decltype(&gst_object_unref)> clock(
        gst_element_get_clock(pipeline.get()), gst_object_unref);
    const auto pts = GST_BUFFER_PTS(buffer);
    const auto* segment = gst_sample_get_segment(sample.get());
    const auto running = segment && segment->format == GST_FORMAT_TIME && segment->rate == 1.0
        ? gst_segment_to_running_time(segment, GST_FORMAT_TIME, pts) : GST_CLOCK_TIME_NONE;
    const auto base = gst_element_get_base_time(pipeline.get());
    const auto clock_now = clock ? gst_clock_get_time(clock.get()) : GST_CLOCK_TIME_NONE;
    const auto duration = static_cast<GstClockTime>(map.size / 2) * GST_SECOND / kSampleRate;
    const auto age = GST_CLOCK_TIME_IS_VALID(base) && GST_CLOCK_TIME_IS_VALID(clock_now) && clock_now >= base
        ? capture_buffer_age_ns(running, duration, clock_now - base) : std::nullopt;
    if (age) {
      buffer_end -= std::chrono::nanoseconds(*age);
      if (!conservative_time_logged && clock_now - base - running < duration) {
        std::cout << "event=capture_time_conservative bound=buffer_start start_age_ns=" << *age
                  << " duration_ns=" << duration << '\n';
        conservative_time_logged = true;
      }
    } else if (quality.publish) {
      gst_buffer_unmap(buffer, &map);
      std::cout << "event=capture_time_unavailable pts_ns=" << pts << " running_ns=" << running
                << " clock_ns=" << clock_now << " base_ns=" << base << " duration_ns=" << duration << '\n';
      throw std::runtime_error("capture timestamp unavailable; publication stopped");
    }
    if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DISCONT)) {
      pending.clear();
      // 첫 버퍼의 시작 표시는 유실이 아니다. 이후 유실된 구간만 무음까지 버린다.
      if (capture_started) {
        vad.discard_until_silence();
        whisper_vad_reset_state(vad_ctx);
      }
    }
    capture_started = true;
    for (std::size_t i = 0; i < map.size; i += 2) {
      int value = map.data[i] | (static_cast<int>(map.data[i + 1]) << 8);
      if (value >= 32768) value -= 65536;
      pending.push_back(static_cast<float>(value) / 32768);
    }
    gst_buffer_unmap(buffer, &map);
    std::size_t consumed = 0;
    while (!stopping && pending.size() - consumed >= kFrameSamples) {
      AudioFrame frame{};
      std::copy_n(pending.begin() + consumed, kFrameSamples, frame.begin());
      consumed += kFrameSamples;
      const auto frame_end = buffer_end - std::chrono::nanoseconds(
          static_cast<std::int64_t>(pending.size() - consumed) * 1000000000 / kSampleRate);
      if (consume_frame(frame, vad, vad_ctx, ctx, fallback_ctx, sensevoice, stats, true, link, quality, frame_end)) {
        consumed = pending.size();  // ASR 전에 받은 나머지 오디오는 재사용하지 않는다.
        break;
      }
    }
    pending.erase(pending.begin(), pending.begin() + consumed);
  }
  if (origin.source_changed.load())
    throw std::runtime_error("capture source changed; processing and publication stopped");
}
#endif

}  // namespace

int main(int argc, char** argv) {
  try {
    std::string model;
    std::string sensevoice_dir;
    std::string fallback_model;
    std::string vad_model;
    std::string source;
    std::string capture_raw;
    std::string core_host;
    int core_port = 0;
    std::vector<std::string> wavs;
    int seconds = 0;
    Quality quality;
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--help") {
        std::cout << "(--model PATH [--fallback-model PATH] | --sensevoice-dir DIR) --vad-model PATH\n"
                     "(--wav PATH [--wav PATH ...] | --pulse-source NAME [--seconds N])\n"
                     "[--core-host IPV4 --core-port PORT] (default: observation only)\n"
                     "[--min-score N --min-score-raise N] [--publish] [--capture-raw NEW_PATH]\n"
                     "Publication requires a receiver. Whisper also requires explicit quality thresholds.\n"
                     "SenseVoice uses HiLg parsing and wire conf=0 (ASR score unavailable); score/fallback options are rejected.\n";
        return 0;
      }
      if (arg == "--publish") { quality.publish = true; continue; }
      if (i + 1 == argc) throw std::invalid_argument("missing option value");
      const std::string value = argv[++i];
      std::size_t used = 0;
      if (arg == "--model") model = value;
      else if (arg == "--sensevoice-dir") sensevoice_dir = value;
      else if (arg == "--fallback-model") fallback_model = value;
      else if (arg == "--vad-model") vad_model = value;
      else if (arg == "--wav") wavs.push_back(value);
      else if (arg == "--pulse-source") source = value;
      else if (arg == "--capture-raw") capture_raw = value;
      else if (arg == "--core-host") core_host = value;
      else if (arg == "--min-score" || arg == "--min-score-raise") {
        const float score = std::stof(value, &used);
        if (used != value.size() || !std::isfinite(score) || score < 0 || score > 1)
          throw std::invalid_argument("quality threshold must be finite and in [0,1]");
        (arg == "--min-score" ? quality.normal : quality.raising) = score;
      }
      else if (arg == "--core-port") {
        core_port = std::stoi(value, &used);
        if (used != value.size() || core_port <= 0 || core_port > 65535)
          throw std::invalid_argument("core port must be in [1,65535]");
      }
      else if (arg == "--seconds") {
        seconds = std::stoi(value, &used);
        if (used != value.size() || seconds < 0 || seconds > 600)
          throw std::invalid_argument("seconds must be in [0,600]");
      } else throw std::invalid_argument("unknown option: " + arg);
    }
    SpeechVad vad;
    if (model.empty() == sensevoice_dir.empty() || vad_model.empty() || (wavs.empty() == source.empty()))
      throw std::invalid_argument("exactly one ASR model, VAD model and exactly one input mode are required");
    if (!capture_raw.empty() && source.empty())
      throw std::invalid_argument("capture-raw requires live PulseAudio input");
    if (core_host.empty() != (core_port == 0))
      throw std::invalid_argument("core host and port must be specified together");
    const bool any_quality = std::isfinite(quality.normal) || std::isfinite(quality.raising);
    if (!sensevoice_dir.empty() && (any_quality || !fallback_model.empty()))
      throw std::invalid_argument("SenseVoice does not provide Whisper quality scores or fallback");
#ifndef HOOD_SEEK_WITH_SENSEVOICE
    if (!sensevoice_dir.empty())
      throw std::invalid_argument("SenseVoice support was not enabled at build time");
#endif
    if (any_quality && !(std::isfinite(quality.normal) && std::isfinite(quality.raising) &&
                         quality.raising >= quality.normal))
      throw std::invalid_argument("both quality thresholds required; raising must not be smaller");
    if (quality.publish && core_host.empty())
      throw std::invalid_argument("publication requires receiver");
    if (quality.publish && sensevoice_dir.empty() && !any_quality)
      throw std::invalid_argument("Whisper publication requires quality thresholds");
    std::unique_ptr<VoiceTcpClient> link;
    if (!core_host.empty())
      link = std::make_unique<VoiceTcpClient>(core_host, static_cast<std::uint16_t>(core_port),
          sensevoice_dir.empty() ? Activation::Hood : Activation::HiLg);
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);
    std::cout << std::unitbuf;
    // Silero의 32ms별 INFO를 생략하고 실패·경고는 stderr에 남긴다.
    whisper_log_set([](enum ggml_log_level level, const char* text, void*) {
      if (level == GGML_LOG_LEVEL_ERROR) engine_error.store(true, std::memory_order_relaxed);
      if (level != GGML_LOG_LEVEL_INFO && level != GGML_LOG_LEVEL_DEBUG) std::fputs(text, stderr);
    }, nullptr);
    auto vad_params = whisper_vad_default_context_params();
    vad_params.n_threads = 1;
    vad_params.use_gpu = false;
    const auto vad_load_started = Clock::now();
    std::unique_ptr<whisper_vad_context, decltype(&whisper_vad_free)> vad_ctx(
        whisper_vad_init_from_file_with_params(vad_model.c_str(), vad_params), whisper_vad_free);
    if (!vad_ctx) throw std::runtime_error("Silero VAD model loading failed");
    std::cout << "event=vad_model_loaded count=1 load_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - vad_load_started).count()
              << " vad_threads=" << vad_params.n_threads << " frame_samples=" << kFrameSamples << " start_ms="
              << kStartVoiceFrames * kFrameSamples * 1000 / kSampleRate
              << " end_ms=" << kEndSilenceFrames * kFrameSamples * 1000 / kSampleRate << '\n';
    const auto load_started = Clock::now();
    auto context_params = whisper_context_default_params();
    context_params.use_gpu = false;
    std::unique_ptr<whisper_context, decltype(&whisper_free)> ctx(nullptr, whisper_free);
    const SherpaOnnxOfflineRecognizer* sensevoice = nullptr;
#ifdef HOOD_SEEK_WITH_SENSEVOICE
    std::unique_ptr<const SherpaOnnxOfflineRecognizer, decltype(&SherpaOnnxDestroyOfflineRecognizer)> sensevoice_owner(
        nullptr, SherpaOnnxDestroyOfflineRecognizer);
    if (!sensevoice_dir.empty()) {
      const auto tokens = sensevoice_dir + "/tokens.txt";
      const auto weights = sensevoice_dir + "/model.int8.onnx";
      if (!std::filesystem::is_regular_file(tokens) || !std::filesystem::is_regular_file(weights))
        throw std::invalid_argument("SenseVoice tokens.txt and model.int8.onnx are required");
      SherpaOnnxOfflineRecognizerConfig config{};
      config.decoding_method = "greedy_search";
      config.feat_config.sample_rate = kSampleRate;
      config.feat_config.feature_dim = 80;
      config.model_config.num_threads = 4;
      config.model_config.provider = "cpu";
      config.model_config.tokens = tokens.c_str();
      config.model_config.sense_voice.model = weights.c_str();
      config.model_config.sense_voice.language = "ko";
      config.model_config.sense_voice.use_itn = true;
      sensevoice_owner.reset(SherpaOnnxCreateOfflineRecognizer(&config));
      sensevoice = sensevoice_owner.get();
      if (!sensevoice) throw std::runtime_error("SenseVoice model loading failed");
    } else
#endif
    {
      ctx.reset(whisper_init_from_file_with_params(model.c_str(), context_params));
      if (!ctx) throw std::runtime_error("Whisper model loading failed");
    }
    std::cout << "event=model_loaded count=1 load_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - load_started).count()
              << " model=" << (sensevoice ? "sensevoice" : "whisper")
              << " vad=streaming_silero mode=" << (quality.publish ? "publish" : "observation_only") << '\n';
    std::unique_ptr<whisper_context, decltype(&whisper_free)> fallback_ctx(nullptr, whisper_free);
    if (!fallback_model.empty()) {
      const auto fallback_started = Clock::now();
      fallback_ctx.reset(whisper_init_from_file_with_params(fallback_model.c_str(), context_params));
      if (!fallback_ctx) throw std::runtime_error("Whisper fallback model loading failed");
      std::cout << "event=fallback_model_loaded count=1 load_ms="
                << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - fallback_started).count() << '\n';
    }
    std::cout << "event=recognition_config model=" << (sensevoice ? "sensevoice" : "whisper");
    if (sensevoice) {
      std::cout << " decoder=greedy language=ko use_itn=1 retry_limit=0 conf_kind=unavailable wire_conf=0 parser=hi_lg_jamo terminal_question=command_punctuation fan_on_publish=level1 bare_action=none activation_min=0.60 target_min=0.60 action_min=0.70 margin=0.10\n";
    } else {
      std::cout << " normal_threshold=" << quality.normal << " raising_threshold=" << quality.raising
                << " fallback_enabled=" << bool(fallback_ctx) << " retry_limit=1 repeat_beam=3 other_beam=5\n";
    }
    Stats stats;
    for (const auto& wav : wavs) {
      if (stopping) break;
      if (std::filesystem::file_size(wav) > 20 * 1024 * 1024)
        throw std::runtime_error("audio file exceeds 20 MiB");
      std::vector<float> pcm;
      std::vector<std::vector<float>> stereo;
      if (!read_audio_data(wav, pcm, stereo, false) || pcm.empty() || pcm.size() > 600 * kSampleRate)
        throw std::runtime_error("invalid audio or duration exceeds 600 seconds");
      vad = SpeechVad{};
      whisper_vad_reset_state(vad_ctx.get());
      std::cout << "event=input path=" << std::quoted(wav) << " samples=" << pcm.size() << '\n';
      for (std::size_t offset = 0; !stopping && offset < pcm.size(); offset += kFrameSamples) {
        AudioFrame frame{};
        std::copy_n(pcm.begin() + offset, std::min<std::size_t>(kFrameSamples, pcm.size() - offset), frame.begin());
        consume_frame(frame, vad, vad_ctx.get(), ctx.get(), fallback_ctx.get(), sensevoice, stats, false, link.get(), quality, Clock::now());
      }
      // EOF로 발화를 확정하지 않는다. 종료 무음이 없는 미완료 구간은 버린다.
      std::cout << "event=input_done";
      if (const auto noise = vad.noise_dbfs()) std::cout << " noise_dbfs=" << *noise;
      std::cout << '\n';
    }
    if (!source.empty()) {
#ifdef HOOD_SEEK_WITH_PULSE
      observe_microphone(source, seconds, capture_raw, vad, vad_ctx.get(), ctx.get(), fallback_ctx.get(), sensevoice, stats, link.get(), quality);
#else
      throw std::runtime_error("PulseAudio capture was not enabled at build time");
#endif
    }
    if (link) std::cout << "event=link_summary connected=" << link->connected()
                        << " received_lines=" << link->received_lines() << " transmitted=" << stats.transmitted << '\n';
    std::cout << "event=stopped frames=" << stats.frames << " peak_dbfs=" << stats.peak_dbfs
              << " asr_calls=" << stats.asr_calls << " overlong_drops=" << stats.too_long
              << " utterances=" << stats.utterances << " retry_calls=" << stats.retry_calls
              << " vad_us=" << stats.vad_us << " max_vad_us=" << stats.max_vad_us
              << " candidates=" << stats.candidates << " transmitted=" << stats.transmitted << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "event=error message=" << std::quoted(error.what()) << '\n';
    return 1;
  }
}
