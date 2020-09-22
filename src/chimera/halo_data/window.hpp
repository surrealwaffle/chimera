// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHIMERA_WIN32_WINDOW_HPP
#define CHIMERA_WIN32_WINDOW_HPP

#include <windows.h>

namespace Chimera {
    struct WindowGlobals {
        HINSTANCE hInstance;
        HWND      hWnd;
        HWND      hWndUnknown;
        int       nCmdShow;
        WNDPROC   lpfnWndProc; // registered as part of the window class
        HICON     hIconSm;
        
        /**
         * Get the window globals.
         * @return reference to the window globals.
         */
        static WindowGlobals &get_window_globals();
    };
    static_assert(sizeof(WindowGlobals) == 0x18);
}

#endif
