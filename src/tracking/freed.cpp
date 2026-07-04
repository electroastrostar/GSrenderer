#include "tracking/freed.hpp"

#include <cmath>

namespace gsr::tracking {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegPerUnit = 1.0f / 32768.0f;  // wire angle: degrees * 32768
constexpr float kMmPerUnit = 1.0f / 64.0f;      // wire position: mm * 64

std::int32_t read_i24(const std::uint8_t* b) {
  std::int32_t v = (static_cast<std::int32_t>(b[0]) << 16) |
                   (static_cast<std::int32_t>(b[1]) << 8) | static_cast<std::int32_t>(b[2]);
  if (v & 0x800000) v -= 0x1000000;  // sign-extend two's complement
  return v;
}

std::uint32_t read_u24(const std::uint8_t* b) {
  return (static_cast<std::uint32_t>(b[0]) << 16) | (static_cast<std::uint32_t>(b[1]) << 8) |
         static_cast<std::uint32_t>(b[2]);
}

void write_i24(std::uint8_t* b, std::int64_t v) {
  if (v > 0x7FFFFF) v = 0x7FFFFF;
  if (v < -0x800000) v = -0x800000;
  const auto u = static_cast<std::uint32_t>(v & 0xFFFFFF);
  b[0] = static_cast<std::uint8_t>((u >> 16) & 0xFF);
  b[1] = static_cast<std::uint8_t>((u >> 8) & 0xFF);
  b[2] = static_cast<std::uint8_t>(u & 0xFF);
}

void write_u24(std::uint8_t* b, std::uint32_t v) {
  if (v > 0xFFFFFF) v = 0xFFFFFF;
  b[0] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
  b[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
  b[2] = static_cast<std::uint8_t>(v & 0xFF);
}

float wire_angle_to_rad(std::int32_t raw) {
  return static_cast<float>(raw) * kDegPerUnit * kPi / 180.0f;
}

float wire_pos_to_m(std::int32_t raw) { return static_cast<float>(raw) * kMmPerUnit / 1000.0f; }

std::int64_t rad_to_wire_angle(float rad) {
  // Angles are periodic: wrap into [-180, 180) instead of clamping at the int24 range
  // (±256°). A continuously growing angle — e.g. the simulator's endless orbit pan —
  // would otherwise hit the clamp and freeze while positions keep moving (PR #4).
  double deg = static_cast<double>(rad) * 180.0 / kPi;
  deg = std::fmod(deg + 180.0, 360.0);
  if (deg < 0.0) deg += 360.0;
  deg -= 180.0;
  return static_cast<std::int64_t>(std::llround(deg * 32768.0));
}

std::int64_t m_to_wire_pos(float m) {
  return static_cast<std::int64_t>(std::llround(static_cast<double>(m) * 1000.0 * 64.0));
}

}  // namespace

std::uint8_t freed_checksum(std::span<const std::uint8_t> bytes, std::size_t count) {
  std::uint32_t sum = 0;
  for (std::size_t i = 0; i < count && i < bytes.size(); ++i) sum += bytes[i];
  return static_cast<std::uint8_t>((0x40u - sum) & 0xFFu);
}

std::optional<FreedPose> parse_freed_d1(std::span<const std::uint8_t> bytes) {
  if (bytes.size() != kFreedPacketSize) return std::nullopt;
  if (bytes[0] != kFreedMessageType) return std::nullopt;
  if (freed_checksum(bytes, kFreedPacketSize - 1) != bytes[kFreedPacketSize - 1]) {
    return std::nullopt;
  }

  FreedPose pose;
  pose.camera_id = bytes[1];
  pose.pan_rad = wire_angle_to_rad(read_i24(&bytes[2]));
  pose.tilt_rad = wire_angle_to_rad(read_i24(&bytes[5]));
  pose.roll_rad = wire_angle_to_rad(read_i24(&bytes[8]));
  pose.x_m = wire_pos_to_m(read_i24(&bytes[11]));
  pose.y_m = wire_pos_to_m(read_i24(&bytes[14]));
  pose.z_m = wire_pos_to_m(read_i24(&bytes[17]));
  pose.zoom_raw = read_u24(&bytes[20]);
  pose.focus_raw = read_u24(&bytes[23]);
  return pose;
}

std::array<std::uint8_t, kFreedPacketSize> serialize_freed_d1(const FreedPose& pose) {
  std::array<std::uint8_t, kFreedPacketSize> out{};
  out[0] = kFreedMessageType;
  out[1] = pose.camera_id;
  write_i24(&out[2], rad_to_wire_angle(pose.pan_rad));
  write_i24(&out[5], rad_to_wire_angle(pose.tilt_rad));
  write_i24(&out[8], rad_to_wire_angle(pose.roll_rad));
  write_i24(&out[11], m_to_wire_pos(pose.x_m));
  write_i24(&out[14], m_to_wire_pos(pose.y_m));
  write_i24(&out[17], m_to_wire_pos(pose.z_m));
  write_u24(&out[20], pose.zoom_raw);
  write_u24(&out[23], pose.focus_raw);
  out[26] = 0;
  out[27] = 0;
  out[28] = freed_checksum(out, kFreedPacketSize - 1);
  return out;
}

}  // namespace gsr::tracking
