#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

// Pi 4 uart3(/dev/ttyAMA3)와 GPIO UART로 연결한다 (설계서 §4.2, Q-S6-1). 프로토콜 문자열은 USB 시절과 같다.
// Pi TX(물리 7, GPIO4) → ESP32 GPIO16(RX2), Pi RX(물리 29, GPIO5) ← ESP32 GPIO17(TX2), GND 공통 [실측 #44].
// USB Serial은 플래시·디버그 출력 전용이다. Pi 명령은 Serial2로만 받는다.
// ⚠ 파서(readPi·15자 버림)·fail-safe 타이머·워치독·reply()는 쿡탑 펌웨어(esp32_cooktop/src/main.cpp)와 같은 규칙이다.
//   한쪽을 고치면 다른 쪽도 같이 고친다 — 두 보드가 갈라지지 않게 (#87 병직 리뷰 2).
namespace {

constexpr uint8_t kFanPwmPin = 26;
constexpr uint8_t kPwmChannel = 0;
// 100 Hz: 25 kHz에서는 2선식 팬이 1·2단에서 돌다 멈춤 [실측 10/21 #64 L5]. 100 Hz는 2·3단 정지 상태 기동 OK
constexpr uint32_t kPwmFrequency = 100;
constexpr uint8_t kPwmResolutionBits = 8;
constexpr unsigned long kCommandTimeoutMs = 3000;
constexpr uint8_t kDutyByLevel[] = {0, 85, 170, 255};
// 1단(33 %)은 정지 상태에서 기동하지 못함(40·45 %도 못 함, 50 %는 2단과 구분 안 됨) → 0단에서 1단으로 갈 때만
// 이 시간 동안 3단 듀티로 돌린 뒤 1단으로 내린다. 0.3 s 2/2 기동·1분 무정지 [실측 10/22 #65]
constexpr unsigned long kKickMs = 300;
constexpr int kPiRxPin = 16;  // Serial2 RX ← Pi TX
constexpr int kPiTxPin = 17;  // Serial2 TX → Pi RX
// 워치독: loop가 이 시간 안에 돌지 않으면 리셋 → 부팅 fail-safe(팬 3단). 명령 timeout(3 s)보다 짧게 (D-039)
constexpr uint32_t kWatchdogTimeoutS = 2;

HardwareSerial& pi = Serial2;
char line[16]{};
size_t lineLength = 0;
bool discarding = false;  // 15자를 넘은 줄은 다음 \n까지 통째로 버린다 (#78 리뷰 1)
unsigned long lastValidCommandAt = 0;
bool failSafeActive = true;
uint8_t appliedLevel = 0xFF;  // 0xFF = 부팅 직후 아직 없음
bool kicking = false;
unsigned long kickStartedAt = 0;

void reply(const char* text) {
  pi.println(text);
  Serial.println(text);  // 디버그 사본
}

void applyFanLevel(uint8_t level) {
  if (level == appliedLevel) return;  // 1 s 재전송은 무시 — 킥 중이면 그대로 둔다
  kicking = appliedLevel == 0 && level == 1;
  kickStartedAt = millis();
  ledcWrite(kPwmChannel, kDutyByLevel[kicking ? 3 : level]);
  appliedLevel = level;
}

bool parseFanLevel(const char* input, uint8_t& level) {
  if (input[0] != 'F' || input[1] != 'A' || input[2] != 'N' || input[3] != ' ' ||
      input[5] != '\0') {
    return false;
  }
  if (input[4] < '0' || input[4] > '3') return false;
  level = static_cast<uint8_t>(input[4] - '0');
  return true;
}

void handleLine() {
  line[lineLength] = '\0';
  uint8_t level = 0;
  if (!parseFanLevel(line, level)) {
    reply("ERR invalid-command");
    return;
  }

  applyFanLevel(level);
  lastValidCommandAt = millis();
  failSafeActive = false;
  char ack[12];
  snprintf(ack, sizeof ack, "ACK FAN %u", level);
  reply(ack);
}

void readPi() {
  while (pi.available() > 0) {
    const char next = static_cast<char>(pi.read());
    if (next == '\r') continue;
    if (next == '\n') {
      if (!discarding) handleLine();  // 넘친 줄의 뒷부분은 명령으로 해석하지 않는다
      discarding = false;
      lineLength = 0;
      continue;
    }
    if (discarding) continue;
    if (lineLength + 1 >= sizeof(line)) {
      // 잡음 뒤에 "FAN 0"이 붙어 와도 실행되지 않게, 넘친 줄은 끝까지 버리고 ERR는 한 번만 보낸다
      discarding = true;
      lineLength = 0;
      reply("ERR command-too-long");
      continue;
    }
    line[lineLength++] = next;
  }
}

}  // namespace

void setup() {
  ledcSetup(kPwmChannel, kPwmFrequency, kPwmResolutionBits);
  ledcAttachPin(kFanPwmPin, kPwmChannel);
  applyFanLevel(3);  // 가장 먼저 fail-safe
  Serial.begin(115200);
  pi.begin(115200, SERIAL_8N1, kPiRxPin, kPiTxPin);
  const esp_err_t wdtInit = esp_task_wdt_init(kWatchdogTimeoutS, true);  // true = 시간 초과 시 패닉 → 리셋
  const esp_err_t wdtAdd = esp_task_wdt_add(nullptr);                    // loop 태스크를 감시 대상에 등록
  // 플랫폼이 바뀌어 설정이 실패하면 워치독이 꺼지거나 기본값(5 s)으로 남으므로 USB 디버그로 드러낸다 (#78 리뷰 4)
  Serial.printf("dbg wdt %us init=%d add=%d%s\n", static_cast<unsigned>(kWatchdogTimeoutS), wdtInit, wdtAdd,
                (wdtInit == ESP_OK && wdtAdd == ESP_OK) ? "" : " FAILED");
  Serial.printf("dbg reset_reason=%d (1 = power-on, 4 = panic, 6 = task watchdog)\n", static_cast<int>(esp_reset_reason()));
  lastValidCommandAt = millis();
  reply("READY FAILSAFE FAN 3");
}

void loop() {
  esp_task_wdt_reset();
  readPi();
  if (!failSafeActive && millis() - lastValidCommandAt > kCommandTimeoutMs) {
    applyFanLevel(3);
    failSafeActive = true;
    reply("FAILSAFE timeout FAN 3");
  }
  if (kicking && millis() - kickStartedAt >= kKickMs) {  // 막지 않고 loop에서 끝냄 — 그동안 명령·워치독 그대로
    kicking = false;
    ledcWrite(kPwmChannel, kDutyByLevel[appliedLevel]);
  }
#ifdef HOODSEEK_WDT_TEST
  // 워치독 시험 전용 빌드(pio run -e wdt_test): USB로 'H'를 받으면 loop를 멈춘다 → 2 s 뒤 리셋되어야 한다
  if (Serial.available() > 0 && Serial.read() == 'H') {
    Serial.println("dbg hang");
    for (;;) delayMicroseconds(100);  // 양보 없이 바쁘게 대기 (빈 무한 루프는 컴파일러가 없앨 수 있음)
  }
#endif
  delay(5);
}
