#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "renderer/sh.hpp"

using Catch::Approx;
using gsr::renderer::eval_sh_color;

namespace {
// dc-only splat, degree d, all rest coeffs zero except (channel, k) = value.
struct Coeffs {
  std::vector<float> dc{0.0f, 0.0f, 0.0f};
  std::vector<float> rest;
  explicit Coeffs(int degree) : rest(3u * ((degree + 1) * (degree + 1) - 1), 0.0f) {}
};
}  // namespace

// Hand-checked: degree 0 is 0.5 + kSH0 * dc, independent of direction.
TEST_CASE("SH degree 0: color = 0.5 + C0*dc for any direction", "[renderer][sh]") {
  Coeffs c(0);
  c.dc = {1.0f, 0.0f, -10.0f};
  float rgb[3];
  eval_sh_color(0, c.dc.data(), c.rest.data(), 0, 0, 1, rgb);
  CHECK(rgb[0] == Approx(0.5f + 0.28209479f).epsilon(1e-5));  // 0.78209479
  CHECK(rgb[1] == Approx(0.5f));
  CHECK(rgb[2] == Approx(0.0f));  // clamped: 0.5 - 2.82 < 0

  float rgb2[3];
  eval_sh_color(0, c.dc.data(), c.rest.data(), 1, 0, 0, rgb2);
  CHECK(rgb2[0] == Approx(rgb[0]));  // no view dependence at degree 0
}

// Hand-checked degree-1 basis: for dir (0,1,0) only basis[0] = -C1 fires;
// for dir (0,0,1) only basis[1] = +C1.
TEST_CASE("SH degree 1: linear terms fire per axis with reference signs", "[renderer][sh]") {
  Coeffs c(1);
  float rgb[3];

  c.rest[0] = 1.0f;  // R channel, coeff 0 (the -C1*y term)
  eval_sh_color(1, c.dc.data(), c.rest.data(), 0, 1, 0, rgb);
  CHECK(rgb[0] == Approx(0.5f - 0.48860251f).epsilon(1e-5));  // 0.01139749
  eval_sh_color(1, c.dc.data(), c.rest.data(), 0, -1, 0, rgb);
  CHECK(rgb[0] == Approx(0.5f + 0.48860251f).epsilon(1e-5));

  Coeffs cz(1);
  cz.rest[1] = 1.0f;  // R channel, coeff 1 (the +C1*z term)
  eval_sh_color(1, cz.dc.data(), cz.rest.data(), 0, 0, 1, rgb);
  CHECK(rgb[0] == Approx(0.5f + 0.48860251f).epsilon(1e-5));
}

// Hand-checked degree-2: along +z only basis[5] = C2[2]*(2z^2-x^2-y^2) = 2*0.315391565
// = 0.63078313 is nonzero.
TEST_CASE("SH degree 2: quadratic z term along the axis", "[renderer][sh]") {
  Coeffs c(2);
  c.rest[5] = 1.0f;  // R channel, coeff 5
  float rgb[3];
  eval_sh_color(2, c.dc.data(), c.rest.data(), 0, 0, 1, rgb);
  CHECK(rgb[0] == Approx(0.5f + 0.63078313f).epsilon(1e-5));
  // Perpendicular direction flips the sign of the same basis: -(x^2) term.
  eval_sh_color(2, c.dc.data(), c.rest.data(), 1, 0, 0, rgb);
  CHECK(rgb[0] == Approx(0.5f - 0.31539157f).epsilon(1e-5));
}

// Hand-checked degree-3: along +z only basis[11] = C3[3]*z*(2z^2-3x^2-3y^2) = 2*0.37317633
// = 0.74635266 is nonzero.
TEST_CASE("SH degree 3: cubic z term along the axis", "[renderer][sh]") {
  Coeffs c(3);
  c.rest[11] = 1.0f;  // R channel, coeff 11
  float rgb[3];
  eval_sh_color(3, c.dc.data(), c.rest.data(), 0, 0, 1, rgb);
  CHECK(rgb[0] == Approx(0.5f + 0.74635266f).epsilon(1e-5));
  eval_sh_color(3, c.dc.data(), c.rest.data(), 0, 0, -1, rgb);
  CHECK(rgb[0] == Approx(0.0f));  // 0.5 - 0.746... clamps to 0
}

// Channel-major layout: the green channel reads rest[R + k], not interleaved.
TEST_CASE("SH rest coefficients are read channel-major", "[renderer][sh]") {
  Coeffs c(1);  // R = 3 rest coeffs per channel
  c.rest[3 + 1] = 1.0f;  // GREEN channel, coeff 1 (+C1*z)
  float rgb[3];
  eval_sh_color(1, c.dc.data(), c.rest.data(), 0, 0, 1, rgb);
  CHECK(rgb[0] == Approx(0.5f));
  CHECK(rgb[1] == Approx(0.5f + 0.48860251f).epsilon(1e-5));
  CHECK(rgb[2] == Approx(0.5f));
}

// The acceptance property behind "view-dependent color responds to orbit": same splat,
// different view directions, different color.
TEST_CASE("SH color is view dependent for degree >= 1", "[renderer][sh]") {
  Coeffs c(3);
  for (std::size_t i = 0; i < c.rest.size(); ++i) {
    c.rest[i] = 0.05f * static_cast<float>(i % 7);
  }
  float a[3], b[3];
  eval_sh_color(3, c.dc.data(), c.rest.data(), 0, 0, 1, a);
  eval_sh_color(3, c.dc.data(), c.rest.data(), 0.70710678f, 0, -0.70710678f, b);
  CHECK(a[0] != Approx(b[0]).margin(1e-4));
}
