#include "ecoai_detector.hpp"
#include <cmath>
#include <stdexcept>

namespace hoodseek {
namespace {
using namespace ecoai::inference;
using DT = EcoTensorInfo::DataType;
std::vector<float> values(const EcoTensorData& tensor, bool categorical) {
  size_t count = 1;
  for (auto d : tensor.info.dimensions) {
    if (d <= 0 || d > 120000 || count > 120000 / static_cast<size_t>(d)) throw std::runtime_error("invalid output tensor shape");
    count *= static_cast<size_t>(d);
  }
  size_t element_size = 0;
  switch (tensor.info.dataType) {
    case DT::FLOAT32: case DT::INT32: element_size = 4; break;
    case DT::UINT8: case DT::INT8: element_size = 1; break;
    default: throw std::runtime_error("unsupported output dtype");
  }
  if (!tensor.data || tensor.size != count * element_size) throw std::runtime_error("output tensor byte count mismatch");
  std::vector<float> result(count);
  const auto& q = tensor.info.quant_params;
  for (size_t i = 0; i < count; ++i) {
    if (tensor.info.dataType == DT::FLOAT32) result[i] = static_cast<const float*>(tensor.data)[i];
    else {
      int64_t value = tensor.info.dataType == DT::UINT8 ? static_cast<const uint8_t*>(tensor.data)[i] :
                      tensor.info.dataType == DT::INT8 ? static_cast<const int8_t*>(tensor.data)[i] : static_cast<const int32_t*>(tensor.data)[i];
      result[i] = decode_integer(value, q.is_quantized, q.scale, q.offset, categorical);
    }
    if (!std::isfinite(result[i])) throw std::runtime_error("nonfinite model output");
  }
  return result;
}
}  // namespace

EcoDetector::EcoDetector(const std::string& model, const std::string& backend, int threads, Config config) : config_(std::move(config)) {
  if ((backend != "cpu" && backend != "gpu") || threads < 1 || threads > 16) throw std::runtime_error("unsupported backend or thread count");
  strategy_ = ecoai::inference::InferenceStrategyBuilder(model).setOptions({{backend, ""}, {backend + ".intra_op_num_threads", std::to_string(threads)}}).build();
  if (!strategy_ || !strategy_->initialize()) throw std::runtime_error("EcoAI initialization failed");
  auto inputs = strategy_->getInputTensorsInfo();
  if (inputs.size() != 1) throw std::runtime_error("expected exactly one model input");
  input_ = inputs.front(); const auto& d = input_.dimensions;
  if (d.size() != 4 || d[0] != 1) throw std::runtime_error("expected static batch-one rank-four input");
  nchw_ = d[1] == 3;
  if (!nchw_ && d[3] != 3) throw std::runtime_error("expected NHWC/NCHW RGB input");
  const auto h = d[nchw_ ? 2 : 1], w = d[nchw_ ? 3 : 2];
  if (h <= 0 || w <= 0 || h > 4096 || w > 4096) throw std::runtime_error("invalid model input dimensions");
  height_ = static_cast<int>(h); width_ = static_cast<int>(w);
}
EcoDetector::~EcoDetector() { if (strategy_) strategy_->teardown(); }
// ponytail: one pass per frame. A second resize-mode pass halves the frame rate (NFR-07 ≥2.5 FPS);
// reconsider only if #49 shows a resize mode misses the doll.
Frame EcoDetector::infer(const cv::Mat& bgr) { return infer_view(bgr, config_.resize_mode); }
Frame EcoDetector::infer_view(const cv::Mat& bgr, const std::string& mode) {
  auto floats = preprocess(bgr, width_, height_, nchw_, mode);
  std::vector<uint8_t> unsigned_bytes;
  std::vector<int8_t> signed_bytes;
  ecoai::inference::EcoTensorData data;
  data.info = input_; data.owns_data = false;
  if (input_.dataType == DT::FLOAT32) { data.data = floats.data(); data.size = floats.size() * sizeof(float); }
  else {
    if (!input_.quant_params.is_quantized) throw std::runtime_error("integer input has no quantization metadata");
    const auto& q = input_.quant_params;
    if (input_.dataType == DT::UINT8) {
      unsigned_bytes.reserve(floats.size());
      for (float value : floats) unsigned_bytes.push_back(static_cast<uint8_t>(quantize(value, q.scale, q.offset, 0, 255)));
      data.data = unsigned_bytes.data(); data.size = unsigned_bytes.size();
    } else if (input_.dataType == DT::INT8) {
      signed_bytes.reserve(floats.size());
      for (float value : floats) signed_bytes.push_back(static_cast<int8_t>(quantize(value, q.scale, q.offset, -128, 127)));
      data.data = signed_bytes.data(); data.size = signed_bytes.size();
    } else throw std::runtime_error("unsupported input dtype");
  }
  ecoai::inference::EcoTensorMap inputs{{input_.name, data}}, outputs;
  if (!strategy_->execute(inputs, outputs)) throw std::runtime_error("EcoAI inference failed");
  for (const auto* name : {"boxes", "scores", "class_idx"}) if (!outputs.count(name)) throw std::runtime_error(std::string("missing named output: ") + name);
  const auto& shape = outputs.at("boxes").info.dimensions;
  if (shape.size() != 3 || shape[0] != 1 || shape[2] != 4) throw std::runtime_error("expected boxes [1,N,4]");
  auto boxes = values(outputs.at("boxes"), false), scores = values(outputs.at("scores"), false), classes = values(outputs.at("class_idx"), true);
  if (boxes.size() != scores.size() * 4 || scores.size() != classes.size()) throw std::runtime_error("output sizes disagree");
  Frame result; result.width = bgr.cols; result.height = bgr.rows; result.model_width = width_; result.model_height = height_;
  result.resize_mode = mode;
  for (size_t i = 0; i < scores.size(); ++i) {
    if (classes[i] < 0 || classes[i] > 79 || std::abs(classes[i] - std::round(classes[i])) > 1e-3F) throw std::runtime_error("invalid COCO class output");
    result.proposals.push_back({static_cast<int>(std::round(classes[i])), scores[i], {boxes[4 * i], boxes[4 * i + 1], boxes[4 * i + 2] - boxes[4 * i], boxes[4 * i + 3] - boxes[4 * i + 1]}});
  }
  return result;
}
}  // namespace hoodseek
