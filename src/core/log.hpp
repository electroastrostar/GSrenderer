#pragma once

#include <spdlog/spdlog.h>

#include <cstdint>
#include <memory>
#include <string>

// Logging facade (see CLAUDE.md): per-subsystem spdlog loggers whose every record carries the
// frame stamp `[frame N][t_mono_us T]`. The main render loop owns the frame counter.
namespace gsr::log {

// Microseconds on the process-wide monotonic timebase (zero at first use).
std::uint64_t mono_us();

// Frame counter advanced once per render-loop iteration; 0 before the loop starts.
std::uint64_t current_frame();
void set_current_frame(std::uint64_t frame);
void advance_frame();

// Formatter producing `[ISO8601] [subsystem] [level] [frame N][t_mono_us T] message`.
// Exposed so tests (and custom sinks) can apply the same format.
std::unique_ptr<spdlog::formatter> make_formatter();

// Install the formatter and default level. Idempotent; call once at startup before get().
void init(spdlog::level::level_enum default_level = spdlog::level::info);

// Flush and drop all loggers. init() may be called again afterwards.
void shutdown();

// Get (or lazily create, attached to stdout) the logger for a subsystem, e.g. "tracking".
std::shared_ptr<spdlog::logger> get(const std::string& subsystem);

// Per-subsystem verbosity, e.g. set_level("tracking", spdlog::level::debug).
void set_level(const std::string& subsystem, spdlog::level::level_enum level);

}  // namespace gsr::log
