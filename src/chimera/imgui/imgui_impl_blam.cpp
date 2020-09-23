// SPDX-License-Identifier: GPL-3.0-only

#include <windows.h>
#include <d3d9.h>
#include "imgui.h"
#include <algorithm> // std::fill, std::remove_if
#include <iterator>  // std::begin, std::end, std::size
#include <limits>    // std::numeric_limits
#include "../signature/signature.hpp"
#include "../signature/hook.hpp"
#include "../chimera.hpp"
#include "../halo_data/input_devices.hpp"
#include "../halo_data/widget.hpp"
#include "../halo_data/window.hpp"

extern "C" {
    /** Set this to non-zero to hide Halo's cursor. */
    long imgui_impl_blam_hide_widget_cursor = 0;
    
    /** imgui_impl_blam_draw_widget_cursor conditionally calls on this to draw the original cursor. */
    const void *imgui_impl_blam_draw_widget_cursor_original = nullptr;
    
    /** See imgui_impl_blam.S for implementation. */
    void imgui_impl_blam_draw_widget_cursor(/*NO ARGUMENTS*/);
}

namespace /* (anonymous) */ {
    // The implementation subclasses the main window for key/character messages.
    // Mouse input does not appear to go through the WindowProc, so that data
    // is taken directly from the widget event queues and input device buffers.
    
    /** The original (unsubclassed) WindowProc callback. */
    WNDPROC lpfnOldWndProc = NULL;
    
    /**
     * The callback that is used to subclass the window.
     *
     * Processes (but does not capture) the following messages:
     *  * `WM_CHAR`,
     *  * `WM_KEYDOWN`,
     *  * `WM_SYSKEYDOWN`,
     *  * `WM_KEYUP`, and
     *  * `WM_SYSKEYUP`.
     * 
     * This is somewhat necessary for better platform integration, as Halo loses information
     * when processing these messages (e.g. `WM_CHAR` only takes the lower byte of `wParam`).
     */
    LRESULT ImGui_ImplBlam_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    /**
     * Tests if ImGui is to receive (but not necessarily capture) input.
     * @return `true` if ImGui should be receiving input, otherwise `false`.
     */
    bool is_imgui_active();
} // namespace /* (anonymous) */

bool ImGui_ImplBlam_Init() {
    using Chimera::Hook;
    using Chimera::WindowGlobals;
    using Chimera::get_chimera;
    using Chimera::write_function_override;
    
    // subclass the main window
    if (lpfnOldWndProc == NULL) {
        HWND hWnd = WindowGlobals::get_window_globals().hWnd;
        SetLastError(0);
        lpfnOldWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ImGui_ImplBlam_WndProc)));
        if (GetLastError() != 0) {
            lpfnOldWndProc = NULL;
            return false;
        }
    }
    
    // used to disable Halo's honkin' cursor when ImGui wants the mouse
    static Hook draw_widget_cursor_hook;
    write_function_override(get_chimera().get_signature("widget_draw_cursor_sig").data(),
                            draw_widget_cursor_hook,
                            reinterpret_cast<const void *>(&imgui_impl_blam_draw_widget_cursor),
                            &imgui_impl_blam_draw_widget_cursor_original);
    
    ImGuiIO &io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_blam";
    
    return true;
}

void ImGui_ImplBlam_Shutdown() {
    // undo the subclass
    if (lpfnOldWndProc != NULL) {
        HWND hWnd = Chimera::WindowGlobals::get_window_globals().hWnd;
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(lpfnOldWndProc));
        lpfnOldWndProc = NULL;
    }
}

