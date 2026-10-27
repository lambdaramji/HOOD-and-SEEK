// MLX90640·SPS30 읽기. 프로토콜은 10/19 #45 실측에 쓴 도구를 옮겼다:
//   MLX90640 — Melexis API(third_party/mlx90640, Apache-2.0) + inbox/정정인/mlx_bundle.sh의 mlx_stream 설정·검증
//   SPS30    — inbox/정정인/sps30_bundle.sh의 sps30_test (Sensirion I2C 프로토콜 직접 구현, 외부 라이브러리 없음)
#include "sensors.h"

#include <cstdio>

#if defined(__linux__)

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <vector>

#include "mlx90640/MLX90640_API.h"
#include "mlx90640/MLX90640_I2C_Driver.h"

namespace {

int g_fd = -1;  // MLX90640_I2C* 함수와 SPS30이 같이 쓰는 /dev/i2c-1 (sensor 스레드 하나만 만진다)

constexpr uint8_t kMlxAddr = 0x33;
constexpr uint8_t kSpsAddr = 0x69;
constexpr float kEmissivity = 0.95f;  // mlx_stream과 같은 값 (#45)
constexpr float kTaShift = 8.0f;      // Melexis 권장 반사 온도 보정 (개방형 설치)
constexpr int kMinValidPixels = 700;  // 768 중 이만큼 정상이어야 프레임 채택
constexpr std::uint64_t kLoopMs = 50;
constexpr std::uint64_t kRetryMs = 2000;
constexpr std::uint64_t kSpsPeriodMs = 1000;  // SPS30 1 Hz (설계서 §5.1)
constexpr std::uint64_t kSpsRestartMs = 3000;  // 이 시간 새 샘플이 없으면 측정 시작 명령을 다시 보낸다(센서 전원 재인가 대비)

// ── SPS30 (Sensirion I2C: 16비트 명령, 2바이트마다 CRC-8 poly 0x31 init 0xFF) ──
constexpr uint16_t kSpsStart = 0x0010, kSpsDataReady = 0x0202, kSpsReadMeas = 0x0300;

uint8_t crc8(const uint8_t* d, int n) {
  uint8_t c = 0xFF;
  for (int i = 0; i < n; ++i) {
    c ^= d[i];
    for (int b = 0; b < 8; ++b) c = (c & 0x80) ? static_cast<uint8_t>((c << 1) ^ 0x31) : static_cast<uint8_t>(c << 1);
  }
  return c;
}

bool sps_select() { return ioctl(g_fd, I2C_SLAVE, kSpsAddr) >= 0; }

bool sps_start() {
  uint8_t b[5] = {kSpsStart >> 8, kSpsStart & 0xFF, 0x03, 0x00, 0};  // 0x0300 = float 출력
  b[4] = crc8(&b[2], 2);
  return sps_select() && write(g_fd, b, 5) == 5;
}

bool sps_read(uint16_t cmd, uint8_t* out, int nwords, int delay_ms) {
  const uint8_t c[2] = {static_cast<uint8_t>(cmd >> 8), static_cast<uint8_t>(cmd & 0xFF)};
  if (!sps_select() || write(g_fd, c, 2) != 2) return false;
  usleep(delay_ms * 1000);
  uint8_t buf[60];
  const int nb = nwords * 3;
  if (read(g_fd, buf, nb) != nb) return false;
  for (int i = 0; i < nwords; ++i) {
    if (crc8(&buf[i * 3], 2) != buf[i * 3 + 2]) return false;  // CRC가 틀린 샘플은 버린다
    out[i * 2] = buf[i * 3];
    out[i * 2 + 1] = buf[i * 3 + 1];
  }
  return true;
}

float be_float(const uint8_t* p) {
  const uint32_t u = (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | uint32_t{p[3]};
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

}  // namespace

// ── MLX90640_I2C_Driver.h 구현 (포크 드라이버와 같은 I2C_RDWR 방식, #45에서 이 방식으로 실측) ──
void MLX90640_I2CInit() {}
void MLX90640_I2CFreqSet(int) {}

int MLX90640_I2CRead(uint8_t addr, uint16_t start, uint16_t nwords, uint16_t* data) {
  if (g_fd < 0 || nwords > 832) return -1;
  uint8_t cmd[2] = {static_cast<uint8_t>(start >> 8), static_cast<uint8_t>(start & 0xFF)};
  uint8_t buf[1664];
  i2c_msg msgs[2] = {{addr, 0, 2, cmd}, {addr, I2C_M_RD | I2C_M_NOSTART, static_cast<uint16_t>(nwords * 2), buf}};
  i2c_rdwr_ioctl_data set{msgs, 2};
  if (ioctl(g_fd, I2C_RDWR, &set) < 0) return -1;
  for (int i = 0; i < nwords; ++i) data[i] = static_cast<uint16_t>((buf[2 * i] << 8) | buf[2 * i + 1]);
  return 0;
}

int MLX90640_I2CWrite(uint8_t addr, uint16_t reg, uint16_t value) {
  if (g_fd < 0) return -1;
  uint8_t cmd[4] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF), static_cast<uint8_t>(value >> 8),
                    static_cast<uint8_t>(value & 0xFF)};
  i2c_msg msg{addr, 0, 4, cmd};
  i2c_rdwr_ioctl_data set{&msg, 1};
  return ioctl(g_fd, I2C_RDWR, &set) < 0 ? -1 : 0;
}

