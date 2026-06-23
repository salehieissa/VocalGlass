#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/GritLookAndFeel.h"
#include "ui/GritDial.h"
#include "ui/LevelMeter.h"
#include "ui/Bounce.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

//==============================================================================
// Vertical slider with a name + live percentage label above it (mockup style).
//==============================================================================
struct VSlider
{
    juce::Slider slider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Label  name, value;
    std::unique_ptr<SliderAtt> att;
    std::function<juce::String (juce::Slider&)> fmt;   // optional custom readout

    void setup (juce::AudioProcessorValueTreeState&, const juce::String& id,
                const juce::String& text, juce::Component& parent);
    void layout (juce::Rectangle<int> cell);
    void refresh();
    void setFormat (std::function<juce::String (juce::Slider&)> f);
};

//==============================================================================
// Small rotary knob with a name label (used by the FX modules).
//==============================================================================
struct Knob
{
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label  name, value;
    std::unique_ptr<SliderAtt> att;
    std::function<juce::String (double)> fmt;

    void setup (juce::AudioProcessorValueTreeState&, const juce::String& id,
                const juce::String& text, juce::Component& parent);
    void layout (juce::Rectangle<int> cell);
    void asPercent();
    void asMillis();
    void refresh();
};

//==============================================================================
class VocalGritEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalGritEditor (VocalGritProcessor&);
    ~VocalGritEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMode (int index);
    void stepPreset (int delta);
    void setupFxModule (juce::ToggleButton& toggle, const juce::String& toggleId,
                        Knob& a, const juce::String& aId, const juce::String& aName,
                        Knob& b, const juce::String& bId, const juce::String& bName,
                        Knob& c, const juce::String& cId, const juce::String& cName);

    VocalGritProcessor& proc;
    GritLookAndFeel lnf;

    // Header / presets
    Bouncy<juce::TextButton> presetPrev { "<" }, presetNext { ">" };
    juce::Label      presetName;

    // GRIT controls
    GritDial gritDial;
    std::unique_ptr<SliderAtt> gritAtt;
    VSlider driveS, toneS, widthS, formantS;

    // Character pills
    std::array<Bouncy<juce::TextButton>, 4> charButtons;

    // Texture pills
    std::array<Bouncy<juce::ToggleButton>, 4> pills;

    // Right card: meters + mix + output
    LevelMeter inMeter, outMeter;
    juce::Label inMeterLabel, outMeterLabel, mixLabel, mixValue, rawLabel, procLabel;
    juce::Slider mixSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<SliderAtt> mixAtt;
    juce::Label   outputLabel, outputValue;
    juce::Slider  outputSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<SliderAtt> outputAtt;

    // FX modules
    Bouncy<juce::ToggleButton> doublerOn, delayOn, reverbOn, glitchOn;
    Knob dblA, dblB, dblC;   // detune / width / mix
    Knob delA, delB, delC;   // time / feedback / mix
    Knob revA, revB, revC;   // size / damp / mix
    Knob glA,  glB,  glC;    // rate / depth / mix
    std::vector<std::unique_ptr<ButtonAtt>> buttonAtts;

    // Delay host-tempo sync: a compact toggle + note-division selector that
    // sit in the DELAY card and take over the "time" knob when engaged.
    Bouncy<juce::ToggleButton> delaySyncBtn;
    juce::ComboBox delayDivBox;
    std::unique_ptr<ButtonAtt> delaySyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> delayDivAtt;
    void updateDelaySyncUI();

    // Geometry remembered for paint().
    juce::Rectangle<int> rightCardArea, presetArea;
    std::array<juce::Rectangle<int>, 4> fxAreas;
    float pulsePhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalGritEditor)
};
