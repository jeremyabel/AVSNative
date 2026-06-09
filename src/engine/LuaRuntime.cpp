#include "LuaRuntime.h"
#include "AudioAnalyzer.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <algorithm>
#include <cstdio>
#include <regex>

static const char* k_mathAliases[] = {
    "sin","cos","tan","asin","acos","atan","atan2",
    "sqrt","abs","floor","ceil","pow","log","exp","max","min",nullptr
};

// Reserved env names that ScanVarDecls and GetUserVars must skip.
static const char* k_reservedEnv[] = {
    "sin","cos","tan","asin","acos","atan","atan2","sqrt","abs","floor","ceil",
    "pow","log","exp","max","min","pi","getspec","getosc",nullptr
};

const std::string LuaRuntime::s_empty = {};

// ── Construction ──────────────────────────────────────────────────────────────

LuaRuntime::LuaRuntime()
{
    m_L = luaL_newstate();
    if (!m_L) return;
    luaL_openlibs(m_L);
    SetupEnv();
    SetupMathAliases();
    SetupAudioFunctions();
}

LuaRuntime::~LuaRuntime()
{
    if (m_L)
    {
        if (m_envRef != -1) luaL_unref(m_L, LUA_REGISTRYINDEX, m_envRef);
        lua_close(m_L);
    }
}

// ── Private setup ─────────────────────────────────────────────────────────────

void LuaRuntime::SetupEnv()
{
    lua_newtable(m_L);                               // env

    // Give env an __index fallback to _G so standard globals (require, getfenv,
    // type, print, debug, etc.) are reachable without polluting the env itself.
    // User/effect vars stored in env still shadow any same-named global.
    lua_createtable(m_L, 0, 1);                      // mt
    lua_pushvalue(m_L, LUA_GLOBALSINDEX);            // _G  (Lua 5.1 / LuaJIT)
    lua_setfield(m_L, -2, "__index");                // mt.__index = _G
    lua_setmetatable(m_L, -2);                       // env.mt = mt

    m_envRef = luaL_ref(m_L, LUA_REGISTRYINDEX);
    printf("[Lua] SetupEnv: env ref = %d\n", m_envRef);
}

void LuaRuntime::SetupMathAliases()
{
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_envRef); // [env]
    lua_getglobal(m_L, "math");                     // [env, math]
    for (int i = 0; k_mathAliases[i]; ++i)
    {
        lua_getfield(m_L, -1, k_mathAliases[i]);    // [env, math, fn]
        lua_setfield(m_L, -3, k_mathAliases[i]);    // [env, math]
    }
    lua_pushnumber(m_L, 3.14159265358979323846);
    lua_setfield(m_L, -3, "pi");                    // [env, math]
    lua_pop(m_L, 2);
}

void LuaRuntime::SetupAudioFunctions()
{
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_envRef);

    lua_pushlightuserdata(m_L, this);
    lua_pushcclosure(m_L, l_getspec, 1);
    lua_setfield(m_L, -2, "getspec");

    lua_pushlightuserdata(m_L, this);
    lua_pushcclosure(m_L, l_getosc, 1);
    lua_setfield(m_L, -2, "getosc");

    lua_pop(m_L, 1);
}

int LuaRuntime::l_getspec(lua_State* L)
{
    auto* self = static_cast<LuaRuntime*>(lua_touserdata(L, lua_upvalueindex(1)));
    double band  = luaL_checknumber(L, 1);
    double bandw = luaL_checknumber(L, 2);
    int    chan  = (int)(luaL_optnumber(L, 3, 0.0) + 0.5);

    double result = 0.0;
    const VisData* v = self->m_audioData;
    if (v)
    {
        int start = std::clamp((int)(band * kAudioBins), 0, kAudioBins - 1);
        int end   = std::clamp((int)((band + bandw) * kAudioBins), start + 1, kAudioBins);
        float sum = 0.0f;
        for (int i = start; i < end; ++i)
        {
            if      (chan == 1) sum += v->spec[0][i];
            else if (chan == 2) sum += v->spec[1][i];
            else                sum += (v->spec[0][i] + v->spec[1][i]) * 0.5f;
        }
        result = sum / (end - start) / 255.0;
    }
    lua_pushnumber(L, result);
    return 1;
}

int LuaRuntime::l_getosc(lua_State* L)
{
    auto* self = static_cast<LuaRuntime*>(lua_touserdata(L, lua_upvalueindex(1)));
    double band  = luaL_checknumber(L, 1);
    double bandw = luaL_checknumber(L, 2);
    int    chan  = (int)(luaL_optnumber(L, 3, 0.0) + 0.5);

    double result = 0.0;
    const VisData* v = self->m_audioData;
    if (v)
    {
        int start = std::clamp((int)(band * kAudioBins), 0, kAudioBins - 1);
        int end   = std::clamp((int)((band + bandw) * kAudioBins), start + 1, kAudioBins);
        float sum = 0.0f;
        for (int i = start; i < end; ++i)
        {
            if      (chan == 1) sum += v->osc[0][i];
            else if (chan == 2) sum += v->osc[1][i];
            else                sum += (v->osc[0][i] + v->osc[1][i]) * 0.5f;
        }
        // osc is 0–255, 128 = silence → normalise to [-1, 1]
        result = (sum / (end - start) - 128.0) / 128.0;
    }
    lua_pushnumber(L, result);
    return 1;
}

