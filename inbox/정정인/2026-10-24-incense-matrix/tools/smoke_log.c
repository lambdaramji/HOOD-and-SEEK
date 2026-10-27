// smoke_log — 인센스 시험 1회차를 폴더 하나에 통째로 기록한다 (#65, 10/24 PM 요청 로그 형식)
//
//   /data/mlx/smoke_log /data/mlx/runs/B1 900 0 <Mac epoch>
//     <폴더> <최대 초> <팬 단수 0~3> [기준 시각: Mac의 date +%s]
//
// 한 프로세스가 같은 시계(CLOCK_REALTIME)로 아래를 모두 찍는다.
//   sps30.csv   SPS30 10개 값 원값(2000 상한 없음) + 장치 상태 레지스터(0xD206) + 그때의 팬 명령
//   fan.csv     팬 ESP32로 1 s마다 보낸 "FAN n"(tx)과 받은 줄(rx: ACK·FAILSAFE·ERR) 전부
//   mlx.csv     /data/mlx/mlx_stream을 자식으로 띄워 받은 CSV에 epoch를 붙인 것.
//               배포본 mlx_stream(9열)의 delta는 p95 − median이다. Core T_metric은 max − median으로 따로 계산할 것 (10/24 정정)
//   events.csv  /data/mlx/ev 로 넣은 이벤트 (점화·놓음·치움·창문 …)
//   meta.txt    시작 epoch, Mac 기준 시각, 인자
//
// 이벤트: 다른 창에서 /data/mlx/ev 놓음
//   "fan N"  → 팬 명령을 N단으로 바꾼다 (D1 시험)
//   "환기"   → 그 뒤 PM2.5가 기준+5 안에 30 s 연속 머물면 저절로 끝난다
// 기준값 = 시작 후 55 s 동안 PM2.5 평균.
//
// - Core(safetycore)가 돌고 있으면 시작하지 않는다 (SPS30·팬 UART를 같이 잡으면 값과 명령이 섞인다).
// - 폴더가 이미 있으면 시작하지 않는다 (덮어쓰기 방지).
// - SSH가 끊겨도(SIGHUP) 기록은 계속된다. 끝내기: Ctrl-C(ssh -t) 또는 killall smoke_log.
// - 끝나면 SPS30 측정 중지, mlx_stream 종료. 팬 명령이 끊기므로 3 s 뒤 ESP32 fail-safe로 팬 3단이 된다.
//
// 빌드(Mac, Docker): docker run --rm --platform linux/arm64 -v "$PWD":/w -w /w alpine:3.20
//   sh -c 'apk add --no-cache gcc musl-dev linux-headers >/dev/null && gcc -O2 -static -Wall -Wextra -o smoke_log smoke_log.c'
// 자체 시험: ./smoke_log --selftest
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/i2c-dev.h>
#endif

#define MLX_STREAM "/data/mlx/mlx_stream"
#define CURRENT_LINK "/data/mlx/runs/current"
#define FAN_TTY "/dev/ttyAMA3"
#define BASE_S 55.0

static int g_i2c = -1;
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

static double epoch_now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double mono_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

// ── SPS30 (sps30_raw.c와 같은 프로토콜) ──
static int sps_select(void) {
#ifdef __linux__
  return ioctl(g_i2c, I2C_SLAVE, 0x69) >= 0;
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
  return sps_select() && write(g_i2c, b, n) == n;
}

