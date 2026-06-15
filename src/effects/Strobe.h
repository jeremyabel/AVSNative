#pragma once

#include "engine/Effect.h"
#include "engine/ColorList.h"

#include <bgfx/bgfx.h>
#include <cstdint>

// Flashes a flat color over the whole screen. Each strobe advances through a color
// list. Speed sets the frames between strobes (min 2 = a strobe every other frame).
//   - Always On:   each strobe shows the color for `Duration` frames, then passes
//                  the underlying image through until the next strobe (overlay flash).
//   - Keyboard:    while the bound key is held, the color fills the whole interval —
//                  it never clears, just strobes between the colors (Duration unused);
//                  when the key is up, the underlying image passes through.
class Strobe : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    ColorList Colors;
    int      Interval    = 8;   // frames between strobe starts (>= 2)
    int      Duration    = 1;   // frames the color is on per strobe (>= 1)
    int      TriggerMode = 0;   // 0 = Always On, 1 = Keyboard (hold to strobe)
    uint32_t TriggerKey  = 0;   // SDL keycode for Keyboard mode (0 = unbound)

    static constexpr int kMinInterval = 2;   // a strobe every other frame
    static constexpr int kMaxInterval = 60;

    static constexpr const char* kColors      = "colors";
    static constexpr const char* kInterval    = "interval";
    static constexpr const char* kDuration    = "duration";
    static constexpr const char* kTriggerMode = "triggerMode";
    static constexpr const char* kTriggerKey  = "triggerKey";

    void Init() override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    std::string Name() const override { return "Strobe"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_colorUnif  = BGFX_INVALID_HANDLE;

    // Runtime state (not serialized).
    int  m_frame      = 0;        // frame counter (advances only while active)
    int  m_colorIdx   = -1;       // first strobe advances to 0
    int  m_prevPhase  = -1;       // detects strobe-interval boundaries to advance color
    bool m_prevActive = false;    // for restarting the strobe when triggering begins
};
