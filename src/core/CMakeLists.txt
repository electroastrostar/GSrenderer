add_library(gsr_core STATIC
  camera.cpp
  log.cpp
  transforms.cpp
  version.cpp
)

target_include_directories(gsr_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gsr_core
  PUBLIC  spdlog::spdlog glm::glm
  PRIVATE gsr_warnings)

if(GSR_CUDA_ENABLED)
  target_compile_definitions(gsr_core PUBLIC GSR_CUDA_ENABLED=1)
endif()
