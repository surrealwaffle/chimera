# SPDX-License-Identifier: GPL-3.0-only

add_library(dear_imgui STATIC
    src/dear_imgui/src/imgui.cpp
    src/dear_imgui/src/imgui_impl_dx9.cpp
    src/dear_imgui/src/imgui_draw.cpp
    src/dear_imgui/src/imgui_widgets.cpp
    src/dear_imgui/src/imgui_demo.cpp
)

target_compile_options(dear_imgui
    PRIVATE -Wno-old-style-cast
)

target_include_directories(dear_imgui
    PUBLIC src/dear_imgui/include
)

include("src/dear_imgui/implot/implot.cmake")