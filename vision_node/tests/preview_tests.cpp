// PreviewServer over real loopback sockets: MJPEG framing, a viewer that leaves, a taken port, shutdown.
#include "preview.hpp"
#include <opencv2/imgcodecs.hpp>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }

int connect_viewer(int port) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  check(fd >= 0 && connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof address) == 0, "cannot connect to the preview port");
  const std::string request = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
  check(send(fd, request.data(), request.size(), 0) == static_cast<ssize_t>(request.size()), "cannot send the request");
  return fd;
}

// Offers frames every 50 ms, like the camera reader, until one whole JPEG part has arrived.
std::string first_part(hoodseek::PreviewServer& server, int fd, const cv::Mat& frame) {
  std::string got;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    server.offer(frame);
    pollfd wait{fd, POLLIN, 0};
    if (poll(&wait, 1, 50) == 1) {
      char buffer[4096];
      const auto n = recv(fd, buffer, sizeof buffer, 0);
      check(n > 0, "server closed the stream");
      got.append(buffer, static_cast<size_t>(n));
    }
    const auto length_at = got.find("Content-Length: ");
    const auto body_at = got.find("\r\n\r\n", length_at);
    if (length_at != std::string::npos && body_at != std::string::npos &&
        got.size() >= body_at + 4 + std::stoul(got.substr(length_at + 16))) return got;
  }
  throw std::runtime_error("no JPEG part within 3 s");
}

// Milliseconds from one offer until its whole part has arrived; the stream must be at a part boundary.
long long part_delay_ms(hoodseek::PreviewServer& server, int fd, const cv::Mat& frame) {
  const auto start = std::chrono::steady_clock::now();
  server.offer(frame);
  std::string got;
  while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
    pollfd wait{fd, POLLIN, 0};
    if (poll(&wait, 1, 10) == 1) {
      char buffer[4096];
      const auto n = recv(fd, buffer, sizeof buffer, 0);
      check(n > 0, "server closed the stream");
      got.append(buffer, static_cast<size_t>(n));
    }
    const auto length_at = got.find("Content-Length: ");
    const auto body_at = got.find("\r\n\r\n", length_at);
    if (length_at != std::string::npos && body_at != std::string::npos && got.size() >= body_at + 6 + std::stoul(got.substr(length_at + 16)))
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  }
  throw std::runtime_error("an offered frame did not arrive within 1 s");
}
}  // namespace

