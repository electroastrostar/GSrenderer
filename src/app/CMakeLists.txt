add_executable(splatcast
  main.cpp
)

target_link_libraries(splatcast
  PRIVATE gsr_core gsr_warnings)
