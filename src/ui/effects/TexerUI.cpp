#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"
#include "ui/FileDialog.h"

#include "effects/Texer.h"

#include <imgui.h>
#include <SDL3/SDL_dialog.h>

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

// ── Bespoke UI ────────────────────────────────────────────────────────────────

static void DrawTexerUI(Effect* effect)
{
    auto* tex = static_cast<Texer*>(effect);

    const int imgW   = tex->GetImageW();
    const int imgH   = tex->GetImageH();
    const int frames = tex->GetFrameCount();
    if (imgW > 0 && imgH > 0)
    {
        if (frames > 1)
            ImGui::Text("Image: %d x %d  (animated, %d frames)", imgW, imgH, frames);
        else
            ImGui::Text("Image: %d x %d", imgW, imgH);
    }
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
    DrawDefault(effect);
}

void RegisterTexerUI(ConfigUiRegistry& reg)
{
    reg.Register("Texer", DrawTexerUI);
}
