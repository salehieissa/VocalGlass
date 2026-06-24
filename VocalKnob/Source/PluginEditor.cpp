#include "PluginEditor.h"

namespace
{
    const std::array<juce::String, 4> kModeWords { "clean", "warm", "dirty", "blown" };
    const std::array<juce::String, 4> kCapLabels { "GRIT", "TONE", "WIDTH", "AMP" };
    const std::array<juce::String, 4> kTaglines {
        "polish and lift, stays natural",
        "round it out, analog glue",
        "add bite without wrecking the vocal",
        "slammed and saturated, full send"
    };
}

//==============================================================================
VocalKnobEditor::VocalKnobEditor (VocalKnobProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    addAndMakeVisible (dial);
    amountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "amount", dial);

    // ---- mode pills ----
    for (int i = 0; i < 4; ++i)
    {
        auto& b = modeBtns[(size_t) i];
        b.setButtonText (kModeWords[(size_t) i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setMode (i); };
        addAndMakeVisible (b);

        auto& c = capLabels[(size_t) i];
        c.setText (kCapLabels[(size_t) i], juce::dontSendNotification);
        c.setJustificationType (juce::Justification::centred);
        c.setFont (theme::font (11.0f, false));
        c.setColour (juce::Label::textColourId, theme::inkSoft);
        addAndMakeVisible (c);
    }

    subtitle.setJustificationType (juce::Justification::centred);
    subtitle.setFont (theme::font (14.0f, false));
    subtitle.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (subtitle);

    // ---- branding (geometry kept; the wordmark itself is painted two-tone) ----
    brand.setText ("vocalknob", juce::dontSendNotification);
    brand.setFont (theme::font (18.0f, true));
    brand.setColour (juce::Label::textColourId, theme::ink);
    addChildComponent (brand);

    brandSub.setText ("VOCAL TEXTURE", juce::dontSendNotification);
    brandSub.setFont (theme::font (10.5f, false));
    brandSub.setColour (juce::Label::textColourId, theme::inkSoft);
    addChildComponent (brandSub);

    // ---- preset selector ----
    prevBtn.setClickingTogglesState (false);
    nextBtn.setClickingTogglesState (false);
    prevBtn.onClick = [this] { stepProgram (-1); };
    nextBtn.onClick = [this] { stepProgram ( 1); };
    for (auto* b : { &prevBtn, &nextBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::textColourOffId, theme::inkSoft);
        addAndMakeVisible (*b);
    }

    presetName.setJustificationType (juce::Justification::centred);
    presetName.setFont (theme::font (13.0f, false));
    presetName.setColour (juce::Label::textColourId, theme::ink);
    addAndMakeVisible (presetName);

    startTimerHz (30);
    setSize (700, 480);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalKnobEditor::~VocalKnobEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalKnobEditor::setMode (int mode)
{
    if (auto* m = proc.apvts.getParameter ("mode"))
        m->setValueNotifyingHost (m->getNormalisableRange().convertTo0to1 ((float) mode));
}

void VocalKnobEditor::stepProgram (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
}

//==============================================================================
void VocalKnobEditor::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    dial.setCaption (kModeWords[(size_t) juce::jlimit (0, 3, mode)]);
    subtitle.setText (kTaglines[(size_t) juce::jlimit (0, 3, mode)], juce::dontSendNotification);

    for (int i = 0; i < 4; ++i)
        modeBtns[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);

    presetName.setText (proc.getProgramName (proc.getCurrentProgram()), juce::dontSendNotification);
}

//==============================================================================
void VocalKnobEditor::paint (juce::Graphics& g)
{
    theme::backdrop (g, getLocalBounds());

    // floating main card
    {
        auto rf = cardArea.toFloat();
        theme::elevate (g, rf, 22.0f);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, 22.0f);
        theme::topHighlight (g, rf, 22.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, 22.0f, 1.0f);
    }

    // mood: a faint accent bloom behind the hero dial
    theme::accentBloom (g, dial.getBounds().getCentre().toFloat(),
                        (float) dial.getWidth() * 0.55f, 0.06f);

    // two-tone wordmark: solid ink "vocal" + accent "knob" with an accent underline
    {
        auto wm = theme::font (18.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        const auto bb = brand.getBounds();
        g.setColour (theme::ink);
        g.drawText ("vocal", bb, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("knob", bb.withTrimmedLeft ((int) vw), juce::Justification::centredLeft);
        g.fillRoundedRectangle ((float) bb.getX(), (float) bb.getBottom() - 2.0f, 18.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL TEXTURE", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.0f, 2.4f, false, juce::Justification::centredLeft);

    // clean white preset selector pill (no shadow, crisp hairline)
    {
        auto pf = presetPill.toFloat();
        const float rad = pf.getHeight() * 0.5f;
        juce::ColourGradient wg (juce::Colours::white, pf.getX(), pf.getY(),
                                 juce::Colour (0xfff4f5f8), pf.getX(), pf.getBottom(), false);
        g.setGradientFill (wg);
        g.fillRoundedRectangle (pf, rad);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (pf, rad, 1.2f);
    }
}

//==============================================================================
void VocalKnobEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    auto r = getLocalBounds().reduced (12);
    cardArea = r;

    auto inner = r.reduced (18);

    // ---- top bar ----
    auto top = inner.removeFromTop (36);
    brand.setBounds (top.getX(), top.getY() + 2, 96, 30);
    brandSub.setBounds (brand.getRight() + 4, top.getY() + 8, 110, 20);

    // preset selector centred in the top bar
    const int pillW = 220, pillH = 32;
    presetPill = juce::Rectangle<int> (inner.getCentreX() - pillW / 2, top.getY(), pillW, pillH);
    prevBtn.setBounds (presetPill.getX() + 3, presetPill.getY() + 3, 28, pillH - 6);
    nextBtn.setBounds (presetPill.getRight() - 31, presetPill.getY() + 3, 28, pillH - 6);
    presetName.setBounds (prevBtn.getRight(), presetPill.getY(),
                          nextBtn.getX() - prevBtn.getRight(), pillH);

    // ---- bottom rows (build from the bottom up) ----
    auto caps = inner.removeFromBottom (18);
    inner.removeFromBottom (3);
    auto pills = inner.removeFromBottom (36);
    inner.removeFromBottom (8);
    auto sub = inner.removeFromBottom (22);
    subtitle.setBounds (sub);

    // four pills centred, with the caption labels directly beneath each
    const int pw = 104, gap = 12;
    const int rowW = pw * 4 + gap * 3;
    int x = inner.getCentreX() - rowW / 2;
    for (int i = 0; i < 4; ++i)
    {
        modeBtns[(size_t) i].setBounds (x, pills.getY() + 2, pw, 32);
        capLabels[(size_t) i].setBounds (x, caps.getY(), pw, 18);
        x += pw + gap;
    }

    // ---- dial fills the remaining centre ----
    dial.setBounds (inner.withSizeKeepingCentre (juce::jmin (inner.getWidth(), 300),
                                                 juce::jmin (inner.getHeight(), 300)));
}
