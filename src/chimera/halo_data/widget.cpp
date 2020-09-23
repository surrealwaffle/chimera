// SPDX-License-Identifier: GPL-3.0-only

#include <d3d9.h>
#include "widget.hpp"
#include "../signature/signature.hpp"
#include "../chimera.hpp"
#include "../fix/widescreen_fix.hpp"

namespace Chimera {
    WidgetEventGlobals &WidgetEventGlobals::get_widget_event_globals() {
        static WidgetEventGlobals *widget_event_globals = *reinterpret_cast<WidgetEventGlobals **>(get_chimera().get_signature("widget_event_globals_sig").data() + 8);
        return *widget_event_globals;
    }
    
    WidgetCursorGlobals &WidgetCursorGlobals::get_widget_cursor_globals() {
        static WidgetCursorGlobals *widget_cursor_globals = nullptr;
        
        if (widget_cursor_globals == nullptr) {
            if (get_chimera().feature_present("client_demo")) {
                widget_cursor_globals = *reinterpret_cast<WidgetCursorGlobals **>(get_chimera().get_signature("widget_cursor_globals_trial_sig").data() + 1);
            } else {
                widget_cursor_globals = *reinterpret_cast<WidgetCursorGlobals **>(get_chimera().get_signature("widget_cursor_globals_sig").data() + 4);
            }
        }
        
        return *widget_cursor_globals;
    }
    
    WidgetGlobals &WidgetGlobals::get_widget_globals() {
        static WidgetGlobals *widget_globals = *reinterpret_cast<WidgetGlobals **>(get_chimera().get_signature("widget_globals_sig").data() + 8);
        return *widget_globals;
    }
    
    std::pair<float, float> WidgetCursorGlobals::get_client_normalized_position() {
        const auto [left, right] = get_widescreen_horizontal_extents();
        const auto [top, bottom] = get_widescreen_vertical_extents();
        const float x = static_cast<float>(position.x - left) / (right - left);
        const float y = static_cast<float>(position.y - top) / (bottom - top);
        return {x, y};
    }
    
    std::pair<float, float> WidgetCursorGlobals::get_framebuffer_position() {
        static const D3DPRESENT_PARAMETERS *present_parameters = *reinterpret_cast<D3DPRESENT_PARAMETERS **>(get_chimera().get_signature("d3d9_present_parameters_sig").data() + 6);
        
        const auto [normalized_x, normalized_y] = get_client_normalized_position();
        return {normalized_x * present_parameters->BackBufferWidth, normalized_y * present_parameters->BackBufferHeight};
    }
}