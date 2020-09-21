# SPDX-License-Identifier: GPL-3.0-only

add_library(implot STATIC
    src/dear_imgui/implot/src/implot.cpp
    src/dear_imgui/implot/src/implot_demo.cpp
    src/dear_imgui/implot/src/implot_items.cpp
)

target_compile_options(implot
    PRIVATE -Wno-old-style-cast
)

target_link_libraries(implot
    PUBLIC dear_imgui
)

target_include_directories(implot
    PUBLIC src/dear_imgui/implot/include
)
