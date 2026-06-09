# AVS Native — Overall Architecture

## Design Principles

1. **Engine / UI separation.** The rendering engine, effect chain, audio pipeline, and scripting runtime are collected into a library (`avs_engine`) that has zero dependency on any UI framework. It can run headless — loading a preset and rendering frames without an editor window.

2. **ImGui for the editor.** The editor UI (`avs_ui`) sits entirely on top of the engine API. It reads engine state and writes back through the public interface. Stripping the UI from a build does not touch the engine.

3. **Single render context.** bgfx owns the GPU context. Both the engine effect passes and ImGui draw calls submit into the same bgfx frame; bgfx view IDs control draw order.

---

## Directory Structure

```
AVSNative/
  CMakeLists.txt
  ARCHITECTURE.md         this file
  PLAN_SHADER_PIPELINE.md detailed shader / audio plan
  PLAN_LUA_SCRIPTING.md   detailed LuaJIT scripting plan

  lib/                    third-party sources (git submodules or FetchContent)
    bgfx/
    glslang/
    spirv-cross/
    SDL3/
    LuaJIT/
    imgui/
    nlohmann_json/
    kissfft/
    dr_mp3.h

  src/
    engine/               avs_engine — no UI headers anywhere in here
      Engine.h / .cpp
      EffectChain.h / .cpp
      Effect.h            base class + ParamDesc / EffectDesc
      FBOManager.h / .cpp
      AudioAnalyzer.h / .cpp
      AudioGLBuffer.h / .cpp
      LuaRuntime.h / .cpp
      Preset.h / .cpp
      Registry.h / .cpp

    effects/              compiled into avs_engine; no UI headers
      Fadeout.cpp, Blur.cpp, Movement.cpp, DynamicMovement.cpp,
      SuperScope.cpp, ColorModifier.cpp, DDM.cpp, ...
      register.cpp        calls Registry::reg() for every effect

    ui/                   avs_ui — depends on avs_engine + imgui, not vice versa
      App.h / .cpp        SDL3 event loop, bgfx frame, ImGui context
      ChainPanel.h / .cpp effect list, enable/disable, drag-reorder
      ConfigPanel.h / .cpp auto-generated param widgets from EffectDesc

    main.cpp              links avs_engine + avs_ui; parses CLI flags
```

---

## Module Overview

| Module | Lib | Depends on | UI allowed? |
|---|---|---|---|
| `Engine` | avs_engine | bgfx, SDL3, LuaJIT | No |
| `EffectChain` | avs_engine | Engine | No |
| `Effect` (base + all effects) | avs_engine | bgfx, glslang, LuaJIT | No |
| `FBOManager` | avs_engine | bgfx | No |
| `AudioAnalyzer` | avs_engine | SDL3, kissfft, dr_mp3 | No |
| `LuaRuntime` | avs_engine | LuaJIT | No |
| `Preset` | avs_engine | nlohmann/json | No |
| `Registry` | avs_engine | — | No |
| `App` | avs_ui | avs_engine, SDL3, imgui | Yes |
| `ChainPanel` | avs_ui | avs_engine, imgui | Yes |
| `ConfigPanel` | avs_ui | avs_engine, imgui | Yes |

---

## Engine Module

### `Engine`

The root object. Owns all subsystems. Constructed by `main.cpp`; the UI holds a reference to it but does not own it.

```cpp
struct EngineConfig {
    int  width, height;
    bool headless;      // no window; renders to offscreen FBO
    bool enableAudio;
};

class Engine {
public:
    bool init(const EngineConfig& cfg);
    void shutdown();

    // Called once per frame by the outer loop (App or headless main).
    // Runs: audio update → Lua frame blocks → effect chain → blit.
    // Does NOT call bgfx::frame() — the caller does that after UI submission.
    void tick();

    void resize(int w, int h);

    EffectChain&   chain();
    AudioAnalyzer& audio();

    bool loadPreset(const std::string& path);
    bool savePreset(const std::string& path) const;

    // Final composited frame — readable as a texture by the UI for
    // display, or via bgfx::readTexture() for headless recording.
    bgfx::TextureHandle outputTexture() const;
};
```

