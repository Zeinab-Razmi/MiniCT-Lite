cmake_minimum_required(VERSION 3.14)
project(minict_lite LANGUAGES CXX)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)  # silences a FetchContent warning
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# stb_image_write: tiny single-header library used to save PNG files
include(FetchContent)
FetchContent_Declare(stb
  URL https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
  DOWNLOAD_NO_EXTRACT TRUE
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/stb)
FetchContent_MakeAvailable(stb)

# DCMTK: the standard C++ library for DICOM (install: sudo apt install libdcmtk-dev)
find_package(DCMTK REQUIRED)

# Public sample CT image (from the pydicom project's test files), downloaded once.
# If the download fails, the build still works: get any .dcm file yourself.
set(SAMPLE_DCM ${CMAKE_BINARY_DIR}/sample_CT_small.dcm)
if(NOT EXISTS ${SAMPLE_DCM})
  file(DOWNLOAD
    https://raw.githubusercontent.com/pydicom/pydicom/main/src/pydicom/data/test_files/CT_small.dcm
    ${SAMPLE_DCM} STATUS dl_status TLS_VERIFY ON)
  list(GET dl_status 0 dl_code)
  if(NOT dl_code EQUAL 0)
    file(REMOVE ${SAMPLE_DCM})
    message(WARNING "Could not download the sample DICOM file. Use your own .dcm file.")
  endif()
endif()

# Library with all the image processing code
add_library(minict
  src/image.cpp
  src/dicom.cpp
  src/png.cpp
  src/phantom.cpp
  src/corrections.cpp
  src/filters.cpp
  src/metrics.cpp)
target_include_directories(minict PUBLIC include)
target_include_directories(minict PRIVATE ${DCMTK_INCLUDE_DIRS})
if(TARGET DCMTK::dcmjpeg)
  # Normal case: DCMTK's own targets know where their libraries are
  target_link_libraries(minict PUBLIC DCMTK::dcmdata DCMTK::dcmjpeg)
else()
  # Fallback: DCMTK_LIBRARIES only contains library names (ofstd, dcmdata, ...) without a
  # folder, which fails on macOS/Homebrew. DCMTK_DIR is <prefix>/lib/cmake/dcmtk, so the
  # libraries are in <prefix>/lib.
  get_filename_component(DCMTK_LIB_DIR "${DCMTK_DIR}/../.." ABSOLUTE)
  message(STATUS "DCMTK libraries expected in ${DCMTK_LIB_DIR}")
  target_link_directories(minict PUBLIC ${DCMTK_LIB_DIR})
  target_link_libraries(minict PUBLIC ${DCMTK_LIBRARIES})
endif()
target_include_directories(minict SYSTEM PRIVATE ${CMAKE_BINARY_DIR}/stb)
target_compile_options(minict PRIVATE -Wall -Wextra)

# Command-line program
add_executable(minict_cli src/main.cpp)
target_link_libraries(minict_cli PRIVATE minict)

# Tests (GoogleTest is downloaded automatically)
enable_testing()
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
FetchContent_MakeAvailable(googletest)

add_executable(minict_tests
  tests/test_corrections.cpp
  tests/test_filters.cpp
  tests/test_dicom.cpp
  tests/test_basics.cpp
  tests/test_pipeline.cpp)
target_link_libraries(minict_tests PRIVATE minict GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(minict_tests)
