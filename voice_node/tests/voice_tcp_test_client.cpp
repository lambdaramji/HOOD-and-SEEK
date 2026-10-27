#include "voice_tcp.h"
#include "command_parser.h"

#include <chrono>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

using namespace hoodseek::voice;
using namespace std::chrono_literals;

namespace {

const char* result_name(PublishResult result) {
  switch (result) {
    case PublishResult::Sent: return "sent";
    case PublishResult::Invalid: return "invalid";
    case PublishResult::QualityRejected: return "quality_rejected";
    case PublishResult::Expired: return "expired";
    case PublishResult::Duplicate: return "duplicate";
    case PublishResult::Disconnected: return "disconnected";
    case PublishResult::IoError: return "io_error";
  }
  return "invalid_result";
}

int self_test() {
  int checks = 0;
  int failures = 0;
  const auto check = [&](bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
  };
  check(voice_line("후드 강풍", .75, 7, 99) ==
      "{\"v\":1,\"type\":\"voice\",\"src\":\"voice\",\"seq\":7,\"t_ms\":99,"
      "\"p\":{\"intent\":\"FAN_SET\",\"value\":3,\"text\":\"후드 강풍\",\"conf\":0.75}}\n",
      "contract example");
  const auto none = voice_line("후드 인덕션 켜지 마", 0, 0, 0);
  check(none && none->find("\"value\"") == std::string::npos, "NONE omits value");
  check(voice_line("후드 팬 꺼", 1, UINT32_MAX, UINT64_MAX).has_value(), "numeric boundaries");
  for (const auto* raw : {"푸드 강풍", "호등 인덕션 켜", "후도 펜을 약품으로 해주세요",
                           "후드 팬 꺼죠", "후드 인덕션 켜져", "후드 팬 약풍으로 켜줘"}) {
    const auto normalized = normalize_command_text(raw);
    check(normalized && parse_command(raw).has_value(), "internal alias is available");
    const auto line = voice_line(raw, .72, 1, 1);
    check(line && line->find("\"text\":\"" + *normalized + "\"") != std::string::npos &&
          line->find("\"conf\":0.72}}\n") != std::string::npos,
          "wire canonical activation and text");
  }
  const auto alias_noun = voice_line("후드 펜을 약품으로 해주세요", .72, 1, 1);
  check(alias_noun && alias_noun->find("\"intent\":\"FAN_SET\",\"value\":1") != std::string::npos &&
        alias_noun->find("\"text\":\"후드 팬을 약풍으로 해주세요\"") != std::string::npos,
        "wire canonical noun aliases");
  for (const auto& pair : {std::pair<double, const char*>{.72, "0.72"},
                           {.41, "0.41"}, {.123456, "0.1235"}, {1e-9, "1e-09"}}) {
    const auto rounded = voice_line("후드 강풍", pair.first, 1, 1);
    check(rounded && rounded->find(std::string("\"conf\":") + pair.second + "}}\n") != std::string::npos,
          "four significant digits without zeroing small scores");
  }
  for (double conf : {-0.1, 1.1, std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
    check(!voice_line("후드 강풍", conf, 1, 1), "invalid confidence");
  for (const auto& text : {std::string(""), std::string("푸드 인덕션 켜지 마"), std::string("인덕션 켜"),
                           std::string("후드\x80"), std::string("후드") + '\0',
                           std::string("후드") + std::string(199, 'a')})
    check(!voice_line(text, .5, 1, 1), "invalid transcript");
  std::string boundary = "후드";
  for (int i = 0; i < 198; ++i) boundary += "😀";
  const auto encoded = voice_line(boundary, .5, 1, 1);
  check(encoded && encoded->size() <= 4096, "200 Unicode characters");
  check(!voice_line(boundary + "😀", .5, 1, 1), "201 Unicode characters");
  const auto hi_lg = voice_line("하이 엘쥐 불 색이 내려줘", 0, 1, 1, Activation::HiLg);
  check(hi_lg && hi_lg->find("\"intent\":\"IND_DOWN\"") != std::string::npos &&
        hi_lg->find("\"text\":\"하이엘지, 불 색이 내려줘\"") != std::string::npos &&
        hi_lg->find("\"conf\":0") != std::string::npos, "HiLg final text and unavailable score");
  check(!voice_line("하이엘지 후드 강풍", .7, 1, 1, Activation::HiLg), "HiLg cannot invent ASR score");
  for (int level = 1; level <= 3; ++level) {
    const std::string heat = level == 1 ? "약불" : level == 2 ? "중불" : "강불";
    for (const auto& body : {"인덕션 " + std::to_string(level) + "단계로 해줘",
                             "인덕션 " + heat + "로 해줘"}) {
      const auto line = voice_line("하이엘지 " + body, 0, 3, 99, Activation::HiLg);
      check(line && line->find("{\"v\":2,") == 0 &&
            line->find("\"intent\":\"IND_SET\",\"value\":" + std::to_string(level)) != std::string::npos,
            "HiLg absolute induction uses voice v2 and a bounded value");
    }
  }
  check(!is_valid_command({Intent::IND_SET, std::nullopt}), "IND_SET requires value");
  for (int value : {-1, 0, 1, 2, 3, 4})
    check(is_valid_command({Intent::IND_SET, value}) == (value >= 1 && value <= 3),
          "IND_SET bounds without clamping");
  check(!voice_line("후드 강풍", 0, 1, 1, Activation::HiLg), "HiLg sender requires its activation");
  const auto legacy = voice_line("하이엘지 후드 강풍", 0, 1, 1);
  check(legacy && legacy->find("\"intent\":\"NONE\"") != std::string::npos,
        "legacy does not execute HiLg prefix");
  for (const auto* text : {"하이엘지 후드 켜", "하이엘지 팬 켜줘", "하이엘지 후드 켜져",
                           "하이엘지 후드 켜?", "하이엘지 호두 켜줘"}) {
    const auto line = voice_line(text, 0, 1, 1, Activation::HiLg);
    check(line && line->find("\"intent\":\"FAN_SET\",\"value\":1") != std::string::npos,
          "PM default fan-on requests level one; Core applies fan_min");
  }
  for (const auto* text : {"하이엘지 인덕션 켜지 마", "하이엘지 인덕션 켜도 돼?",
                           "하이엘지 후드 2단으로 내려줘", "하이엘지 인덕션 끄지 마?",
                           "하이엘지 꺼", "하이엘지 꺼줘", "하이엘지 멈춰줘"}) {
    const auto line = voice_line(text, 0, 1, 1, Activation::HiLg);
    check(line && line->find("\"intent\":\"NONE\"") != std::string::npos, "HiLg NONE boundaries");
  }
  for (const auto& pair : {std::pair<const char*, const char*>{"하이엘지 인덕션 꺼?", "IND_OFF"},
                           {"하이엘지 인덕션 켜？", "IND_ON"}}) {
    const auto line = voice_line(pair.first, 0, 1, 1, Activation::HiLg);
    check(line && line->find(std::string("\"intent\":\"") + pair.second + "\"") != std::string::npos &&
          line->find('?') == std::string::npos && line->find("？") == std::string::npos,
          "HiLg terminal question punctuation preserves command and final text");
  }
  const auto legacy_question = voice_line("후드 인덕션 꺼?", .75, 1, 1);
  check(legacy_question && legacy_question->find("\"intent\":\"NONE\"") != std::string::npos,
        "terminal question policy is restricted to HiLg mode");
  const auto fuzzy_level = voice_line("하이엘지 호두 중궁", 0, 1, 1, Activation::HiLg);
  check(fuzzy_level && fuzzy_level->find("\"intent\":\"FAN_SET\",\"value\":2") != std::string::npos &&
        fuzzy_level->find("\"text\":\"하이엘지, 후드 중풍\"") != std::string::npos,
        "bounded fuzzy level has canonical wire text");
  const auto escaped = voice_line("후드 \"인덕션\" \\ 켜\t\n\r", .5, 1, 1);
  check(escaped && escaped->find("\\\"인덕션\\\"") != std::string::npos &&
        escaped->find("\\\\") != std::string::npos && escaped->find("\\t\\n\\r") != std::string::npos &&
        escaped->find('\n') == escaped->size() - 1, "single physical line and JSON escaping");
  for (const auto& host : {std::string("localhost"), std::string("127.0.0.999"),
                          std::string("127.0.0.1\0extra", 15)}) {
    bool rejected = false;
    try { VoiceTcpClient client(host, 7420); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "invalid numeric address");
  }
  bool port_rejected = false;
  try { VoiceTcpClient client("127.0.0.1", 0); } catch (const std::invalid_argument&) { port_rejected = true; }
  check(port_rejected, "zero port");
  VoiceTcpClient client("127.0.0.1", 7420);
  bool deferred_ran = false;
  auto deferred = std::async(std::launch::deferred, [&] { deferred_ran = true; return 0; });
  bool deferred_rejected = false;
  try { client.wait_with_service(deferred); } catch (const std::invalid_argument&) { deferred_rejected = true; }
  check(deferred_rejected && !deferred_ran, "deferred task cannot block network service");
  std::future<int> invalid;
  bool invalid_rejected = false;
  try { client.wait_with_service(invalid); } catch (const std::future_error&) { invalid_rejected = true; }
  check(invalid_rejected, "missing inference task is rejected");
  std::cout << "tcp_checks=" << checks << " failures=" << failures << '\n';
  return failures == 0 ? 0 : 1;
}

std::string unhex(const std::string& hex) {
  if (hex.size() % 2 != 0 || hex.size() > 2000) throw std::invalid_argument("invalid test text");
  std::string text;
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    std::size_t used = 0;
    const auto byte = std::stoul(hex.substr(i, 2), &used, 16);
    if (used != 2) throw std::invalid_argument("invalid test hex");
    text += static_cast<char>(byte);
  }
  return text;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--self-test") return self_test();
    if (argc < 2 || argc > 3 || (argc == 3 && std::string(argv[2]) != "--hi-lg"))
      throw std::invalid_argument("test receiver port and optional --hi-lg required");
    const auto port = std::stoi(argv[1]);
    if (port <= 0 || port > 65535) throw std::invalid_argument("invalid test port");
    VoiceTcpClient client("127.0.0.1", static_cast<std::uint16_t>(port),
                          argc == 3 ? Activation::HiLg : Activation::Hood);
    std::cout << std::unitbuf;
    std::string line;
    while (std::getline(std::cin, line)) {
      std::istringstream input(line);
      std::string op;
      input >> op;
      if (op == "wait" || op == "start" || op == "idle") {
        int ms = 0;
        input >> ms;
        if (!input || ms < 0 || ms > 10000) throw std::invalid_argument("invalid test wait");
        if (op == "start") client.service();
        if (op == "wait") {
          const auto until = VoiceTcpClient::Clock::now() + std::chrono::milliseconds(ms);
          do { client.service(); std::this_thread::sleep_for(1ms); } while (VoiceTcpClient::Clock::now() < until);
        } else {
          // start는 connect 시작 뒤, idle은 연결 뒤의 긴 추론 대기를 재현한다.
          std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        std::cout << "status " << client.connected() << ' ' << client.received_lines() << '\n';
      } else if (op == "infer" || op == "infer-fail") {
        int ms = 0;
        input >> ms;
        if (!input || ms < 0 || ms > 10000) throw std::invalid_argument("invalid inference wait");
        auto task = std::async(std::launch::async, [ms, fail = op == "infer-fail"] {
          std::this_thread::sleep_for(std::chrono::milliseconds(ms));
          if (fail) throw std::runtime_error("test inference failure");
          return 37;
        });
        try {
          if (client.wait_with_service(task) != 37) throw std::runtime_error("inference result lost");
        } catch (const std::runtime_error& error) {
          if (std::string(error.what()) != "test inference failure") throw;
          std::cout << "task_error ";
        }
        std::cout << "status " << client.connected() << ' ' << client.received_lines() << '\n';
      } else if (op == "publish") {
        std::uint64_t id = 0;
        int quality = 0;
        int budget_ms = 0;
        double conf = 0;
        std::string hex;
        input >> id >> quality >> budget_ms >> conf >> hex;
        if (!input) throw std::invalid_argument("invalid test publish");
        const auto deadline = VoiceTcpClient::Clock::now() + std::chrono::milliseconds(budget_ms);
        const auto result = client.publish(id, hex == "-" ? "" : unhex(hex), conf, quality == 1, deadline);
        std::cout << result_name(result) << '\n';
      } else if (op == "quit") {
        return 0;
      } else throw std::invalid_argument("unknown test command");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
