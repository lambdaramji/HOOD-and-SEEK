#!/bin/sh
# ============================================================
#  열화상 지표 v3 — 작은 열원 대응 + CSV 덮어쓰기 방지
#  마스터 PC 의 ~/mlx 에서:  sh mlx_bundle.sh
# ============================================================
set -e
echo "파일 생성 중..."

cat > mlx_stream.cpp <<'___EOF_MLX_STREAM_CPP___'
// mlx_stream.cpp — MLX90640 열화상을 브라우저에서 컬러 영상으로 본다.
//
// Pi 에는 라이브러리도 화면도 없으므로, 이 바이너리가 직접 아주 작은 HTTP 서버가 되어
//   GET /            -> 뷰어 HTML (바이너리에 내장)
//   GET /frame.json  -> 최신 프레임 768개 온도값
// 을 내려주고, 실제 렌더링(컬러맵/보간/확대)은 브라우저가 한다.
// 외부 의존성 0. 정적 링크 가능.
//
// 실행 : ./mlx_stream [포트] [refreshHz] [CSV파일]
//        ./mlx_stream                                  8080 포트, 1Hz
//        ./mlx_stream 8080 2                           2Hz
//        ./mlx_stream 8080 1 /data/mlx/hotpack.csv     영상 + CSV 동시 기록
//
// 두 프로세스가 동시에 I2C 를 잡으면 프레임이 깨지므로
// mlx_test 와 mlx_stream 을 같이 돌리지 말 것. 이 하나로 영상과 로그를 다 한다.
//
// 보기 : 노트북에서 SSH 터널을 뚫고 브라우저로 접속
//        ssh -L 8080:<Pi4 IP>:8080 <계정>@<마스터PC>
//        http://localhost:8080

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <csignal>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

// API 헤더가 stdint 를 include 하지 않으므로 먼저 넣는다 (라이브러리 원본 수정 안 함)
#include <stdint.h>
#include <cstdint>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

#define MLX_ADDR   0x33
#define EMISSIVITY 0.95f
#define TA_SHIFT   8.0f

static paramsMLX90640 params;
static volatile bool  running = true;
static void on_sigint(int) { running = false; }

static double now_ms() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static uint8_t rate_code(double hz) {
    if (hz <= 0.5) return 0x00;
    if (hz <= 1.0) return 0x01;
    if (hz <= 2.0) return 0x02;
    if (hz <= 4.0) return 0x03;
    if (hz <= 8.0) return 0x04;
    return 0x05;
}

// ------------------------------------------------------------ 최신 프레임 버퍼
static float  g_to[768];
static float  g_ta = 0.0f;
static long   g_seq = 0;
static int    g_drops = 0;
static double g_frame_ms = 0.0;

// ------------------------------------------------------------ 파생 지표
// 최고온도 하나만 보면 노이즈 픽셀에 흔들린다.
// 상위 5%(p95), 배경(중앙값), 고온 픽셀 개수, 상승률을 같이 본다.
// 주의: 작은 열원(핫팩 등)은 768픽셀 중 10~30개밖에 안 차지한다.
// 상위 5%(=38픽셀)를 보면 대부분이 배경이라 열원이 묻혀버린다.
// 그래서 p99(상위 8픽셀)와 hotmean(고온 픽셀들의 평균)을 같이 본다.
struct Metrics {
    float mx, mn, avg;
    float p95;       // 상위 5% — 열원이 클 때 유효
    float p99;       // 상위 1%(약 8픽셀) — 작은 열원에 적합
    float hotmean;   // median+5C 를 넘는 픽셀들의 평균 — 가장 안정적
    float median;    // 배경 온도
    float delta;     // max - median. 배경 대비 열원 온도차
    int   hot;       // median+5C 를 넘는 픽셀 수 = 열원의 크기
    float rise;      // delta 의 초당 변화량 (C/s)
};
static Metrics g_m;

