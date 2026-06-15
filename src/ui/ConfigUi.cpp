#include "ui/ConfigUi.h"
#include "ui/FileDialog.h"

#include "engine/ColorList.h"
#include "engine/Effect.h"
#include "engine/KeyInput.h"
#include "engine/KeyedImageList.h"

#include <imgui.h>
#include <TextEditor.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_keyboard.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
struct EditorState
{
    TextEditor  Editor;
    std::string LastText; // last text we synced with the config field
    bool        Seen = false; // drawn this frame? (for EndFramePrune)
};

std::unordered_map<std::string, EditorState> s_editors;
std::string                                  s_scope; // current effect scope (see SetEditorScope)
} // namespace

namespace ConfigUi
{
void SetEditorScope(const void* effect)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%p|", effect);
    s_scope = buf;
}

bool CodeEditor(const char* id, std::string& text, Lang lang, float height)
{
    // Namespace the persistent editor by the current effect scope, but keep the
    // bare `id` as the ImGui widget id (ImGui ids are window-scoped, so two panels
    // showing different same-type effects don't clash).
    const std::string key = s_scope + id;

    auto it = s_editors.find(key);
    if (it == s_editors.end())
    {
        EditorState& st = s_editors[key];
        st.Editor.SetLanguage(lang == Lang::Glsl ? TextEditor::Language::Glsl()
                                                 : TextEditor::Language::Lua());
        st.Editor.SetText(text);
        st.LastText = text;
        it = s_editors.find(key);
    }
    else if (text != it->second.LastText)
    {
        // Config field changed out from under us (e.g. programmatic default swap).
        it->second.Editor.SetText(text);
        it->second.LastText = text;
    }

    EditorState& st = it->second;
    st.Seen = true;
    st.Editor.Render(id, ImVec2(-1.0f, height));

    std::string newText = st.Editor.GetText();
    if (newText != st.LastText)
    {
        st.LastText = newText;
        text        = newText;
        return true;
    }
    return false;
}

