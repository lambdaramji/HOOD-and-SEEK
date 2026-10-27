// 계약 v1 TCP 서버 (설계서 §2.1, contracts/README.md). poll 기반 단일 스레드:
// 접속·수신·검증·송신을 모두 run() 한 스레드에서 한다. 판정은 하지 않고 Snapshot에 관측·명령만 넣는다.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "message.h"
#include "snapshot.h"

class TcpServer {
 public:
  // on_command: voice·ui_cmd가 들어오면 호출 — policy 스레드를 깨워 즉시 판정하게 한다(변화 즉시 state)
  TcpServer(uint16_t port, Snapshot& snap, std::function<void()> on_command);
  ~TcpServer();
  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;

  bool listen_ok() const { return listen_fd_ >= 0; }
  bool run(const std::atomic<bool>& stop);      // tcp 스레드 본체. stop으로 끝나면 true, 오류로 끝나면 false
  void publish(const std::string& state_line);  // policy 스레드: 최신 state를 hello한 모든 클라이언트에

 private:
  struct Client {
    int fd;
    std::string peer;
    uint64_t accepted_ms;
    Role role = Role::none;
    std::optional<uint32_t> last_seq;  // 같은 연결 안에서만 비교 (D-042 ①)
    std::string rx, tx;
    uint32_t drops = 0;
    bool closing = false;
  };

  void accept_all();
  void read_client(Client& c);
  void handle_line(Client& c, std::string_view line);
  void drop(Client& c, const std::string& why);
  void close_client(Client& c, const char* why);

  int listen_fd_ = -1;
  int wake_rd_ = -1, wake_wr_ = -1;  // publish → poll 깨우기 (self-pipe)
  Snapshot& snap_;
  std::function<void()> on_command_;
  std::vector<Client> clients_;      // tcp 스레드 전용

  std::mutex pub_mu_;                // publish와 run 사이
  std::string latest_;
  bool fresh_ = false;
};
