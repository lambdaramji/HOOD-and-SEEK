#include "tcp_server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace {

constexpr size_t kMaxLine = 4096;             // 개행 포함, 넘으면 연결을 끊음 (설계서 §2.1)
constexpr size_t kMaxClients = 8;             // 넘으면 바로 끊음 (PR #75 리뷰 3번)
constexpr uint64_t kHelloTimeoutMs = 5000;    // hello 없으면 끊음 (설계서 §2.1)
constexpr size_t kMaxTxBacklog = 64 * 1024;   // 못 받아 가는 클라이언트(state 약 1분 분량)는 끊음
constexpr size_t kMaxQueuedCommands = 16;     // tick 사이에 쌓일 수 있는 voice·ui_cmd 수
constexpr size_t kMaxQueuedVision = 16;       // tick 사이에 쌓일 수 있는 vision 메시지 수 (≈3 Hz)

void set_nonblock(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

}  // namespace

TcpServer::TcpServer(uint16_t port, Snapshot& snap, std::function<void()> on_command)
    : snap_(snap), on_command_(std::move(on_command)) {
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    std::perror("[tcp] pipe");
    return;
  }
  wake_rd_ = pipefd[0];
  wake_wr_ = pipefd[1];
  set_nonblock(wake_rd_);
  set_nonblock(wake_wr_);

  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    std::perror("[tcp] socket");
    return;
  }
  const int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || listen(fd, 16) != 0) {
    std::fprintf(stderr, "[tcp] :%u bind/listen 실패: %s\n", port, std::strerror(errno));
    close(fd);
    return;
  }
  set_nonblock(fd);
  listen_fd_ = fd;
  std::printf("[tcp] listening :%u\n", port);
}

TcpServer::~TcpServer() {
  for (Client& c : clients_) close(c.fd);
  for (int fd : {listen_fd_, wake_rd_, wake_wr_})
    if (fd >= 0) close(fd);
}

void TcpServer::publish(const std::string& state_line) {
  {
    std::lock_guard<std::mutex> lock(pub_mu_);
    latest_ = state_line;
    fresh_ = true;
  }
  const char b = 1;
  (void)!write(wake_wr_, &b, 1);  // 파이프가 가득 차 있으면(EAGAIN) 이미 깨울 예정이므로 무시
}

bool TcpServer::run(const std::atomic<bool>& stop) {
  std::vector<pollfd> fds;
  char drain[64];
  while (!stop) {
    fds.clear();
    fds.push_back({listen_fd_, POLLIN, 0});
    fds.push_back({wake_rd_, POLLIN, 0});
    const uint64_t now = mono_ms();
    int timeout = 1000;
    for (const Client& c : clients_) {
      fds.push_back({c.fd, static_cast<short>(POLLIN | (c.tx.empty() ? 0 : POLLOUT)), 0});
      if (c.role == Role::none) {
        const uint64_t deadline = c.accepted_ms + kHelloTimeoutMs;
        timeout = std::min<int>(timeout, deadline > now ? static_cast<int>(deadline - now) : 0);
      }
    }
    if (poll(fds.data(), fds.size(), timeout) < 0) {
      if (errno == EINTR) continue;
      std::perror("[tcp] poll");
      return false;
    }

    if (fds[1].revents & POLLIN) {
      while (read(wake_rd_, drain, sizeof drain) > 0) {
      }
      std::string line;
      {
        std::lock_guard<std::mutex> lock(pub_mu_);
        if (fresh_) line = latest_;
        fresh_ = false;
      }
      if (!line.empty())
        for (Client& c : clients_)
          if (c.role != Role::none) c.tx += line + '\n';
    }

    // clients_는 아래 accept_all()에서 늘어날 수 있으므로, 이번 poll 결과는 기존 클라이언트에만 적용한다
    const size_t polled = clients_.size();
    for (size_t i = 0; i < polled; ++i) {
      Client& c = clients_[i];
      const short ev = fds[i + 2].revents;
      if (ev & POLLIN) read_client(c);
      else if (ev & (POLLERR | POLLHUP | POLLNVAL)) close_client(c, "hangup");
      if (!c.closing && (ev & POLLOUT) && !c.tx.empty()) {
        const ssize_t n = send(c.fd, c.tx.data(), c.tx.size(), 0);
        if (n > 0) c.tx.erase(0, static_cast<size_t>(n));
        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) close_client(c, "send error");
      }
      if (!c.closing && c.tx.size() > kMaxTxBacklog) close_client(c, "tx backlog");
      if (!c.closing && c.role == Role::none && mono_ms() - c.accepted_ms > kHelloTimeoutMs) close_client(c, "no hello in 5 s");
    }
    // 닫힌 연결을 먼저 지운 뒤 accept — 살아 있는 연결만 세어 8개 상한을 적용한다 (#76 리뷰 nit)
    clients_.erase(std::remove_if(clients_.begin(), clients_.end(), [](const Client& c) { return c.closing; }),
                   clients_.end());
    if (fds[0].revents & POLLIN) accept_all();
  }
  return true;
}

