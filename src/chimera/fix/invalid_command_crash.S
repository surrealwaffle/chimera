;# SPDX-License-Identifier: GPL-3.0-only

.intel_syntax noprefix

.data
meme_global_text: .string "rasterizer_soft_filter"
ignore_broken_globals: .long 0

.text

;# Some information on the hook site:
;# Halo calls on the function in which the hook site is located, i.e. a caller of halo_get_global_index_fn,
;# in order to compile a script node to (possibly) a global. I will dub this function as halo_bind_global_script_node.
;# Signature: byte /*AL*/ halo_bind_global_script_node(HaloID script_node /*EAX*/);
;#  In GCC: byte halo_bind_global_script_node(HaloID script_node) __attribute__((cdecl, regparm(1)));
;# 0 is returned if the global under the text of the node cannot be found or the global has incompatible type with the expected type of the script node.
;#    If the global is found but has incompatible type, then an error is reported (see invalid_global_crash_compile_error_report).
;#    When this happens, the global bind is said to fail with error.
;# Otherwise, this functions binds the script node to the global variable and non-zero is returned.
;#
;# It is important to note that halo_bind_global_script_node is called on nodes which may contain literals before attempting to parse their text as literals.
;# Thus if a global called 0 could be introduced, then we could effectively replace the literal 0 via a recompile.
;#
;# There are two call sites of halo_bind_global_script_node.
;#  1. When a global is expected, e.g. for the first script node in mouse_acceleration 0.
;#     In this case, halo_get_global_index_fn is called directly on mouse_acceleration before calling halo_bind_global_script_node on the node, circumventing the hook below.
;#     If that call fails to produce a global, then the compile stops and returns an error ("this is not a valid global variable").
;#     Thus by the time we get to the code below through this path, we may assume that ax does not compare equal to 0xFFFF.
;#  2. When a value is expected, e.g. for the second script node in mouse_acceleration 0 or mouse_acceleration real_global.
;#     If the global bind fails with error, then the compile stops.
;#     If the global bind succeeds, then the compile proceeds.
;#     If the global bind fails with no error and a global is required for the node (a strange flag sets this), then an error is reported ("this is not a valid variable name") and the compile stops.
;#     Otherwise, the script node text is parsed as a literal.
;#        If it cannot be parsed as a literal, an error is reported (the message is dependent on the type being parsed) and the compile stops.
;#        Otherwise, the compile proceeds.

.globl _handle_invalid_global_crash_asm
_handle_invalid_global_crash_asm:
    ;# Ignore?
    cmp dword ptr [ignore_broken_globals], 1
    je done_handle_invalid_global_crash_asm

    ;# Did we get 0xFFFF?
    cmp ax, 0xFFFF

    ;# If not, keep going
    jne done_handle_invalid_global_crash_asm
    
    ;# Test the script node to see if it is unparsed.
    ;# If the node is unparsed, do not try to parse it as a literal.
    mov cx, [esi+4]
    test cx, cx
    jz meme_fix_handle_invalid_global_crash_asm
    
    ;# See if the value type has a parsing function associated with it in the first place.
    movsx eax, cx
    shl eax, 2
    
    mov ecx, dword ptr [_halo_try_parse_node_literal_fn_list]
    add ecx, eax
    test ecx, ecx
    jz meme_fix_handle_invalid_global_crash_asm
    
    ;# Attempt to process the script node as a literal.
    ;# This requires some work to reconstruct the script node identity in full from the address.

    ;# Reconstruct the ID index.
    mov eax, esi
    mov edx, dword ptr [_invalid_global_crash_script_node_table_ptr]
    mov edx, dword ptr [edx]
    mov edx, dword ptr [edx+0x34]
    sub eax, edx
    ;# This is a div by 0x14.
    mov edx, 0xCCCCCCCD
    mul edx
    mov eax, edx
    shr eax, 4
    ;# Get the ID salt.
    and eax, 0xffff
    movzx edx, word ptr [esi]
    shl edx, 16
    or eax, edx
    ;# Attempt to process the node as a literal.
    ;# The call just below returns 0 in AL on failure, non-zero on success.
    ;# On success, we need to return 0xffff immediately,
    ;# so that the node does not compile to rasterizer_soft_filter.
    ;# Also save the script node value and error-reporting values.
    push ebx
    mov ebx, dword ptr [esi+0x10]
    push esi
    push edi
    push ebp
    mov ebp, dword ptr [_invalid_global_crash_compile_error_report]
    mov esi, dword ptr [ebp]   ;# reason
    mov edi, dword ptr [ebp+4] ;# position
    push eax
    call [ecx]
    add esp, 4
    mov dword ptr [ebp], esi
    mov dword ptr [ebp+4], edi
    pop ebp
    pop edi
    pop esi
    mov dword ptr [esi+0x10], ebx
    pop ebx
    mov ecx, eax
    xor eax, eax
    or ax, 0xffff
    test cl, cl
    jnz done_handle_invalid_global_crash_asm

    ;# Otherwise, try again, but this time put "rasterizer_soft_filter" into ebx as this effectively does nothing
    meme_fix_handle_invalid_global_crash_asm:
    lea ebx, [meme_global_text]
    call dword ptr [_halo_get_global_index_fn]

    ;# Done!
    done_handle_invalid_global_crash_asm:
    ret

.globl _should_ignore_broken_globals_asm
_should_ignore_broken_globals_asm:
    mov dword ptr [ignore_broken_globals], 0
    ret

.globl _should_not_ignore_broken_globals_asm
_should_not_ignore_broken_globals_asm:
    mov dword ptr [ignore_broken_globals], 0
    ret
