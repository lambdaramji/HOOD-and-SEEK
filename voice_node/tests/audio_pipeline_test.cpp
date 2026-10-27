#include "audio_pipeline.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace hoodseek::voice;

int main() {
  int checks = 0;
  int failures = 0;
  const auto check = [&](bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
  };
  AudioFrame zero{};
  check(kEndSilenceFrames * kFrameSamples * 1000 / kSampleRate == 544,
        "shared VAD completion and publication age baseline");
  AudioFrame voice{};
  voice.fill(0.1f);
  check(std::isinf(frame_dbfs(zero)) && frame_dbfs(zero) < 0, "digital silence");
  check(std::abs(frame_dbfs(voice) + 20) < 0.001, "RMS dBFS");
  SpeechVad silence;
  for (int i = 0; i < 500; ++i) check(!silence.push(zero, 0.f), "no silence segment");
  SpeechVad short_sound;
  for (int i = 0; i < kStartVoiceFrames - 1; ++i) check(!short_sound.push(voice, .9f), "short sound");
  for (int i = 0; i < kEndSilenceFrames; ++i) check(!short_sound.push(zero, 0.f), "short sound discarded");

  SpeechVad vad;
  for (int i = 0; i < 60; ++i) check(!vad.push(zero, 0.f), "idle");
  for (int i = 0; i < 20; ++i) check(!vad.push(voice, .9f), "voice waits for end");
  for (int i = 0; i < kEndSilenceFrames - 1; ++i) check(!vad.push(zero, 0.f), "less than shared end wait is not end");
  const auto complete = vad.push(zero, 0.f);
  check(complete && !complete->too_long, "shared end wait");
  check(complete && complete->samples.size() == (kPrerollFrames + 20 + kEndSilenceFrames) * kFrameSamples, "preroll retained");
  if (complete) {
    int voiced_samples = 0;
    for (const auto sample : complete->samples) if (sample != 0) ++voiced_samples;
    check(voiced_samples == 20 * kFrameSamples, "onset voice retained");
  }
  SpeechVad delayed_onset;
  for (int i = 0; i < 50; ++i) delayed_onset.push(zero, 0.f);
  for (int i = 0; i < 3; ++i) delayed_onset.push(voice, .9f);  // 짧은 활성 단어
  for (int i = 0; i < 20; ++i) delayed_onset.push(zero, 0.f);  // 단어 사이 휴지
  for (int i = 0; i < 20; ++i) delayed_onset.push(voice, .9f);
  for (int i = 0; i < kEndSilenceFrames - 1; ++i) delayed_onset.push(zero, 0.f);
  const auto delayed = delayed_onset.push(zero, 0.f);
  int preserved = 0;
  if (delayed) for (const auto sample : delayed->samples) if (sample != 0) ++preserved;
  check(delayed && preserved == 23 * kFrameSamples, "short wake word before delayed onset");
  for (int utterance = 0; utterance < 2; ++utterance) {
    for (int i = 0; i < 15; ++i) check(!vad.push(voice, .9f), "next utterance onset");
    for (int i = 0; i < kEndSilenceFrames - 1; ++i) check(!vad.push(zero, 0.f), "next utterance tail");
    check(vad.push(zero, 0.f).has_value(), "next utterance completed");
  }
  // Pre-roll and terminal silence must not consume the four-second speech span.
  SpeechVad almost_four_seconds;
  for (int i = 0; i < 60; ++i) almost_four_seconds.push(zero, 0.f);
  for (int i = 0; i < 120; ++i)
    check(!almost_four_seconds.push(voice, .9f), "3.84s speech retained");
  for (int i = 0; i < kEndSilenceFrames - 1; ++i)
    check(!almost_four_seconds.push(zero, 0.f), "terminal silence excluded from speech span");
  const auto near_limit = almost_four_seconds.push(zero, 0.f);
  check(near_limit && !near_limit->too_long, "preroll plus 3.84s speech plus end wait fits");

  SpeechVad long_voice;
  for (int i = 0; i < 4 * kSampleRate / kFrameSamples - 1; ++i) check(!long_voice.push(voice, .9f), "under 4 seconds");
  const auto truncated = long_voice.push(voice, .9f);
  check(truncated && truncated->too_long && truncated->samples.empty(), "4 seconds drop");
  for (int i = 0; i < 300; ++i) check(!long_voice.push(voice, .9f), "no split commands");
  for (int i = 0; i < kEndSilenceFrames; ++i) check(!long_voice.push(zero, 0.f), "rearm silence");
  for (int i = 0; i < 15; ++i) check(!long_voice.push(voice, .9f), "rearmed onset");
  for (int i = 0; i < kEndSilenceFrames - 1; ++i) check(!long_voice.push(zero, 0.f), "rearmed tail");
  check(long_voice.push(zero, 0.f).has_value(), "recover after overlong utterance");
  vad.discard_until_silence();
  for (int i = 0; i < 80; ++i) check(!vad.push(voice, .9f), "discard audio during inference");
  for (int i = 0; i < kEndSilenceFrames - 1; ++i) check(!vad.push(zero, 0.f), "guard silence");
  check(!vad.push(zero, 0.f), "guard releases without an utterance");

  for (float probability : {-0.1f, 1.1f, std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()}) {
    bool rejected = false;
    try { SpeechVad invalid; invalid.push(zero, probability); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "invalid VAD probability");
  }
  // 큰 비음성도 발화로 만들지 않으며 작은 발화도 음량으로 차단하지 않는다.
  SpeechVad probability_gate;
  for (int i = 0; i < 100; ++i)
    check(!probability_gate.push(voice, .1f), "loud non-speech is not an utterance");
  AudioFrame soft{};
  soft.fill(.0001f);
  const auto background = probability_gate.noise_dbfs();
  for (int i = 0; i < kStartVoiceFrames; ++i)
    check(!probability_gate.push(soft, .9f), "quiet speech does not require noise calibration");
  for (int i = 0; i < kEndSilenceFrames - 1; ++i)
    check(!probability_gate.push(zero, 0.f), "wait before final quiet syllable");
  check(!probability_gate.push(soft, .4f), "lower exit threshold preserves a final syllable");
  for (int i = 0; i < kEndSilenceFrames - 1; ++i)
    check(!probability_gate.push(zero, 0.f), "final syllable restarts end wait");
  const auto quiet_speech = probability_gate.push(zero, 0.f);
  int quiet_preserved = 0;
  if (quiet_speech) for (auto x : quiet_speech->samples) if (x == .0001f) ++quiet_preserved;
  check(quiet_speech && quiet_preserved == (kStartVoiceFrames + 1) * kFrameSamples,
        "quiet onset and final syllable retained");
  check(probability_gate.noise_dbfs() == background, "noise estimate frozen throughout speech");
  SpeechVad background_tracker;
  check(!background_tracker.noise_dbfs(), "startup does not assume a silent calibration");
  for (int i = 0; i < 32; ++i) background_tracker.push(soft, .1f);
  const auto before_noise = background_tracker.noise_dbfs();
  background_tracker.push(voice, .1f);
  check(before_noise && background_tracker.noise_dbfs() &&
        *background_tracker.noise_dbfs() - *before_noise <=
            static_cast<float>(kFrameSamples) / kSampleRate + .0001f,
        "background rise limited to 1 dB per second");
  SpeechVad initial_outlier;
  initial_outlier.push(soft, .1f);
  check(!initial_outlier.noise_dbfs(), "first quiet sample does not fix the noise baseline");
  for (int i = 0; i < 31; ++i) initial_outlier.push(voice, .1f);
  check(initial_outlier.noise_dbfs() && std::abs(*initial_outlier.noise_dbfs() + 20) < .001f,
        "initial background uses a median rather than a startup outlier");
  SpeechVad immediate;
  for (int i = 0; i < kStartVoiceFrames; ++i) immediate.push(soft, .9f);
  for (int i = 0; i < kEndSilenceFrames - 1; ++i) immediate.push(zero, 0.f);
  check(immediate.push(zero, 0.f).has_value() && !immediate.noise_dbfs(),
        "speech from the first frame works before any background statistics");
  for (float sample : {1.1f, -1.1f, std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()}) {
    auto bad = voice;
    bad[0] = sample;
    bool rejected = false;
    try { silence.push(bad, .9f); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "invalid PCM sample");
  }
  check(!mean_token_score({}), "no text tokens");
  check(mean_token_score({0, 1}).value_or(-1) == 0.5f, "mean diagnostic score");
  for (float bad : {-0.1f, 1.1f, std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::quiet_NaN()})
    check(!mean_token_score({0.5f, bad}), "invalid token score");
  check(capture_buffer_age_ns(100, 20, 150) == 30, "capture end age preserves old buffers");
  check(capture_buffer_age_ns(100, 20, 120) == 0, "capture end exactly at clock");
  check(capture_buffer_age_ns(100, 20, 110) == 10, "ahead end uses older start bound");
  check(capture_buffer_age_ns(100, 20, 100) == 0, "capture start at clock");
  check(!capture_buffer_age_ns(101, 20, 100), "future capture start rejected");
  check(!capture_buffer_age_ns(UINT64_MAX, 20, 100), "missing capture timestamp rejected");
  check(!capture_buffer_age_ns(100, 20, UINT64_MAX), "missing clock rejected");
  check(!capture_buffer_age_ns(100, 0, 110), "empty capture duration rejected");
  check(!capture_buffer_age_ns(100, 1000000001ULL, 110), "oversized capture rejected");
  check(capture_buffer_age_ns(0, 10000000, 6000000000ULL) == 5990000000ULL,
        "stale capture retains age beyond five seconds");
  std::cout << "audio_checks=" << checks << " failures=" << failures << '\n';
  return failures == 0 ? 0 : 1;
}
