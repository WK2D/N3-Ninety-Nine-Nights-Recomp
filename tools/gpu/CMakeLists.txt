# Build the pinned Xenos plugin with the project's live-resolution extension.
# All original SDK sources stay in the ignored tool directory.
set(REXGLUE_ROOT "${CMAKE_SOURCE_DIR}/.tools/live-gpu-source/rexglue-sdk")
if(NOT EXISTS "${REXGLUE_ROOT}/src/graphics/d3d12/n3_live_resolution.inc")
    message(FATAL_ERROR "Run tools/prepare-gpu.py before configuring")
endif()
set(REXGLUE_USE_D3D12 ON)
set(REXGLUE_USE_VULKAN OFF)
add_library(rexruntime ALIAS rex::runtime)
add_library(xxHash::xxhash ALIAS rex::xxhash)
function(rexglue_add_version_resource target)
endfunction()
function(rex_target_pch target)
endfunction()
add_subdirectory("${REXGLUE_ROOT}/src/graphics" "${CMAKE_BINARY_DIR}/n3-gpu")
target_sources(rexgpu-xenos PRIVATE "${REXGLUE_ROOT}/src/ui/d3d12/n3_d3d12_presenter.cpp")
target_include_directories(rexgpu-xenos PRIVATE "${REXGLUE_ROOT}/include" "${REXGLUE_ROOT}/src" "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(rexgpu-xenos PRIVATE REX_PLATFORM_WINDOWS=1 NOMINMAX)
target_compile_options(rexgpu-xenos PRIVATE -march=x86-64-v2 -fno-strict-aliasing -ffp-model=strict -fno-char8_t)
set_target_properties(rexgpu-xenos PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/n3-gpu"
    PREFIX "" OUTPUT_NAME "rexgpu-xenos" DEBUG_POSTFIX "d" RELWITHDEBINFO_POSTFIX "rd")
add_dependencies(n3 rexgpu-xenos)
# Runtime-loaded plugins do not participate in the EXE's link dependency graph.
# Stage on every build, including when only renderer sources changed.
add_custom_target(n3_stage_gpu ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rexgpu-xenos>" "$<TARGET_FILE_DIR:n3>"
    DEPENDS rexgpu-xenos VERBATIM)
add_dependencies(n3 n3_stage_gpu)