static int cmp_float(const void* a, const void* b) {
    float x = *(const float*)a, y = *(const float*)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

static void compute_metrics(const float* to, double dt_s) {
    static float buf[768];
    int n = 0;
    for (int i = 0; i < 768; i++) {
        float v = to[i];
        if (std::isfinite(v) && v > -40.0f && v < 300.0f) buf[n++] = v;
    }
    if (n < 100) { g_m.mx = g_m.mn = g_m.avg = g_m.p95 = g_m.p99 =
                   g_m.hotmean = g_m.median = NAN;
                   g_m.delta = 0; g_m.hot = 0; g_m.rise = 0; return; }

    double sum = 0; for (int i = 0; i < n; i++) sum += buf[i];
    g_m.avg = (float)(sum / n);

    qsort(buf, n, sizeof(float), cmp_float);
    g_m.mn     = buf[0];
    g_m.mx     = buf[n - 1];
    g_m.median = buf[n / 2];
    g_m.p95    = buf[(int)(n * 0.95)];
    g_m.p99    = buf[(int)(n * 0.99)];

    float prev_delta = g_m.delta;
    g_m.delta = g_m.mx - g_m.median;   // p95 는 작은 열원을 놓친다. max 기준으로.

    // 고온 픽셀 평균: max 는 픽셀 하나라 노이즈에 흔들리므로 이쪽이 안정적
    float th = g_m.median + 5.0f;
    g_m.hot = 0;
    double hs = 0;
    for (int i = n - 1; i >= 0 && buf[i] > th; i--) { hs += buf[i]; g_m.hot++; }
    g_m.hotmean = (g_m.hot > 0) ? (float)(hs / g_m.hot) : g_m.median;

    g_m.rise = (dt_s > 0.1) ? (float)((g_m.delta - prev_delta) / dt_s) : 0.0f;
}

static int read_full_frame() {
    static uint16_t frame[834];
    bool got[2] = { false, false };
    g_drops = 0;
    double t0 = now_ms();

    for (int a = 0; a < 12 && !(got[0] && got[1]) && running; a++) {
        int sp = MLX90640_GetFrameData(MLX_ADDR, frame);
        if (sp < 0) { g_drops++; usleep(20000); continue; }

        float vdd = MLX90640_GetVdd(frame, &params);
        float ta  = MLX90640_GetTa(frame, &params);
        if (vdd < 2.0f || vdd > 4.5f || ta < -40.0f || ta > 125.0f) { g_drops++; continue; }

        g_ta = ta;
        MLX90640_CalculateTo(frame, &params, EMISSIVITY, ta - TA_SHIFT, g_to);
        if (sp == 0 || sp == 1) got[sp] = true;
    }
    g_frame_ms = now_ms() - t0;
    return (got[0] ? 1 : 0) + (got[1] ? 1 : 0);
}

// ------------------------------------------------------------ 내장 뷰어 페이지
static const char* PAGE = R"HTML(<!doctype html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MLX90640 Thermal</title>
<style>
  :root{--bg:#0d0f14;--panel:#161a22;--line:#262c38;--fg:#e6e9ef;--dim:#8a93a6;--acc:#ffb020}
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--fg);
       font:14px/1.5 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
  .wrap{max-width:900px;margin:0 auto;padding:18px}
  h1{font-size:15px;font-weight:600;letter-spacing:.06em;margin:0 0 14px;color:var(--dim)}
  h1 b{color:var(--fg);font-weight:600}
  .card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:14px}
  canvas{width:100%;height:auto;display:block;border-radius:6px;background:#000;
         image-rendering:auto;cursor:crosshair}
  .bar{height:14px;border-radius:3px;margin:10px 0 4px}
  .barlbl{display:flex;justify-content:space-between;color:var(--dim);font-size:12px}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:10px;margin-top:14px}
  .stat{background:#11151d;border:1px solid var(--line);border-radius:8px;padding:9px 11px}
  .stat .k{color:var(--dim);font-size:11px;letter-spacing:.05em}
  .stat .v{font-size:19px;margin-top:2px}
  .hot{color:var(--acc)}
  .ctl{display:flex;flex-wrap:wrap;gap:14px;align-items:center;margin-top:14px;
       padding-top:12px;border-top:1px solid var(--line);color:var(--dim);font-size:13px}
  label{display:flex;align-items:center;gap:6px;cursor:pointer}
  input[type=number]{width:62px;background:#11151d;border:1px solid var(--line);
       color:var(--fg);border-radius:5px;padding:3px 6px;font:inherit;font-size:12px}
  #err{color:#ff6b6b;margin-top:10px;min-height:18px;font-size:12px}
</style></head><body><div class="wrap">
<h1>MLX90640 <b>THERMAL VIEW</b> — 32&times;24</h1>

<div class="card">
  <canvas id="cv" width="640" height="480"></canvas>
  <div class="bar" id="bar"></div>
  <div class="barlbl"><span id="blo">-</span><span id="bhi">-</span></div>

  <div class="grid">
    <div class="stat"><div class="k">MAX</div><div class="v hot" id="smax">-</div></div>
    <div class="stat"><div class="k">MIN</div><div class="v" id="smin">-</div></div>
    <div class="stat"><div class="k">AVG</div><div class="v" id="savg">-</div></div>
    <div class="stat"><div class="k">CENTER</div><div class="v" id="sctr">-</div></div>
    <div class="stat"><div class="k">CURSOR</div><div class="v" id="scur">-</div></div>
    <div class="stat"><div class="k">SENSOR Ta</div><div class="v" id="sta">-</div></div>
  </div>

  <div class="grid" style="margin-top:10px">
    <div class="stat"><div class="k">고온부 평균</div><div class="v hot" id="mp95">-</div></div>
    <div class="stat"><div class="k">배경(중앙값)</div><div class="v" id="mmed">-</div></div>
    <div class="stat"><div class="k">배경 대비 ΔT</div><div class="v hot" id="mdel">-</div></div>
    <div class="stat"><div class="k">고온 픽셀</div><div class="v" id="mhot">-</div></div>
    <div class="stat"><div class="k">상승률</div><div class="v" id="mris">-</div></div>
  </div>

  <div class="ctl">
    <label><input type="checkbox" id="auto" checked> 자동 범위</label>
    <label>min <input type="number" id="lo" value="20" step="1"></label>
    <label>max <input type="number" id="hi" value="40" step="1"></label>
    <label><input type="checkbox" id="flip" checked> 좌우반전</label>
    <label><input type="checkbox" id="mark" checked> 최고점 표시</label>
    <span id="meta"></span>
  </div>
  <div id="err"></div>
</div>
</div><script>
const W=32,H=24;
const cv=document.getElementById('cv'), ctx=cv.getContext('2d');
const off=document.createElement('canvas'); off.width=W; off.height=H;
const octx=off.getContext('2d');
const $=id=>document.getElementById(id);
let cur=null, cx=-1, cy=-1;

// ironbow 계열 팔레트
const S=[[0.00,  0,  0,  0],[0.15, 35, 12, 92],[0.30,110, 20,120],
         [0.45,190, 40, 88],[0.60,240, 95, 38],[0.75,255,160, 22],
         [0.90,255,226, 84],[1.00,255,255,255]];
function cmap(f){
  f=Math.max(0,Math.min(1,f));
  for(let i=0;i<S.length-1;i++){
    if(f<=S[i+1][0]){
      const a=S[i],b=S[i+1], t=(f-a[0])/(b[0]-a[0]||1);
      return [a[1]+(b[1]-a[1])*t, a[2]+(b[2]-a[2])*t, a[3]+(b[3]-a[3])*t];
    }
  }
  return [255,255,255];
}
(function legend(){
  let s=[]; for(let i=0;i<=20;i++){const c=cmap(i/20);
    s.push(`rgb(${c[0]|0},${c[1]|0},${c[2]|0}) ${i*5}%`);}
  $('bar').style.background=`linear-gradient(90deg,${s.join(',')})`;
})();

const f1=v=>v.toFixed(1)+'°C';

function render(){
  if(!cur) return;
  const t=cur.t, flip=$('flip').checked;
  let mn=Infinity,mx=-Infinity,sum=0;
  for(let i=0;i<768;i++){const v=t[i]; if(v<mn)mn=v; if(v>mx)mx=v; sum+=v;}
  const avg=sum/768;

  let lo,hi;
  if($('auto').checked){ lo=mn; hi=mx; $('lo').value=mn.toFixed(0); $('hi').value=mx.toFixed(0); }
  else { lo=parseFloat($('lo').value); hi=parseFloat($('hi').value); }
  const span=(hi-lo)||1;

  const img=octx.createImageData(W,H);
  let bx=0,by=0;
  for(let y=0;y<H;y++) for(let x=0;x<W;x++){
    const sx = flip ? (W-1-x) : x;
    const v=t[y*W+sx];
    if(v===mx){bx=x;by=y;}
    const c=cmap((v-lo)/span), i=(y*W+x)*4;
    img.data[i]=c[0]; img.data[i+1]=c[1]; img.data[i+2]=c[2]; img.data[i+3]=255;
  }
  octx.putImageData(img,0,0);
  ctx.imageSmoothingEnabled=true; ctx.imageSmoothingQuality='high';
  ctx.drawImage(off,0,0,cv.width,cv.height);

  if($('mark').checked){
    const px=(bx+0.5)*cv.width/W, py=(by+0.5)*cv.height/H;
    ctx.strokeStyle='#fff'; ctx.lineWidth=2;
    ctx.beginPath(); ctx.arc(px,py,13,0,6.283); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(px-20,py); ctx.lineTo(px-16,py);
    ctx.moveTo(px+16,py); ctx.lineTo(px+20,py);
    ctx.moveTo(px,py-20); ctx.lineTo(px,py-16);
    ctx.moveTo(px,py+16); ctx.lineTo(px,py+20); ctx.stroke();
    ctx.font='bold 15px ui-monospace,monospace'; ctx.fillStyle='#fff';
    ctx.fillText(f1(mx), Math.min(px+18,cv.width-70), Math.max(py-18,18));
  }

  $('blo').textContent=f1(lo); $('bhi').textContent=f1(hi);
  $('smax').textContent=f1(mx); $('smin').textContent=f1(mn);
  $('savg').textContent=f1(avg);
  $('sctr').textContent=f1((t[11*W+15]+t[11*W+16]+t[12*W+15]+t[12*W+16])/4);
  $('sta').textContent=f1(cur.ta);
  $('mp95').textContent=f1(cur.hotmean);
  $('mmed').textContent=f1(cur.median);
  $('mdel').textContent='+'+cur.delta.toFixed(1)+'°C';
  $('mhot').textContent=cur.hot+' px';
  $('mris').textContent=(cur.rise>=0?'+':'')+cur.rise.toFixed(2)+'°C/s';
  if(cx>=0){ const sx=flip?(W-1-cx):cx; $('scur').textContent=f1(t[cy*W+sx]); }
  $('meta').textContent=`#${cur.seq} · ${cur.ms|0}ms · drop ${cur.drop}`;
}

cv.addEventListener('mousemove',e=>{
  const r=cv.getBoundingClientRect();
  cx=Math.floor((e.clientX-r.left)/r.width*W);
  cy=Math.floor((e.clientY-r.top)/r.height*H);
  cx=Math.max(0,Math.min(W-1,cx)); cy=Math.max(0,Math.min(H-1,cy));
  render();
});
cv.addEventListener('mouseleave',()=>{cx=-1;$('scur').textContent='-';});
['auto','lo','hi','flip','mark'].forEach(id=>
  $(id).addEventListener('change',render));

async function loop(){
  try{
    const r=await fetch('frame.json',{cache:'no-store'});
    if(!r.ok) throw new Error('HTTP '+r.status);
    cur=await r.json(); $('err').textContent=''; render();
  }catch(e){ $('err').textContent='수신 실패: '+e.message+' (센서/서버 확인)'; }
  setTimeout(loop,250);
}
loop();
</script></body></html>)HTML";

