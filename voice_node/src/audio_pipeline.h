#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace hoodseek::voice {

constexpr int kSampleRate = 16000;
constexpr int kFrameSamples = 512;  // Silero 16 kHz의 한 입력 조각, 32 ms
constexpr int kStartVoiceFrames = 7;  // 224 ms, 200 ms 이상을 조각 단위로 확인
constexpr int kEndSilenceFrames = 17;  // 544 ms, 종료 대기
constexpr int kPrerollFrames = 31;  // 992 ms, 앞의 짧은 활성어 보존
using AudioFrame = std::array<float, kFrameSamples>;

struct VadResult {
  std::vector<float> samples;
  bool too_long;
};

float frame_dbfs(const AudioFrame& frame);
std::optional<float> mean_token_score(const std::vector<float>& scores);

// All times use the same pipeline running-time clock. If the buffer end is
// ahead of that clock, its known start is the conservative freshness bound.
std::optional<std::uint64_t> capture_buffer_age_ns(std::uint64_t start_ns,
                                                 std::uint64_t duration_ns,
                                                 std::uint64_t clock_ns);

class SpeechVad {
 public:
  std::optional<VadResult> push(const AudioFrame& frame, float speech_probability);
  void discard_until_silence();
  std::optional<float> noise_dbfs() const { return noise_dbfs_; }

 private:
  std::optional<float> noise_dbfs_;
  std::deque<float> noise_history_;
  int voiced_ = 0;
  int quiet_ = 0;
  int utterance_samples_ = 0;
  bool active_ = false;
  bool discarding_ = false;
  std::deque<AudioFrame> history_;
  std::vector<float> samples_;
};

}  // namespace hoodseek::voice
