// Pins the committed fixture (assets/fixtures/cube_deg3.ply) to its hand-derived values —
// the same numbers docs/verification/phase-1.md tells the operator to expect from
// asset_inspector. Regenerate the fixture with tools/make_fixture_ply.py.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "loader/ply_loader.hpp"
#include "loader/splat_data.hpp"

TEST_CASE("committed cube_deg3 fixture loads with known values", "[loader][fixture]") {
  const auto data = gsr::loader::load_ply(GSR_TEST_ASSETS_DIR "/fixtures/cube_deg3.ply");

  REQUIRE(data.count == 8);
  CHECK(data.sh_degree == 3);
  CHECK(gsr::loader::byte_size(data) == 8 * 236);  // 1888 B = 1.8 KiB

  const auto b = gsr::loader::compute_bounds(data);
  for (int axis = 0; axis < 3; ++axis) {
    CHECK(b.min[axis] == Catch::Approx(-1.0f));
    CHECK(b.max[axis] == Catch::Approx(1.0f));
  }

  for (std::size_t i = 0; i < data.count; ++i) {
    CHECK(data.opacity[i] == Catch::Approx(0.5f));            // sigmoid(0)
    CHECK(data.scale[3 * i] == Catch::Approx(0.05f));         // exp(ln 0.05)
    CHECK(data.rotation[4 * i] == Catch::Approx(1.0f));       // identity quat
    CHECK(data.sh_dc[3 * i] == Catch::Approx(0.5f * (i + 1)));
    CHECK(data.sh_rest[45 * i] == Catch::Approx(0.1f * i));   // f_rest_0 per splat
  }
}

// Phase 4 fiducial: pins the committed grid asset to its generator's values (regenerate
// with tools/make_fiducial_ply.py). The verification doc's pixel measurements assume
// exactly this geometry.
TEST_CASE("committed grid_fiducial fixture loads with known values", "[loader][fixture]") {
  const auto data = gsr::loader::load_ply(GSR_TEST_ASSETS_DIR "/fixtures/grid_fiducial.ply");
  REQUIRE(data.count == 1463);
  CHECK(data.sh_degree == 0);
  const auto b = gsr::loader::compute_bounds(data);
  CHECK(b.min[0] == Catch::Approx(-2.0f));
  CHECK(b.max[0] == Catch::Approx(2.0f));
  CHECK(b.min[2] == Catch::Approx(0.0f).margin(1e-6));  // flat wall at asset z=0
  CHECK(b.max[2] == Catch::Approx(0.0f).margin(1e-6));
  // Opaque grid: every opacity ~sigmoid(6).
  CHECK(data.opacity[0] == Catch::Approx(0.9975274f).epsilon(1e-4));
}
