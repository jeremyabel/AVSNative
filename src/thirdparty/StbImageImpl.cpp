// Compile stb_image implementation exactly once for the avs_engine target.
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// ── Streaming GIF decoder ─────────────────────────────────────────────────────
// Built on stb's internal frame stepper (stbi__gif_load_next + stbi__gif /
// stbi__context), which are `static` and therefore only visible here, in the TU
// that defines STB_IMAGE_IMPLEMENTATION. See StbGifStream.h for the API contract.

#include "StbGifStream.h"

#include <cstring>
#include <vector>

struct GifStream
{
    std::vector<unsigned char> bytes;     // owned copy of the GIF data
    stbi__context              ctx;       // reads from `bytes`
    stbi__gif                  g;         // persistent decoder state (zeroed once)
    int                        w = 0, h = 0;
    int                        emitted = 0;
    // Copies of the last two emitted frames — stbi__gif_load_next needs frame
    // N-2's pixels (`two_back`) to honour GIF disposal mode 3. ring[i % 2] holds
    // frame i, so when decoding frame i the slot still contains frame i-2.
    std::vector<unsigned char> ring[2];
};

GifStream* GifStreamOpen(const uint8_t* data, int len, int* w, int* h)
{
    if (!data || len < 10) return nullptr;

    GifStream* gs = new GifStream();
    gs->bytes.assign(data, data + len);
    stbi__start_mem(&gs->ctx, gs->bytes.data(), (int)gs->bytes.size());

    if (!stbi__gif_test(&gs->ctx))   // also rewinds the context
    {
        delete gs;
        return nullptr;
    }

    std::memset(&gs->g, 0, sizeof(gs->g));

    // Canvas size = logical screen descriptor (offset 6/8, little-endian), which
    // is exactly what stbi__gif_header reads into g.w/g.h.
    gs->w = data[6] | (data[7] << 8);
    gs->h = data[8] | (data[9] << 8);
    if (w) *w = gs->w;
    if (h) *h = gs->h;
    return gs;
}

bool GifStreamNextFrame(GifStream* gs, const uint8_t** outRGBA, int* outDelayMs)
{
    if (!gs) return false;

    unsigned char* two_back = nullptr;
    if (gs->emitted >= 2)
    {
        std::vector<unsigned char>& slot = gs->ring[gs->emitted % 2];
        if (!slot.empty()) two_back = slot.data();
    }

    int comp = 4;
    stbi_uc* u = stbi__gif_load_next(&gs->ctx, &gs->g, &comp, 4, two_back);
    if (u == (stbi_uc*)&gs->ctx) return false;  // end-of-stream marker
    if (u == nullptr)            return false;  // decode error

    gs->w = gs->g.w;
    gs->h = gs->g.h;
    const size_t frameBytes = (size_t)gs->g.w * gs->g.h * 4;

    // Retain a copy for use as `two_back` two frames from now (overwrites the
    // frame-(emitted-2) copy that two_back referenced above — already consumed).
    gs->ring[gs->emitted % 2].assign(u, u + frameBytes);
    gs->emitted++;

    if (outRGBA)    *outRGBA = u;            // live canvas, valid until next call
    if (outDelayMs) *outDelayMs = gs->g.delay;
    return true;
}

void GifStreamClose(GifStream* gs)
{
    if (!gs) return;
    STBI_FREE(gs->g.out);
    STBI_FREE(gs->g.history);
    STBI_FREE(gs->g.background);
    delete gs;
}
