#pragma once

#include "engine/Effect.h"

class ColorFade : public Effect
{
public:

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
        InterpFaders[0] = (float)Faders[0];
        InterpFaders[1] = (float)Faders[1];
        InterpFaders[2] = (float)Faders[2];
    }

public:

    int Faders[3] = { 0, 0, 0 };
    int BeatFaders[3] = { 0, 0, 0 };
    bool EnableOnBeatChange = false;
    bool EnableRandomBeat = false;

private:

    void UpdateFaderPos(bool IsBeat);

    float InterpFaders[3] = { 0.0f, 0.0f, 0.0f }; // interpolated fader positions

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
