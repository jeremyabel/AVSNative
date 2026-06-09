#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include <bgfx/bgfx.h>

// Uniform descriptor for constructing the bgfx shader binary header.
// Matches the layout that bgfx's shaderc writes to the .bin file.
struct BgfxUniformDesc
{
    const char* Name;
    uint8_t Type;       // UniformType | kUniformFragmentBit | kUniformSamplerBit
    uint8_t Num;        // element count (1 for scalars, 0 for samplers)
    uint16_t RegIndex;   // Vec4: byte offset in UBO | Sampler: Vulkan binding index
    uint16_t RegCount;   // Vec4: vec4 count | Sampler: 0 (unused)
    uint8_t TexComponent;
    uint8_t TexDimension;
    uint16_t TexFormat;
};

// bgfx Vulkan binding constants (from bgfx/src/shader.h)
// Vertex UBO sits at binding kSpirvVertexBinding.
// Fragment UBO sits at binding kSpirvFragmentBinding.
// Sampler at stage N sits at binding N + kSpirvBindShift.
static constexpr uint8_t kSpirvVertexBinding = 0;
static constexpr uint8_t kSpirvFragmentBinding = 1;
static constexpr uint8_t kSpirvBindShift = 2;

// bgfx uniform type bits (from bgfx/src/bgfx_p.h)
static constexpr uint8_t kBgfxSamplerType = 0;   // UniformType::Sampler
static constexpr uint8_t kBgfxVec4Type = 2;   // UniformType::Vec4
static constexpr uint8_t kBgfxFragmentBit = 0x10;
static constexpr uint8_t kBgfxSamplerBit = 0x20;

// Standard texFmt for a 2D RGBA8 sampler (matches bgfx's shaderc output)
static constexpr uint16_t kTexFmt2DSampler = 2;

class ShaderCompiler
{
public:

    // Call once at engine startup / shutdown.
    static void Init();
    static void Shutdown();

    // Compile GLSL 450 source to SPIRV words.
    // IsFragment: true for fragment shader, false for vertex.
    // Returns false and sets ErrorOut on failure.
    static bool GlslToSpirv(const std::string& Glsl, bool IsFragment, std::vector<uint32_t>& OutSpirv, std::string& ErrorOut);
    
    // Wrap compiled SPIRV in bgfx's shader binary format and create a ShaderHandle.
    // Uniforms: array of BgfxUniformDesc describing every uniform/sampler.
    // UniformBufSize: total UBO size in bytes (sum of Vec4 regCount*16).
    static bgfx::ShaderHandle WrapFragmentSpirv(const std::vector<uint32_t>& Spirv, const BgfxUniformDesc* Uniforms, int32_t UniformCount, uint16_t UniformBufSize);

    static bgfx::ShaderHandle WrapVertexSpirv(const std::vector<uint32_t>& Spirv, const BgfxUniformDesc* Uniforms, int32_t UniformCount, uint16_t UniformBufSize);
};
