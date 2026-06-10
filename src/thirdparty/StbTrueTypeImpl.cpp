// Compile the stb_truetype implementation exactly once.
//
// nanovg's fontstash (examples/common/nanovg/fontstash.h) includes <stb/stb_truetype.h>
// with STBTT_DEF=extern and WITHOUT the implementation, expecting the stbtt_* symbols to
// be provided elsewhere at link time. They used to come from bgfx's example imgui backend;
// now that we host our own Dear ImGui, this TU supplies them instead.
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>
