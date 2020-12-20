// SPDX-License-Identifier: GPL-3.0-only

#ifndef LUA__CHIMERA__HPP
#define LUA__CHIMERA__HPP

struct lua_State;

namespace Chimera {
    /**
     * Set up ImGui and ImPlot functions for the lua state
     * @param state state to set up functions for
     * @param api   API to target
     */
    void set_up_imgui_functions(lua_State *state, unsigned int api) noexcept;
}

#endif