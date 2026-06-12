add_executable(gsr_tests
  test_log.cpp
  test_version.cpp
)

target_link_libraries(gsr_tests
  PRIVATE gsr_core gsr_warnings Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(gsr_tests)
