#pragma once

#include "loader/splat_data.hpp"

#include <filesystem>

namespace gsr::loader {

// Loads an INRIA-format 3DGS binary PLY (binary_little_endian 1.0) from disk:
// x/y/z, scale_0..2 (log), rot_0..3 (wxyz quat), opacity (logit), f_dc_0..2, f_rest_*.
// SH degree is detected from the f_rest_* property count (degrees 0..3 supported).
// Activations are applied at load: exp(scale), sigmoid(opacity), quaternion normalization.
// Property order in the file is arbitrary; unknown scalar properties (e.g. nx/ny/nz) are
// skipped. Throws std::runtime_error on malformed or truncated input (load-time only —
// see CLAUDE.md error policy).
SplatData load_ply(const std::filesystem::path& path);

}  // namespace gsr::loader