`tick()` sequence each frame:
1. `audio.update()` — pull PCM from SDL3 stream, run FFT, beat detection
2. `audioBuffer.update()` — upload 576×1 RGBA8 audio texture to bgfx
3. For each enabled effect in chain: clear next FBO → `effect.render(ctx)` → `fboManager.swap()`
4. Blit final ping-pong FBO to `m_outputTex` (always — window or headless)

`bgfx::frame()` is called by the outer context **after** `tick()` (and after any UI submission). This keeps the engine agnostic about whether a UI is present.

---

### `EffectChain` + `EffectEntry`

Identical structure to AVS_Remake's `effect-chain.js`: an ordered list of `EffectEntry { Effect*, bool enabled }`. `render(ctx)` iterates the list; disabled entries are skipped. `EffectList` (container effect) runs a nested chain in private FBOs.

The `RenderCtx` struct passed to each effect:

```cpp
struct RenderCtx {
    bgfx::TextureHandle inputTex;    // current ping-pong frame
    bgfx::FrameBufferHandle outputFBO; // where this effect writes
    FBOManager*    fboManager;
    AudioAnalyzer* audio;
    bgfx::TextureHandle audioTex;   // 576×1 RGBA8 audio texture
    bool   isBeat;
    int    width, height;
    int    frame;
    double time;
};
```

---

### `Effect` — base class and descriptor system

All effects extend `Effect` and implement five methods:

```cpp
class Effect {
public:
    virtual void        init(bgfx::RendererType::Enum renderer) = 0;
    virtual void        render(const RenderCtx& ctx) = 0;
    virtual EffectDesc  getDescriptor() const = 0;
    virtual nlohmann::json getConfig()  const = 0;
    virtual void        setConfig(const nlohmann::json& cfg) = 0;
    virtual void        destroy() = 0;
    virtual ~Effect() = default;
};
```

`getDescriptor()` returns the param schema — used by `ConfigPanel` to generate ImGui widgets. The engine never reads descriptors; that is purely UI/tooling.

```cpp
enum class ParamType { Range, Color, Select, Bool, Number, Glsl, Lua, Colors };

struct ParamDesc {
    std::string name;
    std::string label;
    ParamType   type;
    float       min, max, step;                  // Range, Number
    std::vector<std::string> options;            // Select
    // Glsl/Lua: live recompile on change; errors queryable via getError(name)
};

struct EffectDesc {
    std::string            name;
    std::vector<ParamDesc> params;
};
```

For effects with live-editable code, `getError(const std::string& paramName)` returns the last compile or runtime error string (empty if clean). `ConfigPanel` calls this each frame to display error text below the editor.

---

### `FBOManager`

Manages 10 bgfx framebuffers: 2 ping-pong + 8 scratch slots. Identical responsibility to AVS_Remake's `framebuffer-manager.js` — see `PLAN_SHADER_PIPELINE.md` for the render convention (every effect reads `inputTex`, writes to `outputFBO`, calls `fboManager.swap()`).

On resize, all FBOs are destroyed and recreated. The bgfx view ID pool (views 0–253) is allocated by `FBOManager` to keep view assignments stable and avoid conflicts with the two reserved views at the top.

---

### `AudioAnalyzer`

Wraps SDL3 audio capture and `dr_mp3` file decoding into the same 576-bin spectrum / waveform format as AVS_Remake. Full details in `PLAN_SHADER_PIPELINE.md` (audio pipeline section). Exposes:

```cpp
struct VisData {
    float spec[2][576];  // [L/R][bin], 0–255
    float osc[2][576];   // [L/R][bin], 0–255, 128=silence
};

class AudioAnalyzer {
public:
    void connectDevice(SDL_AudioDeviceID id);
    void connectFile(const std::string& path);   // dr_mp3 decode
    void update();                               // called at top of tick()
    const VisData& visdata() const;
    bool isBeat() const;
    float bpm() const;
};
```

