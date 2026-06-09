#pragma once

#include "engine/Reflect.h"

#include <string>

struct MovementConfig
{
    std::string Coordinates = "polar";
    std::string Code;
    bool Wrap         = false;
    bool Bilinear     = true;
    bool Blend        = false;
    bool SourceMap    = false;
    bool OnBeatToggle = false;
};

class Movement : public ReflectedEffect<MovementConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    const std::string& GetCompileError() const { return CompileError; }

    std::string GetScriptError(const std::string& paramName) const override
    {
        return paramName == "code" ? CompileError : std::string{};
    }

    // Switches coordinate system, swapping the built-in default code if the user
    // hasn't customized it, then recompiles. Used by the bespoke UI.
    void SetCoordinateSystem(const std::string& coord);

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectS(&MovementConfig::Coordinates, "coordinates", "Coordinates", { "polar", "cartesian" }),
            Glsl(&MovementConfig::Code, "code", "GLSL Code"),
            Bool(&MovementConfig::Bilinear, "bilinear", "Bilinear"),
            Bool(&MovementConfig::Wrap, "wrap", "Wrap"),
            Bool(&MovementConfig::Blend, "blend", "Blend (50/50)"),
            Bool(&MovementConfig::SourceMap, "sourceMap", "Source Map"),
            Bool(&MovementConfig::OnBeatToggle, "onBeatToggle", "On-Beat Toggle"),
        };
        return f;
    }
    std::string EffectName() const override { return "Movement"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override
    {
        for (const std::string& k : changed)
            if (k == "code" || k == "coordinates")
            {
                Compile();
                break;
            }
    }

private:
    void Compile();
    std::string BuildPullFragGlsl() const;
    std::string BuildScatterVertGlsl() const;
    void RenderPull(const RenderContext& Context);
    void RenderScatter(const RenderContext& Context);

    std::string CompileError;

    // Pull mode
    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;

    // Scatter (source map) mode
    bgfx::ProgramHandle ScatterProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ScatterParamsUniform = BGFX_INVALID_HANDLE;
};
