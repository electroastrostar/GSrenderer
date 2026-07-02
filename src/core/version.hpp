#pragma once

#include <string_view>

namespace gsr::core {

// Project version, e.g. "0.1.0". Bumped per plan §7 (v0.x phases 0-4, v1.0 at Phase 5).
std::string_view version();

// True when this binary was built with the CUDA toolchain (GPU targets available).
bool cuda_enabled();

}  // namespace gsr::core
