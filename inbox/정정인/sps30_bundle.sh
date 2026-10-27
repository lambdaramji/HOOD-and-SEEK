#!/bin/sh
# ============================================================
#  SPS30 연기/수증기 판별 + CSV 로깅 — self-extracting 번들 (v3)
#  마스터 PC 의 ~/mlx 에서:  sh sps30_bundle.sh
# ============================================================
set -e
echo "파일 생성 중..."

cat > i2c_probe.cpp <<'___EOF_I2C_PROBE_CPP___'
// i2c_probe.cpp — i2c-tools 없는 환경용 i2cdetect 대체 + 버스 상태 진단
//
// 단순 스캔에 더해 errno 를 분류한다.
//   EREMOTEIO(121) = 그 주소에 아무도 없음 (NAK)
//   EIO(5)         = 버스 오류로 분류되지만, 커널/드라이버에 따라
//                    "장치 없음"도 이걸로 보고한다. ATLAS 커널이 그렇다.
// => EE 가 많다는 것만으로 버스가 죽었다고 판단하면 안 된다.
//    하나라도 응답하면 버스는 살아 있는 것이다.
//
// 빌드 : g++ -O2 -o i2c_probe i2c_probe.cpp
// 실행 : ./i2c_probe                      전체 스캔
//        ./i2c_probe /dev/i2c-0           다른 버스
//        ./i2c_probe stress 0x69 200      한 주소를 200회 두드려 안정성 측정
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#ifndef I2C_SLAVE
#define I2C_SLAVE 0x0703
#endif

// 한 주소를 반복 접근해서 접촉/전원 안정성을 측정한다.
// "가끔 잡히고 가끔 안 잡히는" 상황을 수치로 확인하기 위한 것.
static int stress(const char* dev, int addr, int n) {
    int fd = open(dev, O_RDWR);
    if (fd < 0) { printf("[X] %s 열기 실패 (%s)\n", dev, strerror(errno)); return 1; }
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        printf("[X] 주소 설정 실패 (%s)\n", strerror(errno)); close(fd); return 1;
    }
    printf("[안정성 테스트] 0x%02X 를 %d회 반복 접근\n\n", addr, n);

    int ok = 0, eio = 0, erem = 0, oth = 0;
    int run_ok = 0, worst_fail_run = 0, cur_fail = 0;
    for (int i = 0; i < n; i++) {
        uint8_t b; errno = 0;
        if (read(fd, &b, 1) >= 0) {
            ok++; run_ok++;
            if (cur_fail > worst_fail_run) worst_fail_run = cur_fail;
            cur_fail = 0;
        } else {
            cur_fail++;
            if (errno == EIO) eio++;
            else if (errno == EREMOTEIO) erem++;
            else oth++;
        }
        if ((i + 1) % 50 == 0) {
            printf("  %3d회 시점: 성공 %d / 실패 %d\n", i + 1, ok, i + 1 - ok);
        }
        usleep(20000);   // 20ms 간격
    }
    if (cur_fail > worst_fail_run) worst_fail_run = cur_fail;
    close(fd);

    double rate = 100.0 * ok / n;
    printf("\n---------------------------------------------------------\n");
    printf("  성공 %d / %d  (%.1f%%)\n", ok, n, rate);
    printf("  실패 내역: EIO %d, EREMOTEIO %d, 기타 %d\n", eio, erem, oth);
    printf("  연속 실패 최대 %d회\n", worst_fail_run);
    printf("---------------------------------------------------------\n\n");

    if (rate >= 99.5)
        printf("접촉/전원 안정. 통신 계층 문제로 좁혀집니다.\n");
    else if (rate <= 0.5)
        printf("완전 무응답. 장치가 버스에서 떨어져 있습니다.\n");
    else {
        printf("### 간헐적입니다 — 접촉 불량 또는 전원 부족 ###\n\n");
        printf("  1. 점퍼선을 다시 꽂고 브레드보드 접촉을 확인하세요\n");
        printf("  2. SPS30 VDD 가 5V(Pin 2)인지 확인 — 3.3V 면 브라운아웃이 납니다\n");
        printf("  3. 케이블을 짧고 굵은 것으로 (SPS30 은 팬 때문에 60~80mA 소모)\n");
        printf("  4. GND 를 센서와 Pi 가 확실히 공유하는지 확인\n");
    }
    return 0;
}

