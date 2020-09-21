// SPDX-License-Identifier: GPL-3.0-only

#include "window.hpp"
#include "../signature/signature.hpp"
#include "../chimera.hpp"

namespace Chimera {
    WindowGlobals &WindowGlobals::get_window_globals() {
        WindowGlobals *window_globals = *reinterpret_cast<WindowGlobals **>(get_chimera().get_signature("window_globals_sig").data() + 4);
        return *window_globals;
    }
}