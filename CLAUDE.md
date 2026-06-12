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

### macOS

The renderer is Vulkan/SPIRV on every platform (all shader blobs are SPIRV-only,
and the runtime GLSL→SPIRV effects need a SPIRV backend). On macOS Vulkan runs
through **MoltenVK**, which is a required dependency:

```
brew install molten-vk
```

Two macOS-only gotchas are handled in code (Apple-guarded, so the Windows path is
untouched). Don't remove these:

- **MoltenVK on the dylib path** — bgfx's Vulkan backend `dlopen`s the bare name
  `libMoltenVK.dylib`, but Homebrew installs it to `/opt/homebrew/lib`, which is
  *not* on dyld's default search path. `src/main.cpp` (`EnsureMoltenVKOnDyldPath`)
  finds the dylib and re-exec's once with `DYLD_LIBRARY_PATH` set (a sentinel env
  var prevents a loop; `DYLD_*` is only read at launch, so runtime `setenv` /
  preload-`dlopen` does not work). If MoltenVK is missing, bgfx silently falls
  back to the **Metal** renderer, which can't load SPIRV blobs → fatal "Failed to
  create Vertex shader"; `App` guards against this and prints the `brew` hint.
- **Single-threaded mode** — bgfx defaults to multithreaded
  (`BGFX_CONFIG_MULTITHREADED=1`), but its render thread can't attach the
  swapchain's `CAMetalLayer` to the NSWindow's content view (Cocoa requires the
  main thread) — it fails silently and both windows render solid white. `App`
  calls `bgfx::renderFrame()` once before `bgfx::init()` to force single-threaded
  mode; the loop still only calls `bgfx::frame()` (see bgfx conventions below).

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
Interferences, Interleave, MultiFilter, Multiplier, OnBeatClear, Water, DotGrid, BassSpin, Ramp, Strobe.

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
- `dt` (read-only number) — elapsed seconds since the previous frame, the same value for every scripted effect. `Engine::Tick` computes it from a monotonic clock (clamped to ≤0.25s to survive stalls) and calls `LuaRuntime::SetFrameDelta`; each `LuaRuntime` seeds it into env (`SetEnvNumber("dt", …)`) at the top of `RunBlock`/`RunPointLoop`/`RunTriangleLoop`, so it's available in every block (init/frame/beat/point/triangle) with no per-effect wiring. Reserved in `k_reservedEnv`.
- `key(code)` C closure (in env, so available in every Lua block — regular blocks and the point/triangle loop wrappers, which inherit env): returns true if the key is held down this frame. `code` is a number (raw SDL keycode) or a name string (`"a"`, `"Space"`, `"Left"`, `"F1"`); resolved cross-platform via single-char ASCII or `SDL_GetKeyFromName` (`ResolveKeyName` in `LuaRuntime.cpp`). Backed by `avs::KeyInput::IsKeyDown` (the persistent held set fed by `App`). Lua-only — GLSL blocks compile through `ShaderCompiler` and never see it.
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
- **Samplers in runtime GLSL: just write `layout(binding = N) uniform sampler2D NAME;`** — but know it's auto-rewritten. bgfx's Vulkan pipeline layout declares every sampler as a *separate* `SAMPLED_IMAGE` (binding N) + `SAMPLER` (binding N+16, `kSpirvSamplerShift`), matching shaderc's output. A combined `sampler2D` (one COMBINED_IMAGE_SAMPLER) mismatches that layout; desktop Vulkan tolerates it but **MoltenVK crashes** (SPIR-V→MSL null, or a null pipeline bound in submit). So `ShaderCompiler::SeparateCombinedSamplers()` (top of `GlslToSpirv`) rewrites each combined declaration into `texture2D _NAME_t` (N) + `sampler _NAME_s` (N+16) + `#define NAME sampler2D(_NAME_t, _NAME_s)`. `texture()`/`textureSize()`/`texelFetch()` calls are unchanged (point-of-use). **Caveat:** GLSL forbids passing a constructed combined sampler through a function parameter, so a helper that takes a sampler (e.g. `bilinearCompat`) must take separate `texture2D, sampler` and be invoked via the `BILINEAR_COMPAT(name, …)` macro (token-pastes to `_name_t`/`_name_s`). Static `.sc` shaders are unaffected (shaderc has its own sampler-struct mechanism).
- **Bilinear-compat** is the canonical example: the original AVS used an 8-bit integer 2×2 blend (`blend_bilinear_2x2`), not hardware bilinear. `bilinearCompat(tex, uv, sz)` reproduces it bit-exactly. Two synced copies: `src/shaders/bilinear_compat.sh` (static) and `avs::kBilinearCompatGlsl` in `ShaderSnippets.h` (dynamic). Warp/displacement effects expose a "Bilinear (precise)" toggle that selects it: when on, bind the source `BGFX_SAMPLER_*_POINT` (the function does its own nearest `texelFetch`) and pass a compat flag uniform. Wired into Movement, Dynamic Movement, Dynamic Distance Modifier, Dynamic Shift, RotoBlitter, Blit. Edit one `.sh`/snippet pair to change the math everywhere (the two copies share the *blend math* but have different signatures now: the static `.sh` takes a `sampler2D`; the runtime `kBilinearCompatGlsl` takes separate `texture2D, sampler` and is called via the `BILINEAR_COMPAT` macro — see the sampler note above).

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

