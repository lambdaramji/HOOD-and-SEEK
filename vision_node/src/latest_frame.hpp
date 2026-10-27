#pragma once
#include <opencv2/core.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>

namespace hoodseek {
// Camera reader thread -> inference loop. Holds the newest frame only: one the loop has not taken yet is overwritten,
// never queued, so inference starts on the latest camera frame as V4L2Camera::read alone did. Buffers are swapped,
// never shared, so the reader never writes into a frame the loop holds.
class LatestFrame {
 public:
  // `frame` comes back holding a buffer the caller may overwrite.
  void put(cv::Mat& frame) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      cv::swap(frame, latest_);
      fresh_ = true;
    }
    ready_.notify_one();
  }
  // The reader stopped; take throws `reason` so the fault record keeps it.
  void fail(const std::string& reason) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      failure_ = reason.empty() ? "camera reader stopped" : reason;
    }
    ready_.notify_one();
  }
  // A frame newer than the last take, waiting up to wait_ms; false = none yet. Throws once the reader failed.
  bool take(cv::Mat& frame, int wait_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    ready_.wait_for(lock, std::chrono::milliseconds(wait_ms), [this] { return fresh_ || !failure_.empty(); });
    if (!failure_.empty()) throw std::runtime_error(failure_);
    if (!fresh_) return false;
    cv::swap(frame, latest_);
    fresh_ = false;
    return true;
  }

 private:
  std::mutex mutex_;
  std::condition_variable ready_;
  cv::Mat latest_;
  bool fresh_ = false;
  std::string failure_;  // non-empty = the reader stopped
};
}  // namespace hoodseek
