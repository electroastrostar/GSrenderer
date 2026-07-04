#pragma once

#include "tracking/freed.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

// FreeD-over-UDP ingest (Winsock/POSIX). Startup errors throw; once the receive thread
// runs, everything is status/stats based (per-frame hot path feeds off this).
namespace gsr::tracking {

struct ListenerStats {
  std::uint64_t packets_ok = 0;
  std::uint64_t packets_rejected = 0;  // wrong size / type / checksum
  double packet_rate_hz = 0.0;         // good packets over the last ~1 s window
};

// A decoded pose stamped with the process monotonic clock (gsr::log::mono_us()).
struct TimedPose {
  FreedPose pose;
  std::uint64_t t_mono_us = 0;
};

// Listens for FreeD D1 packets on a background thread.
class FreedListener {
 public:
  using PoseCallback = std::function<void(const TimedPose&)>;

  // Binds 0.0.0.0:port (port 0 = ephemeral, see port()) and starts receiving.
  // Throws std::runtime_error on socket/bind failure. `callback` (optional) runs on the
  // receive thread for every good packet — keep it cheap; renderers should poll latest().
  explicit FreedListener(std::uint16_t port, PoseCallback callback = nullptr);
  ~FreedListener();
  FreedListener(const FreedListener&) = delete;
  FreedListener& operator=(const FreedListener&) = delete;

  std::uint16_t port() const;               // actual bound port
  ListenerStats stats() const;              // thread-safe snapshot
  std::optional<TimedPose> latest() const;  // most recent good pose, if any

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Minimal UDP sender (simulator + tests). Throws on resolution/socket failure at
// construction; send() is status-only.
class UdpSender {
 public:
  UdpSender(const std::string& host, std::uint16_t port);
  ~UdpSender();
  UdpSender(const UdpSender&) = delete;
  UdpSender& operator=(const UdpSender&) = delete;

  bool send(std::span<const std::uint8_t> bytes);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace gsr::tracking
