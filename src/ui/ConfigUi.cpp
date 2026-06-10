#include "ui/ConfigUi.h"

#include <imgui.h>
#include <TextEditor.h>

#include <string>
#include <unordered_map>

namespace
{
struct EditorState
{
    TextEditor  Editor;
    std::string LastText; // last text we synced with the config field
};

std::unordered_map<std::string, EditorState> s_editors;
} // namespace

namespace ConfigUi
{
bool CodeEditor(const char* id, std::string& text, Lang lang, float height)
{
    auto it = s_editors.find(id);
    if (it == s_editors.end())
    {
        EditorState& st = s_editors[id];
        st.Editor.SetLanguage(lang == Lang::Glsl ? TextEditor::Language::Glsl()
                                                 : TextEditor::Language::Lua());
        st.Editor.SetText(text);
        st.LastText = text;
        it = s_editors.find(id);
    }
    else if (text != it->second.LastText)
    {
        // Config field changed out from under us (e.g. programmatic default swap).
        it->second.Editor.SetText(text);
        it->second.LastText = text;
    }

    EditorState& st = it->second;
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
} // namespace ConfigUi
