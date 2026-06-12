#include "core/log.hpp"
#include "core/version.hpp"

int main() {
  gsr::log::init();
  auto log = gsr::log::get("app");
  log->info("splatcast {} starting (CUDA: {})", gsr::core::version(),
            gsr::core::cuda_enabled() ? "enabled" : "disabled");
  log->info("scaffolding only — nothing to render yet (Phase 0)");
  gsr::log::shutdown();
  return 0;
}
