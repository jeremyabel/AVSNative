#pragma once

#include "engine/Effect.h"

#include <string>

class Movement : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::string Coordinates = "polar";  // "polar" or "cartesian"
    std::string Code;
    bool Wrap         = false;
    bool Bilinear     = true;
    bool Compat       = false;  // 8-bit integer bilinear matching win32 (needs Bilinear)
    bool Blend        = false;
    bool SourceMap    = false;
    bool OnBeatToggle = false;

    static constexpr const char* kCoordinates  = "coordinates";
    static constexpr const char* kCode         = "code";
    static constexpr const char* kBilinear     = "bilinear";
    static constexpr const char* kCompat       = "bilinearCompat";
    static constexpr const char* kWrap         = "wrap";
    static constexpr const char* kBlend        = "blend";
    static constexpr const char* kSourceMap    = "sourceMap";
    static constexpr const char* kOnBeatToggle = "onBeatToggle";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Movement"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    const std::string& GetCompileError() const { return CompileError; }

    std::string GetScriptError(const std::string& paramName) const override
    {
        return paramName == kCode ? CompileError : std::string{};
    }

    // Switches coordinate system, swapping the built-in default code if the user
    // hasn't customized it, then recompiles. Used by the bespoke UI.
    void SetCoordinateSystem(const std::string& coord);

    // Recompiles the pull/scatter shaders from Code. Called after Deserialize and
    // by the UI when the code editor changes.
    void Compile();

private:
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
