#pragma once
#include "vision.hpp"
#include <ecoai-inference/InferenceStrategy/InferenceStrategyBuilder.hpp>

namespace hoodseek {
class EcoDetector {
 public:
  EcoDetector(const std::string& model, const std::string& backend, int threads, Config config);
  ~EcoDetector();
  Frame infer(const cv::Mat& bgr);
 private:
  ecoai::inference::StrategyPtr strategy_;
  ecoai::inference::EcoTensorInfo input_;
  bool nchw_ = false;
  int width_ = 0, height_ = 0;
  Config config_;
  Frame infer_view(const cv::Mat& bgr, const std::string& mode);
};
}  // namespace hoodseek
