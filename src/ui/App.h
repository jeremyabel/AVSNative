#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;
class Engine;
class EffectChain;

// One level in the chain-panel navigation stack.
struct ChainNavEntry
{
    EffectChain* Chain          = nullptr;
    std::string  Label;           // breadcrumb display name
    int32_t      SelectedEffect = -1;
};

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

    int32_t  m_mouseX       = 0;
    int32_t  m_mouseY       = 0;
    uint8_t  m_mouseButtons = 0;
    int32_t  m_scroll       = 0;

    int32_t  m_selectedEffect = -1;

    // Navigation stack for the ChainPanel.  Empty = viewing the root chain.
    // Each entry adds one level of nesting (i.e. an EffectList's inner chain).
    std::vector<ChainNavEntry> m_chainNav;

    std::string m_presetPath;
    bool        m_pendingLoad      = false;
    bool        m_pendingSave      = false;
    bool        m_pendingAudioFile = false;

    // ── Options window ────────────────────────────────────────────────────────
    bool                     m_showOptions      = false;
    std::vector<uint32_t>    m_audioCaptureIds;    // SDL_AudioDeviceID per entry
    std::vector<std::string> m_audioCaptureNames;
    int                      m_audioCaptureIdx  = -1; // -1 = none selected

    void RefreshAudioDevices();
    void RenderOptionsWindow();
};
