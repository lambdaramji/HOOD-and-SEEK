#include "v4l2_camera.hpp"
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <opencv2/imgproc.hpp>

namespace hoodseek {
namespace {
int xioctl(int fd, unsigned long request, void* arg) {
  int r;
  do { r = ioctl(fd, request, arg); } while (r == -1 && errno == EINTR);
  return r;
}

// ITU-R BT.601 YUYV -> BGR conversion (same constants OpenCV's
// COLOR_YUV2BGR_YUYV uses); one UV pair covers two adjacent pixels.
void yuyv_to_bgr(const uint8_t* yuyv, size_t stride, cv::Mat& bgr, int width, int height) {
  auto clamp = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); };
  for (int y = 0; y < height; ++y) {
    const uint8_t* row = yuyv + static_cast<size_t>(y) * stride;
    auto* out = bgr.ptr<cv::Vec3b>(y);
    for (int x = 0; x < width; x += 2) {
      int y0 = row[x * 2], u = row[x * 2 + 1], y1 = row[x * 2 + 2], v = row[x * 2 + 3];
      int d = u - 128, e = v - 128, c0 = y0 - 16, c1 = y1 - 16;
      out[x] = cv::Vec3b(clamp((298 * c0 + 516 * d + 128) >> 8), clamp((298 * c0 - 100 * d - 208 * e + 128) >> 8),
                          clamp((298 * c0 + 409 * e + 128) >> 8));
      out[x + 1] = cv::Vec3b(clamp((298 * c1 + 516 * d + 128) >> 8), clamp((298 * c1 - 100 * d - 208 * e + 128) >> 8),
                              clamp((298 * c1 + 409 * e + 128) >> 8));
    }
  }
}
}  // namespace

V4L2Camera::V4L2Camera(const std::string& device, int width, int height, bool rotate180)
    : width_(width), height_(height), rotate180_(rotate180) {
  fd_ = open(device.c_str(), O_RDWR);
  if (fd_ < 0) throw std::runtime_error("cannot open camera device: " + device);
  v4l2_capability cap{};
  if (xioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0 || !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
      !(cap.capabilities & V4L2_CAP_STREAMING)) {
    close(fd_);
    throw std::runtime_error("camera does not support V4L2 streaming capture: " + device);
  }
  v4l2_format fmt{};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = static_cast<__u32>(width_);
  fmt.fmt.pix.height = static_cast<__u32>(height_);
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (xioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
    close(fd_);
    throw std::runtime_error("camera rejected requested format (" + std::to_string(width_) + "x" +
                              std::to_string(height_) + " YUYV)");
  }
  if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
    close(fd_);
    throw std::runtime_error("camera did not negotiate YUYV; unsupported pixel format");
  }
  width_ = static_cast<int>(fmt.fmt.pix.width);
  height_ = static_cast<int>(fmt.fmt.pix.height);
  stride_ = fmt.fmt.pix.bytesperline ? fmt.fmt.pix.bytesperline : static_cast<size_t>(width_) * 2;
  if (width_ <= 0 || height_ <= 0 || width_ % 2 || stride_ < static_cast<size_t>(width_) * 2) {
    close(fd_);
    throw std::runtime_error("camera negotiated an unusable YUYV layout");
  }

  v4l2_requestbuffers req{};
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
    close(fd_);
    throw std::runtime_error("camera buffer allocation failed");
  }
  buffers_.resize(req.count);
  for (unsigned i = 0; i < req.count; ++i) {
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (xioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
      close(fd_);
      throw std::runtime_error("camera buffer query failed");
    }
    if (buf.length < stride_ * static_cast<size_t>(height_)) {
      close(fd_);
      throw std::runtime_error("camera buffer smaller than one frame");
    }
    void* start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);
    if (start == MAP_FAILED) {
      close(fd_);
      throw std::runtime_error("camera buffer mmap failed");
    }
    buffers_[i] = {start, buf.length};
    if (xioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
      close(fd_);
      throw std::runtime_error("camera buffer queue failed");
    }
  }
  auto type = static_cast<v4l2_buf_type>(V4L2_BUF_TYPE_VIDEO_CAPTURE);
  if (xioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
    close(fd_);
    throw std::runtime_error("camera stream start failed");
  }
}

V4L2Camera::~V4L2Camera() {
  if (fd_ < 0) return;
  auto type = static_cast<v4l2_buf_type>(V4L2_BUF_TYPE_VIDEO_CAPTURE);
  xioctl(fd_, VIDIOC_STREAMOFF, &type);
  for (auto& buffer : buffers_)
    if (buffer.start) munmap(buffer.start, buffer.length);
  close(fd_);
}

bool V4L2Camera::read(cv::Mat& out) {
  const size_t frame_bytes = stride_ * static_cast<size_t>(height_);
  for (;;) {
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_DQBUF, &buf) < 0 || buf.index >= buffers_.size()) return false;
    // Inference (~290 ms) is slower than capture, so the buffer FIFO served frames 1.4 s old on average
    // [실측 2026-10-19 Pi 5]. Hand every older ready buffer straight back and keep only the newest.
    for (pollfd ready{fd_, POLLIN, 0}; poll(&ready, 1, 0) == 1 && (ready.revents & POLLIN);) {
      v4l2_buffer newer{};
      newer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      newer.memory = V4L2_MEMORY_MMAP;
      if (xioctl(fd_, VIDIOC_DQBUF, &newer) < 0) break;
      if (newer.index >= buffers_.size() || xioctl(fd_, VIDIOC_QBUF, &buf) < 0) return false;
      buf = newer;
    }
    // A short or error-flagged frame (USB hiccup) is skipped rather than inferred as a torn image;
    // the stall watchdog ends the process if no usable frame follows.
    if ((buf.flags & V4L2_BUF_FLAG_ERROR) || buf.bytesused < frame_bytes) {
      if (xioctl(fd_, VIDIOC_QBUF, &buf) < 0) return false;
      continue;
    }
    out.create(height_, width_, CV_8UC3);
    yuyv_to_bgr(static_cast<const uint8_t*>(buffers_[buf.index].start), stride_, out, width_, height_);
    if (rotate180_) cv::rotate(out, out, cv::ROTATE_180);
    return xioctl(fd_, VIDIOC_QBUF, &buf) >= 0;
  }
}
}  // namespace hoodseek
