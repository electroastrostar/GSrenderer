add_executable(splatcast
  main.cpp
)

target_link_libraries(splatcast
  PRIVATE gsr_core gsr_loader gsr_warnings)

if(GSR_CUDA_ENABLED)
  find_package(OpenGL REQUIRED)
  target_sources(splatcast PRIVATE preview.cpp)
  target_link_libraries(splatcast
    PRIVATE gsr_renderer gsr_tracking glfw OpenGL::GL CUDA::cudart)
endif()