void TcpServer::accept_all() {
  for (;;) {
    sockaddr_in addr{};
    socklen_t len = sizeof addr;
    const int fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len);
    if (fd < 0) return;  // EAGAIN: 더 없음
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof ip);
    const std::string peer = std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
    if (clients_.size() >= kMaxClients) {
      std::printf("[tcp] %s 거부: 동시 연결 %zu개 초과\n", peer.c_str(), kMaxClients);
      close(fd);
      continue;
    }
    set_nonblock(fd);
#ifdef SO_NOSIGPIPE
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    Client c{};
    c.fd = fd;
    c.peer = peer;
    c.accepted_ms = mono_ms();
    clients_.push_back(std::move(c));
    std::printf("[tcp] %s 접속 (%zu/%zu)\n", peer.c_str(), clients_.size(), kMaxClients);
  }
}

void TcpServer::read_client(Client& c) {
  char buf[2048];
  for (;;) {
    const ssize_t n = recv(c.fd, buf, sizeof buf, 0);
    if (n == 0) return close_client(c, "eof");
    if (n < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) close_client(c, "recv error");
      return;
    }
    c.rx.append(buf, static_cast<size_t>(n));
    size_t pos;
    while (!c.closing && (pos = c.rx.find('\n')) != std::string::npos) {
      if (pos + 1 > kMaxLine) return close_client(c, "line > 4096 B");
      std::string_view line(c.rx.data(), pos);
      if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
      if (!line.empty()) handle_line(c, line);
      c.rx.erase(0, pos + 1);
    }
    if (c.closing) return;
    if (c.rx.size() >= kMaxLine) return close_client(c, "line > 4096 B");
  }
}

void TcpServer::handle_line(Client& c, std::string_view line) {
  Inbound m{};
  std::string why = parse_inbound(line, c.role, m);
  if (why.empty() && c.last_seq && m.seq <= *c.last_seq) why = "seq " + std::to_string(m.seq) + " <= " + std::to_string(*c.last_seq);
  if (!why.empty()) return drop(c, why);
  c.last_seq = m.seq;

  const uint64_t now = mono_ms();
  switch (m.type) {
    case Inbound::Type::hello: {
      c.role = m.hello_role;
      static const char* kRoleName[] = {"none", "vision", "voice", "ui"};
      std::printf("[tcp] %s hello (%s)\n", c.peer.c_str(), kRoleName[static_cast<int>(c.role)]);
      std::lock_guard<std::mutex> lock(pub_mu_);
      if (!latest_.empty()) c.tx += latest_ + '\n';  // 서버는 state 1회로 즉시 응답 (설계서 §2.3)
      return;
    }
    case Inbound::Type::vision: {
      std::lock_guard<std::mutex> lock(snap_.mu);
      // 큐가 차면 가장 오래된 관측을 버리고 최신을 남긴다 — 링크가 조용히 끊겼다 붙어 밀린 줄이 한꺼번에 와도
      // 최신 관측이 살아남게 (#76 리뷰 nit, #77 리뷰 ①). 평소에는 tick 사이 2~3개라 차지 않는다.
      if (snap_.vision_msgs.size() >= kMaxQueuedVision) {
        snap_.vision_msgs.erase(snap_.vision_msgs.begin());
        ++snap_.rx_dropped;
      }
      snap_.vision_msgs.push_back(m.vision);
      snap_.vision_rx_ms = now;
      return;
    }
    case Inbound::Type::command: {
      {
        std::lock_guard<std::mutex> lock(snap_.mu);
        if (snap_.commands.size() >= kMaxQueuedCommands) {
          ++snap_.rx_dropped;
          std::printf("[tcp] %s drop: command queue full\n", c.peer.c_str());
          return;
        }
        snap_.commands.emplace_back(now, m.command);
      }
      on_command_();
      return;
    }
  }
}

void TcpServer::drop(Client& c, const std::string& why) {
  {
    std::lock_guard<std::mutex> lock(snap_.mu);
    ++snap_.rx_dropped;
  }
  ++c.drops;
  // 고장 난 노드가 로그를 덮지 않게 처음 5건과 이후 2의 거듭제곱 번째만 남긴다
  if (c.drops <= 5 || (c.drops & (c.drops - 1)) == 0)
    std::printf("[tcp] %s drop #%u: %s\n", c.peer.c_str(), c.drops, why.c_str());
}

void TcpServer::close_client(Client& c, const char* why) {
  if (c.closing) return;
  std::printf("[tcp] %s 끊음: %s\n", c.peer.c_str(), why);
  close(c.fd);
  c.closing = true;
}