// ------------------------------------------------------------ 아주 작은 HTTP 서버
static bool send_all(int fd, const char* p, size_t n) {
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) return false;
        p += w; n -= w;
    }
    return true;
}

static void respond(int fd, const char* type, const std::string& body) {
    char hdr[256];
    int hn = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n", type, body.size());
    if (send_all(fd, hdr, hn)) send_all(fd, body.data(), body.size());
}

static std::string frame_json() {
    std::string s;
    s.reserve(8192);
    char b[256];
    snprintf(b, sizeof(b),
             "{\"seq\":%ld,\"ta\":%.1f,\"ms\":%.0f,\"drop\":%d,"
             "\"max\":%.1f,\"p99\":%.1f,\"hotmean\":%.1f,\"median\":%.1f,"
             "\"delta\":%.1f,\"hot\":%d,\"rise\":%.2f,\"t\":[",
             g_seq, g_ta, g_frame_ms, g_drops,
             g_m.mx, g_m.p99, g_m.hotmean, g_m.median, g_m.delta, g_m.hot, g_m.rise);
    s += b;
    for (int i = 0; i < 768; i++) {
        float v = g_to[i];
        if (!std::isfinite(v) || v < -40.0f || v > 300.0f) v = g_ta;  // 깨진 픽셀은 주변온도로
        snprintf(b, sizeof(b), "%s%.1f", i ? "," : "", v);
        s += b;
    }
    s += "]}";
    return s;
}

