#include "core/log.hpp"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <atomic>
#include <chrono>
#include <mutex>

namespace gsr::log {

namespace {

std::atomic<std::uint64_t> g_frame{0};

// Custom pattern flag emitting the frame stamp.
class FrameStampFlag final : public spdlog::custom_flag_formatter {
 public:
  void format(const spdlog::details::log_msg&, const std::tm&,
              spdlog::memory_buf_t& dest) override {
    fmt::format_to(std::back_inserter(dest), "[frame {}][t_mono_us {}]", current_frame(),
                   mono_us());
  }

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<FrameStampFlag>();
  }
};

}  // namespace

std::uint64_t mono_us() {
  static const auto t0 = std::chrono::steady_clock::now();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::steady_clock::now() - t0)
                                        .count());
}

std::uint64_t current_frame() { return g_frame.load(std::memory_order_relaxed); }

void set_current_frame(std::uint64_t frame) { g_frame.store(frame, std::memory_order_relaxed); }

void advance_frame() { g_frame.fetch_add(1, std::memory_order_relaxed); }

std::unique_ptr<spdlog::formatter> make_formatter() {
  auto formatter = std::make_unique<spdlog::pattern_formatter>();
  formatter->add_flag<FrameStampFlag>('*').set_pattern(
      "[%Y-%m-%dT%H:%M:%S.%e] [%n] [%^%l%$] %* %v");
  return formatter;
}

void init(spdlog::level::level_enum default_level) {
  (void)mono_us();  // pin the monotonic epoch to startup
  spdlog::set_formatter(make_formatter());
  spdlog::set_level(default_level);
}

void shutdown() { spdlog::shutdown(); }

std::shared_ptr<spdlog::logger> get(const std::string& subsystem) {
  static std::mutex create_mutex;
  std::lock_guard<std::mutex> lock(create_mutex);
  if (auto existing = spdlog::get(subsystem)) {
    return existing;
  }
  return spdlog::stdout_color_mt(subsystem);
}

void set_level(const std::string& subsystem, spdlog::level::level_enum level) {
  get(subsystem)->set_level(level);
}

}  // namespace gsr::log
