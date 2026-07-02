add_executable(asset_inspector
  main.cpp
)

target_link_libraries(asset_inspector
  PRIVATE gsr_loader gsr_warnings)
