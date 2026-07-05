add_library(gsr_output STATIC
  frame_pacer.cpp
)

target_include_directories(gsr_output PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gsr_output
  PUBLIC  gsr_core
  PRIVATE gsr_warnings)
