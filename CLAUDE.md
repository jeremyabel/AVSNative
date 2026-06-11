# AVS Native — Claude Context

## What this is

A C++ native port of Winamp AVS (Audio Visualization Studio), targeting Vulkan via bgfx.
The working reference is `ref/AVSWeb/` — a JavaScript/WebGL2 remake of the original.
When implementing or debugging an effect, always check the corresponding JS file in `ref/AVSWeb/src/effects/` first.

## Build

```
cmake --build e:/Workspace/AVSNative/build --config Release --target avs_editor
```

Run with a preset path as the first argument:
```
avs_editor.exe presets/test_movement_feedback.json
```

## Project layout

```
AVS_Native/
  src/
    engine/         Engine, EffectChain, FBOManager, Registry, Preset, ShaderCompiler
    effects/        One .h/.cpp per effect
    ui/             Editor UI (avs_ui lib) — App, ChainPanel, ConfigPanel, ConfigUiRegistry, ConfigUi, FileDialog
    ui/effects/     One bespoke config-UI file per effect (registered in ConfigUiRegistry)
    shaders/        bgfx .sc shader source files (compiled to .bin.h at build time)
  presets/          JSON preset files
  build/            CMake build output
    include/generated/spirv/   Compiled shader headers (*.sc.bin.h)
ref/AVSWeb/
  src/effects/      Reference JS implementations — canonical behavior
```

## Implemented effects

The registered effect list is the source of truth: `Engine.cpp` (the `EffectRegistry.Register(...)`
calls). There are ~33 effects — AddBorders, BufferSave, ChannelShift, Clear, Grain, Invert, Scatter,
UniqueTone, EffectList, FadeOut, MovingParticle, Movement, Starfield, Simple, Mosaic, FastBrightness,
Mirror, Blur, ColorReduction, ColorClip, Blit, RotoBlitter, Colorfade, SetRenderMode, SuperScope,
Interferences, Interleave, MultiFilter, Multiplier, OnBeatClear, Water, DotGrid, BassSpin.

Notable ones:

| Effect | Shader(s) | Notes |
|--------|-----------|-------|
| EffectList | `fs_effectlist_blend.sc` | Inner chain with 14 blend modes; see below |
| Movement | runtime GLSL | Pull/scatter modes, polar/cartesian; bespoke config UI |
| SuperScope | `fs_superscope_blend.sc` | CPU line/dot drawing; LuaJIT Init/Frame/Beat/Point blocks; multi-color cycling; uses `LineBlendMode` (no own blend param) |
| Simple / BassSpin | `fs_simple.sc` | NanoVG-drawn scopes/analyzers |

## LuaJIT scripting (SuperScope and future scriptable effects)

LuaJIT is included as a git submodule at `AVS_Native/lib/luajit` (WohlSoft CMake fork). The CMake target is `libluajit` and it's linked into `avs_engine`.

`LuaRuntime` (`src/engine/LuaRuntime.h/.cpp`) wraps a `lua_State` with a sandboxed env table:
- Math aliases (`sin`, `cos`, `pi`, etc.) and `getspec`/`getosc` C closures are pre-seeded.
- The env table has a `__index = _G` metatable so standard globals (`require`, `getfenv`, `ffi`, etc.) fall through to `_G`.
- `CompileBlock` / `RunBlock` — for Init, Frame, Beat blocks (one `lua_pcall` each).
- `CompilePointLoop` / `RunPointLoop` — builds a Lua wrapper that loops N times with LuaJIT FFI array access, called once per frame. Persistent user vars are copied in/out of the loop via `_env` (the env table captured via `getfenv(1)`).
- `ScanVarDecls` — regex scan for bare assignments in init code to find user-declared vars.
- `SeedVar` — sets a var to 0 in env if not already set. Effects must call this for all user-declared vars (via `ScanVarDecls` over all 4 code blocks) before running any block, or arithmetic on uninitialized vars causes Lua errors.
- `GetError(paramName)` — returns the last runtime/compile error string for a named code block (used by ConfigPanel to display errors below editors).

