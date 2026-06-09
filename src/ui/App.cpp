#include "ui/App.h"

#include "engine/AudioAnalyzer.h"
#include "engine/Effect.h"
#include "engine/EffectChain.h"
#include "engine/Engine.h"
#include "engine/Preset.h"
#include "ui/ChainPanel.h"
#include "ui/ConfigPanel.h"
#include "ui/FileDialog.h"

// bgfx imgui backend — also pulls in <dear-imgui/imgui.h>
#include <imgui/imgui.h>

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

// ─── SDL scancode → ImGuiKey ──────────────────────────────────────────────────

static ImGuiKey SdlScancodeToImGui(SDL_Scancode sc)
{
    switch (sc)
    {
    case SDL_SCANCODE_TAB:          return ImGuiKey_Tab;
    case SDL_SCANCODE_LEFT:         return ImGuiKey_LeftArrow;
    case SDL_SCANCODE_RIGHT:        return ImGuiKey_RightArrow;
    case SDL_SCANCODE_UP:           return ImGuiKey_UpArrow;
    case SDL_SCANCODE_DOWN:         return ImGuiKey_DownArrow;
    case SDL_SCANCODE_PAGEUP:       return ImGuiKey_PageUp;
    case SDL_SCANCODE_PAGEDOWN:     return ImGuiKey_PageDown;
    case SDL_SCANCODE_HOME:         return ImGuiKey_Home;
    case SDL_SCANCODE_END:          return ImGuiKey_End;
    case SDL_SCANCODE_INSERT:       return ImGuiKey_Insert;
    case SDL_SCANCODE_DELETE:       return ImGuiKey_Delete;
    case SDL_SCANCODE_BACKSPACE:    return ImGuiKey_Backspace;
    case SDL_SCANCODE_SPACE:        return ImGuiKey_Space;
    case SDL_SCANCODE_RETURN:       return ImGuiKey_Enter;
    case SDL_SCANCODE_ESCAPE:       return ImGuiKey_Escape;
    case SDL_SCANCODE_APOSTROPHE:   return ImGuiKey_Apostrophe;
    case SDL_SCANCODE_COMMA:        return ImGuiKey_Comma;
    case SDL_SCANCODE_MINUS:        return ImGuiKey_Minus;
    case SDL_SCANCODE_PERIOD:       return ImGuiKey_Period;
    case SDL_SCANCODE_SLASH:        return ImGuiKey_Slash;
    case SDL_SCANCODE_SEMICOLON:    return ImGuiKey_Semicolon;
    case SDL_SCANCODE_EQUALS:       return ImGuiKey_Equal;
    case SDL_SCANCODE_LEFTBRACKET:  return ImGuiKey_LeftBracket;
    case SDL_SCANCODE_BACKSLASH:    return ImGuiKey_Backslash;
    case SDL_SCANCODE_RIGHTBRACKET: return ImGuiKey_RightBracket;
    case SDL_SCANCODE_GRAVE:        return ImGuiKey_GraveAccent;
    case SDL_SCANCODE_CAPSLOCK:     return ImGuiKey_CapsLock;
    case SDL_SCANCODE_F1:           return ImGuiKey_F1;
    case SDL_SCANCODE_F2:           return ImGuiKey_F2;
    case SDL_SCANCODE_F3:           return ImGuiKey_F3;
    case SDL_SCANCODE_F4:           return ImGuiKey_F4;
    case SDL_SCANCODE_F5:           return ImGuiKey_F5;
    case SDL_SCANCODE_F6:           return ImGuiKey_F6;
    case SDL_SCANCODE_F7:           return ImGuiKey_F7;
    case SDL_SCANCODE_F8:           return ImGuiKey_F8;
    case SDL_SCANCODE_F9:           return ImGuiKey_F9;
    case SDL_SCANCODE_F10:          return ImGuiKey_F10;
    case SDL_SCANCODE_F11:          return ImGuiKey_F11;
    case SDL_SCANCODE_F12:          return ImGuiKey_F12;
    case SDL_SCANCODE_A:            return ImGuiKey_A;
    case SDL_SCANCODE_B:            return ImGuiKey_B;
    case SDL_SCANCODE_C:            return ImGuiKey_C;
    case SDL_SCANCODE_D:            return ImGuiKey_D;
    case SDL_SCANCODE_E:            return ImGuiKey_E;
    case SDL_SCANCODE_F:            return ImGuiKey_F;
    case SDL_SCANCODE_G:            return ImGuiKey_G;
    case SDL_SCANCODE_H:            return ImGuiKey_H;
    case SDL_SCANCODE_I:            return ImGuiKey_I;
    case SDL_SCANCODE_J:            return ImGuiKey_J;
    case SDL_SCANCODE_K:            return ImGuiKey_K;
    case SDL_SCANCODE_L:            return ImGuiKey_L;
    case SDL_SCANCODE_M:            return ImGuiKey_M;
    case SDL_SCANCODE_N:            return ImGuiKey_N;
    case SDL_SCANCODE_O:            return ImGuiKey_O;
    case SDL_SCANCODE_P:            return ImGuiKey_P;
    case SDL_SCANCODE_Q:            return ImGuiKey_Q;
    case SDL_SCANCODE_R:            return ImGuiKey_R;
    case SDL_SCANCODE_S:            return ImGuiKey_S;
    case SDL_SCANCODE_T:            return ImGuiKey_T;
    case SDL_SCANCODE_U:            return ImGuiKey_U;
    case SDL_SCANCODE_V:            return ImGuiKey_V;
    case SDL_SCANCODE_W:            return ImGuiKey_W;
    case SDL_SCANCODE_X:            return ImGuiKey_X;
    case SDL_SCANCODE_Y:            return ImGuiKey_Y;
    case SDL_SCANCODE_Z:            return ImGuiKey_Z;
    case SDL_SCANCODE_0:            return ImGuiKey_0;
    case SDL_SCANCODE_1:            return ImGuiKey_1;
    case SDL_SCANCODE_2:            return ImGuiKey_2;
    case SDL_SCANCODE_3:            return ImGuiKey_3;
    case SDL_SCANCODE_4:            return ImGuiKey_4;
    case SDL_SCANCODE_5:            return ImGuiKey_5;
    case SDL_SCANCODE_6:            return ImGuiKey_6;
    case SDL_SCANCODE_7:            return ImGuiKey_7;
    case SDL_SCANCODE_8:            return ImGuiKey_8;
    case SDL_SCANCODE_9:            return ImGuiKey_9;
    case SDL_SCANCODE_LCTRL:        return ImGuiKey_LeftCtrl;
    case SDL_SCANCODE_RCTRL:        return ImGuiKey_RightCtrl;
    case SDL_SCANCODE_LSHIFT:       return ImGuiKey_LeftShift;
    case SDL_SCANCODE_RSHIFT:       return ImGuiKey_RightShift;
    case SDL_SCANCODE_LALT:         return ImGuiKey_LeftAlt;
    case SDL_SCANCODE_RALT:         return ImGuiKey_RightAlt;
    case SDL_SCANCODE_LGUI:         return ImGuiKey_LeftSuper;
    case SDL_SCANCODE_RGUI:         return ImGuiKey_RightSuper;
    default:                        return ImGuiKey_None;
    }
}

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
                    m_chainNav.clear();
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

    imguiCreate(18.0f);
    SDL_StartTextInput(m_editorWin);

    m_running = true;
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void App::Shutdown()
{
    // Guard against double-calls (destructor after Run, or failed Init).
    if (!m_editorWin && !m_outputWin)
        return;

    m_running = false;

    imguiDestroy();

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
        SDL_StopTextInput(m_editorWin);
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
    // Keyboard events must reach ImGui IO before imguiBeginFrame() / NewFrame().
    ImGuiIO& io = ImGui::GetIO();

    const SDL_WindowID editorID = SDL_GetWindowID(m_editorWin);
    const SDL_WindowID outputID = SDL_GetWindowID(m_outputWin);

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
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
        case SDL_EVENT_KEY_UP:
        {
            // Only route keyboard to ImGui when the editor window has focus.
            if (event.key.windowID != editorID)
                break;

            bool Down = (event.type == SDL_EVENT_KEY_DOWN);
            SDL_Keymod Mod = event.key.mod;
            io.AddKeyEvent(ImGuiMod_Ctrl,  (Mod & SDL_KMOD_CTRL)  != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (Mod & SDL_KMOD_SHIFT) != 0);
            io.AddKeyEvent(ImGuiMod_Alt,   (Mod & SDL_KMOD_ALT)   != 0);
            io.AddKeyEvent(ImGuiMod_Super, (Mod & SDL_KMOD_GUI)   != 0);

            ImGuiKey Key = SdlScancodeToImGui(event.key.scancode);
            if (Key != ImGuiKey_None)
                io.AddKeyEvent(Key, Down);

            if (!Down && event.key.key == SDLK_ESCAPE && !io.WantCaptureKeyboard)
                m_running = false;
            break;
        }

        case SDL_EVENT_TEXT_INPUT:
            if (event.text.windowID == editorID)
                io.AddInputCharactersUTF8(event.text.text);
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            // Only scroll when the mouse is over the editor window.
            if (event.wheel.windowID == editorID)
                m_scroll += (int32_t)event.wheel.y;
            break;

        default:
            break;
        }
    }

    // Read mouse state relative to the editor window.
    float Mx = 0.0f, My = 0.0f;
    SDL_MouseButtonFlags Buttons = SDL_GetMouseState(&Mx, &My);
    m_mouseX = (int32_t)Mx;
    m_mouseY = (int32_t)My;
    m_mouseButtons = 0;
    if (Buttons & SDL_BUTTON_LMASK) m_mouseButtons |= IMGUI_MBUT_LEFT;
    if (Buttons & SDL_BUTTON_RMASK) m_mouseButtons |= IMGUI_MBUT_RIGHT;
    if (Buttons & SDL_BUTTON_MMASK) m_mouseButtons |= IMGUI_MBUT_MIDDLE;

    // Start the ImGui frame (calls ImGui::NewFrame internally).
    // Pass editor window dimensions so ImGui layout stays within that window.
    imguiBeginFrame(m_mouseX, m_mouseY, m_mouseButtons, m_scroll,
                    (uint16_t)m_editorWidth, (uint16_t)m_editorHeight, -1, 255);
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
            if (ImGui::MenuItem("Audio..."))
            {
                m_showOptions = true;
                RefreshAudioDevices();
            }
            ImGui::EndMenu();
        }

        // Current path hint on the right side of the menu bar.
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(m_presetPath.empty() ? "(unsaved)" : m_presetPath.c_str()).x - 8.0f);
        ImGui::TextDisabled("%s", m_presetPath.empty() ? "(unsaved)" : m_presetPath.c_str());

        ImGui::EndMainMenuBar();
    }

    if (m_showOptions)
        RenderOptionsWindow();

    // Resolve the chain and selection being displayed.
    // When nav stack is non-empty the front-most entry owns the current chain.
    EffectChain* currentChain = &m_engine.GetChain();
    int32_t& currentSel = m_selectedEffect;
    if (!m_chainNav.empty())
    {
        currentChain = m_chainNav.back().Chain;
        currentSel   = m_chainNav.back().SelectedEffect;
    }

    ChainPanel::Render(m_engine, m_chainNav, m_selectedEffect, currentChain);

    // Re-read selection after ChainPanel may have changed it.
    int32_t displaySel = m_chainNav.empty() ? m_selectedEffect
                                            : m_chainNav.back().SelectedEffect;

    Effect* Selected = nullptr;
    if (displaySel >= 0 && displaySel < currentChain->Count())
        Selected = currentChain->GetEntry(displaySel).Effect.get();

    ConfigPanel::Render(m_engine, Selected);

    imguiEndFrame();
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
    ImGui::SetNextWindowSize(ImVec2(480, 155), ImGuiCond_FirstUseEver);
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

    ImGui::End();
}
