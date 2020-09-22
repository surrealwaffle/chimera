;# SPDX-License-Identifier: GPL-3.0-only

.intel_syntax noprefix

.text

.globl imgui_impl_blam_draw_widget_cursor
imgui_impl_blam_draw_widget_cursor:
    mov eax, _imgui_impl_blam_hide_widget_cursor
    test eax, eax
    jz _imgui_impl_blam_draw_widget_cursor_do_draw_cursor
    ret
    _imgui_impl_blam_draw_widget_cursor_do_draw_cursor:
    jmp [imgui_impl_blam_draw_widget_cursor_original]