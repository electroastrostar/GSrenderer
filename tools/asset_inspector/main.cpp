// asset_inspector — dump splat count, SH degree, bounds, and memory footprint of a 3DGS PLY.
// Plain stdout is the product here (CLI tool), so no gsr::log.

#include "loader/ply_loader.hpp"
#include "loader/splat_data.hpp"

#include <cstdio>
#include <exception>

namespace {

void print_bytes(const char* label, double bytes) {
  const char* units[] = {"B", "KiB", "MiB", "GiB"};
  int u = 0;
  while (bytes >= 1024.0 && u < 3) {
    bytes /= 1024.0;
    ++u;
  }
  std::printf("%s%.1f %s (CPU == GPU upload size)\n", label, bytes, units[u]);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr,
                 "usage: asset_inspector <asset.ply>\n"
                 "Prints splat count, SH degree, bounding box and memory footprint\n"
                 "of an INRIA-format 3D Gaussian Splatting PLY.\n");
    return 2;
  }

  try {
    const auto data = gsr::loader::load_ply(argv[1]);

    std::printf("asset:      %s\n", argv[1]);
    std::printf("splats:     %zu\n", data.count);
    std::printf("SH degree:  %d (%d rest + 3 DC coefficients per splat)\n", data.sh_degree,
                3 * gsr::loader::rest_coeffs_for_degree(data.sh_degree));
    if (data.count > 0) {
      const auto b = gsr::loader::compute_bounds(data);
      std::printf("bounds min: [%9.3f, %9.3f, %9.3f] m (asset space)\n", b.min[0], b.min[1],
                  b.min[2]);
      std::printf("bounds max: [%9.3f, %9.3f, %9.3f] m (asset space)\n", b.max[0], b.max[1],
                  b.max[2]);
    } else {
      std::printf("bounds:     n/a (no splats)\n");
    }
    print_bytes("memory:     ", static_cast<double>(gsr::loader::byte_size(data)));
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
}
