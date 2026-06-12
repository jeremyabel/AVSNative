#pragma once

#include "engine/Effect.h"

#include <array>
#include <bgfx/bgfx.h>

class Convolution : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    bool Wrap     = false;  // mutually exclusive with Absolute (UI enforces)
    bool Absolute = false;
    bool TwoPass  = false;
    int  Bias     = 0;
    int  Scale    = 1;
    // 7x7 kernel. Identity = centre cell.
    std::array<int, 49> Kernel{};

    static constexpr const char* kWrap     = "wrap";
    static constexpr const char* kAbsolute = "absolute";
    static constexpr const char* kTwoPass  = "twoPass";
    static constexpr const char* kBias     = "bias";
    static constexpr const char* kScale    = "scale";
    static constexpr const char* kKernel   = "kernel";

    Convolution() { Kernel[24] = 1; }

    void Init() override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    std::string Name() const override { return "Convolution Filter"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle m_prog        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_kernelUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_params2Unif = BGFX_INVALID_HANDLE;
};
