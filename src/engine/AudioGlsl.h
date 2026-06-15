#pragma once

// GLSL function definitions for reading audio inside a runtime-compiled fragment
// shader, ported from AVSWeb's AUDIO_GLSL_SRC (ref/AVSWeb/src/core/audio-data.js).
//
// The including shader must declare the audio sampler itself with an explicit
// binding, e.g.:   layout(binding = 3) uniform sampler2D s_audio;
// then splice kAudioGlslFns after it.
//
// s_audio is the engine's 576x1 RGBA8 audio texture WITH a full box-filtered mip
// chain (see Engine::UploadAudioTex). Per texel: r=spec_L, g=spec_R, b=osc_L, a=osc_R
// (raw 0..255 normalized to 0..1). getspec/getosc read a band average in O(1) by
// sampling the mip LOD whose width matches the requested bandwidth.
//
// Parameters match the Lua getspec/getosc (LuaRuntime):
//   band    normalized bin position 0..1
//   bandw   normalized width 0..1
//   chan    0=center (L+R avg), 1=left, 2=right
// Returns:  getspec -> [0,1]   getosc -> [-1,1]
inline constexpr const char* kAudioGlslFns = R"(
float getspec(float band, float bandw, float chan_f) 
{
    int ch = int(chan_f + 0.5);

    if (ch < 0 || ch > 2) 
        return 0.0;

    int lod = clamp(int(floor(log2(max(1.0, bandw * 576.0)))), 0, 9);
    int lodW = max(1, 576 >> lod);
    int idx = clamp(int(band * float(lodW)), 0, lodW - 1);
    vec4 s = texelFetch(s_audio, ivec2(idx, 0), lod);
    
    if (ch == 0) 
        return (s.r + s.g) * 0.5;

    return ch == 1 ? s.r : s.g;
}

float getosc(float band, float bandw, float chan_f) 
{
    const float C = 128.0 / 255.0;
    int ch = int(chan_f + 0.5);

    if (ch < 0 || ch > 2) 
        return 0.0;

    int lod = clamp(int(floor(log2(max(1.0, bandw * 576.0)))), 0, 9);
    int lodW = max(1, 576 >> lod);
    int idx = clamp(int(band * float(lodW)), 0, lodW - 1);
    vec4 s = texelFetch(s_audio, ivec2(idx, 0), lod);

    if (ch == 0) 
        return (s.b - C) + (s.a - C);

    float ov = ch == 1 ? s.b : s.a;
    return (ov - C) * 2.0;
}
)";
