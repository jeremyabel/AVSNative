cmake_minimum_required(VERSION 3.25)
project(avs_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── bgfx + bx + bimg ─────────────────────────────────────────────────────────
set(BGFX_BUILD_TOOLS    ON  CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/bgfx.cmake)

# ── SDL3 (static) ─────────────────────────────────────────────────────────────
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/SDL3)

# ── nlohmann/json (header-only, git submodule at lib/nlohmann_json) ───────────
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/nlohmann_json)

# ── kissfft (FFT for audio spectrum analysis, git submodule at lib/kissfft) ───
set(KISSFFT_DATATYPE float CACHE STRING "" FORCE)
set(KISSFFT_STATIC   ON    CACHE BOOL   "" FORCE)
set(KISSFFT_TEST     OFF   CACHE BOOL   "" FORCE)
set(KISSFFT_TOOLS    OFF   CACHE BOOL   "" FORCE)
add_subdirectory(lib/kissfft)

# ── dr_libs (single-header MP3 decoder) ──────────────────────────────────────
add_library(dr_libs INTERFACE)
target_include_directories(dr_libs INTERFACE lib/dr_libs)

# ── LuaJIT (WohlSoft CMake fork, git submodule at lib/luajit) ─────────────────
set(LUAJIT_DISABLE_JIT  OFF CACHE BOOL "" FORCE)
set(LUAJIT_DISABLE_FFI  OFF CACHE BOOL "" FORCE)
set(LUAJIT_ENABLE_LUA52 OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/luajit)

