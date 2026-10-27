#include "audio_pipeline.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace hoodseek::voice {

float frame_dbfs(const AudioFrame& frame) {
  double sum = 0;
  for (const auto sample : frame) {
    if (!std::isfinite(sample) || sample < -1 || sample > 1)
      throw std::invalid_argument("PCM sample must be finite and in [-1,1]");
    sum += static_cast<double>(sample) * sample;
  }
  return sum == 0 ? -std::numeric_limits<float>::infinity()
                  : static_cast<float>(10 * std::log10(sum / frame.size()));
}

std::optional<float> mean_token_score(const std::vector<float>& scores) {
  if (scores.empty()) return std::nullopt;
  double sum = 0;
  for (const auto score : scores) {
    if (!std::isfinite(score) || score < 0 || score > 1) return std::nullopt;
    sum += score;
  }
  return static_cast<float>(sum / scores.size());
}

std::optional<std::uint64_t> capture_buffer_age_ns(std::uint64_t start_ns,
                                                 std::uint64_t duration_ns,
                                                 std::uint64_t clock_ns) {
  constexpr auto invalid = std::numeric_limits<std::uint64_t>::max();
  if (start_ns == invalid || clock_ns == invalid || duration_ns == 0 ||
      duration_ns > 1000000000ULL || start_ns > clock_ns)
    return std::nullopt;
  const auto age_start = clock_ns - start_ns;
  if (age_start > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    return std::nullopt;
  // Never replace an old timestamp with receipt time or extend the 5 s deadline.
  return duration_ns <= age_start ? age_start - duration_ns : age_start;
}

void SpeechVad::discard_until_silence() {
  voiced_ = quiet_ = 0;
  utterance_samples_ = 0;
  active_ = false;
  discarding_ = true;
  history_.clear();
  samples_.clear();
}

std::optional<VadResult> SpeechVad::push(const AudioFrame& frame, float speech_probability) {
  const float level = frame_dbfs(frame);  // 무음에서도 PCM 입력 경계를 검사한다.
  if (!std::isfinite(speech_probability) || speech_probability < 0 || speech_probability > 1)
    throw std::invalid_argument("VAD speech probability must be finite and in [0,1]");
  // 시작보다 낮은 종료 문턱으로 작은 끝 음절을 보존한다. 음량은 발화 게이트가 아니다.
  const bool voice = speech_probability >= (active_ || discarding_ ? 0.35f : 0.5f);
  if (discarding_) {
    quiet_ = voice ? 0 : quiet_ + 1;
    if (quiet_ >= kEndSilenceFrames) {
      discarding_ = false;
      quiet_ = 0;
    }
    return std::nullopt;
  }
  if (!active_) {
    // 별도 배경 녹음 없이 확실한 비음성에서만 느리게 갱신한다. 진단 전용이며
    // 발화 시작·종료나 명령 수용에 사용하지 않는다. 디지털 무음은 통계에 넣지 않는다.
    if (speech_probability <= 0.15f && std::isfinite(level)) {
      constexpr float dt = static_cast<float>(kFrameSamples) / kSampleRate;
      noise_history_.push_back(level);
      if (noise_history_.size() > 32) noise_history_.pop_front();
      // 처음의 거의 무음인 한 프레임을 배경으로 고정하지 않는다. 이 통계가
      // 준비되기 전에도 발화 판정은 즉시 작동하며 사용자는 기다리지 않는다.
      if (noise_history_.size() == 32) {
        std::vector<float> sorted(noise_history_.begin(), noise_history_.end());
        std::sort(sorted.begin(), sorted.end());
        const float median = (sorted[15] + sorted[16]) / 2;
        if (!noise_dbfs_) noise_dbfs_ = median;
        else *noise_dbfs_ += std::clamp((median - *noise_dbfs_) * dt / (3.0f + dt), -dt, dt);
      }
    }
    history_.push_back(frame);
    // 시작 확인 224 ms + 직전 992 ms를 보존한다. 발화/전체 상한은 별도로 센다.
    if (history_.size() > kPrerollFrames + kStartVoiceFrames) history_.pop_front();
    voiced_ = voice ? voiced_ + 1 : 0;
    if (voiced_ < kStartVoiceFrames) return std::nullopt;
    for (const auto& previous : history_)
      samples_.insert(samples_.end(), previous.begin(), previous.end());
    history_.clear();
    active_ = true;
    utterance_samples_ = kStartVoiceFrames * kFrameSamples;
    quiet_ = 0;
    return std::nullopt;
  }
  samples_.insert(samples_.end(), frame.begin(), frame.end());
  utterance_samples_ += kFrameSamples;
  quiet_ = voice ? 0 : quiet_ + 1;
  if (quiet_ >= kEndSilenceFrames) {
    auto samples = std::move(samples_);
    samples_.clear();
    active_ = false;
    voiced_ = quiet_ = 0;
    utterance_samples_ = 0;
    return VadResult{std::move(samples), false};
  }
  // ponytail: 제한 초과 발화는 분할 실행하지 않는다. 끝의 부정어가 잘릴 수 있다.
  // Experimental: detector start to last voice includes internal pauses, but
  // excludes pre-roll and terminal silence. Keep an independent total bound.
  const int speech_span = utterance_samples_ - quiet_ * kFrameSamples;
  if (speech_span >= 4 * kSampleRate || samples_.size() >= 6 * kSampleRate) {
    discard_until_silence();
    return VadResult{{}, true};
  }
  return std::nullopt;
}

}  // namespace hoodseek::voice
