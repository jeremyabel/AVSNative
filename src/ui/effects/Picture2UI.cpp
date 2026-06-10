#include "ui/ConfigUiRegistry.h"
#include "ui/FileDialog.h"

#include "effects/Picture2.h"

#include <imgui.h>
#include <SDL3/SDL_dialog.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <vector>

// ── Base64 encode ─────────────────────────────────────────────────────────────

static std::string Base64Encode(const uint8_t* data, size_t len, const char* mime)
{
    static const char kChars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out = "data:";
    out += mime;
    out += ";base64,";
    out.reserve(out.size() + (len + 2) / 3 * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = (uint32_t)data[i] << 16;
        if (i + 1 < len) b |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) b |= (uint32_t)data[i + 2];
        out += kChars[(b >> 18) & 63];
        out += kChars[(b >> 12) & 63];
        out += (i + 1 < len) ? kChars[(b >>  6) & 63] : '=';
        out += (i + 2 < len) ? kChars[(b      ) & 63] : '=';
    }
    return out;
}

// ── Blend mode combo helper ───────────────────────────────────────────────────

static const char* kBlendLabels[] = {
    "Replace", "Additive", "Maximum", "50/50",
    "Subtractive 1", "Subtractive 2", "Multiply",
    "Adjustable", "XOR", "Minimum", "Ignore",
};
static constexpr int kNumBlendModes = 11;

static bool BlendCombo(const char* label, int& mode)
{
    const char* preview = (mode >= 0 && mode < kNumBlendModes) ? kBlendLabels[mode] : "?";
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        for (int i = 0; i < kNumBlendModes; ++i) {
            bool sel = (i == mode);
            if (ImGui::Selectable(kBlendLabels[i], sel)) { mode = i; changed = true; }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

// ── Bespoke UI ────────────────────────────────────────────────────────────────

static void DrawPicture2UI(Effect* effect)
{
    auto* pic = static_cast<Picture2*>(effect);
    Picture2Config& cfg = pic->ConfigRef();

    // Image status
    const int imgW = pic->GetImageW();
    const int imgH = pic->GetImageH();
    if (imgW > 0 && imgH > 0)
        ImGui::Text("Image: %d x %d", imgW, imgH);
    else
        ImGui::TextUnformatted("No image loaded");

    static const SDL_DialogFileFilter kImgFilters[] = {
        { "Images",    "png;jpg;jpeg;bmp;gif;tga;psd" },
        { "All Files", "*"                             },
    };

    if (ImGui::Button("Load Image..."))
    {
        const std::string path = FileDialog::Open("Open Image", kImgFilters, 2);
        if (!path.empty())
        {
            const char* mime = "image/png";
            const auto dot = path.rfind('.');
            if (dot != std::string::npos) {
                const std::string ext = path.substr(dot + 1);
                if (ext == "jpg" || ext == "jpeg") mime = "image/jpeg";
                else if (ext == "bmp")             mime = "image/bmp";
                else if (ext == "gif")             mime = "image/gif";
                else if (ext == "tga")             mime = "image/x-tga";
            }
            std::ifstream f(path, std::ios::binary);
            if (f) {
                const std::vector<uint8_t> raw(
                    (std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
                effect->SetConfig({ { "imageData", Base64Encode(raw.data(), raw.size(), mime) } });
            }
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Normal");

    bool dirty = false;

    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    dirty |= BlendCombo("##blend", cfg.BlendMode);

    if (cfg.BlendMode == 7) {
        ImGui::TextUnformatted("Blend Amount");
        ImGui::SetNextItemWidth(-1.0f);
        dirty |= ImGui::SliderInt("##adj", &cfg.AdjustBlend, 0, 255);
    }

    dirty |= ImGui::Checkbox("Bilinear", &cfg.Bilinear);

    ImGui::Spacing();
    ImGui::SeparatorText("On Beat");

    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    dirty |= BlendCombo("##obblend", cfg.OnBeatBlendMode);

    if (cfg.OnBeatBlendMode == 7) {
        ImGui::TextUnformatted("Blend Amount");
        ImGui::SetNextItemWidth(-1.0f);
        dirty |= ImGui::SliderInt("##obadj", &cfg.OnBeatAdjustBlend, 0, 255);
    }

    dirty |= ImGui::Checkbox("On-Beat Bilinear", &cfg.OnBeatBilinear);

    if (dirty)
        pic->NotifyConfigChanged({ "blendMode", "adjustBlend", "bilinear",
                                   "onBeatBlendMode", "onBeatAdjustBlend", "onBeatBilinear" });
}

void RegisterPicture2UI(ConfigUiRegistry& reg)
{
    reg.Register("Picture II", DrawPicture2UI);
}
