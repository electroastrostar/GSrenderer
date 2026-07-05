# Config parsing is its own lib so tests link it without dragging in the app/CUDA side.
add_library(gsr_config STATIC
  config.cpp
)
target_include_directories(gsr_config PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gsr_config
  PUBLIC  tomlplusplus::tomlplusplus
  PRIVATE gsr_warnings)

add_executable(splatcast
  main.cpp
)

target_link_libraries(splatcast
  PRIVATE gsr_config gsr_core gsr_loader gsr_warnings)

if(GSR_CUDA_ENABLED)
  find_package(OpenGL REQUIRED)
  target_sources(splatcast PRIVATE preview.cpp)
  target_link_libraries(splatcast
    PRIVATE gsr_renderer gsr_tracking glfw OpenGL::GL CUDA::cudart)
endif()