static int sps_read(uint16_t cmd, uint8_t* out, int nwords, int delay_ms, int* crc_ok) {
  const uint8_t c[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
  if (!sps_select() || write(g_i2c, c, 2) != 2) return 0;
  usleep(delay_ms * 1000);
  uint8_t buf[60];
  const int nb = nwords * 3;
  if (read(g_i2c, buf, nb) != nb) return 0;
  *crc_ok = 1;
  for (int i = 0; i < nwords; ++i) {
    if (crc8(&buf[i * 3], 2) != buf[i * 3 + 2]) *crc_ok = 0;
    out[i * 2] = buf[i * 3];
    out[i * 2 + 1] = buf[i * 3 + 1];
  }
  return 1;
}

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

// ── 줄 단위로 따라 읽기 (팬 UART·ev_in·mlx_raw.csv 공용) ──
typedef struct {
  int fd;
  char buf[512];
  int len;
} LineTail;

// 완성된 줄 하나를 out에 담고 1, 없으면 0. 넘치는 줄은 잘라서 낸다.
static int tail_line(LineTail* t, char* out, int cap) {
  for (;;) {
    for (int i = 0; i < t->len; ++i) {
      if (t->buf[i] == '\n') {
        int n = i < cap - 1 ? i : cap - 1;
        memcpy(out, t->buf, n);
        out[n] = '\0';
        if (n > 0 && out[n - 1] == '\r') out[n - 1] = '\0';
        memmove(t->buf, t->buf + i + 1, t->len - i - 1);
        t->len -= i + 1;
        return 1;
      }
    }
    if (t->len == (int)sizeof t->buf) {
      int n = cap - 1 < t->len ? cap - 1 : t->len;
      memcpy(out, t->buf, n);
      out[n] = '\0';
      t->len = 0;
      return 1;
    }
    if (t->fd < 0) return 0;
    ssize_t r = read(t->fd, t->buf + t->len, sizeof t->buf - t->len);
    if (r <= 0) return 0;
    t->len += (int)r;
  }
}

static int open_fan_tty(void) {
  int fd = open(FAN_TTY, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) return -1;
  struct termios tio;
  if (tcgetattr(fd, &tio) == 0) {
    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~HUPCL;
    tcsetattr(fd, TCSANOW, &tio);
  }
  tcflush(fd, TCIFLUSH);
  return fd;
}

static pid_t spawn_mlx(const char* dir) {
  char csv[600], out[600];
  snprintf(csv, sizeof csv, "%s/mlx_raw.csv", dir);
  snprintf(out, sizeof out, "%s/mlx_stream.out", dir);
  pid_t p = fork();
  if (p != 0) return p;
  signal(SIGHUP, SIG_IGN);  // SSH가 끊겨도 같이 죽지 않게 (exec 뒤에도 유지됨)
  int o = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (o >= 0) {
    dup2(o, 1);
    dup2(o, 2);
  }
  execl(MLX_STREAM, MLX_STREAM, "8080", "2", csv, (char*)NULL);  // 서브페이지 2 Hz = 전체 프레임 1 Hz (Core와 같음)
  _exit(127);
}

// 이벤트 'fan N' 파싱: N(0~3) 또는 -1
static int parse_fan_event(const char* s) {
  if (strncmp(s, "fan ", 4) != 0) return -1;
  if (s[4] < '0' || s[4] > '3' || s[5] != '\0') return -1;
  return s[4] - '0';
}

// 헤더에서 이름이 name인 열 번호(0부터), 없으면 -1
static int csv_col(const char* header, const char* name) {
  const size_t n = strlen(name);
  int col = 0;
  for (const char* p = header;; ++col) {
    if (strncmp(p, name, n) == 0 && (p[n] == ',' || p[n] == '\0')) return col;
    p = strchr(p, ',');
    if (!p) return -1;
    ++p;
  }
}

// col번째 필드 시작 위치, 없으면 NULL
static const char* csv_field(const char* line, int col) {
  if (col < 0) return NULL;
  const char* p = line;
  for (int i = 0; i < col && p; ++i)
    if ((p = strchr(p, ','))) ++p;
  return p && *p && *p != ',' ? p : NULL;
}

static int selftest(void) {
  const uint8_t a[2] = {0xBE, 0xEF};
  const uint8_t f[4] = {0x44, 0xFA, 0x00, 0x00};
  int ok = crc8(a, 2) == 0x92 && be_float(f) == 2000.0f;
  ok = ok && parse_fan_event("fan 3") == 3 && parse_fan_event("fan 0") == 0 && parse_fan_event("fan 4") == -1 &&
       parse_fan_event("fan 33") == -1 && parse_fan_event("놓음") == -1;
  ok = ok && csv_col("sec,max,p95,median,delta,hot_px", "delta") == 4 &&
       csv_col("sec,max,p95,p99,hotmean,median,delta,hot_px", "delta") == 6 && csv_col("sec,max", "delta") == -1;
  const char* fd = csv_field("12.0,40.5,30.1,25.0,15.50,3", 4);
  ok = ok && fd && strtof(fd, NULL) == 15.5f && csv_field("1,2", 5) == NULL;
  int p[2];
  if (pipe(p) == 0) {
    LineTail t = {p[0], {0}, 0};
    fcntl(p[0], F_SETFL, O_NONBLOCK);
    const char* s = "ACK FAN 1\r\nFAILSA";
    write(p[1], s, strlen(s));
    char line[64];
    ok = ok && tail_line(&t, line, sizeof line) && strcmp(line, "ACK FAN 1") == 0;
    ok = ok && !tail_line(&t, line, sizeof line);
    write(p[1], "FE timeout FAN 3\n", 17);
    ok = ok && tail_line(&t, line, sizeof line) && strcmp(line, "FAILSAFE timeout FAN 3") == 0;
    close(p[0]);
    close(p[1]);
  }
  printf("selftest %s\n", ok ? "OK" : "FAIL");
  return ok ? 0 : 1;
}

int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) return selftest();
  if (argc != 4 && argc != 5) {
    fprintf(stderr, "사용: %s <폴더> <최대 초> <팬 0~3> [Mac epoch]\n", argv[0]);
    return 2;
  }
  const char* dir = argv[1];
  const double max_s = atof(argv[2]);
  int fan = (argv[3][0] >= '0' && argv[3][0] <= '3' && argv[3][1] == '\0') ? argv[3][0] - '0' : -1;
  if (max_s <= 0 || fan < 0) {
    fprintf(stderr, "최대 초 > 0, 팬 0~3 이어야 함\n");
    return 2;
  }
  if (core_running()) {
    fprintf(stderr, "safetycore가 돌고 있음 — 먼저 systemctl stop hoodseek-safety-core\n");
    return 1;
  }
  mkdir("/data/mlx/runs", 0755);  // 회차 폴더의 기본 위치
  if (mkdir(dir, 0755) != 0) {
    fprintf(stderr, "%s: %s (이미 있으면 다른 이름으로)\n", dir, strerror(errno));
    return 1;
  }
  g_i2c = open("/dev/i2c-1", O_RDWR);
  if (g_i2c < 0) {
    perror("/dev/i2c-1");
    return 1;
  }
  int fan_fd = open_fan_tty();
  if (fan_fd < 0) {
    perror(FAN_TTY);
    return 1;
  }

  char path[600];
