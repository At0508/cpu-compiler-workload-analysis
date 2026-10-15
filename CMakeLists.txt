cmake_minimum_required(VERSION 3.24)
project(cpu_compiler_workload_analysis LANGUAGES C CXX)

find_package(LLVM 18 REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include(CTest)
add_llvm_pass_plugin(LoopReport src/LoopReport.cpp)
target_include_directories(LoopReport SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_features(LoopReport PRIVATE cxx_std_17)

add_executable(loop_benchmark workloads/loop_benchmark.cpp)
target_compile_features(loop_benchmark PRIVATE cxx_std_17)
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
  target_link_libraries(loop_benchmark PRIVATE OpenMP::OpenMP_CXX)
endif()

if(BUILD_TESTING)
  add_test(NAME loop_report_fixture
    COMMAND ${CMAKE_COMMAND}
      -DOPT_EXECUTABLE=${LLVM_TOOLS_BINARY_DIR}/opt
      -DPLUGIN_FILE=$<TARGET_FILE:LoopReport>
      -DINPUT_IR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/loops.ll
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_report.cmake)
  add_test(NAME loop_benchmark_smoke COMMAND loop_benchmark 64 3)
endif()
