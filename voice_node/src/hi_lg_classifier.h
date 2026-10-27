#pragma once
#include "command_parser.h"
#include "hi_lg_rules.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <stdexcept>
namespace hoodseek::voice::hi_lg {
// Similarity is a jamo edit score, not ASR confidence. The sender must be
// explicitly selected and publication must remain opt-in.
inline constexpr double kActivationMinimum = 0.60;
inline constexpr double kTargetMinimum = 0.60;
inline constexpr double kActionMinimum = 0.70;
inline constexpr double kIntentMargin = 0.10;
struct Command {
  std::string intent = "NONE";
  int value = 0;
  std::string reason = "unsupported_whole_sentence";
  double target = -1, score = -1, activation = -1, action = -1;
  std::string normalized = "";
  bool default_fan_on = false;
  double level_similarity = -1;
};
inline bool starts(const std::string &s, const std::string &t) {
  return s.compare(0, t.size(), t) == 0;
}
inline bool ends(const std::string &s, const std::string &t) {
  return s.size() >= t.size() &&
         s.compare(s.size() - t.size(), t.size(), t) == 0;
}
inline std::u32string decode(const std::string &s) {
  std::u32string out;
  for (std::size_t i = 0; i < s.size();) {
    uint32_t c = static_cast<unsigned char>(s[i++]);
    int n = 0;
    uint32_t lo = 0;
    if (c >= 0xf0 && c <= 0xf4) {
      n = 3;
      c &= 7;
      lo = 0x10000;
    } else if (c >= 0xe0 && c <= 0xef) {
      n = 2;
      c &= 15;
      lo = 0x800;
    } else if (c >= 0xc2 && c <= 0xdf) {
      n = 1;
      c &= 31;
      lo = 0x80;
    } else if (c >= 0x80)
      throw std::runtime_error("utf8");
    for (int j = 0; j < n; ++j) {
      if (i == s.size())
        throw std::runtime_error("utf8");
      auto b = static_cast<unsigned char>(s[i++]);
      if ((b & 0xc0) != 0x80)
        throw std::runtime_error("utf8");
      c = (c << 6) | (b & 63);
    }
    if (c < lo || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
      throw std::runtime_error("utf8");
    out.push_back(c);
  }
  return out;
}
inline std::string encode(const std::u32string &s) {
  std::string o;
  for (auto c : s) {
    if (c < 0x80)
      o += char(c);
    else if (c < 0x800) {
      o += char(0xc0 | (c >> 6));
      o += char(0x80 | (c & 63));
    } else if (c < 0x10000) {
      o += char(0xe0 | (c >> 12));
      o += char(0x80 | ((c >> 6) & 63));
      o += char(0x80 | (c & 63));
    } else {
      o += char(0xf0 | (c >> 18));
      o += char(0x80 | ((c >> 12) & 63));
      o += char(0x80 | ((c >> 6) & 63));
      o += char(0x80 | (c & 63));
    }
  }
  return o;
}
inline bool space(char32_t c) {
  return (c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0 ||
         c == 0x1680 || (c >= 0x2000 && c <= 0x200a) || c == 0x2028 ||
         c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000;
}
inline std::string normalize(const std::string &s, bool remove = false) {
  auto x = decode(s);
  std::u32string n;
  for (std::size_t i = 0; i < x.size(); ++i) {
    char32_t c = x[i];
    if (c >= 0x1100 && c <= 0x1112 && i + 1 < x.size() && x[i + 1] >= 0x1161 &&
        x[i + 1] <= 0x1175) {
      c = 0xac00 + ((c - 0x1100) * 21 + (x[++i] - 0x1161)) * 28;
      if (i + 1 < x.size() && x[i + 1] >= 0x11a8 && x[i + 1] <= 0x11c2)
        c += x[++i] - 0x11a7;
    }
    if (space(c)) {
      if (!remove && !n.empty() && n.back() != 32)
        n += 32;
    } else
      n += c;
  }
  if (!n.empty() && n.back() == 32)
    n.pop_back();
  return encode(n);
}
inline std::string compact(const std::string &s) {
  std::u32string o;
  for (auto c : decode(s))
    if (!space(c) && c != U',' && c != U'，' && c != U'.' && c != U'!' &&
        c != U'！')
      o += c;
  return encode(o);
}
inline void replace_all(std::string &s, const std::string &a,
                        const std::string &b) {
  std::size_t p = 0;
  while ((p = s.find(a, p)) != std::string::npos) {
    s.replace(p, a.size(), b);
    p += b.size();
  }
}
inline bool executable(const Command &c) {
  return c.intent != "NONE" && c.intent != "DROP";
}
inline std::string name(hoodseek::voice::Intent i) {
  using I = hoodseek::voice::Intent;
  switch (i) {
  case I::FAN_SET:
    return "FAN_SET";
  case I::FAN_OFF:
    return "FAN_OFF";
  case I::IND_ON:
    return "IND_ON";
  case I::IND_SET:
    return "IND_SET";
  case I::IND_OFF:
    return "IND_OFF";
  case I::IND_UP:
    return "IND_UP";
  case I::IND_DOWN:
    return "IND_DOWN";
  default:
    return "NONE";
  }
}
inline Command literal(const std::string &body) {
  std::string legacy = body;
  bool has = starts(body, "후드");
  for (const auto &a : hoods)
    has = has || starts(body, a);
  if (!has)
    legacy = "후드 " + body;
  auto c = hoodseek::voice::parse_command(legacy);
  const auto target = compact(body);
  if (!has && c && c->intent == hoodseek::voice::Intent::FAN_OFF &&
      !starts(target, "팬") && !starts(target, "풍량") && !starts(target, "바람"))
    return {"NONE", 0, "target_required"};
  return c ? Command{name(c->intent), c->value.value_or(0),
                     "existing_whole_command"}
           : Command{};
}
inline std::string canonical_body(std::string body) {
  for (const auto &a : hoods)
    if (starts(body, a)) {
      body = "후드" + body.substr(a.size());
      break;
    }
  for (const auto &w : words)
    replace_all(body, w.first, w.second);
  return body;
}
inline Command semantic(const std::string &body) {
  if (body.empty())
    return {"NONE", 0, "activation_only"};
  auto b = normalize(body, true);
  bool question =
      b.find('?') != std::string::npos || b.find("？") != std::string::npos;
  if (question) {
    bool ok = false;
    std::string t = b;
    if (ends(t, "?"))
      t.pop_back();
    else if (ends(t, "？"))
      t.resize(t.size() - std::string("？").size());
    for (const auto &e : std::vector<std::string>{
             "줘", "주세요", "줄래", "줄래요", "끄세요", "켜세요", "올리세요",
             "높이세요", "내리세요", "줄이세요", "낮추세요"})
      if (t.size() > e.size() && ends(t, e))
        ok = true;
    if (!ok)
      return {"NONE", 0, "question_or_permission"};
  }
  static const std::regex refuse(
      "지마|말고|말아|않|(?:안|못)(?:켜|꺼|끄|해|올려|내려)|라고|다고|했다가|"
      "했지|켜고|끄고|그리고|더세게|라는|이란|꺼주고|켜주고|해주고|멈춰주고|"
      "올려주고|내려주고|높여주고|낮춰주고|있|켜짐|꺼짐|멈춤|상태");
  if (std::regex_search(b, refuse))
    return {"NONE", 0, "negation_quote_compound_or_relative"};
  std::string c = b;
  for (const auto &p : std::vector<std::string>{".", "!", "?", "？"})
    if (ends(c, p)) {
      c.resize(c.size() - p.size());
      break;
    }
  c = canonical_body(c);
  static const std::regex fan_target("팬|풍량|바람|약풍|중풍|강풍");
  if (c.find("인덕션") != std::string::npos && std::regex_search(c, fan_target))
    return {"NONE", 0, "multiple_device_targets"};
  static const std::regex heat_level("약불|중불|강불");
  if (std::regex_search(c, heat_level) && !starts(c, "인덕션"))
    return {"NONE", 0, "induction_target_required"};
  // Closed ASR alias for the complete fan-off request; never rewrite 거 globally.
  if (c == "팬거" || c == "팬꺼" || c == "페꺼" || c == "패꺼") {
    Command result{"FAN_OFF", 0, "fan_off_asr_alias"};
    result.normalized = "팬 꺼";
    return result;
  }
  auto base = literal(body);
  if (executable(base) || base.reason == "target_required")
    return base;
  if (!ends(b, "?") && !ends(b, "？") && ends(c, "죠")) {
    auto s = c.substr(0, c.size() - std::string("죠").size());
    for (const auto &v : std::vector<std::string>{"꺼", "켜", "멈춰", "올려",
                                                  "내려", "높여", "낮춰"})
      if (ends(s, v)) {
        c = s + "줘";
        break;
      }
  }
  std::set<std::pair<std::string, int>> choices;
  bool default_fan_on = false;
  for (const auto &r : rules())
    if (std::regex_match(c, r.pattern)) {
      choices.insert({r.intent, r.value});
      default_fan_on = default_fan_on || r.default_fan_on;
    }
  if (choices.size() == 1) {
    Command result{choices.begin()->first, choices.begin()->second,
                   "composed_whole_command"};
    result.default_fan_on = default_fan_on;
    return result;
  }
  return {"NONE", 0,
          choices.empty() ? "unsupported_whole_sentence" : "multiple_commands"};
}
inline std::u32string phones(const std::string &s) {
  const std::u32string l = U"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ",
                       v = U"ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ";
  static const std::vector<std::u32string> t = {
      U"",     U"ㄱ",   U"ㄲ",   U"ㄱㅅ", U"ㄴ",   U"ㄴㅈ", U"ㄴㅎ",
      U"ㄷ",   U"ㄹ",   U"ㄹㄱ", U"ㄹㅁ", U"ㄹㅂ", U"ㄹㅅ", U"ㄹㅌ",
      U"ㄹㅍ", U"ㄹㅎ", U"ㅁ",   U"ㅂ",   U"ㅂㅅ", U"ㅅ",   U"ㅆ",
      U"ㅇ",   U"ㅈ",   U"ㅊ",   U"ㅋ",   U"ㅌ",   U"ㅍ",   U"ㅎ"};
  std::u32string o;
  for (auto c : decode(s)) {
    if (c >= 0xac00 && c < 0xac00 + 11172) {
      auto n = c - 0xac00;
      auto li = n / 588;
      if (li != 11)
        o += l[li];
      o += v[(n % 588) / 28];
      o += t[n % 28];
    } else
      o += c;
  }
  return o;
}
inline double sim(const std::string &a, const std::string &b) {
  auto x = phones(a), y = phones(b);
  if (x.empty() || y.empty())
    return 0;
  std::vector<double> p(y.size() + 1);
  for (std::size_t j = 0; j < p.size(); ++j)
    p[j] = j;
  for (std::size_t i = 0; i < x.size(); ++i) {
    std::vector<double> n(y.size() + 1);
    n[0] = i + 1;
    for (std::size_t j = 0; j < y.size(); ++j)
      n[j + 1] = std::min({n[j] + 1, p[j + 1] + 1, p[j] + (x[i] != y[j])});
    p = std::move(n);
  }
  return 1 - p.back() / std::max(x.size(), y.size());
}
inline std::vector<std::string> tokens(std::string s) {
  static const std::vector<std::string> nouns = {
      "인덕션", "후드", "풍량", "바람", "화력", "세기", "팬", "불"};
  std::vector<std::string> o;
  std::string pending;
  while (!s.empty()) {
    std::string found;
    for (const auto &n : nouns)
      if (starts(s, n)) {
        found = n;
        break;
      }
    if (!found.empty()) {
      if (!pending.empty()) {
        o.push_back(pending);
        pending.clear();
      }
      o.push_back(found);
      s.erase(0, found.size());
      for (const auto &p :
           std::vector<std::string>{"을", "를", "은", "는", "의"})
        if (starts(s, p)) {
          s.erase(0, p.size());
          break;
        }
    } else {
      auto x = decode(s);
      pending += encode(std::u32string(1, x[0]));
      s.erase(0, encode(std::u32string(1, x[0])).size());
    }
  }
  if (!pending.empty())
    o.push_back(pending);
  return o;
}
inline double target_sim(const std::string &a, const std::string &b) {
  auto x = tokens(a), y = tokens(b);
  if (x.empty() || y.empty())
    return 0;
  std::vector<double> p(y.size() + 1);
  for (std::size_t j = 0; j < p.size(); ++j)
    p[j] = j;
  for (std::size_t i = 0; i < x.size(); ++i) {
    std::vector<double> n(y.size() + 1);
    n[0] = i + 1;
    for (std::size_t j = 0; j < y.size(); ++j)
      n[j + 1] = std::min({n[j] + 1, p[j + 1] + 1, p[j] + 1 - sim(x[i], y[j])});
    p = std::move(n);
  }
  return 1 - p.back() / std::max(x.size(), y.size());
}
inline void strip_particle(std::string &b) {
  for (const auto &p : std::vector<std::string>{"을", "를", "은", "는", "의"})
    if (ends(b, p)) {
      b.resize(b.size() - p.size());
      break;
    }
}
inline Command fallback(const std::string &body, Command base) {
  if (base.reason != "unsupported_whole_sentence")
    return base;
  auto b = canonical_body(compact(body));
  for (const auto &e :
       std::vector<std::string>{"주세요", "줄래요", "줄래", "줘", "죠"})
    if (ends(b, e)) {
      b.resize(b.size() - e.size());
      break;
    }
  std::vector<std::pair<std::string, std::string>> verbs = {
      {"설정해", "SET"},    {"맞춰", "SET"},      {"바꿔", "SET"},
      {"틀어", "SET"},      {"해", "SET"},        {"작동멈춰", "OFF"},
      {"정지해", "OFF"},    {"멈춰", "OFF"},      {"정지", "OFF"},
      {"끄세요", "OFF"},    {"꺼", "OFF"},        {"작동시켜", "ON"},
      {"켜세요", "ON"},     {"켜", "ON"},         {"올리세요", "UP"},
      {"높이세요", "UP"},   {"올려", "UP"},       {"높여", "UP"},
      {"낮추어", "DOWN"},   {"내리세요", "DOWN"}, {"줄이세요", "DOWN"},
      {"낮추세요", "DOWN"}, {"내려", "DOWN"},     {"줄여", "DOWN"},
      {"낮춰", "DOWN"}};
  std::stable_sort(verbs.begin(), verbs.end(), [](auto &a, auto &b) {
    return a.first.size() > b.first.size();
  });
  std::string action;
  for (const auto &v : verbs)
    if (ends(b, v.first)) {
      action = v.second;
      b.resize(b.size() - v.first.size());
      break;
    }
  if (ends(b, "좀"))
    b.resize(b.size() - std::string("좀").size());
  int level = 0;
  const std::vector<std::vector<std::string>> levels = {
      {"약풍", "1단", "일단", "한단", "약"},
      {"중풍", "2단", "이단", "두단", "중"},
      {"강풍", "3단", "삼단", "세단", "강"}};
  for (int i = 0; i < 3 && !level; ++i)
    for (const auto &f : levels[i]) {
      bool got = false;
      for (const auto &p : std::vector<std::string>{"으로", "로", ""})
        if (ends(b, f + p)) {
          b.resize(b.size() - f.size() - p.size());
          level = i + 1;
          got = true;
          break;
        }
      if (got)
        break;
    }
  double level_similarity = -1;
  if (!level) {
    auto level_text = b;
    for (const auto &p : std::vector<std::string>{"으로", "로"})
      if (ends(level_text, p)) {
        level_text.resize(level_text.size() - p.size());
        break;
      }
    const auto chars = decode(level_text);
    // Only a two-syllable level suffix with a remaining target can be fuzzy.
    // Keep the parsed action: a direction plus target level stays unsupported.
    if (chars.size() > 2) {
      const auto heard = encode(chars.substr(chars.size() - 2));
      double best = -1, second = -1;
      int selected = 0;
      for (int i = 0; i < 3; ++i) {
        const auto score = sim(heard, levels[i][0]);
        if (score > best) {
          second = best;
          best = score;
          selected = i + 1;
        } else {
          second = std::max(second, score);
        }
      }
      if (best + 1e-9 >= kTargetMinimum &&
          best - second + 1e-9 >= kIntentMargin) {
        level = selected;
        level_similarity = best;
        b = encode(chars.substr(0, chars.size() - 2));
      }
    }
  }
  if ((action.empty() || action == "ON") && level)
    action = "SET";
  strip_particle(b);
  for (const auto &m : std::vector<std::string>{"전원", "작동"})
    if (ends(b, m)) {
      b.resize(b.size() - m.size());
      break;
    }
  strip_particle(b);
  const std::vector<std::string> fan = {"후드",   "팬",         "풍량",
                                        "바람",   "후드팬",     "후드풍량",
                                        "팬풍량", "후드팬풍량", "후드바람",
                                        "팬바람", "후드팬바람"},
                                 ind = {"인덕션", "후드인덕션"};
  // A suffix such as 1단 must not turn 11단 or -1단 into a target-name typo.
  if (level && b.find_first_of("0123456789-") != std::string::npos) {
    base.reason = "target_too_different";
    return base;
  }
  std::vector<std::string> heat;
  for (const auto &p :
       std::vector<std::string>{"", "후드", "인덕션", "후드인덕션"})
    for (const auto &n : std::vector<std::string>{"화력", "불", "세기", "불세기"})
      heat.push_back(p + n);
  struct C {
    std::string intent;
    std::string group;
    const std::vector<std::string> *targets;
    int level;
  };
  std::vector<C> options = {
      {"FAN_SET", "SET", &fan, 1}, {"FAN_SET", "SET", &fan, 2},
      {"FAN_SET", "SET", &fan, 3}, {"FAN_OFF", "OFF", &fan, 0},
      {"IND_ON", "ON", &ind, 0},   {"IND_OFF", "OFF", &ind, 0},
      {"IND_UP", "UP", &heat, 0},  {"IND_DOWN", "DOWN", &heat, 0}};
  std::vector<Command> cand;
  for (const auto &o : options) {
    if (o.group != action || o.level != level)
      continue;
    double t = 0;
    if (b.empty() && level)
      t = 1;
    else
      for (const auto &n : *o.targets)
        t = std::max(t, target_sim(b, n));
    double score = level ? .30 * t + .40 + .30 : .40 * t + .60;
    cand.push_back({o.intent, o.level, "token_composition60", t, score});
  }
  if (cand.empty()) {
    base.reason = "no_explicit_action_or_level";
    return base;
  }
  std::stable_sort(cand.begin(), cand.end(), [](const auto &a, const auto &b) {
    return a.score > b.score;
  });
  auto top = cand[0];
  if (top.target < kTargetMinimum) {
    base.reason = "target_too_different";
    return base;
  }
  if (top.score < kTargetMinimum)
    return base;
  double margin = top.score - (cand.size() > 1 ? cand[1].score : 0);
  if (margin < kIntentMargin) {
    base.reason = "different_target_commands_too_close";
    return base;
  }
  if (level_similarity >= 0) {
    top.reason = "similar_level_form";
    top.level_similarity = level_similarity;
    top.normalized = "후드 " + levels[level - 1][0];
  }
  return top;
}
// Reuse the target-token similarity for bounded action-form candidates. Known
// exact actions with missing slots stay NONE ("켜줘", "바람 바꿔줘").
inline const std::vector<std::string> &action_forms() {
  static const std::vector<std::string> forms = [] {
    std::vector<std::string> out;
    for (const auto &stem : std::vector<std::string>{
             "설정해", "맞춰", "바꿔", "틀어", "해", "작동멈춰", "정지해",
             "멈춰", "정지", "꺼", "작동시켜", "켜", "올려", "높여", "낮추어",
             "내려", "줄여", "낮춰"})
      for (const auto &ending :
           std::vector<std::string>{"", "줘", "주세요", "줄래", "줄래요"})
        out.push_back(stem + ending);
    for (const auto &word :
         std::vector<std::string>{"끄세요", "켜세요", "올리세요", "높이세요",
                                  "내리세요", "줄이세요", "낮추세요"})
      out.push_back(word);
    return out;
  }();
  return forms;
}
inline Command classify_body(std::string body) {
  // Both units reuse the absolute-level grammar; relative directions still
  // have to match the complete command and remain unsupported.
  replace_all(body, "단계", "단");
  auto base = semantic(body);
  if (executable(base) || base.reason != "unsupported_whole_sentence")
    return base;
  auto current = fallback(body, base);
  if (executable(current))
    return current;
  auto text = compact(body);
  // Fuzziness never discards residual question marks or unrelated punctuation.
  if (text.find('?') != std::string::npos ||
      text.find("？") != std::string::npos)
    return current;
  // Known request-ending variants must not turn an undirected SET into DOWN.
  auto known_action = text;
  if (ends(known_action, "죠"))
    known_action.replace(known_action.size() - std::string("죠").size(),
                         std::string("죠").size(), "줘");
  for (const auto &word : action_forms())
    if (ends(known_action, word))
      return current;
  auto chars = decode(text);
  std::vector<Command> choices;
  for (const auto &form : action_forms()) {
    auto length = decode(form).size();
    for (std::size_t count = length > 1 ? length - 1 : 1;
         count <= length + 1 && count < chars.size(); ++count) {
      auto suffix = encode(chars.substr(chars.size() - count));
      double score = sim(suffix, form);
      if (score + 1e-9 < kActionMinimum)
        continue;
      auto corrected = encode(chars.substr(0, chars.size() - count)) + form;
      // A fuzzy action needs a complete known target grammar. Combining fuzzy
      // targets and actions can promote unrelated requests (e.g. "알려줘").
      auto candidate = semantic(corrected);
      if (!executable(candidate))
        continue;
      candidate.action = score;
      candidate.normalized = corrected;
      candidate.reason = "similar_action_form";
      auto same =
          std::find_if(choices.begin(), choices.end(), [&](const auto &c) {
            return c.intent == candidate.intent && c.value == candidate.value;
          });
      if (same == choices.end())
        choices.push_back(candidate);
      else if (same->action < candidate.action)
        *same = candidate;
    }
  }
  if (choices.empty())
    return current;
  std::stable_sort(
      choices.begin(), choices.end(),
      [](const auto &a, const auto &b) { return a.action > b.action; });
  if (choices.size() > 1 &&
      choices[0].action - choices[1].action + 1e-9 < kIntentMargin) {
    current.reason = "ambiguous_similar_action";
    return current;
  }
  return choices[0];
}
inline Command classify(const std::string &raw) {
  try {
    auto unicode = decode(raw);
    if (unicode.size() > 200)
      return {"DROP", 0, "invalid_text"};
    for (auto c : unicode)
      if ((c < 32 && c != 9 && c != 10 && c != 13) || c == 127)
        return {"DROP", 0, "invalid_text"};
    auto text = normalize(raw);
    std::string alias;
    double activation = 1;
    auto exact = aliases;
    std::stable_sort(
        exact.begin(), exact.end(),
        [](const auto &a, const auto &b) { return a.size() > b.size(); });
    for (const auto &a : exact)
      if (starts(text, a)) {
        alias = a;
        break;
      }
    if (alias.empty()) {
      activation = 0;
      auto input = decode(text);
      std::u32string prefix;
      std::size_t letters = 0;
      // Choose the best activation prefix independently of whether the body
      // would become executable. Never absorb extra words to manufacture a
      // command.
      for (std::size_t i = 0; i < input.size() && letters < 6; ++i) {
        auto c = input[i];
        prefix += c;
        if (space(c))
          continue;
        if (c < 0xac00 || c > 0xd7a3)
          break;
        if (++letters < 3)
          continue;
        auto compact_prefix = normalize(encode(prefix), true);
        double score = 0;
        for (const auto &a : aliases)
          score = std::max(score, sim(compact_prefix, normalize(a, true)));
        if (score > activation + 1e-9) {
          activation = score;
          alias = encode(prefix);
        }
      }
      if (activation + 1e-9 < kActivationMinimum)
        return {"DROP", 0, "activation_similarity_below_threshold"};
    }
    auto body = normalize(text.substr(alias.size()));
    if (starts(body, ","))
      body = normalize(body.substr(1));
    else if (starts(body, "，"))
      body = normalize(body.substr(std::string("，").size()));
    const auto original_body = body;
    // 호출어 확인 뒤 끝 물음표는 명령 문장부호로 취급한다. 내부 물음표나
    // 허가/상태/부정/복합 문장을 잘라 명령으로 만들지는 않는다.
    while (ends(body, "?") || ends(body, "？")) {
      body.resize(body.size() - (ends(body, "?") ? 1 : std::string("？").size()));
      body = normalize(body);
    }
    auto result = classify_body(body);
    result.activation = activation;
    result.normalized =
        "하이엘지, " + (result.normalized.empty()
                           ? (executable(result) ? body : original_body)
                           : result.normalized);
    return result;
  } catch (const std::runtime_error &) {
    return {"DROP", 0, "invalid_text"};
  }
}
} // namespace hoodseek::voice::hi_lg