static long g_hits = 0;

static void handle(int fd) {
    char req[2048];
    ssize_t n = read(fd, req, sizeof(req) - 1);
    if (n <= 0) return;          // 브라우저의 선접속(preconnect) 등 -> 그냥 닫음
    req[n] = 0;
    g_hits++;

    if      (strncmp(req, "GET /frame.json", 15) == 0) respond(fd, "application/json", frame_json());
    else if (strncmp(req, "GET / ",  6) == 0 ||
             strncmp(req, "GET /index.html", 15) == 0) respond(fd, "text/html; charset=utf-8", PAGE);
    else {
        const char* r = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send_all(fd, r, strlen(r));
    }
}

// 프레임 사이사이에 밀린 요청을 처리한다 (단일 스레드)
static void serve_for(int listen_fd, double ms) {
    double end = now_ms() + ms;
    while (running) {
        double left = end - now_ms();
        if (left <= 0) break;
        fd_set rs; FD_ZERO(&rs); FD_SET(listen_fd, &rs);
        struct timeval tv;
        tv.tv_sec  = (long)(left / 1000);
        tv.tv_usec = (long)((left - tv.tv_sec * 1000) * 1000);
        if (select(listen_fd + 1, &rs, NULL, NULL, &tv) <= 0) break;
        int c = accept(listen_fd, NULL, NULL);
        if (c < 0) continue;
        int one = 1;
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        // 브라우저가 소켓만 열고 요청을 안 보내는 경우 read 에서 영원히 멈추지 않도록
        struct timeval to; to.tv_sec = 2; to.tv_usec = 0;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
        setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
        handle(c);
        close(c);
    }
}

