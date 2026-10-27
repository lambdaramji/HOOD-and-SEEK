// sps30_raw — SPS30 값을 하나도 버리지 않고 10개 전부 CSV로 남기는 기록 도구 (#65, 10/23 포화 구간 조사)
//
//   /data/mlx/sps30_raw /data/mlx/sat_mist2_1023.csv 240
//
// - Core(safetycore)가 돌고 있으면 시작하지 않는다. 같은 I2C 장치를 둘이 읽으면 값이 깨진다.
// - 범위 검사 없음: 2000 초과(포화)·NaN도 그대로 적는다. CRC가 틀린 샘플도 status=crc_bad로 적는다.
// - 프로토콜은 safety_core/src/sensors.cpp와 같다(0x69, float 출력, 1 Hz data-ready).
// - 새 샘플이 3 s 없으면 측정 시작을 다시 보내고 status=restart 줄을 남긴다.
// - 끝나면(시간 다 됨·Ctrl-C) 측정 중지(0x0104)를 보내 센서를 idle로 돌려 둔다. Core가 켜지면 다시 시작한다.
//
// 빌드(Mac, Docker): docker run --rm --platform linux/arm64 -v "$PWD":/w -w /w alpine:3.20
//   sh -c 'apk add --no-cache gcc musl-dev linux-headers >/dev/null && gcc -O2 -static -Wall -Wextra -o sps30_raw sps30_raw.c'
// 자체 시험: ./sps30_raw --selftest
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/i2c-dev.h>
#endif

static int g_fd = -1;
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static uint8_t crc8(const uint8_t* d, int n) {
  uint8_t c = 0xFF;
  for (int i = 0; i < n; ++i) {
    c ^= d[i];
    for (int b = 0; b < 8; ++b) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
  }
  return c;
}

static float be_float(const uint8_t* p) {
  uint32_t u = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
  float f;
  memcpy(&f, &u, 4);
  return f;
}

// 단조 시계(ms)
static double mono_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int sps_select(void) {
#ifdef __linux__
  return ioctl(g_fd, I2C_SLAVE, 0x69) >= 0;
#else
  return 0;
#endif
}

static int sps_cmd(uint16_t cmd, const uint8_t* arg2) {
  uint8_t b[5] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF), 0, 0, 0};
  int n = 2;
  if (arg2) {
    b[2] = arg2[0];
    b[3] = arg2[1];
    b[4] = crc8(&b[2], 2);
    n = 5;
  }
  return sps_select() && write(g_fd, b, n) == n;
}

