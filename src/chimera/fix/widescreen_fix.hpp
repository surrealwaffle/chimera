// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHIMERA_WIDESCREEN_FIX_HPP
#define CHIMERA_WIDESCREEN_FIX_HPP

#include <utility>

namespace Chimera {
    /**
     * Set whether or not to have the widescreen fix
     * @param new_setting setting for the widescreen fix
     */
    void set_widescreen_fix(bool new_setting) noexcept;

    /**
     * Get whether the widescreen fix is enabled
     * @return true if the widescreen fix is enabled
     */
    bool widescreen_fix_enabled() noexcept;
    
    /**
     * Get the widget coordinate values of the left and right bounds of the client window.
     * @return The left and right bounds, in that order.
     */
    std::pair<long, long> get_widescreen_horizontal_extents() noexcept;
    
    /**
     * Get the widget coordinate values of the top and bottom bounds of the client window.
     * @return The top and bottom bounds, in that order.
     */
    std::pair<long, long> get_widescreen_vertical_extents() noexcept;
}

#endif