**Point-loop output buffer:** stride 7 floats per point — `x, y, r, g, b, skip, drawmode`.

**Audio buffer passed to point loop:** `float[n]` values in [0,255] (sized to at least `n`, not just `kAudioBins`):
- Waveform: raw osc values (128 = silence; `/128 - 1` in Lua gives `[-1,1]`).
- Spectrum: packed as `spec * (128/255) + 128` so the same formula gives `[0,1]` in Lua.
- Extra entries beyond `kAudioBins` are filled with `128.0f` (silence) so Lua `audio[_i]` never reads out of bounds when `n > kAudioBins`.

### WohlSoft LuaJIT fork — critical quirk

**`lua_setupvalue(L, -2, 1)` does NOT pop the stack** in this fork. Use `lua_setfenv(L, -2)` instead (correct Lua 5.1 API) when setting the environment of a compiled chunk. Using `setupvalue` leaks the env table on the stack, causing `luaL_ref` to store the env table instead of the chunk — every block ref then points to a table, producing "attempt to call a table value" errors on every call.

## Shader pipeline

There are two paths for shaders:

**Static effects** (most effects):
- Written in bgfx's shading language (`.sc` files in `src/shaders/`)
- Compiled at build time by bgfx's shaderc → SPIRV → embedded as `*.sc.bin.h` headers
- Included in C++ with `#include "generated/spirv/fs_xxx.sc.bin.h"`
- Add new shaders to the `bgfx_compile_shaders()` call in `CMakeLists.txt`

**Dynamic effects** (Movement — user-editable GLSL at runtime):
- Fragment and vertex shaders built as strings in C++ (`BuildPullFragGlsl()` etc.)
- Compiled at runtime via `ShaderCompiler::GlslToSpirv()` → `WrapFragmentSpirv()` / `WrapVertexSpirv()`
- Uses glslang; both VS and FS must go through glslang (mixing precompiled HLSL→SPIRV VS with GLSL→SPIRV FS breaks Vulkan pipeline creation)
- The fullscreen triangle VS uses `gl_VertexIndex` to generate a triangle without a vertex buffer; submit with `bgfx::setVertexCount(3)`

**Shared shader code (both paths):**
- Static `.sc` shaders can `#include "foo.sh"` from `src/shaders/` — that dir is on the shaderc include path via `INCLUDE_DIRS` in the `bgfx_compile_shaders(FRAGMENT …)` call. `texelFetch`/`textureSize` and a `sampler2D` function parameter are valid in the spirv profile (native GLSL types).
- Runtime-GLSL effects can't include a file; they concatenate snippets from `src/engine/ShaderSnippets.h` (C++ `inline constexpr const char*` strings) into their generated source.
- **Bilinear-compat** is the canonical example: the original AVS used an 8-bit integer 2×2 blend (`blend_bilinear_2x2`), not hardware bilinear. `bilinearCompat(tex, uv, sz)` reproduces it bit-exactly. Two synced copies: `src/shaders/bilinear_compat.sh` (static) and `avs::kBilinearCompatGlsl` in `ShaderSnippets.h` (dynamic). Warp/displacement effects expose a "Bilinear (precise)" toggle that selects it: when on, bind the source `BGFX_SAMPLER_*_POINT` (the function does its own nearest `texelFetch`) and pass a compat flag uniform. Wired into Movement, Dynamic Movement, Dynamic Distance Modifier, Dynamic Shift, RotoBlitter, Blit. Edit one `.sh`/snippet pair to change the math everywhere.

## Editor UI

The editor is split across two SDL3 windows:
- **Editor window** — hosts ImGui panels (primary bgfx surface, view 255)
- **Output window** — shows the AVS renderer output via a native-handle bgfx framebuffer (view 254)

`App` (in `src/ui/`) owns both windows, bgfx init, and the main loop. `Engine::Tick()` never calls `bgfx::frame()` or `bgfx::reset()` — those belong to `App`.