#define OPEN_OUT(var, name)                         \
  snprintf(path, sizeof path, "%s/%s", dir, name);  \
  FILE* var = fopen(path, "wx");                    \
  if (!var) {                                       \
    perror(path);                                   \
    return 1;                                       \
  }
  OPEN_OUT(sps, "sps30.csv")
  OPEN_OUT(fanf, "fan.csv")
  OPEN_OUT(mlx, "mlx.csv")
  OPEN_OUT(evf, "events.csv")
  OPEN_OUT(meta, "meta.txt")
  snprintf(path, sizeof path, "%s/ev_in", dir);
  int ev_fd = open(path, O_RDONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0666);
  chmod(path, 0666);
  unlink(CURRENT_LINK);
  char absdir[600];
  if (dir[0] == '/') snprintf(absdir, sizeof absdir, "%s", dir);
  else {
    char cwd[400];
    snprintf(absdir, sizeof absdir, "%s/%s", getcwd(cwd, sizeof cwd) ? cwd : ".", dir);
  }
  if (symlink(absdir, CURRENT_LINK) != 0) perror("symlink " CURRENT_LINK);

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);

  const double t0 = mono_s();
  const double e0 = epoch_now();
  time_t w0 = (time_t)e0;
  char pidate[64];
  strftime(pidate, sizeof pidate, "%Y-%m-%d %H:%M:%S %z", localtime(&w0));
  fprintf(meta, "start_epoch_pi=%.3f\npi_date=%s\nmac_epoch_at_launch=%s\nmax_s=%s\nfan_start=%d\n", e0, pidate,
          argc == 5 ? argv[4] : "", argv[2], fan);
  if (argc == 5) fprintf(meta, "pi_minus_mac_s=%.1f\n", e0 - atof(argv[4]));
  fflush(meta);

  fprintf(sps,
          "epoch,sec,pm1,pm25,pm4,pm10,nc05,nc1,nc25,nc4,nc10,tsize,pm10_pm25,pm1_pm25,dev_status,status,fan_cmd\n");
  fprintf(fanf, "epoch,sec,dir,line\n");
  fprintf(evf, "epoch,sec,event\n");
  fflush(sps);
  fflush(fanf);
  fflush(evf);

  pid_t mlx_pid = spawn_mlx(dir);
  int mlx_header_done = 0, mlx_delta_col = -1;
  LineTail mlx_t = {-1, {0}, 0}, fan_t = {fan_fd, {0}, 0}, ev_t = {ev_fd, {0}, 0};
  float mlx_metric = -1;

  const uint8_t float_fmt[2] = {0x03, 0x00};
  if (!sps_cmd(0x0010, float_fmt)) fprintf(stderr, "SPS30 측정 시작 실패 — 3 s 뒤 다시 시도\n");
  double last_new = t0, last_start = t0, last_fan_tx = -10;
  unsigned rows = 0, sat = 0, crc_bad = 0, restarts = 0, acks = 0, failsafe = 0;
  double base_sum = 0;
  unsigned base_n = 0;
  double base = -1;
  int vent = 0;
  double in_band_since = -1;
  float last_pm25 = 0;

  printf("기록 → %s  (팬 %d단, 최대 %.0f s, 기준 = 처음 %.0f s 평균)\n", dir, fan, max_s, BASE_S);
  printf("이벤트: 다른 창에서  ssh root@$PI4 '/data/mlx/ev 놓음'   팬 바꾸기: ev fan 3   환기 시작: ev 환기\n");
  fflush(stdout);

  while (!g_stop) {
    const double now = mono_s();
    const double sec = now - t0;
    if (sec >= max_s) break;

    // 팬: 1 s마다 같은 명령
    if (now - last_fan_tx >= 1.0) {
      char cmd[16];
      int n = snprintf(cmd, sizeof cmd, "FAN %d\n", fan);
      if (write(fan_fd, cmd, n) == n) fprintf(fanf, "%.3f,%.1f,tx,FAN %d\n", epoch_now(), sec, fan);
      else fprintf(fanf, "%.3f,%.1f,tx_fail,FAN %d\n", epoch_now(), sec, fan);
      last_fan_tx = now;
      fflush(fanf);
    }
    char line[512];
    while (tail_line(&fan_t, line, sizeof line)) {
      if (strncmp(line, "ACK", 3) == 0) ++acks;
      if (strstr(line, "FAILSAFE")) ++failsafe;
      fprintf(fanf, "%.3f,%.1f,rx,%s\n", epoch_now(), sec, line);
      fflush(fanf);
    }

    // 이벤트
    while (tail_line(&ev_t, line, sizeof line)) {
      if (!line[0]) continue;
      fprintf(evf, "%.3f,%.1f,%s\n", epoch_now(), sec, line);
      fflush(evf);
      int nf = parse_fan_event(line);
      if (nf >= 0) fan = nf, last_fan_tx = -10;  // 바로 새 명령을 보낸다
      if (strncmp(line, "환기", strlen("환기")) == 0) vent = 1, in_band_since = -1;
      printf("  ── 이벤트 %5.0f s: %s\n", sec, line);
      fflush(stdout);
    }

    // MLX: mlx_stream이 쓰는 CSV를 따라 읽어 epoch를 붙인다
    if (mlx_t.fd < 0) {
      snprintf(path, sizeof path, "%s/mlx_raw.csv", dir);
      mlx_t.fd = open(path, O_RDONLY | O_NONBLOCK);
    }
    while (tail_line(&mlx_t, line, sizeof line)) {
      if (!mlx_header_done) {
        fprintf(mlx, "epoch,%s\n", line);
        mlx_header_done = 1;
        mlx_delta_col = csv_col(line, "delta");
      } else {
        fprintf(mlx, "%.3f,%s\n", epoch_now(), line);
        // 화면 표시용 delta (배포본은 p95 − median — Core T_metric 아님). 열 위치는 헤더에서 찾는다
        const char* f = csv_field(line, mlx_delta_col);
        if (f) mlx_metric = strtof(f, NULL);
      }
      fflush(mlx);
    }

    // SPS30
    if (now - last_new >= 3.0 && now - last_start >= 3.0) {
      last_start = now;
      ++restarts;
      fprintf(sps, "%.3f,%.1f,,,,,,,,,,,,,,restart,%d\n", epoch_now(), sec, fan);
      fflush(sps);
      sps_cmd(0x0010, float_fmt);
    }
    uint8_t d[40];
    int crc_ok = 0;
    if (sps_read(0x0202, d, 1, 5, &crc_ok) && crc_ok && d[1] == 0x01 && sps_read(0x0300, d, 20, 20, &crc_ok)) {
      const double ep = epoch_now();
      last_new = now;
      float v[10];
      for (int i = 0; i < 10; ++i) v[i] = be_float(d + i * 4);
      uint8_t s[4];
      int st_ok = 0;
      char dev[16] = "";
      if (sps_read(0xD206, s, 2, 5, &st_ok) && st_ok)
        snprintf(dev, sizeof dev, "0x%02X%02X%02X%02X", s[0], s[1], s[2], s[3]);
      const char* status = !crc_ok ? "crc_bad" : (v[1] > 2000.0f ? "sat" : "ok");
      if (!crc_ok) ++crc_bad;
      if (crc_ok && v[1] > 2000.0f) ++sat;
      fprintf(sps, "%.3f,%.1f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%.3f,%.3f,%s,%s,%d\n", ep, sec, v[0],
              v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[3] / v[1], v[0] / v[1], dev, status, fan);
      fflush(sps);
      ++rows;
      last_pm25 = v[1];

      if (sec < BASE_S && crc_ok) base_sum += v[1], ++base_n;
      if (base < 0 && sec >= BASE_S && base_n > 0) {
        base = base_sum / base_n;
        printf("  ── 기준값 PM2.5 %.2f (%u개 평균)\n", base, base_n);
        fprintf(meta, "baseline_pm25=%.2f\nbaseline_n=%u\n", base, base_n);
        fflush(meta);
      }
      char band[48] = "";
      if (base >= 0) {
        if (v[1] <= base + 5) {
          if (in_band_since < 0) in_band_since = now;
          snprintf(band, sizeof band, "기준+5 안 %2.0f s", now - in_band_since);
        } else {
          in_band_since = -1;
        }
      }
      printf("%5.0f s  PM2.5 %9.1f  tsize %.3f  PM10/2.5 %.3f  팬 %d  MLX Δ %5.1f  %s %s %s\n", sec, v[1], v[9],
             v[3] / v[1], fan, mlx_metric, status, band, vent ? "[환기]" : "");
      fflush(stdout);
      if (vent && in_band_since >= 0 && now - in_band_since >= 30.0) {
        printf("  ── 환기 끝: 기준+5 안에 30 s 머묾\n");
        fprintf(evf, "%.3f,%.1f,auto_end_baseline+5_30s\n", epoch_now(), sec);
        fflush(evf);
        break;
      }
    }

    if (mlx_pid > 0 && waitpid(mlx_pid, NULL, WNOHANG) == mlx_pid) {
      printf("  !! mlx_stream이 끝났음 — %s/mlx_stream.out 확인. SPS30·팬 기록은 계속\n", dir);
      fprintf(evf, "%.3f,%.1f,mlx_stream_exited\n", epoch_now(), sec);
      fflush(evf);
      mlx_pid = -1;
    }
    usleep(100 * 1000);
  }

  sps_cmd(0x0104, NULL);
  if (mlx_pid > 0) {
    kill(mlx_pid, SIGINT);
    for (int i = 0; i < 30 && waitpid(mlx_pid, NULL, WNOHANG) == 0; ++i) usleep(100 * 1000);
    kill(mlx_pid, SIGKILL);
    waitpid(mlx_pid, NULL, 0);
  }
  // mlx_stream이 마지막에 쓴 줄까지
  char line[512];
  while (tail_line(&mlx_t, line, sizeof line)) fprintf(mlx, "%.3f,%s\n", epoch_now(), line);
  const double end_s = mono_s() - t0;
  fprintf(meta, "end_sec=%.1f\nsps_rows=%u\nsat=%u\ncrc_bad=%u\nrestart=%u\nfan_ack=%u\nfan_failsafe_lines=%u\n", end_s,
          rows, sat, crc_bad, restarts, acks, failsafe);
  FILE* fs[] = {sps, fanf, mlx, evf, meta};
  for (unsigned i = 0; i < sizeof fs / sizeof fs[0]; ++i) {
    fflush(fs[i]);
    fsync(fileno(fs[i]));
    fclose(fs[i]);
  }
  close(fan_fd);
  close(g_i2c);
  printf("끝 %.0f s: SPS30 %u줄 (포화 %u, crc_bad %u, 재시작 %u), 팬 ACK %u, FAILSAFE %u, 마지막 PM2.5 %.1f → %s\n", end_s,
         rows, sat, crc_bad, restarts, acks, failsafe, last_pm25, dir);
  printf("팬 명령이 멈췄으므로 3 s 뒤 팬 3단(fail-safe)이 됩니다.\n");
  return 0;
}
