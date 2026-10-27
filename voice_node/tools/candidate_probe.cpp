// 오프라인 진단 도구: 전체 prefix 기준 점수와 중복 prefix 재사용 점수를
// 대조한다. 모든 후보·EOT를 유지한다. 점수는 ASR conf가 아니며, 전사 교정·품질
// 통과·TCP 발행을 하지 않는다. 모델·원음은 외부에서 명시한다. compare 모드는
// 토큰 점수 차이가 1e-6을 넘으면 실패한다.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "whisper.h"

using Clock = std::chrono::steady_clock;
double ms(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start)
      .count();
}
struct Candidate {
  std::string id;
  std::vector<whisper_token> tokens;
};
std::vector<std::pair<std::string, std::string>> tsv(const char* path) {
  std::ifstream in(path, std::ios::ate);
  if (!in) throw std::runtime_error("cannot open TSV");
  if (in.tellg() <= 0 || in.tellg() > 1024 * 1024)
    throw std::runtime_error("TSV size must be in (0, 1 MiB]");
  in.seekg(0);
  std::vector<std::pair<std::string, std::string>> result;
  std::set<std::string> ids;
  std::string line;
  while (std::getline(in, line)) {
    if (line.size() > 4096 || result.size() >= 512)
      throw std::runtime_error("TSV line or record limit exceeded");
    auto sep = line.find('\t');
    if (sep == std::string::npos || sep == 0 || sep + 1 == line.size() ||
        line.find('\t', sep + 1) != std::string::npos)
      throw std::runtime_error("invalid TSV");
    auto id = line.substr(0, sep);
    if (id.size() > 64 || !ids.insert(id).second)
      throw std::runtime_error("duplicate or oversized id");
    if (id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvw"
                             "xyz0123456789_") != std::string::npos)
      throw std::runtime_error("invalid id");
    result.emplace_back(id, line.substr(sep + 1));
  }
  if (in.bad() || result.empty()) throw std::runtime_error("invalid TSV read");
  return result;
}
std::vector<float> samples(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) throw std::runtime_error("cannot open PCM");
  const auto size = static_cast<std::streamoff>(in.tellg());
  if (size <= 0 || size % 2 || size > 16000 * 2 * 30)
    throw std::runtime_error("invalid PCM size");
  in.seekg(0);
  std::vector<unsigned char> bytes(static_cast<size_t>(size));
  if (!in.read(reinterpret_cast<char*>(bytes.data()), size))
    throw std::runtime_error("incomplete PCM read");
  std::vector<float> result;
  result.reserve(bytes.size() / 2);
  for (size_t i = 0; i < bytes.size(); i += 2) {
    int value = bytes[i] | (static_cast<int>(bytes[i + 1]) << 8);
    if (value >= 32768) value -= 65536;
    result.push_back(value / 32768.0f);
  }
  return result;
}
double logp(const float* logits, int vocab, whisper_token target) {
  if (target < 0 || target >= vocab)
    throw std::runtime_error("target out of range");
  float peak = *std::max_element(logits, logits + vocab);
  if (!std::isfinite(peak)) throw std::runtime_error("invalid logits");
  double total = 0;
  for (int i = 0; i < vocab; i++) {
    if (!std::isfinite(logits[i])) throw std::runtime_error("nonfinite logits");
    total += std::exp(static_cast<double>(logits[i]) - peak);
  }
  const double result = logits[target] - peak - std::log(total);
  if (!std::isfinite(result) || result > 1e-6)
    throw std::runtime_error("invalid log probability");
  return result;
}
std::vector<double> score(whisper_context* ctx,
                          const std::vector<whisper_token>& prefix,
                          const Candidate& candidate) {
  std::vector<whisper_token> history = prefix;
  std::vector<whisper_token> targets = candidate.tokens;
  targets.push_back(whisper_token_eot(ctx));
  std::vector<double> result;
  const int vocab = whisper_n_vocab(ctx);
  int rows = static_cast<int>(prefix.size());
  if (whisper_decode(ctx, prefix.data(), rows, 0, 4))
    throw std::runtime_error("prefix decode failed");
  for (size_t i = 0; i < targets.size(); i++) {
    result.push_back(
        logp(whisper_get_logits(ctx) + (rows - 1) * vocab, vocab, targets[i]));
    if (i + 1 == targets.size()) break;
    history.push_back(targets[i]);
    rows = static_cast<int>(history.size());
    if (whisper_decode(ctx, history.data(), rows, 0, 4))
      throw std::runtime_error("full-prefix decode failed");
  }
  return result;
}

