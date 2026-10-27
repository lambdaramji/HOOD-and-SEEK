#include "vision.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <opencv2/imgproc.hpp>

namespace hoodseek {
namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void unit(float value) { require(std::isfinite(value) && value >= 0 && value <= 1, "expected finite value in [0,1]"); }
int64_t integer(const Json& j) {
  require(j.is_number_integer(), "expected integer");
  if (j.is_number_unsigned()) require(j.get<uint64_t>() <= static_cast<uint64_t>(INT64_MAX), "integer overflow");
  return j.get<int64_t>();
}
int dimension(const Json& j) {
  auto n = integer(j); require(n > 0 && n <= 4096, "invalid frame dimension (contract frame_w/frame_h 1..4096)"); return static_cast<int>(n);
}
float iou(const cv::Rect2f& a, const cv::Rect2f& b) {
  float intersection = (a & b).area();
  return intersection / (a.area() + b.area() - intersection);
}
std::vector<int> coco_ids(const Json& v, const std::string& key) {
  if (!v.is_array() || v.empty() || v.size() > 80) throw std::runtime_error(key + " must be a non-empty array");
  std::vector<int> ids;
  for (const auto& id : v) {
    auto n = integer(id);
    if (n < 0 || n >= 80) throw std::runtime_error(key + " must be COCO 0..79");
    ids.push_back(static_cast<int>(n));
  }
  return ids;
}
// Vertices on the convex hull in boundary order (rejects bow-ties and star orderings).
void require_convex(const Polygon& points) {
  require(cv::isContourConvex(points) && std::abs(cv::contourArea(points)) > 1e-6, "ROI must be a nonzero simple convex polygon in boundary order");
  // isContourConvex alone accepts some self-intersecting star orderings.
  Polygon hull; cv::convexHull(points, hull);
  require(hull.size() == points.size(), "ROI vertices must lie on the convex boundary");
  auto first = std::find(hull.begin(), hull.end(), points.front());
  require(first != hull.end(), "invalid ROI boundary");
  size_t start = static_cast<size_t>(first - hull.begin()), n = hull.size();
  bool forward = true, reverse = true;
  for (size_t i = 0; i < n; ++i) {
    forward &= points[i] == hull[(start + i) % n];
    reverse &= points[i] == hull[(start + n - i) % n];
  }
  require(forward || reverse, "ROI must follow boundary order without self intersections");
}
double round4(double value) { return std::round(value * 1e4) / 1e4; }
}  // namespace

Roi load_roi(const Json& j) {
  require(j.is_object(), "ROI file must be an object");
  for (auto it = j.begin(); it != j.end(); ++it)
    require(it.key() == "frame_w" || it.key() == "frame_h" || it.key() == "rotate" || it.key() == "points" || it.key() == "note", "unknown ROI key");
  Roi r;
  r.frame_w = dimension(j.at("frame_w")); r.frame_h = dimension(j.at("frame_h"));
  auto rotate = integer(j.at("rotate"));
  require(rotate == 0 || rotate == 180, "ROI rotate must be 0 or 180");
  r.rotate = static_cast<int>(rotate);
  const auto& points = j.at("points");
  require(points.is_array() && points.size() >= 3 && points.size() <= 32, "ROI needs 3..32 points");
  for (const auto& p : points) {
    require(p.is_array() && p.size() == 2, "ROI point must be [x,y]");
    auto x = integer(p.at(0)), y = integer(p.at(1));
    require(x >= 0 && x <= r.frame_w && y >= 0 && y <= r.frame_h, "ROI point outside the calibrated frame");
    r.points.emplace_back(static_cast<float>(x), static_cast<float>(y));
  }
  require_convex(r.points);
  return r;
}

