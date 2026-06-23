#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Bounce.h"
#include "ui/TuneLookAndFeel.h"
#include "ui/RingKnob.h"
#include "ui/NoteDisplay.h"
#include "ui/CentsMeter.h"
#include "ui/NoteButton.h"
#include "ui/ToggleSwitch.h"
#include "ui/SegmentedControl.h"
#include "ui/IconButton.h"
#include <array>
#include <memory>

//==============================================================================
class VocalTuneEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit VocalTuneEditor (VocalTuneProcessor&);
    ~VocalTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void stepProgram (int delta);
    void showPresetMenu();
    void configureKnob (RingKnob&, const juce::String& paramId, std::unique_ptr<SliderAtt>&);
    void configureBox (juce::Label&, const juce::String& paramId);

    VocalTuneProcessor& proc;
    TuneLookAndFeel laf;

    // top bar
    juce::Label brand, brandSub;
    juce::Label vrLabel, keyLabel, ksLabel;
    juce::ComboBox vocalRange, keyBox, keyScale;
    std::unique_ptr<ComboAtt> vrAtt, keyAtt, ksAtt;

    juce::Label presetName;
    IconButton prevBtn { IconButton::Icon::Prev, false };
    IconButton nextBtn { IconButton::Icon::Next, false };
    IconButton dotsBtn { IconButton::Icon::Dots, false };

    // left card
    NoteDisplay noteDisplay;
    CentsMeter  centsMeter;
    juce::Label editNotesLabel;
    IconButton  resetBtn { IconButton::Icon::Reset, false };
    std::array<Bouncy<NoteButton>, 12> noteButtons {
        Bouncy<NoteButton>("C"),  Bouncy<NoteButton>("C#"), Bouncy<NoteButton>("D"),
        Bouncy<NoteButton>("D#"), Bouncy<NoteButton>("E"),  Bouncy<NoteButton>("F"),
        Bouncy<NoteButton>("F#"), Bouncy<NoteButton>("G"),  Bouncy<NoteButton>("G#"),
        Bouncy<NoteButton>("A"),  Bouncy<NoteButton>("A#"), Bouncy<NoteButton>("B") };
    std::array<std::unique_ptr<ButtonAtt>, 12> noteAtt;

    juce::Label detuneLabel, detuneValue, modeLabel;
    RingKnob detuneKnob { 3.0f, false };
    std::unique_ptr<SliderAtt> detuneAtt;
    SegmentedControl modeSeg { "Low Latency", "HQ" };

    // right card
    juce::Label retuneLabel, humanizeLabel, flexLabel, modernLabel, classicLabel;
    RingKnob retuneKnob { 5.0f, true };
    RingKnob humanizeKnob { 3.5f, false };
    RingKnob flexKnob { 3.5f, false };
    std::unique_ptr<SliderAtt> retuneAtt, humanizeAtt, flexAtt;
    juce::Label retuneBox, humanizeBox, flexBox;
    ToggleSwitch modernSwitch;
    std::unique_ptr<ButtonAtt> modernAtt;

    // icon row
    IconButton undoBtn { IconButton::Icon::Undo };
    IconButton redoBtn { IconButton::Icon::Redo };
    IconButton gearBtn { IconButton::Icon::Gear };
    IconButton powerBtn { IconButton::Icon::Power };

    // card geometry (filled in resized)
    juce::Rectangle<int> leftCard, rightCard, iconCard, presetPill;
    juce::Rectangle<int> retuneBoxR, humanizeBoxR, flexBoxR;
    int editLineY = 0, detuneDividerY = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalTuneEditor)
};
