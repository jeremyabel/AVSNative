#include "effects/Strobe.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"
#include "engine/KeyInput.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_strobe.sc.bin.h"

#include <algorithm>

void Strobe::Init()
{
    const bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_strobe_spv,     sizeof(fs_strobe_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif = bgfx::createUniform("s_input",       bgfx::UniformType::Sampler);
    m_colorUnif = bgfx::createUniform("u_strobeColor", bgfx::UniformType::Vec4);
}

void Strobe::Render(const RenderContext& Ctx)
{
    const int interval = std::max(kMinInterval, Interval);
    const int duration = std::max(1, Duration);
    const int n        = (int)Colors.size();

    // Active = should we be strobing this frame at all?
    const bool keyboard = (TriggerMode == 1);
    const bool active   = !keyboard ||
                          (TriggerKey != 0 && avs::KeyInput::IsKeyDown(TriggerKey));

    // Restart the strobe (phase + palette) whenever triggering begins, so a key
    // press flashes immediately from the first color.
    if (active && !m_prevActive)
    {
        m_frame     = 0;
        m_colorIdx  = -1;
        m_prevPhase = -1;
    }
    m_prevActive = active;

    bool colorOn = false;
    if (active && n > 0)
    {
        const int phase = m_frame % interval;

        // Advance to the next color at each strobe-interval boundary (not on an
        // on/off edge — while held there is no off phase to ride).
        if (phase == 0 && m_prevPhase != 0)
            m_colorIdx = (m_colorIdx + 1) % n;
        m_prevPhase = phase;

        // Keyboard (hold): the color fills the whole interval — never clears, just
        // strobes between colors. Always On: a Duration-frame flash, then passthrough.
        colorOn = keyboard || (phase < duration);

        ++m_frame;
    }

    float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };   // w=0 → shader passes input through
    if (colorOn)
    {
        const std::array<uint8_t,3>& c = Colors[std::clamp(m_colorIdx, 0, n - 1)];
        color[0] = c[0] / 255.0f;
        color[1] = c[1] / 255.0f;
        color[2] = c[2] / 255.0f;
        color[3] = 1.0f;   // on
    }

    bgfx::setTexture(0, m_inputUnif, Ctx.InputTexture);
    bgfx::setUniform(m_colorUnif, color);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Ctx.QuadVB);
    bgfx::submit(Ctx.ViewId, m_prog);

    Ctx.FboManager->Swap();
}

nlohmann::json Strobe::Serialize() const
{
    return {
        { kColors,      JsonUtil::ColorsToJson(Colors) },
        { kInterval,    Interval    },
        { kDuration,    Duration    },
        { kTriggerMode, TriggerMode },
        { kTriggerKey,  TriggerKey  },
    };
}

void Strobe::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColors(j, kColors,      Colors);
    JsonUtil::ReadInt   (j, kInterval,    Interval);
    JsonUtil::ReadInt   (j, kDuration,    Duration);
    JsonUtil::ReadInt   (j, kTriggerMode, TriggerMode);

    if (auto it = j.find(kTriggerKey); it != j.end() && it->is_number_unsigned())
        TriggerKey = it->get<uint32_t>();
}

void Strobe::Destroy()
{
    if (bgfx::isValid(m_colorUnif)) bgfx::destroy(m_colorUnif);
    if (bgfx::isValid(m_inputUnif)) bgfx::destroy(m_inputUnif);
    if (bgfx::isValid(m_prog))      bgfx::destroy(m_prog);

    m_colorUnif = BGFX_INVALID_HANDLE;
    m_inputUnif = BGFX_INVALID_HANDLE;
    m_prog      = BGFX_INVALID_HANDLE;
}