---

### `LuaRuntime`

Not a singleton — every scriptable effect (`SuperScope`, `DynamicMovement`, `DDM`, `ColorModifier`, `DynamicShift`) owns a `LuaRuntime` instance, which wraps one `lua_State`. Full implementation details in `PLAN_LUA_SCRIPTING.md`. Key interface:

```cpp
class LuaRuntime {
public:
    void init(const AudioAnalyzer* audio);
    void setBuiltin(const char* name, double value);   // b, w, h, n, etc.
    void runInit(const std::string& code);
    void runFrame(const std::string& code);
    void runBeat(const std::string& code);
    void runPoint(const std::string& code,             // SuperScope only
                  const float* audioSamples,
                  float* outBuffer, int n);
    double getVar(const char* name) const;
    std::string getError(const char* blockName) const;
};
```

---

### `Preset`

JSON serialisation via `nlohmann/json`. Format extends AVS_Remake's:

```json
{
  "version": "2.0",
  "effects": [
    { "type": "Fadeout", "enabled": true, "config": { "speed": 0.04 } }
  ]
}
```

`Preset::load()` destroys the current chain, then reconstructs each effect via `Registry::create(type)` followed by `setConfig(cfg)`. `Preset::save()` iterates the chain calling `getConfig()`.

---

### `Registry`

```cpp
class Registry {
public:
    void reg(const std::string& name, std::function<Effect*()> factory);
    std::unique_ptr<Effect> create(const std::string& name) const;
    std::vector<std::string> names() const;   // for the "Add Effect" dropdown
};
```

`src/effects/register.cpp` calls `reg()` for every effect class. `Registry` is a dependency of `Preset` and `App` only; individual effect `.cpp` files do not include it.

---

## Effects Module

Each effect file lives in `src/effects/` and includes only `engine/Effect.h`, `engine/FBOManager.h`, and (for scriptable effects) `engine/LuaRuntime.h`. No imgui headers. Effects fall into the same four categories as AVS_Remake:

| Category | Mechanism | Examples |
|---|---|---|
| Full-screen shader | bgfx SPIR-V program + FBO quad | Fadeout, Blur, Invert, Mirror |
| Scriptable GLSL | glslang runtime compile → SPIR-V + LuaJIT Init/Frame/Beat | DynamicMovement, DDM, ColorModifier |
| CPU overlay | Lua Point loop → CPU pixel buffer → bgfx texture upload | SuperScope |
| Container | Nested EffectChain in private FBOs | EffectList |

Static effects (no user GLSL) ship pre-compiled `.sc` → SPIR-V blobs generated by the offline `shaderc` build step and embedded in the binary as `const uint8_t[]` arrays via `xxd -i` or `cmrc`.

---

## UI Module

### `App`

Owns one or two SDL3 windows and the ImGui context. Contains the main event loop; is the only place `bgfx::frame()` is called (in GUI mode). bgfx is always initialised with the **editor window** as its primary; the optional output window gets a native-handle framebuffer.

```cpp
class App {
public:
    App(Engine& engine);
    // outputDisplay: index into SDL_GetDisplays() for the fullscreen output window
    void run(int outputDisplay);
private:
    void processEvents();
    void renderUI();
    void blitOutput();   // view 254 → m_outputFB

    SDL_Window*              m_editorWin  = nullptr;
    SDL_Window*              m_outputWin  = nullptr;
    bgfx::FrameBufferHandle  m_outputFB   = BGFX_INVALID_HANDLE;
};
```

Frame sequence inside `run()`:
```
while (!quit) {
    processEvents();          // SDL3 events → ImGui IO; route by SDL_WindowID
    engine.tick();            // effect chain, audio, LuaJIT — bgfx cmds queued
    blitOutput();             // view 254 → m_outputFB (output window or backbuffer)
    renderUI();               // ImGui panels — bgfx cmds queued (view 255, editor window)
    bgfx::frame();            // GPU submit
}
```