int main(int argc, char** argv) {
    // ./i2c_probe stress 0x69 [횟수] [디바이스]
    if (argc > 1 && strcmp(argv[1], "stress") == 0) {
        int addr = (argc > 2) ? (int)strtol(argv[2], NULL, 0) : 0x69;
        int n    = (argc > 3) ? atoi(argv[3]) : 200;
        const char* d = (argc > 4) ? argv[4] : "/dev/i2c-1";
        return stress(d, addr, n);
    }

    const char* dev = (argc > 1) ? argv[1] : "/dev/i2c-1";

    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        printf("[X] %s 열기 실패 (%s)\n", dev, strerror(errno));
        printf("    - 커널에 i2c-dev 모듈이 있는지: ls /dev/i2c*\n");
        printf("    - 없다면 config.txt 에 dtparam=i2c_arm=on 후 재부팅\n");
        return 1;
    }
    printf("[OK] %s 열림\n\n", dev);

    int n_found = 0, n_absent = 0, n_buserr = 0, n_other = 0;
    int found[8]; int nf = 0;

    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    for (int base = 0; base < 128; base += 16) {
        printf("%02x: ", base);
        for (int i = 0; i < 16; i++) {
            int addr = base + i;
            if (addr < 0x03 || addr > 0x77) { printf("   "); continue; }
            if (ioctl(fd, I2C_SLAVE, addr) < 0) { printf(" -- "); continue; }

            uint8_t b;
            errno = 0;
            if (read(fd, &b, 1) >= 0) {
                printf("%02x ", addr);
                n_found++;
                if (nf < 8) found[nf++] = addr;
            } else if (errno == EREMOTEIO) {
                printf("-- ");            // 정상적인 "장치 없음"
                n_absent++;
            } else if (errno == EIO) {
                printf("EE ");            // 버스 오류
                n_buserr++;
            } else {
                printf("?? ");
                n_other++;
            }
        }
        printf("\n");
    }
    close(fd);

    printf("\n---------------------------------------------------------\n");
    printf("응답한 주소 : %d개", n_found);
    for (int i = 0; i < nf; i++) printf("  0x%02X", found[i]);
    printf("\n");
    printf("없음(정상)  : %d개   버스오류(EE) : %d개   기타 : %d개\n",
           n_absent, n_buserr, n_other);
    printf("---------------------------------------------------------\n\n");

    // ---- 진단 ----
    // 주의: 커널/드라이버에 따라 "장치 없음"을 EREMOTEIO 가 아니라 EIO 로 보고한다.
    //       따라서 EE 가 많다는 것만으로 버스가 죽었다고 볼 수 없다.
    //       하나라도 응답했다면 버스는 살아 있는 것이다.
    if (n_found > 0 && n_buserr > 100) {
        printf("버스 정상입니다. (EE 가 많은 것은 이 커널이 '장치 없음'을\n");
        printf("EREMOTEIO 대신 EIO 로 보고하기 때문이며, 문제가 아닙니다.)\n\n");
        printf("감지된 장치:\n");
        for (int i = 0; i < nf; i++) {
            printf("  0x%02X", found[i]);
            if (found[i] == 0x33) printf("  MLX90640 (열화상)");
            if (found[i] == 0x69) printf("  SPS30 (미세먼지)");
            printf("\n");
        }
        printf("\n장치가 잡히는데도 명령이 실패한다면 접촉/전원이 불안정한 것입니다.\n");
        printf("안정성을 측정하세요:  ./i2c_probe stress 0x69\n");
    } else if (n_found == 0 && n_buserr > 100) {
        printf("### 응답하는 장치가 없습니다 ###\n\n");
        printf("  1. 전원(VCC/VDD), GND, SDA, SCL 네 가닥 모두 확인\n");
        printf("  2. MLX90640 은 3.3V(Pin 1), SPS30 은 5V(Pin 2) 로 서로 다름\n");
        printf("  3. SPS30 은 SEL 핀을 GND 에 연결한 상태로 전원을 넣어야 I2C 모드\n");
        printf("  4. SDA(Pin 3)/SCL(Pin 5) 이 옆칸 GND(Pin 6)에 잘못 꽂혔는지 확인\n");
    } else if (n_found == 0) {
        printf("장치가 하나도 없습니다 (버스 자체는 정상).\n");
        printf("  - 전원(VCC/VDD), GND, SDA, SCL 네 가닥 모두 확인\n");
        printf("  - MLX90640 은 3.3V(Pin 1), SPS30 은 5V(Pin 2) 로 서로 다름\n");
        printf("  - SPS30 은 SEL 핀을 GND 에 연결한 상태로 전원을 넣어야 I2C 모드\n");
    } else {
        printf("버스 정상. 감지된 장치:\n");
        for (int i = 0; i < nf; i++) {
            printf("  0x%02X", found[i]);
            if (found[i] == 0x33) printf("  MLX90640 (열화상)");
            if (found[i] == 0x69) printf("  SPS30 (미세먼지)");
            printf("\n");
        }
    }
    return n_found > 0 ? 0 : 2;
}
___EOF_I2C_PROBE_CPP___

cat > sps30_test.cpp <<'___EOF_SPS30_TEST_CPP___'
// sps30_test.cpp — SPS30 미세먼지/연기 센서 테스트 (외부 라이브러리 0)
//
// Sensirion SPS30 의 I2C 프로토콜을 직접 구현했다.
// Pi(ATLAS Platform)에 Python 도 라이브러리도 못 깔기 때문.
//
// 빌드 : xbuild.sh 가 자동으로 처리 (aarch64 정적 링크)
// 실행 :
//   ./sps30_test                          진단 + 15회 측정 (기본)
//   ./sps30_test live                     연기 감지 실시간 모니터
//   ./sps30_test log <파일> <초>          CSV 로깅 (임계값 튜닝용)
//   ./sps30_test clean                    팬 청소 (10초)
//   ./sps30_test info                     시리얼/펌웨어/상태만
//
// ------------------------------------------------------------------
// 연기 vs 일반 먼지 구분 원리
//   연기 입자는 0.3~1um 로 매우 작다. 그래서 연기가 나면
//     - PC0.5(0.5um 이하 개수)가 급격히 증가하고
//     - typical particle size 는 작게(<0.6um) 유지된다
//   반면 흙먼지/꽃가루는 입자가 커서 typical size 가 1um 이상으로 올라간다.
//   => "PM2.5 상승 + 작은 입자크기" 조합일 때 연기로 판정한다.
// ------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <csignal>
#include <ctime>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#ifndef I2C_SLAVE
#define I2C_SLAVE 0x0703
#endif

#define SPS30_ADDR 0x69

// ---- 커맨드 ----
#define CMD_START_MEAS      0x0010
#define CMD_STOP_MEAS       0x0104
#define CMD_DATA_READY      0x0202
#define CMD_READ_MEAS       0x0300
#define CMD_SLEEP           0x1001
#define CMD_WAKEUP          0x1103
#define CMD_FAN_CLEAN       0x5607
#define CMD_PRODUCT_TYPE    0xD002
#define CMD_SERIAL          0xD033
#define CMD_FW_VERSION      0xD100
#define CMD_STATUS_REG      0xD206
#define CMD_RESET           0xD304

static int  g_fd = -1;
static volatile bool running = true;
static void on_sigint(int) { running = false; }

