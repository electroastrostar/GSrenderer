/* COMPILE-CHECK STUB of the NDI SDK header — NOT the real SDK.
 *
 * Purpose: lets CI/containers without the proprietary NDI SDK compile-check
 * src/output/ndi/ndi_sender.cpp against the exact API surface it uses. Selected with
 * -DGSR_NDI_STUB=ON only when the real SDK is absent.
 *
 * HONESTY GUARANTEE: NDIlib_initialize() returns false here, so a stub build can never
 * pretend to stream (plan §7: never fake a pass). The real SDK is at https://ndi.video.
 * Declarations mirror the public NDI 5/6 SDK API (names/ABI subset only).
 */
#pragma once

#include <cstdint>
#include <cstddef>

using NDIlib_send_instance_t = void*;

enum NDIlib_FourCC_video_type_e : std::uint32_t {
  // 'RGBA' / 'BGRA' little-endian fourcc codes, as in the real SDK.
  NDIlib_FourCC_type_RGBA = 0x41424752,
  NDIlib_FourCC_type_BGRA = 0x41524742,
};

enum NDIlib_frame_format_type_e : std::uint32_t {
  NDIlib_frame_format_type_progressive = 1,
};

struct NDIlib_send_create_t {
  const char* p_ndi_name = nullptr;
  const char* p_groups = nullptr;
  bool clock_video = false;
  bool clock_audio = false;
};

struct NDIlib_video_frame_v2_t {
  int xres = 0, yres = 0;
  NDIlib_FourCC_video_type_e FourCC = NDIlib_FourCC_type_RGBA;
  int frame_rate_N = 30000, frame_rate_D = 1001;
  float picture_aspect_ratio = 0.0f;
  NDIlib_frame_format_type_e frame_format_type = NDIlib_frame_format_type_progressive;
  std::int64_t timecode = 0;  // 100 ns units
  std::uint8_t* p_data = nullptr;
  int line_stride_in_bytes = 0;
  const char* p_metadata = nullptr;
  std::int64_t timestamp = 0;
};

inline bool NDIlib_initialize() { return false; }  // stub: never claims to work
inline void NDIlib_destroy() {}
inline NDIlib_send_instance_t NDIlib_send_create(const NDIlib_send_create_t*) {
  return nullptr;
}
inline void NDIlib_send_destroy(NDIlib_send_instance_t) {}
inline void NDIlib_send_send_video_v2(NDIlib_send_instance_t,
                                      const NDIlib_video_frame_v2_t*) {}