void ScriptError(Effect* effect, const char* paramName)
{
    if (std::string err = effect->GetScriptError(paramName); !err.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", err.c_str());
}

void EndFramePrune()
{
    for (auto it = s_editors.begin(); it != s_editors.end(); )
    {
        if (!it->second.Seen)
            it = s_editors.erase(it);
        else
        {
            it->second.Seen = false;
            ++it;
        }
    }
}

void ResetEditors()
{
    s_editors.clear();
}

bool ColorEdit(const char* label, std::array<uint8_t, 3>& c)
{
    float col[3] = { c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f };
    if (ImGui::ColorEdit3(label, col))
    {
        c[0] = (uint8_t)(col[0] * 255.0f + 0.5f);
        c[1] = (uint8_t)(col[1] * 255.0f + 0.5f);
        c[2] = (uint8_t)(col[2] * 255.0f + 0.5f);
        return true;
    }
    return false;
}

bool ColorsEdit(const char* id, ColorList& list)
{
    auto& colors = list.Entries;
    bool changed = false;
    ImGui::PushID(id);
    for (int i = 0; i < (int)colors.size(); ++i)
    {
        ImGui::PushID(i);
        float col[3] = { colors[i][0] / 255.0f, colors[i][1] / 255.0f, colors[i][2] / 255.0f };
        if (ImGui::ColorEdit3("##c", col,
                              ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs))
        {
            colors[i] = { (uint8_t)(col[0] * 255.0f + 0.5f),
                          (uint8_t)(col[1] * 255.0f + 0.5f),
                          (uint8_t)(col[2] * 255.0f + 0.5f) };
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x") && colors.size() > 1)
        {
            colors.erase(colors.begin() + i);
            changed = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("+"))
    {
        colors.push_back({ 255, 255, 255 });
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool PickImageInto(Effect* effect, const char* configKey)
{
    static const SDL_DialogFileFilter kImgFilters[] = {
        { "Images",    "png;jpg;jpeg;bmp;gif;tga;psd" },
        { "All Files", "*"                             },
    };

    const std::string path = FileDialog::Open("Open Image", kImgFilters, 2);
    if (path.empty())
        return false;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    if (raw.empty())
        return false;

    const size_t sep = path.find_last_of("/\\");
    const std::string name = (sep == std::string::npos) ? path : path.substr(sep + 1);

    effect->ApplyAsset(configKey, name, std::move(raw));
    return true;
}

bool KeyCaptureButton(const char* id, const void* owner, int index, uint32_t& keycode)
{
    // Only one button is armed at a time, bound to a specific (owner, index) so it
    // can never write a different binding.
    static const void* s_owner = nullptr;
    static int         s_index = -1;

    bool changed = false;
    const bool capturing = (s_owner == owner && s_index == index);

    if (capturing)
    {
        if (const uint32_t k = avs::KeyInput::LastKeyPressed(); k != 0)
        {
            keycode  = k;
            s_owner  = nullptr;
            s_index  = -1;
            changed  = true;
        }
    }

    std::string name;
    const char* label = "(unset)";
    if (capturing)
        label = "press a key...";
    else if (keycode != 0)
    {
        name  = SDL_GetKeyName((SDL_Keycode)keycode);
        label = name.empty() ? "(unknown)" : name.c_str();
    }

    char btn[96];
    std::snprintf(btn, sizeof(btn), "Key: %s###%s", label, id);
    if (ImGui::Button(btn))
    {
        s_owner = owner;
        s_index = index;
    }
    // Right-click clears the binding.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && keycode != 0)
    {
        keycode = 0;
        if (capturing) { s_owner = nullptr; s_index = -1; }
        changed = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click to bind a key; right-click to clear.");

    return changed;
}

bool KeyedImageListEditor(Effect* effect, KeyedImageList& list)
{
    // Press-to-capture target. Only one capture is armed at a time (you press one
    // key at a time), but it's bound to a specific list pointer + index so it can
    // never write to another effect instance's entry.
    static const void* s_capturingList  = nullptr;
    static int         s_capturingIndex = -1;

    bool reload = false;
    ImGui::PushID(&list);

    // Columns: selection radio | key bind | load | filename | remove. A table keeps
    // them aligned across rows regardless of differing key-name / filename widths.
    if (ImGui::BeginTable("##keyedimages", 5,
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody))
    {
        ImGui::TableSetupColumn("##sel",  ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##key",  ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##load", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##rm",   ImGuiTableColumnFlags_WidthFixed);

        for (int i = 0; i < (int)list.Images.size(); ++i)
        {
            ImGui::PushID(i);
            KeyedImage& img = list.Images[i];
            ImGui::TableNextRow();

            // Selection radio — picking an entry switches the displayed image.
            ImGui::TableNextColumn();
            if (ImGui::RadioButton("##sel", list.Selected == i) && list.Selected != i)
            {
                list.Selected = i;
                reload = true;
            }

            // Press-to-capture key binding.
            ImGui::TableNextColumn();
            const bool capturing = (s_capturingList == &list && s_capturingIndex == i);
            if (capturing)
            {
                if (const uint32_t k = avs::KeyInput::LastKeyPressed(); k != 0)
                {
                    img.Keycode      = k;
                    s_capturingList  = nullptr;
                    s_capturingIndex = -1;
                }
            }

            std::string keyName;
            const char* keyLabel = "(unset)";
            if (capturing)
                keyLabel = "press a key...";
            else if (img.Keycode != 0)
            {
                keyName  = SDL_GetKeyName((SDL_Keycode)img.Keycode);
                keyLabel = keyName.empty() ? "(unknown)" : keyName.c_str();
            }

            char btn[80];
            std::snprintf(btn, sizeof(btn), "Key: %s###setkey", keyLabel);
            if (ImGui::Button(btn))
            {
                s_capturingList  = &list;
                s_capturingIndex = i;
            }

            // Load.
            ImGui::TableNextColumn();
            if (ImGui::Button("Load..."))
            {
                const std::string key = "image" + std::to_string(i);
                if (PickImageInto(effect, key.c_str()) && list.Selected == i)
                    reload = true;
            }

            // Filename.
            ImGui::TableNextColumn();
            if (img.Name.empty())
                ImGui::TextDisabled("(no image)");
            else
                ImGui::TextUnformatted(img.Name.c_str());

            // Remove.
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x"))
            {
                list.Images.erase(list.Images.begin() + i);
                if (s_capturingList == &list) { s_capturingList = nullptr; s_capturingIndex = -1; }
                list.ClampSelected();
                reload = true;
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (ImGui::Button("Add image"))
    {
        list.Images.push_back(KeyedImage{});
        if (list.Images.size() == 1)  // first entry becomes the selection
            reload = true;
    }

    ImGui::PopID();
    return reload;
}
} // namespace ConfigUi
