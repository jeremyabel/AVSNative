#pragma once

#include <cstdint>

// Opaque streaming GIF decoder built on stb_image's internal frame stepper
// (stbi__gif_load_next). Decodes one frame at a time so callers can lazily
// decode + cache frames instead of paying the up-front cost of decoding the
// whole GIF. Implemented in StbImageImpl.cpp, where stb's static GIF internals
// are visible.

struct GifStream;

// Open a GIF from an in-memory buffer (the bytes are copied and owned by the
// stream). Fills *w/*h from the logical screen descriptor. Returns nullptr if
// the data is not a GIF.
GifStream* GifStreamOpen(const uint8_t* data, int len, int* w, int* h);

// Decode the next frame. On success sets *outRGBA to the frame's w*h*4 RGBA8
// pixels (valid only until the next GifStreamNextFrame/Close call) and
// *outDelayMs to its delay in milliseconds, then returns true. Returns false at
// end-of-stream or on a decode error.
bool GifStreamNextFrame(GifStream* gs, const uint8_t** outRGBA, int* outDelayMs);

// Free the decoder state and the copied bytes.
void GifStreamClose(GifStream* gs);
