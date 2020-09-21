// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHIMERA_GAME_QUIT_HPP
#define CHIMERA_GAME_QUIT_HPP

#include "event.hpp"

namespace Chimera {
    /**
     * Add or replace a game quit event. This event occurs as soon as Halo leaves the main game loop.
     * @param function This is the function
     * @param priority Priority to use
     */
    void add_game_quit_event(const EventFunction function, EventPriority priority = EventPriority::EVENT_PRIORITY_DEFAULT);
    
    /**
     * Remove a game quit event if the function is being used as an event.
     * @param function This is the function
     */
    void remove_game_quit_event(const EventFunction function);
}

#endif
