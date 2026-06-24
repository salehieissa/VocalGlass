#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/KnobLookAndFeel.h"
#include "ui/KnobDial.h"
#include "../../common/Licensing/ActivationOverlay.h"
#include <array>

//==============================================================================
class VocalKnobEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalKnobEditor (VocalKnobProcessor&);
    ~VocalKnobEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMode (int mode);
    void stepProgram (int delta);

    VocalKnobProcessor& proc;
    KnobLookAndFeel laf;

    KnobDial dial;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAtt;

    std::array<Bouncy<juce::TextButton>, 4> modeBtns;
    juce::Label subtitle;
    std::array<juce::Label, 4> capLabels;

    juce::Label brand, brandSub;
    Bouncy<juce::TextButton> prevBtn { "<" }, nextBtn { ">" };
    juce::Label presetName;

    juce::Rectangle<int> cardArea, presetPill;

    // Full-editor "enter your license key" overlay (shown until activated).
    ActivationOverlay licenseOverlay { proc.license, "VocalKnob" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalKnobEditor)
};
