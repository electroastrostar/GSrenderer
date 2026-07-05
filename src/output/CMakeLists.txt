add_library(gsr_output STATIC
  frame_pacer.cpp
  ndi/ndi_sender.cpp
)

target_include_directories(gsr_output PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gsr_output
  PUBLIC  gsr_core
  PRIVATE gsr_warnings)

# ---- NDI SDK: proprietary, DETECTED not fetched (like CUDA). --------------------------
# Search order: NDI_SDK_DIR (cache or env) -> platform default install locations.
if(NOT NDI_SDK_DIR)
  set(NDI_SDK_DIR "$ENV{NDI_SDK_DIR}")
endif()
if(NOT NDI_SDK_DIR)
  foreach(candidate
      "C:/Program Files/NDI/NDI 6 SDK"
      "C:/Program Files/NDI/NDI 5 SDK"
      "/usr/local/ndi"
      "$ENV{HOME}/NDI SDK for Linux")
    if(EXISTS "${candidate}")
      set(NDI_SDK_DIR "${candidate}")
      break()
    endif()
  endforeach()
endif()

option(GSR_NDI_STUB "Compile the NDI sender against the bundled header stub (compile checks only - can never stream)" OFF)

find_path(NDI_INCLUDE_DIR Processing.NDI.Lib.h
  PATHS "${NDI_SDK_DIR}/Include" "${NDI_SDK_DIR}/include" NO_DEFAULT_PATH)
find_library(NDI_LIBRARY
  NAMES Processing.NDI.Lib.x64 ndi
  PATHS "${NDI_SDK_DIR}/Lib/x64" "${NDI_SDK_DIR}/lib/x86_64-linux-gnu" "${NDI_SDK_DIR}/lib"
  NO_DEFAULT_PATH)

if(NDI_INCLUDE_DIR AND NDI_LIBRARY)
  target_include_directories(gsr_output PRIVATE "${NDI_INCLUDE_DIR}")
  target_link_libraries(gsr_output PUBLIC "${NDI_LIBRARY}")
  target_compile_definitions(gsr_output PRIVATE GSR_NDI_ENABLED=1)
  message(STATUS "splatcast: NDI SDK found at ${NDI_SDK_DIR} - NDI output enabled")
elseif(GSR_NDI_STUB)
  target_include_directories(gsr_output PRIVATE "${CMAKE_SOURCE_DIR}/third_party/ndi_stub")
  target_compile_definitions(gsr_output PRIVATE GSR_NDI_ENABLED=1 GSR_NDI_IS_STUB=1)
  message(STATUS "splatcast: NDI sender compiled against the STUB header (cannot stream)")
else()
  message(WARNING "splatcast: NDI SDK not found - NDI output disabled. Install the NDI 6 "
                  "SDK (https://ndi.video) and set NDI_SDK_DIR if it isn't auto-detected.")
endif()
