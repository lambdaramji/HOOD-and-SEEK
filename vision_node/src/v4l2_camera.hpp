#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace hoodseek {
// Minimal V4L2 camera reader for UVC devices such as the Microsoft LifeCam
// HD-3000. The ATLAS device image does not ship OpenCV's videoio module (it
// needs GStreamer/FFmpeg/libgphoto2, none of which are installed), so this
// talks to the kernel V4L2 API directly and requires nothing beyond libc.
class V4L2Camera {
 public:
  // rotate180 corrects a camera physically mounted upside down (e.g. clipped
  // under a hood facing down); the sensor itself is never rotated in software
  // otherwise, so this must match how the camera is actually mounted.
  explicit V4L2Camera(const std::string& device, int width = 640, int height = 480, bool rotate180 = false);
  ~V4L2Camera();
  V4L2Camera(const V4L2Camera&) = delete;
  V4L2Camera& operator=(const V4L2Camera&) = delete;
  bool read(cv::Mat& out);
  // Size the driver actually negotiated (may differ from the request).
  int width() const { return width_; }
  int height() const { return height_; }
 private:
  struct MappedBuffer { void* start; size_t length; };
  int fd_ = -1;
  int width_ = 0, height_ = 0;
  size_t stride_ = 0;  // bytes per YUYV row as reported by the driver (may include padding)
  bool rotate180_ = false;
  std::vector<MappedBuffer> buffers_;
};
}  // namespace hoodseek
