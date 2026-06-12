#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;
struct VisData;
struct GlobalSlider;

// Sandboxed LuaJIT state for one scriptable effect instance.
// All user code blocks run inside a shared env table (the _ENV / global table for chunks).
// Math aliases (sin, cos, etc.), audio functions (getspec, getosc), and a keyboard
// query (key) are pre-seeded.
class LuaRuntime
{
public:
    LuaRuntime();
    ~LuaRuntime();

    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;

    bool IsValid() const { return m_L != nullptr; }

    void SetAudioData(const VisData* data) { m_audioData = data; }

    // Per-frame delta time (seconds), shared by all scripted effects. Engine sets it
    // once per Tick; every Lua block then sees it as the read-only `dt` variable
    // (seeded into env before each block runs, so no per-effect wiring is needed).
    static void   SetFrameDelta(double seconds);
    static double GetFrameDelta();

    // Preset-global named sliders. Engine pushes the current list once per Tick;
    // every Lua block can then read a raw value via slider("name"). Unknown names
    // raise a Lua error. Backed by a shared static store (like SetFrameDelta).
    static void SetSliders(const std::vector<GlobalSlider>& sliders);

    // Stable, unique id of the effect currently rendering. EffectChain sets it
    // before each effect's Render; every Lua block can then read it via getId()
    // (handy as a per-effect random seed: randomseed(getId())). Shared static, so
    // it reflects whichever effect is mid-Render — valid inside all run-time blocks.
    static void SetCurrentEffectId(uint32_t id);

    // Seed a variable into the env table at 0.0 if not already set.
    void SeedVar(const std::string& name);

    // Write a number into the env table (called before running frame/beat blocks).
    void   SetEnvNumber(const std::string& name, double value);
    double GetEnvNumber(const std::string& name) const;

    // Enumerate all env keys that aren't math/audio built-ins.
    // Used to build the persistent-var copy list for the point loop wrapper.
    std::vector<std::string> GetUserVars() const;

    // Compile a code block; stores a LUA_REGISTRYINDEX ref in refOut.
    // Returns false on syntax error (error stored via GetError).
    // Pass refOut = LUA_NOREF (= -1) on first call.
    bool CompileBlock(const std::string& code, const std::string& blockName, int& refOut);

    // Run a compiled block. No-op if ref == LUA_NOREF.
    void RunBlock(int ref, const std::string& blockName);

    // Build the SuperScope point-loop wrapper around user code and compile it.
    // persistentVars: vars read from env at loop entry, written back after.
    // On success, stores a ref to the inner function in refOut.
    bool CompilePointLoop(const std::string& pointCode,
                          const std::vector<std::string>& persistentVars,
                          int& refOut);

    // Call the compiled point-loop function once per frame.
    // audioSamples: float[kAudioBins] values in [0, 255].
    // outBuf:       float[n * 7] — stride: x, y, r, g, b, skip, drawmode.
    void RunPointLoop(int ref, int n, bool isBeat, int width, int height,
                      const float* audioSamples, float* outBuf,
                      const std::string& blockName);

    // Build the Triangle triangle-loop wrapper around user code and compile it.
    // The loop runs N times; per-iteration built-ins (x1..y3, red1..blue3, z1, skip,
    // i) live in the env table, so no persistent-var copy list is needed. w/h/b and
    // the per-frame vertex/colour defaults must be set in env (SetEnvNumber) first.
    bool CompileTriangleLoop(const std::string& triangleCode, int& refOut);

    // Call the compiled triangle-loop function once per frame.
    // outBuf: float[n * 11] — stride: x1,y1,x2,y2,x3,y3,r,g,b,z,skip.
    void RunTriangleLoop(int ref, int n, float* outBuf, const std::string& blockName);

    // Scan Lua init code for bare assignments not in builtins (e.g., "t = 0").
    // Returns unique names suitable for seeding into env and the point-loop wrapper.
    static std::vector<std::string> ScanVarDecls(
        const std::string& code,
        const std::vector<std::string>& builtins);

    const std::string& GetError(const std::string& blockName) const;
    void               ClearError(const std::string& blockName);

private:
    void SetupEnv();
    void SetupMathAliases();
    void SetupAudioFunctions();
    void SetupKeyFunction();
    void SetupSliderFunction();
    void SetupIdFunction();
    void SetupRandFunction();

    static int l_getspec(lua_State* L);
    static int l_getosc(lua_State* L);
    static int l_key(lua_State* L);
    static int l_slider(lua_State* L);
    static int l_getId(lua_State* L);
    static int l_rand(lua_State* L);

    lua_State*     m_L        = nullptr;
    int            m_envRef   = -1;      // LUA_REGISTRYINDEX ref to env table
    const VisData* m_audioData = nullptr;

    static const std::string s_empty;
    mutable std::unordered_map<std::string, std::string> m_errors;
};
