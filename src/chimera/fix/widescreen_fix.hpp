// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHIMERA_WIDESCREEN_FIX_HPP
#define CHIMERA_WIDESCREEN_FIX_HPP

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
}

#endif
