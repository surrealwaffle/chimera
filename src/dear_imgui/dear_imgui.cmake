# SPDX-License-Identifier: GPL-3.0-only

add_library(dear_imgui STATIC
    src/dear_imgui/imgui.cpp
    src/dear_imgui/imgui_impl_dx9.cpp
    src/dear_imgui/imgui_draw.cpp
    src/dear_imgui/imgui_impl_dx9.cpp
    src/dear_imgui/imgui_widgets.cpp
)

target_include_directories(dear_imgui
    PRIVATE   src/dear_imgui/include/dear_imgui
    INTERFACE src/dear_imgui/include
)