std::vector<std::vector<double>> cached_scores(
    whisper_context* ctx, const std::vector<whisper_token>& prefix,
    const std::vector<Candidate>& candidates, size_t& calls) {
  // 음원마다 새로 만든다. 같은 전체 prefix의 계산 결과만 공유하며 KV 증분
  // 경로는 쓰지 않는다.
  std::map<std::vector<whisper_token>, std::map<whisper_token, double>>
      requests;
  for (const auto& candidate : candidates) {
    auto history = prefix;
    auto targets = candidate.tokens;
    targets.push_back(whisper_token_eot(ctx));
    for (auto target : targets) {
      requests[history].emplace(target, 0.0);
      history.push_back(target);
    }
  }
  const int vocab = whisper_n_vocab(ctx);
  calls = requests.size();
  for (auto& request : requests) {
    const auto& history = request.first;
    if (whisper_decode(ctx, history.data(), static_cast<int>(history.size()), 0,
                       4))
      throw std::runtime_error("cached full-prefix decode failed");
    const float* logits =
        whisper_get_logits(ctx) + (history.size() - 1) * vocab;
    const float peak = *std::max_element(logits, logits + vocab);
    if (!std::isfinite(peak)) throw std::runtime_error("invalid peak");
    double total = 0;
    for (int i = 0; i < vocab; i++) {
      if (!std::isfinite(logits[i]))
        throw std::runtime_error("nonfinite logits");
      total += std::exp(static_cast<double>(logits[i]) - peak);
    }
    const double normalizer = std::log(total);
    for (auto& child : request.second) {
      if (child.first < 0 || child.first >= vocab)
        throw std::runtime_error("invalid target");
      child.second = logits[child.first] - peak - normalizer;
      if (!std::isfinite(child.second) || child.second > 1e-6)
        throw std::runtime_error("invalid cached logp");
    }
  }
  std::vector<std::vector<double>> values;
  for (const auto& candidate : candidates) {
    auto history = prefix;
    auto targets = candidate.tokens;
    targets.push_back(whisper_token_eot(ctx));
    values.emplace_back();
    for (auto target : targets) {
      values.back().push_back(requests.at(history).at(target));
      history.push_back(target);
    }
  }
  return values;
}

