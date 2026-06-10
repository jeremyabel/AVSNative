#pragma once

#include "engine/Reflect.h"

struct MirrorConfig
{
    bool FlipX  = true;
    bool FlipY  = false;
    bool OnBeat = false;
};

class Mirror : public ReflectedEffect<MirrorConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Bool(&MirrorConfig::FlipX, "flipX", "Flip Horizontal"),
            Bool(&MirrorConfig::FlipY, "flipY", "Flip Vertical"),
            Bool(&MirrorConfig::OnBeat, "onBeat", "Toggle on Beat"),
        };
        return f;
    }
    std::string EffectName() const override { return "Mirror"; }

private:
    bool m_beatActive = false; // runtime toggle state, not serialized

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
