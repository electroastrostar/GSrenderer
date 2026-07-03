#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <vector>

#include "tracking/freed.hpp"

using Catch::Approx;
using gsr::tracking::FreedPose;
namespace trk = gsr::tracking;

namespace {
constexpr float kPi = 3.14159265358979323846f;

// Builds a raw packet from hand-assembled field bytes; checksum appended via the helper
// (whose algorithm is itself pinned by a hand-computed case below).
std::array<std::uint8_t, trk::kFreedPacketSize> hand_packet() {
  std::array<std::uint8_t, trk::kFreedPacketSize> p{};
  p[0] = 0xD1;
  p[1] = 7;                             // camera id
  p[2] = 0x2D; p[3] = 0x00; p[4] = 0x00;  // pan  +90 deg  = 90*32768  = 0x2D0000
  p[5] = 0xE9; p[6] = 0x80; p[7] = 0x00;  // tilt -45 deg  = -1474560  = 0xE98000 (2's c.)
  p[8] = 0x00; p[9] = 0x00; p[10] = 0x00; // roll 0
  p[11] = 0x01; p[12] = 0x77; p[13] = 0x00;  // X +1.5 m = 96000 = 0x017700
  p[14] = 0x00; p[15] = 0x00; p[16] = 0x00;  // Y 0
  p[17] = 0xFF; p[18] = 0x83; p[19] = 0x00;  // Z -0.5 m = -32000 = 0xFF8300 (2's c.)
  p[20] = 0x00; p[21] = 0x12; p[22] = 0x34;  // zoom 0x001234
  p[23] = 0x00; p[24] = 0x00; p[25] = 0xFF;  // focus 255
  p[26] = 0; p[27] = 0;
  p[28] = trk::freed_checksum(p, 28);
  return p;
}
}  // namespace

// Hand-computed checksum: bytes {0xD1}, rest zero -> (0x40 - 0xD1) mod 256 = 0x6F.
TEST_CASE("freed_checksum matches the hand-computed spec formula", "[tracking][freed]") {
  std::array<std::uint8_t, 28> bytes{};
  bytes[0] = 0xD1;
  CHECK(trk::freed_checksum(bytes, 28) == 0x6F);
}

// Hand-built byte fixture (values derived on paper from the wire encoding, not from the
// code): pan +90 deg, tilt -45 deg, X +1.5 m, Z -0.5 m, zoom 0x1234, focus 255.
TEST_CASE("parse_freed_d1 decodes a hand-built packet to radians/meters",
          "[tracking][freed]") {
  const auto packet = hand_packet();
  const auto pose = trk::parse_freed_d1(packet);
  REQUIRE(pose.has_value());
  CHECK(pose->camera_id == 7);
  CHECK(pose->pan_rad == Approx(kPi / 2.0f).epsilon(1e-5));
  CHECK(pose->tilt_rad == Approx(-kPi / 4.0f).epsilon(1e-5));
  CHECK(pose->roll_rad == Approx(0.0f).margin(1e-6));
  CHECK(pose->x_m == Approx(1.5f).epsilon(1e-5));
  CHECK(pose->y_m == Approx(0.0f).margin(1e-6));
  CHECK(pose->z_m == Approx(-0.5f).epsilon(1e-5));
  CHECK(pose->zoom_raw == 0x1234u);
  CHECK(pose->focus_raw == 255u);
}

TEST_CASE("serialize/parse round-trips a pose", "[tracking][freed]") {
  FreedPose in;
  in.camera_id = 3;
  in.pan_rad = 1.234f;
  in.tilt_rad = -0.5f;
  in.roll_rad = 0.05f;
  in.x_m = -2.75f;
  in.y_m = 4.5f;
  in.z_m = 1.8f;
  in.zoom_raw = 123456;
  in.focus_raw = 654321;

  const auto bytes = trk::serialize_freed_d1(in);
  const auto out = trk::parse_freed_d1(bytes);
  REQUIRE(out.has_value());
  CHECK(out->camera_id == in.camera_id);
  CHECK(out->pan_rad == Approx(in.pan_rad).epsilon(1e-4));
  CHECK(out->tilt_rad == Approx(in.tilt_rad).epsilon(1e-4));
  CHECK(out->roll_rad == Approx(in.roll_rad).margin(1e-4));
  CHECK(out->x_m == Approx(in.x_m).epsilon(1e-4));
  CHECK(out->y_m == Approx(in.y_m).epsilon(1e-4));
  CHECK(out->z_m == Approx(in.z_m).epsilon(1e-4));
  CHECK(out->zoom_raw == in.zoom_raw);
  CHECK(out->focus_raw == in.focus_raw);
}

TEST_CASE("parse_freed_d1 rejects malformed packets", "[tracking][freed]") {
  const auto good = hand_packet();

  SECTION("wrong size") {
    std::vector<std::uint8_t> shorter(good.begin(), good.end() - 1);
    CHECK_FALSE(trk::parse_freed_d1(shorter).has_value());
    std::vector<std::uint8_t> longer(good.begin(), good.end());
    longer.push_back(0);
    CHECK_FALSE(trk::parse_freed_d1(longer).has_value());
  }
  SECTION("wrong message type") {
    auto bad = good;
    bad[0] = 0xD2;
    bad[28] = trk::freed_checksum(bad, 28);  // valid checksum, still wrong type
    CHECK_FALSE(trk::parse_freed_d1(bad).has_value());
  }
  SECTION("corrupted payload byte breaks the checksum") {
    auto bad = good;
    bad[12] ^= 0xFF;
    CHECK_FALSE(trk::parse_freed_d1(bad).has_value());
  }
  SECTION("corrupted checksum byte") {
    auto bad = good;
    bad[28] ^= 0x01;
    CHECK_FALSE(trk::parse_freed_d1(bad).has_value());
  }
}

TEST_CASE("serialize clamps out-of-range wire values instead of wrapping",
          "[tracking][freed]") {
  FreedPose in;
  in.x_m = 1.0e9f;  // far beyond int24 mm*64 range
  const auto bytes = trk::serialize_freed_d1(in);
  const auto out = trk::parse_freed_d1(bytes);
  REQUIRE(out.has_value());
  CHECK(out->x_m == Approx(0x7FFFFF / 64.0f / 1000.0f).epsilon(1e-4));  // int24 max ≈ 131.07 m
}