static double now_ms() {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// ------------------------------------------------------------- CRC-8
// 다항식 0x31, 초기값 0xFF, 최종 XOR 없음 (Sensirion 표준)
static uint8_t crc8(const uint8_t* d, int n) {
    uint8_t c = 0xFF;
    for (int i = 0; i < n; i++) {
        c ^= d[i];
        for (int b = 0; b < 8; b++)
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
    }
    return c;
}

// ------------------------------------------------------------- I2C 기본
static bool i2c_open(const char* dev) {
    g_fd = open(dev, O_RDWR);
    if (g_fd < 0) {
        printf("[X] %s 열기 실패: %s\n", dev, strerror(errno));
        return false;
    }
    if (ioctl(g_fd, I2C_SLAVE, SPS30_ADDR) < 0) {
        printf("[X] 주소 0x%02X 설정 실패: %s\n", SPS30_ADDR, strerror(errno));
        return false;
    }
    return true;
}

// 커맨드만 전송
static bool cmd(uint16_t c) {
    uint8_t b[2] = { (uint8_t)(c >> 8), (uint8_t)(c & 0xFF) };
    return write(g_fd, b, 2) == 2;
}

// 커맨드 + 인자 1워드 (데이터 2바이트 + CRC)
static bool cmd_arg(uint16_t c, uint16_t arg) {
    uint8_t b[5];
    b[0] = (uint8_t)(c >> 8);
    b[1] = (uint8_t)(c & 0xFF);
    b[2] = (uint8_t)(arg >> 8);
    b[3] = (uint8_t)(arg & 0xFF);
    b[4] = crc8(&b[2], 2);
    return write(g_fd, b, 5) == 5;
}

// 커맨드 전송 후 nwords 워드 읽기. CRC 전부 검증하고 데이터만 out 에 담는다.
// 반환: 성공 여부. SPS30 은 write 후 실행시간이 필요하므로 delay_ms 를 준다.
static bool rd(uint16_t c, uint8_t* out, int nwords, int delay_ms) {
    if (!cmd(c)) return false;
    usleep(delay_ms * 1000);

    int nb = nwords * 3;
    uint8_t buf[128];
    if (nb > (int)sizeof(buf)) return false;
    if (read(g_fd, buf, nb) != nb) return false;

    for (int i = 0; i < nwords; i++) {
        if (crc8(&buf[i * 3], 2) != buf[i * 3 + 2]) {
            printf("[!] CRC 불일치 (word %d) — I2C 노이즈/배선 확인\n", i);
            return false;
        }
        out[i * 2]     = buf[i * 3];
        out[i * 2 + 1] = buf[i * 3 + 1];
    }
    return true;
}

// 빅엔디안 4바이트 -> float
static float be_float(const uint8_t* p) {
    uint32_t u = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                 ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
    float f; memcpy(&f, &u, 4); return f;
}

// ------------------------------------------------------------- 측정값
struct Meas {
    float pm1, pm25, pm4, pm10;          // 질량농도 ug/m3
    float pc05, pc1, pc25, pc4, pc10;    // 개수농도 #/cm3 (누적)
    float tsize;                         // 평균 입자 크기 um
    bool  ok;
};

static Meas read_meas() {
    Meas m; m.ok = false;
    uint8_t d[40];
    // float 포맷은 20워드(40바이트). 실행시간 확보를 위해 20ms.
    if (!rd(CMD_READ_MEAS, d, 20, 20)) return m;
    m.pm1   = be_float(d +  0);
    m.pm25  = be_float(d +  4);
    m.pm4   = be_float(d +  8);
    m.pm10  = be_float(d + 12);
    m.pc05  = be_float(d + 16);
    m.pc1   = be_float(d + 20);
    m.pc25  = be_float(d + 24);
    m.pc4   = be_float(d + 28);
    m.pc10  = be_float(d + 32);
    m.tsize = be_float(d + 36);
    // 값 타당성 검사 (통신이 깨지면 NaN 이나 말도 안 되는 값이 나온다)
    if (!std::isfinite(m.pm25) || m.pm25 < 0 || m.pm25 > 2000) return m;
    m.ok = true;
    return m;
}

static bool data_ready() {
    uint8_t d[2];
    if (!rd(CMD_DATA_READY, d, 1, 5)) return false;
    return d[1] == 0x01;
}

// 최대 timeout_ms 동안 새 측정값을 기다린다 (SPS30 은 1초 주기)
static Meas wait_meas(int timeout_ms = 3000) {
    double end = now_ms() + timeout_ms;
    while (running && now_ms() < end) {
        if (data_ready()) return read_meas();
        usleep(100000);
    }
    Meas m; m.ok = false; return m;
}

// ------------------------------------------------------------- 장치 정보
static void print_info() {
    uint8_t buf[64];

    if (rd(CMD_PRODUCT_TYPE, buf, 4, 5)) {
        buf[8] = 0;
        printf("  제품 코드    : %s\n", (char*)buf);
    }
    if (rd(CMD_SERIAL, buf, 16, 5)) {
        buf[32] = 0;
        printf("  시리얼       : %s\n", (char*)buf);
    }
    if (rd(CMD_FW_VERSION, buf, 1, 5)) {
        printf("  펌웨어       : %d.%d\n", buf[0], buf[1]);
    }
    // 상태 레지스터는 32비트 = 2워드(데이터 4바이트 + CRC 2바이트 = 6바이트).
    // 3워드로 요청하면 없는 워드까지 읽어서 CRC 불일치가 난다.
    if (rd(CMD_STATUS_REG, buf, 2, 5)) {
        uint32_t st = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                      ((uint32_t)buf[2] << 8)  |  (uint32_t)buf[3];
        printf("  상태 레지스터: 0x%08X%s\n", st, st ? "" : "  (이상 없음)");
        if (st & (1u << 21)) printf("    [!] 팬 속도 범위 이탈 — 팬이 막혔거나 고장\n");
        if (st & (1u <<  5)) printf("    [!] 레이저 고장\n");
        if (st & (1u <<  4)) printf("    [!] 팬 고장 — 팬이 멈췄거나 이물질\n");
    }
}

// ------------------------------------------------------------- 출력 도우미
static void bar(float v, float full, int width) {
    int n = (int)(v / full * width + 0.5f);
    if (n > width) n = width;
    if (n < 0) n = 0;
    putchar('[');
    for (int i = 0; i < width; i++) putchar(i < n ? '#' : ' ');
    putchar(']');
}

// 실내 공기질 등급 (WHO/환경부 PM2.5 기준 참고)
static const char* aq_grade(float pm25) {
    if (pm25 <= 15)  return "좋음";
    if (pm25 <= 35)  return "보통";
    if (pm25 <= 75)  return "나쁨";
    return "매우나쁨";
}

// ---- 연기 vs 수증기 판별 지표 --------------------------------------
// 인센스 연기 : 입자 0.2~0.5um. 거의 전부 PC0.5 에 잡히고 PM10 이 PM2.5 와 비슷.
// 가습기 미스트: 물방울 1~5um. 입자크기가 커지고 PM10 이 PM2.5 보다 뚜렷이 큼.
// 세 지표를 같이 봐야 확실하게 갈린다.
struct Feat {
    float rise;        // 평상시 대비 PM2.5 상승량
    float ratio;       // 평상시 대비 배수
    float coarse;      // PM10 / PM2.5   1.0 에 가까우면 미세입자만 (연기)
    float fine_frac;   // PC0.5 / PC10   1.0 에 가까우면 미세입자 우세 (연기)
    float tsize;       // 평균 입자크기
};

static Feat features(const Meas& m, float base) {
    Feat f;
    f.rise      = m.pm25 - base;
    f.ratio     = (base > 1.0f) ? (m.pm25 / base) : m.pm25;
    f.coarse    = (m.pm25 > 0.5f) ? (m.pm10 / m.pm25) : 1.0f;
    f.fine_frac = (m.pc10 > 1.0f) ? (m.pc05 / m.pc10) : 1.0f;
    f.tsize     = m.tsize;
    return f;
}

// ---- 임계값 (실측 후 이 값들만 조정하면 된다) ----
static float TH_RATIO   = 2.5f;    // 평상시 대비 몇 배부터 "이상"으로 볼지
static float TH_RISE    = 10.0f;   // 최소 상승량 ug/m3
static float TH_TSIZE   = 0.60f;   // 이보다 작으면 연기 쪽
static float TH_COARSE  = 1.20f;   // 이보다 크면 굵은 입자 존재 = 수증기/먼지
static float TH_FINE    = 0.90f;   // PC0.5 비율이 이보다 높으면 연기 쪽

enum Verdict { V_NORMAL = 0, V_SMOKE = 1, V_MIST = 2 };

// 반환: 판정. lvl 에 심각도(0 정상 / 1 주의 / 2 경고)를 담는다.
static Verdict classify(const Feat& f, int* lvl, const char** why) {
    // 농도 변화 자체가 없으면 정상
    if (!(f.ratio > TH_RATIO && f.rise > TH_RISE)) {
        *lvl = 0; *why = "평상시 수준";
        return V_NORMAL;
    }
    // 농도는 올랐다. 그럼 무엇 때문인가?
    bool looks_smoke = (f.tsize < TH_TSIZE) &&
                       (f.coarse < TH_COARSE) &&
                       (f.fine_frac > TH_FINE);
    if (looks_smoke) {
        *lvl = (f.ratio > 5.0f || f.rise > 60.0f) ? 2 : 1;
        *why = "미세입자 우세 — 연소 연기 특성";
        return V_SMOKE;
    }
    *lvl = 0;
    *why = "굵은 입자 — 수증기/먼지로 판단, 경보 안 함";
    return V_MIST;
}

// ------------------------------------------------------------- 측정 시작/정지
static bool start_measurement() {
    // 0x0300 = float 출력 포맷, 하위 바이트는 더미(0x00)
    if (!cmd_arg(CMD_START_MEAS, 0x0300)) {
        printf("[X] 측정 시작 명령 실패\n");
        return false;
    }
    usleep(30000);
    return true;
}

static void stop_measurement() {
    cmd(CMD_STOP_MEAS);
    usleep(30000);
}

// ------------------------------------------------------------- 모드 1: 진단
static void run_diag() {
    printf("\n[2] 측정 시작 (팬 가동)\n");
    if (!start_measurement()) return;

    // 데이터시트상 8초 후부터 유효, 30초 후 안정. 테스트는 12초로 절충.
    printf("[3] 팬 안정화 대기 — 12초 (정확한 값은 30초 이상 필요)\n     ");
    for (int i = 12; i > 0 && running; i--) {
        printf("%d ", i); fflush(stdout);
        sleep(1);
    }
    printf("\n\n[4] 측정 15회\n");
    printf("-------------------------------------------------------------------------\n");
    printf("  #   PM1.0   PM2.5   PM4.0   PM10    PC0.5    입자크기   등급\n");
    printf("-------------------------------------------------------------------------\n");

    int okc = 0;
    float sum25 = 0;
    for (int i = 0; i < 15 && running; i++) {
        Meas m = wait_meas();
        if (!m.ok) { printf("  %-3d  측정 실패 (데이터 미도착 또는 CRC 오류)\n", i + 1); continue; }
        okc++; sum25 += m.pm25;
        printf("  %-3d %6.1f  %6.1f  %6.1f  %6.1f  %8.1f   %5.2fum   %s\n",
               i + 1, m.pm1, m.pm25, m.pm4, m.pm10, m.pc05, m.tsize, aq_grade(m.pm25));
    }
    printf("-------------------------------------------------------------------------\n");

    stop_measurement();

    printf("\n[결과]\n");
    printf("  성공 %d/15회", okc);
    if (okc) printf(", 평균 PM2.5 %.1f ug/m3 (%s)", sum25 / okc, aq_grade(sum25 / okc));
    printf("\n\n");
    printf("  판정 기준\n");
    printf("   - 실내 PM2.5 가 0~35 ug/m3 범위면 정상\n");
    printf("   - 15회 모두 성공하고 값이 급변하지 않으면 통신 안정\n");
    printf("   - 전부 0.0 이면: 팬 안 돌거나 흡입구 막힘 (info 로 상태 레지스터 확인)\n");
    printf("   - 측정 실패 반복: 배선/전원(5V) 또는 SEL 핀 접지 확인\n");
    printf("\n  연기 테스트는 live 모드가 편합니다:  ./sps30_test live\n");
}

// ------------------------------------------------------------- 모드 2: 실시간
static void run_live() {
    printf("\n측정 시작 (Ctrl+C 종료)\n");
    if (!start_measurement()) return;

    printf("팬 안정화 12초...\n");
    for (int i = 0; i < 12 && running; i++) sleep(1);

    // 베이스라인 학습: 처음 15회 평균을 평상시 값으로 삼는다
    const int BASE_N = 15;
    float base = 0.0f;
    int   bn = 0;
    float peak = 0.0f;

    while (running) {
        Meas m = wait_meas();
        if (!m.ok) { printf("측정 실패\n"); continue; }

        if (bn < BASE_N) {
            base = (base * bn + m.pm25) / (bn + 1);
            bn++;
        }
        if (m.pm25 > peak) peak = m.pm25;

        Feat f = features(m, base);
        int lv = 0; const char* why = "-";
        Verdict v = V_NORMAL;
        if (bn >= BASE_N) v = classify(f, &lv, &why);

        printf("\033[H\033[J");
        printf("=========================================================\n");
        printf(" SPS30  연기 감지 모니터\n");
        printf("=========================================================\n\n");

        printf("  PM1.0   %7.1f ug/m3  ", m.pm1);  bar(m.pm1,  100, 22); printf("\n");
        printf("  PM2.5   %7.1f ug/m3  ", m.pm25); bar(m.pm25, 100, 22);
        printf("  %s\n", aq_grade(m.pm25));
        printf("  PM10    %7.1f ug/m3  ", m.pm10); bar(m.pm10, 100, 22); printf("\n\n");

        printf("  개수농도  PC0.5 %8.1f   PC2.5 %8.1f   PC10 %8.1f  (#/cm3)\n",
               m.pc05, m.pc25, m.pc10);
        printf("\n");

        // --- 판별 지표를 그대로 보여준다 (시연/튜닝용) ---
        printf("  ---- 판별 지표 -------------------------------------\n");
        printf("   입자크기   %5.2f um    (< %.2f 이면 연기 쪽)   %s\n",
               f.tsize, TH_TSIZE, f.tsize < TH_TSIZE ? "O" : "X");
        printf("   PM10/PM2.5 %5.2f       (< %.2f 이면 연기 쪽)   %s\n",
               f.coarse, TH_COARSE, f.coarse < TH_COARSE ? "O" : "X");
        printf("   PC0.5/PC10 %5.2f       (> %.2f 이면 연기 쪽)   %s\n",
               f.fine_frac, TH_FINE, f.fine_frac > TH_FINE ? "O" : "X");
        printf("   상승 배수  %5.2f x  (+%.1f ug/m3)\n", f.ratio, f.rise);
        printf("  ----------------------------------------------------\n\n");

        if (bn < BASE_N) {
            printf("  [베이스라인 학습중 %d/%d] 깨끗한 공기를 유지하세요\n", bn, BASE_N);
        } else {
            printf("  평상시 PM2.5 %.1f   최고 %.1f\n\n", base, peak);
            if (v == V_SMOKE && lv == 2)
                printf("   ####  연 기  경 고  ####   %s\n", why);
            else if (v == V_SMOKE)
                printf("   !!!!  연기 주의  !!!!      %s\n", why);
            else if (v == V_MIST)
                printf("   [수증기/먼지]  농도는 올랐지만 경보 안 함\n   %s\n", why);
            else
                printf("   정상   %s\n", why);
        }
        printf("\n  Ctrl+C 종료\n");
        fflush(stdout);
    }
    stop_measurement();
    printf("\n종료 (팬 정지)\n");
}

// ------------------------------------------------------------- 모드 3: CSV 로깅
// 임계값 튜닝용. 인센스/가습기 각각 돌려서 파일을 받아 비교하면 된다.
static void run_log(const char* path, int seconds) {
    FILE* fp = fopen(path, "w");
    if (!fp) { printf("[X] %s 열기 실패\n", path); return; }

    printf("\n측정 시작 -> %s (%d초, Ctrl+C 로 조기 종료 가능)\n", path, seconds);
    if (!start_measurement()) { fclose(fp); return; }

    printf("팬 안정화 12초...\n");
    for (int i = 0; i < 12 && running; i++) sleep(1);

    fprintf(fp, "sec,pm1,pm25,pm4,pm10,pc05,pc1,pc25,pc4,pc10,tsize,"
                "coarse_pm10_pm25,fine_pc05_pc10\n");
    fflush(fp);

    double t0 = now_ms();
    int n = 0;
    while (running) {
        double el = (now_ms() - t0) / 1000.0;
        if (seconds > 0 && el > seconds) break;

        Meas m = wait_meas();
        if (!m.ok) continue;

        float coarse = (m.pm25 > 0.5f) ? (m.pm10 / m.pm25) : 1.0f;
        float fine   = (m.pc10 > 1.0f) ? (m.pc05 / m.pc10) : 1.0f;

        fprintf(fp, "%.1f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%.3f,%.3f\n",
                el, m.pm1, m.pm25, m.pm4, m.pm10,
                m.pc05, m.pc1, m.pc25, m.pc4, m.pc10, m.tsize, coarse, fine);
        fflush(fp);
        n++;

        printf("\r  %5.0fs  PM2.5 %6.1f  크기 %.2fum  PM10/2.5 %.2f  PC0.5/10 %.2f  (%d행)   ",
               el, m.pm25, m.tsize, coarse, fine, n);
        fflush(stdout);
    }
    stop_measurement();
    fclose(fp);
    printf("\n\n%d행 기록됨 -> %s\n", n, path);
    printf("마스터 PC 로 가져오기:\n");
    printf("  scp root@<Pi4 IP>:%s .\n", path);
}

// ------------------------------------------------------------- 모드 4: 팬 청소
static void run_clean() {
    printf("\n팬 청소를 시작합니다. 측정을 먼저 켜야 동작합니다.\n");
    if (!start_measurement()) return;
    sleep(2);
    if (!cmd(CMD_FAN_CLEAN)) { printf("[X] 청소 명령 실패\n"); stop_measurement(); return; }
    printf("팬 고속 회전 중 — 10초 대기\n  ");
    for (int i = 10; i > 0 && running; i--) { printf("%d ", i); fflush(stdout); sleep(1); }
    printf("\n청소 완료\n");
    stop_measurement();
}

// ------------------------------------------------------------- main
int main(int argc, char** argv) {
    signal(SIGINT,  on_sigint);
    signal(SIGTERM, on_sigint);

    const char* mode = (argc > 1) ? argv[1] : "diag";

    printf("=====================================================\n");
    printf(" SPS30 미세먼지/연기 센서 테스트\n");
    printf("=====================================================\n");
    printf("[1] I2C 연결 (/dev/i2c-1, 주소 0x%02X)\n", SPS30_ADDR);

    if (!i2c_open("/dev/i2c-1")) return 1;

    // 슬립 상태일 수 있으므로 깨운다 (펌웨어 2.0+). 실패해도 무시.
    cmd(CMD_WAKEUP); usleep(5000);
    cmd(CMD_WAKEUP); usleep(20000);

    print_info();

    if      (strcmp(mode, "live")  == 0) run_live();
    else if (strcmp(mode, "clean") == 0) run_clean();
    else if (strcmp(mode, "info")  == 0) printf("\n(info 모드: 위 정보만 출력)\n");
    else if (strcmp(mode, "log")   == 0) {
        const char* path = (argc > 2) ? argv[2] : "/data/mlx/sps30_log.csv";
        int secs = (argc > 3) ? atoi(argv[3]) : 120;
        run_log(path, secs);
    }
    else                                 run_diag();

    close(g_fd);
    return 0;
}
___EOF_SPS30_TEST_CPP___

cat > sps30_diag.cpp <<'___EOF_SPS30_DIAG_CPP___'
// sps30_diag.cpp — SPS30 이 0x69 로 잡히는데 명령이 안 먹을 때 원인을 좁힌다.
//
// "주소는 ACK 되는데 커맨드는 실패" 상황에서 어느 단계가 깨지는지,
// errno 까지 전부 찍어서 하드웨어 문제인지 타이밍 문제인지 가른다.
//
// 실행 : ./sps30_diag
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#ifndef I2C_SLAVE
#define I2C_SLAVE 0x0703
#endif

#define ADDR 0x69

static int fd = -1;

static uint8_t crc8(const uint8_t* d, int n) {
    uint8_t c = 0xFF;
    for (int i = 0; i < n; i++) {
        c ^= d[i];
        for (int b = 0; b < 8; b++)
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
    }
    return c;
}

static const char* en() { return strerror(errno); }

static void hex(const uint8_t* p, int n) {
    for (int i = 0; i < n; i++) printf("%02X ", p[i]);
}

// ---------------------------------------------- 1. 주소 응답 확인
static void t_addr() {
    printf("\n[1] 주소 0x%02X 응답 확인\n", ADDR);
    uint8_t b;
    errno = 0;
    ssize_t r = read(fd, &b, 1);
    printf("    1바이트 read : ret=%zd  errno=%d (%s)\n", r, errno, r < 0 ? en() : "-");
    if (r == 1) printf("    -> 센서가 주소에 ACK 함. 물리적으로는 붙어 있음. 받은 값 0x%02X\n", b);
    else        printf("    -> 주소 자체가 무응답. 배선/전원 문제.\n");
}

// ---------------------------------------------- 2. 커맨드 write 단독
static void t_write(uint16_t c, const char* name) {
    uint8_t b[2] = { (uint8_t)(c >> 8), (uint8_t)(c & 0xFF) };
    errno = 0;
    ssize_t r = write(fd, b, 2);
    printf("    %-18s write(2) ret=%zd  errno=%d (%s)\n",
           name, r, errno, r < 0 ? en() : "OK");
}

// ---------------------------------------------- 3. write 후 read (지연 변화)
static void t_read(uint16_t c, const char* name, int nwords, int delay_ms) {
    uint8_t cb[2] = { (uint8_t)(c >> 8), (uint8_t)(c & 0xFF) };
    errno = 0;
    if (write(fd, cb, 2) != 2) {
        printf("    %-14s delay %3dms  write 실패 errno=%d (%s)\n",
               name, delay_ms, errno, en());
        return;
    }
    usleep(delay_ms * 1000);

    int nb = nwords * 3;
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    errno = 0;
    ssize_t r = read(fd, buf, nb);
    if (r != nb) {
        printf("    %-14s delay %3dms  read 실패 ret=%zd errno=%d (%s)\n",
               name, delay_ms, r, errno, en());
        return;
    }
    // CRC 검사
    int bad = 0;
    for (int i = 0; i < nwords; i++)
        if (crc8(&buf[i * 3], 2) != buf[i * 3 + 2]) bad++;
    printf("    %-14s delay %3dms  OK  CRC오류 %d/%d  raw: ",
           name, delay_ms, bad, nwords);
    hex(buf, nb > 12 ? 12 : nb);
    if (nb > 12) printf("...");
    printf("\n");
}

// ---------------------------------------------- 4. repeated start 방식
// write/read 를 STOP 없이 한 트랜잭션으로 묶는다. 일부 환경에서 이쪽만 되기도 한다.
static void t_rdwr(uint16_t c, const char* name, int nwords) {
    uint8_t cb[2] = { (uint8_t)(c >> 8), (uint8_t)(c & 0xFF) };
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    struct i2c_msg m[2];
    m[0].addr = ADDR; m[0].flags = 0;        m[0].len = 2;             m[0].buf = cb;
    m[1].addr = ADDR; m[1].flags = I2C_M_RD; m[1].len = nwords * 3;    m[1].buf = buf;

    struct i2c_rdwr_ioctl_data io;
    io.msgs = m; io.nmsgs = 2;

    errno = 0;
    int r = ioctl(fd, I2C_RDWR, &io);
    if (r < 0) {
        printf("    %-14s I2C_RDWR 실패 errno=%d (%s)\n", name, errno, en());
        return;
    }
    int bad = 0;
    for (int i = 0; i < nwords; i++)
        if (crc8(&buf[i * 3], 2) != buf[i * 3 + 2]) bad++;
    printf("    %-14s I2C_RDWR OK  CRC오류 %d/%d  raw: ", name, bad, nwords);
    hex(buf, nwords * 3 > 12 ? 12 : nwords * 3);
    printf("\n");
}

int main() {
    printf("=========================================================\n");
    printf(" SPS30 통신 진단 (0x%02X)\n", ADDR);
    printf("=========================================================\n");

    fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) { printf("[X] /dev/i2c-1 열기 실패: %s\n", en()); return 1; }
    if (ioctl(fd, I2C_SLAVE, ADDR) < 0) {
        printf("[X] I2C_SLAVE 설정 실패: %s\n", en());
        printf("    EBUSY 면 커널 드라이버가 이 주소를 이미 점유한 것.\n");
        return 1;
    }
    printf("[OK] /dev/i2c-1 열림, 주소 0x%02X 설정됨\n", ADDR);

    t_addr();

    printf("\n[2] 커맨드 write 만 (센서가 명령을 ACK 하는가)\n");
    t_write(0xD002, "Product Type");
    t_write(0xD033, "Serial Number");
    t_write(0xD100, "FW Version");
    t_write(0x0202, "Data Ready");
    printf("    -> errno=121(EREMOTEIO) : 센서가 NAK. 주소는 맞는데 응답을 안 함.\n");
    printf("       errno=5  (EIO)       : 버스 자체가 비정상. SDA/SCL 단락이나\n");
    printf("                              라인이 low 로 붙들린 상태. 배선부터 확인.\n");

    printf("\n[3] 소프트 리셋 후 재시도\n");
    t_write(0xD304, "Reset");
    usleep(100000);
    printf("    (100ms 대기 완료)\n");

    printf("\n[4] write + read, 지연시간을 바꿔가며\n");
    static const int DELAYS[] = { 1, 5, 20, 50, 100 };
    for (unsigned i = 0; i < sizeof(DELAYS) / sizeof(DELAYS[0]); i++)
        t_read(0xD002, "ProductType", 4, DELAYS[i]);
    printf("\n");
    for (unsigned i = 0; i < sizeof(DELAYS) / sizeof(DELAYS[0]); i++)
        t_read(0xD100, "FWVersion", 1, DELAYS[i]);

    printf("\n[5] repeated start (I2C_RDWR) 방식\n");
    t_rdwr(0xD002, "ProductType", 4);
    t_rdwr(0xD100, "FWVersion", 1);
    t_rdwr(0xD033, "Serial", 16);

    printf("\n[6] 웨이크업 후 재시도 (펌웨어 2.0+ 는 슬립 지원)\n");
    t_write(0x1103, "Wakeup(1st)");   // 슬립 상태면 1회차는 NAK 되는 것이 정상
    usleep(5000);
    t_write(0x1103, "Wakeup(2nd)");
    usleep(30000);
    t_read(0xD002, "ProductType", 4, 20);

    printf("\n=========================================================\n");
    printf(" 해석\n");
    printf("=========================================================\n");
    printf("  [1] 부터 errno=5(EIO) 로 실패\n");
    printf("      -> 센서 유무 이전에 버스가 죽은 상태. i2c_probe 를 돌려\n");
    printf("         모든 주소가 EE 로 나오는지 확인할 것. 그렇다면 배선/단락 문제.\n");
    printf("  [1] 성공 + [2] 전부 실패\n");
    printf("      -> 주소는 받지만 데이터는 못 받는 상태.\n");
    printf("         VDD 가 5V 인지 먼저 확인 (Pin 2 이지 Pin 1 아님).\n");
    printf("         3.3V 로 물리면 센서가 반쯤 켜져 ACK 만 하고 동작은 못 한다.\n");
    printf("  [4] 특정 지연부터 성공\n");
    printf("      -> 단순 타이밍 문제. sps30_test 의 delay 를 그 값으로 올리면 됨.\n");
    printf("  [5] 만 성공\n");
    printf("      -> STOP 없는 repeated start 가 필요한 환경. 그 방식으로 바꾸면 됨.\n");
    printf("  [6] 이후부터 성공\n");
    printf("      -> 센서가 슬립 상태였던 것. 시작 시 웨이크업을 더 확실히 하면 됨.\n");
    printf("  전부 실패 + CRC 오류만 잔뜩\n");
    printf("      -> 로직 레벨 문제. 레벨 시프터 필요 (SPS30 VIH = 0.7x5V = 3.5V > Pi 3.3V).\n");

    close(fd);
    return 0;
}
___EOF_SPS30_DIAG_CPP___