// ------------------------------------------------------------ main
int main(int argc, char** argv) {
    signal(SIGINT,  on_sigint);
    signal(SIGTERM, on_sigint);
    signal(SIGPIPE, SIG_IGN);

    int         port = (argc > 1) ? atoi(argv[1]) : 8080;
    double      hz   = (argc > 2) ? atof(argv[2]) : 1.0;
    const char* csv  = (argc > 3) ? argv[3] : NULL;   // 있으면 CSV 로 기록

    // !! 포트를 먼저 잡는다 !!
    // CSV 를 먼저 열면, 이미 다른 인스턴스가 돌고 있을 때
    // 두 번째 인스턴스가 파일을 덮어쓴 뒤에야 bind 에서 죽는다.
    // 그러면 두 프로세스의 기록이 한 파일에 섞여 데이터가 망가진다.
    int lf = socket(AF_INET, SOCK_STREAM, 0);
    if (lf < 0) { perror("socket"); return 1; }
    int one = 1;
    setsockopt(lf, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (bind(lf, (struct sockaddr*)&a, sizeof(a)) < 0) {
        perror("bind");
        printf("\n[X] 포트 %d 가 이미 사용 중입니다. 기존 프로세스를 먼저 종료하세요:\n", port);
        printf("      killall mlx_stream\n");
        printf("    CSV 는 건드리지 않았습니다.\n");
        return 1;
    }
    if (listen(lf, 16) < 0) { perror("listen"); return 1; }

    // 포트를 확보한 뒤에야 CSV 를 연다. 덮어쓰기도 막는다.
    FILE* fp = NULL;
    if (csv) {
        FILE* chk = fopen(csv, "r");
        if (chk) {
            fclose(chk);
            printf("\n[X] %s 가 이미 있습니다. 덮어쓰지 않습니다.\n", csv);
            printf("    다른 이름을 쓰거나 기존 파일을 옮기세요.\n");
            close(lf);
            return 1;
        }
        fp = fopen(csv, "w");
        if (!fp) { printf("[X] %s 열기 실패\n", csv); close(lf); return 1; }
        fprintf(fp, "sec,max,p95,p99,hotmean,median,delta,hot_px,rise_c_per_s,ta,drop\n");
        fflush(fp);
        printf("CSV 기록 -> %s\n", csv);
    }

    printf("MLX90640 초기화...\n");
    MLX90640_I2CInit();

    static uint16_t ee[832];
    if (MLX90640_DumpEE(MLX_ADDR, ee) != 0) {
        printf("[X] EEPROM 덤프 실패. i2c_probe 로 0x33 먼저 확인하세요.\n");
        if (fp) { fclose(fp); remove(csv); }
        return 1;
    }
    if (MLX90640_ExtractParameters(ee, &params) != 0) {
        printf("[X] 캘리브레이션 추출 실패 (I2C 통신 불안정)\n");
        if (fp) { fclose(fp); remove(csv); }
        return 1;
    }
    MLX90640_SetRefreshRate(MLX_ADDR, rate_code(hz));
    MLX90640_SetChessMode(MLX_ADDR);
    MLX90640_SetResolution(MLX_ADDR, 0x02);
    printf("[OK] 센서 준비됨 (%.1f Hz)\n", hz);

    printf("[OK] HTTP 서버 :%d\n\n", port);
    printf("  노트북에서 터널을 뚫고 브라우저로 접속:\n");
    printf("    ssh -L %d:<Pi4 IP>:%d 계정@마스터PC\n", port, port);
    printf("    http://localhost:%d\n\n", port);
    printf("  Ctrl+C 로 종료\n\n");

    for (int i = 0; i < 2 && running; i++) read_full_frame();   // 워밍업

    double t_start = now_ms(), t_prev = t_start;
    while (running) {
        int ok = read_full_frame();
        if (ok > 0) {
            g_seq++;
            double now = now_ms();
            compute_metrics(g_to, (now - t_prev) / 1000.0);
            t_prev = now;
            if (fp) {
                fprintf(fp, "%.1f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%.3f,%.2f,%d\n",
                        (now - t_start) / 1000.0,
                        g_m.mx, g_m.p95, g_m.p99, g_m.hotmean, g_m.median,
                        g_m.delta, g_m.hot, g_m.rise, g_ta, g_drops);
                fflush(fp);
            }
        }
        printf("\r#%ld  max %.1fC  고온평균 %.1fC  배경 %.1fC  dT +%.1f  %d px  "
               "상승 %+.2f C/s  drop %d  요청 %ld   ",
               g_seq, g_m.mx, g_m.hotmean, g_m.median, g_m.delta,
               g_m.hot, g_m.rise, g_drops, g_hits);
        fflush(stdout);
        serve_for(lf, 300);
    }
    printf("\n종료\n");
    if (fp) {
        fclose(fp);
        printf("CSV 저장됨: %s\n", csv);
        printf("가져오기:  scp root@<Pi4 IP>:%s .\n", csv);
    }
    close(lf);
    return 0;
}
___EOF_MLX_STREAM_CPP___

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
ls -l mlx_stream.cpp xbuild.sh
echo
echo "이제: sh xbuild.sh root@<Pi4 IP>"