int main(int argc, char** argv) {
  whisper_context* ctx = nullptr;
  try {
    if (argc != 5)
      throw std::runtime_error(
          "usage: probe MODEL CANDIDATES.tsv INPUTS.tsv "
          "reference|cached|compare");
    const std::string mode = argv[4];
    if (mode != "reference" && mode != "cached" && mode != "compare")
      throw std::runtime_error("invalid scoring mode");
    std::cout << std::setprecision(17);
    auto params = whisper_context_default_params();
    params.use_gpu = false;
    auto start = Clock::now();
    ctx = whisper_init_from_file_with_params(argv[1], params);
    if (!ctx) throw std::runtime_error("model loading failed");
    const double load_ms = ms(start);
    std::vector<Candidate> candidates;
    for (const auto& row : tsv(argv[2])) {
      std::vector<whisper_token> tokens(256);
      const int n = whisper_tokenize(ctx, row.second.c_str(), tokens.data(),
                                     static_cast<int>(tokens.size()));
      if (n <= 0 || n > 200) throw std::runtime_error("invalid token count");
      tokens.resize(n);
      for (auto token : tokens)
        if (token < 0 || token >= whisper_token_eot(ctx))
          throw std::runtime_error("special token in text");
      candidates.push_back({row.first, tokens});
    }
    const std::vector<whisper_token> prefix = {
        whisper_token_sot(ctx), whisper_token_lang(ctx, whisper_lang_id("ko")),
        whisper_token_transcribe(ctx), whisper_token_not(ctx)};
    size_t reference_calls = 0;
    for (const auto& c : candidates) reference_calls += c.tokens.size() + 1;
    std::cout << "{\"type\":\"model\",\"load_ms\":" << load_ms
              << ",\"audio_ctx\":" << whisper_n_audio_ctx(ctx) << ",\"mode\":\""
              << mode << "\",\"candidate_count\":" << candidates.size()
              << ",\"reference_calls\":" << reference_calls << "}" << std::endl;
    size_t index = 0;
    for (const auto& input : tsv(argv[3])) {
      const auto audio = samples(input.second);
      start = Clock::now();
      if (whisper_pcm_to_mel(ctx, audio.data(), static_cast<int>(audio.size()),
                             4))
        throw std::runtime_error("mel failed");
      const double mel_ms = ms(start);
      start = Clock::now();
      if (whisper_encode(ctx, 0, 4)) throw std::runtime_error("encode failed");
      const double encode_ms = ms(start);
      std::vector<std::vector<double>> reference, values;
      size_t cached_calls = 0;
      double reference_ms = 0, cached_ms = 0, max_delta = 0;
      auto run_reference = [&] {
        start = Clock::now();
        for (const auto& c : candidates)
          reference.push_back(score(ctx, prefix, c));
        reference_ms = ms(start);
      };
      auto run_cached = [&] {
        start = Clock::now();
        values = cached_scores(ctx, prefix, candidates, cached_calls);
        cached_ms = ms(start);
      };
      if (mode == "compare") {
        if (index % 2) {
          run_cached();
          run_reference();
        } else {
          run_reference();
          run_cached();
        }
        for (size_t i = 0; i < values.size(); i++)
          for (size_t j = 0; j < values[i].size(); j++)
            max_delta =
                std::max(max_delta, std::abs(values[i][j] - reference[i][j]));
        if (max_delta > 1e-6)
          throw std::runtime_error("optimized/reference token logp mismatch");
      } else if (mode == "reference") {
        run_reference();
        values = reference;
      } else
        run_cached();
      if (index == 0 && mode == "compare") {
        auto reversed = candidates;
        std::reverse(reversed.begin(), reversed.end());
        size_t calls = 0;
        auto repeated = cached_scores(ctx, prefix, reversed, calls);
        std::reverse(repeated.begin(), repeated.end());
        if (repeated != values)
          throw std::runtime_error("candidate order contamination");
      }
      std::cout << "{\"type\":\"input\",\"review_id\":\"" << input.first
                << "\",\"samples\":" << audio.size() << ",\"mel_ms\":" << mel_ms
                << ",\"encode_ms\":" << encode_ms
                << ",\"reference_ms\":" << reference_ms
                << ",\"cached_ms\":" << cached_ms
                << ",\"cached_calls\":" << cached_calls
                << ",\"max_token_logp_delta\":" << max_delta << ",\"scores\":[";
      for (size_t i = 0; i < candidates.size(); i++) {
        if (i) std::cout << ',';
        double sum = 0;
        for (double value : values[i]) sum += value;
        std::cout << "{\"candidate_id\":\"" << candidates[i].id
                  << "\",\"sum_logp\":" << sum
                  << ",\"mean_logp\":" << sum / values[i].size()
                  << ",\"eot_logp\":" << values[i].back()
                  << ",\"token_logps\":[";
        for (size_t j = 0; j < values[i].size(); j++) {
          if (j) std::cout << ',';
          std::cout << values[i][j];
        }
        std::cout << "]}";
      }
      std::cout << "]}" << std::endl;
      ++index;
    }
    whisper_free(ctx);
    return 0;
  } catch (const std::exception& error) {
    if (ctx) whisper_free(ctx);
    std::cerr << "probe_error: " << error.what() << '\n';
    return 2;
  }
}
