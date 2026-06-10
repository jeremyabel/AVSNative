#pragma once

#include "engine/Reflect.h"

#include <array>
#include <bgfx/bgfx.h>

struct ConvolutionConfig
{
    bool Wrap     = false;
    bool Absolute = false;
    bool TwoPass  = false;
    int  Bias     = 0;
    int  Scale    = 1;
    // 7x7 kernel; managed manually (not reflected). Identity = centre cell.
    std::array<int, 49> Kernel{};

    ConvolutionConfig() { Kernel[24] = 1; }
};

class Convolution : public ReflectedEffect<ConvolutionConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    // Augment reflected JSON with the 49-element kernel array.
    nlohmann::json GetConfig() const override;
    void           SetConfig(const nlohmann::json& cfg) override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> kFields = {
            ::Bool(&ConvolutionConfig::Wrap,     "wrap",     "Wrap"),
            ::Bool(&ConvolutionConfig::Absolute, "absolute", "Absolute"),
            ::Bool(&ConvolutionConfig::TwoPass,  "twoPass",  "Two Pass"),
            NumberI(&ConvolutionConfig::Bias,  "bias",  "Bias",  -100000, 100000),
            NumberI(&ConvolutionConfig::Scale, "scale", "Scale", -100000, 100000),
        };
        return kFields;
    }
    std::string EffectName() const override { return "Convolution Filter"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    bgfx::ProgramHandle m_prog        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_kernelUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_params2Unif = BGFX_INVALID_HANDLE;
};
