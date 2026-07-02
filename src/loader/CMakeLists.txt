add_library(gsr_loader STATIC
  ply_loader.cpp
  splat_data.cpp
)

target_include_directories(gsr_loader PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gsr_loader PRIVATE gsr_warnings)