int main() {
  try {
    const cv::Mat frame(48, 64, CV_8UC3, cv::Scalar(30, 120, 220));
    {
      hoodseek::PreviewServer server(0);
      check(server.port() > 0, "port 0 must resolve to the bound port");
      bool taken = false;
      try { hoodseek::PreviewServer clash(server.port()); } catch (const std::runtime_error&) { taken = true; }
      check(taken, "a taken port must throw so the node can turn the video off and keep running");
      server.offer(frame);  // no viewer yet: nothing to copy, must return at once

      int fd = connect_viewer(server.port());
      std::string got = first_part(server, fd, frame);
      check(got.rfind("HTTP/1.0 200 OK\r\n", 0) == 0, "status line");
      check(got.find("Content-Type: multipart/x-mixed-replace; boundary=hoodseekframe\r\n") != std::string::npos, "MJPEG content type");
      const auto part_at = got.find("\r\n\r\n--hoodseekframe\r\nContent-Type: image/jpeg\r\nContent-Length: ");
      check(part_at != std::string::npos, "part header follows the response header");
      const auto length_at = got.find("Content-Length: ", part_at);
      const auto size = std::stoul(got.substr(length_at + 16));
      const auto body_at = got.find("\r\n\r\n", length_at) + 4;
      check(size > 4 && static_cast<unsigned char>(got[body_at]) == 0xFF && static_cast<unsigned char>(got[body_at + 1]) == 0xD8, "JPEG starts with SOI");
      check(static_cast<unsigned char>(got[body_at + size - 2]) == 0xFF && static_cast<unsigned char>(got[body_at + size - 1]) == 0xD9, "Content-Length ends at EOI");
      const cv::Mat shown = cv::imdecode(std::vector<unsigned char>(got.begin() + static_cast<std::ptrdiff_t>(body_at), got.begin() + static_cast<std::ptrdiff_t>(body_at + size)), cv::IMREAD_COLOR);
      check(shown.cols == frame.cols * 3 / 4 && shown.rows == frame.rows * 3 / 4, "the card gets the frame at 3/4 size");

      // Two seats. The app reconnects (screen change, restart): a closed viewer must free its seat, or after two
      // reconnects the card never gets video again.
      const int second = connect_viewer(server.port());
      first_part(server, second, frame);
      close(fd);
      for (int i = 0; i < 6; ++i) { server.offer(frame); usleep(50 * 1000); }
      const int third = connect_viewer(server.port());
      check(first_part(server, third, frame).find("Content-Type: image/jpeg") != std::string::npos, "a closed viewer did not free its seat");
      // Both seats taken: an extra client is closed at once instead of hanging.
      const int extra = connect_viewer(server.port());
      pollfd wait{extra, POLLIN, 0};
      char byte;
      // Closed with its request unread, so the kernel may send RST instead of FIN: EOF or ECONNRESET both mean closed.
      check(poll(&wait, 1, 2000) == 1 && recv(extra, &byte, 1, 0) <= 0, "a client beyond two seats must be closed at once");
      for (int open : {second, third, extra}) close(open);
    }  // destructor joins the server thread; a hang here fails the ctest timeout
    {
      // An offered frame goes out at once instead of waiting out the server's poll (#144 follow-up: the card lagged).
      hoodseek::PreviewServer server(0);
      const int fd = connect_viewer(server.port());
      first_part(server, fd, frame);
      usleep(300 * 1000);  // let parts from the offers above finish, then drop them
      for (char buffer[4096]; recv(fd, buffer, sizeof buffer, MSG_DONTWAIT) > 0;) {}
      int fast = 0;
      long long worst = 0;
      for (int i = 0; i < 10; ++i) {
        usleep(70 * 1000);  // a camera-rate gap, so the rate limit below does not drop the offer
        const auto ms = part_delay_ms(server, fd, frame);
        worst = std::max(worst, ms);
        fast += ms < 50;
      }
      std::cout << "preview_tests: " << fast << "/10 offers within 50 ms (worst " << worst << " ms)\n";
      // 8 of 10, not all: a busy PC may delay one now and then, while a 100 ms poll wait delays every one.
      check(fast >= 8, "an offered frame must reach the viewer within 50 ms");

      // The camera reader offers every camera frame; the card gets at most ~16 a second so encoding here and decoding
      // in the app (same Pi 5) stay small next to inference (#148 follow-up).
      std::string got;
      const auto start = std::chrono::steady_clock::now();
      while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
        server.offer(frame);
        pollfd wait{fd, POLLIN, 0};
        char buffer[4096];
        if (poll(&wait, 1, 1) == 1) { const auto n = recv(fd, buffer, sizeof buffer, 0); check(n > 0, "server closed the stream"); got.append(buffer, static_cast<size_t>(n)); }
      }
      usleep(200 * 1000);
      for (char buffer[4096]; ;) { const auto n = recv(fd, buffer, sizeof buffer, MSG_DONTWAIT); if (n <= 0) break; got.append(buffer, static_cast<size_t>(n)); }
      int parts = 0;
      for (auto at = got.find("--hoodseekframe"); at != std::string::npos; at = got.find("--hoodseekframe", at + 1)) ++parts;
      std::cout << "preview_tests: " << parts << " parts from 1 s of back-to-back offers\n";
      check(parts >= 5 && parts <= 17, "offers faster than ~16 a second must be thinned");
      close(fd);
    }
    std::cout << "preview_tests: ok\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "preview_tests: " << e.what() << '\n';
    return 1;
  }
}