# ── imgui_gradient (gradient editor widget, git submodule at lib/imgui_gradient) ─
# Built as a static lib that needs imgui headers to compile; imgui symbols are
# provided by avs_ui at final link. IMGUI_DEFINE_MATH_OPERATORS must be PUBLIC so
# consumers including the gradient headers get the ImVec2/ImVec4 operators too.
add_subdirectory(lib/imgui_gradient)
target_include_directories(imgui_gradient SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/imgui)  # imgui.h, imconfig.h, imgui_internal.h (self-hosted docking)
target_compile_definitions(imgui_gradient PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_compile_options(imgui_gradient PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus /Zc:preprocessor>)

# ── Shaders ───────────────────────────────────────────────────────────────────
# bgfx_compile_shaders uses BGFX_SHADER_INCLUDE_PATH but add_subdirectory doesn't set it.
set(BGFX_SHADER_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/bgfx.cmake/bgfx/src")

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/generated)

set(VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/varying.def.sc)

bgfx_compile_shaders(
    TYPE VERTEX
    SHADERS
        src/shaders/vs_fullscreen.sc
        src/shaders/vs_starfield.sc
    VARYING_DEF ${VARYING_DEF}
    OUTPUT_DIR ${SHADER_OUTPUT_DIR}
    OUT_FILES_VAR VS_SHADER_HEADERS
    PROFILES spirv
    AS_HEADERS
)

bgfx_compile_shaders(
    TYPE FRAGMENT
    SHADERS
        src/shaders/fs_addborders.sc
        src/shaders/fs_blit.sc
        src/shaders/fs_channelshift.sc
        src/shaders/fs_clear.sc
        src/shaders/fs_grain.sc
        src/shaders/fs_invert.sc
        src/shaders/fs_scatter.sc
        src/shaders/fs_uniquetone.sc
        src/shaders/fs_buffersave_blend.sc
        src/shaders/fs_effectlist_blend.sc
        src/shaders/fs_fadeout.sc
        src/shaders/fs_movingparticle.sc
        src/shaders/fs_starfield.sc
        src/shaders/fs_simple.sc
        src/shaders/fs_mosaic.sc
        src/shaders/fs_fastbrightness.sc
        src/shaders/fs_mirror.sc
        src/shaders/fs_blur.sc
        src/shaders/fs_colorreduction.sc
        src/shaders/fs_colorclip.sc
        src/shaders/fs_bliteffect.sc
        src/shaders/fs_rotoblitter.sc
        src/shaders/fs_colorfade.sc
        src/shaders/fs_superscope_blend.sc
        src/shaders/fs_interferences.sc
        src/shaders/fs_interleave.sc
        src/shaders/fs_multifilter.sc
        src/shaders/fs_multiplier.sc
        src/shaders/fs_onbeatclear.sc
        src/shaders/fs_bump.sc
        src/shaders/fs_water.sc
        src/shaders/fs_waterbump.sc
        src/shaders/fs_dotgrid.sc
        src/shaders/fs_picture.sc
        src/shaders/fs_picture2.sc
        src/shaders/fs_convolution.sc
        src/shaders/fs_colormap.sc
        src/shaders/fs_dynamicshift.sc
        src/shaders/fs_texer2_comp.sc
        src/shaders/fs_imagegrid.sc
        src/shaders/fs_dotplane.sc
        src/shaders/fs_timescope.sc
        src/shaders/fs_brightness.sc
    VARYING_DEF ${VARYING_DEF}
    OUTPUT_DIR ${SHADER_OUTPUT_DIR}
    OUT_FILES_VAR FS_SHADER_HEADERS
    PROFILES spirv
    AS_HEADERS
)

add_custom_target(avs_shaders DEPENDS ${VS_SHADER_HEADERS} ${FS_SHADER_HEADERS})

set(BGFX_EXAMPLES_COMMON ${CMAKE_CURRENT_SOURCE_DIR}/lib/bgfx.cmake/bgfx/examples/common)
set(BGFX_3RDPARTY        ${CMAKE_CURRENT_SOURCE_DIR}/lib/bgfx.cmake/bgfx/3rdparty)

# ── avs_engine (static lib — no UI headers) ───────────────────────────────────
add_library(avs_engine STATIC
    src/engine/Engine.cpp
    src/engine/EffectChain.cpp
    src/engine/FBOManager.cpp
    src/engine/Registry.cpp
    src/engine/Preset.cpp
    src/engine/ShaderCompiler.cpp
    src/engine/AudioAnalyzer.cpp
    src/effects/AddBorders.cpp
    src/effects/BufferSave.cpp
    src/effects/ChannelShift.cpp
    src/effects/Clear.cpp
    src/effects/Grain.cpp
    src/effects/Invert.cpp
    src/effects/Scatter.cpp
    src/effects/UniqueTone.cpp
    src/effects/EffectList.cpp
    src/effects/FadeOut.cpp
    src/effects/Starfield.cpp
    src/effects/MovingParticle.cpp
    src/effects/Movement.cpp
    src/effects/Simple.cpp
    src/effects/Mosaic.cpp
    src/effects/FastBrightness.cpp
    src/effects/Mirror.cpp
    src/effects/Blur.cpp
    src/effects/ColorReduction.cpp
    src/effects/ColorClip.cpp
    src/effects/BlitEffect.cpp
    src/effects/RotoBlitter.cpp
    src/effects/ColorFade.cpp
    src/effects/SetRenderMode.cpp
    src/effects/SuperScope.cpp
    src/effects/Interferences.cpp
    src/effects/Interleave.cpp
    src/effects/MultiFilter.cpp
    src/effects/Multiplier.cpp
    src/effects/OnBeatClear.cpp
    src/effects/Bump.cpp
    src/effects/Water.cpp
    src/effects/WaterBump.cpp
    src/effects/DotGrid.cpp
    src/effects/BassSpin.cpp
    src/effects/Normalize.cpp
    src/effects/ColorModifier.cpp
    src/effects/RotatingStars.cpp
    src/effects/OscilloscopeStar.cpp
    src/effects/Ring.cpp
    src/effects/Picture.cpp
    src/effects/Picture2.cpp
    src/effects/Convolution.cpp
    src/effects/ColorMap.cpp
    src/effects/CustomBpm.cpp
    src/effects/DynamicDistanceModifier.cpp
    src/effects/DynamicShift.cpp
    src/effects/Texer.cpp
    src/effects/Texer2.cpp
    src/effects/ImageGrid.cpp
    src/effects/MultiDelay.cpp
    src/effects/Triangle.cpp
    src/effects/VideoDelay.cpp
    src/effects/DynamicMovement.cpp
    src/effects/DotFountain.cpp
    src/effects/DotPlane.cpp
    src/effects/Timescope.cpp
    src/effects/Brightness.cpp
    src/effects/Text.cpp
    src/thirdparty/StbImageImpl.cpp
    src/thirdparty/StbTrueTypeImpl.cpp
    src/engine/LuaRuntime.cpp
    src/engine/LuaUniformBridge.cpp
    # NanoVG bgfx backend (used by Simple and future vector effects)
    ${BGFX_EXAMPLES_COMMON}/nanovg/nanovg.cpp
    ${BGFX_EXAMPLES_COMMON}/nanovg/nanovg_bgfx.cpp
)

target_include_directories(avs_engine
    PUBLIC  src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include

    PRIVATE ${BGFX_EXAMPLES_COMMON}                                             # <nanovg/nanovg.h>, <nanovg/nanovg_bgfx.h>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/bgfx.cmake/bx/include               # <bx/bx.h> etc. for nanovg sources
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/bgfx.cmake/bgfx/include             # <bgfx/embedded_shader.h>
    PRIVATE ${BGFX_3RDPARTY}                                                    # <stb/stb_truetype.h> for fontstash (nanovg text)
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/bgfx.cmake/bimg/3rdparty           # <stb/stb_image.h> for Picture
)

target_compile_options(avs_engine PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus /Zc:preprocessor>)

target_link_libraries(avs_engine PUBLIC
    bgfx
    nlohmann_json::nlohmann_json
    glslang
    spirv-opt
    SDL3-static
    kissfft::kissfft-float
    dr_libs
    libluajit
)

add_dependencies(avs_engine avs_shaders)

# ── avs_ui (static lib — depends on avs_engine + imgui) ──────────────────────
add_library(avs_ui STATIC
    # Dear ImGui core (self-hosted docking branch, git submodule at lib/imgui)
    lib/imgui/imgui.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
    # official SDL3 platform backend + our own bgfx renderer backend
    lib/imgui/backends/imgui_impl_sdl3.cpp
    src/ui/backends/imgui_impl_bgfx.cpp
    # ImGuiColorTextEdit (code editor widget)
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/ImGuiColorTextEdit/TextEditor.cpp
    # editor UI panels
    src/ui/App.cpp
    src/ui/ChainPanel.cpp
    src/ui/ConfigPanel.cpp
    src/ui/ConfigUi.cpp
    src/ui/ConfigUiRegistry.cpp
    src/ui/FileDialog.cpp
    # per-effect bespoke config UIs (one file per effect)
    src/ui/effects/AddBordersUI.cpp
    src/ui/effects/BufferSaveUI.cpp
    src/ui/effects/ChannelShiftUI.cpp
    src/ui/effects/ClearUI.cpp
    src/ui/effects/GrainUI.cpp
    src/ui/effects/InvertUI.cpp
    src/ui/effects/ScatterUI.cpp
    src/ui/effects/UniqueToneUI.cpp
    src/ui/effects/EffectListUI.cpp
    src/ui/effects/FadeOutUI.cpp
    src/ui/effects/MovingParticleUI.cpp
    src/ui/effects/MovementUI.cpp
    src/ui/effects/StarfieldUI.cpp
    src/ui/effects/SimpleUI.cpp
    src/ui/effects/MosaicUI.cpp
    src/ui/effects/FastBrightnessUI.cpp
    src/ui/effects/MirrorUI.cpp
    src/ui/effects/BlurUI.cpp
    src/ui/effects/ColorReductionUI.cpp
    src/ui/effects/ColorClipUI.cpp
    src/ui/effects/BlitEffectUI.cpp
    src/ui/effects/RotoBlitterUI.cpp
    src/ui/effects/ColorFadeUI.cpp
    src/ui/effects/SetRenderModeUI.cpp
    src/ui/effects/SuperScopeUI.cpp
    src/ui/effects/InterferencesUI.cpp
    src/ui/effects/InterleaveUI.cpp
    src/ui/effects/MultiFilterUI.cpp
    src/ui/effects/MultiplierUI.cpp
    src/ui/effects/OnBeatClearUI.cpp
    src/ui/effects/BumpUI.cpp
    src/ui/effects/WaterUI.cpp
    src/ui/effects/WaterBumpUI.cpp
    src/ui/effects/DotGridUI.cpp
    src/ui/effects/BassSpinUI.cpp
    src/ui/effects/NormalizeUI.cpp
    src/ui/effects/ColorModifierUI.cpp
    src/ui/effects/RotatingStarsUI.cpp
    src/ui/effects/OscilloscopeStarUI.cpp
    src/ui/effects/RingUI.cpp
    src/ui/effects/PictureUI.cpp
    src/ui/effects/Picture2UI.cpp
    src/ui/effects/ConvolutionUI.cpp
    src/ui/effects/ColorMapUI.cpp
    src/ui/effects/CustomBpmUI.cpp
    src/ui/effects/DynamicDistanceModifierUI.cpp
    src/ui/effects/DynamicShiftUI.cpp
    src/ui/effects/TexerUI.cpp
    src/ui/effects/Texer2UI.cpp
    src/ui/effects/ImageGridUI.cpp
    src/ui/effects/MultiDelayUI.cpp
    src/ui/effects/TriangleUI.cpp
    src/ui/effects/VideoDelayUI.cpp
    src/ui/effects/DynamicMovementUI.cpp
    src/ui/effects/DotFountainUI.cpp
    src/ui/effects/DotPlaneUI.cpp
    src/ui/effects/TimescopeUI.cpp
    src/ui/effects/BrightnessUI.cpp
    src/ui/effects/TextUI.cpp
)

target_include_directories(avs_ui
    PUBLIC  src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/imgui                       # "imgui.h", "imgui_internal.h" (also for TextEditor.h)
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/imgui/backends              # "imgui_impl_sdl3.h"
    PRIVATE ${BGFX_EXAMPLES_COMMON}/imgui                               # vs/fs_ocornut_imgui.bin.h (shader blobs only)
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/ImGuiColorTextEdit          # "TextEditor.h"
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/imgui_gradient/include      # <imgui_gradient/imgui_gradient.hpp>
)

# Match imgui_gradient's PUBLIC IMGUI_DEFINE_MATH_OPERATORS so the inline ImVec2/ImVec4
# operators have identical definitions across every TU that includes imgui.h (ODR).
target_compile_definitions(avs_ui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

target_link_libraries(avs_ui
    PUBLIC  avs_engine
    PUBLIC  imgui_gradient
    PRIVATE bx
    PRIVATE bimg
    PRIVATE SDL3-static
)

# ── Editor executable ─────────────────────────────────────────────────────────
add_executable(avs_editor
    src/main.cpp
)

target_link_libraries(avs_editor PRIVATE
    avs_ui
    SDL3-static
)

if(WIN32)
    set_target_properties(avs_editor PROPERTIES WIN32_EXECUTABLE FALSE)
endif()