Config Config::from_json(const Json& j) {
  require(j.is_object(), "configuration must be an object");
  Config c;
  for (auto it = j.begin(); it != j.end(); ++it) {
    const auto& k = it.key(); const auto& v = it.value();
    if (k == "resize_mode") c.resize_mode = v.get<std::string>();
    else if (k == "detection_floor") c.detection_floor = v.get<float>();
    else if (k == "nms_iou") c.nms_iou = v.get<float>();
    else if (k == "watch_classes") c.watch_classes = coco_ids(v, "watch_classes");
    else if (k == "cat_classes") c.cat_classes = coco_ids(v, "cat_classes");
    else throw std::runtime_error("unknown configuration key: " + k);
  }
  unit(c.detection_floor); unit(c.nms_iou);
  // Lowering the noise floor is allowed for #49 measurements; raising it would be a judgment threshold (SafetyCore's P_*_score).
  require(c.detection_floor > 0 && c.detection_floor <= 0.10F, "detection_floor must be in (0, 0.10]");
  require(c.resize_mode == "stretch" || c.resize_mode == "letterbox", "resize_mode must be stretch or letterbox");
  return c;
}

Frame Frame::from_json(const Json& j) {
  Frame f;
  f.resize_mode = j.value("resize_mode", "stretch");
  f.width = dimension(j.at("width")); f.height = dimension(j.at("height"));
  f.model_width = dimension(j.at("model_width")); f.model_height = dimension(j.at("model_height"));
  f.sequence = integer(j.at("sequence")); f.timestamp_ms = integer(j.at("timestamp_ms"));
  const auto& proposals = j.at("proposals");
  require(proposals.is_array() && proposals.size() <= 30000, "invalid proposal count");
  for (const auto& p : proposals) {
    const auto& b = p.at("bbox");
    require(b.is_array() && b.size() == 4, "bbox must be xyxy");
    auto id = integer(p.at("class_id")); require(id >= 0 && id < 80, "class_id must be COCO 0..79");
    float x = b.at(0).get<float>(), y = b.at(1).get<float>();
    f.proposals.push_back({static_cast<int>(id), p.at("score").get<float>(), {x, y, b.at(2).get<float>() - x, b.at(3).get<float>() - y}});
  }
  return f;
}

std::vector<Proposal> postprocess(const Frame& f, const Config& c) {
  require(f.width > 0 && f.height > 0 && f.width <= 32768 && f.height <= 32768 && f.model_width > 0 && f.model_height > 0 && f.model_width <= 32768 && f.model_height <= 32768, "invalid frame dimensions");
  require(f.proposals.size() <= 30000, "too many proposals");
  std::vector<Proposal> candidates, kept;
  const auto transform = input_transform(f.width, f.height, f.model_width, f.model_height, f.resize_mode);
  for (auto p : f.proposals) {
    unit(p.score);
    require(p.class_id >= 0 && p.class_id < 80, "invalid COCO class");
    require(std::isfinite(p.box.x) && std::isfinite(p.box.y) && std::isfinite(p.box.width) && std::isfinite(p.box.height), "nonfinite box");
    if (p.score < c.detection_floor) continue;
    float x1 = std::clamp((p.box.x - transform.left) / transform.scale_x, 0.0F, static_cast<float>(f.width));
    float y1 = std::clamp((p.box.y - transform.top) / transform.scale_y, 0.0F, static_cast<float>(f.height));
    float x2 = std::clamp((p.box.x + p.box.width - transform.left) / transform.scale_x, 0.0F, static_cast<float>(f.width));
    float y2 = std::clamp((p.box.y + p.box.height - transform.top) / transform.scale_y, 0.0F, static_cast<float>(f.height));
    if (x2 <= x1 || y2 <= y1) continue;
    p.box = {x1, y1, x2 - x1, y2 - y1}; candidates.push_back(p);
  }
  std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.score > b.score; });
  for (const auto& p : candidates) {
    bool suppressed = std::any_of(kept.begin(), kept.end(), [&](const auto& k) { return p.class_id == k.class_id && iou(p.box, k.box) > c.nms_iou; });
    if (!suppressed) kept.push_back(p);
    if (kept.size() == 300) break;
  }
  return kept;
}

