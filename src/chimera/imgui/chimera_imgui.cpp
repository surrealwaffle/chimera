// SPDX-License-Identifier: GPL-3.0-only

#include "chimera_imgui.hpp"
#include "../event/d3d9_reset.hpp"
#include "../event/d3d9_end_scene.hpp"
#include "../event/game_quit.hpp"
#include "../signature/hook.hpp"
#include "../signature/signature.hpp"
#include "../chimera.hpp"
#include "imgui.h"
#include "implot.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_blam.hpp"

namespace /*(anonymous)*/ {
    bool context_initialized     = false; 
    bool platform_initialized    = false;
    bool renderer_initialized    = false;
    bool device_objects_created  = false;
    
    bool imgui_demo_window_open  = false;
    bool implot_demo_window_open = false;
    
    /**
     * Checks if Dear ImGui is completely initialized.
     * @return `true` if and only if Dear ImGui is initialized.
     */
    bool ready();
    
    /**
     * Initializes the rendering implementation and device objects, as needed.
     * @return `ready()`
     */
    bool prepare_renderer_implementation(LPDIRECT3DDEVICE9 device);
    
    /**
     * Destroys device objects associated with the rendering implementation and flags them for recreation on the next render frame.
     */
    void imgui_reset_device(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS *present);
    
    /**
     * Performs an update on the Dear ImGui state.
     */
    void imgui_update();
    
    /**
     * Renders the GUI to the device.
     */
    void imgui_new_frame(LPDIRECT3DDEVICE9 device);
    
    /**
     * Destroys the ImGui context and renderer/platform implementation resources.
     */
    void imgui_destroy();
} // namespace (anonymous)

namespace Chimera {   
    void initialize_imgui() {
        if (!context_initialized) {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImPlot::CreateContext();
            context_initialized = true;
        }
        
        if (!platform_initialized) {
            platform_initialized = ImGui_ImplBlam_Init();
            if (!platform_initialized)
                return;
        }
        
        add_d3d9_reset_event(imgui_reset_device);
        add_d3d9_end_scene_event(imgui_new_frame);
        add_game_quit_event(imgui_destroy);
        
        static Hook update_widgets_hook;
        write_jmp_call(get_chimera().get_signature("update_widgets_sig").data(), 
                       update_widgets_hook,
                       reinterpret_cast<const void *>(&imgui_update),
                       nullptr,
                       true);
    }
    
    void show_imgui_demo_window(bool show) {
        imgui_demo_window_open = show;
    }
    
    void show_implot_demo_window(bool show) {
        implot_demo_window_open = show;
    }
}

namespace {    
    void imgui_reset_device(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS *) {
        if (!renderer_initialized)
            return;
        
        ImGui_ImplDX9_InvalidateDeviceObjects();
        device_objects_created = false;
    }
    
    void imgui_update() {
        if (!ready())
            return;
        
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplBlam_NewFrame();
        ImGui::NewFrame();
        
        if (imgui_demo_window_open)
            ImGui::ShowDemoWindow(&imgui_demo_window_open);
        
        if (implot_demo_window_open)
            ImPlot::ShowDemoWindow(&implot_demo_window_open);
        
        ImGui::EndFrame();
        ImGui_ImplBlam_CaptureInput();
    }
    
    void imgui_new_frame(LPDIRECT3DDEVICE9 device) {
        if (!ready()) {
            // no data for this frame, so dont bother even if renderer is prepared
            prepare_renderer_implementation(device);
            return;
        }
        
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
    
    void imgui_destroy() {
        if (renderer_initialized) {
            ImGui_ImplDX9_Shutdown();
            renderer_initialized = false;
            device_objects_created = false;
        }
        
        if (platform_initialized) {
            ImGui_ImplBlam_Shutdown();
            platform_initialized = false;
        }
        
        if (context_initialized) {
            ImGui::DestroyContext();
            context_initialized = false;
        }
    }
    
    bool ready() {
        return context_initialized
            && platform_initialized
            && renderer_initialized
            && device_objects_created;
    }
    
    bool prepare_renderer_implementation(LPDIRECT3DDEVICE9 device) {
        if (!context_initialized)
            return ready();
        
        if (!renderer_initialized) {
            ImGui_ImplDX9_Init(device);
            renderer_initialized = true;
            device_objects_created = true;
        }
        
        if (renderer_initialized && !device_objects_created) {
            ImGui_ImplDX9_CreateDeviceObjects();
            device_objects_created = true;
        }
        
        return ready();
    }
}