// ── Env access ────────────────────────────────────────────────────────────────

void LuaRuntime::SeedVar(const std::string& name)
{
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_envRef);
    lua_getfield(m_L, -1, name.c_str());
    const bool alreadySet = !lua_isnil(m_L, -1);
    lua_pop(m_L, 1);
    if (!alreadySet)
    {
        lua_pushnumber(m_L, 0.0);
        lua_setfield(m_L, -2, name.c_str());
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::SetEnvNumber(const std::string& name, double value)
{
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_envRef);
    lua_pushnumber(m_L, value);
    lua_setfield(m_L, -2, name.c_str());
    lua_pop(m_L, 1);
}

double LuaRuntime::GetEnvNumber(const std::string& name) const
{
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_envRef);
    lua_getfield(m_L, -1, name.c_str());
    double val = lua_tonumber(m_L, -1);
    lua_pop(m_L, 2);
    return val;
}

std::vector<std::string> LuaRuntime::GetUserVars() const
{
    std::vector<std::string> vars;
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_envRef);
    lua_pushnil(m_L);
    while (lua_next(m_L, -2))
    {
        if (lua_type(m_L, -2) == LUA_TSTRING)
        {
            const char* key = lua_tostring(m_L, -2);
            bool skip = false;
            for (int i = 0; k_reservedEnv[i]; ++i)
                if (strcmp(key, k_reservedEnv[i]) == 0) { skip = true; break; }
            if (!skip) vars.emplace_back(key);
        }
        lua_pop(m_L, 1);
    }
    lua_pop(m_L, 1);
    return vars;
}

// ── Block compilation & execution ─────────────────────────────────────────────

bool LuaRuntime::CompileBlock(const std::string& code, const std::string& blockName, int& refOut)
{
    if (refOut != -1)
    {
        luaL_unref(m_L, LUA_REGISTRYINDEX, refOut);
        refOut = -1;
    }
    if (code.empty()) { m_errors.erase(blockName); return true; }

    printf("[Lua] Compiling %s (len=%d): %.60s%s\n",
           blockName.c_str(), (int)code.size(), code.c_str(),
           code.size() > 60 ? "..." : "");

    if (luaL_loadbuffer(m_L, code.c_str(), code.size(), blockName.c_str()) != 0)
    {
        m_errors[blockName] = lua_tostring(m_L, -1);
        printf("[Lua] Compile error in %s: %s\n", blockName.c_str(), m_errors[blockName].c_str());
        lua_pop(m_L, 1);
        return false;
    }
    // Bind env as the chunk's environment (Lua 5.1 / LuaJIT).
    // lua_setfenv always pops the table and is unambiguous; lua_setupvalue(,-2,1)
    // was unreliable in the WohlSoft LuaJIT fork (failed to pop, corrupting refs).
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_envRef);
    lua_setfenv(m_L, -2);

    refOut = luaL_ref(m_L, LUA_REGISTRYINDEX);
    m_errors.erase(blockName);
    printf("[Lua] Compiled %s OK (ref=%d, envRef=%d)\n", blockName.c_str(), refOut, m_envRef);
    return true;
}

void LuaRuntime::RunBlock(int ref, const std::string& blockName)
{
    if (ref == -1) return;
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, ref);
    const int tp = lua_type(m_L, -1);
    if (tp != LUA_TFUNCTION)
    {
        printf("[Lua] RunBlock %s: ref=%d holds %s, not a function — skipping\n",
               blockName.c_str(), ref, lua_typename(m_L, tp));
        lua_pop(m_L, 1);
        return;
    }
    if (lua_pcall(m_L, 0, 0, 0) != 0)
    {
        const std::string err = lua_tostring(m_L, -1);
        if (m_errors[blockName] != err)
        {
            m_errors[blockName] = err;
            printf("[Lua] Runtime error in %s: %s\n", blockName.c_str(), err.c_str());
        }
        lua_pop(m_L, 1);
    }
    else
    {
        m_errors.erase(blockName);
    }
}

// ── Point-loop wrapper ────────────────────────────────────────────────────────

