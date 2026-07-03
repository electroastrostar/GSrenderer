add_executable(gsr_tests
  test_camera.cpp
  test_covariance.cpp
  test_ewa_frame.cpp
  test_fixture_asset.cpp
  test_freed.cpp
  test_log.cpp
  test_ply_loader.cpp
  test_pose_predictor.cpp
  test_sh.cpp
  test_splat_data.cpp
  test_transforms.cpp
  test_udp_listener.cpp
  test_version.cpp
)

if(GSR_CUDA_ENABLED)
  # Hidden "[.gpu]" tests: built only with CUDA, run explicitly (see docs/verification).
  target_sources(gsr_tests PRIVATE test_renderer_gpu.cpp)
  target_link_libraries(gsr_tests PRIVATE gsr_renderer)
endif()

target_compile_definitions(gsr_tests
  PRIVATE GSR_TEST_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

target_link_libraries(gsr_tests
  PRIVATE gsr_core gsr_loader gsr_tracking gsr_warnings Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(gsr_tests)