// nwords 워드를 읽는다. CRC 하나라도 틀리면 *crc_ok=0 (값은 그대로 out에 채운다 — 버리지 않음)
static int sps_read(uint16_t cmd, uint8_t* out, int nwords, int delay_ms, int* crc_ok) {
  const uint8_t c[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
  if (!sps_select() || write(g_fd, c, 2) != 2) return 0;
  usleep(delay_ms * 1000);
  uint8_t buf[60];
  const int nb = nwords * 3;
  if (read(g_fd, buf, nb) != nb) return 0;
  *crc_ok = 1;
  for (int i = 0; i < nwords; ++i) {
    if (crc8(&buf[i * 3], 2) != buf[i * 3 + 2]) *crc_ok = 0;
    out[i * 2] = buf[i * 3];
    out[i * 2 + 1] = buf[i * 3 + 1];
  }
  return 1;
}

// /proc에서 safetycore 프로세스를 찾는다
static int core_running(void) {
  DIR* d = opendir("/proc");
  if (!d) return 0;
  struct dirent* e;
  int found = 0;
  while (!found && (e = readdir(d))) {
    if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
    char path[300], name[64] = {0};
    snprintf(path, sizeof path, "/proc/%s/comm", e->d_name);
    FILE* f = fopen(path, "r");
    if (!f) continue;
    if (fgets(name, sizeof name, f) && strncmp(name, "safetycore", 10) == 0) found = 1;
    fclose(f);
  }
  closedir(d);
  return found;
}

static int selftest(void) {
  const uint8_t a[2] = {0xBE, 0xEF};  // Sensirion 데이터시트 예: CRC(0xBEEF) = 0x92
  const uint8_t f[4] = {0x44, 0xFA, 0x00, 0x00};  // 2000.0f
  int ok = crc8(a, 2) == 0x92 && be_float(f) == 2000.0f;
  printf("selftest %s (crc 0x%02X, float %.1f)\n", ok ? "OK" : "FAIL", crc8(a, 2), be_float(f));
  return ok ? 0 : 1;
}

int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) return selftest();
  if (argc != 3) {
    fprintf(stderr, "사용: %s <csv 경로> <초>\n", argv[0]);
    return 2;
  }
  const double dur_s = atof(argv[2]);
  if (dur_s <= 0) {
    fprintf(stderr, "초는 0보다 커야 함\n");
    return 2;
  }
  if (core_running()) {
    fprintf(stderr, "safetycore가 돌고 있음 — 먼저 systemctl stop hoodseek-safety-core\n");
    return 1;
  }
  g_fd = open("/dev/i2c-1", O_RDWR);
  if (g_fd < 0) {
    perror("/dev/i2c-1");
    return 1;
  }
  FILE* out = fopen(argv[1], "wx");  // 기존 파일을 덮어쓰지 않는다
  if (!out) {
    perror(argv[1]);
    return 1;
  }
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  fprintf(out, "sec,clock,pm1,pm25,pm4,pm10,pc05,pc1,pc25,pc4,pc10,tsize,coarse_pm10_pm25,pm1_pm25,status\n");
  fflush(out);
  const uint8_t float_fmt[2] = {0x03, 0x00};
  const double t0 = mono_ms();
  double last_new = t0, last_start = t0;
  unsigned rows = 0, sat = 0, crc_bad = 0, restarts = 0;
  if (!sps_cmd(0x0010, float_fmt)) fprintf(stderr, "측정 시작 명령 실패 — 3 s 뒤 다시 시도\n");

  while (!g_stop) {
    const double now = mono_ms();
    const double sec = (now - t0) / 1000.0;
    if (sec >= dur_s) break;
    time_t wall = time(NULL);
    char clk[16];
    strftime(clk, sizeof clk, "%H:%M:%S", localtime(&wall));

    if (now - last_new >= 3000 && now - last_start >= 3000) {
      last_start = now;
      ++restarts;
      fprintf(out, "%.1f,%s,,,,,,,,,,,,,restart\n", sec, clk);
      fflush(out);
      sps_cmd(0x0010, float_fmt);
    }
    uint8_t d[40];
    int crc_ok = 0;
    if (sps_read(0x0202, d, 1, 5, &crc_ok) && crc_ok && d[1] == 0x01 && sps_read(0x0300, d, 20, 20, &crc_ok)) {
      last_new = now;
      float v[10];
      for (int i = 0; i < 10; ++i) v[i] = be_float(d + i * 4);
      const char* status = !crc_ok ? "crc_bad" : (v[1] > 2000.0f ? "sat" : "ok");
      if (!crc_ok) ++crc_bad;
      if (crc_ok && v[1] > 2000.0f) ++sat;
      fprintf(out, "%.1f,%s,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%.3f,%.3f,%s\n", sec, clk, v[0], v[1],
              v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[3] / v[1], v[0] / v[1], status);
      fflush(out);
      ++rows;
      printf("%5.0f초 %s  PM2.5 %9.1f  tsize %.3f  PM10/PM2.5 %.3f  PM1/PM2.5 %.3f  %s\n", sec, clk, v[1], v[9],
             v[3] / v[1], v[0] / v[1], status);
      fflush(stdout);
    }
    usleep(200 * 1000);
  }
  sps_cmd(0x0104, NULL);  // 측정 중지 → idle
  fflush(out);
  fsync(fileno(out));
  fclose(out);
  close(g_fd);
  printf("끝: %u줄 (포화 %u, crc_bad %u, 재시작 %u) → %s\n", rows, sat, crc_bad, restarts, argv[1]);
  return 0;
}