float overlap(const cv::Rect2f& b, const Polygon& roi) {
  if (roi.empty() || b.area() <= 0) return 0;
  Polygon intersection;
  Polygon box{{b.x, b.y}, {b.x + b.width, b.y}, {b.x + b.width, b.y + b.height}, {b.x, b.y + b.height}};
  return std::clamp(cv::intersectConvexConvex(box, roi, intersection) / b.area(), 0.0F, 1.0F);
}

ResizeTransform input_transform(int w, int h, int mw, int mh, const std::string& mode) {
  require(w > 0 && h > 0 && mw > 0 && mh > 0, "invalid transform dimensions");
  require(mode == "stretch" || mode == "letterbox", "unknown resize mode");
  int nw = mw, nh = mh;
  if (mode == "letterbox") {
    double scale = std::min(static_cast<double>(mw) / w, static_cast<double>(mh) / h);
    nw = std::max(1, static_cast<int>(std::floor(w * scale)));
    nh = std::max(1, static_cast<int>(std::floor(h * scale)));
  }
  return {{nw, nh}, (mw - nw) / 2, (mh - nh) / 2, static_cast<float>(nw) / w, static_cast<float>(nh) / h};
}
std::vector<float> preprocess(const cv::Mat& bgr, int width, int height, bool nchw, const std::string& mode) {
  require(!bgr.empty() && bgr.type() == CV_8UC3 && width > 0 && height > 0 && width <= 4096 && height <= 4096, "invalid model image input");
  cv::Mat rgb, resized, values;
  const auto transform = input_transform(bgr.cols, bgr.rows, width, height, mode);
  cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
  cv::resize(rgb, resized, transform.size, 0, 0, cv::INTER_LINEAR);
  cv::Mat padded(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
  resized.copyTo(padded(cv::Rect(transform.left, transform.top, resized.cols, resized.rows)));
  padded.convertTo(values, CV_32FC3, 1.0 / 255.0);
  std::vector<float> result(static_cast<size_t>(width) * height * 3);
  if (nchw) {
    std::vector<cv::Mat> channels; cv::split(values, channels);
    for (size_t i = 0; i < 3; ++i) std::memcpy(result.data() + i * width * height, channels[i].ptr<float>(), static_cast<size_t>(width) * height * sizeof(float));
  } else std::memcpy(result.data(), values.ptr<float>(), result.size() * sizeof(float));
  return result;
}

float decode_integer(int64_t value, bool quantized, float scale, int offset, bool categorical) {
  if (!quantized && categorical) return static_cast<float>(value);
  require(quantized && std::isfinite(scale) && scale > 0, "numeric integer tensor requires positive quantization scale");
  return static_cast<float>((static_cast<double>(value) - offset) * scale);
}
int quantize(float value, float scale, int offset, int lower, int upper) {
  require(std::isfinite(value) && std::isfinite(scale) && scale > 0, "invalid input quantization");
  return static_cast<int>(std::clamp(std::round(static_cast<double>(value) / scale) + offset, static_cast<double>(lower), static_cast<double>(upper)));
}

const char* label(int id) {
  static const char* names[] = {"person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"};
  require(id >= 0 && id < 80, "invalid label"); return names[id];
}
std::optional<std::array<int, 4>> pixel_box(const cv::Rect2f& b, int width, int height) {
  require(width > 0 && height > 0, "invalid frame dimensions");
  require(std::isfinite(b.x) && std::isfinite(b.y) && std::isfinite(b.width) && std::isfinite(b.height), "nonfinite box");
  auto edge = [](float value, int limit, bool upper) {
    float rounded = upper ? std::ceil(value) : std::floor(value);
    return static_cast<int>(std::clamp(rounded, 0.0F, static_cast<float>(limit)));
  };
  int x1 = edge(b.x, width, false), y1 = edge(b.y, height, false);
  int x2 = edge(b.x + b.width, width, true), y2 = edge(b.y + b.height, height, true);
  if (x2 <= x1 || y2 <= y1) return std::nullopt;
  return std::array<int, 4>{x1, y1, x2, y2};
}
namespace {
// Highest-scoring candidate among `classes` whose box survives clipping to the frame. The #49 record
// and the vision message both use this, so they can never pick different candidates.
std::pair<const Proposal*, std::array<int, 4>> top_candidate(const std::vector<Proposal>& found, const std::vector<int>& classes, int width, int height) {
  const Proposal* top = nullptr;
  std::array<int, 4> top_box{};
  for (const auto& d : found) {
    if (std::find(classes.begin(), classes.end(), d.class_id) == classes.end() || (top && d.score <= top->score)) continue;
    if (auto box = pixel_box(d.box, width, height)) { top = &d; top_box = *box; }
  }
  return {top, top_box};
}
}  // namespace
Json best_by_class(const std::vector<Proposal>& detections, const std::vector<int>& classes, int width, int height) {
  Json result = Json::object();
  for (int id : classes) {
    auto [top, box] = top_candidate(detections, {id}, width, height);
    // 4 decimals is finer than the model's uint8 score step (≈0.0038).
    result[label(id)] = top ? Json{{"score", round4(top->score)}, {"bbox", box}} : Json(nullptr);
  }
  return result;
}
void write_ppm(const std::string& path, const cv::Mat& bgr) {
  require(!bgr.empty() && bgr.type() == CV_8UC3, "frame dump needs an 8-bit BGR image");
  cv::Mat rgb; cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
  std::ofstream out(path, std::ios::binary);
  out << "P6\n" << rgb.cols << ' ' << rgb.rows << "\n255\n";
  for (int y = 0; y < rgb.rows; ++y) out.write(reinterpret_cast<const char*>(rgb.ptr(y)), static_cast<std::streamsize>(rgb.cols) * 3);
  if (!out) throw std::runtime_error("cannot write frame dump: " + path);
}
Json vision_payload(const Frame& f, const std::vector<Proposal>& found, int64_t infer_ms, const std::vector<int>& cat_classes, const Roi& roi) {
  require(f.width == roi.frame_w && f.height == roi.frame_h, "frame size differs from the ROI calibration; recalibrate config/roi.json");
  require(infer_ms >= 0 && infer_ms <= 60000, "infer_ms out of contract range");
  Json p = {{"frame_w", f.width}, {"frame_h", f.height}, {"infer_ms", infer_ms}};
  auto [person, person_box] = top_candidate(found, {0}, f.width, f.height);
  p["person"] = person ? Json{{"seen", true}, {"score", round4(person->score)}, {"bbox", person_box}} : Json{{"seen", false}, {"score", 0.0}, {"bbox", nullptr}};
  auto [cat, cat_box] = top_candidate(found, cat_classes, f.width, f.height);
  if (cat) {
    const auto& b = cat_box;  // overlap of the integer box actually sent, so Core and the #49 table agree
    float ratio = overlap(cv::Rect2f(b[0], b[1], b[2] - b[0], b[3] - b[1]), roi.points);
    p["cat"] = {{"seen", true}, {"score", round4(cat->score)}, {"bbox", b}, {"roi_overlap", round4(ratio)}};
  } else {
    p["cat"] = {{"seen", false}, {"score", 0.0}, {"bbox", nullptr}, {"roi_overlap", 0.0}};
  }
  return p;
}
Json envelope(const std::string& type, uint32_t seq, int64_t t_ms, Json payload) {
  require(t_ms >= 0, "t_ms must be non-negative");
  return {{"v", 1}, {"type", type}, {"src", "vision"}, {"seq", seq}, {"t_ms", t_ms}, {"p", std::move(payload)}};
}
std::string encode_line(const Json& message) {
  auto line = message.dump() + '\n';
  require(line.size() <= 4096, "message exceeds the 4096-byte line limit");
  return line;
}
}  // namespace hoodseek
