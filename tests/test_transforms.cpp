#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include "core/transforms.hpp"

using Catch::Approx;

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