cat > xbuild.sh <<'___EOF_XBUILD_SH___'
#!/bin/sh
# 마스터 PC(x86_64 Ubuntu) 에서 aarch64 로 크로스 컴파일 후 Pi 로 전송.
# Pi(ATLAS Platform)는 컴파일러가 없고 rootfs 가 읽기전용이라 이 방식이 유일하다.
# make 를 쓰지 않고 컴파일러를 직접 호출한다. (마스터 PC 에 make 가 없어도 동작)
#
#   sh xbuild.sh root@<Pi4 IP>                      # -> /data/mlx (주소는 꼭 넘긴다)
#   sh xbuild.sh root@<Pi4 IP> /data/mlx
#   sh xbuild.sh root@<Pi4 IP> /tmp/mlx      # 재부팅 시 사라지는 위치
set -e

PI="${1:?사용법: sh xbuild.sh root@<Pi4 IP> [대상 폴더]}"
DEST="${2:-/data/mlx}"
CROSS=aarch64-linux-gnu-
LIB_DIR=./mlx90640-library

echo "== 1. 크로스 툴체인 확인 =="
if ! command -v ${CROSS}g++ >/dev/null 2>&1; then
    echo "  ${CROSS}g++ 없음 -> 설치합니다 (sudo 암호 필요)"
    sudo apt update
    sudo apt install -y g++-aarch64-linux-gnu
