#include "core/version.hpp"

namespace gsr::core {

std::string_view version() { return "0.1.0"; }

bool cuda_enabled() {
#ifdef GSR_CUDA_ENABLED
  return true;
#else
  return false;
#endif
}

}  // namespace gsr::core
