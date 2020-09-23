// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHIMERA_WIN32_WINDOW_HPP
#define CHIMERA_WIN32_WINDOW_HPP

#include <windows.h>

namespace Chimera {
    /**
     * Basic application window information.
     */
    struct WindowGlobals {
        HINSTANCE hInstance;   ///< A handle to the application instance.
        HWND      hWnd;        ///< Halo's main window handle.
        HWND      hWndUnknown; // possibly used for error dialog menus?
        int       nCmdShow;    ///< `wShow` from `GetStartupInfo()`, if the `STARTF_USESHOWWINDOW` flag is set.
                               ///< Otherwise, takes on the value `SW_SHOWDEFAULT`.
        WNDPROC   lpfnWndProc; ///< The WindowProc callback function as registered with the window class.
        HICON     hIconSm;     ///< Halo's small icon resource.
        
        /**
         * Get the window globals.
         * @return reference to the window globals.
         */
        static WindowGlobals &get_window_globals();
    };
    static_assert(sizeof(WindowGlobals) == 0x18);
}

#endif
