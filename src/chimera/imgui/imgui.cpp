// SPDX-License-Identifier: GPL-3.0-only

#include "imgui.hpp"
#include <d3d9.h>
#include "../event/d3d9_reset.hpp"
#include "../event/d3d9_end_scene.hpp"
#include "../event/game_quit.hpp"
#include "dear_imgui/imgui.h"
#include "dear_imgui/imgui_impl_dx9.h"

namespace {
    bool imgui_initialized = false;
    bool imgui_renderer_initialized = false;
    bool imgui_device_objects_created = false;
    
    void imgui_reset_device(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS *present);
    
    void imgui_new_frame(LPDIRECT3DDEVICE9 device);
    
    void imgui_destroy();
} // (anonymous)

namespace Chimera {   
    void initialize_imgui() {
        if (imgui_initialized)
            return;
        
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        
        add_d3d9_reset_event(imgui_reset_device);
        add_d3d9_end_scene_event(imgui_new_frame);
        add_game_quit_event(imgui_destroy);
        
        imgui_initialized = true;
    }
}

namespace {
    bool imgui_initialize_renderer(LPDIRECT3DDEVICE9 device) {
        if (!imgui_initialized)
            return false;
        
        if (!imgui_renderer_initialized)
            imgui_renderer_initialized = ImGui_ImplDX9_Init(device);
        
        return imgui_renderer_initialized;
    }
    
    bool imgui_create_device_objects() {
        if (!imgui_initialized || !imgui_renderer_initialized)
            return false;
        
        if (!imgui_device_objects_created)
            imgui_device_objects_created = ImGui_ImplDX9_CreateDeviceObjects();
        
        return imgui_device_objects_created;
    }
    
    void imgui_reset_device([[maybe_unused]] LPDIRECT3DDEVICE9 device, [[maybe_unused]] D3DPRESENT_PARAMETERS *present) {
        if (!imgui_initialized)
            return;
        
        ImGui_ImplDX9_InvalidateDeviceObjects();
        imgui_device_objects_created = false;
    }
    
    void imgui_new_frame(LPDIRECT3DDEVICE9 device) {
        if (!imgui_initialized || !imgui_initialize_renderer(device) || !imgui_create_device_objects())
            return;
        
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();
        
        // do stuff here
        
        ImGui::EndFrame();
        
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
    
    void imgui_destroy() {
        if (imgui_renderer_initialized) {
            ImGui_ImplDX9_Shutdown();
            imgui_renderer_initialized = false;
        }
        
        if (imgui_initialized) {
            ImGui::DestroyContext();
            imgui_initialized = false;
        }
    }
}