#include "ShaderCompiler.h"

// Suppress warnings from third-party glslang headers
#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <ShaderLang.h>
#include <ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <regex>
#include <string>

// bgfx shader magic bytes: [type, 'S', 'H', BGFX_SHADER_BIN_VERSION=0x0b]
// Type byte: 'F'=0x46 (fragment), 'V'=0x56 (vertex)
static constexpr uint32_t k_FshMagic = 0x0b485346u; // 'F','S','H',0x0b
static constexpr uint32_t k_VshMagic = 0x0b485356u; // 'V','S','H',0x0b

static void WriteU8(std::vector<uint8_t>& Out, uint8_t V)
{
    Out.push_back(V);
}

static void WriteU16(std::vector<uint8_t>& Out, uint16_t V)
{
    Out.push_back(uint8_t(V & 0xFF));
    Out.push_back(uint8_t(V >> 8));
}

static void WriteU32(std::vector<uint8_t>& Out, uint32_t V)
{
    Out.push_back(uint8_t(V & 0xFF));
    Out.push_back(uint8_t((V >> 8)  & 0xFF));
    Out.push_back(uint8_t((V >> 16) & 0xFF));
    Out.push_back(uint8_t(V >> 24));
}

static void WriteBytes(std::vector<uint8_t>& Out, const void* Src, size_t N)
{
    const uint8_t* B = static_cast<const uint8_t*>(Src);
    Out.insert(Out.end(), B, B + N);
}

static void Log(const char* Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);

    char Buf[2048];
    vsnprintf(Buf, sizeof(Buf), Fmt, Args);
    va_end(Args);

    printf("%s\n", Buf);
    fflush(stdout);

    if (FILE* F = fopen("avs_shader.log", "a"))
    {
        fprintf(F, "%s\n", Buf);
        fclose(F);
    }
}

void ShaderCompiler::Init()
{
    glslang::InitializeProcess();
}

void ShaderCompiler::Shutdown()
{
    glslang::FinalizeProcess();
}

// bgfx's Vulkan pipeline layout declares every sampler as TWO separate
// descriptors — a sampled image at `binding` and a sampler at `binding + 16`
// (kSpirvSamplerShift) — because bgfx's own shaderc emits separated
// texture/sampler. Our runtime GLSL writes combined `sampler2D`, producing a
// single COMBINED_IMAGE_SAMPLER descriptor at `binding`. Desktop Vulkan drivers
// tolerate that mismatch, but MoltenVK does not: with Metal argument buffers the
// combined sampler claims two adjacent slots and overlaps the next sampler
// (SPIR-V→MSL conversion returns null); without them the pipeline layout no
// longer matches the shader and a null pipeline is bound (SIGSEGV in submit).
//
// Rewrite each `layout(binding = N) uniform sampler2D NAME;` into bgfx's
// separated form + a macro so call sites (`texture(NAME, …)`,
// `textureSize(NAME, …)`, `bilinearCompat(NAME, …)`) are unchanged:
//     layout(binding = N)    uniform texture2D _NAME_t;
//     layout(binding = N+16) uniform sampler   _NAME_s;
//     #define NAME sampler2D(_NAME_t, _NAME_s)
// Centralised here so every runtime-GLSL effect is fixed in one place.
static std::string SeparateCombinedSamplers(const std::string& Glsl)
{
    static const std::regex kRe(
        R"(layout\s*\(\s*binding\s*=\s*(\d+)\s*\)\s*uniform\s+sampler2D\s+(\w+)\s*;)");

    std::string Out;
    Out.reserve(Glsl.size() + 256);

    auto Begin = std::sregex_iterator(Glsl.begin(), Glsl.end(), kRe);
    auto End   = std::sregex_iterator();
    size_t Last = 0;
    for (auto It = Begin; It != End; ++It)
    {
        const std::smatch& M = *It;
        Out.append(Glsl, Last, (size_t)M.position() - Last);

        const int  binding = std::stoi(M[1].str());
        const std::string name = M[2].str();
        Out += "layout(binding = " + std::to_string(binding) +
               ") uniform texture2D _" + name + "_t;\n";
        Out += "layout(binding = " + std::to_string(binding + 16) +
               ") uniform sampler _" + name + "_s;\n";
        Out += "#define " + name + " sampler2D(_" + name + "_t, _" + name + "_s)";

        Last = (size_t)(M.position() + M.length());
    }
    Out.append(Glsl, Last, std::string::npos);
    return Out;
}

