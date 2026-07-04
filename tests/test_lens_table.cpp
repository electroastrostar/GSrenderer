#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

#include "tracking/lens_table.hpp"

using Catch::Approx;
namespace trk = gsr::tracking;
namespace fs = std::filesystem;

namespace {
class TempCsv {
 public:
  explicit TempCsv(const std::string& text) {
    static std::mt19937_64 rng{std::random_device{}()};
    path_ = fs::temp_directory_path() / ("gsr_lens_" + std::to_string(rng()) + ".csv");
    std::ofstream(path_) << text;
  }
  ~TempCsv() { std::error_code ec; fs::remove(path_, ec); }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};
}  // namespace

TEST_CASE("lens table interpolates between calibration rows", "[tracking][lens]") {
  TempCsv csv("zoom_raw,focal_mm\n# wide to tele\n0,24.0\n1000,50.0\n2000,100.0\n");
  const auto table = trk::LensTable::from_csv(csv.path());
  CHECK_FALSE(table.is_fixed());
  // Hand-checked: exact rows, midpoints, and clamping at both ends.
  CHECK(table.focal_mm(0) == Approx(24.0f));
  CHECK(table.focal_mm(500) == Approx(37.0f));    // 24 + 0.5*(50-24)
  CHECK(table.focal_mm(1000) == Approx(50.0f));
  CHECK(table.focal_mm(1500) == Approx(75.0f));   // 50 + 0.5*(100-50)
  CHECK(table.focal_mm(5000) == Approx(100.0f));  // clamp high
}

TEST_CASE("lens table sorts unsorted rows", "[tracking][lens]") {
  TempCsv csv("2000,100.0\n0,24.0\n1000,50.0\n");
  const auto table = trk::LensTable::from_csv(csv.path());
  CHECK(table.focal_mm(500) == Approx(37.0f));
}

TEST_CASE("fixed lens fallback ignores zoom", "[tracking][lens]") {
  const auto table = trk::LensTable::fixed(35.0f);
  CHECK(table.is_fixed());
  CHECK(table.focal_mm(0) == Approx(35.0f));
  CHECK(table.focal_mm(999999) == Approx(35.0f));
  CHECK_THROWS_AS(trk::LensTable::fixed(0.0f), std::runtime_error);
}

TEST_CASE("lens table rejects malformed CSVs", "[tracking][lens]") {
  SECTION("missing file") {
    CHECK_THROWS_AS(trk::LensTable::from_csv("/nonexistent/lens.csv"), std::runtime_error);
  }
  SECTION("empty / header only") {
    TempCsv csv("zoom_raw,focal_mm\n");
    CHECK_THROWS_AS(trk::LensTable::from_csv(csv.path()), std::runtime_error);
  }
  SECTION("non-numeric data row") {
    TempCsv csv("0,24.0\nhello,world\n");
    CHECK_THROWS_AS(trk::LensTable::from_csv(csv.path()), std::runtime_error);
  }
  SECTION("missing column") {
    TempCsv csv("0,24.0\n1000\n");
    CHECK_THROWS_AS(trk::LensTable::from_csv(csv.path()), std::runtime_error);
  }
  SECTION("non-positive focal") {
    TempCsv csv("0,0.0\n");
    CHECK_THROWS_AS(trk::LensTable::from_csv(csv.path()), std::runtime_error);
  }
}

TEST_CASE("committed example lens CSV loads", "[tracking][lens]") {
  const auto table =
      trk::LensTable::from_csv(GSR_TEST_ASSETS_DIR "/../configs/example_lens.csv");
  CHECK(table.focal_mm(0) == Approx(24.0f));
  CHECK(table.focal_mm(1048575) == Approx(135.0f));
}

// Hand-checked: 35 mm focal on a 24 mm-high sensor at 1080 px -> fy = 35/24*1080 = 1575.
TEST_CASE("focal_px_from_mm converts via sensor size", "[tracking][lens]") {
  CHECK(trk::focal_px_from_mm(35.0f, 24.0f, 1080) == Approx(1575.0f));
}
