#include "preview.hpp"
#include <opencv2/imgproc.hpp>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef HOODSEEK_ECOAI
#include <dlfcn.h>
#else
#include <opencv2/imgcodecs.hpp>
#endif

namespace hoodseek {
namespace {
// kPollMs only bounds how late a lost wake or a stop is noticed; offers and the destructor wake the poll at once.
// kOfferGapMs: the camera reader offers every frame (~30 Hz); at most ~16 a second (15 from a 30 fps camera) reach
// the card, so JPEG encoding here and decoding in the app, both on the Pi 5 that runs inference, stay small.
constexpr int kQuality = 70, kMaxViewers = 2, kPollMs = 100, kSendWaitMs = 200, kMaxEncodeFailures = 10, kOfferGapMs = 60;
// The card favours responsiveness over detail (병직 10/26): 640x480 goes out at 3/4 (480x360), about half the pixels
// to encode here and decode in the app. Judgment still uses the full frame.
constexpr double kScale = 0.75;
const std::string kBoundary = "hoodseekframe";
const std::string kResponse = "HTTP/1.0 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=" + kBoundary +
                              "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";

// Whole buffer within 200 ms or false. A viewer that cannot take a frame in time is dropped: a half-sent part would
// corrupt the stream, and only this display thread ever waits on it.
bool send_all(int fd, const std::string& data) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kSendWaitMs);
  for (size_t sent = 0; sent < data.size();) {
    const auto n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);  // a viewer that left must not SIGPIPE the node
    if (n > 0) { sent += static_cast<size_t>(n); continue; }
    if (n < 0 && errno == EINTR) continue;
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    pollfd wait{fd, POLLOUT, 0};
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && left > 0 && poll(&wait, 1, static_cast<int>(left)) == 1) continue;
    return false;
  }
  return true;
}
}  // namespace

#ifdef HOODSEEK_ECOAI
// The ATLAS image ships libturbojpeg.so.0 (libjpeg-turbo) but the SDK has neither its header nor a link library, so
// the four TurboJPEG calls are declared from turbojpeg.h and resolved at start (TJPF_BGR = 1, TJSAMP_420 = 2).
struct PreviewServer::Jpeg {
  using Init = void* (*)();
  using Compress = int (*)(void*, const unsigned char*, int, int, int, int, unsigned char**, unsigned long*, int, int, int);
  using Free = void (*)(unsigned char*);
  using Destroy = int (*)(void*);
  void* library = nullptr;
  void* handle = nullptr;
  Compress compress = nullptr;
  Free release = nullptr;
  Destroy destroy = nullptr;
  Jpeg() {
    library = dlopen("libturbojpeg.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!library) throw std::runtime_error(std::string("no JPEG codec: ") + dlerror());
    const auto init = reinterpret_cast<Init>(dlsym(library, "tjInitCompress"));
    compress = reinterpret_cast<Compress>(dlsym(library, "tjCompress2"));
    release = reinterpret_cast<Free>(dlsym(library, "tjFree"));
    destroy = reinterpret_cast<Destroy>(dlsym(library, "tjDestroy"));
    if (!init || !compress || !release || !destroy || !(handle = init())) {
      dlclose(library);
      throw std::runtime_error("libturbojpeg.so.0 lacks the TurboJPEG 2 API");
    }
  }
  ~Jpeg() { destroy(handle); dlclose(library); }
  bool encode(const cv::Mat& bgr, std::vector<unsigned char>& out) {
    unsigned char* buffer = nullptr;
    unsigned long size = 0;
    const bool ok = compress(handle, bgr.data, bgr.cols, static_cast<int>(bgr.step), bgr.rows, 1, &buffer, &size, 2, kQuality, 0) == 0;
    if (ok) out.assign(buffer, buffer + size);
    release(buffer);  // tjFree(NULL) does nothing
    return ok;
  }
};
#else
// PC build (tests): imgcodecs is already linked for --export-input.
struct PreviewServer::Jpeg {
  bool encode(const cv::Mat& bgr, std::vector<unsigned char>& out) { return cv::imencode(".jpg", bgr, out, {cv::IMWRITE_JPEG_QUALITY, kQuality}); }
};
#endif

PreviewServer::PreviewServer(int port) : jpeg_(std::make_unique<Jpeg>()) {
  if (port < 0 || port > 65535) throw std::runtime_error("preview port must be 0..65535");
  wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (wake_fd_ < 0) throw std::runtime_error(std::string("preview eventfd: ") + std::strerror(errno));
  listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (listen_fd_ < 0) {
    const std::string reason = std::strerror(errno);
    close(wake_fd_);
    throw std::runtime_error("preview socket: " + reason);
  }
  int one = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);  // systemd restarts within 1 s
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // the app runs on the same Pi 5; never on the LAN (#72)
  socklen_t length = sizeof address;
  if (bind(listen_fd_, reinterpret_cast<const sockaddr*>(&address), sizeof address) < 0 || listen(listen_fd_, 4) < 0 ||
      getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
    const std::string reason = std::strerror(errno);
    close(listen_fd_);
    close(wake_fd_);
    throw std::runtime_error("preview port " + std::to_string(port) + ": " + reason);
  }
  port_ = ntohs(address.sin_port);
  thread_ = std::thread([this] { run(); });
}

