# n3 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(n3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options("-ffile-prefix-map=${CMAKE_SOURCE_DIR}=." "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.")
endif()
if(WIN32)
    add_link_options("LINKER:/Brepro" "LINKER:/PDBALTPATH:%_PDB%")
endif()

include(generated/rexglue.cmake)

# Sources
set(N3_SOURCES
    src/main.cpp
    src/crash_diagnostics.cpp
    src/runtime_fixes.cpp
    src/game_setup.cpp
)

if(WIN32)
    add_executable(n3 WIN32 ${N3_SOURCES})
else()
    add_executable(n3 ${N3_SOURCES})
endif()

rexglue_setup_target(n3 GPU_PLUGINS xenos)

add_library(n3_game_data STATIC src/game_data.cpp)
target_include_directories(n3_game_data PUBLIC src)
target_link_libraries(n3_game_data PUBLIC bcrypt)
target_link_libraries(n3 PRIVATE n3_game_data comdlg32 ole32 uuid shell32)

add_executable(n3_import tools/import_main.cpp)
target_link_libraries(n3_import PRIVATE n3_game_data)

include(CTest)
if(BUILD_TESTING)
    add_executable(n3_fp_policy_test tests/fp_policy_test.cpp)
    target_include_directories(n3_fp_policy_test PRIVATE src)
    target_link_libraries(n3_fp_policy_test PRIVATE rex::runtime)
    add_test(NAME callback_fp_policy COMMAND n3_fp_policy_test)
    add_executable(n3_game_data_test tests/game_data_test.cpp)
    target_link_libraries(n3_game_data_test PRIVATE n3_game_data)
    add_test(NAME game_data_import COMMAND n3_game_data_test)
    add_executable(n3_game_setup_test tests/game_setup_test.cpp src/game_setup.cpp)
    target_link_libraries(n3_game_setup_test PRIVATE n3_game_data rex::runtime comdlg32 ole32 uuid shell32)
    add_test(NAME game_data_setup COMMAND n3_game_setup_test)
    add_executable(n3_crash_diagnostics_test tests/crash_diagnostics_test.cpp src/crash_diagnostics.cpp)
    target_include_directories(n3_crash_diagnostics_test PRIVATE src)
    add_test(NAME crash_diagnostics COMMAND n3_crash_diagnostics_test)
endif()