**ChainPanel** — effect list with enable checkbox, selectable name, drag-drop reorder, an amber **"L"
lock toggle** + red **"x"** remove button per row, and an Add combo. Effect names come from
`Registry::Names()`. Lock toggles `EffectEntry::Locked` (assigning `EffectEntry::Id` on first lock).

**Lockable property panels** — `EffectEntry` (in `EffectChain.h`) carries `bool Locked` + `uint32_t Id`
(0 = unassigned) alongside `Enabled`; both are serialized in the preset (item level, beside `enabled`)
and travel with the effect through reorder/drag. New effects get an id via `AllocEffectId()` in
`EffectChain::Insert`; the load path keeps the serialized id (`NoteEffectId` bumps the allocator past
it). A locked effect gets its own dockable panel (`ConfigPanel::RenderLockedPanels`, walked recursively
over the tree) titled `"<Name>###avsprop<id>"` — the stable id lets imgui.ini reattach its dock
position across reloads (the user chose: locks travel in the preset, dock positions stay machine-local).
Closing a locked panel's window untoggles its lock. This lets several effects be edited at once.

**ConfigPanel** — thin host. `Render(engine, EffectEntry* selected)` draws the shared **Properties**
window: the selected effect's name + its `ConfigUiRegistry` draw function ("No UI registered." if
missing), unless that effect is locked (then it shows a hint, so the same effect's editors are never
drawn twice in one frame). `RenderLockedPanels` draws the per-locked-effect panels. Both route through
a `DrawBody(Effect*)` that calls `ConfigUi::SetEditorScope(effect)` first so code editors are
per-instance. `App::RenderUI` calls `Render`, then `RenderLockedPanels`, then `ConfigUi::EndFramePrune()`.

**ConfigUiRegistry** (`src/ui/ConfigUiRegistry.h/.cpp`) — maps an effect's display name → a
`void(Effect*)` draw function. `RegisterAllEffectUis()` explicitly calls each effect's
`Register<Effect>UI(reg)` (mirroring the explicit effect registration in `Engine.cpp` — do NOT rely
on static-initializer self-registration, it gets stripped from the static lib). Every effect has a
bespoke UI; there is no auto-generated fallback (ConfigPanel shows "No UI registered." if a name is
missing from the registry).

**Per-effect UI** — one file per effect in `src/ui/effects/<Effect>UI.cpp`, all bespoke.
Each `static_cast`s the `Effect*` to the concrete type and binds plain ImGui widgets directly to the
effect's public config members (e.g. `&fx->Amount`). Slider/combo ranges and option labels live only
here. When a param has a side-effect, the UI calls the effect's public method directly (e.g.
`fx->Compile()` after a code edit, `fx->ResetZoomAnim()` after a zoom change). Free to use
`BeginDisabled`, conditional hiding, `SeparatorText`, etc.

