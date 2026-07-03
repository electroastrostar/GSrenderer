#include "tracking/udp_listener.hpp"

#include "core/log.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace gsr::tracking {

namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

void close_socket(SocketHandle s) { closesocket(s); }

// One-time Winsock init, released at process exit.
void ensure_socket_runtime() {
  static const int rc = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data);
  }();
  if (rc != 0) throw std::runtime_error("WSAStartup failed");
}

void set_recv_timeout_ms(SocketHandle s, int ms) {
  const DWORD timeout = static_cast<DWORD>(ms);
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
             sizeof timeout);
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

void close_socket(SocketHandle s) { ::close(s); }
void ensure_socket_runtime() {}

void set_recv_timeout_ms(SocketHandle s, int ms) {
  timeval timeout{};
  timeout.tv_sec = ms / 1000;
  timeout.tv_usec = (ms % 1000) * 1000;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
}
#endif

SocketHandle open_udp_socket() {
  ensure_socket_runtime();
  const SocketHandle s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == kInvalidSocket) throw std::runtime_error("UDP socket creation failed");
  return s;
}

}  // namespace

// ---------------------------------------------------------------- FreedListener

struct FreedListener::Impl {
  SocketHandle socket = kInvalidSocket;
  std::uint16_t bound_port = 0;
  PoseCallback callback;
  std::thread receiver;
  std::atomic<bool> stop{false};

  mutable std::mutex mutex;  // guards stats + latest
  ListenerStats stats;
  std::optional<TimedPose> latest;

  // Rate window state (receive thread only).
  std::uint64_t window_start_us = 0;
  std::uint64_t window_count = 0;

  void receive_loop() {
    auto log = gsr::log::get("tracking");
    std::uint8_t buffer[512];
    window_start_us = gsr::log::mono_us();

    while (!stop.load(std::memory_order_relaxed)) {
      const auto received =
          ::recv(socket, reinterpret_cast<char*>(buffer), sizeof buffer, 0);
      const std::uint64_t now_us = gsr::log::mono_us();

      if (received > 0) {
        const auto pose = parse_freed_d1(
            std::span<const std::uint8_t>(buffer, static_cast<std::size_t>(received)));
        if (pose.has_value()) {
          const TimedPose timed{*pose, now_us};
          {
            std::lock_guard<std::mutex> lock(mutex);
            ++stats.packets_ok;
            ++window_count;
            latest = timed;
          }
          if (callback) callback(timed);  // outside the lock — callback may be slow
        } else {
          std::lock_guard<std::mutex> lock(mutex);
          ++stats.packets_rejected;
          if (stats.packets_rejected % 100 == 1) {
            log->warn("rejected FreeD packet ({} bytes; total rejected {})", received,
                      stats.packets_rejected);
          }
        }
      }
      // recv timeout (or error) falls through to the rate/stop bookkeeping.

      if (now_us - window_start_us >= 1'000'000) {
        std::lock_guard<std::mutex> lock(mutex);
        stats.packet_rate_hz = static_cast<double>(window_count) * 1e6 /
                               static_cast<double>(now_us - window_start_us);
        window_start_us = now_us;
        window_count = 0;
      }
    }
  }
};

FreedListener::FreedListener(std::uint16_t port, PoseCallback callback)
    : impl_(std::make_unique<Impl>()) {
  impl_->callback = std::move(callback);
  impl_->socket = open_udp_socket();

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (::bind(impl_->socket, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0) {
    close_socket(impl_->socket);
    throw std::runtime_error("FreedListener: bind failed on port " + std::to_string(port));
  }

  sockaddr_in bound{};
#ifdef _WIN32
  int bound_len = sizeof bound;
#else
  socklen_t bound_len = sizeof bound;
#endif
  if (::getsockname(impl_->socket, reinterpret_cast<sockaddr*>(&bound), &bound_len) == 0) {
    impl_->bound_port = ntohs(bound.sin_port);
  }

  set_recv_timeout_ms(impl_->socket, 100);
  impl_->receiver = std::thread([impl = impl_.get()] { impl->receive_loop(); });
  gsr::log::get("tracking")->info("FreeD listener on UDP port {}", impl_->bound_port);
}

FreedListener::~FreedListener() {
  impl_->stop.store(true, std::memory_order_relaxed);
  if (impl_->receiver.joinable()) impl_->receiver.join();
  if (impl_->socket != kInvalidSocket) close_socket(impl_->socket);
}

std::uint16_t FreedListener::port() const { return impl_->bound_port; }

ListenerStats FreedListener::stats() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->stats;
}

std::optional<TimedPose> FreedListener::latest() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->latest;
}

// ---------------------------------------------------------------- UdpSender

struct UdpSender::Impl {
  SocketHandle socket = kInvalidSocket;
  sockaddr_in dest{};
};

UdpSender::UdpSender(const std::string& host, std::uint16_t port)
    : impl_(std::make_unique<Impl>()) {
  impl_->socket = open_udp_socket();
  impl_->dest.sin_family = AF_INET;
  impl_->dest.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &impl_->dest.sin_addr) != 1) {
    close_socket(impl_->socket);
    throw std::runtime_error("UdpSender: invalid IPv4 address '" + host + "'");
  }
}

UdpSender::~UdpSender() {
  if (impl_->socket != kInvalidSocket) close_socket(impl_->socket);
}

bool UdpSender::send(std::span<const std::uint8_t> bytes) {
  const auto sent = ::sendto(impl_->socket, reinterpret_cast<const char*>(bytes.data()),
#ifdef _WIN32
                             static_cast<int>(bytes.size()),
#else
                             bytes.size(),
#endif
                             0, reinterpret_cast<const sockaddr*>(&impl_->dest),
                             sizeof impl_->dest);
  return sent >= 0 && static_cast<std::size_t>(sent) == bytes.size();
}

}  // namespace gsr::tracking
