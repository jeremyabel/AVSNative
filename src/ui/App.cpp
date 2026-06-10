#include "ui/App.h"

#include "engine/AudioAnalyzer.h"
#include "engine/Effect.h"
#include "engine/EffectChain.h"
#include "engine/Engine.h"
#include "engine/Preset.h"
#include "ui/ChainPanel.h"
#include "ui/ConfigPanel.h"
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

    while (m_running)
    {
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
            if (!path.empty() && Preset::Save(path.c_str(), m_engine))
                m_presetPath = path;
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

        m_engine.Tick();
        RenderUI();
        bgfx::frame();
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
    m_editorWin = SDL_CreateWindow("AVS Editor",
                                   m_editorWidth, m_editorHeight,
                                   SDL_WINDOW_RESIZABLE);
    if (!m_editorWin)
    {
        fprintf(stderr, "SDL_CreateWindow (editor) failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    // Output window — shows the AVS renderer output.
    m_outputWin = SDL_CreateWindow("AVS Output",
                                   m_outputWidth, m_outputHeight,
                                   SDL_WINDOW_RESIZABLE);
    if (!m_outputWin)
    {
        fprintf(stderr, "SDL_CreateWindow (output) failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(m_editorWin);
        m_editorWin = nullptr;
        SDL_Quit();
        return;
    }

    // bgfx initialises against the editor window (primary swapchain).
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

    // Create a bgfx framebuffer backed by the output window's native handle.
    m_outputFB = bgfx::createFrameBuffer(NativeWindowHandle(m_outputWin),
                                         (uint16_t)m_outputWidth,
                                         (uint16_t)m_outputHeight);
    m_engine.SetOutputFrameBuffer(m_outputFB);

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

// ─── Resize helpers ───────────────────────────────────────────────────────────

void App::ResizeOutput(int32_t Width, int32_t Height)
{
    m_outputWidth  = Width;
    m_outputHeight = Height;

    // Recreate the output framebuffer at the new size.
    if (bgfx::isValid(m_outputFB))
        bgfx::destroy(m_outputFB);

    m_outputFB = bgfx::createFrameBuffer(NativeWindowHandle(m_outputWin),
                                         (uint16_t)Width, (uint16_t)Height);
    m_engine.SetOutputFrameBuffer(m_outputFB);
    m_engine.Resize(Width, Height);
}

// ─── ProcessEvents ────────────────────────────────────────────────────────────

void App::ProcessEvents()
{
    const SDL_WindowID editorID = SDL_GetWindowID(m_editorWin);
    const SDL_WindowID outputID = SDL_GetWindowID(m_outputWin);

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
            if (event.window.windowID == editorID)
            {
                m_editorWidth  = event.window.data1;
                m_editorHeight = event.window.data2;
                bgfx::reset((uint32_t)m_editorWidth, (uint32_t)m_editorHeight,
                             BGFX_RESET_VSYNC);
            }
            else if (event.window.windowID == outputID)
            {
                ResizeOutput(event.window.data1, event.window.data2);
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            // ESC quits, unless a text field has keyboard focus.
            if (event.key.windowID == editorID &&
                event.key.scancode == SDL_SCANCODE_ESCAPE &&
                !ImGui::GetIO().WantCaptureKeyboard)
                m_running = false;
            break;

        default:
            break;
        }
    }

    // Begin the ImGui frame (renderer first, then platform, then ImGui core).
    ImGui_ImplBgfx_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

// ─── RenderUI ────────────────────────────────────────────────────────────────

void App::RenderUI()
{
    bgfx::setViewClear(255, BGFX_CLEAR_COLOR, 0x252526ff);
    bgfx::setViewFrameBuffer(255, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(255, 0, 0, (uint16_t)m_editorWidth, (uint16_t)m_editorHeight);
    bgfx::touch(255);

    // ── Menu bar ──────────────────────────────────────────────────────────────
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
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

    ChainPanel::Render(m_engine, m_engine.GetChain(), m_selectedChain, m_selectedEffect);

    Effect* Selected = nullptr;
    if (m_selectedChain && m_selectedEffect >= 0 &&
        m_selectedEffect < m_selectedChain->Count())
        Selected = m_selectedChain->GetEntry(m_selectedEffect).Effect.get();

    ConfigPanel::Render(m_engine, Selected);

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

    ImGui::DockBuilderDockWindow("Effect Chain", left);
    ImGui::DockBuilderDockWindow("Properties",   center);
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

    ImGui::End();
}
