#pragma once

// UI-only beat-meter animation state for Custom BPM — not part of the effect and
// not serialized. The effect exposes monotonic in/out beat counts; the UI advances
// each 8-segment meter (bouncing between 0 and 7) by the number of new beats since
// the previous frame.
struct CustomBpmUIState
{
    int InSeg  = 0, InDir  = 1;
    int OutSeg = 0, OutDir = 1;
    int LastInCount  = 0;
    int LastOutCount = 0;
};