**File menu** — `App` has `m_pendingLoad` / `m_pendingSave` booleans set by menu items. They are checked at the top of the main loop (before `RenderUI`) so the SDL3 file dialog runs outside the ImGui frame. `FileDialog::Open()` / `FileDialog::Save()` use `SDL_ShowOpenFileDialog` / `SDL_ShowSaveFileDialog` — cross-platform, no Windows-specific code.

**CMake targets:**
- `avs_engine` — static lib: engine + effects, no UI headers
- `avs_ui` — static lib: `src/ui/` + self-hosted Dear ImGui (docking) + backends
- `avs_editor` — executable: links `avs_ui` (which pulls in `avs_engine`)

**ImGui integration** uses our own Dear ImGui **docking branch** (git submodule at `lib/imgui`, pinned to `v1.92.8-docking`), not bgfx's bundled fork. Platform input is the official `imgui_impl_sdl3` backend (`lib/imgui/backends/`); rendering is our own `imgui_impl_bgfx` (`src/ui/backends/imgui_impl_bgfx.{h,cpp}`, derived from bgfx's example render path and implementing the 1.92 `ImTextureData` texture API — it reuses only the example's compiled shader blobs `vs/fs_ocornut_imgui.bin.h`). `App` wires them: `ImGui_ImplSDL3_InitForOther` + `ImGui_ImplBgfx_Init(255)` in `Init`; `ImGui_ImplSDL3_ProcessEvent` in `ProcessEvents`; `NewFrame` trio at the end of `ProcessEvents`; `ImGui::Render()` + `ImGui_ImplBgfx_RenderDrawData` at the end of `RenderUI`. `IMGUI_DEFINE_MATH_OPERATORS` is defined PUBLIC on `avs_ui` (and `imgui_gradient`) so the inline ImVec operators are consistent across TUs. `imgui_gradient` and `ImGuiColorTextEdit` compile against `lib/imgui`. The C++ standard is C++20 (required by bx SIMD headers).

nanovg's fontstash needs an external `stb_truetype` implementation (it used to come from bgfx's example imgui backend); `src/thirdparty/StbTrueTypeImpl.cpp` now provides it.

**Docking UI** — `App::RenderUI` hosts a `DockSpaceOverViewport` (PassthruCentralNode). The Chain/Properties panels are plain dockable `ImGui::Begin` windows (no manual pos/size). On first run (no `imgui.ini`), `App::BuildDefaultDockLayout` uses `DockBuilder*` to dock **Effect Chain** left (~30%) and **Properties** in the central node; afterwards the layout persists via `imgui.ini` in the working directory. The FPS status bar is a bottom `BeginViewportSideBar`, created before the dockspace so it reserves its space.

**View ID allocation:**
- Views 0–N: effect chain render passes
- View 254: engine blit → output window framebuffer (`Engine::SetOutputFrameBuffer()`)
- View 255: ImGui → editor window backbuffer (`BGFX_INVALID_HANDLE`)

**ChainPanel** — effect list with enable checkbox, selectable name, drag-drop reorder, remove button, and an Add combo. Effect names come from `Registry::Names()`.

**ConfigPanel** — thin host: window chrome, then looks up the selected effect's name in the
`ConfigUiRegistry` and calls its draw function (falling back to `DrawDefault` if none registered).
It resets the shared code editors (`ConfigUi::ResetEditors()`) when the selected effect changes.

**ConfigUiRegistry** (`src/ui/ConfigUiRegistry.h/.cpp`) — maps an effect's display name → a
`void(Effect*)` draw function. `RegisterAllEffectUis()` explicitly calls each effect's
`Register<Effect>UI(reg)` (mirroring the explicit effect registration in `Engine.cpp` — do NOT rely
on static-initializer self-registration, it gets stripped from the static lib). `DrawDefault(effect)`
is the descriptor-driven auto-generator (the old `ConfigPanel` switch): it reads
`GetDescriptor()` + `GetConfig()`/`SetConfig()` and works for any reflected effect with no custom UI.

