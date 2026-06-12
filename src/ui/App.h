#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;
struct ImGuiStyle;
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
    bool     m_showSliders        = true;   // Sliders panel visibility (View menu toggle)

    // UI (DPI) content scale. 0 = Auto (native HiDPI already handles density, so
    // Auto = 1.0); otherwise a manual percentage (100/125/150/200). Applied to the
    // ImGui style (font + spacing) at the top of RenderUI when dirty.
    int          m_uiScalePct   = 0;
    bool         m_uiScaleDirty = true;
    ImGuiStyle*  m_baseStyle    = nullptr;  // pristine style snapshot (heap, to keep App.h light)

    // Output render resolution as a percentage of the output window's pixel size.
    // The window stays the same size; only the internal render resolution changes
    // (lower = cheaper/softer, higher = supersampled). Blit scales it to fill.
    int      m_outputRenderPct   = 100;
    int32_t  m_outputPixelW      = 0;  // output window size in PIXELS (HiDPI aware)
    int32_t  m_outputPixelH      = 0;
    bool     m_outputSizingDirty = false; // apply ApplyOutputSizing() before next Tick

    uint32_t m_lastKey = 0;   // most recent key seen by KeyInput (shown in the status bar)

    std::string m_presetPath;
    bool        m_pendingLoad         = false;
    bool        m_pendingSave         = false;
    bool        m_pendingNewAfterSave = false; // clear preset after Save As completes
    bool        m_showNewConfirm     = false; // open the "New?" modal next frame
    bool        m_limitFramerate    = true;  // cap render loop to 60 fps
    bool        m_pendingAudioFile    = false;

    // ── Options window ────────────────────────────────────────────────────────
    bool                     m_showOptions      = false;
    std::vector<uint32_t>    m_audioCaptureIds;    // SDL_AudioDeviceID per entry
    std::vector<std::string> m_audioCaptureNames;
    int                      m_audioCaptureIdx  = -1; // -1 = none selected
    int                      m_pendingOutputW   = 1280;
    int                      m_pendingOutputH   = 720;

    void ClearPreset();
    void RefreshAudioDevices();
    void RenderOptionsWindow();
    void RenderStatusBar();
    void BuildDefaultDockLayout(unsigned int dockId);

    // DPI / sizing helpers.
    void ApplyEditorSizing();   // editor window pixel size -> bgfx reset + view 255
    void ApplyOutputSizing();   // output window pixel size + render% -> FBO + engine
    void ApplyUiScale();        // (re)apply m_uiScalePct to the ImGui style
    float AutoUiScale() const;  // content scale for "Auto" (1.0; density handled natively)
};
