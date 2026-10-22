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

option(N3_BUILD_GAME "Build the Windows game and renderer" ON)
include(CTest)

add_library(n3_native STATIC src/native/frame_clock.cpp src/native/shape_bounds.cpp src/native/vector_ops.cpp
    src/native/frame_countdown.cpp src/native/scalar_math.cpp src/native/box_geometry.cpp)
target_include_directories(n3_native PUBLIC src)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(n3_native PRIVATE -ffp-model=strict -ffp-contract=off)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(n3_native PRIVATE -frounding-math -ffp-contract=off)
elseif(MSVC)
    target_compile_options(n3_native PRIVATE /fp:strict)
endif()

add_library(n3_game_data STATIC src/game_data.cpp src/file_hash.cpp src/import_paths.cpp)
target_include_directories(n3_game_data PUBLIC src)
if(WIN32)
    target_compile_definitions(n3_game_data PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(n3_game_data PUBLIC bcrypt)
elseif(NOT APPLE)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    target_link_libraries(n3_game_data PRIVATE OpenSSL::Crypto)
endif()
add_library(n3_menu_assets STATIC src/graphics_menu_assets.cpp)
target_include_directories(n3_menu_assets PUBLIC src)
add_executable(n3_import tools/import_main.cpp)
target_link_libraries(n3_import PRIVATE n3_game_data)
if(BUILD_TESTING)
    add_executable(n3_native_clock_test tests/native_clock_test.cpp)
    target_link_libraries(n3_native_clock_test PRIVATE n3_native)
    add_test(NAME native_frame_clock COMMAND n3_native_clock_test)
    add_executable(n3_native_bounds_test tests/native_bounds_test.cpp)
    target_link_libraries(n3_native_bounds_test PRIVATE n3_native)
    add_test(NAME native_shape_bounds COMMAND n3_native_bounds_test)
    add_executable(n3_native_vector_test tests/native_vector_test.cpp)
    target_link_libraries(n3_native_vector_test PRIVATE n3_native)
    add_test(NAME native_vector_ops COMMAND n3_native_vector_test)
    add_executable(n3_native_primitives_test tests/native_primitives_test.cpp)
    target_link_libraries(n3_native_primitives_test PRIVATE n3_native)
    add_test(NAME native_engine_primitives COMMAND n3_native_primitives_test)
    add_executable(n3_native_box_test tests/native_box_test.cpp)
    target_link_libraries(n3_native_box_test PRIVATE n3_native)
    add_test(NAME native_box_geometry COMMAND n3_native_box_test)
    add_executable(n3_game_data_test tests/game_data_test.cpp)
    target_link_libraries(n3_game_data_test PRIVATE n3_game_data)
    add_test(NAME game_data_import COMMAND n3_game_data_test)
    add_executable(n3_graphics_menu_test tests/graphics_menu_test.cpp)
    target_link_libraries(n3_graphics_menu_test PRIVATE n3_menu_assets)
    add_test(NAME graphics_menu_assets COMMAND n3_graphics_menu_test)
endif()
if(NOT N3_BUILD_GAME)
    message(STATUS "Building portable native components only; no game runtime")
    return()
endif()
if(NOT WIN32)
    message(FATAL_ERROR "The full N3 runtime is Windows-only while platform/GPU adapters are ported. Use -DN3_BUILD_GAME=OFF for portable components.")
endif()
include(generated/rexglue.cmake)

# Sources
set(N3_SOURCES
    src/main.cpp
    src/app_icon.cpp
    src/crash_diagnostics.cpp
    src/runtime_fixes.cpp
    src/shape_bounds_hooks.cpp
    src/vector_ops_hooks.cpp
    src/engine_primitives_hooks.cpp
    src/box_geometry_hooks.cpp
    src/game_setup.cpp
    src/frame_rate.cpp
    src/display_settings.cpp
    src/profile_config.cpp
    src/graphics_menu.cpp
    src/graphics_notification.cpp
    src/graphics_settings.cpp
    src/graphics_audit.cpp
)

if(WIN32)
    enable_language(RC)
    configure_file(resources/n3.rc.in ${CMAKE_CURRENT_BINARY_DIR}/n3.rc @ONLY)
    add_executable(n3 WIN32 ${N3_SOURCES})
    target_sources(n3 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/n3.rc)
else()
    add_executable(n3 ${N3_SOURCES})
endif()

add_subdirectory(tools/gpu)
rexglue_setup_target(n3 GPU_PLUGINS xenos)

target_link_libraries(n3 PRIVATE n3_native n3_game_data n3_menu_assets comdlg32 ole32 uuid shell32)
if(BUILD_TESTING)
    add_executable(n3_fp_policy_test tests/fp_policy_test.cpp)
    target_include_directories(n3_fp_policy_test PRIVATE src)
    target_link_libraries(n3_fp_policy_test PRIVATE rex::runtime)
    add_test(NAME callback_fp_policy COMMAND n3_fp_policy_test)
    add_executable(n3_game_setup_test tests/game_setup_test.cpp src/game_setup.cpp)
    target_link_libraries(n3_game_setup_test PRIVATE n3_game_data rex::runtime comdlg32 ole32 uuid shell32)
    add_test(NAME game_data_setup COMMAND n3_game_setup_test)
    add_executable(n3_crash_diagnostics_test tests/crash_diagnostics_test.cpp src/crash_diagnostics.cpp)
    target_include_directories(n3_crash_diagnostics_test PRIVATE src)
    add_test(NAME crash_diagnostics COMMAND n3_crash_diagnostics_test)
    add_executable(n3_frame_rate_test tests/frame_rate_test.cpp src/frame_rate.cpp)
    target_include_directories(n3_frame_rate_test PRIVATE src)
    target_link_libraries(n3_frame_rate_test PRIVATE n3_native n3_recomp rex::runtime)
    rexglue_apply_target_settings(n3_frame_rate_test)
    add_test(NAME frame_rate_policy COMMAND n3_frame_rate_test)
    add_executable(n3_shape_bounds_test tests/shape_bounds_test.cpp src/shape_bounds_hooks.cpp)
    target_include_directories(n3_shape_bounds_test PRIVATE src)
    target_link_libraries(n3_shape_bounds_test PRIVATE n3_native n3_recomp rex::runtime)
    rexglue_apply_target_settings(n3_shape_bounds_test)
    add_test(NAME shape_bounds_equivalence COMMAND n3_shape_bounds_test)
    add_executable(n3_vector_ops_test tests/vector_ops_test.cpp src/vector_ops_hooks.cpp)
    target_include_directories(n3_vector_ops_test PRIVATE src)
    target_link_libraries(n3_vector_ops_test PRIVATE n3_native n3_recomp rex::runtime)
    rexglue_apply_target_settings(n3_vector_ops_test)
    add_test(NAME vector_ops_equivalence COMMAND n3_vector_ops_test)
    add_executable(n3_engine_primitives_test tests/engine_primitives_test.cpp src/engine_primitives_hooks.cpp)
    target_include_directories(n3_engine_primitives_test PRIVATE src)
    target_link_libraries(n3_engine_primitives_test PRIVATE n3_native n3_recomp rex::runtime)
    rexglue_apply_target_settings(n3_engine_primitives_test)
    add_test(NAME engine_primitives_equivalence COMMAND n3_engine_primitives_test)
    add_executable(n3_box_geometry_test tests/box_geometry_test.cpp src/box_geometry_hooks.cpp)
    target_include_directories(n3_box_geometry_test PRIVATE src)
    target_link_libraries(n3_box_geometry_test PRIVATE n3_native n3_recomp rex::runtime)
    rexglue_apply_target_settings(n3_box_geometry_test)
    add_test(NAME box_geometry_equivalence COMMAND n3_box_geometry_test)
    add_executable(n3_profile_config_test tests/profile_config_test.cpp src/profile_config.cpp)
    target_include_directories(n3_profile_config_test PRIVATE src)
    target_link_libraries(n3_profile_config_test PRIVATE rex::runtime)
    add_test(NAME profile_config COMMAND n3_profile_config_test)
    add_executable(n3_graphics_policy_test tests/graphics_policy_test.cpp)
    target_include_directories(n3_graphics_policy_test PRIVATE src)
    add_test(NAME graphics_policy COMMAND n3_graphics_policy_test)
endif()
