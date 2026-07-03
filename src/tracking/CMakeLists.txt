add_library(gsr_tracking STATIC
  freed.cpp
  pose_predictor.cpp
  udp_listener.cpp
)

target_include_directories(gsr_tracking PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gsr_tracking
  PUBLIC  gsr_core
  PRIVATE gsr_warnings)

if(WIN32)
  target_link_libraries(gsr_tracking PUBLIC ws2_32)
endif()
