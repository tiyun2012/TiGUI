cmake_minimum_required(VERSION 3.25)

project(MyUI_Milestone1 LANGUAGES CXX)

# Native Windows executable using a C++20 baseline for the future framework.
add_executable(MyUI_Milestone1 WIN32
    src/main.cpp
    src/Win32Window.cpp
    src/Win32Window.h
    src/DX12Renderer.cpp
    src/DX12Renderer.h
)

target_compile_features(MyUI_Milestone1 PRIVATE cxx_std_20)

target_compile_definitions(MyUI_Milestone1 PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(MyUI_Milestone1 PRIVATE
    d3d12.lib
    dxgi.lib
)
