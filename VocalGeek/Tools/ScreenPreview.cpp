// Offscreen preview harness for the VocalGeek pixel screen. Feeds the
// display synthetic vocal-shaped scope data and renders every cartridge's
// live + idle scene to PNGs so the visuals can be verified without opening
// an audio device.

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/ui/GeekDisplay.h"

static void fillScope (GeekEngine& engine, float phase)
{
    // vocal-ish: syllable bursts with decay + vibrato
    for (int i = 0; i < GeekEngine::scopeSize; ++i)
    {
        const float t = (float) i / (float) GeekEngine::scopeSize;
        const float syllable = std::pow (0.5f + 0.5f * std::sin ((t * 3.0f + phase) * juce::MathConstants<float>::twoPi), 3.0f);
        const float vib = 0.75f + 0.25f * std::sin ((t * 21.0f + phase * 3.0f) * juce::MathConstants<float>::twoPi);
        engine.scope[(size_t) i] = juce::jlimit (0.0f, 1.0f, 0.08f + 0.55f * syllable * vib);
    }
    engine.scopeWrite.store (0);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    GeekEngine engine;
    engine.prepare (44100.0, 512, 2);

    GeekDisplay display (engine);
    display.setSize (648, 540);

    juce::Thread::sleep (1600);   // let the boot sequence finish

    for (int theme = 0; theme <= 5; ++theme)
    {
        GeekDisplay::State s;
        s.theme = theme;
        s.dose = 0.35f;

        // live scene: warm up the animation so particles/plumes populate
        s.inLevel = 0.5f;
        s.outLevel = 0.55f;
        for (int f = 0; f < 240; ++f)
        {
            fillScope (engine, (float) f / 60.0f);
            display.refresh (s);
        }
        auto live = display.createComponentSnapshot (display.getLocalBounds());
        juce::File liveOut ("/tmp/geek-screen-" + juce::String (GeekDisplay::themeName (theme)) + "-live.png");
        liveOut.deleteFile();
        juce::PNGImageFormat png;
        juce::FileOutputStream lo (liveOut);
        png.writeImageToStream (live, lo);

        // idle scene
        s.inLevel = 0.0f;
        s.outLevel = 0.0f;
        engine.scope.fill (0.0f);
        for (int f = 0; f < 800; ++f)
            display.refresh (s);
        auto idle = display.createComponentSnapshot (display.getLocalBounds());
        juce::File idleOut ("/tmp/geek-screen-" + juce::String (GeekDisplay::themeName (theme)) + "-idle.png");
        idleOut.deleteFile();
        juce::FileOutputStream io (idleOut);
        png.writeImageToStream (idle, io);

        std::cout << GeekDisplay::themeName (theme) << " done\n";
    }
    return 0;
}
