#include <Arduino.h>
#include <FastLED.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

// 쿡탑 ESP32 (설계서 §4.3·§4.4, #60). 쿡탑은 시뮬레이터다 — 화력은 Pi 4 SafetyCore의 `IND n`으로만 바뀌고
// 이 보드에는 물리 입력이 없다(Q-S7-1, #55). 판정은 하지 않는다. 잠금 표시도 Pi 4가 보낸 `LOCK x` 그대로.
//
// Pi 4 uart5(/dev/ttyAMA5)와 GPIO UART로 연결한다. 통신·fail-safe·워치독은 팬 펌웨어(#78)와 같은 구조:
//   Pi TX(물리 32, GPIO12) → [직렬 저항] → ESP32 GPIO16(RX2), Pi RX(물리 33, GPIO13) ← [직렬 저항] ← GPIO17(TX2), GND 공통.
//   USB Serial은 플래시·디버그 전용이다(같은 줄 사본 + `dbg …`). 디버그용 `BRI n`만 USB로 받는다.
// ⚠ 파서(readPi·15자 버림)·fail-safe 타이머·워치독·reply()는 팬 펌웨어(esp32_fan_controller/src/main.cpp)와 같은 규칙이다.
//   한쪽을 고치면 다른 쪽도 같이 고친다 — 두 보드가 갈라지지 않게 (#87 병직 리뷰 2).
// LED 표현(약·중·강 동심원)은 PM 지시서(2026-10-20 「가상 인덕션 화구 LED 표시」)의 규칙·숫자를 그대로 옮겼다.
namespace {

// ───────────── 하드웨어 ─────────────
constexpr int kPiRxPin = 16;   // Serial2 RX ← Pi TX
constexpr int kPiTxPin = 17;   // Serial2 TX → Pi RX
constexpr uint8_t kDataPin = 18;  // 매트릭스 DIN (220~330 Ω 직렬). 10/20 실측 배선. 박스 배선이 정해지면 이 줄만 바꾼다
constexpr uint8_t kWidth = 16, kHeight = 16;
constexpr uint16_t kNumLeds = kWidth * kHeight;
// 0번이 앞면 왼쪽 아래, 줄마다 방향이 바뀌는 지그재그 [실측 10/20 `z` 시험]. 무늬가 원형이라 방향은 결과에 영향 없음
constexpr bool kSerpentine = true;

// ───────────── 안전·전원 ─────────────
constexpr unsigned long kCommandTimeoutMs = 3000;  // 설계서 §4.1 — 정상 명령이 끊기면 OFF + 잠금 표시 (D-21)
constexpr uint32_t kWatchdogTimeoutS = 2;           // D-039. 명령 timeout보다 짧게
constexpr uint32_t kMaxMilliamps = 1500;            // LED 전원 상한 — FastLED가 밝기를 줄여 맞춘다 (PM 지시서·10/04 문서)
constexpr uint8_t kDefaultBrightness = 76;          // ≈ 30 % (D-036 밝기 상한). 아크릴 위에서 USB `BRI n`으로 맞춰 보고 PM과 정함

// ───────────── 표현 (PM 지시서 §1~§3) ─────────────
constexpr unsigned long kFadeMs = 350;
constexpr bool kFlickerOn = true;
constexpr unsigned long kBlinkHalfMs = 500;  // 잠금·fail-safe 외곽 링 1 Hz 점멸 (§4.4)

const CRGB kRed(255, 35, 0), kOrangeRed(255, 60, 0);  // 화구 코일 색은 이 둘의 가운데 하나 (10/26)
const CRGB kLockBlue(0, 60, 255);  // §4.4 잠금 = 외곽 링 파란색
const CRGB kFailRed(255, 18, 0);   // §4.4 FAILSAFE = 외곽 링 빨간색 (WS2812 순수 빨강은 분홍빛이라 RED_DEEP)
// 코일 링: 켜진 링 수 = 단수, 가운데는 끈다. 바깥 코일은 판 가장자리 LED까지. 색은 하나, 밝기만 단계마다 오른다 (10/26)
constexpr float kRCoil[3] = {2.75f, 4.8f, 7.2f};
// 링 폭. 안쪽은 칸이 적어 반지름·폭을 16×16 칸 거리(2.55·2.92)에 맞춰야 16칸이 고르게 켜진다 (10/26 실측: 2.4·0.55는 칸마다 35·42·19로 얼룩)
constexpr float kWCoil[3] = {0.40f, 0.55f, 0.55f};
constexpr float kRStatus = 7.0f;  // 잠금·fail-safe 링: 판 안에 들어가는 가장 큰 원 (화구 바깥 링과 거의 같은 원)

HardwareSerial& pi = Serial2;
// base = 잠금·fail-safe 링을 덧쓰기 전 화면. 페이드는 base끼리 섞는다 (링이 페이드 시작 화면에 섞이지 않게, #87 재범 리뷰 2)
CRGB leds[kNumLeds], base[kNumLeds], target[kNumLeds], prevFrame[kNumLeds];

// 상태 (Pi 4 명령으로만 바뀜)
uint8_t indLevel = 0;  // 0..3
bool lockOn = true;
bool failSafeActive = true;  // 부팅 = fail-safe (§4.1)
unsigned long lastValidCommandAt = 0;
unsigned long fadeStart = 0;
uint8_t shownStage = 255;  // 지금 target에 그려 둔 단계 (255 = 아직 없음)

char line[16]{};
size_t lineLength = 0;
bool discarding = false;  // 15자를 넘은 줄은 다음 \n까지 통째로 버린다 (팬 펌웨어 #78 리뷰 1과 같은 규칙)

void reply(const char* text) {
  pi.println(text);
  Serial.println(text);  // 디버그 사본
}

uint16_t xy(uint8_t x, uint8_t y) {
  if (kSerpentine && (y & 1)) return y * kWidth + (kWidth - 1 - x);
  return y * kWidth + x;
}


// 가우시안 링 (PM 지시서 §1): 링 중심선에서 떨어진 거리로 밝기를 매끄럽게 → 16×16에서도 균일한 원
float ringB(float d, float r, float w) {
  const float t = (d - r) / w;
  return expf(-t * t);
}
struct F3 {
  float r, g, b;
};
F3 lerpC(const CRGB& a, const CRGB& b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
void addC(F3& acc, const F3& c, float k) {
  acc.r += c.r * k;
  acc.g += c.g * k;
  acc.b += c.b * k;
}
bool onRing(float d) {
  for (float r : kRCoil)
    if (fabsf(d - r) < 0.8f) return true;
  return false;
}

// 코일 i의 밝기. 안쪽 코일은 대각선 4칸(칸 거리 2.12)도 링과 같은 밝기로 채워 네모가 아니라 원으로 보이게 한다 (10/26)
float coilB(uint8_t i, float d) {
  const float b = ringB(d, kRCoil[i], kWCoil[i]);
  return i == 0 ? b + 0.70f * ringB(d, 2.12f, 0.2f) : b;
}

// 단계별 규칙. PM 지시서 §3의 가우시안 링·팔레트를 쓰되 코일 링 수로 단계를 보인다 (10/26)
CRGB pixelFor(uint8_t stage, uint8_t x, uint8_t y) {
  const float dx = x - 7.5f, dy = y - 7.5f;
  const float d = sqrtf(dx * dx + dy * dy);
  const F3 coil = lerpC(kRed, kOrangeRed, 0.5f);  // 한 색. 단계는 링 수와 밝기로 가른다 (10/26)
  F3 acc{0, 0, 0};
  switch (stage) {
    case 1:  // 약: 안쪽 코일 하나 + 바로 밖 잔열 점
      addC(acc, coil, 0.60f * coilB(0, d));
      // 2단의 가운데 코일이 안쪽 링 바로 밖에 남기는 꼬리만 1단에도 그린다. 원이 더 둥글어 보이고 1단 → 2단이 이어진다 (10/26)
      if (d < kRCoil[0] + 1.1f) addC(acc, coil, 0.80f * ringB(d, kRCoil[1], kWCoil[1]));
      break;
    case 2:  // 중: 코일 둘
      for (uint8_t i = 0; i < 2; ++i) addC(acc, coil, 0.80f * coilB(i, d));
      break;
    case 3:  // 강: 코일 셋(바깥은 판 가장자리까지), 가장 밝게
      for (uint8_t i = 0; i < 3; ++i) addC(acc, coil, 1.00f * coilB(i, d));
      break;
    default:
      return CRGB::Black;  // 0: 소등
  }
  // 안쪽 링 안 8칸(칸 거리 1.58)에도 링 밖 꼬리와 같은 밝기(약 2/255)의 잔열 점. 모든 단계 같아 단계를 넘어가도 이어진다 (10/26)
  addC(acc, coil, 0.032f * ringB(d, 1.58f, 0.2f));
  return CRGB(uint8_t(fminf(255.f, acc.r + 0.5f)), uint8_t(fminf(255.f, acc.g + 0.5f)),
              uint8_t(fminf(255.f, acc.b + 0.5f)));
}

// 단계가 바뀔 때만 프레임을 다시 그리고 350 ms 크로스페이드
void showStage(uint8_t stage) {
  if (stage == shownStage) return;
  memcpy(prevFrame, base, sizeof(base));
  for (uint8_t y = 0; y < kHeight; ++y)
    for (uint8_t x = 0; x < kWidth; ++x) target[xy(x, y)] = pixelFor(stage, x, y);
  shownStage = stage;
  fadeStart = millis();
}

void enterFailSafe() {
  indLevel = 0;
  lockOn = true;
  failSafeActive = true;
  showStage(0);  // 중앙 소등 (§4.4 FAILSAFE)
  fadeStart = millis() - kFadeMs;  // fail-safe는 페이드 없이 즉시 소등 — 「3 s → OFF」를 화면에서도 (#87 병직 리뷰 1)
}

void render() {
  const unsigned long now = millis();
  const unsigned long el = now - fadeStart;
  if (el < kFadeMs) {
    const uint8_t t = uint8_t(el * 255 / kFadeMs);
    for (uint16_t i = 0; i < kNumLeds; ++i) base[i] = blend(prevFrame[i], target[i], t);
  } else {
    memcpy(base, target, sizeof(base));
    if (kFlickerOn && shownStage == 3) {  // 강불: 링 위 LED만 최대 6 % 느린 일렁임
      for (uint8_t y = 0; y < kHeight; ++y)
        for (uint8_t x = 0; x < kWidth; ++x) {
          const float dx = x - 7.5f, dy = y - 7.5f;
          if (onRing(sqrtf(dx * dx + dy * dy))) base[xy(x, y)].nscale8(240 + (inoise8(x * 40, y * 40, now / 6) >> 4));
        }
    }
  }
  memcpy(leds, base, sizeof(leds));
  // 외곽 원형 링 덧씀 (§4.4): fail-safe = 빨간 점멸(우선), 잠금 = 파란 점멸. 꺼진 반주기엔 아래 화면 그대로.
  // 화구와 같은 가우시안 링(반지름 kRStatus) — 사각 테두리보다 화구 모양에 맞다 (10/20 정인 의견)
  if ((failSafeActive || lockOn) && (now / kBlinkHalfMs) % 2 == 0) {
    const CRGB c = failSafeActive ? kFailRed : kLockBlue;
    for (uint8_t y = 0; y < kHeight; ++y)
      for (uint8_t x = 0; x < kWidth; ++x) {
        const float dx = x - 7.5f, dy = y - 7.5f;
        const float k = ringB(sqrtf(dx * dx + dy * dy), kRStatus, 0.55f);
        if (k > 0.03f) leds[xy(x, y)] = blend(leds[xy(x, y)], c, uint8_t(k * 255));
      }
  }
  FastLED.show();
}

// 허용 명령만 정확히 받는다 (불변식 7: allowlist·범위). 그 밖은 ERR, 상태·타이머 그대로
void handleLine() {
  line[lineLength] = '\0';
  char ack[16];
  if (lineLength == 5 && strncmp(line, "IND ", 4) == 0 && line[4] >= '0' && line[4] <= '3') {
    indLevel = uint8_t(line[4] - '0');
    snprintf(ack, sizeof ack, "ACK IND %u", indLevel);
  } else if (lineLength == 6 && strncmp(line, "LOCK ", 5) == 0 && (line[5] == '0' || line[5] == '1')) {
    lockOn = line[5] == '1';
    snprintf(ack, sizeof ack, "ACK LOCK %u", lockOn ? 1u : 0u);
  } else if (strcmp(line, "STAT") == 0) {  // 질의는 fail-safe 타이머를 살리지 않는다
    char stat[48];
    snprintf(stat, sizeof stat, "STAT ind=%u lock=%u fs=%u age=%lu", indLevel, lockOn ? 1u : 0u,
             failSafeActive ? 1u : 0u, millis() - lastValidCommandAt);
    reply(stat);
    return;
  } else {
    reply("ERR invalid-command");
    return;
  }
  lastValidCommandAt = millis();
  failSafeActive = false;
  showStage(indLevel);
  reply(ack);
}

void readPi() {
  while (pi.available() > 0) {
    const char next = char(pi.read());
    if (next == '\r') continue;
    if (next == '\n') {
      if (!discarding) handleLine();
      discarding = false;
      lineLength = 0;
      continue;
    }
    if (discarding) continue;
    if (lineLength + 1 >= sizeof(line)) {
      discarding = true;
      lineLength = 0;
      reply("ERR command-too-long");
      continue;
    }
    line[lineLength++] = next;
  }
}

// 디버그 전용 (USB): `BRI n` — 아크릴 투과 밝기 맞출 때. Pi 4 프로토콜이 아니다
void readUsbDebug() {
  static char buf[12];
  static size_t n = 0;
  while (Serial.available() > 0) {
    const char c = char(Serial.read());
#ifdef HOODSEEK_WDT_TEST
    if (c == 'H') {  // 워치독 시험 빌드: loop를 멈춘다 → 2 s 뒤 리셋 → 부팅 fail-safe
      Serial.println("dbg hang");
      for (;;) delayMicroseconds(100);
    }
#endif
    if (c == '\r' || c == '\n') {
      buf[n] = '\0';
      if (n > 4 && strncmp(buf, "BRI ", 4) == 0) {
        const int v = constrain(atoi(buf + 4), 0, 255);
        FastLED.setBrightness(uint8_t(v));
        Serial.printf("dbg BRI %d\n", v);
      }
      n = 0;
    } else if (n + 1 < sizeof buf) {
      buf[n++] = c;
    }
  }
}

}  // namespace

void setup() {
  FastLED.addLeds<WS2812B, kDataPin, GRB>(leds, kNumLeds);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, kMaxMilliamps);
  FastLED.setBrightness(kDefaultBrightness);
  FastLED.clear(true);  // 가장 먼저 전부 소등 (돌입 전류·이전 화면 잔상 방지)
  enterFailSafe();
  Serial.begin(115200);
  pi.begin(115200, SERIAL_8N1, kPiRxPin, kPiTxPin);
  const esp_err_t wdtInit = esp_task_wdt_init(kWatchdogTimeoutS, true);
  const esp_err_t wdtAdd = esp_task_wdt_add(nullptr);
  Serial.printf("dbg wdt %us init=%d add=%d%s\n", unsigned(kWatchdogTimeoutS), wdtInit, wdtAdd,
                (wdtInit == ESP_OK && wdtAdd == ESP_OK) ? "" : " FAILED");
  Serial.printf("dbg reset_reason=%d (1 = power-on, 4 = panic, 6 = task watchdog)\n", int(esp_reset_reason()));
  Serial.printf("dbg data_pin=%u brightness=%u max_mA=%u\n", kDataPin, kDefaultBrightness, unsigned(kMaxMilliamps));
  lastValidCommandAt = millis();
  reply("READY FAILSAFE IND 0 LOCK 1");
}

void loop() {
  esp_task_wdt_reset();
  readPi();
  readUsbDebug();
  if (!failSafeActive && millis() - lastValidCommandAt > kCommandTimeoutMs) {
    enterFailSafe();
    reply("FAILSAFE timeout IND 0 LOCK 1");
  }
  render();
  delay(16);  // ≈ 60 fps
}
