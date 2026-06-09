#pragma once

#include "engine/ShaderCompiler.h"   // BgfxUniformDesc

#include <bgfx/bgfx.h>

#include <string>
#include <vector>

class LuaRuntime;

// Bridges user-declared Lua variables into a runtime-compiled GLSL fragment shader.
//
// User "declarations" in Lua are just bare assignments (e.g. `t = 0`); LuaRuntime::
// ScanVarDecls finds them. All discovered vars are packed into a single dynamic array
// uniform `vec4 <prefix>[N]` with N = ceil(vars/4) — one bgfx uniform handle and one
// setUniform call per frame. N is recomputed on every Rescan, so the var count is
// effectively unbounded (capped only by the Vulkan UBO size limit).
//
// Usage by an effect:
//   Init:      bridge.Configure("u_xxx_v");  bridge.Rescan(lua, code, builtins);
//              (build GLSL splicing EmitUboMembers()/EmitLocals(); AppendDescs into the
//               uniform list; bridge.CreateUniforms())
//   Recompile: bridge.Rescan(...); bridge.DestroyUniforms(); <rebuild program>; bridge.CreateUniforms();
//   Render:    <run frame/beat Lua>; bridge.Upload(lua);
//   Destroy:   bridge.DestroyUniforms();
class LuaUniformBridge
{
public:
    void Configure(std::string prefix) { m_prefix = std::move(prefix); }

    // Scan `code` for bare assignments (excluding `builtins`); seed each into the Lua env.
    void Rescan(LuaRuntime& lua, const std::string& code,
                const std::vector<std::string>& builtins);

    const std::vector<std::string>& Vars() const { return m_vars; }
    int      Vec4Count() const { return ((int)m_vars.size() + 3) / 4; }
    uint16_t UboBytes()  const { return (uint16_t)(Vec4Count() * 16); }

    // GLSL: the UBO array member (empty when there are no vars).
    std::string EmitUboMembers() const;
    // GLSL: per-var local unpacking (`float <var> = <prefix>[i/4].<swizzle>;`).
    std::string EmitLocals() const;

    // Append the array uniform's BgfxUniformDesc (nothing when there are no vars).
    // startOffsetBytes is the array's byte offset within the fragment UBO.
    void AppendDescs(std::vector<BgfxUniformDesc>& out, uint16_t startOffsetBytes) const;

    void CreateUniforms();    // single createUniform(prefix, Vec4, N)
    void DestroyUniforms();

    // Read each var from the Lua env, pack, and upload (one setUniform call).
    void Upload(LuaRuntime& lua) const;

private:
    std::string              m_prefix;
    std::vector<std::string> m_vars;
    bgfx::UniformHandle      m_handle = BGFX_INVALID_HANDLE;
};
