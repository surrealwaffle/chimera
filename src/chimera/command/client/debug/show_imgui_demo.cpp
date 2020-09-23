// SPDX-License-Identifier: GPL-3.0-only

#include "../../../imgui/chimera_imgui.hpp"

namespace Chimera {
    bool show_imgui_demo_command(int, const char **) {
        show_imgui_demo_window();
        return true;
    }
    
    bool show_implot_demo_command(int, const char **) {
        show_implot_demo_window();
        return true;
    }
}