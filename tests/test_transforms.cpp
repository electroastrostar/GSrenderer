#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <glm/glm.hpp>

#include "core/camera.hpp"
#include "core/transforms.hpp"

using Catch::Approx;

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

// Hand-checked fixture: COLMAP asset space is y-down. A point 2 units BELOW the asset
// origin (asset y = +2, since asset +Y is down) sits 2 units below the world origin
// (world y = -2). Forward: asset +Z maps to world -Z.
TEST_CASE("world_from_asset flips y and z (COLMAP y-down -> render y-up)",
          "[core][transforms]") {
  const glm::vec3 w = gsr::core::world_from_asset({1.0f, 2.0f, 3.0f});
  CHECK(w.x == Approx(1.0f));
  CHECK(w.y == Approx(-2.0f));
  CHECK(w.z == Approx(-3.0f));
}

TEST_CASE("asset_from_world is the exact inverse (involution)", "[core][transforms]") {
  const glm::vec3 p{-4.0f, 0.5f, 7.25f};
  const glm::vec3 round_trip = gsr::core::asset_from_world(gsr::core::world_from_asset(p));
  CHECK(round_trip.x == Approx(p.x));
  CHECK(round_trip.y == Approx(p.y));
  CHECK(round_trip.z == Approx(p.z));
}

TEST_CASE("rotation matrices match the point transforms and are proper rotations",
          "[core][transforms]") {
  const glm::mat4 m = gsr::core::world_from_asset_rotation();
  const glm::vec3 p{1.0f, 2.0f, 3.0f};
  const glm::vec3 via_matrix = glm::vec3(m * glm::vec4(p, 1.0f));
  const glm::vec3 via_fn = gsr::core::world_from_asset(p);
  CHECK(via_matrix.x == Approx(via_fn.x));
  CHECK(via_matrix.y == Approx(via_fn.y));
  CHECK(via_matrix.z == Approx(via_fn.z));

  // Proper rotation: det = +1 (a mirror here would silently break handedness).
  CHECK(glm::determinant(glm::mat3(m)) == Approx(1.0f));
  // Involution: applying twice is identity.
  const glm::mat4 twice = m * gsr::core::asset_from_world_rotation();
  CHECK(twice[0][0] == Approx(1.0f));
  CHECK(twice[1][1] == Approx(1.0f));
  CHECK(twice[2][2] == Approx(1.0f));
  CHECK(twice[1][0] == Approx(0.0f).margin(1e-6));
}

// ---- render_from_freed (FreeD/StarTracker -> render world) ---------------------------

// Hand-checked position mapping: freed (x,y,z) -> render (-y, z, -x).
// A camera 1.5 m along freed +X, 2 m along +Y, at 0.5 m height lands at (-2, 0.5, -1.5).
TEST_CASE("render_from_freed maps positions with the documented axis convention",
          "[core][transforms][freed]") {
  const auto pose = gsr::core::render_from_freed(0, 0, 0, 1.5f, 2.0f, 0.5f);
  CHECK(pose.position.x == Approx(-2.0f));
  CHECK(pose.position.y == Approx(0.5f));
  CHECK(pose.position.z == Approx(-1.5f));
}

// Hand-checked orientations, derived on paper from the conventions in freed-protocol.md:
//  - pan 0: freed forward +X = render -Z (identity camera)
//  - pan +90 deg (clockwise from above): freed forward -Y = render +X
//  - tilt +45 deg: forward halfway between -Z and +Y
TEST_CASE("render_from_freed orientation fixtures", "[core][transforms][freed]") {
  SECTION("pan 0 is the render identity forward") {
    const auto pose = gsr::core::render_from_freed(0, 0, 0, 0, 0, 0);
    const auto fwd = gsr::core::forward_world(pose);
    CHECK(fwd.x == Approx(0.0f).margin(1e-6));
    CHECK(fwd.y == Approx(0.0f).margin(1e-6));
    CHECK(fwd.z == Approx(-1.0f).epsilon(1e-6));
  }
  SECTION("pan +90 deg turns clockwise seen from above") {
    const auto pose = gsr::core::render_from_freed(kPi / 2, 0, 0, 0, 0, 0);
    const auto fwd = gsr::core::forward_world(pose);
    CHECK(fwd.x == Approx(1.0f).epsilon(1e-5));
    CHECK(fwd.y == Approx(0.0f).margin(1e-5));
    CHECK(fwd.z == Approx(0.0f).margin(1e-5));
  }
  SECTION("tilt +45 deg looks up") {
    const auto pose = gsr::core::render_from_freed(0, kPi / 4, 0, 0, 0, 0);
    const auto fwd = gsr::core::forward_world(pose);
    const float inv_sqrt2 = 1.0f / std::sqrt(2.0f);
    CHECK(fwd.y == Approx(inv_sqrt2).epsilon(1e-5));
    CHECK(fwd.z == Approx(-inv_sqrt2).epsilon(1e-5));
  }
  SECTION("roll spins about the view axis without moving forward") {
    const auto pose = gsr::core::render_from_freed(0, 0, 0.3f, 0, 0, 0);
    const auto fwd = gsr::core::forward_world(pose);
    CHECK(fwd.z == Approx(-1.0f).epsilon(1e-5));
    // Hand-derived: the behind-the-camera viewer faces -Z with right = +X, so a
    // clockwise (positive) roll tips the up vector toward +X (12 -> 3 o'clock).
    CHECK(gsr::core::up_world(pose).x > 0.0f);
  }
}

// Consistency with the simulator's orbit math (tools/freed_simulator): a camera on the
// orbit at angle theta, pan = pi - theta, tilt = -atan2(h, r), must look STRAIGHT AT the
// render-world origin from anywhere on the circle.
TEST_CASE("render_from_freed: simulator orbit poses face the origin",
          "[core][transforms][freed]") {
  const float r = 3.0f, h = 1.5f;
  for (const float theta : {0.0f, 0.6f, kPi / 2, 2.5f, kPi, 4.2f}) {
    const float pan = kPi - theta;
    const float tilt = -std::atan2(h, r);
    const auto pose = gsr::core::render_from_freed(pan, tilt, 0, r * std::cos(theta),
                                                   r * std::sin(theta), h);
    const auto fwd = gsr::core::forward_world(pose);
    const auto expected = glm::normalize(-pose.position);  // toward the origin
    CHECK(fwd.x == Approx(expected.x).margin(1e-4));
    CHECK(fwd.y == Approx(expected.y).margin(1e-4));
    CHECK(fwd.z == Approx(expected.z).margin(1e-4));
  }
}
