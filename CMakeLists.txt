cmake_minimum_required(VERSION 3.10)

project(private_mp4_frame_search CXX)

option(ENABLE_COVERAGE "Enable GCC/gcov coverage flags" OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(private_mp4_core
  src/frame_locator.cpp
  src/frame_locator_internal.cpp
  src/frame_query.cpp
  src/mapped_file_win32.cpp
)

target_include_directories(private_mp4_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MINGW)
  target_compile_options(private_mp4_core PRIVATE -Wall -Wextra -Wpedantic)
else()
  target_compile_options(private_mp4_core PRIVATE)
endif()

if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(private_mp4_core PRIVATE -O0 -g --coverage)
  target_link_options(private_mp4_core PRIVATE --coverage)
elseif(MINGW)
  target_compile_options(private_mp4_core PRIVATE -O3)
else()
  target_compile_options(private_mp4_core PRIVATE -O3)
endif()

add_executable(private_mp4_search
  src/main.cpp
)

target_link_libraries(private_mp4_search
  PRIVATE
    private_mp4_core
)

add_library(private_mp4_synth_support
  tools/synthetic_mp4_support.cpp
)

target_include_directories(private_mp4_synth_support
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

if(MINGW)
  target_compile_options(private_mp4_synth_support PRIVATE -Wall -Wextra -Wpedantic)
else()
  target_compile_options(private_mp4_synth_support PRIVATE)
endif()

if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(private_mp4_synth_support PRIVATE -O0 -g --coverage)
  target_link_options(private_mp4_synth_support PRIVATE --coverage)
elseif(MINGW)
  target_compile_options(private_mp4_synth_support PRIVATE -O3)
else()
  target_compile_options(private_mp4_synth_support PRIVATE -O3)
endif()

add_executable(private_mp4_generate_sample
  tools/generate_synthetic_mp4.cpp
)

target_link_libraries(private_mp4_generate_sample
  PRIVATE
    private_mp4_synth_support
)

add_executable(private_mp4_benchmark
  tools/benchmark_locate_frame.cpp
)

target_link_libraries(private_mp4_benchmark
  PRIVATE
    private_mp4_core
    private_mp4_synth_support
)

add_subdirectory(third_party/gtest-1.8.1)

enable_testing()

add_executable(private_mp4_tests
  tests/frame_locator_test.cpp
  tests/frame_locator_internal_test.cpp
  tests/mapped_file_win32_test.cpp
  tests/mp4_format_test.cpp
  tests/frame_query_test.cpp
  tests/synthetic_mp4_support_test.cpp
)

target_include_directories(private_mp4_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

target_link_libraries(private_mp4_tests
  PRIVATE
    private_mp4_core
    private_mp4_synth_support
    gtest
    gtest_main
)

if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(private_mp4_tests PRIVATE -O0 -g --coverage)
  target_link_options(private_mp4_tests PRIVATE --coverage)
endif()

add_test(NAME private_mp4_tests COMMAND private_mp4_tests)