fi
echo "  $(${CROSS}g++ --version | head -1)"

echo
echo "== 2. Melexis 드라이버 라이브러리 =="
if [ -d "$LIB_DIR" ]; then
    echo "  이미 있음"
else
    command -v git >/dev/null || { echo "!! git 이 필요합니다."; exit 1; }
    git clone --depth 1 https://github.com/pimoroni/mlx90640-library.git
fi

# 포크마다 드라이버 파일명이 달라서 존재하는 것을 찾는다
DRIVER=""
for f in MLX90640_LINUX_I2C_Driver.cpp MLX90640_RPI_I2C_Driver.cpp MLX90640_I2C_Driver.cpp; do
    if [ -f "$LIB_DIR/functions/$f" ]; then DRIVER="$LIB_DIR/functions/$f"; break; fi
done
if [ -z "$DRIVER" ]; then
    echo "!! I2C 드라이버 소스를 못 찾음. 아래 목록 확인:"
    ls "$LIB_DIR/functions/"
    exit 1
fi
echo "  드라이버 : $DRIVER"

echo
echo "== 3. 크로스 컴파일 (정적 링크) =="
CXXFLAGS="-std=c++11 -O2 -Wall -I$LIB_DIR/headers"
BINS="mlx_test i2c_probe"
[ -f mlx_stream.cpp ] && BINS="$BINS mlx_stream"
[ -f sps30_test.cpp ] && BINS="$BINS sps30_test"
[ -f sps30_diag.cpp ] && BINS="$BINS sps30_diag"
rm -f $BINS

