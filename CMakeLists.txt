cmake_minimum_required(VERSION 3.24)

project(splatcast
  VERSION 0.1.0
  DESCRIPTION "Real-time Gaussian splat renderer for ICVFX (NDI / SMPTE 2110)"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release for single-config generators.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)
endif()

# --- CUDA: detected, not required (see CLAUDE.md). Rasterization core (Phase 2+) needs it. ---
include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
  # Must be set BEFORE enable_language, or CMake pins its own default (52).
  if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 86) # RTX A6000 (Ampere)
  endif()
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  set(GSR_CUDA_ENABLED ON)
  find_package(CUDAToolkit REQUIRED)  # cudart for host-side interop code
  message(STATUS "splatcast: CUDA ${CMAKE_CUDA_COMPILER_VERSION} found "
                 "(arch ${CMAKE_CUDA_ARCHITECTURES}) — GPU targets enabled")
else()
  set(GSR_CUDA_ENABLED OFF)
  message(WARNING "splatcast: no CUDA toolchain found — building CPU-only targets. "
                  "The rasterization core (Phase 2+) requires CUDA.")
endif()

option(GSR_BUILD_TESTS "Build unit tests" ON)

# Warning flags for our own targets only (not dependencies). Guarded per-language so CUDA
# sources get them via -Xcompiler instead of choking nvcc.
add_library(gsr_warnings INTERFACE)
if(MSVC)
  target_compile_options(gsr_warnings INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:/W4>
    $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/W4>)
else()
  target_compile_options(gsr_warnings INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>
    $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-Wall,-Wextra>)
endif()

include(cmake/Dependencies.cmake)

add_subdirectory(src/core)
add_subdirectory(src/loader)
add_subdirectory(src/tracking)
add_subdirectory(src/renderer)
add_subdirectory(src/app)
add_subdirectory(tools/asset_inspector)

if(GSR_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
