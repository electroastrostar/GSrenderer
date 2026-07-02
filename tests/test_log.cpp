#include <spdlog/sinks/ostream_sink.h>

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <sstream>
#include <thread>

#include "core/log.hpp"

TEST_CASE("mono_us is monotonic and advances with wall time", "[core][log]") {
  const auto a = gsr::log::mono_us();
  const auto b = gsr::log::mono_us();
  REQUIRE(b >= a);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  REQUIRE(gsr::log::mono_us() >= a + 2000);
}

TEST_CASE("frame counter: set, read, advance", "[core][log]") {
  gsr::log::set_current_frame(41);
  REQUIRE(gsr::log::current_frame() == 41);
  gsr::log::advance_frame();
  REQUIRE(gsr::log::current_frame() == 42);
  gsr::log::set_current_frame(0);
}

TEST_CASE("get returns one logger per subsystem", "[core][log]") {
  gsr::log::init();
  auto a = gsr::log::get("test_subsys_a");
  auto b = gsr::log::get("test_subsys_b");
  REQUIRE(a != nullptr);
  REQUIRE(b != nullptr);
  REQUIRE(a != b);
  REQUIRE(gsr::log::get("test_subsys_a") == a);
  REQUIRE(a->name() == "test_subsys_a");
}

TEST_CASE("set_level controls per-subsystem verbosity", "[core][log]") {
  gsr::log::init();
  gsr::log::set_level("test_levels", spdlog::level::warn);
  auto logger = gsr::log::get("test_levels");
  REQUIRE(logger->level() == spdlog::level::warn);
  REQUIRE_FALSE(logger->should_log(spdlog::level::info));
  REQUIRE(logger->should_log(spdlog::level::err));
}

TEST_CASE("records carry the [frame N][t_mono_us T] stamp", "[core][log]") {
  std::ostringstream out;
  auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(out);
  auto logger = std::make_shared<spdlog::logger>("test_stamp", sink);
  logger->set_formatter(gsr::log::make_formatter());

  gsr::log::set_current_frame(7);
  logger->info("hello stamp");
  gsr::log::set_current_frame(0);

  const std::string text = out.str();
  REQUIRE(text.find("[frame 7][t_mono_us ") != std::string::npos);
  REQUIRE(text.find("[test_stamp]") != std::string::npos);
  REQUIRE(text.find("hello stamp") != std::string::npos);
}

TEST_CASE("init is idempotent and shutdown allows re-init", "[core][log]") {
  gsr::log::init();
  gsr::log::init(spdlog::level::debug);
  REQUIRE(gsr::log::get("test_reinit") != nullptr);
  gsr::log::shutdown();
  gsr::log::init();
  REQUIRE(gsr::log::get("test_reinit") != nullptr);
}
