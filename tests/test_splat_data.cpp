#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "loader/splat_data.hpp"

using gsr::loader::SplatData;

TEST_CASE("rest_coeffs_for_degree matches (d+1)^2 - 1", "[loader]") {
  CHECK(gsr::loader::rest_coeffs_for_degree(0) == 0);
  CHECK(gsr::loader::rest_coeffs_for_degree(1) == 3);
  CHECK(gsr::loader::rest_coeffs_for_degree(2) == 8);
  CHECK(gsr::loader::rest_coeffs_for_degree(3) == 15);
  CHECK_THROWS_AS(gsr::loader::rest_coeffs_for_degree(-1), std::invalid_argument);
  CHECK_THROWS_AS(gsr::loader::rest_coeffs_for_degree(4), std::invalid_argument);
}

TEST_CASE("degree_from_rest_property_count inverts the property counts", "[loader]") {
  CHECK(gsr::loader::degree_from_rest_property_count(0) == 0);
  CHECK(gsr::loader::degree_from_rest_property_count(9) == 1);
  CHECK(gsr::loader::degree_from_rest_property_count(24) == 2);
  CHECK(gsr::loader::degree_from_rest_property_count(45) == 3);
  CHECK_THROWS_AS(gsr::loader::degree_from_rest_property_count(7), std::invalid_argument);
  CHECK_THROWS_AS(gsr::loader::degree_from_rest_property_count(46), std::invalid_argument);
}

namespace {
SplatData two_splats() {
  SplatData d;
  d.count = 2;
  d.sh_degree = 0;
  d.position = {-1.0f, 2.0f, 0.5f, 3.0f, -4.0f, 0.25f};
  d.scale = {1, 1, 1, 1, 1, 1};
  d.rotation = {1, 0, 0, 0, 1, 0, 0, 0};
  d.opacity = {0.5f, 1.0f};
  d.sh_dc = {0, 0, 0, 0, 0, 0};
  return d;
}
}  // namespace

TEST_CASE("compute_bounds finds the axis-aligned min/max", "[loader]") {
  const auto b = gsr::loader::compute_bounds(two_splats());
  CHECK(b.min[0] == Catch::Approx(-1.0f));
  CHECK(b.min[1] == Catch::Approx(-4.0f));
  CHECK(b.min[2] == Catch::Approx(0.25f));
  CHECK(b.max[0] == Catch::Approx(3.0f));
  CHECK(b.max[1] == Catch::Approx(2.0f));
  CHECK(b.max[2] == Catch::Approx(0.5f));
}

TEST_CASE("compute_bounds rejects empty data", "[loader]") {
  CHECK_THROWS_AS(gsr::loader::compute_bounds(SplatData{}), std::invalid_argument);
}

// Robust bounds must ignore far outliers — the PR #3 preview bug: a handful of sky
// splats hundreds of units out must not blow up the camera framing.
TEST_CASE("compute_robust_bounds ignores outlier splats", "[loader]") {
  SplatData d;
  d.sh_degree = 0;
  // 99 splats clustered in [-1,1]^3, one outlier at 1000.
  for (int i = 0; i < 99; ++i) {
    const float v = -1.0f + 2.0f * static_cast<float>(i) / 98.0f;
    d.position.push_back(v);
    d.position.push_back(-v);
    d.position.push_back(0.5f * v);
  }
  d.position.insert(d.position.end(), {1000.0f, 1000.0f, 1000.0f});
  d.count = 100;

  const auto raw = gsr::loader::compute_bounds(d);
  const auto robust = gsr::loader::compute_robust_bounds(d, 0.05f, 0.95f);
  CHECK(raw.max[0] == Catch::Approx(1000.0f));      // raw sees the outlier...
  CHECK(robust.max[0] < 1.01f);                     // ...robust does not
  CHECK(robust.min[0] > -1.01f);
  CHECK(robust.max[0] > 0.7f);                      // but still spans the cluster
}

TEST_CASE("compute_robust_bounds rejects bad input", "[loader]") {
  CHECK_THROWS_AS(gsr::loader::compute_robust_bounds(SplatData{}), std::invalid_argument);
  CHECK_THROWS_AS(gsr::loader::compute_robust_bounds(two_splats(), 0.9f, 0.1f),
                  std::invalid_argument);
}

TEST_CASE("byte_size sums all arrays", "[loader]") {
  // 2 splats, degree 0: (3+3+4+1+3+0) floats each = 14 floats = 56 bytes per splat.
  CHECK(gsr::loader::byte_size(two_splats()) == 2 * 14 * sizeof(float));
  CHECK(gsr::loader::byte_size(SplatData{}) == 0);
}