bool ShaderCompiler::GlslToSpirv(const std::string& Glsl, bool IsFragment, std::vector<uint32_t>& OutSpirv, std::string& ErrorOut)
{
    EShLanguage Stage = IsFragment ? EShLangFragment : EShLangVertex;

    const std::string Transformed = SeparateCombinedSamplers(Glsl);

    glslang::TShader Shader(Stage);
    const char* Src = Transformed.c_str();
    Shader.setStrings(&Src, 1);

    Shader.setEnvInput(glslang::EShSourceGlsl, Stage, glslang::EShClientVulkan, 100);
    Shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    Shader.setEnvTarget(glslang::EShTargetSpv,glslang::EShTargetSpv_1_3);

    const TBuiltInResource* Limits = GetDefaultResources();
    if (!Shader.parse(Limits, 450, false, EShMsgDefault))
    {
        ErrorOut = Shader.getInfoLog();
        Log("[ShaderCompiler] GlslToSpirv PARSE ERROR  stage=%s\n%s",
            IsFragment ? "fragment" : "vertex", ErrorOut.c_str());
        return false;
    }

    glslang::TProgram Program;
    Program.addShader(&Shader);
    if (!Program.link(EShMsgDefault))
    {
        ErrorOut = Program.getInfoLog();
        Log("[ShaderCompiler] GlslToSpirv LINK ERROR  stage=%s\n%s",
            IsFragment ? "fragment" : "vertex", ErrorOut.c_str());
        return false;
    }

    glslang::SpvOptions Opts{};
    glslang::GlslangToSpv(*Program.getIntermediate(Stage), OutSpirv, &Opts);

    ErrorOut.clear();
    Log("[ShaderCompiler] GlslToSpirv OK  stage=%s  spirv_words=%zu",
        IsFragment ? "fragment" : "vertex", OutSpirv.size());
    return true;
}

static bgfx::ShaderHandle WrapSpirvImpl(uint32_t Magic, const std::vector<uint32_t>& Spirv, const BgfxUniformDesc* Uniforms, int32_t UniformCount, uint16_t UniformBufSize)
{
    std::vector<uint8_t> Blob;
    Blob.reserve(512 + Spirv.size() * 4);

    WriteU32(Blob, Magic);
    WriteU32(Blob, 0u); // hashIn (unused)
    WriteU32(Blob, 0u); // hashOut (unused)

    WriteU16(Blob, uint16_t(UniformCount));
    for (int UniformIdx = 0; UniformIdx < UniformCount; UniformIdx++)
    {
        const BgfxUniformDesc& U = Uniforms[UniformIdx];
        const uint8_t NameLen = uint8_t(strlen(U.Name));
        WriteU8(Blob, NameLen);
        WriteBytes(Blob, U.Name, NameLen);
        WriteU8(Blob,  U.Type);
        WriteU8(Blob,  U.Num);
        WriteU16(Blob, U.RegIndex);
        WriteU16(Blob, U.RegCount);
        WriteU8(Blob,  U.TexComponent);
        WriteU8(Blob,  U.TexDimension);
        WriteU16(Blob, U.TexFormat);
    }

    const uint32_t SpvBytes = uint32_t(Spirv.size() * sizeof(uint32_t));
    WriteU32(Blob, SpvBytes);
    WriteBytes(Blob, Spirv.data(), SpvBytes);
    WriteU8(Blob, 0u);  // null terminator
    WriteU8(Blob, 0u);  // numAttr (no vertex attribute table)
    WriteU16(Blob, UniformBufSize);

    const bgfx::Memory* Mem = bgfx::copy(Blob.data(), uint32_t(Blob.size()));
    return bgfx::createShader(Mem);
}

bgfx::ShaderHandle ShaderCompiler::WrapFragmentSpirv(const std::vector<uint32_t>& Spirv, const BgfxUniformDesc* Uniforms, int32_t UniformCount, uint16_t UniformBufSize)
{
    return WrapSpirvImpl(k_FshMagic, Spirv, Uniforms, UniformCount, UniformBufSize);
}

bgfx::ShaderHandle ShaderCompiler::WrapVertexSpirv(const std::vector<uint32_t>& Spirv, const BgfxUniformDesc* Uniforms, int32_t UniformCount, uint16_t UniformBufSize)
{
    return WrapSpirvImpl(k_VshMagic, Spirv, Uniforms, UniformCount, UniformBufSize);
}