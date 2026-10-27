#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>

namespace hoodseek {
using Json = nlohmann::json;
using Polygon = std::vector<cv::Point2f>;
// Observation settings only. Judgment thresholds, debounce and presence timing live in SafetyCore
// (design §3, invariant: Pi 5 observation nodes report, they do not decide).
struct Config {
  std::string resize_mode = "letterbox";
  // Noise floor for reporting a candidate at all; SafetyCore applies the real thresholds (P_cat_score…).
  float detection_floor = 0.10F, nms_iou = 0.45F;
  // COCO person, cat, dog, teddy bear: a cheap cat doll may come out as dog or teddy bear (#49).
  std::vector<int> watch_classes{0, 15, 16, 77};
  // COCO classes reported in the contract's `cat` (pet, design R4) slot. #49 [실측 2026-10-20]: the demo doll
  // face-on comes out as dog 0.75–0.82 in 15/15 frames and never as cat; teddy bear stays out (weak, unstable).
  std::vector<int> cat_classes{15, 16};
  static Config from_json(const Json& value);
};
// config/roi.json from tools/roi_click.py: induction polygon in frame pixels for one resolution and rotation.
struct Roi { int frame_w = 0, frame_h = 0, rotate = 0; Polygon points; };
Roi load_roi(const Json& value);
struct Proposal { int class_id; float score; cv::Rect2f box; };
struct Frame {
  std::string resize_mode = "stretch";
  int width = 0, height = 0, model_width = 0, model_height = 0;
  int64_t sequence = 0, timestamp_ms = 0;
  std::vector<Proposal> proposals;
  static Frame from_json(const Json& value);
};
// Input boxes are decoded xyxy in model pixels; output boxes are frame pixels.
std::vector<Proposal> postprocess(const Frame& frame, const Config& config);
// bbox∩ROI / bbox with a pixel polygon (design §5.2, P-003).
float overlap(const cv::Rect2f& box, const Polygon& roi);
// Integer xyxy rounded outward and clipped to the frame; nullopt when nothing is left inside.
std::optional<std::array<int, 4>> pixel_box(const cv::Rect2f& box, int width, int height);
// {"<label>": {"score", "bbox"} | null} for every watched class, keeping the highest score.
Json best_by_class(const std::vector<Proposal>& detections, const std::vector<int>& classes, int width, int height);
void write_ppm(const std::string& path, const cv::Mat& bgr);
// Contract v1 (contracts/vision.schema.json): `p` of one vision message. Raw scores only; SafetyCore judges.
Json vision_payload(const Frame& frame, const std::vector<Proposal>& found, int64_t infer_ms, const std::vector<int>& cat_classes, const Roi& roi);
Json envelope(const std::string& type, uint32_t seq, int64_t t_ms, Json payload);
// One message per line; lines over 4096 bytes make the server drop the connection (design §2.1).
std::string encode_line(const Json& message);
struct ResizeTransform { cv::Size size; int left, top; float scale_x, scale_y; };
ResizeTransform input_transform(int width, int height, int model_width, int model_height, const std::string& mode);
std::vector<float> preprocess(const cv::Mat& bgr, int width, int height, bool nchw, const std::string& mode = "stretch");
float decode_integer(int64_t value, bool quantized, float scale, int offset, bool categorical);
int quantize(float value, float scale, int offset, int lower, int upper);
const char* label(int class_id);
}  // namespace hoodseek
