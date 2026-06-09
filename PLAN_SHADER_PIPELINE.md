# Plan: Native C++ Port — Shader Pipeline Architecture

## Context

AVS_Remake runs in the browser (WebGL2 + GLSL ES 3.00) and relies heavily on **live, user-editable GLSL** for ~5 effects: Movement, Dynamic Movement, DDM, Color Modifier, and (the vertex path of) Texer2. These effects let users type a GLSL pixel/vertex stub into a textarea that is assembled into a full shader and recompiled at runtime via `gl.compileShader` with inline error display. This is the core AVS "scripting" UX.

The goal is to port to a native C++ app using **bgfx** for rendering (long-term maintenance, Apple Metal support via Vulkan+MoltenVK), with the shader pipeline being the highest-risk architectural decision. This plan resolves that risk by defining exactly how live GLSL recompilation will work across all platforms.

---

## Current Pipeline (JS, for reference)

All scriptable effects share the same pattern:

1. **Template assembly** — a JS template string builds the full GLSL source, injecting the user's stub at a `${stub}` site surrounded by engine-provided uniforms, GLSL version header, `AUDIO_GLSL_SRC`, and user-bridged `uniform float` declarations.
2. **Runtime compile** — `gl.compileShader()` / `gl.linkProgram()` compiles in-driver.
3. **Error extraction** — `gl.getShaderInfoLog()` / `gl.getProgramInfoLog()` on failure; error stored in `_compileError` and displayed below the textarea in the config panel.
4. **User variable bridging** — `scanVarDecls(initCode, BUILTINS)` (regex `/\bvar\s+([a-zA-Z_]\w*)/g`) discovers user `var` declarations; each becomes a `uniform float` in the shader and is set per-frame via `gl.uniform1f()`.

All live-GLSL shaders use `#version 300 es` with `precision highp float`. They use `texelFetch`, `in`/`out`, and `texture()` — all standard GLSL ES 3.00 / GLSL 3.30+.

No OES or platform-specific extensions are used.

### Effects that need runtime GLSL recompilation

| Effect | Stub type | User vars → uniforms | Audio GLSL |
|---|---|---|---|
| Movement | Fragment (pull) or Vertex (scatter) | No | No |
| Dynamic Movement | Vertex + Fragment (grid) or Fragment (direct) | Yes | Yes |
| DDM | Fragment | Yes | Yes |
| Color Modifier | Fragment (stub runs 3× per pixel, one per channel) | Yes | Yes |
| (Texer2) | Vertex (point placement) | TBD | TBD |

SuperScope: user writes **JS** (not GLSL), drawn on CPU — no runtime GLSL needed. Convolution: purely data-driven (kernel array as uniform) — shader compiled once at startup, not by user.

---

## The Problem

bgfx does **not** accept raw GLSL at runtime. It uses its own `.sc` shader dialect, compiled offline by the `shaderc` tool into backend-specific bytecode. `bgfx::createShader()` expects a binary blob in bgfx's format, not a GLSL string.

The only community runtime wrapper (`brtshaderc`) was last updated ~8 years ago and is incompatible with current bgfx — eliminated.

---

## Recommended Approach: bgfx (Vulkan) + glslang + MoltenVK

Target Vulkan everywhere. On macOS, bundle **MoltenVK** (`libMoltenVK.dylib` in `App.app/Contents/Frameworks/`) — zero user setup required, transparent Vulkan-to-Metal translation. On Windows, native Vulkan. On Linux, native Vulkan.

For the ~5 scriptable effects: compile user stubs at runtime with **glslang** (Khronos-maintained, used in Vulkan SDK) from GLSL → SPIR-V, then feed the SPIR-V to `bgfx::createShader()`. SPIR-V is bgfx's native shader format for the Vulkan renderer — no format conversion needed beyond a thin header describing uniform metadata (extracted via **spirv-cross** reflection).

For the ~35 static effects: compile `.sc` shaders offline via bgfx's `shaderc` tool targeting SPIR-V, embed the blobs in the binary.

### Why glslang + spirv-cross over alternatives