**Per-effect UI** — one file per effect in `src/ui/effects/<Effect>UI.cpp`. Most just register
`&DrawDefault`. Bespoke ones (e.g. `MovementUI.cpp`) `static_cast` the `Effect*` to the concrete type,
draw plain ImGui against `effect->ConfigRef()` (the typed `Config` struct), and call
`effect->NotifyConfigChanged({keys})` so side-effects (shader/Lua recompiles) fire. This keeps bespoke
UIs close to idiomatic ImGui — free to use `BeginDisabled`, conditional hiding, `SeparatorText`, etc.

**ConfigUi** (`src/ui/ConfigUi.h/.cpp`) — small shared helpers: `CodeEditor(id, text, lang)` owns the
persistent `TextEditor` (ImGuiColorTextEdit) instances and re-syncs when `text` changes programmatically;
`ColorEdit` converts uint8 RGB ↔ ImGui float[3]; `ResetEditors()` clears editor state.

**ImGuiColorTextEdit** (`future` branch) is at `lib/ImGuiColorTextEdit/`. Sources are added directly to the `avs_ui` CMake target (no sub-CMakeLists). Include path: `${BGFX_3RDPARTY}/dear-imgui` (for `imgui.h`) and `lib/ImGuiColorTextEdit`. API: `ed.SetLanguage(TextEditor::Language::Lua())`, `ed.SetText(...)`, `ed.GetText()`, `ed.Render("##id", size)`. The editor instances are owned by `ConfigUi.cpp`.

**EffectChain** — `Remove()` calls `effect->Destroy()` before erasing. `Clear()` destroys all effects and is called by `Engine::Shutdown()` before `bgfx::shutdown()` to ensure bgfx handles are released while the API is still live.

## Effect parameters (reflection)

Parameters are declared **once** per effect, eliminating the old 3-way duplication between
`GetDescriptor`/`GetConfig`/`SetConfig`. The machinery is in `src/engine/Reflect.h`:

- Each effect declares a plain `struct <Effect>Config { ... };` — one member per serializable param
  (use `std::array<uint8_t,3>` for colors, `std::vector<std::array<uint8_t,3>>` for color lists).
- The effect inherits `ReflectedEffect<Config>` (not `Effect` directly) and accesses its config via the
  inherited `Cfg` member (e.g. `Cfg.Amount`).
- It implements `Fields()` returning a `static const std::vector<Field>` built with typed factories that
  bind a pointer-to-member to a JSON key + UI metadata: `Range`, `RangeI`, `RangeIArr` (int[] element),
  `NumberI`, `Bool`, `SelectI` (int index), `SelectS` (string value), `Color`, `Colors`, `Glsl`, `Lua`.
- `ReflectedEffect` implements `GetDescriptor`/`GetConfig`/`SetConfig` generically from that table.
  Range/Number factories clamp on load automatically.
- **Side-effects** (recompiling a shader/Lua block when a field changes) go in an overridden
  `OnConfigChanged(changedKeys)` — called by `SetConfig` and by the UI's `NotifyConfigChanged`. See
  `Movement` (GLSL recompile), `SuperScope` (Lua recompile), `RotoBlitter`/`Interleave`/`ColorFade`
  (animation-state resets).
- Non-serialized runtime state stays as ordinary private members, not in `Config`.
- Two effects override `GetConfig`/`SetConfig` on top of the generic ones: `EffectList` (appends its
  inner-chain `effects` array) and `DotGrid` (legacy `color` alias for `colors[0]`).

## Adding an effect

1. Add `MyEffect.h` / `MyEffect.cpp` in `src/effects/`:
   - declare `struct MyEffectConfig { ... };` and `class MyEffect : public ReflectedEffect<MyEffectConfig>`
   - implement `Fields()` (the param table) and `EffectName()`; read config via `Cfg.*` in `Render`
   - override `OnConfigChanged` only if a param has a side-effect
