#include "ui/ConfigUiRegistry.h"
#include "ui/FileDialog.h"

#include "effects/Convolution.h"

#include <imgui.h>
#include <SDL3/SDL_dialog.h>

#include <cstdint>
#include <fstream>
#include <numeric>
#include <string>

// .cff layout: 55 little-endian int32 (220 bytes):
//   [0]=enabled, [1]=wrap, [2]=absolute, [3]=twoPass, [4..52]=kernel[49], [53]=bias, [54]=scale

static void WriteI32(std::ofstream& f, int32_t v)
{
    uint8_t b[4] = { (uint8_t)(v & 0xff), (uint8_t)((v >> 8) & 0xff), (uint8_t)((v >> 16) & 0xff), (uint8_t)((v >> 24) & 0xff) };
    f.write((const char*)b, 4);
}

static int32_t ReadI32(const uint8_t* p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static const SDL_DialogFileFilter kCffFilters[] = {
    { "Convolution Filter (.cff)", "cff" },
    { "All Files",                 "*"   },
};

static void SaveKernel(const Convolution& fx)
{
    const std::string path = FileDialog::Save("Save Kernel", "kernel.cff", kCffFilters, 2);
    if (path.empty()) return;

    std::ofstream f(path, std::ios::binary);
    if (!f) return;

    WriteI32(f, 1);                       // enabled
    WriteI32(f, fx.EnableWrap     ? 1 : 0);
    WriteI32(f, fx.EnableAbsolute ? 1 : 0);
    WriteI32(f, fx.EnableTwoPass  ? 1 : 0);
    for (int k : fx.Kernel) WriteI32(f, k);
    WriteI32(f, fx.Bias);
    WriteI32(f, fx.Scale);
}

static bool LoadKernel(Convolution& fx)
{
    const std::string path = FileDialog::Open("Load Kernel", kCffFilters, 2);
    if (path.empty()) return false;

    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    uint8_t buf[220];
    f.read((char*)buf, sizeof(buf));
    if (f.gcount() < (std::streamsize)sizeof(buf)) return false;

    int pos = 4;                          // skip enabled
    fx.EnableWrap     = ReadI32(buf + pos) != 0; pos += 4;
    fx.EnableAbsolute = ReadI32(buf + pos) != 0; pos += 4;
    fx.EnableTwoPass  = ReadI32(buf + pos) != 0; pos += 4;
    for (int i = 0; i < 49; ++i) { fx.Kernel[i] = ReadI32(buf + pos); pos += 4; }
    fx.Bias  = ReadI32(buf + pos); pos += 4;
    fx.Scale = ReadI32(buf + pos);
    if (fx.Scale == 0) fx.Scale = 1;
    return true;
}

static void DrawConvolutionUI(Effect* effect)
{
    auto* conv = static_cast<Convolution*>(effect);

    // Mode flags (wrap/absolute mutually exclusive, matching the reference)
    if (ImGui::Checkbox("Wrap", &conv->EnableWrap) && conv->EnableWrap)
        conv->EnableAbsolute = false;
    ImGui::SameLine();
    if (ImGui::Checkbox("Absolute", &conv->EnableAbsolute) && conv->EnableAbsolute)
        conv->EnableWrap = false;
    ImGui::SameLine();
    ImGui::Checkbox("Two Pass", &conv->EnableTwoPass);

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Bias", &conv->Bias);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Scale", &conv->Scale);

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
            ImGui::InputInt("##k", &conv->Kernel[row * 7 + col], 0, 0);
            ImGui::PopID();
        }
    }

    ImGui::Spacing();

    if (ImGui::Button("Auto Scale"))
    {
        int sum = std::accumulate(conv->Kernel.begin(), conv->Kernel.end(), 0) + conv->Bias;
        if (conv->EnableTwoPass) sum *= 2;
        conv->Scale = (sum == 0) ? 1 : sum;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        conv->Kernel.fill(0);
        conv->Kernel[24] = 1;
        conv->EnableWrap = conv->EnableAbsolute = conv->EnableTwoPass = false;
        conv->Bias = 0;
        conv->Scale = 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save .cff"))
        SaveKernel(*conv);
    ImGui::SameLine();
    if (ImGui::Button("Load .cff"))
        LoadKernel(*conv);
}

void RegisterConvolutionUI(ConfigUiRegistry& reg)
{
    reg.Register("Convolution Filter", DrawConvolutionUI);
}
