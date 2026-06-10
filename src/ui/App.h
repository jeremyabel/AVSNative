#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;
class Engine;
class EffectChain;

class App
{
public:
    explicit App(Engine& InEngine);
    ~App();

    void Run(const char* PresetPath = nullptr);

private:
    void Init(const char* PresetPath);
    void Shutdown();
    void ProcessEvents();
    void RenderUI();
    void ResizeOutput(int32_t Width, int32_t Height);

    Engine&      m_engine;

    SDL_Window*  m_editorWin  = nullptr;
    SDL_Window*  m_outputWin  = nullptr;
    bgfx::FrameBufferHandle m_outputFB = BGFX_INVALID_HANDLE;

    bool     m_running      = false;
    int32_t  m_editorWidth  = 620;
    int32_t  m_editorHeight = 720;
    int32_t  m_outputWidth  = 1280;
    int32_t  m_outputHeight = 720;

    // Selected effect: both the chain it lives in and its index within that chain.
    // selectedChain is nullptr when nothing is selected.
    EffectChain* m_selectedChain  = nullptr;
    int32_t      m_selectedEffect = -1;

    bool     m_buildDefaultLayout = false;  // build the dock layout on first frame (no imgui.ini)

    std::string m_presetPath;
    bool        m_pendingLoad      = false;
    bool        m_pendingSave      = false;
    bool        m_pendingAudioFile = false;

    // ── Options window ────────────────────────────────────────────────────────
    bool                     m_showOptions      = false;
    std::vector<uint32_t>    m_audioCaptureIds;    // SDL_AudioDeviceID per entry
    std::vector<std::string> m_audioCaptureNames;
    int                      m_audioCaptureIdx  = -1; // -1 = none selected
    int                      m_pendingOutputW   = 1280;
    int                      m_pendingOutputH   = 720;

    void RefreshAudioDevices();
    void RenderOptionsWindow();
    void RenderStatusBar();
    void BuildDefaultDockLayout(unsigned int dockId);
};