bool LuaRuntime::CompilePointLoop(const std::string& pointCode,
                                   const std::vector<std::string>& persistentVars,
                                   int& refOut)
{
    if (refOut != -1)
    {
        luaL_unref(m_L, LUA_REGISTRYINDEX, refOut);
        refOut = -1;
    }
    if (pointCode.empty()) { m_errors.erase("pointCode"); return true; }

    // Build LOCAL_COPIES and WRITEBACK for persistent user vars.
    std::string localCopies, writeback;
    for (const auto& var : persistentVars)
    {
        localCopies += "  local " + var + " = _env." + var + "\n";
        writeback   += "  _env." + var + " = " + var + "\n";
    }

    // The chunk runs with env as _ENV. getfenv(1) returns env so the inner
    // function can read/write persistent vars through _env each call.
    std::string src =
        "local ffi = require('ffi')\n"
        "local cast = ffi.cast\n"
        "local _env = getfenv(1)\n"
        "return function(n, b, w, h, audio_ptr, out_ptr)\n"
        "  local audio = cast('float*', audio_ptr)\n"
        "  local out   = cast('float*', out_ptr)\n"
        // Per-frame state read from env once (colour cycling result + config defaults).
        "  local red      = _env.red\n"
        "  local green    = _env.green\n"
        "  local blue     = _env.blue\n"
        "  local drawmode = _env.drawmode\n"
        + localCopies +
        "  local _n1 = n > 1 and (n - 1) or 1\n"
        "  for _i = 0, n - 1 do\n"
        "    local i    = _i / _n1\n"
        "    local v    = audio[_i] / 128.0 - 1.0\n"
        "    local x    = 0.0\n"
        "    local y    = 0.0\n"
        "    local skip = 0.0\n"
        // User point code inlined here.
        + pointCode + "\n"
        "    local base = _i * 7\n"
        "    out[base]     = x\n"
        "    out[base + 1] = y\n"
        "    out[base + 2] = red\n"
        "    out[base + 3] = green\n"
        "    out[base + 4] = blue\n"
        "    out[base + 5] = skip\n"
        "    out[base + 6] = drawmode\n"
        "  end\n"
        + writeback +
        "end\n";

    printf("[Lua] Compiling pointLoop (%d persistent vars)\n", (int)persistentVars.size());

    if (luaL_loadbuffer(m_L, src.c_str(), src.size(), "pointLoop") != 0)
    {
        m_errors["pointCode"] = lua_tostring(m_L, -1);
        printf("[Lua] pointLoop compile error: %s\n--- source ---\n%s\n---\n",
               m_errors["pointCode"].c_str(), src.c_str());
        lua_pop(m_L, 1);
        return false;
    }
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_envRef);
    lua_setfenv(m_L, -2);

    // Execute the outer chunk; it pushes the inner function onto the stack.
    if (lua_pcall(m_L, 0, 1, 0) != 0)
    {
        m_errors["pointCode"] = lua_tostring(m_L, -1);
        printf("[Lua] pointLoop init error: %s\n", m_errors["pointCode"].c_str());
        lua_pop(m_L, 1);
        return false;
    }

    refOut = luaL_ref(m_L, LUA_REGISTRYINDEX);
    m_errors.erase("pointCode");
    printf("[Lua] pointLoop compiled OK\n");
    return true;
}

void LuaRuntime::RunPointLoop(int ref, int n, bool isBeat, int width, int height,
                               const float* audioSamples, float* outBuf,
                               const std::string& blockName)
{
    if (ref == -1) return;
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, ref);
    const int tp = lua_type(m_L, -1);
    if (tp != LUA_TFUNCTION)
    {
        printf("[Lua] RunPointLoop: ref=%d holds %s, not a function — skipping\n",
               ref, lua_typename(m_L, tp));
        lua_pop(m_L, 1);
        return;
    }
    lua_pushinteger(m_L, n);
    lua_pushnumber(m_L, isBeat ? 1.0 : 0.0);
    lua_pushnumber(m_L, (double)width);
    lua_pushnumber(m_L, (double)height);
    lua_pushlightuserdata(m_L, const_cast<float*>(audioSamples));
    lua_pushlightuserdata(m_L, outBuf);
    if (lua_pcall(m_L, 6, 0, 0) != 0)
    {
        const std::string err = lua_tostring(m_L, -1);
        if (m_errors[blockName] != err)
        {
            m_errors[blockName] = err;
            printf("[Lua] Runtime error in %s: %s\n", blockName.c_str(), err.c_str());
        }
        lua_pop(m_L, 1);
    }
    else
    {
        m_errors.erase(blockName);
    }
}

// ── Var scanning ──────────────────────────────────────────────────────────────

std::vector<std::string> LuaRuntime::ScanVarDecls(
    const std::string& code,
    const std::vector<std::string>& builtins)
{
    // Match bare identifier assignments: "name =" (not ==).
    std::vector<std::string> result;
    std::regex pat(R"(\b([a-zA-Z_]\w*)\s*=(?!=))");
    auto it  = std::sregex_iterator(code.begin(), code.end(), pat);
    auto end = std::sregex_iterator();
    for (; it != end; ++it)
    {
        const std::string name = (*it)[1].str();
        auto inBuiltins = [&]() {
            for (const auto& b : builtins) if (b == name) return true;
            return false;
        };
        auto inResult = [&]() {
            for (const auto& r : result) if (r == name) return true;
            return false;
        };
        if (!inBuiltins() && !inResult()) result.push_back(name);
    }
    return result;
}

// ── Error access ──────────────────────────────────────────────────────────────

const std::string& LuaRuntime::GetError(const std::string& blockName) const
{
    auto it = m_errors.find(blockName);
    return it != m_errors.end() ? it->second : s_empty;
}

void LuaRuntime::ClearError(const std::string& blockName)
{
    m_errors.erase(blockName);
}
