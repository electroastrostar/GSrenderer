#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

// Hand-checked conic: cov2d = [4, 0, 1] -> det 4, conic = [1/4, 0, 1]. Radius is the
// alpha-cutoff extent: for opacity 1, r = sqrt(2*ln(255) * lambda_max)
// = sqrt(2*5.54126*4) = 6.658 -> ceil 7.
TEST_CASE("conic_from_cov2d inverts and sizes the splat (opaque)", "[renderer][cov]") {
  const float cov2d[3] = {4, 0, 1};
  float conic[3], radius = 0;
  REQUIRE(gsr::renderer::conic_from_cov2d(cov2d, 1.0f, conic, &radius));
  CHECK(conic[0] == Approx(0.25f));
  CHECK(conic[1] == Approx(0.0f).margin(1e-6));
  CHECK(conic[2] == Approx(1.0f));
  CHECK(radius == Approx(7.0f));
}

// Hand-checked: opacity 0.05 -> cutoff 2*ln(12.75) = 5.0910, r = sqrt(5.0910*4)
// = 4.513 -> ceil 5. Translucent splats get materially smaller binning extents.
TEST_CASE("conic_from_cov2d shrinks the extent for translucent splats", "[renderer][cov]") {
  const float cov2d[3] = {4, 0, 1};
  float conic[3], radius = 0;
  REQUIRE(gsr::renderer::conic_from_cov2d(cov2d, 0.05f, conic, &radius));
  CHECK(radius == Approx(5.0f));
}

TEST_CASE("conic_from_cov2d culls sub-threshold opacity and degenerate covariance",
          "[renderer][cov]") {
  float conic[3], radius = 0;
  const float ok[3] = {4, 0, 1};
  CHECK_FALSE(gsr::renderer::conic_from_cov2d(ok, 0.001f, conic, &radius));  // < 1/255
  const float degenerate[3] = {1, 2, 1};  // det = 1 - 4 < 0
  CHECK_FALSE(gsr::renderer::conic_from_cov2d(degenerate, 1.0f, conic, &radius));
}

// ---- exact tile/splat overlap (binning) ---------------------------------------------

TEST_CASE("tile_overlaps_splat: isotropic hit and miss", "[renderer][cov]") {
  // Unit gaussian (conic [1,0,1]) at pixel (8,8), opacity 1 -> cutoff ln(255) = 5.54,
  // effective radius sqrt(2*5.54) = 3.33 px.
  const float conic[3] = {1, 0, 1};
  const float cutoff = gsr::renderer::power_cutoff(1.0f);
  CHECK(gsr::renderer::tile_overlaps_splat(0, 0, 16, 8.0f, 8.0f, conic, cutoff));
  // Tile (10,0): nearest pixel x=160, 152 px away -> power ~11552 >> cutoff.
  CHECK_FALSE(gsr::renderer::tile_overlaps_splat(10, 0, 16, 8.0f, 8.0f, conic, cutoff));
}

// Hand-checked elongated case: sigma_x = 10, sigma_y = 1 (conic [0.01, 0, 1]) at (8,8).
// Bounding-square binning (radius 34) claims tiles (2,0) AND (2,2); the exact test keeps
// (2,0) — min power at pixel (32,8): 0.5*0.01*24^2 = 2.88 <= 5.54 — and rejects (2,2) —
// min power at (32,32): 2.88 + 0.5*24^2 = 290.9.
TEST_CASE("tile_overlaps_splat: elongated splat keeps edge tile, drops corner tile",
          "[renderer][cov]") {
  const float conic[3] = {0.01f, 0, 1};
  const float cutoff = gsr::renderer::power_cutoff(1.0f);
  CHECK(gsr::renderer::tile_overlaps_splat(2, 0, 16, 8.0f, 8.0f, conic, cutoff));
  CHECK_FALSE(gsr::renderer::tile_overlaps_splat(2, 2, 16, 8.0f, 8.0f, conic, cutoff));
}

// Structural invariant behind the CUDA offsets buffer: counting pass and emission pass
// must agree exactly. Replicates both kernel loops (same rect, same helper) over a mix
// of splats and asserts count == emitted.
TEST_CASE("tile overlap count equals emission count (offsets-parity guard)",
          "[renderer][cov]") {
  constexpr int kTile = 16;
  constexpr int kGrid = 8;  // 128x128 px image
  struct Splat {
    float u, v, opacity;
    float conic[3];
  };
  const Splat splats[] = {
      {64.0f, 64.0f, 1.0f, {0.01f, 0.0f, 1.0f}},     // elongated, center
      {8.0f, 8.0f, 0.05f, {1.0f, 0.0f, 1.0f}},       // small, translucent, corner
      {120.0f, 4.0f, 0.8f, {0.02f, 0.015f, 0.04f}},  // rotated anisotropic, edge
      {-20.0f, 130.0f, 1.0f, {0.5f, 0.0f, 0.5f}},    // center off-screen
  };
  for (const auto& s : splats) {
    float conic_out[3], radius = 0;
    const float cov2d[3] = {s.conic[2] / (s.conic[0] * s.conic[2] - s.conic[1] * s.conic[1]),
                            -s.conic[1] / (s.conic[0] * s.conic[2] - s.conic[1] * s.conic[1]),
                            s.conic[0] / (s.conic[0] * s.conic[2] - s.conic[1] * s.conic[1])};
    if (!gsr::renderer::conic_from_cov2d(cov2d, s.opacity, conic_out, &radius)) continue;
    const float cutoff = gsr::renderer::power_cutoff(s.opacity);

    const auto rect_edge = [&](float value, float extent) {
      return std::min(kGrid, std::max(0, static_cast<int>((value + extent) / kTile)));
    };
    const int x_min = rect_edge(s.u, -radius);
    const int x_max = rect_edge(s.u, radius + kTile - 1.0f);
    const int y_min = rect_edge(s.v, -radius);
    const int y_max = rect_edge(s.v, radius + kTile - 1.0f);

    int counted = 0;
    for (int ty = y_min; ty < y_max; ++ty) {
      for (int tx = x_min; tx < x_max; ++tx) {
        if (gsr::renderer::tile_overlaps_splat(tx, ty, kTile, s.u, s.v, conic_out, cutoff)) {
          ++counted;
        }
      }
    }
    int emitted = 0;
    for (int ty = y_min; ty < y_max; ++ty) {
      for (int tx = x_min; tx < x_max; ++tx) {
        if (!gsr::renderer::tile_overlaps_splat(tx, ty, kTile, s.u, s.v, conic_out, cutoff)) {
          continue;
        }
        ++emitted;
      }
    }
    CHECK(counted == emitted);
  }
}

// The named view->projection-frame adapter flips y and z (CLAUDE.md rule 4 lives here).
TEST_CASE("projection_frame_from_view flips y and z", "[renderer][cov]") {
  float out[3];
  gsr::renderer::projection_frame_from_view(1.0f, 2.0f, -3.0f, out);
  CHECK(out[0] == Approx(1.0f));
  CHECK(out[1] == Approx(-2.0f));
  CHECK(out[2] == Approx(3.0f));
}
