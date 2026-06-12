#pragma once

#include "engine/Effect.h"

class ColorFade : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  Faders[3]     = { 8, -8, -8 }; // -32..32, 0 = no change
    int  BeatFaders[3] = { 8, -8, -8 };
    bool Gradual       = false;
    bool RandomBeat    = false;

    static constexpr const char* kFader0     = "fader0";
    static constexpr const char* kFader1     = "fader1";
    static constexpr const char* kFader2     = "fader2";
    static constexpr const char* kBeatFader0 = "beat_fader0";
    static constexpr const char* kBeatFader1 = "beat_fader1";
    static constexpr const char* kBeatFader2 = "beat_fader2";
    static constexpr const char* kGradual    = "gradual";
    static constexpr const char* kRandomBeat = "random_beat";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Colorfade"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Snaps the interpolated fader positions to the current Faders. Called after
    // Deserialize and by the UI when a fader slider changes.
    void ResetFaderPos()
    {
        m_fp[0] = (float)Faders[0];
        m_fp[1] = (float)Faders[1];
        m_fp[2] = (float)Faders[2];
    }

private:
    void UpdateFaderPos(bool isBeat);

    // Runtime state (not serialized)
    float m_fp[3] = { 8.0f, -8.0f, -8.0f }; // interpolated fader positions

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