void SensorThread::run(const std::atomic<bool>& stop) {
  paramsMLX90640 params{};
  static uint16_t ee[832];
  static uint16_t frame[834];
  std::vector<float> to(768, NAN), buf;
  bool mlx_ready = false, got[2] = {false, false};
  int mlx_errors = 0;
  std::uint64_t mlx_try_ms = 0, sps_poll_ms = 0, sps_start_ms = 0, sps_last_ready_ms = 0;
  unsigned sps_saturated = 0, sps_dropped = 0, sps_restarts = 0;  // 누적 — 로그 줄에 같이 찍는다
  bool mlx_logged = false, sps_logged = false, open_logged = false;

  const auto mlx_fail = [&](const char* why) {
    if (++mlx_errors >= 5 && mlx_ready) {  // 순간 오류 몇 번은 넘기고, 이어지면 다시 초기화
      mlx_ready = false;
      std::fprintf(stderr, "[sensor] MLX90640 읽기 실패 연속 (%s) — %llu ms마다 재초기화\n", why,
                   static_cast<unsigned long long>(kRetryMs));
    }
  };

  while (!stop) {
    const std::uint64_t now = mono_ms();
    if (g_fd < 0) {
      g_fd = open(path_.c_str(), O_RDWR);
      if (g_fd < 0) {
        if (!open_logged) std::fprintf(stderr, "[sensor] %s 열기 실패: %s — 재시도\n", path_.c_str(), std::strerror(errno));
        open_logged = true;
        // 2 s를 한 번에 자지 않고 잘게 나눠 stop을 본다 → 종료 신호 뒤 join이 늦어지지 않게 (#82 재범 nit)
        for (std::uint64_t slept = 0; slept < kRetryMs && !stop; slept += kLoopMs) usleep(kLoopMs * 1000);
        continue;
      }
      std::printf("[sensor] %s 열림\n", path_.c_str());
    }

    // MLX90640: 초기화(EEPROM 보정값) → 데이터 준비 비트가 설 때만 프레임을 읽는다.
    // Melexis GetFrameData는 준비가 안 되면 최대 5 s 동안 버스를 두드리며 기다리므로, 먼저 상태 레지스터를 본다(SPS30을 굶기지 않게).
    if (!mlx_ready && now - mlx_try_ms >= kRetryMs) {
      mlx_try_ms = now;
      if (MLX90640_DumpEE(kMlxAddr, ee) == 0 && MLX90640_ExtractParameters(ee, &params) == 0 &&
          MLX90640_SetRefreshRate(kMlxAddr, 0x02) == 0 && MLX90640_SetChessMode(kMlxAddr) == 0) {
        mlx_ready = true;  // 0x02 = 서브페이지 2 Hz → 전체 프레임 1 Hz (#45 실측)
        mlx_errors = 0;
        got[0] = got[1] = false;
        std::printf("[sensor] MLX90640 준비 (0x33, 서브페이지 2 Hz)\n");
        mlx_logged = false;
      } else if (!mlx_logged) {
        std::fprintf(stderr, "[sensor] MLX90640 초기화 실패 (0x33) — %llu ms마다 재시도\n",
                     static_cast<unsigned long long>(kRetryMs));
        mlx_logged = true;
      }
    }
    if (mlx_ready) {
      uint16_t status = 0;
      if (MLX90640_I2CRead(kMlxAddr, 0x8000, 1, &status) != 0) {
        mlx_fail("상태 레지스터");
      } else if (status & 0x0008) {
        const int sp = MLX90640_GetFrameData(kMlxAddr, frame);
        const float vdd = sp >= 0 ? MLX90640_GetVdd(frame, &params) : 0;
        const float ta = sp >= 0 ? MLX90640_GetTa(frame, &params) : 0;
        if (sp < 0 || sp > 1) {
          mlx_fail("프레임");
        } else if (!(vdd > 2.0f && vdd < 4.5f && ta > -40.0f && ta < 125.0f)) {
          mlx_fail("Vdd·Ta 범위 밖");  // mlx_stream과 같은 검증 — 깨진 프레임
        } else {
          mlx_errors = 0;
          MLX90640_CalculateTo(frame, &params, kEmissivity, ta - kTaShift, to.data());
          got[sp] = true;
          if (got[0] && got[1]) {  // 체스판 서브페이지 2장 = 전체 프레임 1장
            got[0] = got[1] = false;
            buf.clear();
            for (float v : to) {
              if (std::isfinite(v) && v > -40.0f && v < 300.0f) buf.push_back(v);
            }
            if (static_cast<int>(buf.size()) >= kMinValidPixels) {
              std::nth_element(buf.begin(), buf.begin() + buf.size() / 2, buf.end());
              const float median = buf[buf.size() / 2];
              const float mx = *std::max_element(buf.begin(), buf.end());
              std::lock_guard<std::mutex> lock(snap_.mu);
              snap_.thermal_ok_ms = mono_ms();
              snap_.t_max_c = mx;
              snap_.t_median_c = median;
            } else {
              mlx_fail("정상 픽셀 부족");
            }
          }
        }
      }
    }

    // SPS30: 1 s마다 data-ready를 보고 새 샘플이면 읽는다. 새 샘플(data-ready + CRC 정상)이 3 s 없을 때만 측정 시작을
    // 다시 보낸다 — 값이 범위 밖이라는 이유로는 재시작하지 않는다(재시작하면 공백이 더 길어짐, #65 PM 팔로업 2번).
    if (now - sps_poll_ms >= kSpsPeriodMs) {
      sps_poll_ms = now;
      const std::uint64_t last = std::max(sps_start_ms, sps_last_ready_ms);
      if (sps_start_ms == 0 || now - last >= kSpsRestartMs) {
        sps_start_ms = now;
        if (sps_start()) {
          if (sps_logged || sps_last_ready_ms == 0) {
            std::printf("[sensor] SPS30 측정 시작 (0x69)\n");
          } else {
            std::printf("[sensor] SPS30 새 샘플 %llu ms 없음 → 측정 다시 시작 (누적 %u)\n",
                        static_cast<unsigned long long>(kSpsRestartMs), ++sps_restarts);
          }
          sps_logged = false;
        } else if (!sps_logged) {
          std::fprintf(stderr, "[sensor] SPS30 측정 시작 실패 (0x69) — %llu ms마다 재시도\n",
                       static_cast<unsigned long long>(kSpsRestartMs));
          sps_logged = true;
        }
      } else {
        uint8_t d[40];
        // 측정값 = float 10개 = 20워드(워드마다 CRC). 순서: pm1·pm2.5·pm4·pm10·nc0.5·nc1·nc2.5·nc4·nc10·tsize
        if (sps_read(kSpsDataReady, d, 1, 5) && d[1] == 0x01 && sps_read(kSpsReadMeas, d, 20, 20)) {
          sps_last_ready_ms = now;
          const float pm25 = be_float(d + 4);
          const float tsize = be_float(d + 36);
          // 버린 값은 pm_ok_ms가 갱신되지 않아 3 s 뒤 fault (F4). 포화·버림은 원값을 1줄씩 남긴다(샘플이 1 Hz라 1 s에 1줄)
          const SpsSample kind = classify_sps30(pm25, tsize);
          if (kind == SpsSample::kInvalid) {
            std::fprintf(stderr, "[sensor] SPS30 값 버림 pm2.5=%.2f tsize=%.3f (누적 %u)\n", pm25, tsize, ++sps_dropped);
          } else {
            if (kind == SpsSample::kSaturated) {
              std::printf("[sensor] SPS30 포화 pm2.5=%.2f → %.0f tsize=%.3f (누적 %u)\n", pm25, kSpsPm25Max, tsize,
                          ++sps_saturated);
            }
            std::lock_guard<std::mutex> lock(snap_.mu);
            snap_.pm_ok_ms = mono_ms();
            ++snap_.pm_seq;  // 1부터 — 0은 「아직 없음」이라 Policy가 새 샘플로 세지 않는다 (#79 리뷰 메모)
            snap_.pm25 = kind == SpsSample::kSaturated ? kSpsPm25Max : pm25;
            snap_.pm_saturated = kind == SpsSample::kSaturated;
            snap_.tsize = tsize;
          }
        }
      }
    }
    usleep(kLoopMs * 1000);
  }
  if (g_fd >= 0) close(g_fd);
  g_fd = -1;
}

#else  // 호스트(Mac) 빌드: I2C가 없다. 센서는 계속 무효 → Policy가 3 s 뒤 fault (호스트 시험용)

#include <chrono>
#include <thread>

void SensorThread::run(const std::atomic<bool>& stop) {
  (void)snap_;
  std::fprintf(stderr, "[sensor] 호스트 빌드 — I2C 없음 (%s), 센서 무효\n", path_.c_str());
  while (!stop) std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

#endif
