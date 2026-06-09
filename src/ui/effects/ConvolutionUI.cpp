#include "ui/ConfigUiRegistry.h"
#include "ui/FileDialog.h"

#include "effects/Convolution.h"

#include <imgui/imgui.h>
#include <SDL3/SDL_dialog.h>

#include <cstdint>
#include <fstream>
#include <numeric>
#include <string>

// .cff layout: 55 little-endian int32 (220 bytes):
//   [0]=enabled, [1]=wrap, [2]=absolute, [3]=twoPass, [4..52]=kernel[49], [53]=bias, [54]=scale

static void WriteI32(std::ofstream& f, int32_t v)
{
    uint8_t b[4] = { (uint8_t)(v & 0xff), (uint8_t)((v >> 8) & 0xff),
                     (uint8_t)((v >> 16) & 0xff), (uint8_t)((v >> 24) & 0xff) };
    f.write((const char*)b, 4);
}

static int32_t ReadI32(const uint8_t* p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static const SDL_DialogFileFilter kCffFilters[] = {
    { "Convolution Filter (.cff)", "cff" },
    { "All Files",                 "*"   },
};

static void SaveKernel(const ConvolutionConfig& cfg)
{
    const std::string path = FileDialog::Save("Save Kernel", "kernel.cff", kCffFilters, 2);
    if (path.empty()) return;

    std::ofstream f(path, std::ios::binary);
    if (!f) return;

    WriteI32(f, 1);                       // enabled
    WriteI32(f, cfg.Wrap     ? 1 : 0);
    WriteI32(f, cfg.Absolute ? 1 : 0);
    WriteI32(f, cfg.TwoPass  ? 1 : 0);
    for (int k : cfg.Kernel) WriteI32(f, k);
    WriteI32(f, cfg.Bias);
    WriteI32(f, cfg.Scale);
}

static bool LoadKernel(ConvolutionConfig& cfg)
{
    const std::string path = FileDialog::Open("Load Kernel", kCffFilters, 2);
    if (path.empty()) return false;

    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    uint8_t buf[220];
    f.read((char*)buf, sizeof(buf));
    if (f.gcount() < (std::streamsize)sizeof(buf)) return false;

    int pos = 4;                          // skip enabled
    cfg.Wrap     = ReadI32(buf + pos) != 0; pos += 4;
    cfg.Absolute = ReadI32(buf + pos) != 0; pos += 4;
    cfg.TwoPass  = ReadI32(buf + pos) != 0; pos += 4;
    for (int i = 0; i < 49; ++i) { cfg.Kernel[i] = ReadI32(buf + pos); pos += 4; }
    cfg.Bias  = ReadI32(buf + pos); pos += 4;
    cfg.Scale = ReadI32(buf + pos);
    if (cfg.Scale == 0) cfg.Scale = 1;
    return true;
}

static void DrawConvolutionUI(Effect* effect)
{
    auto* conv = static_cast<Convolution*>(effect);
    ConvolutionConfig& cfg = conv->ConfigRef();

    bool flagsDirty  = false;
    bool kernelDirty = false;

    // Mode flags (wrap/absolute mutually exclusive — enforced in OnConfigChanged)
    if (ImGui::Checkbox("Wrap", &cfg.Wrap))         flagsDirty = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Absolute", &cfg.Absolute)) flagsDirty = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Two Pass", &cfg.TwoPass))  flagsDirty = true;

    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("Bias", &cfg.Bias))   flagsDirty = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("Scale", &cfg.Scale)) flagsDirty = true;

    ImGui::Spacing();
    ImGui::SeparatorText("Kernel (7x7)");

    // 7x7 grid of integer cells.
    const float cellW = 46.0f;
    for (int row = 0; row < 7; ++row)
    {
        for (int col = 0; col < 7; ++col)
        {
            if (col > 0) ImGui::SameLine();
            ImGui::PushID(row * 7 + col);
            ImGui::SetNextItemWidth(cellW);
            if (ImGui::InputInt("##k", &cfg.Kernel[row * 7 + col], 0, 0))
                kernelDirty = true;
            ImGui::PopID();
        }
    }

    ImGui::Spacing();

    if (ImGui::Button("Auto Scale"))
    {
        int sum = std::accumulate(cfg.Kernel.begin(), cfg.Kernel.end(), 0) + cfg.Bias;
        if (cfg.TwoPass) sum *= 2;
        cfg.Scale = (sum == 0) ? 1 : sum;
        flagsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        cfg.Kernel.fill(0);
        cfg.Kernel[24] = 1;
        cfg.Wrap = cfg.Absolute = cfg.TwoPass = false;
        cfg.Bias = 0;
        cfg.Scale = 1;
        flagsDirty = kernelDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save .cff"))
        SaveKernel(cfg);
    ImGui::SameLine();
    if (ImGui::Button("Load .cff"))
    {
        if (LoadKernel(cfg))
            flagsDirty = kernelDirty = true;
    }

    if (flagsDirty || kernelDirty)
        conv->NotifyConfigChanged({ "wrap", "absolute", "twoPass", "bias", "scale", "kernel" });
}

void RegisterConvolutionUI(ConfigUiRegistry& reg)
{
    reg.Register("Convolution Filter", DrawConvolutionUI);
}