**ConfigUi** (`src/ui/ConfigUi.h/.cpp`) — small shared helpers: `CodeEditor(id, text, lang)` owns the
persistent `TextEditor` (ImGuiColorTextEdit) instances and re-syncs when `text` changes programmatically
(always bind it to the effect's member string, never a copy). Editor state is **namespaced per effect
instance** via `SetEditorScope(effect)` (called by `ConfigPanel::DrawBody` before each panel) so two
panels showing different effects — even of the same type — don't share editors; `EndFramePrune()` (once
per frame, after all panels) drops editors not drawn that frame. `ColorEdit` converts uint8 RGB ↔ ImGui
float[3]; `ColorsEdit` edits a color list (add/remove, keeps ≥1 entry); `ResetEditors()` clears all
editor state (full chain clear); `PickImageInto(effect, key)` opens a file dialog and hands raw bytes to
`ApplyAsset`.

**ImGuiColorTextEdit** (`future` branch) is at `lib/ImGuiColorTextEdit/`. Sources are added directly to the `avs_ui` CMake target (no sub-CMakeLists). Include path: `${BGFX_3RDPARTY}/dear-imgui` (for `imgui.h`) and `lib/ImGuiColorTextEdit`. API: `ed.SetLanguage(TextEditor::Language::Lua())`, `ed.SetText(...)`, `ed.GetText()`, `ed.Render("##id", size)`. The editor instances are owned by `ConfigUi.cpp`.

**EffectChain** — `Remove()` calls `effect->Destroy()` before erasing. `Clear()` destroys all effects and is called by `Engine::Shutdown()` before `bgfx::shutdown()` to ensure bgfx handles are released while the API is still live.

## Effect parameters (direct members + per-effect serialization)

Each effect inherits `Effect` directly and stores its parameters as **public members** on the class
(use `std::array<uint8_t,3>` for colors, `std::vector<std::array<uint8_t,3>>` for color lists; defaults
live in the member initializers). Runtime-only state stays private. There is no reflection layer.

- JSON key names are `static constexpr const char*` constants on the class (e.g.
  `static constexpr const char* kAmount = "amount";`) — each key string is written once and shared by
  `Serialize`/`Deserialize` (and any legacy aliases).
- `std::string Name() const` returns the display name; it must exactly match the `Engine.cpp` Registry
  key and the ConfigUiRegistry key, and is used as the preset `"type"` string.
- `nlohmann::json Serialize() const` returns `{ { kAmount, Amount }, ... }` (colors via
  `JsonUtil::ColorToJson`/`ColorsToJson`).
- `void Deserialize(const nlohmann::json& j)` reads via the reference-based helpers in
  `src/engine/JsonUtil.h` (`ReadInt`, `ReadFloat`, `ReadBool`, `ReadString`, `ReadColor`, `ReadColors`):
  if the key exists, the member is assigned; otherwise it keeps its default. No clamping or validation —
  ranges live only in the UI widgets. `ReadColors` ignores empty arrays so color lists keep ≥1 entry.
- **Side-effects** (shader/Lua recompiles, animation-state resets) are public methods on the effect
  (e.g. `Movement::Compile`, `SuperScope::Recompile`, `RotoBlitter::ResetZoomAnim`). `Deserialize`
  calls them at its end so they fire on preset load; the bespoke UI calls them directly when the
  relevant widget changes.
- Special cases: `EffectList::Serialize` emits only its own params — the inner chain is appended as
  `config["effects"]` by Preset's `SerialiseChain` (via `GetInnerChain()`) and populated on load by
  `LoadChain`'s recursion. `DotGrid` keeps a legacy `"color"` alias (= `Colors[0]`). `MultiDelay`'s
  shared per-buffer settings (`usebeats0..5`/`delay0..5`) serialize through static accessors over the
  global singleton. Asset effects (Picture, Picture2, Texer, Texer2, ImageGrid) store the
  `assets/<name>` path string in `imageData`; raw bytes arrive via `ApplyAsset` after `Deserialize`.