PreviewServer::~PreviewServer() {
  stop_ = true;
  wake();
  if (thread_.joinable()) thread_.join();
  if (listen_fd_ >= 0) close(listen_fd_);
  close(wake_fd_);
}

void PreviewServer::offer(const cv::Mat& bgr) {
  if (!watching_) return;
  const auto now = std::chrono::steady_clock::now();
  if (now - last_offer_ < std::chrono::milliseconds(kOfferGapMs)) return;
  last_offer_ = now;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    bgr.copyTo(latest_);
  }
  wake();
}

void PreviewServer::wake() {
  const uint64_t one = 1;
  if (write(wake_fd_, &one, sizeof one) < 0) {}  // only fails when 2^64-2 wakes are pending; the poll timeout covers it
}

void PreviewServer::run() {
  std::vector<int> viewers;
  int encode_failures = 0;
  try {
    std::vector<unsigned char> jpeg;
    cv::Mat small;
    while (!stop_) {
      std::vector<pollfd> wait{{listen_fd_, POLLIN, 0}, {wake_fd_, POLLIN, 0}};
      for (int fd : viewers) wait.push_back({fd, POLLIN, 0});
      if (poll(wait.data(), wait.size(), kPollMs) < 0 && errno != EINTR) throw std::runtime_error(std::strerror(errno));
      if (wait[1].revents & POLLIN) {
        uint64_t pending;
        if (read(wake_fd_, &pending, sizeof pending) < 0) {}  // EAGAIN: already drained
      }
      // The request is not parsed: any client gets the stream. Readable = request bytes (dropped) or the viewer left.
      for (size_t i = viewers.size(); i-- > 0;) {
        if (!wait[i + 2].revents) continue;
        char buffer[1024];
        const auto n = recv(viewers[i], buffer, sizeof buffer, MSG_DONTWAIT);
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
          close(viewers[i]);
          viewers.erase(viewers.begin() + static_cast<std::ptrdiff_t>(i));
        }
      }
      if (wait[0].revents & POLLIN) {
        const int fd = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd >= 0 && static_cast<int>(viewers.size()) < kMaxViewers && send_all(fd, kResponse)) {
          if (viewers.empty()) {
            // A frame offered just before the last viewer left would otherwise reach this one minutes old.
            std::lock_guard<std::mutex> lock(mutex_);
            latest_.release();
          }
          viewers.push_back(fd);
        } else if (fd >= 0) {
          close(fd);
        }
      }
      watching_ = !viewers.empty();
      cv::Mat frame;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        cv::swap(frame, latest_);
      }
      if (frame.empty() || viewers.empty()) continue;
      // One failure is skipped; a codec that keeps failing (wrong library build) stops the video instead of
      // copying a frame for nothing up to 16 times a second.
      cv::resize(frame, small, cv::Size(), kScale, kScale, cv::INTER_AREA);
      if (!jpeg_->encode(small, jpeg)) {
        if (++encode_failures >= kMaxEncodeFailures) throw std::runtime_error("JPEG encode failed 10 times in a row");
        continue;
      }
      encode_failures = 0;
      std::string part = "--" + kBoundary + "\r\nContent-Type: image/jpeg\r\nContent-Length: " + std::to_string(jpeg.size()) + "\r\n\r\n";
      part.append(jpeg.begin(), jpeg.end()).append("\r\n");
      for (size_t i = viewers.size(); i-- > 0;) {
        if (send_all(viewers[i], part)) continue;
        close(viewers[i]);
        viewers.erase(viewers.begin() + static_cast<std::ptrdiff_t>(i));
      }
    }
  } catch (const std::exception& e) {
    // A display failure ends the preview only; the observer keeps running.
    std::cerr << "hoodseek_vision: preview stopped: " << e.what() << '\n';
    close(listen_fd_);  // refuse new viewers instead of leaving them connected to nothing
    listen_fd_ = -1;
  }
  watching_ = false;
  for (int fd : viewers) close(fd);
}
}  // namespace hoodseek
