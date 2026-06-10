// Dear ImGui renderer backend for bgfx.
//
// Derived from bgfx's example backend (examples/common/imgui/imgui.cpp), reduced
// to a standalone renderer that compiles against vanilla Dear ImGui (docking) and
// implements the 1.92 ImTextureData texture API. No ImGuizmo, no embedded fonts,
// no platform/input handling — pair it with imgui_impl_sdl3 for the platform side.
//
// Usage (App owns the ImGui context):
//   ImGui::CreateContext();
//   ImGui_ImplSDL3_InitForOther(window);
//   ImGui_ImplBgfx_Init(viewId);
//   ... per frame ...
//   ImGui_ImplBgfx_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
//   ... build UI ...
//   ImGui::Render(); ImGui_ImplBgfx_RenderDrawData(ImGui::GetDrawData());
//   ... shutdown ...
//   ImGui_ImplBgfx_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext();
#pragma once

#include <imgui.h>
#include <bgfx/bgfx.h>

// viewId is the bgfx view the ImGui draw data is submitted to (the editor uses 255).
bool ImGui_ImplBgfx_Init(bgfx::ViewId viewId);
void ImGui_ImplBgfx_Shutdown();
void ImGui_ImplBgfx_NewFrame();
void ImGui_ImplBgfx_RenderDrawData(ImDrawData* drawData);

// Change the target view id (the backend defaults to whatever was passed to Init).
void ImGui_ImplBgfx_SetViewId(bgfx::ViewId viewId);
