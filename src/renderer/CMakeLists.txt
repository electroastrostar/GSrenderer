# The math headers (sh.hpp, covariance.hpp, ewa_frame.hpp) are header-only and CPU-tested
# from gsr_tests everywhere. The rasterizer target itself requires the CUDA toolchain.
if(GSR_CUDA_ENABLED)
  add_library(gsr_renderer STATIC
    readback.cu
    splat_renderer.cu
  )

  # Namespace-scope constexpr tables (sh.hpp) are read from device code.
  target_compile_options(gsr_renderer PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>)

  target_include_directories(gsr_renderer PUBLIC ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(gsr_renderer
    PUBLIC  gsr_core gsr_loader glm::glm
    PRIVATE gsr_warnings)
endif()
