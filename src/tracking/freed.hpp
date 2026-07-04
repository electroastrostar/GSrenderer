#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// FreeD D1 packet codec. Wire layout and conventions: docs/freed-protocol.md.
namespace gsr::tracking {

inline constexpr std::size_t kFreedPacketSize = 29;
inline constexpr std::uint8_t kFreedMessageType = 0xD1;

// A decoded D1 message in INTERNAL units (radians, meters — converted at parse time).
// Axes/signs are FreeD-space (Z up); the render-space mapping is core/transforms.hpp.
struct FreedPose {
  std::uint8_t camera_id = 0;
  float pan_rad = 0.0f;
  float tilt_rad = 0.0f;
  float roll_rad = 0.0f;
  float x_m = 0.0f;
  float y_m = 0.0f;
  float z_m = 0.0f;
  std::uint32_t zoom_raw = 0;   // opaque encoder counts (lens table domain)
  std::uint32_t focus_raw = 0;  // opaque encoder counts
};

// Checksum over the first `count` bytes: (0x40 - sum) mod 256.
std::uint8_t freed_checksum(std::span<const std::uint8_t> bytes, std::size_t count);

// Parses a 29-byte D1 packet, CONVERTING WIRE UNITS (degrees/32768, mm/64) to
// radians/meters. Returns nullopt on wrong size, wrong type, or checksum mismatch.
std::optional<FreedPose> parse_freed_d1(std::span<const std::uint8_t> bytes);

// Inverse of parse_freed_d1 (radians/meters -> wire units), used by the simulator.
// Values outside the wire's int24 range are clamped.
std::array<std::uint8_t, kFreedPacketSize> serialize_freed_d1(const FreedPose& pose);

}  // namespace gsr::tracking