void ImGui_ImplBlam_NewFrame() {
    ImGuiIO &io = ImGui::GetIO();
    auto clear_keys = [&io] {
        io.KeyCtrl  = false;
        io.KeyShift = false;
        io.KeyAlt   = false;
        io.KeySuper = false;
        std::fill(std::begin(io.KeysDown), std::end(io.KeysDown), false);
    };
    
    auto clear_mouse = [&io] {
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX); // mouse unavailable
        std::fill(std::begin(io.MouseDown), std::end(io.MouseDown), false);
    };
    
    if (!is_imgui_active()) {
        clear_keys();
        clear_mouse();
        imgui_impl_blam_hide_widget_cursor = 0;
        return;
    }
    
    using Chimera::InputGlobals;
    using Chimera::MouseButtonWidgetEvent;
    using Chimera::WidgetEvent;
    using Chimera::WidgetEventGlobals;
    using Chimera::WidgetCursorGlobals;
    
    // ---------
    // DISPLAY
    {
        using Chimera::get_chimera;
        static const D3DPRESENT_PARAMETERS *present_parameters = *reinterpret_cast<D3DPRESENT_PARAMETERS **>(get_chimera().get_signature("d3d9_present_parameters_sig").data() + 6);
        io.DisplaySize = ImVec2(present_parameters->BackBufferWidth, present_parameters->BackBufferHeight);
    }
    
    // ---------
    // MOUSE
    // Mouse position from WidgetCursorGlobals
    // Mouse buttons from WidgetEventGlobals
    // Mouse wheel from InputGlobals
    {
        clear_mouse();
        const auto [mouse_x, mouse_y] = WidgetCursorGlobals::get_widget_cursor_globals().get_framebuffer_position();
        io.MousePos = ImVec2(mouse_x, mouse_y);
        for (const auto& queue : WidgetEventGlobals::get_widget_event_globals().queues) {
            for (const WidgetEvent& e : queue) {
                if (e.event_type != WidgetEvent::type_mouse_button || e.event.mouse.duration == 0)
                    continue;
                
                switch (e.event.mouse.button) {
                case MouseButtonWidgetEvent::button_left_mouse:
                    io.MouseDown[0] = true;
                    break;
                case MouseButtonWidgetEvent::button_middle_mouse:
                    io.MouseDown[2] = true;
                    break;
                case MouseButtonWidgetEvent::buttom_right_mouse:
                    io.MouseDown[1] = true;
                    break;
                case MouseButtonWidgetEvent::button_double_click:
                    break;
                default:
                    break;
                }
            }
        }
        io.MouseWheel -= InputGlobals::get_input_globals().enumerated_devices.direct_mouse_state.wheel;
    }
    
    // ---------
    // KEYBOARD
    // Handled by ImGui_ImplBlam_WndProc
}

void ImGui_ImplBlam_CaptureInput() {
    if (!is_imgui_active())
        return;
    
    ImGuiIO &io = ImGui::GetIO();
    io.MouseDrawCursor = io.WantCaptureMouse;
    imgui_impl_blam_hide_widget_cursor = io.WantCaptureMouse ? 1 : 0;
    
    using Chimera::InputGlobals;
    using Chimera::WidgetEvent;
    
    // Note: never remove `custom_activation` widget events.
    auto remove_widget_events_by_type = [] (WidgetEvent::EventType type) {
        using Chimera::WidgetEventGlobals;
        for (auto& queue : WidgetEventGlobals::get_widget_event_globals().queues) {
            std::fill(std::remove_if(std::begin(queue),
                                     std::end(queue),
                                     [type] (auto&& e) { return e.event_type == type; }),
                      std::end(queue),
                      WidgetEvent{/*ZERO INITIALIZED*/});
        }
    };
    
    if (io.WantCaptureMouse) {
        remove_widget_events_by_type(WidgetEvent::type_mouse_button);
        InputGlobals::get_input_globals().enumerated_devices.direct_mouse_state.wheel = 0;
    }
    
    if (io.WantCaptureKeyboard) {
        InputGlobals &input_globals = InputGlobals::get_input_globals();
        remove_widget_events_by_type(WidgetEvent::type_gamepad_button);
        input_globals.buffered_keys.read_index = 0;
        input_globals.buffered_keys.count = 0;
        std::fill(std::begin(input_globals.buffered_keys.keys),
                  std::end(input_globals.buffered_keys.keys),
                  InputGlobals::BufferedKey{/*ZERO-INITIALIZED*/});
    }
}

namespace {
    LRESULT ImGui_ImplBlam_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (is_imgui_active()) {
            ImGuiIO &io = ImGui::GetIO();
            switch (uMsg) {
                case WM_CHAR:
                    if (wParam != 0 && wParam <= std::numeric_limits<ImWchar16>::max())
                        io.AddInputCharacterUTF16(static_cast<ImWchar16>(wParam));
                    break;
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                    if (wParam < std::size(io.KeysDown))
                        io.KeysDown[wParam] = true;
                    break;
                case WM_KEYUP:
                case WM_SYSKEYUP:
                    if (wParam < std::size(io.KeysDown))
                        io.KeysDown[wParam] = false;
                    break;
                default:
                    break;
            }
        }
        
        return CallWindowProc(lpfnOldWndProc, hWnd, uMsg, wParam, lParam);
    }
    
    bool is_imgui_active() {
        return Chimera::WidgetGlobals::get_widget_globals().top_widget_instance[0] != nullptr;
    }
} // namespace (anonymous)