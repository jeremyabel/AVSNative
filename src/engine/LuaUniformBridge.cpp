#include "engine/LuaUniformBridge.h"
#include "engine/LuaRuntime.h"

void LuaUniformBridge::Rescan(LuaRuntime& lua, const std::string& code,
                              const std::vector<std::string>& builtins)
{
    m_vars = LuaRuntime::ScanVarDecls(code, builtins);
    for (const auto& v : m_vars)
        lua.SeedVar(v);
}

std::string LuaUniformBridge::EmitUboMembers() const
{
    const int n = Vec4Count();
    if (n == 0) return {};
    return "    vec4 " + m_prefix + "[" + std::to_string(n) + "];\n";
}

std::string LuaUniformBridge::EmitLocals() const
{
    static const char* k_swizzle[] = { ".x", ".y", ".z", ".w" };
    std::string s;
    for (int i = 0; i < (int)m_vars.size(); i++)
        s += "    float " + m_vars[i] + " = " + m_prefix
           + "[" + std::to_string(i / 4) + "]" + k_swizzle[i % 4] + ";\n";
    return s;
}

void LuaUniformBridge::AppendDescs(std::vector<BgfxUniformDesc>& out,
                                   uint16_t startOffsetBytes) const
{
    const int n = Vec4Count();
    if (n == 0) return;
    // Vec4 fragment array: Num = RegCount = N; RegIndex = byte offset in the UBO.
    out.push_back({ m_prefix.c_str(), 0x12, (uint8_t)n, startOffsetBytes, (uint16_t)n, 0, 0, 0 });
}

void LuaUniformBridge::CreateUniforms()
{
    const int n = Vec4Count();
    if (n == 0) { m_handle = BGFX_INVALID_HANDLE; return; }
    m_handle = bgfx::createUniform(m_prefix.c_str(), bgfx::UniformType::Vec4, (uint16_t)n);
}

void LuaUniformBridge::DestroyUniforms()
{
    if (bgfx::isValid(m_handle))
        bgfx::destroy(m_handle);
    m_handle = BGFX_INVALID_HANDLE;
}

void LuaUniformBridge::Upload(LuaRuntime& lua) const
{
    const int n = Vec4Count();
    if (n == 0 || !bgfx::isValid(m_handle)) return;

    // Flat layout: var i maps to vec4 (i/4) component (i%4) -> flat index i.
    std::vector<float> data((size_t)n * 4, 0.0f);
    for (int i = 0; i < (int)m_vars.size(); i++)
        data[i] = (float)lua.GetEnvNumber(m_vars[i]);

    bgfx::setUniform(m_handle, data.data(), (uint16_t)n);
}
