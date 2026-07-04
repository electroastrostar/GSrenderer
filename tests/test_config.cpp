#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

#include "app/config.hpp"

using Catch::Approx;
namespace fs = std::filesystem;

namespace {
class TempToml {
 public:
  explicit TempToml(const std::string& text) {
    static std::mt19937_64 rng{std::random_device{}()};
    path_ = fs::temp_directory_path() / ("gsr_cfg_" + std::to_string(rng()) + ".toml");
    std::ofstream(path_) << text;
  }
  ~TempToml() { std::error_code ec; fs::remove(path_, ec); }
  std::string path() const { return path_.string(); }

 private:
  fs::path path_;
};
}  // namespace

TEST_CASE("config file overlays values; absent fields keep defaults", "[app][config]") {
  TempToml file(R"(
asset = "scene.ply"
[output]
width = 3840
overscan_pct = 12.5
[tracking]
freed_port = 8001
[stage]
yaw_deg = 90.0
offset_m = [1.0, 0.5, -2.0]
)");
  gsr::app::RunConfig cfg;
  gsr::app::apply_config_file(file.path(), &cfg);

  CHECK(cfg.asset == "scene.ply");
  CHECK(cfg.width == 3840);
  CHECK(cfg.height == 1080);  // untouched default
  CHECK(cfg.overscan_pct == Approx(12.5f));
  CHECK(cfg.freed_port == 8001);
  CHECK(cfg.latency_ms == Approx(0.0f));  // untouched default
  CHECK(cfg.stage_yaw_deg == Approx(90.0f));
  CHECK(cfg.stage_offset_m[0] == Approx(1.0f));
  CHECK(cfg.stage_offset_m[2] == Approx(-2.0f));
  CHECK(cfg.flip_scene);  // untouched default
}

TEST_CASE("config file: full example in configs/ parses", "[app][config]") {
  gsr::app::RunConfig cfg;
  gsr::app::apply_config_file(GSR_TEST_ASSETS_DIR "/../configs/example_run.toml", &cfg);
  CHECK(cfg.asset == "assets/fixtures/cube_deg3.ply");
  CHECK(cfg.sensor_height_mm == Approx(24.0f));
}

TEST_CASE("config file rejects malformed input", "[app][config]") {
  gsr::app::RunConfig cfg;
  SECTION("missing file") {
    CHECK_THROWS_AS(gsr::app::apply_config_file("/nonexistent/run.toml", &cfg),
                    std::runtime_error);
  }
  SECTION("syntax error") {
    TempToml file("asset = [unterminated\n");
    CHECK_THROWS_AS(gsr::app::apply_config_file(file.path(), &cfg), std::runtime_error);
  }
  SECTION("wrong type") {
    TempToml file("[output]\nwidth = \"wide\"\n");
    CHECK_THROWS_AS(gsr::app::apply_config_file(file.path(), &cfg), std::runtime_error);
  }
  SECTION("bad stage offset arity") {
    TempToml file("[stage]\noffset_m = [1.0, 2.0]\n");
    CHECK_THROWS_AS(gsr::app::apply_config_file(file.path(), &cfg), std::runtime_error);
  }
}