- Both are **Khronos-maintained** (same org as Vulkan/GLSL specs) — strong long-term guarantee
- glslang accepts standard GLSL directly — user stubs are unchanged from what they write today
- SPIR-V reflection via spirv-cross extracts uniform names/types for `bgfx::UniformHandle` creation without requiring `.sc` dialect translation
- Error messages use standard glslang format (`line:col: error: ...`) — familiar to GLSL authors

### MoltenVK user burden

None for end users — dylib ships in the app bundle. macOS 11.0+ required (reasonable baseline). Developer burden: add a "Copy Files" build phase to embed the dylib; sign it with the app's identity; link `Metal.framework`, `IOSurface.framework`, `QuartzCore.framework` (all Apple system frameworks, no install required).

---

## Build / Dependencies

```
bgfx + bx + bimg        rendering core
glslang                 GLSL → SPIR-V (runtime, for scriptable effects)
spirv-cross             SPIR-V reflection → uniform metadata
MoltenVK                bundled; Vulkan → Metal on macOS (no user install)
SDL3                    window, input events, audio device enumeration + capture
dr_mp3                  single-header MP3 decoder for file input mode
kissfft or pffft        FFT for spectrum analysis (replaces AnalyserNode)
```

SDL3 covers both windowing and audio capture, replacing the need for a separate audio I/O library. `dr_mp3` covers file decoding. `miniaudio` is not needed.

---

## Implementation Outline

### Window + bgfx initialization (SDL3 + Vulkan)

```cpp
SDL_Window* win = SDL_CreateWindow("AVS", w, h, SDL_WINDOW_VULKAN);

bgfx::PlatformData pd{};
#if defined(__APPLE__)
    pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(win),
                                    SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif defined(_WIN32)
    pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(win),
                                    SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else // Linux
    pd.ndt = SDL_GetPointerProperty(SDL_GetWindowProperties(win),
                                    SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    pd.nwh = (void*)(uintptr_t)SDL_GetNumberProperty(SDL_GetWindowProperties(win),
                                    SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
#endif

bgfx::setPlatformData(pd);
bgfx::Init init;
init.type = bgfx::RendererType::Vulkan;
bgfx::init(init);
```

### Static effects (non-scriptable, ~35 effects)

Shader sources live in `shaders/` as `.sc` files. A build step runs bgfx's offline `shaderc` targeting `spirv` to produce `.bin` blobs, embedded into the binary or loaded from disk at startup. At `Effect::init()`, load the `.bin` and call `bgfx::createProgram()`.

One-time GLSL ES → bgfx `.sc` conversion is mechanical (see `.sc` format notes): header lines change, math is untouched.

### Scriptable effects (runtime-compiled, ~5 effects)

Each effect maintains a C++ template string (GLSL with a stub injection site). On stub change, compile on a background thread:

```cpp
// 1. Assemble full GLSL source
std::string src = buildTemplatePrefix(userVars)
                + "// --- user stub ---\n"
                + userStub
                + "\n// --- end stub ---\n"
                + buildTemplateSuffix();

// 2. GLSL → SPIR-V via glslang
glslang::TShader shader(EShLangFragment);
const char* c = src.c_str();
shader.setStrings(&c, 1);
shader.setEnvInput(glslang::EShSourceGlsl, EShLangFragment,
                   glslang::EShClientVulkan, 100);
shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
shader.setEnvTarget(glslang::EShTargetSpv,   glslang::EShTargetSpv_1_3);
if (!shader.parse(&DefaultTBuiltInResource, 450, false, EShMsgDefault)) {
    m_compileError = offsetErrorLines(shader.getInfoLog(), templatePrefixLines);
    return;
}
glslang::TProgram prog;
prog.addShader(&shader);
prog.link(EShMsgDefault);
std::vector<uint32_t> spirv;
glslang::GlslangToSpv(*prog.getIntermediate(EShLangFragment), spirv);

// 3. Reflect uniforms via spirv-cross (to build bgfx UniformHandles)
spirv_cross::Compiler refl(spirv);
// ... extract active uniforms, create/reuse bgfx::UniformHandle per name

// 4. Wrap SPIR-V in bgfx's expected binary format (thin header) and create shader
const bgfx::Memory* mem = wrapSpirv(spirv);  // ~30-line helper
bgfx::destroy(m_prog);
m_prog = bgfx::createProgram(m_vs, bgfx::createShader(mem), true);
m_compileError = "";
```

