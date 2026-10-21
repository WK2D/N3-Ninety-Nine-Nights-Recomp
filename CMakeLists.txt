# n3 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(n3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(N3_SOURCES
    src/main.cpp
    src/crash_diagnostics.cpp
    src/runtime_fixes.cpp
)

if(WIN32)
    add_executable(n3 WIN32 ${N3_SOURCES})
else()
    add_executable(n3 ${N3_SOURCES})
endif()

rexglue_setup_target(n3 GPU_PLUGINS xenos)

include(CTest)
if(BUILD_TESTING)
    add_executable(n3_fp_policy_test tests/fp_policy_test.cpp)
    target_include_directories(n3_fp_policy_test PRIVATE src)
    target_link_libraries(n3_fp_policy_test PRIVATE rex::runtime)
    add_test(NAME callback_fp_policy COMMAND n3_fp_policy_test)
endif()
