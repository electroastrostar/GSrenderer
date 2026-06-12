#include <cstdio>

#include "core/version.hpp"

int main() {
  std::printf("splatcast %.*s (CUDA: %s)\n",
              static_cast<int>(gsr::core::version().size()), gsr::core::version().data(),
              gsr::core::cuda_enabled() ? "enabled" : "disabled");
  return 0;
}