`scanVarDecls(initCode, BUILTINS)` (same regex as JS, in C++) discovers user `var` names; each becomes a `bgfx::UniformHandle` set via `bgfx::setUniform()` per frame.

`AUDIO_GLSL_SRC` is injected verbatim into the template — it is standard GLSL (no `.sc` macros), compatible with glslang as-is after removing the ES precision qualifier.

### Audio pipeline

SDL3 handles device enumeration and capture. `dr_mp3` handles file input.

| Web Audio API | Native C++ equivalent |
|---|---|
| Mic device selection | `SDL_GetAudioRecordingDevices()` → enumerate; `SDL_OpenAudioDevice(id, spec)` |
| `AnalyserNode` capture (PCM) | SDL3 `SDL_AudioStream` bound to recording device |
| `ChannelSplitter` (L/R) | Deinterleave stereo PCM from SDL3 stream buffer |
| FFT (fftSize=2048) | kissfft / pffft, 2048-point complex FFT |
| `getFloatFrequencyData()` (dB) | Magnitude → dB → log-scale to 576 bins |
| `getFloatTimeDomainData()` | Raw PCM → 576-sample downsample, map [-1,1] → [0,255] |
| `connectAudioElement(el)` | `dr_mp3` decode → same PCM analysis chain |
| Beat detection | Port rolling-RMS from `AVS_Original/main.cpp` (already ported to JS) |

The 576×1 RGBA8 audio texture upload: `bgfx::updateTexture2D()` replaces `gl.texSubImage2D()`. Layout is identical: `[specL, specR, oscL, oscR]` per texel.

---

## GLSL Stub Compatibility

User stubs written today (for the JS version) work unchanged. The template prefix handles all engine-side scaffolding. Key notes:

- `#version` / `precision highp float` live in the template, not the stub — unchanged
- User vars still declared as local `float` inside the stub — glslang sees them as already-declared locals initialized from uniforms in the prefix
- Error messages from glslang use `filename:line:col: error:` format — subtract template prefix line count to give user-relative line numbers for display

---

## Verification Plan

1. **Build smoke test:** Compile SDL3 + bgfx Vulkan triangle on Windows + macOS (with bundled MoltenVK). Confirm Vulkan renderer active on both; confirm MoltenVK translating to Metal on macOS via GPU profiler.
2. **Static effect:** Port `Fadeout` end-to-end (`.sc` → offline `shaderc` SPIR-V → bgfx program → FBO ping-pong). Confirm pixel output matches the JS version at the same speed setting.
3. **Dynamic effect:** Port `Movement` with live GLSL stub editing via glslang. Edit stub in config UI; confirm recompile on change; confirm error message shown on invalid GLSL with correct line number.
4. **User-var bridging + audio:** Port `Dynamic Movement` with a `getspec`-reactive stub. Confirm `AUDIO_GLSL_SRC` injection compiles, uniforms for user vars set correctly, and audio-reactive motion works.
5. **Audio device selection:** Open the recording device selector; switch between devices; confirm spectrum + waveform update from the selected device within one frame.

---

## Potential Next Steps

### MIDI Device Support

Library candidate: **libremidi** (C++17 header-only, fork of RtMidi) — wraps CoreMIDI on macOS, WinMM on Windows, ALSA/JACK/PipeWire on Linux. Supports hot-plug via `libremidi::observer`.

**Mapping model:** `{ midi_channel, cc_number } → { effect_id, param_name, range_min, range_max }`. CC value 0–127 normalised to param range. Note On/Off for `bool` params; `select` params bucketed across CC range. Mappings serialised as a `midiMappings` array in the preset JSON.

**MIDI Learn UX:** right-click a param in `ConfigPanel` → listen for next CC/Note → bind. Mappable types: `range`, `bool`, `select`, `number`.

**Device selection:** `libremidi::observer` enumerates ports with hot-plug callbacks; a panel lists available devices and allows opening multiple ports simultaneously.
