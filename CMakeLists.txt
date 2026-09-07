cmake_minimum_required(VERSION 3.25)

project(MyUI_Milestone1 LANGUAGES CXX)

# ============================================================================
# Source layout
#
#     Core
#         General application infrastructure.
#
#     Platform
#         Windows-specific functionality.
#
#     Renderer
#         DirectX 12-specific functionality.
#
#     main.cpp
#         Application entry point.
# ============================================================================

add_executable(
    MyUI_Milestone1
    WIN32

    src/main.cpp

    src/Core/Log.cpp
    src/Core/Log.h

    src/Platform/Win32Window.cpp
    src/Platform/Win32Window.h

    src/Renderer/DX12Renderer.cpp
    src/Renderer/DX12Renderer.h
)

# ============================================================================
# C++ standard
# ============================================================================

target_compile_features(
    MyUI_Milestone1
    PRIVATE
        cxx_std_20
)

# ============================================================================
# Include root
#
# This is important because our source code uses:
#
#     #include "Core/Log.h"
#     #include "Platform/Win32Window.h"
#     #include "Renderer/DX12Renderer.h"
#
# Adding "src" means those paths resolve correctly.
# ============================================================================

target_include_directories(
    MyUI_Milestone1
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# Windows definitions
# ============================================================================

target_compile_definitions(
    MyUI_Milestone1
    PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

# ============================================================================
# DirectX 12 libraries
# ============================================================================

target_link_libraries(
    MyUI_Milestone1
    PRIVATE
        d3d12.lib
        dxgi.lib
)
