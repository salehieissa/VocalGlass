#pragma once

#include <juce_core/juce_core.h>
#include <array>

//==============================================================================
// Shared musical definitions: note names, vocal ranges, key & scale patterns.
//==============================================================================
namespace music
{
    // Pitch-class names, C = 0 .. B = 11.
    inline const juce::StringArray& noteNames()
    {
        static const juce::StringArray names {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names;
    }

    inline const juce::StringArray& keyNames() { return noteNames(); }

    //==========================================================================
    struct Range { juce::String name; float minHz; float maxHz; };

    inline const std::vector<Range>& vocalRanges()
    {
        static const std::vector<Range> r {
            { "Soprano",    250.0f, 1100.0f },
            { "Alto/Tenor", 130.0f,  700.0f },
            { "Tenor",      130.0f,  525.0f },
            { "Baritone",    98.0f,  400.0f },
            { "Bass",        75.0f,  330.0f },
            { "Instrument",  50.0f, 2000.0f },
        };
        return r;
    }

    inline juce::StringArray vocalRangeNames()
    {
        juce::StringArray a;
        for (auto& r : vocalRanges()) a.add (r.name);
        return a;
    }

    //==========================================================================
    struct Scale { juce::String name; std::vector<int> steps; };

    inline const std::vector<Scale>& scales()
    {
        static const std::vector<Scale> s {
            { "Chromatic",       { 0,1,2,3,4,5,6,7,8,9,10,11 } },
            { "Major",           { 0,2,4,5,7,9,11 } },
            { "Minor",           { 0,2,3,5,7,8,10 } },
            { "Harmonic Minor",  { 0,2,3,5,7,8,11 } },
            { "Dorian",          { 0,2,3,5,7,9,10 } },
            { "Mixolydian",      { 0,2,4,5,7,9,10 } },
            { "Phrygian",        { 0,1,3,5,7,8,10 } },
            { "Lydian",          { 0,2,4,6,7,9,11 } },
            { "Pentatonic Maj",  { 0,2,4,7,9 } },
            { "Pentatonic Min",  { 0,3,5,7,10 } },
        };
        return s;
    }

    inline juce::StringArray scaleNames()
    {
        juce::StringArray a;
        for (auto& s : scales()) a.add (s.name);
        return a;
    }

    // Build the 12 note-class mask for a scale rooted at `key` (0 = C).
    inline std::array<bool, 12> maskFor (int scaleIndex, int key)
    {
        std::array<bool, 12> mask { };
        const auto& list = scales();
        if (! juce::isPositiveAndBelow (scaleIndex, (int) list.size()))
            scaleIndex = 0;
        for (int step : list[(size_t) scaleIndex].steps)
            mask[(size_t) (((step + key) % 12 + 12) % 12)] = true;
        return mask;
    }
}
