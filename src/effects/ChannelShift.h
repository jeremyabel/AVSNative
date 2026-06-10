#pragma once

#include "engine/Reflect.h"

struct ChannelShiftConfig
{
    int  Mode         = 0;     // 0=RGB 1=RBG 2=GRB 3=GBR 4=BRG 5=BGR
    bool OnBeatRandom = false;
};

class ChannelShift : public ReflectedEffect<ChannelShiftConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&ChannelShiftConfig::Mode, "mode", "Channel Order",
                    { "RGB (none)", "RBG", "GRB", "GBR", "BRG", "BGR" }),
            Bool(&ChannelShiftConfig::OnBeatRandom, "onBeatRandom", "On Beat Random"),
        };
        return f;
    }
    std::string EffectName() const override { return "Channel Shift"; }

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