2. Register the effect in `Engine.cpp` (the `EffectRegistry.Register(...)` list).
3. Add `src/ui/effects/MyEffectUI.cpp` with `RegisterMyEffectUI(ConfigUiRegistry&)` (usually just
   `reg.Register("My Effect", &DrawDefault);`), declare + call it in `ConfigUiRegistry.cpp`'s
   `EFFECT_UI(...)` lists, and add the file to the `avs_ui` target in `CMakeLists.txt`.
4. If it needs a shader: add the `.sc` file to `src/shaders/` and register it in `CMakeLists.txt`.
5. Reference the JS implementation in `ref/AVSWeb/src/effects/<name>.js` for behavior.

## bgfx conventions

- **Never call `bgfx::renderFrame()` explicitly.** In single-threaded mode, `bgfx::frame()` handles it internally. Calling `renderFrame()` again re-processes the command buffer and crashes.
- Effects read from `Context.InputTexture`, draw into `Context.ViewId` (which is bound to `Context.OutputFBO`), then call `Context.FboManager->Swap()`.
- Sampler uniforms: `bgfx::createUniform("s_name", bgfx::UniformType::Sampler)`, then `bgfx::setTexture(slot, handle, texture)`.
- Vec4 uniforms: `bgfx::createUniform("u_name", bgfx::UniformType::Vec4)`, then `bgfx::setUniform(handle, float[4])`.
- **Shared quad vertex buffer** — `Engine` owns `BlitQuadVB` and exposes it via `Context.QuadVB`. Effects use `Context.QuadVB` directly; they do not create their own quad VBs.
- **Point-sample inputs in per-pixel effects that branch on color.** FBO textures are created with the default **bilinear** filter (`FBOManager`, no `BGFX_SAMPLER_POINT`). For a 1:1 fullscreen pass, bilinear at texel centers is exact — but any sub-texel offset blends neighbors and injects **sub-LSB per-channel float noise**. If a shader makes a hard branching decision on the sampled color (e.g. strict channel-dominance tests `g > b`), that noise can flip the branch: a bit-exact gray pixel gets pushed into a color branch and tinted. Fix: override the sampler to point for that bind — `bgfx::setTexture(slot, uniform, tex, BGFX_SAMPLER_POINT)` — and/or round the reconstructed channels to integers before comparing (`floor(c*255.0 + 0.5)`) to match the original's integer pixel arithmetic. This was the root cause of a phantom cyan tint in Colorfade; see `fs_colorfade.sc` + `ColorFade::Render`.
- **Feedback-buffer resampling: match the win32 original's nearest/bilinear choice.** Effects that warp/displace the ping-pong buffer and feed the result back (Movement, Dynamic Movement, RotoBlitter, Blitter Feedback, Dynamic Shift, Interferences) resample the *previous* frame's output. Bilinear filtering at fractional offsets is a spatial low-pass that **compounds across frames**: every bright pixel bleeds into its neighbors, those bleed further next frame, and the smear grows. Two regimes:
  - **Replace / 50-50 feedback** (RotoBlitter, Blit, Dynamic Shift): bilinear merely over-softens and blooms. The win32 effects expose a `Bilinear` checkbox for exactly this; mirror its **default** (RotoBlitter/DynamicShift default on, Blitter Feedback defaults **off**) and gate the sampler with `Cfg.Bilinear ? bilinear-flags : (BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | …clamp)`.
  - **Additive multi-tap feedback** (Interferences): gain ≥ 1 *plus* bilinear diffusion → the buffer **blows out to white** within a few frames. The original stays bounded because it samples at **integer pixel offsets with nearest-neighbor** and does integer-truncated color math. Port faithfully: snap offsets to whole texels (`(int)(cos·dist)` then `/dim`), point-sample, treat out-of-bounds as black (not clamp-to-edge — clamp re-injects a bright edge column every frame), and do the alpha multiply in 0–255 integer space (`floor(v*alpha/255)`, matching `lut_u8_multiply`). See `fs_interferences.sc` + `Interferences::Render`.
  - Effects that sample at **texel centers** (Scatter, Mosaic: integer offset `+ 0.5`, then `/dim`) are bit-exact under bilinear and need no fixup. Neighbor-tap filters at integer texel offsets (Water, Bump, Convolution) and damped sims are bounded by construction.

