// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHIMERA_IMGUI_IMPL_BLAM_HPP
#define CHIMERA_IMGUI_IMPL_BLAM_HPP

/**
 * Initializes the platform implementation for Dear ImGui over Halo.
 *
 * @return `true` if the implementation was initialized, otherwise `false`.
 */
bool ImGui_ImplBlam_Init();

/**
 * Destroys resources associated with the platform implementation.
 *
 * This function is safe to call if #ImGui_ImplBlam_Init() was not called
 * or it returned `false`.
 */
void ImGui_ImplBlam_Shutdown();

/**
 * Performs platform-specific operations associated with starting a new frame.
 *
 * This largely includes filling out a few IO structures.
 */
void ImGui_ImplBlam_NewFrame();

/**
 * Diverts input from Halo as necessary.
 *
 * This should be called just after `ImGui::EndFrame()`.
 */
void ImGui_ImplBlam_CaptureInput();

#endif