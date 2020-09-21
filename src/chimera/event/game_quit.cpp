// SPDX-License-Identifier: GPL-3.0-only

#include "../chimera.hpp"
#include "../signature/hook.hpp"
#include "../signature/signature.hpp"

#include "game_quit.hpp"

#include <algorithm>

namespace Chimera {
    static void enable_game_quit_hook();
    
    static std::vector<Event<EventFunction>> game_quit_events;
    
    void add_game_quit_event(const EventFunction function, EventPriority priority) {
        remove_game_quit_event(function); // remove function if it exists
        
        enable_game_quit_hook(); // enable hook if not enabled
        
        game_quit_events.emplace_back(Event<EventFunction> { function, priority });
    }
    
    void remove_game_quit_event(const EventFunction function) {
        auto is_event_function = [&function] (const auto& e) { return e.function == function; };
        if (auto it = std::find_if(game_quit_events.cbegin(), game_quit_events.cend(), is_event_function); it != game_quit_events.cend())
            game_quit_events.erase(it);
    }
    
    void enable_game_quit_hook() {
        static bool enabled = false;
        if (enabled)
            return;
        enabled = true;
        
        static Hook hook;        
        auto on_game_quit = +[] { call_in_order(game_quit_events); };
        write_jmp_call(get_chimera().get_signature("leave_game_loop_sig").data(), hook, reinterpret_cast<const void *>(on_game_quit), nullptr);
    }
}