## Adding an effect

1. Add `MyEffect.h` / `MyEffect.cpp` in `src/effects/`:
   - `class MyEffect : public Effect` with public config members (+ defaults) and
     `static constexpr const char* k<Param>` key constants
   - implement `Name()`, `Serialize()`, `Deserialize()` (JsonUtil `Read*` helpers), and
     Init/Render/Destroy; if a param has a side-effect, give it a public method and call it at the
     end of `Deserialize`
2. Register the effect in `Engine.cpp` (the `EffectRegistry.Register(...)` list).
3. Add `src/ui/effects/MyEffectUI.cpp` with `RegisterMyEffectUI(ConfigUiRegistry&)` registering a
   bespoke draw function that casts to `MyEffect*` and binds ImGui widgets to the public members
   (calling side-effect methods on change), declare + call it in `ConfigUiRegistry.cpp`'s
   `EFFECT_UI(...)` lists, and add the file to the `avs_ui` target in `CMakeLists.txt`.
4. If it needs a shader: add the `.sc` file to `src/shaders/` and register it in `CMakeLists.txt`.
5. Reference the JS implementation in `ref/AVSWeb/src/effects/<name>.js` for behavior.

## bgfx conventions

- **Never call `bgfx::renderFrame()` explicitly *in the loop*.** In single-threaded mode, `bgfx::frame()` handles it internally; calling `renderFrame()` again re-processes the command buffer and crashes. The one exception is the single pre-`init` call on macOS that *selects* single-threaded mode (see Build → macOS) — that's a one-shot before any `frame()`, not a per-frame call.
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

Effect config properties store colors as `std::array<uint8_t, 3>` (0–255), serialized to JSON as `[r,g,b]` via `JsonUtil::ColorToJson` / `ReadColor` (lists via `ColorsToJson` / `ReadColors`). Divide by 255.0f when passing to GPU uniforms. UI editing via `ConfigUi::ColorEdit` / `ColorsEdit`.

## Preset serialization

`Preset::Load` and `Preset::Save` are in `src/engine/Preset.cpp`. The preset JSON is:

```json
{
  "effects": [
    { "type": "Effect Name", "enabled": true, "config": { ... } }
  ]
}
```

`EffectList` stores its inner chain under `config.effects` (same schema, nested). `Preset::Load` uses a recursive `LoadChain()` helper — after calling `SetConfig` on any effect it checks `effect->GetInnerChain()` and if non-null, recurses into `config["effects"]`. This means `EffectList::SetConfig` only handles its own properties; it does not create inner effects (it has no access to the Registry).

### Bundle format (`.avsz`) — binary assets are NOT base64

