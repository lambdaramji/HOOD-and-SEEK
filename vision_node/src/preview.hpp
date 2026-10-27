#pragma once
#include <opencv2/core.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

namespace hoodseek {
// Camera card video for the Pi 5 app (UI design v1 §3.2): the frames vision-node already read, as HTTP MJPEG on
// 127.0.0.1 only. Display only — it never reaches SafetyCore, never feeds a judgment and nothing is stored.
class PreviewServer {
 public:
  // 0 = any free port (tests). Throws when the JPEG codec is missing or the port cannot be bound.
  explicit PreviewServer(int port);
  ~PreviewServer();
  PreviewServer(const PreviewServer&) = delete;
  PreviewServer& operator=(const PreviewServer&) = delete;
  int port() const { return port_; }
  // Camera reader side, one thread only: copies the frame only while a viewer is connected and at most every 60 ms,
  // and never touches the network.
  void offer(const cv::Mat& bgr);

 private:
  struct Jpeg;
  void run();
  std::unique_ptr<Jpeg> jpeg_;
  void wake();
  int listen_fd_ = -1, wake_fd_ = -1, port_ = 0;  // wake_fd_: eventfd in the server's poll, so an offer goes out at once
  std::atomic<bool> stop_{false}, watching_{false};
  std::mutex mutex_;
  cv::Mat latest_;  // non-empty = a frame the server thread has not sent yet
  std::chrono::steady_clock::time_point last_offer_{};  // offer's thread only
  std::thread thread_;
};
}  // namespace hoodseek
