// SPDX-License-Identifier: GPL-3.0-only

#include "input_devices.hpp"
#include "../signature/signature.hpp"
#include "../chimera.hpp"

namespace Chimera {
    InputGlobals &InputGlobals::get_input_globals() {
        static InputGlobals *input_globals = *reinterpret_cast<InputGlobals **>(get_chimera().get_signature("input_globals_sig").data() + 4);
        return *input_globals;
    }
}