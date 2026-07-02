add_executable(gsr_tests
  test_camera.cpp
  test_fixture_asset.cpp
  test_log.cpp
  test_ply_loader.cpp
  test_splat_data.cpp
  test_version.cpp
)

target_compile_definitions(gsr_tests
  PRIVATE GSR_TEST_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

target_link_libraries(gsr_tests
  PRIVATE gsr_core gsr_loader gsr_warnings Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(gsr_tests)