## Coordinate conventions

- bgfx/Vulkan textures: UV (0,0) = top-left. This differs from WebGL where (0,0) = bottom-left.
- `vs_fullscreen.sc` maps NDC → UV as: `u = x * 0.5 + 0.5`, `v = 0.5 - y * 0.5`. This correctly puts UV (0,0) at the top-left of the texture.
- Movement uses `y = -(Uv.y * 2.0 - 1.0)` in the pull fragment shader (matching the JS reference), and `gl_Position.y = 1.0 - DestUV.y * 2.0` in the scatter VS — both correct for the Vulkan/bgfx UV convention.
- Shader debug log: `avs_shader.log` in the working directory; also printed to stdout.

## Colors

Effect config properties store colors as `std::array<uint8_t, 3>` (0–255), serialized to JSON as `[r,g,b]`. Divide by 255.0f when passing to GPU uniforms. Use the `Color` / `Colors` field factories to bind them.

## Preset serialization

`Preset::Load` and `Preset::Save` are in `src/engine/Preset.cpp`. The JSON format is:

```json
{
  "effects": [
    { "type": "Effect Name", "enabled": true, "config": { ... } }
  ]
}
```

`EffectList` stores its inner chain under `config.effects` (same schema, nested). `Preset::Load` uses a recursive `LoadChain()` helper — after calling `SetConfig` on any effect it checks `effect->GetInnerChain()` and if non-null, recurses into `config["effects"]`. This means `EffectList::SetConfig` only handles its own properties; it does not create inner effects (it has no access to the Registry).

## Key files

| File | Purpose |
|------|---------|
| `src/engine/ShaderCompiler.h/.cpp` | Runtime GLSL→SPIRV + bgfx shader blob wrapping |
| `src/engine/FBOManager.h/.cpp` | Ping-pong FBO + scratch buffers |
| `src/engine/Effect.h` | Base class + `RenderContext` struct (includes `QuadVB`, `LineBlendMode`) |
| `src/engine/LuaRuntime.h/.cpp` | LuaJIT sandboxed scripting runtime; see WohlSoft quirk above |
| `src/engine/Reflect.h` | Param reflection: `Field`, factory helpers, `ReflectedEffect<Config>` base |
| `src/engine/Preset.h/.cpp` | JSON preset load/save with recursive EffectList support |
| `src/ui/ConfigPanel.cpp` | Thin host that dispatches to the per-effect UI registry |
| `src/ui/ConfigUiRegistry.h/.cpp` | Name→draw-fn registry + `DrawDefault` auto-generator + central registration |
| `src/ui/ConfigUi.h/.cpp` | Shared UI helpers (code editor instances, color conversion) |
| `src/ui/effects/*UI.cpp` | One bespoke config-UI file per effect |
| `src/ui/FileDialog.h/.cpp` | SDL3-based cross-platform native file picker |
| `src/effects/SuperScope.cpp` | CPU line/dot oscilloscope with LuaJIT scripting and multi-color |
| `src/effects/Movement.cpp` | Example of runtime-compiled dynamic shader effect |
| `src/effects/MovingParticle.cpp` | Example of static precompiled shader effect |
| `src/effects/EffectList.cpp` | Inner chain effect with 14 blend modes |
| `ref/AVSWeb/src/effects/movement.js` | Canonical reference for Movement polar coordinate math |
| `ref/AVSWeb/src/effects/effect-list.js` | Canonical reference for EffectList blend logic |
| `ref/AVSWeb/src/effects/superscope.js` | Canonical reference for SuperScope color cycling and draw logic |
