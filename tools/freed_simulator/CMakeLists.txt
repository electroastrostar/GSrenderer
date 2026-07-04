add_executable(freed_simulator
  main.cpp
)

target_link_libraries(freed_simulator
  PRIVATE gsr_tracking gsr_warnings)