`engine.tick()` never calls `bgfx::frame()` — so both blit and UI can always append to the same frame.

### ImGui + bgfx integration

- **Input backend:** `imgui_impl_sdl3` (SDL3 events → ImGui IO)
- **Renderer backend:** bgfx ImGui renderer submits `ImDrawList` as bgfx draw calls
- **View IDs:** engine effect passes use views 0–253; blit-to-screen uses view 254; ImGui uses view 255. Higher view ID renders last (on top).
- **Multi-window:** view 254 targets `m_outputFB`, a native-handle framebuffer backed by the fullscreen output window. View 255 always targets the editor window. bgfx's Vulkan backend creates a separate `VkSwapchainKHR` per native-handle framebuffer — no special configuration required.

### `ChainPanel`

Displays the effect list as a scrollable `ImGui::BeginListBox`. Each entry: enable checkbox, effect name, remove button. Drag-to-reorder via `ImGui::SetDragDropPayload` / `AcceptDragDropPayload` — the payload carries an index into `EffectChain`, not a serialized copy of the effect. Clicking an entry sets the selected effect for `ConfigPanel`.

`EffectList` entries render an expand button; when open, the sub-chain indents below using `ImGui::Indent`.

### `ConfigPanel`

Auto-generates widgets from `effect->getDescriptor().params`. Called each frame for the selected effect:

| ParamType | ImGui widget |
|---|---|
| `Range` | `ImGui::SliderFloat` / `ImGui::SliderInt` |
| `Color` | `ImGui::ColorEdit3` |
| `Select` | `ImGui::Combo` |
| `Bool` | `ImGui::Checkbox` |
| `Number` | `ImGui::InputFloat` |
| `Glsl` / `Lua` | `ImGui::InputTextMultiline` (300ms debounce) + error text in red below |
| `Colors` | custom widget: color list with `ImGui::ColorEdit3` per entry, add/remove buttons |

On any widget change, `ConfigPanel` calls `effect->setConfig(updatedJson)` with only the changed key.

---

## Operating Modes

### GUI mode

`main.cpp` instantiates `Engine` + `App`, calls `app.run(outputDisplay)`. Two SDL3 windows are always created: the editor on the primary display, and a fullscreen output window on display `N`. `app.run(N)` creates the output window on display `N` and fullscreens it:

```cpp
int numDisplays;
SDL_DisplayID* displays = SDL_GetDisplays(&numDisplays);

m_outputWin = SDL_CreateWindow("AVS Output", w, h, SDL_WINDOW_VULKAN);
SDL_SetWindowPosition(m_outputWin,
    SDL_WINDOWPOS_CENTERED_DISPLAY(displays[N]),
    SDL_WINDOWPOS_CENTERED_DISPLAY(displays[N]));
SDL_SetWindowFullscreen(m_outputWin, true);

void* nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(m_outputWin),
                SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr); // platform-specific
m_outputFB = bgfx::createFrameBuffer(nwh, w, h);
```

View 254 targets `m_outputFB`; the editor window (view 255, ImGui) remains on the primary display. Close events on the output window hide it without quitting; the editor's close button exits the app. The engine, all effects, and `FBOManager` are completely unchanged — only `App` knows about the second window.

### Headless mode (`--headless` flag)

`main.cpp` instantiates `Engine` only with `EngineConfig::headless = true`. SDL3 initialises without the video subsystem (audio still works). bgfx runs in offscreen mode — no swap chain. The outer loop calls `engine.tick()` + `bgfx::frame()` directly:

```cpp
while (running) {
    engine.tick();
    if (record) captureFrame(engine.outputTexture());  // bgfx::readTexture
    bgfx::frame();
}
```

No ImGui headers are included in `main.cpp` for headless builds; `avs_ui` is not linked.

---

## Build System

CMake 3.25+. Each third-party dependency is added as a submodule under `lib/` and integrated as follows:

| Dependency | Integration |
|---|---|
| bgfx + bx + bimg | `bgfx.cmake` wrapper (`add_subdirectory`) |
| glslang | `add_subdirectory(lib/glslang)`, targets `glslang::glslang` |
| spirv-cross | `add_subdirectory(lib/spirv-cross)`, targets `spirv-cross-core` |
| SDL3 | `add_subdirectory(lib/SDL)`, target `SDL3::SDL3` |
| LuaJIT | `ExternalProject_Add` (has its own Makefile); import as imported lib |
| imgui | source files added directly to `avs_ui` target |
| nlohmann/json | `FetchContent_Declare` or submodule; `nlohmann_json::nlohmann_json` |
| kissfft | `add_subdirectory(lib/kissfft)` |
| dr_mp3 | single header in `lib/`; no CMake needed |
| MoltenVK (macOS) | pre-built dylib; CMake `install` copies into `.app/Contents/Frameworks/` |

Two CMake targets:
- `avs_engine` — static lib: `src/engine/` + `src/effects/`
- `avs_ui` — static lib: `src/ui/` + imgui sources; links `avs_engine`
- `avs_editor` — executable: `src/main.cpp`; links `avs_engine` + `avs_ui`
- `avs_headless` — executable: `src/main.cpp -DAVS_HEADLESS`; links `avs_engine` only

---

## Dependency Summary

| Library | Purpose | Notes |
|---|---|---|
| bgfx + bx + bimg | Rendering core, FBO management, draw calls | Vulkan backend |
| glslang | GLSL → SPIR-V at runtime (scriptable effect stubs) | Khronos |
| spirv-cross | SPIR-V reflection for uniform metadata | Khronos |
| MoltenVK | Vulkan → Metal on macOS; bundled in .app | No user install |
| SDL3 | Window, input events, audio device capture | Replaces Web Audio API |
| dr_mp3 | MP3 file decoding (single header) | Replaces `<audio>` element |
| kissfft | 2048-point FFT for spectrum analysis | Replaces AnalyserNode |
| LuaJIT | CPU scripting — Init/Frame/Beat/Point blocks | See PLAN_LUA_SCRIPTING.md |
| Dear ImGui | Editor UI | imgui_impl_sdl3 for input |
| nlohmann/json | Preset serialisation | Single header |

---

## Verification Plan

Tests are ordered to validate each layer before the next depends on it.

1. **Engine headless smoke test.** `avs_headless --preset default.json` renders 60 frames without crashing. Capture frame 60 as PNG; compare pixel checksum against AVS_Remake rendering the same preset at the same resolution.

2. **Static effect.** Port `Fadeout`. Confirm FBO ping-pong, speed param, pixel output matches JS reference.

3. **Shader pipeline.** Port `Movement` with a live GLSL stub. Edit stub in `ConfigPanel` textarea; confirm recompile, error display, correct line numbers. See `PLAN_SHADER_PIPELINE.md` §Verification for full shader tests.

4. **LuaJIT scripting.** Port `ColorModifier` (Init/Frame/Beat + GLSL stub) and `SuperScope` (Point hot loop). See `PLAN_LUA_SCRIPTING.md` §Verification for full scripting tests.

5. **Audio.** Open the recording device selector; switch devices; confirm spectrum + waveform update. Load an MP3; confirm beat detection triggers match JS version.

6. **GUI isolation.** Launch `avs_headless`; confirm it compiles and runs with `avs_ui` not linked and zero ImGui headers reachable from `src/engine/` or `src/effects/`.

7. **Full preset round-trip.** Load `AVS_Remake/presets/default.json`; confirm all effects in the chain instantiate, run, and produce non-black output. Save as `native_default.json`; reload; confirm chain is identical.

---

## Potential Next Steps

- **MIDI device support** — USB MIDI input for mapping CC messages and note on/off to effect parameters. Library candidate: `libremidi` (C++17 header-only, wraps CoreMIDI / WinMM / ALSA). Mapping model and MIDI Learn UX are already sketched in `PLAN_SHADER_PIPELINE.md` §MIDI.
