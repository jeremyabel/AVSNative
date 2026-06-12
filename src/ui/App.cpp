#include "ui/App.h"

#include "engine/AudioAnalyzer.h"
#include "engine/Effect.h"
#include "engine/EffectChain.h"
#include "engine/Engine.h"
#include "engine/KeyInput.h"
#include "engine/Preset.h"
#include "ui/ChainPanel.h"
#include "ui/ConfigPanel.h"
#include "ui/ConfigUi.h"
#include "ui/SlidersPanel.h"
#include "ui/FileDialog.h"

// Self-hosted Dear ImGui (docking) + our own backends.
#include <imgui.h>
#include <imgui_internal.h>            // DockBuilder* for the first-run default layout
#include <imgui_impl_sdl3.h>
#include "ui/backends/imgui_impl_bgfx.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <filesystem>

// Helper: get the platform-native window handle from an SDL_Window.
static void* NativeWindowHandle(SDL_Window* win)
{
#if defined(_WIN32)
    return SDL_GetPointerProperty(SDL_GetWindowProperties(win),
                                  SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__)
    return SDL_GetPointerProperty(SDL_GetWindowProperties(win),
                                  SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#else
    return SDL_GetPointerProperty(SDL_GetWindowProperties(win),
                                  SDL_PROP_WINDOW_X11_WINDOW_NUMBER, nullptr);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────

App::App(Engine& InEngine)
    : m_engine(InEngine)
{}

App::~App()
{
    Shutdown();
}

void App::Run(const char* PresetPath)
{
    Init(PresetPath);

    constexpr Uint64 kFrameTargetNs = 1'000'000'000ULL / 60; // 60 fps cap

    while (m_running)
    {
        const Uint64 frameStart = SDL_GetTicksNS();

        ProcessEvents();

        if (m_pendingLoad)
        {
            m_pendingLoad = false;
            std::string path = FileDialog::Open("Load Preset");
            if (!path.empty())
            {
                m_engine.GetChain().Clear();
                if (Preset::Load(path.c_str(), m_engine))
                {
                    m_presetPath = path;
                    m_selectedChain  = nullptr;
                    m_selectedEffect = -1;
                }
            }
        }

        if (m_pendingSave)
        {
            m_pendingSave = false;
            std::string path = FileDialog::Save("Save Preset",
                m_presetPath.empty() ? nullptr : m_presetPath.c_str());
            // Presets are .avsz bundles — append the extension if the user omitted it.
            if (!path.empty())
            {
                const size_t slash = path.find_last_of("/\\");
                const size_t dot    = path.find_last_of('.');
                const bool hasExt = (dot != std::string::npos &&
                                     (slash == std::string::npos || dot > slash));
                if (!hasExt)
                    path += ".avsz";
            }
            if (!path.empty() && Preset::Save(path.c_str(), m_engine))
            {
                m_presetPath = path;
                if (m_pendingNewAfterSave)
                {
                    m_pendingNewAfterSave = false;
                    ClearPreset();
                }
            }
            else
            {
                m_pendingNewAfterSave = false; // dialog cancelled — abort New
            }
        }

        if (m_pendingAudioFile)
        {
            m_pendingAudioFile = false;
            static const SDL_DialogFileFilter kMp3Filter[] = {
                { "MP3 Audio", "mp3" },
                { "All Files", "*"   },
            };
            std::string path = FileDialog::Open("Load MP3", kMp3Filter, 2);
            if (!path.empty())
            {
                m_audioCaptureIdx = -1; // deselect device entry
                m_engine.GetAudio().ConnectFile(path);
            }
        }

        // Apply a deferred output-resize (from the render-scale slider) before Tick,
        // outside any submitted frame, so bgfx resources aren't recreated mid-frame.
        if (m_outputSizingDirty)
        {
            m_outputSizingDirty = false;
            ApplyOutputSizing();
        }

        m_engine.Tick();
        RenderUI();
        bgfx::frame();

        if (m_limitFramerate)
        {
            const Uint64 elapsed = SDL_GetTicksNS() - frameStart;
            if (elapsed < kFrameTargetNs)
                SDL_DelayNS(kFrameTargetNs - elapsed);
        }
    }

    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────

void App::Init(const char* PresetPath)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    // Editor window — hosts ImGui panels; this is the bgfx primary surface.
    // HIGH_PIXEL_DENSITY requests a native-resolution backing on HiDPI displays
    // (Retina etc.) so the UI renders crisply rather than being upscaled by the OS.
    m_editorWin = SDL_CreateWindow("AVS Editor",
                                   m_editorWidth, m_editorHeight,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!m_editorWin)
    {
        fprintf(stderr, "SDL_CreateWindow (editor) failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    // Output window — shows the AVS renderer output.
    m_outputWin = SDL_CreateWindow("AVS Output",
                                   m_outputWidth, m_outputHeight,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!m_outputWin)
    {
        fprintf(stderr, "SDL_CreateWindow (output) failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(m_editorWin);
        m_editorWin = nullptr;
        SDL_Quit();
        return;
    }

    // bgfx initialises against the editor window (primary swapchain).
#if defined(__APPLE__)
    // Force bgfx into single-threaded mode (render on this, the main thread).
    // Calling renderFrame() once before init() disables the separate render
    // thread. On macOS that thread is fatal: bgfx creates the swapchain's
    // CAMetalLayer by mutating the NSWindow's content view, which Cocoa only
    // permits on the main thread — off-thread it fails silently and both
    // windows stay blank white. The loop still only calls bgfx::frame(), which
    // drives the render internally in single-threaded mode.
    bgfx::renderFrame();
#endif
    // Track the editor backbuffer in PIXELS (HiDPI-aware), not points: bgfx renders
    // to the native-resolution swapchain. SDL window/event sizes are in points.
    SDL_GetWindowSizeInPixels(m_editorWin, &m_editorWidth, &m_editorHeight);

    bgfx::Init GfxInit;
    GfxInit.type                   = bgfx::RendererType::Vulkan;
    GfxInit.platformData.nwh       = NativeWindowHandle(m_editorWin);
    GfxInit.resolution.width       = (uint32_t)m_editorWidth;
    GfxInit.resolution.height      = (uint32_t)m_editorHeight;
    GfxInit.resolution.reset       = BGFX_RESET_VSYNC;

    if (!bgfx::init(GfxInit))
    {
        fprintf(stderr, "bgfx::init failed\n");
        SDL_DestroyWindow(m_outputWin);  m_outputWin  = nullptr;
        SDL_DestroyWindow(m_editorWin);  m_editorWin  = nullptr;
        SDL_Quit();
        return;
    }

    // Our shader blobs are SPIRV-only. If bgfx fell back to another backend
    // (e.g. Metal because MoltenVK was unavailable), every shader create would
    // fail with an opaque fatal — bail out with an actionable message instead.
    if (bgfx::getRendererType() != bgfx::RendererType::Vulkan)
    {
        fprintf(stderr,
                "bgfx selected the %s renderer, but only Vulkan/SPIRV shaders are "
                "built. Install MoltenVK (`brew install molten-vk`) for Vulkan on macOS.\n",
                bgfx::getRendererName(bgfx::getRendererType()));
        bgfx::shutdown();
        SDL_DestroyWindow(m_outputWin);  m_outputWin  = nullptr;
        SDL_DestroyWindow(m_editorWin);  m_editorWin  = nullptr;
        SDL_Quit();
        return;
    }

    // Engine renders at output-window resolution.
    EngineConfig Cfg;
    Cfg.Width  = m_outputWidth;
    Cfg.Height = m_outputHeight;
    if (!m_engine.Init(Cfg, bgfx::getRendererType()))
    {
        fprintf(stderr, "Engine::Init failed\n");
        bgfx::shutdown();
        SDL_DestroyWindow(m_outputWin);  m_outputWin  = nullptr;
        SDL_DestroyWindow(m_editorWin);  m_editorWin  = nullptr;
        SDL_Quit();
        return;
    }

    // Create the output framebuffer (window pixel size) and set the engine's render
    // resolution from the render-percentage. Both are HiDPI-aware.
    ApplyOutputSizing();

    if (PresetPath)
    {
        Preset::Load(PresetPath, m_engine);
        m_presetPath = PresetPath;
    }

    // Populate audio device list and connect to the system default recording device.
    RefreshAudioDevices();
    if (!m_audioCaptureIds.empty())
    {
        m_audioCaptureIdx = 0;
        m_engine.GetAudio().ConnectDevice((SDL_AudioDeviceID)m_audioCaptureIds[0]);
    }

    // ── Dear ImGui: context + platform (SDL3) + renderer (bgfx, view 255) ───────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    // Snapshot the pristine style so UI-scale changes always derive from a clean base
    // (ScaleAllSizes is cumulative). Then apply the initial scale.
    m_baseStyle = new ImGuiStyle(ImGui::GetStyle());
    ApplyUiScale();

    // Build the default dock layout only if there's no saved imgui.ini yet.
    m_buildDefaultLayout = !std::filesystem::exists("imgui.ini");

    ImGui_ImplSDL3_InitForOther(m_editorWin);   // bgfx owns rendering → "Other" variant
    ImGui_ImplBgfx_Init(255);

    m_running = true;
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void App::Shutdown()
{
    // Guard against double-calls (destructor after Run, or failed Init).
    if (!m_editorWin && !m_outputWin)
        return;

    m_running = false;

    ImGui_ImplBgfx_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    delete m_baseStyle;
    m_baseStyle = nullptr;

    m_engine.Shutdown();

    if (bgfx::isValid(m_outputFB))
    {
        bgfx::destroy(m_outputFB);
        m_outputFB = BGFX_INVALID_HANDLE;
    }

    bgfx::shutdown();

    if (m_outputWin)
    {
        SDL_DestroyWindow(m_outputWin);
        m_outputWin = nullptr;
    }
    if (m_editorWin)
    {
        SDL_DestroyWindow(m_editorWin);
        m_editorWin = nullptr;
    }
    SDL_Quit();
}

// ─── ClearPreset ─────────────────────────────────────────────────────────────

void App::ClearPreset()
{
    m_engine.GetChain().Clear();
    m_presetPath     = "";
    m_selectedChain  = nullptr;
    m_selectedEffect = -1;
}

// ─── Resize helpers ───────────────────────────────────────────────────────────

void App::ApplyEditorSizing()
{
    SDL_GetWindowSizeInPixels(m_editorWin, &m_editorWidth, &m_editorHeight);
    bgfx::reset((uint32_t)m_editorWidth, (uint32_t)m_editorHeight, BGFX_RESET_VSYNC);
}

void App::ApplyOutputSizing()
{
    // Output framebuffer matches the window's pixel size (HiDPI-aware); the engine's
    // internal render resolution is that scaled by the render percentage. The window
    // size is unchanged — the blit upscales/downscales to fill it.
    SDL_GetWindowSizeInPixels(m_outputWin, &m_outputPixelW, &m_outputPixelH);
    m_outputPixelW = std::max(1, m_outputPixelW);
    m_outputPixelH = std::max(1, m_outputPixelH);

    if (bgfx::isValid(m_outputFB))
        bgfx::destroy(m_outputFB);
    m_outputFB = bgfx::createFrameBuffer(NativeWindowHandle(m_outputWin),
                                         (uint16_t)m_outputPixelW,
                                         (uint16_t)m_outputPixelH);
    m_engine.SetOutputFrameBuffer(m_outputFB);
    m_engine.SetOutputViewport(m_outputPixelW, m_outputPixelH);

    const int rw = std::max(1, m_outputPixelW * m_outputRenderPct / 100);
    const int rh = std::max(1, m_outputPixelH * m_outputRenderPct / 100);
    m_engine.Resize(rw, rh);
}

float App::AutoUiScale() const
{
    // Native HiDPI already renders at the monitor's pixel density and lays the UI out
    // in points, so the content is correctly sized at 1.0 — no extra zoom needed.
    return 1.0f;
}

void App::ApplyUiScale()
{
    if (!m_baseStyle)
        return;
    const float scale = (m_uiScalePct == 0) ? AutoUiScale()
                                            : (float)m_uiScalePct / 100.0f;
    ImGuiStyle s = *m_baseStyle;
    s.ScaleAllSizes(scale);        // spacing/padding/rounding (not fonts)
    s.FontScaleMain = scale;       // 1.92 dynamic fonts → crisp at any scale
    ImGui::GetStyle() = s;
    m_uiScaleDirty = false;
}

// ─── ProcessEvents ────────────────────────────────────────────────────────────

void App::ProcessEvents()
{
    const SDL_WindowID editorID = SDL_GetWindowID(m_editorWin);
    const SDL_WindowID outputID = SDL_GetWindowID(m_outputWin);

    // Reset the per-frame keyboard state before draining events; effects poll it in
    // Engine::Tick and the config UI polls it in RenderUI, both later this iteration.
    avs::KeyInput::BeginFrame();

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        // The SDL3 platform backend handles all ImGui input (mouse/keyboard/text/
        // focus/clipboard). It only acts on events for the editor window it owns.
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            m_running = false;
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            // Closing either window quits the app.
            if (event.window.windowID == editorID ||
                event.window.windowID == outputID)
                m_running = false;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            // RESIZED carries the window size in POINTS — used only to display the
            // output window's logical size. The framebuffer is sized from the PIXEL
            // event below (authoritative on HiDPI, and fires when moving between
            // monitors of differing density without a points change).
            if (event.window.windowID == outputID)
            {
                m_outputWidth  = event.window.data1;
                m_outputHeight = event.window.data2;
            }
            break;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (event.window.windowID == editorID)
                ApplyEditorSizing();
            else if (event.window.windowID == outputID)
                ApplyOutputSizing();
            break;

        case SDL_EVENT_KEY_DOWN:
            // ESC quits, unless a text field has keyboard focus.
            if (event.key.windowID == editorID &&
                event.key.scancode == SDL_SCANCODE_ESCAPE &&
                !ImGui::GetIO().WantCaptureKeyboard)
                m_running = false;

            // Feed key-DOWN edges to effects' keyboard-driven image switching (and
            // the press-to-capture UI). Switching is key-down only — key-up is never
            // forwarded. !repeat collapses auto-repeat to a single edge. We gate on
            // WantTextInput (true only while a text field/code editor is actively
            // focused), NOT WantCaptureKeyboard — the latter is always true in the
            // editor window because keyboard nav is enabled, which would block every
            // key. WantTextInput keeps keys typed into code editors from triggering
            // swaps while still letting hotkeys and press-to-capture through.
            if (!ImGui::GetIO().WantTextInput)
            {
                if (!event.key.repeat)
                    avs::KeyInput::PushKeyDown((uint32_t)event.key.key);
                // Held state (for hold-to-trigger effects like Strobe). Repeats are
                // harmless here — already held.
                avs::KeyInput::SetKeyDown((uint32_t)event.key.key, true);
            }
            break;

        case SDL_EVENT_KEY_UP:
            // Always release, even while a text field is focused, so a key can never
            // get stuck "held".
            avs::KeyInput::SetKeyDown((uint32_t)event.key.key, false);
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            // The OS delivers key-up to whoever has focus, so a key held as we lose
            // focus would otherwise stay stuck. Drop all held keys.
            avs::KeyInput::ClearHeld();
            break;

        default:
            break;
        }
    }

    // Remember the most recent key for the status-bar reference readout (same keys
    // KeyInput exposes to scripts/bindings — i.e. gated by !WantTextInput above).
    if (uint32_t k = avs::KeyInput::LastKeyPressed(); k != 0)
        m_lastKey = k;

    // Begin the ImGui frame (renderer first, then platform, then ImGui core).
    ImGui_ImplBgfx_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

// ─── RenderUI ────────────────────────────────────────────────────────────────

void App::RenderUI()
{
    // Apply a pending UI-scale change before any window is drawn this frame.
    if (m_uiScaleDirty)
        ApplyUiScale();

    bgfx::setViewClear(255, BGFX_CLEAR_COLOR, 0x252526ff);
    bgfx::setViewFrameBuffer(255, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(255, 0, 0, (uint16_t)m_editorWidth, (uint16_t)m_editorHeight);
    bgfx::touch(255);

    // ── Menu bar ──────────────────────────────────────────────────────────────
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New..."))
                m_showNewConfirm = true;

            if (ImGui::MenuItem("Load..."))
                m_pendingLoad = true;

            ImGui::Separator();

            bool canSave = !m_presetPath.empty();
            if (ImGui::MenuItem("Save", nullptr, false, canSave))
                Preset::Save(m_presetPath.c_str(), m_engine);

            if (ImGui::MenuItem("Save As..."))
                m_pendingSave = true;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Options"))
        {
            if (ImGui::MenuItem("Options..."))
            {
                m_showOptions = true;
                m_pendingOutputW = m_outputWidth;
                m_pendingOutputH = m_outputHeight;
                RefreshAudioDevices();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Sliders", nullptr, &m_showSliders);

            if (ImGui::BeginMenu("UI Scale"))
            {
                if (ImGui::MenuItem("Auto", nullptr, m_uiScalePct == 0))
                {
                    m_uiScalePct = 0;
                    m_uiScaleDirty = true;
                }
                ImGui::Separator();
                static const int kScales[] = { 100, 125, 150, 200 };
                for (int pct : kScales)
                {
                    char label[16];
                    std::snprintf(label, sizeof(label), "%d%%", pct);
                    if (ImGui::MenuItem(label, nullptr, m_uiScalePct == pct))
                    {
                        m_uiScalePct = pct;
                        m_uiScaleDirty = true;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        // Current path hint on the right side of the menu bar.
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(m_presetPath.empty() ? "(unsaved)" : m_presetPath.c_str()).x - 8.0f);
        ImGui::TextDisabled("%s", m_presetPath.empty() ? "(unsaved)" : m_presetPath.c_str());

        ImGui::EndMainMenuBar();
    }

    // Status bar (bottom side bar) is created before the dockspace so the dockspace
    // reserves the remaining work area for the dockable panels.
    RenderStatusBar();

    const ImGuiID dockId = ImGui::DockSpaceOverViewport(
        0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    if (m_buildDefaultLayout)
    {
        m_buildDefaultLayout = false;
        BuildDefaultDockLayout(dockId);
    }

    if (m_showOptions)
        RenderOptionsWindow();

    // ── New-preset confirmation modal ─────────────────────────────────────────
    // OpenPopup must be called in the same window context as BeginPopupModal, so
    // we use a flag set by the menu item and call OpenPopup here in the main frame.
    if (m_showNewConfirm)
    {
        m_showNewConfirm = false;
        ImGui::OpenPopup("##new_confirm");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##new_confirm", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::Text("Create a new preset?");
        ImGui::Text("Unsaved changes will be lost.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // "Save" — write to the known path immediately, or open Save As if no path.
        if (ImGui::Button("Save"))
        {
            if (!m_presetPath.empty())
            {
                Preset::Save(m_presetPath.c_str(), m_engine);
                ClearPreset();
            }
            else
            {
                // No path yet: open the Save As dialog next loop iteration,
                // then clear once the dialog confirms a destination.
                m_pendingSave         = true;
                m_pendingNewAfterSave = true;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Don't Save"))
        {
            ClearPreset();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    ChainPanel::Render(m_engine, m_engine.GetChain(), m_selectedChain, m_selectedEffect);

    EffectEntry* Selected = nullptr;
    if (m_selectedChain && m_selectedEffect >= 0 &&
        m_selectedEffect < m_selectedChain->Count())
        Selected = &m_selectedChain->GetEntry(m_selectedEffect);

    ConfigPanel::Render(m_engine, Selected);
    ConfigPanel::RenderLockedPanels(m_engine, m_engine.GetChain());

    if (m_showSliders)
        SlidersPanel::Render(m_engine, &m_showSliders);

    // Drop editor state for any effect that wasn't drawn this frame.
    ConfigUi::EndFramePrune();

    // Submit all ImGui draw data to bgfx view 255.
    ImGui::Render();
    ImGui_ImplBgfx_RenderDrawData(ImGui::GetDrawData());
}

// ─── Dock layout ──────────────────────────────────────────────────────────────
// First-run default: Effect Chain docked left (~30%), Properties filling the rest.
// Only called when no imgui.ini exists; afterwards the layout persists via the ini.

void App::BuildDefaultDockLayout(unsigned int dockId)
{
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID center = dockId;
    const ImGuiID left = ImGui::DockBuilderSplitNode(
        center, ImGuiDir_Left, 0.30f, nullptr, &center);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(
        center, ImGuiDir_Down, 0.25f, nullptr, &center);

    ImGui::DockBuilderDockWindow("Effect Chain", left);
    ImGui::DockBuilderDockWindow("Properties",   center);
    ImGui::DockBuilderDockWindow("Sliders",      bottom);
    ImGui::DockBuilderFinish(dockId);
}

// ─── Status bar ───────────────────────────────────────────────────────────────
// Bottom side bar attached to the main viewport. Shows the smoothed render-loop
// framerate (engine tick + UI run in lockstep in Run()).

void App::RenderStatusBar()
{
    const float barH = ImGui::GetFrameHeight();
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_MenuBar;

    if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(),
                                    ImGuiDir_Down, barH, flags))
    {
        if (ImGui::BeginMenuBar())
        {
            const ImGuiIO& io = ImGui::GetIO();
            const float ms = (io.Framerate > 0.0f) ? 1000.0f / io.Framerate : 0.0f;
            ImGui::Text("FPS: %.1f  (%.2f ms/frame)", io.Framerate, ms);

            // Last-pressed key (for reference when binding keys / using key() in scripts).
            ImGui::SameLine();
            if (m_lastKey != 0)
            {
                const char* name = SDL_GetKeyName((SDL_Keycode)m_lastKey);
                ImGui::Text("   |   Last key: %s (%u)", (name && name[0]) ? name : "?", m_lastKey);
            }
            else
            {
                ImGui::TextDisabled("   |   Last key: (none)");
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

// ─── Options helpers ──────────────────────────────────────────────────────────

void App::RefreshAudioDevices()
{
    m_audioCaptureIds.clear();
    m_audioCaptureNames.clear();

    auto devs = AudioAnalyzer::GetCaptureDevices();
    for (auto& d : devs)
    {
        m_audioCaptureIds.push_back((uint32_t)d.Id);
        m_audioCaptureNames.push_back(d.Name);
    }

    // Keep current selection index in range.
    if (m_audioCaptureIdx >= (int)m_audioCaptureIds.size())
        m_audioCaptureIdx = m_audioCaptureIds.empty() ? -1 : 0;
}

void App::RenderOptionsWindow()
{
    ImGui::SetNextWindowSize(ImVec2(480, 280), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Options", &m_showOptions, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Performance");
    ImGui::Checkbox("Limit framerate to 60 fps", &m_limitFramerate);

    ImGui::SeparatorText("Audio Input");

    // ── Device combo ──────────────────────────────────────────────────────────
    if (m_audioCaptureIds.empty())
    {
        ImGui::TextDisabled("No recording devices found");
    }
    else
    {
        const char* preview = (m_audioCaptureIdx >= 0)
            ? m_audioCaptureNames[m_audioCaptureIdx].c_str()
            : "(none)";

        ImGui::SetNextItemWidth(-80.0f);
        if (ImGui::BeginCombo("##audiodev", preview))
        {
            for (int i = 0; i < (int)m_audioCaptureNames.size(); i++)
            {
                bool selected = (i == m_audioCaptureIdx);
                if (ImGui::Selectable(m_audioCaptureNames[i].c_str(), selected))
                {
                    m_audioCaptureIdx = i;
                    m_engine.GetAudio().ConnectDevice(
                        (SDL_AudioDeviceID)m_audioCaptureIds[i]);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
        {
            RefreshAudioDevices();
            // Reconnect if the previously selected device is still present.
            if (m_audioCaptureIdx >= 0)
                m_engine.GetAudio().ConnectDevice(
                    (SDL_AudioDeviceID)m_audioCaptureIds[m_audioCaptureIdx]);
        }
    }

    // ── MP3 file input ────────────────────────────────────────────────────────
    ImGui::Spacing();
    if (ImGui::Button("Load MP3..."))
        m_pendingAudioFile = true;

    // ── Status indicator ──────────────────────────────────────────────────────
    ImGui::SameLine();
    if (m_engine.GetAudio().IsFileMode())
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Playing MP3");
    else if (m_engine.GetAudio().IsConnected())
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Device active");
    else
        ImGui::TextDisabled("Not connected");

    // ── Output size ───────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::SeparatorText("Output Size");

    static const struct { const char* label; int w, h; } kPresets[] = {
        { "320 x 240",   320,  240 },
        { "640 x 480",   640,  480 },
        { "800 x 600",   800,  600 },
        { "1280 x 720",  1280, 720 },
        { "1920 x 1080", 1920, 1080 },
    };

    // Preset buttons
    for (const auto& p : kPresets)
    {
        if (ImGui::Button(p.label))
        {
            m_pendingOutputW = p.w;
            m_pendingOutputH = p.h;
        }
        ImGui::SameLine();
    }
    ImGui::NewLine();

    // Custom W × H inputs + Apply
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputInt("##ow", &m_pendingOutputW, 0);
    m_pendingOutputW = std::max(1, m_pendingOutputW);
    ImGui::SameLine();
    ImGui::TextUnformatted("x");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputInt("##oh", &m_pendingOutputH, 0);
    m_pendingOutputH = std::max(1, m_pendingOutputH);
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
        SDL_SetWindowSize(m_outputWin, m_pendingOutputW, m_pendingOutputH);

    ImGui::SameLine();
    ImGui::TextDisabled("(current: %d x %d)", m_outputWidth, m_outputHeight);

    // ── Output render scale ───────────────────────────────────────────────────
    // Internal render resolution as a % of the output window's pixel size. The
    // window size is unchanged; lower = cheaper/softer, higher = supersampled.
    ImGui::Spacing();
    ImGui::SeparatorText("Output Render Scale");
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::SliderInt("##renderpct", &m_outputRenderPct, 25, 200, "%d%%"))
    {
        m_outputRenderPct = std::max(1, m_outputRenderPct);
        // Defer: recreating the output FBO / resizing engine FBOs mid-frame (after
        // Tick already submitted draws referencing them) crashes. Apply before Tick.
        m_outputSizingDirty = true;
    }
    const int rw = std::max(1, m_outputPixelW * m_outputRenderPct / 100);
    const int rh = std::max(1, m_outputPixelH * m_outputRenderPct / 100);
    ImGui::SameLine();
    ImGui::TextDisabled("(render: %d x %d)", rw, rh);

    ImGui::End();
}
