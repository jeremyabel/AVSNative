# Plan: LuaJIT CPU Scripting System

## Context

Several AVS effects run user-authored code on the CPU each frame to compute per-frame state (variable values, offsets, blend amounts) that is then either drawn directly or bridged to GLSL uniforms. In AVS_Remake these blocks are plain JavaScript run via `new Function(...)`. In the native C++ port they are replaced by **LuaJIT**, which JIT-compiles Lua to native x86/x64 code.

The runtime GLSL stub pipeline (glslang → SPIR-V) is a separate system — this plan covers only the CPU scripting side.

---

## Affected Effects and Call Frequency

| Effect | Code blocks | Call frequency | Hot? |
|---|---|---|---|
| SuperScope | Init, Frame, Beat, **Point** | Point: 576× per frame | **Yes — hot loop** |
| Dynamic Movement | Init, Frame, Beat | Once per frame/beat | No |
| DDM | Init, Frame, Beat | Once per frame/beat | No |
| Color Modifier | Init, Frame, Beat | Once per frame/beat | No |
| Dynamic Shift | Init, Frame, Beat | Once per frame/beat | No |

SuperScope's Point block is the only hot loop (576 iterations × 60 fps ≈ 35,000 calls/sec). Everything else is per-frame and any scripting language would be fast enough. LuaJIT's JIT compiler makes the Point loop competitive with C.

---

## Architecture

### One `lua_State` per effect instance

Each effect instance gets its own `lua_State`. This gives clean isolation — no cross-effect variable leakage — and the overhead is negligible for a handful of active effects.

```
EffectInstance
  └── lua_State* L
        ├── env table (persistent vars + built-ins)
        ├── initRef   (compiled init chunk, LUA_REGISTRYINDEX ref)
        ├── frameRef  (compiled frame chunk)
        ├── beatRef   (compiled beat chunk)
        └── pointRef  (compiled point chunk — SuperScope only)
```

### Sandboxed environment table

Each `lua_State` has a single environment table `env` that acts as `_ENV` for all user code chunks. It is pre-populated with:

- **Audio functions:** `getspec`, `getosc` — C closures that read the current audio buffer
- **Math aliases:** `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sqrt`, `abs`, `floor`, `ceil`, `pow`, `log`, `exp`, `pi` — pulled from Lua's `math` table into the env directly so user code doesn't need `math.sin(...)`
- **Effect built-ins:** effect-specific variables (`b`, `w`, `h`, `n`, `alpha`, etc.) updated by the engine each frame before running user code
- **User persistent vars:** discovered by `scanVarDecls` (below), pre-seeded to `0`

User code is loaded with `env` as its `_ENV`:
```cpp
luaL_loadbuffer(L, code, len, chunkName);   // pushes compiled chunk
// set env as _ENV upvalue for the chunk
lua_rawgeti(L, LUA_REGISTRYINDEX, envRef);
lua_setupvalue(L, -2, 1);
```

### Variable persistence (`scanVarDecls`)

The same regex used in AVS_Remake — `/\bvar\s+([a-zA-Z_]\w*)/g` — is applied in C++ to the Init code to discover user-declared variable names. Each discovered name is seeded into the env table as `0.0` before Init runs. Because all code blocks share the same `env` table, values written during Init or Frame persist into subsequent Frame, Beat, and Point calls.

```cpp
// In C++, before running Init:
for (const std::string& var : scanVarDecls(initCode, BUILTINS)) {
    lua_pushnumber(L, 0.0);
    lua_setfield(L, envRef, var.c_str());
}
```

---

## Per-frame scripts (Init / Frame / Beat)

Simple `lua_pcall` pattern. The engine updates built-ins in the env table, then calls the compiled chunk:

```cpp
void runBlock(lua_State* L, int chunkRef, const char* blockName) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, chunkRef);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        m_errors[blockName] = lua_tostring(L, -1);
        lua_pop(L, 1);
    }
}

// Each frame, before runBlock("frame"):
setEnvNumber(L, "b",  isBeat ? 1.0 : 0.0);
setEnvNumber(L, "w",  (double)width);
setEnvNumber(L, "h",  (double)height);
runBlock(L, frameRef, "frame");

// Read back any vars that need to go to GLSL uniforms:
double userVar = getEnvNumber(L, "myVar");
bgfx::setUniform(m_uMyVar, &userVar);
```

---

## SuperScope hot loop (Point block)

Calling Lua 576× per frame from C++ would incur C→Lua transition overhead on every call. Instead, the Point code body is wrapped in a Lua-side loop compiled as a single chunk — C++ calls it **once per frame** and passes audio data via LuaJIT's FFI as a C array pointer.

### Wrapper template

The engine builds this Lua source around the user's point code:

```lua
local ffi = require("ffi")
local cast = ffi.cast

return function(n, b, w, h, audio_ptr, out_ptr)
  local audio = cast("float*", audio_ptr)
  local out   = cast("float*", out_ptr)   -- stride 6: x,y,r,g,b,skip
  -- persistent vars copied to locals for fast loop access
  ${LOCAL_COPIES}   -- e.g: local myVar = env_myVar
  for _i = 0, n - 1 do
    local i = _i / (n - 1)
    local v = audio[_i] / 128.0 - 1.0    -- [0,255] → [-1,1]
    local x, y = 0.0, 0.0
    local red, green, blue = 1.0, 1.0, 1.0
    local skip = 0.0
    -- USER POINT CODE START
    ${POINT_CODE}
    -- USER POINT CODE END
    local base = _i * 6
    out[base]     = x
    out[base + 1] = y
    out[base + 2] = red
    out[base + 3] = green
    out[base + 4] = blue
    out[base + 5] = skip
  end
  ${WRITEBACK}   -- e.g: env_myVar = myVar  (for any vars the point code can modify)
end
```