set -x
${CROSS}g++ $CXXFLAGS -o mlx_test \
    mlx_test.cpp "$LIB_DIR/functions/MLX90640_API.cpp" "$DRIVER" \
    -static -lm
${CROSS}g++ -std=c++11 -O2 -Wall -o i2c_probe i2c_probe.cpp -static
set +x
if [ -f mlx_stream.cpp ]; then
    set -x
    ${CROSS}g++ $CXXFLAGS -o mlx_stream \
        mlx_stream.cpp "$LIB_DIR/functions/MLX90640_API.cpp" "$DRIVER" \
        -static -lm
    set +x
fi
if [ -f sps30_test.cpp ]; then
    # SPS30 은 Melexis 라이브러리와 무관 (I2C 프로토콜 직접 구현)
    set -x
    ${CROSS}g++ -std=c++11 -O2 -Wall -o sps30_test sps30_test.cpp -static -lm
    set +x
fi
if [ -f sps30_diag.cpp ]; then
    set -x
    ${CROSS}g++ -std=c++11 -O2 -Wall -o sps30_diag sps30_diag.cpp -static
    set +x
fi
echo "  빌드 완료: $BINS"

echo
echo "== 4. 바이너리 검증 =="
for b in $BINS; do
    if command -v file >/dev/null 2>&1; then
        out=$(file "$b")
        echo "  $out"
        echo "$out" | grep -q "ARM aarch64" || { echo "!! aarch64 바이너리가 아님"; exit 1; }
    elif command -v ${CROSS}readelf >/dev/null 2>&1; then
        arch=$(${CROSS}readelf -h "$b" | grep Machine)
        echo "  $b : $arch"
    else
        ls -l "$b"
    fi