Presets are saved as a **store-only ZIP** named `*.avsz` (no compression — pure bundling, openable by any unzip tool) containing `preset.json` plus the raw, unmodified image/GIF files under `assets/`. Base64 is gone entirely (it was slow for multi-MB GIFs). The zip layer is `src/engine/ZipArchive.h/.cpp` — a self-contained store reader/writer with its own CRC32 (don't reuse the vendored miniz: its archive APIs are disabled and it lives in `bimg` → duplicate `mz_*` symbols).

- **Asset interface** (`src/engine/Effect.h`): asset-bearing effects override `CollectAssets()` (returns `PresetAsset{Key, Name, Bytes}` to bundle on save) and `ApplyAsset(key, name, bytes)` (receives raw bytes on load, after `SetConfig`). Empty defaults, so non-asset effects need nothing. The 5 asset effects are ImageGrid, Picture, Picture2, Texer, Texer2 — each retains the raw bytes + original filename and builds its texture via a `BuildFromRaw(bytes)`. Their `imageData` config value is the bundle path `assets/<name>`, not a data URL; `SetConfig` just stores the string, bytes arrive via `ApplyAsset`.
  - **Keyed image arrays** (Texer2, ImageGrid): both have a `Mode` (0 = Single Image — the `imageData` slot above; 1 = Keyed Array). In array mode a per-instance `KeyedImageList Keyed` (`src/engine/KeyedImageList.h`) holds N `{Raw, Name, Keycode}` entries, each a separate bundle asset under the top-level key `imageN` (never collides with `imageData` — non-digit suffix), plus an int `imageKeys` array and `selectedImage` index. Pressing a bound key switches the displayed image (persists). Keyboard plumbing: `src/engine/KeyInput.h` (SDL-free global keyboard layer; App feeds it in `ProcessEvents`, gated by `!WantTextInput`). It tracks both a per-frame key-DOWN **edge** set (`WasKeyPressed`/`LastKeyPressed`, cleared each `BeginFrame`) and a persistent **held** set (`SetKeyDown`/`IsKeyDown`/`ClearHeld`, fed by key-down AND key-up; App calls `ClearHeld()` on focus loss to avoid stuck keys). Edge polling drives image/map switching (effects poll `Keyed.UpdateSelection()` / their own keycode list in `Render`); held polling drives hold-to-trigger effects (Strobe). The reusable UI press-to-capture button is `ConfigUi::KeyCaptureButton(id, owner, index, keycode&)` (left-click to bind, right-click to clear; used by Color Map's per-map keys and Strobe's hold key). `KeyedImageListEditor` predates it and has its own equivalent inline capture.
- **Save** (`Preset.cpp`): a recursive `SerialiseChain(chain, assets, usedNames)` walks the tree, calls `CollectAssets()` per effect, stores each asset as `assets/<original-filename>` (with `_N` on basename collisions via `usedNames`), and rewrites `config[key]` to that path. `preset.json` is the first zip entry. EffectList recursion rebuilds nested `config.effects` so nested asset refs are rewritten too.
- **Load** auto-detects by magic bytes: `PK\x03\x04` → bundle (read zip, parse `preset.json`, and in `LoadChain` after `SetConfig` resolve any string config value starting with `assets/` to bytes → `ApplyAsset`); otherwise plain JSON (asset-less presets still load). Legacy base64 `.json` presets are NOT supported — re-make them as bundles.
- **UI**: `ConfigUi::PickImageInto(effect, key)` reads the picked file and hands raw bytes + basename to `effect->ApplyAsset` — no base64 encode (the old per-UI `Base64Encode` helpers are gone). `FileDialog` filters: open `avsz;json`, save `avsz`; `App.cpp` appends `.avsz` if the user omits it.

## Key files

| File | Purpose |
|------|---------|
| `src/engine/ShaderCompiler.h/.cpp` | Runtime GLSL→SPIRV + bgfx shader blob wrapping |
| `src/engine/FBOManager.h/.cpp` | Ping-pong FBO + scratch buffers |
| `src/engine/Effect.h` | Base class + `RenderContext` struct (includes `QuadVB`, `LineBlendMode`) |
| `src/engine/LuaRuntime.h/.cpp` | LuaJIT sandboxed scripting runtime; see WohlSoft quirk above |
| `src/engine/JsonUtil.h` | Reference-based JSON readers (`ReadInt`/`ReadColor`/…) + color↔JSON converters for effect Serialize/Deserialize |
| `src/engine/Preset.h/.cpp` | `.avsz` bundle (or plain-JSON) preset load/save; recursive EffectList + asset collect/resolve |
| `src/engine/ZipArchive.h/.cpp` | Self-contained store-only (no compression) zip reader/writer + CRC32 for `.avsz` bundles |
| `src/ui/ConfigPanel.cpp` | Thin host that dispatches to the per-effect UI registry |
| `src/ui/ConfigUiRegistry.h/.cpp` | Name→draw-fn registry + central registration of every per-effect UI |
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
