# Vector Drawing Plan

Most AVS effects (Superscope, DotPlane, Oscilloscope) follow the same pattern:
generate N positions from audio data, draw as connected lines or scattered dots.

## Primitive types needed

- **Lines / waveforms** — connected segments from audio-driven vertex arrays
- **Dots** — scattered point sprites or small circles
- **Filled triangles** — solid geometry for filled shape effects

## Options

### A: Dynamic vertex buffers + bgfx primitive states

Fill a CPU-side vertex array each frame, upload as a dynamic VB, render with:

- `BGFX_STATE_PT_LINES` / `BGFX_STATE_PT_LINESTRIP` for waveforms
- `BGFX_STATE_PT_POINTS` for dot effects
- Default triangles for filled shapes

**AA problem:** Vulkan/D3D12 enforce 1px line width (wide lines are deprecated).
Points are also 1px. Everything is aliased without extra work.

**AA fix:** Expand each line segment CPU-side to a thin quad (2 triangles) and use
a fragment shader that feathers the edges with smoothstep. For dots, same idea —
a billboard quad with a circle SDF in the fragment shader.

This matches how webvs works and how the original AVS plugin operated via
DirectDraw blending.

### B: SDF fragment shaders

Draw a fullscreen quad per shape; the fragment shader computes the signed distance
from each pixel to the primitive and uses smoothstep for free sub-pixel AA.

```glsl
float Dist = length(UV - Center) - Radius;
float Alpha = 1.0 - smoothstep(-1.0/Resolution, 1.0/Resolution, Dist);
```

Built-in AA at any resolution, trivial to implement. Scales poorly when thousands
of independent shapes are needed (one draw call per shape), but ideal for effects
with a small number of large primitives.

### C: NanoVG

A 2D vector graphics library with a bgfx backend available in bgfx's own
examples/common. Handles stroked/filled paths, rounded caps, joins, and text —
all anti-aliased.

Pros: Rich feature set, everything just works.
Cons: Extra dependency, overkill for simple waveform lines, has its own render pass.

## Decision

- **Lines/waveforms**: dynamic VB → expand segments to quads CPU-side → fragment shader AA
- **Dots**: instanced billboard quads with a circle SDF shader, one instance per dot
- **Filled triangles**: plain geometry, MSAA on the FBO if needed
- **NanoVG**: revisit if filled shapes with complex strokes/joins are required
