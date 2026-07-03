#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "renderer/covariance.hpp"

using Catch::Approx;

namespace {
constexpr float kIdentityRows[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
}

// Hand-checked: identity rotation, unit scale -> Σ = I.
TEST_CASE("covariance_3d: identity quat + unit scale gives identity", "[renderer][cov]") {
  const float s[3] = {1, 1, 1};
  const float q[4] = {1, 0, 0, 0};
  float cov[6];
  gsr::renderer::covariance_3d(s, q, cov);
  CHECK(cov[0] == Approx(1.0f));
  CHECK(cov[1] == Approx(0.0f).margin(1e-6));
  CHECK(cov[2] == Approx(0.0f).margin(1e-6));
  CHECK(cov[3] == Approx(1.0f));
  CHECK(cov[4] == Approx(0.0f).margin(1e-6));
  CHECK(cov[5] == Approx(1.0f));
}

// Hand-checked on paper: scale (2,1,1) rotated 90° about Z moves the long axis onto Y:
// R = [[0,-1,0],[1,0,0],[0,0,1]], M = R diag(2,1,1), Σ = M Mᵀ = diag(1,4,1).
TEST_CASE("covariance_3d: 90 deg Z rotation moves the long axis onto Y", "[renderer][cov]") {
  const float inv_sqrt2 = 1.0f / std::sqrt(2.0f);
  const float s[3] = {2, 1, 1};
  const float q[4] = {inv_sqrt2, 0, 0, inv_sqrt2};  // 90° about Z, wxyz
  float cov[6];
  gsr::renderer::covariance_3d(s, q, cov);
  CHECK(cov[0] == Approx(1.0f).epsilon(1e-5));
  CHECK(cov[1] == Approx(0.0f).margin(1e-5));
  CHECK(cov[2] == Approx(0.0f).margin(1e-5));
  CHECK(cov[3] == Approx(4.0f).epsilon(1e-5));
  CHECK(cov[4] == Approx(0.0f).margin(1e-5));
  CHECK(cov[5] == Approx(1.0f).epsilon(1e-5));
}

// Hand-checked EWA fixture: Σ = I, on-axis point at depth 10, fx = fy = 100.
// J = [[10,0,0],[0,10,0]] so cov2d = diag(100,100) + 0.3 dilation.
TEST_CASE("project_covariance_ewa: on-axis unit gaussian", "[renderer][cov]") {
  const float cov6[6] = {1, 0, 0, 1, 0, 1};
  float cov2d[3];
  gsr::renderer::project_covariance_ewa(cov6, 0, 0, 10, 100, 100, 1.0f, 1.0f, kIdentityRows,
                                        cov2d);
  CHECK(cov2d[0] == Approx(100.3f).epsilon(1e-5));
  CHECK(cov2d[1] == Approx(0.0f).margin(1e-5));
  CHECK(cov2d[2] == Approx(100.3f).epsilon(1e-5));
}

// Hand-checked off-axis: t = (1,0,10) adds J02 = -fx*tx/tz² = -1, so
// cov2d_00 = 10² + (-1)² + 0.3 = 101.3.
TEST_CASE("project_covariance_ewa: off-axis point picks up the depth-derivative term",
          "[renderer][cov]") {
  const float cov6[6] = {1, 0, 0, 1, 0, 1};
  float cov2d[3];
  gsr::renderer::project_covariance_ewa(cov6, 1, 0, 10, 100, 100, 1.0f, 1.0f, kIdentityRows,
                                        cov2d);
  CHECK(cov2d[0] == Approx(101.3f).epsilon(1e-5));
  CHECK(cov2d[2] == Approx(100.3f).epsilon(1e-5));
}

// Clamp: a point far outside the frustum is pulled to 1.3*tan_fov*z before the Jacobian,
// so widening tan_fov changes the result while the unclamped case is unaffected.
TEST_CASE("project_covariance_ewa clamps to 1.3x the frustum", "[renderer][cov]") {
  const float cov6[6] = {1, 0, 0, 1, 0, 1};
  float clamped[3], wide[3];
  gsr::renderer::project_covariance_ewa(cov6, 1000, 0, 10, 100, 100, 0.5f, 0.5f,
                                        kIdentityRows, clamped);
  gsr::renderer::project_covariance_ewa(cov6, 1000, 0, 10, 100, 100, 5.0f, 5.0f,
                                        kIdentityRows, wide);
  CHECK(clamped[0] != Approx(wide[0]).epsilon(1e-3));
}

// Hand-checked conic: cov2d = [4, 0, 1] -> det 4, conic = [1/4, 0, 1], radius =
// ceil(3*sqrt(4)) = 6.
TEST_CASE("conic_from_cov2d inverts and sizes the splat", "[renderer][cov]") {
  const float cov2d[3] = {4, 0, 1};
  float conic[3], radius = 0;
  REQUIRE(gsr::renderer::conic_from_cov2d(cov2d, conic, &radius));
  CHECK(conic[0] == Approx(0.25f));
  CHECK(conic[1] == Approx(0.0f).margin(1e-6));
  CHECK(conic[2] == Approx(1.0f));
  CHECK(radius == Approx(6.0f));
}

TEST_CASE("conic_from_cov2d rejects degenerate covariance", "[renderer][cov]") {
  const float degenerate[3] = {1, 2, 1};  // det = 1 - 4 < 0
  float conic[3], radius = 0;
  CHECK_FALSE(gsr::renderer::conic_from_cov2d(degenerate, conic, &radius));
}

// The named view->projection-frame adapter flips y and z (CLAUDE.md rule 4 lives here).
TEST_CASE("projection_frame_from_view flips y and z", "[renderer][cov]") {
  float out[3];
  gsr::renderer::projection_frame_from_view(1.0f, 2.0f, -3.0f, out);
  CHECK(out[0] == Approx(1.0f));
  CHECK(out[1] == Approx(-2.0f));
  CHECK(out[2] == Approx(3.0f));
}
