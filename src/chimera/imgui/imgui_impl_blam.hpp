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
 * This function is safe to call, even if the platform implementation was not initialized.
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
 * This should be called after `ImGui::EndFrame()` and before the widgets get updated.
 */
void ImGui_ImplBlam_CaptureInput();

#endif