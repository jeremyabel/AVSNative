#pragma once

#include <cstdint>

// ── Global per-frame keyboard state ───────────────────────────────────────────
// A tiny, SDL-free keyboard layer. The App feeds raw SDL keycodes (cast to
// uint32_t) once per frame in ProcessEvents; effects poll WasKeyPressed() in
// Render() to react to key-down edges, and config UIs poll LastKeyPressed() to
// implement press-to-capture key binding.
//
// "Pressed this frame" is edge-triggered: only genuine key-DOWN events are pushed
// (the App ignores auto-repeat and key-up), so a held key registers exactly once.
// The set is cleared at the start of each frame (BeginFrame) and stays valid through
// Engine::Tick (effects) and RenderUI (capture UI) of the same loop iteration.
//
// Keys are identified by SDL keycode everywhere, so the capture UI and the matching
// effect agree without any ImGui<->SDL key translation.
namespace avs
{
class KeyInput
{
public:
    // Clears the per-frame pressed set. Called by App at the top of ProcessEvents.
    static void BeginFrame();

    // Records a key-down edge for this frame. Called by App per SDL_EVENT_KEY_DOWN.
    static void PushKeyDown(uint32_t keycode);

    // True if `keycode` saw a key-down edge this frame. Polled by effects in Render().
    static bool WasKeyPressed(uint32_t keycode);

    // The most recent keycode pressed this frame (0 if none). Polled by the config
    // UI for press-to-capture key binding.
    static uint32_t LastKeyPressed();

    // ── Held-key state (persists across frames, unlike the edge set above) ────────
    // The App updates this on every key-down/up (NOT cleared by BeginFrame), so
    // effects can poll whether a key is currently held (e.g. hold-to-strobe). On
    // focus loss the App clears all held keys to avoid a stuck key.
    static void SetKeyDown(uint32_t keycode, bool down);
    static bool IsKeyDown(uint32_t keycode);
    static void ClearHeld();
};
} // namespace avs
