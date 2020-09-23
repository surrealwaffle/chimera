// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHIMERA_IMGUI_HPP
#define CHIMERA_IMGUI_HPP

namespace Chimera {
    /**
     * Initializes the Dear ImGui implementation for Halo.
     *
     * This operation does not initialize the implementation completely.
     * The rendering implementation is initialized as needed when the frames are drawn.
     * There may therefore be some delay between when this function is called and when Dear ImGui is ready.
     * When exposing a binding for user scripts, this detail should be presented to the script developers.
     */
    void initialize_imgui();
    
    /**
     * Shows or hides the ImGui demo window.
     *
     * This function is exposed to the user (with @a show as `true`) as the command `chimera_show_imgui_demo`.
     * @param[in] show `true` to show the window, or `false` to hide it.
     */
    void show_imgui_demo_window(bool show = true);
    
    /**
     * Shows or hides the ImPlot demo window.
     *
     * This function is exposed to the user (with @a show as `true`) as the command `chimera_show_implot_demo`.
     * @param[in] show `true` to show the window, or `false` to hide it.
     */
    void show_implot_demo_window(bool show = true);
}

#endif