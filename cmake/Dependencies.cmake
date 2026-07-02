# Third-party dependencies, pinned to exact tags. Network needed on first configure only.
include(FetchContent)

FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.14.1
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(spdlog)

if(GSR_BUILD_TESTS)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.0
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()