The C++ side holds a pre-allocated `float[576 * 6]` output buffer and passes raw pointers:

```cpp
lua_rawgeti(L, LUA_REGISTRYINDEX, m_pointFuncRef);
lua_pushinteger(L, n);
lua_pushnumber(L, isBeat ? 1.0 : 0.0);
lua_pushnumber(L, width);
lua_pushnumber(L, height);
lua_pushlightuserdata(L, audioSamples);   // float[576], values 0–255
lua_pushlightuserdata(L, m_outBuffer);    // float[576*6]
lua_pcall(L, 6, 0, 0);
// draw from m_outBuffer
```

LuaJIT traces and JIT-compiles the inner loop to native code; the FFI array accesses become direct memory reads/writes with no Lua boxing overhead.

### Persistent vars in the hot loop

Variables declared in Init and updated by Frame/Beat that the user also reads in Point code are copied to Lua locals at loop entry (the `${LOCAL_COPIES}` block) and written back after the loop (the `${WRITEBACK}` block). The engine regenerates the wrapper template whenever Init code changes (which triggers `scanVarDecls`).

---

## Compilation and live editing

### Compiling a block

```cpp
bool compileBlock(lua_State* L, const std::string& code,
                  const std::string& name, int& refOut) {
    std::string wrapped = wrapCode(code, name);   // adds env _ENV setup
    if (luaL_loadbuffer(L, wrapped.c_str(), wrapped.size(), name.c_str())) {
        m_errors[name] = lua_tostring(L, -1);
        lua_pop(L, 1);
        return false;
    }
    // bind env table as _ENV upvalue
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_envRef);
    lua_setupvalue(L, -2, 1);
    if (refOut != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, refOut);
    refOut = luaL_ref(L, LUA_REGISTRYINDEX);
    m_errors[name] = "";
    return true;
}
```

- Syntax errors caught immediately at `luaL_loadbuffer` — message contains line number relative to the wrapped source.
- Line offset (number of lines added by the wrapper prefix) is subtracted before displaying to the user, so errors reference the user's code lines.
- Runtime errors caught by `lua_pcall` error handler, stored per block name.
- On init code change: `scanVarDecls` re-runs, new user vars seeded, all blocks recompiled (because the point wrapper's `${LOCAL_COPIES}` block changes).
- On frame/beat/point code change: only that block recompiles.
- A **300ms debounce** on the textarea prevents compile-on-every-keystroke.

---

## Built-in function implementations

### `getspec` and `getosc`

Registered as C closures with the audio buffer pointer as an upvalue. Signature matches the AVS_Remake GLSL version (band 0–1, bandw 0–1, chan 0/1/2):

```cpp
static int l_getspec(lua_State* L) {
    AudioData* audio = (AudioData*)lua_touserdata(L, lua_upvalueindex(1));
    double band  = luaL_checknumber(L, 1);
    double bandw = luaL_checknumber(L, 2);
    int    chan  = (int)(luaL_optnumber(L, 3, 0.0) + 0.5);
    lua_pushnumber(L, audio->getspec(band, bandw, chan));
    return 1;
}
```

### Math aliases

```cpp
// Pull math.sin etc. directly into env so user writes sin(...) not math.sin(...)
const char* mathAliases[] = {
    "sin","cos","tan","asin","acos","atan","atan2",
    "sqrt","abs","floor","ceil","pow","log","exp","max","min","pi",nullptr
};
lua_getglobal(L, "math");
for (int i = 0; mathAliases[i]; i++) {
    lua_getfield(L, -1, mathAliases[i]);
    lua_setfield(L, envRef, mathAliases[i]);
}
```

---

## Lua syntax differences from JS (migration note)

User scripts from AVS_Remake (JS) need minor translation for Lua. Key differences:

| JS | Lua |
|---|---|
| `var x = 0` | just `x = 0` (global in env) |
| `Math.sin(x)` | `sin(x)` (aliased in env) |
| `if (cond) { }` | `if cond then end` |
| `&&` / `\|\|` / `!` | `and` / `or` / `not` |
| `cond ? a : b` | `cond and a or b` *(only safe if `a` is not false/nil)* |
| `//` floor divide | `math.floor(a/b)` |
| `%` modulo | `%` (same) |

A short migration guide should be included in the UI (tooltip or help panel next to the script textarea).

---

## Verification plan

1. **Init/Frame/Beat:** Implement Color Modifier with LuaJIT blocks. Confirm `b` (beat) drives a user variable that modulates the GLSL uniform correctly at 60fps.
2. **Variable persistence:** Declare `var counter = 0` in Init, increment in Frame (`counter = counter + 1`), display in Dynamic Shift offset. Confirm counter increments each frame and survives beat events.
3. **SuperScope hot loop:** Port SuperScope. Confirm 576 points drawn per frame, `i` and `v` correct per point, `skip` works. Profile the point loop: should be well under 1ms at 576 points on any modern CPU.
4. **Persistent vars in hot loop:** Declare a var in SuperScope Init, modify in Frame, read in Point — confirm wrapper template correctly copies it to a local and the loop sees the Frame-updated value.
5. **Live edit + error feedback:** Change Point code to invalid Lua syntax; confirm error displayed with correct line number (relative to user's code, not the wrapper). Fix it; confirm immediate recompile and correct rendering.
6. **`getspec`/`getosc`:** Write a SuperScope Point block that calls `getspec(i, 0.1, 0)` to drive `y`. Confirm audio-reactive output matches the JS version's behaviour.