done

echo
echo "== 5. Pi 로 전송 : $PI:$DEST =="
ssh "$PI" "mkdir -p $DEST"
tar czf - $BINS | ssh "$PI" "tar xzf - -C $DEST"
ssh "$PI" "cd $DEST && chmod +x $BINS && ls -l"

echo
echo "== 완료 =="
echo "  ssh $PI '$DEST/i2c_probe'"
echo "  ssh $PI '$DEST/mlx_test'           # 5회 측정 + 진단"
echo "  ssh -t $PI '$DEST/mlx_test live'   # 터미널 ASCII 실시간"
if [ -f sps30_test.cpp ]; then
    echo
    echo "  [SPS30 미세먼지/연기]"
    echo "  ssh $PI '$DEST/sps30_diag'           # 통신 안될 때 원인 진단"
    echo "  ssh $PI '$DEST/sps30_test info'      # 시리얼/펌웨어/상태만"
    echo "  ssh $PI '$DEST/sps30_test'           # 진단 + 15회 측정"
    echo "  ssh -t $PI '$DEST/sps30_test live'   # 연기 감지 실시간 (-t 필수)"
    echo "  ssh $PI '$DEST/sps30_test log /data/mlx/incense.csv 120'   # CSV 로깅"
    echo "  ssh $PI '$DEST/sps30_test clean'     # 팬 청소"
fi
if [ -f mlx_stream.cpp ]; then
    echo
    echo "  [브라우저 컬러 열화상]"
    echo "  1) Pi 에서 서버 실행:"
    echo "       ssh $PI '$DEST/mlx_stream 8080 1'"
    echo "  2) 노트북에서 다른 터미널 열고 터널:"
    echo "       ssh -L 8080:<Pi4 IP>:8080 <계정>@<마스터PC-tailscale-IP>"
    echo "  3) 브라우저에서 http://localhost:8080"
fi
___EOF_XBUILD_SH___


chmod +x xbuild.sh
echo
ls -l i2c_probe.cpp sps30_test.cpp sps30_diag.cpp xbuild.sh
echo
echo "이제: sh xbuild.sh root@<Pi4 IP>"
