#include "PluginEditor.h"
#include "ui/Theme.h"

//==============================================================================
// Baked-plate geometry. The chassis PNGs are 1386x1135 and the plate fills the
// whole canvas, so every coordinate below is a fraction of the full image
// (measured with Python by diffing the ON plate against the OFF plate and by
// circle-fitting the neon rings — never eyeballed).
namespace plategeo
{
    // preset pill: invisible hit circles over the baked arrows + name between
    constexpr float presetArrowL = 0.385f, presetArrowR = 0.632f, presetCy = 0.100f;

    // hero dial: ring band r[0.1030,0.1313], glow bleeds to ~0.127
    constexpr float dialCx = 0.1995f, dialCy = 0.3529f;
    constexpr float dialWedgeR = 0.135f, dialDomeDia = 0.199f;

    // vertical sliders (drive / tone / width / formant)
    constexpr float vsX[4] = { 0.3781f, 0.4589f, 0.5437f, 0.6270f };
    constexpr float vsY0 = 0.2150f, vsY1 = 0.5242f, vsHalfW = 0.0065f;

    // horizontal meter bars (continuous fill) + mix / output slider tracks
    constexpr float mX0 = 0.7121f, mX1 = 0.8889f;
    constexpr float mInY0 = 0.2035f, mInY1 = 0.2185f;
    constexpr float mOutY0 = 0.2969f, mOutY1 = 0.3128f;
    constexpr float hX0 = 0.7143f, hX1 = 0.9315f;
    constexpr float mixCy = 0.4203f, mixY0 = 0.4123f, mixY1 = 0.4308f;
    constexpr float outCy = 0.5696f, outY0 = 0.5612f, outY1 = 0.5806f;

    // selector pills: measured lit-diff bounds (glow included; pills don't touch)
    constexpr float charX[4][2] = { { 0.0563f, 0.1876f }, { 0.2100f, 0.3413f },
                                    { 0.3658f, 0.4993f }, { 0.5253f, 0.6580f } };
    constexpr float charY0 = 0.5542f, charY1 = 0.6203f;
    constexpr float texX[4][2]  = { { 0.0563f, 0.1869f }, { 0.2100f, 0.3420f },
                                    { 0.3658f, 0.4993f }, { 0.5253f, 0.6587f } };
    constexpr float texY0 = 0.6308f, texY1 = 0.6960f;

    // FX cards: divider line (lights with the module), on/sync pills, knobs
    constexpr float divX[4][2] = { { 0.0592f, 0.2511f }, { 0.2922f, 0.4877f },
                                   { 0.5238f, 0.7150f }, { 0.7504f, 0.9387f } };
    constexpr float divY0 = 0.7360f, divY1 = 0.7415f;
    constexpr float onX[4][2] = { { 0.1948f, 0.2468f }, { 0.4365f, 0.4885f },
                                  { 0.6580f, 0.7085f }, { 0.8874f, 0.9401f } };
    constexpr float syncX0 = 0.3802f, syncX1 = 0.4343f;
    constexpr float onY0 = 0.7489f, onY1 = 0.7833f;

    constexpr float fxX[12] = { 0.0887f, 0.1562f, 0.2219f,  0.3236f, 0.3900f, 0.4563f,
                                0.5545f, 0.6205f, 0.6854f,  0.7839f, 0.8499f, 0.9145f };
    constexpr float fxCy = 0.8520f, fxWedgeR = 0.032f, fxDomeDia = 0.038f;
    constexpr float fxValY0 = 0.888f, fxValY1 = 0.926f;
}

//==============================================================================
static juce::String toPercent (juce::Slider& s)
{
    const auto prop = s.valueToProportionOfLength (s.getValue());
    return juce::String (juce::roundToInt ((float) prop * 100.0f)) + "%";
}

void VSlider::setup (juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
                     const juce::String& text, juce::Component& parent)
{
    parent.addAndMakeVisible (slider);

    name.setText (text, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (theme::font (12.0f, false));
    name.setColour (juce::Label::textColourId, theme::inkSoft);
    parent.addAndMakeVisible (name);

    value.setJustificationType (juce::Justification::centred);
    value.setFont (theme::font (14.0f, true));
    value.setColour (juce::Label::textColourId, theme::ink);
    parent.addAndMakeVisible (value);

    att = std::make_unique<SliderAtt> (apvts, id, slider);
    slider.onValueChange = [this] { refresh(); };
    refresh();
}

void VSlider::refresh()
{
    value.setText (fmt ? fmt (slider) : toPercent (slider), juce::dontSendNotification);
}

void VSlider::setFormat (std::function<juce::String (juce::Slider&)> f)
{
    fmt = std::move (f);
    refresh();
}

void VSlider::layout (juce::Rectangle<int> cell)
{
    name.setBounds (cell.removeFromTop (16));
    value.setBounds (cell.removeFromTop (18));
    slider.setBounds (cell.reduced (cell.getWidth() / 2 - 16, 6));
}

//==============================================================================
void Knob::setup (juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
                  const juce::String& text, juce::Component& parent)
{
    parent.addAndMakeVisible (slider);

    name.setText (text, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (theme::font (11.5f, false));
    name.setColour (juce::Label::textColourId, theme::inkSoft);
    parent.addAndMakeVisible (name);

    value.setJustificationType (juce::Justification::centred);
    value.setFont (theme::font (12.0f, true));
    value.setColour (juce::Label::textColourId, theme::ink);
    parent.addAndMakeVisible (value);

    fmt = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    att = std::make_unique<SliderAtt> (apvts, id, slider);
    slider.onValueChange = [this] { refresh(); };
    refresh();
}

void Knob::layout (juce::Rectangle<int> cell)
{
    name.setBounds (cell.removeFromTop (15));
    value.setBounds (cell.removeFromBottom (16));
    slider.setBounds (cell);
}

void Knob::refresh()
{
    value.setText (fmt ? fmt (slider.getValue()) : juce::String (slider.getValue(), 2),
                   juce::dontSendNotification);
}

void Knob::asPercent()
{
    fmt = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    refresh();
}

void Knob::asMillis()
{
    fmt = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };
    refresh();
}

//==============================================================================
VocalGritEditor::VocalGritEditor (VocalGritProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      inMeter (p.inputLevel), outMeter (p.outputLevel)
{
    setLookAndFeel (&lnf);

    chassisImg   = skin::image ("grit-chassis@2x.png");
    chassisOnImg = skin::image ("grit-chassis-on@2x.png");
    const bool baked = chassisImg.isValid() && chassisOnImg.isValid();
    lnf.plate = baked;

    // --- preset browser ---
    presetPrev.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    presetNext.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    presetPrev.onClick = [this] { stepPreset (-1); };
    presetNext.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (presetPrev);
    addAndMakeVisible (presetNext);

    presetName.setJustificationType (juce::Justification::centred);
    presetName.setFont (theme::font (15.0f, false));
    presetName.setColour (juce::Label::textColourId, theme::ink);
    presetName.setText (proc.getProgramName (proc.getCurrentProgram()), juce::dontSendNotification);
    addAndMakeVisible (presetName);

    // --- grit dial ---
    gritDial.setCaption ("add bite without wrecking the vocal");
    addAndMakeVisible (gritDial);
    gritAtt = std::make_unique<SliderAtt> (proc.apvts, "drive", gritDial);

    driveS  .setup (proc.apvts, "bias",        "drive",   *this);
    toneS   .setup (proc.apvts, "tone",        "tone",    *this);
    widthS  .setup (proc.apvts, "stereoWidth", "width",   *this);
    formantS.setup (proc.apvts, "formant",     "formant", *this);

    // Tone reads in Hz/kHz, not a meaningless percentage.
    toneS.setFormat ([] (juce::Slider& s)
    {
        const double hz = s.getValue();
        return hz >= 1000.0 ? juce::String (hz / 1000.0, 1) + " kHz"
                            : juce::String (juce::roundToInt (hz)) + " Hz";
    });

    // Formant is bipolar: show it as +/- semitones, "0" at the centre detent.
    formantS.setFormat ([] (juce::Slider& s)
    {
        const int st = juce::roundToInt ((s.getValue() - 0.5) * 2.0 * 12.0);
        return st == 0 ? juce::String ("0 st")
                       : (st > 0 ? "+" : "") + juce::String (st) + " st";
    });

    // --- character buttons ---
    const char* charNames[] = { "clean", "warm", "dirty", "blown" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = charButtons[(size_t) i];
        b.setButtonText (charNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (100);
        b.onClick = [this, i] { setMode (i); };
        addAndMakeVisible (b);
    }

    // --- texture pills ---
    const char* pillNames[] = { "fuzz", "amp", "speaker", "presence" };
    const char* pillIds[]   = { "fuzzOn", "ampOn", "speakerOn", "presenceOn" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = pills[(size_t) i];
        b.setButtonText (pillNames[i]);
        addAndMakeVisible (b);
        buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, pillIds[i], b));
    }

    // --- right card: meters + mix ---
    auto labelInit = [this] (juce::Label& l, const juce::String& t, float sz, juce::Colour c)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (theme::font (sz, false));
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };
    labelInit (inMeterLabel,  "input level",  13.0f, theme::ink);
    labelInit (outMeterLabel, "output level", 13.0f, theme::ink);
    labelInit (mixLabel,      "mix",          14.0f, theme::ink);
    labelInit (rawLabel,      "raw vocal",    11.0f, theme::inkSoft);
    labelInit (procLabel,     "processed vocal", 11.0f, theme::inkSoft);
    procLabel.setJustificationType (juce::Justification::right);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    mixValue.setJustificationType (juce::Justification::right);
    mixValue.setFont (theme::font (16.0f, true));
    mixValue.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (mixValue);
    addAndMakeVisible (mixSlider);
    mixAtt = std::make_unique<SliderAtt> (proc.apvts, "mix", mixSlider);
    mixSlider.onValueChange = [this] { mixValue.setText (toPercent (mixSlider), juce::dontSendNotification); };
    mixValue.setText (toPercent (mixSlider), juce::dontSendNotification);

    // Output trim (dB).
    labelInit (outputLabel, "output", 14.0f, theme::ink);
    outputValue.setJustificationType (juce::Justification::right);
    outputValue.setFont (theme::font (16.0f, true));
    outputValue.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (outputValue);
    addAndMakeVisible (outputSlider);
    outputAtt = std::make_unique<SliderAtt> (proc.apvts, "output", outputSlider);
    auto fmtDb = [this]
    {
        const double db = outputSlider.getValue();
        outputValue.setText ((db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB",
                             juce::dontSendNotification);
    };
    outputSlider.onValueChange = fmtDb;
    fmtDb();

    // --- FX modules ---
    setupFxModule (doublerOn, "doublerOn",
                   dblA, "doublerDetune", "detune",
                   dblB, "doublerWidth",  "width",
                   dblC, "doublerMix",    "mix");
    setupFxModule (delayOn, "delayOn",
                   delA, "delayTime",     "time",
                   delB, "delayFeedback", "feedback",
                   delC, "delayMix",      "mix");
    setupFxModule (reverbOn, "reverbOn",
                   revA, "reverbSize", "size",
                   revB, "reverbDamp", "damp",
                   revC, "reverbMix",  "mix");
    setupFxModule (glitchOn, "glitchOn",
                   glA, "glitchRate",  "rate",
                   glB, "glitchDepth", "depth",
                   glC, "glitchMix",   "mix");

    // Clean value readouts.
    for (auto* k : { &dblA, &dblB, &dblC, &delB, &delC, &revA, &revB, &revC, &glB, &glC })
        k->asPercent();
    delA.asMillis();

    // Glitch rate snaps to a note division — show the division name.
    glA.fmt = [] (double v)
    {
        static const char* names[6] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
        return juce::String (names[juce::jlimit (0, 5, juce::roundToInt (v * 5.0))]);
    };
    glA.refresh();

    // --- DELAY host-tempo sync: compact toggle + division selector ---
    delaySyncBtn.setButtonText ("sync");
    addAndMakeVisible (delaySyncBtn);
    delaySyncAtt = std::make_unique<ButtonAtt> (proc.apvts, "delaySync", delaySyncBtn);
    delaySyncBtn.onClick = [this] { updateDelaySyncUI(); };

    delayDivBox.addItemList ({ "1/1", "1/2", "1/2.", "1/2T", "1/4", "1/4.", "1/4T",
                               "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T", "1/32" }, 1);
    delayDivBox.setJustificationType (juce::Justification::centred);
    delayDivBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::white);
    delayDivBox.setColour (juce::ComboBox::textColourId,       theme::ink);
    delayDivBox.setColour (juce::ComboBox::outlineColourId,    theme::cardLine);
    delayDivBox.setColour (juce::ComboBox::arrowColourId,      theme::accent);
    addAndMakeVisible (delayDivBox);
    delayDivAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                      (proc.apvts, "delayDiv", delayDivBox);
    updateDelaySyncUI();

    // ------------------------------------------------------------------
    // Baked plate: components become sprite-painted or invisible hit areas;
    // all static text lives on the chassis image.
    // ------------------------------------------------------------------
    if (baked)
    {
        using namespace plategeo;

        gritDial.setPlateMode (true);

        // preset arrows are baked chrome buttons — invisible hit circles
        for (auto* b : { &presetPrev, &presetNext })
        {
            b->setButtonText ("");
            b->setComponentID ("hit");
        }
        presetName.setFont (theme::font (15.0f, false));
        presetName.setColour (juce::Label::textColourId, theme::ink);

        // drive/tone/width/formant stay vertical sliders: the recessed track
        // is baked, the pink fill is masked from the ON plate, and the LnF
        // paints only the chrome stud thumb. Live value under the caption.
        for (auto* vs : { &driveS, &toneS, &widthS, &formantS })
        {
            vs->name.setVisible (false);   // caption is baked into the plate
            vs->value.setJustificationType (juce::Justification::centred);
            vs->value.setFont (theme::font (13.0f, false));
            vs->value.setColour (juce::Label::textColourId, theme::ink);
        }

        // mix / output stay horizontal sliders; pink readouts in the caption row
        for (auto* v : { &mixValue, &outputValue })
        {
            v->setJustificationType (juce::Justification::centredRight);
            v->setFont (theme::font (13.0f, true));
            v->setColour (juce::Label::textColourId, theme::accent);
        }

        // FX knobs sweep 6 o'clock -> 6 o'clock, once around; live value below
        for (auto* k : { &dblA, &dblB, &dblC, &delA, &delB, &delC,
                         &revA, &revB, &revC, &glA, &glB, &glC })
        {
            k->slider.setRotaryParameters (juce::MathConstants<float>::pi,
                                           juce::MathConstants<float>::pi * 3.0f, true);
            k->name.setVisible (false);    // baked caption
            k->value.setFont (theme::font (10.5f, false));
            k->value.setColour (juce::Label::textColourId, theme::ink);
        }

        // selector banks + FX power/sync pills: the plate draws the pills
        for (auto& b : charButtons) { b.setButtonText (""); b.setComponentID ("hit"); }
        for (auto& b : pills)       { b.setButtonText (""); b.setComponentID ("hit"); }
        for (auto* t : { &doublerOn, &delayOn, &reverbOn, &glitchOn, &delaySyncBtn })
        {
            t->setButtonText ("");
            t->setComponentID ("hit");
        }

        // meter bars + right-card captions are baked / plate-drawn
        inMeter.setVisible (false);
        outMeter.setVisible (false);
        for (auto* l : { &inMeterLabel, &outMeterLabel, &mixLabel,
                         &rawLabel, &procLabel, &outputLabel })
            l->setVisible (false);

        // the sync division box sits flat on the glass like a value readout
        delayDivBox.setColour (juce::ComboBox::backgroundColourId,
                               juce::Colours::transparentBlack);
        delayDivBox.setColour (juce::ComboBox::outlineColourId,
                               juce::Colours::transparentBlack);

        // live dB readouts beside the meter bars
        for (auto* l : { &inDbLbl, &outDbLbl })
        {
            l->setJustificationType (juce::Justification::centred);
            l->setFont (theme::font (11.0f, false));
            l->setColour (juce::Label::textColourId, theme::inkSoft);
            l->setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*l);
        }

        // neon rings revealed from the ON plate: hero dial + 12 FX knobs
        plateKnobs = { { &gritDial, dialCx, dialCy, dialWedgeR, dialDomeDia * 0.5f } };
        juce::Slider* fx[12] = { &dblA.slider, &dblB.slider, &dblC.slider,
                                 &delA.slider, &delB.slider, &delC.slider,
                                 &revA.slider, &revB.slider, &revC.slider,
                                 &glA.slider,  &glB.slider,  &glC.slider };
        for (int i = 0; i < 12; ++i)
            plateKnobs.push_back ({ fx[i], fxX[i], fxCy, fxWedgeR, fxDomeDia * 0.5f });

        // FX knob bounds stay comfortable for dragging while the drawn dome
        // matches the plate seat: sprite side = domeDia / croppedDomeFrac
        const float domeScale = (fxDomeDia / skin::croppedDomeFrac()) / 0.056f;
        for (auto* s : fx)
            s->getProperties().set ("domeScale", domeScale);
    }

    startTimerHz (30);
    setSize (baked ? 1000 : 1000, baked ? 819 : 808);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalGritEditor::~VocalGritEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalGritEditor::setupFxModule (juce::ToggleButton& toggle, const juce::String& toggleId,
                                     Knob& a, const juce::String& aId, const juce::String& aName,
                                     Knob& b, const juce::String& bId, const juce::String& bName,
                                     Knob& c, const juce::String& cId, const juce::String& cName)
{
    toggle.setButtonText ("on");
    addAndMakeVisible (toggle);
    buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, toggleId, toggle));
    a.setup (proc.apvts, aId, aName, *this);
    b.setup (proc.apvts, bId, bName, *this);
    c.setup (proc.apvts, cId, cName, *this);
}

void VocalGritEditor::updateDelaySyncUI()
{
    const bool synced = proc.apvts.getRawParameterValue ("delaySync")->load() > 0.5f;

    // When synced the division box replaces the time readout; on the plate the
    // knob sprite stays put (the box sits in the value row, not over the knob).
    delayDivBox.setVisible (synced);
    delA.slider.setVisible (! synced || chassisImg.isValid());
    delA.value.setVisible  (! synced);
    delA.name.setText (synced ? "div" : "time", juce::dontSendNotification);
}

void VocalGritEditor::setMode (int index)
{
    if (auto* pm = proc.apvts.getParameter ("mode"))
        pm->setValueNotifyingHost (pm->convertTo0to1 ((float) index));
}

void VocalGritEditor::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
    presetName.setText (proc.getProgramName (idx), juce::dontSendNotification);
}

void VocalGritEditor::timerCallback()
{
    // Sync character pill states from the 'mode' parameter.
    if (auto* pm = proc.apvts.getRawParameterValue ("mode"))
    {
        const int mode = (int) pm->load();
        for (int i = 0; i < 4; ++i)
            charButtons[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);
    }
    presetName.setText (proc.getProgramName (proc.getCurrentProgram()), juce::dontSendNotification);

    // Keep the delay sync UI in step with presets / host automation.
    updateDelaySyncUI();

    if (chassisImg.isValid())
    {
        // smooth the meter levels for the plate bars
        auto smooth = [] (float& s, float t)
        {
            if (t > s) s = t;
            else       s += (t - s) * 0.2f;
        };
        smooth (inSm,  proc.inputLevel.load());
        smooth (outSm, proc.outputLevel.load());

        auto dbText = [] (float lvl)
        {
            const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
            return db <= -59.5f ? juce::String ("-inf")
                                : juce::String (db, 1);
        };
        inDbLbl.setText (dbText (inSm),  juce::dontSendNotification);
        outDbLbl.setText (dbText (outSm), juce::dontSendNotification);

        repaint();   // masks + ring wedges + meters all live in paintPlate
        return;
    }

    // advance the glow pulse and repaint just the character row glow band
    pulsePhase += 0.13f;
    if (pulsePhase > juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;

    // Repaint the halo bands every tick so glows animate when on and clear
    // promptly when toggled off (including via presets/automation).
    for (auto& b : charButtons) repaint (b.getBounds().expanded (18));
    for (auto& b : pills)       repaint (b.getBounds().expanded (18));
    for (auto* b : { &doublerOn, &delayOn, &reverbOn, &glitchOn })
        repaint (b->getBounds().expanded (18));
}

//==============================================================================
juce::Point<float> VocalGritEditor::plateXY (float fx, float fy) const
{
    return { fx * (float) getWidth(), fy * (float) getHeight() };
}

juce::Rectangle<int> VocalGritEditor::plateFracRect (float fx, float fy, float fw, float fh) const
{
    return juce::Rectangle<float> (fx * (float) getWidth(),  fy * (float) getHeight(),
                                   fw * (float) getWidth(),  fh * (float) getHeight())
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas
// (the plate fills the whole canvas, so screen fractions map 1:1).
void VocalGritEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
{
    const float iw = (float) chassisOnImg.getWidth(), ih = (float) chassisOnImg.getHeight();
    g.drawImage (chassisOnImg,
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight(),
                 juce::roundToInt (((float) screenRect.getX()      / (float) getWidth())  * iw),
                 juce::roundToInt (((float) screenRect.getY()      / (float) getHeight()) * ih),
                 juce::roundToInt (((float) screenRect.getWidth()  / (float) getWidth())  * iw),
                 juce::roundToInt (((float) screenRect.getHeight() / (float) getHeight()) * ih));
}

// Reveal the lit neon ring around a knob as an annular wedge clipped to the
// value sweep. The dome disc is carved out so the glow never tints the steel.
void VocalGritEditor::drawPlateHalo (juce::Graphics& g, const PlateKnob& k)
{
    const auto c = plateXY (k.cx, k.cy);
    const float R = k.wedgeR * (float) getWidth();

    const float prop = juce::jlimit (0.0f, 1.0f,
                                     (float) k.s->valueToProportionOfLength (k.s->getValue()));

    const float a0 = juce::MathConstants<float>::pi;
    const float a1 = a0 + prop * juce::MathConstants<float>::twoPi;
    const bool ringFull = prop >= 0.995f;
    if (! ringFull && prop <= 0.002f) return;

    const float domeR = k.domeR * (float) getWidth();
    const juce::Rectangle<int> box ((int) std::floor (c.x - R), (int) std::floor (c.y - R),
                                    (int) std::ceil (R * 2.0f), (int) std::ceil (R * 2.0f));

    if (ringFull)
    {
        juce::Path clip;
        clip.addEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
        clip.addEllipse (c.x - domeR, c.y - domeR, domeR * 2.0f, domeR * 2.0f);
        clip.setUsingNonZeroWinding (false);
        g.saveState();
        g.reduceClipRegion (clip);
        maskFromOn (g, box);
        g.restoreState();
        return;
    }

    // annular wedge with a feathered leading edge (never a hard radial cut)
    auto wedge = [&] (float from, float to, float alpha)
    {
        if (to - from <= 0.0005f) return;
        juce::Path p;
        p.addPieSegment (c.x - R, c.y - R, R * 2.0f, R * 2.0f, from, to, domeR / R);
        g.saveState();
        g.reduceClipRegion (p);
        g.setOpacity (alpha);
        maskFromOn (g, box);
        g.restoreState();
    };

    const float feather = juce::jmin (0.22f, (a1 - a0) * 0.5f);
    wedge (a0, a1 - feather, 1.0f);
    constexpr int steps = 5;
    for (int i = 0; i < steps; ++i)
        wedge (a1 - feather * (1.0f - (float) i / steps),
               a1 - feather * (1.0f - (float) (i + 1) / steps),
               1.0f - ((float) i + 0.5f) / steps);
}

void VocalGritEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    // base plate fills the window (the window matches the canvas aspect)
    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    // ---- selector banks: reveal the lit pill for the active states
    if (auto* pm = proc.apvts.getRawParameterValue ("mode"))
    {
        const int mode = juce::jlimit (0, 3, (int) pm->load());
        maskFromOn (g, plateFracRect (charX[mode][0], charY0,
                                      charX[mode][1] - charX[mode][0], charY1 - charY0));
    }
    for (int i = 0; i < 4; ++i)
        if (pills[(size_t) i].getToggleState())
            maskFromOn (g, plateFracRect (texX[i][0], texY0,
                                          texX[i][1] - texX[i][0], texY1 - texY0));

    // ---- FX cards: the divider line + "on" pill light with the module
    const juce::ToggleButton* fxOn[4] = { &doublerOn, &delayOn, &reverbOn, &glitchOn };
    for (int i = 0; i < 4; ++i)
        if (fxOn[i]->getToggleState())
        {
            maskFromOn (g, plateFracRect (divX[i][0], divY0,
                                          divX[i][1] - divX[i][0], divY1 - divY0));
            maskFromOn (g, plateFracRect (onX[i][0], onY0,
                                          onX[i][1] - onX[i][0], onY1 - onY0));
        }
    if (delaySyncBtn.getToggleState())
        maskFromOn (g, plateFracRect (syncX0, onY0, syncX1 - syncX0, onY1 - onY0));

    // ---- meter bars: continuous pink fill, left to right
    auto meterMask = [&] (float y0, float y1, float level)
    {
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float prop = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        if (prop <= 0.004f) return;
        maskFromOn (g, plateFracRect (mX0, y0, (mX1 - mX0) * prop, y1 - y0));
    };
    meterMask (mInY0,  mInY1,  inSm);
    meterMask (mOutY0, mOutY1, outSm);

    // ---- slider fills: vertical bars fill bottom-up, horizontal left-to-right
    auto propOf = [] (juce::Slider& s)
    {
        return juce::jlimit (0.0f, 1.0f,
                             (float) s.valueToProportionOfLength (s.getValue()));
    };
    const juce::Slider* vs[4] = { &driveS.slider, &toneS.slider,
                                  &widthS.slider, &formantS.slider };
    for (int i = 0; i < 4; ++i)
    {
        const float p = propOf (const_cast<juce::Slider&> (*vs[i]));
        if (p <= 0.004f) continue;
        const float yTop = vsY1 - p * (vsY1 - vsY0);
        maskFromOn (g, plateFracRect (vsX[i] - vsHalfW, yTop,
                                      vsHalfW * 2.0f, vsY1 - yTop));
    }
    if (const float p = propOf (mixSlider); p > 0.004f)
        maskFromOn (g, plateFracRect (hX0, mixY0, (hX1 - hX0) * p, mixY1 - mixY0));
    if (const float p = propOf (outputSlider); p > 0.004f)
        maskFromOn (g, plateFracRect (hX0, outY0, (hX1 - hX0) * p, outY1 - outY0));

    // ---- neon ring wedges (value-swept): hero dial + FX knobs
    // (skip hidden sliders, e.g. the delay time knob under the sync box)
    for (const auto& k : plateKnobs)
        if (k.s->isVisible())
            drawPlateHalo (g, k);
}

void VocalGritEditor::paint (juce::Graphics& g)
{
    if (chassisImg.isValid())
    {
        paintPlate (g);
        return;
    }

    theme::backdrop (g, getLocalBounds());

    // Mood: a faint accent bloom behind the hero dial.
    theme::accentBloom (g, { 178.0f, 270.0f }, 240.0f, 0.06f);

    auto card = [&] (juce::Rectangle<int> r, float radius = 18.0f)
    {
        auto rf = r.toFloat();
        theme::elevate (g, rf, radius);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, radius);
        theme::topHighlight (g, rf, radius);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, radius, 1.0f);
    };

    // Wordmark: solid ink "vocal" + accent "grit", with a spaced tagline.
    {
        auto wm = theme::font (27.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", 24, 16, 120, 32, juce::Justification::left);
        g.setColour (theme::accent);
        g.drawText ("grit", 24 + (int) vw, 16, 120, 32, juce::Justification::left);
        // accent underline tick under the wordmark
        g.setColour (theme::accent);
        g.fillRoundedRectangle (24.0f, 45.0f, 22.0f, 2.5f, 1.25f);
    }
    theme::spacedText (g, "VOCAL TEXTURE",
                       juce::Rectangle<float> (54.0f, 41.0f, 220.0f, 14.0f),
                       theme::inkSoft, 9.5f, 2.6f, false, juce::Justification::left);

    // Cards first (so glows can sit on top of card fills, behind the controls).
    card (presetArea, presetArea.getHeight() * 0.5f);
    card (rightCardArea);

    const char* titles[] = { "DOUBLER", "DELAY", "REVERB", "GLITCH" };
    const char* kinds[]  = { "doubler", "delay", "reverb", "glitch" };
    for (int i = 0; i < 4; ++i)
    {
        card (fxAreas[(size_t) i]);
        auto head = fxAreas[(size_t) i].reduced (16, 12).removeFromTop (18).toFloat();
        auto iconBox = head.removeFromLeft (16.0f);
        theme::moduleIcon (g, kinds[i], iconBox.withSizeKeepingCentre (15.0f, 15.0f), theme::accent);
        theme::spacedText (g, titles[i], head.withTrimmedLeft (7.0f),
                           theme::ink, 11.5f, 2.2f, true, juce::Justification::left);
        // dotted divider under the header
        auto fa = fxAreas[(size_t) i];
        theme::dottedDivider (g, (float) fa.getX() + 16.0f, (float) fa.getRight() - 16.0f,
                              (float) fa.getY() + 38.0f, theme::cardLine);
    }

    // Glow behind active toggles (pulsing) — drawn after cards, behind controls.
    const float pulse = 0.5f + 0.5f * std::sin (pulsePhase);
    auto pillGlow = [&] (juce::Component& b, float base, float amp, int radius)
    {
        juce::Path p;
        p.addRoundedRectangle (b.getBounds().toFloat(), b.getHeight() * 0.5f);
        theme::glowPath (g, p, base + amp * pulse, radius);
    };

    for (auto& b : charButtons)
        if (b.getToggleState()) pillGlow (b, 0.22f, 0.22f, 20);

    for (auto& b : pills)
        if (b.getToggleState()) pillGlow (b, 0.16f, 0.18f, 16);

    for (auto* b : { &doublerOn, &delayOn, &reverbOn, &glitchOn })
        if (b->getToggleState()) pillGlow (*b, 0.14f, 0.16f, 14);
}

void VocalGritEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (chassisImg.isValid())
    {
        using namespace plategeo;
        const float W = (float) getWidth();
        const float H = (float) getHeight();

        // centred square for a dome sprite, sized so the visible dome (a
        // measured fraction of the sprite canvas) matches the plate's seat
        auto knobRect = [&] (float cx, float cy, float sideFracW)
        {
            const int side = juce::roundToInt (sideFracW * W);
            const auto c = plateXY (cx, cy);
            return juce::Rectangle<int> (juce::roundToInt (c.x - side * 0.5f),
                                         juce::roundToInt (c.y - side * 0.5f), side, side);
        };

        // hero dial: sprite dome dia == plate seat dia (the sprite was
        // generated in place), so bounds = domeDia / croppedDomeFrac
        gritDial.setBounds (knobRect (dialCx, dialCy,
                                      dialDomeDia / skin::croppedDomeFrac()));

        // preset arrows (baked chrome) + name between them
        presetPrev.setBounds (knobRect (presetArrowL, presetCy, 0.050f));
        presetNext.setBounds (knobRect (presetArrowR, presetCy, 0.050f));
        presetName.setBounds (plateFracRect (0.420f, 0.075f, 0.176f, 0.050f));

        // vertical sliders: bounds sized so the JUCE thumb travel (inset by
        // the fixed 12px V4 thumb radius) matches the baked track exactly
        const int tr = 12;   // LookAndFeel_V4 thumb radius (jmin 12)
        const std::pair<VSlider*, float> verts[] = {
            { &driveS, vsX[0] }, { &toneS, vsX[1] },
            { &widthS, vsX[2] }, { &formantS, vsX[3] } };
        for (auto& [s, cx] : verts)
        {
            s->slider.setBounds (juce::roundToInt (cx * W - 14.0f),
                                 juce::roundToInt (vsY0 * H) - tr,
                                 28,
                                 juce::roundToInt ((vsY1 - vsY0) * H) + tr * 2);
            s->value.setBounds (plateFracRect (cx - 0.040f, 0.176f, 0.080f, 0.036f));
        }

        // mix / output horizontal sliders + pink readouts in the caption rows
        mixSlider.setBounds    (juce::roundToInt (hX0 * W) - tr,
                                juce::roundToInt (mixCy * H - 14.0f),
                                juce::roundToInt ((hX1 - hX0) * W) + tr * 2, 28);
        outputSlider.setBounds (juce::roundToInt (hX0 * W) - tr,
                                juce::roundToInt (outCy * H - 14.0f),
                                juce::roundToInt ((hX1 - hX0) * W) + tr * 2, 28);
        mixValue.setBounds    (plateFracRect (0.830f, 0.370f, 0.102f, 0.024f));
        outputValue.setBounds (plateFracRect (0.830f, 0.516f, 0.102f, 0.024f));

        // live dB readouts to the right of the meter bars
        inDbLbl.setBounds  (plateFracRect (0.893f, mInY0 - 0.008f,  0.062f, 0.031f));
        outDbLbl.setBounds (plateFracRect (0.893f, mOutY0 - 0.008f, 0.062f, 0.031f));

        // selector banks + FX pills: invisible hit areas over the baked pills
        for (int i = 0; i < 4; ++i)
        {
            charButtons[(size_t) i].setBounds (plateFracRect (charX[i][0], charY0,
                                                              charX[i][1] - charX[i][0],
                                                              charY1 - charY0));
            pills[(size_t) i].setBounds (plateFracRect (texX[i][0], texY0,
                                                        texX[i][1] - texX[i][0],
                                                        texY1 - texY0));
        }
        juce::ToggleButton* togs[4] = { &doublerOn, &delayOn, &reverbOn, &glitchOn };
        for (int i = 0; i < 4; ++i)
            togs[i]->setBounds (plateFracRect (onX[i][0], onY0,
                                               onX[i][1] - onX[i][0], onY1 - onY0));
        delaySyncBtn.setBounds (plateFracRect (syncX0, onY0, syncX1 - syncX0, onY1 - onY0));

        // FX knobs + live values under the baked captions
        Knob* fxK[12] = { &dblA, &dblB, &dblC, &delA, &delB, &delC,
                          &revA, &revB, &revC, &glA, &glB, &glC };
        for (int i = 0; i < 12; ++i)
        {
            fxK[i]->slider.setBounds (knobRect (fxX[i], fxCy, 0.056f));
            fxK[i]->value.setBounds (plateFracRect (fxX[i] - 0.030f, fxValY0,
                                                    0.060f, fxValY1 - fxValY0));
        }

        // division box lands on the time knob's value row when synced
        delayDivBox.setBounds (plateFracRect (fxX[3] - 0.032f, fxValY0 - 0.004f,
                                              0.064f, fxValY1 - fxValY0 + 0.008f));
        return;
    }

    auto content = getLocalBounds().reduced (24);

    // Header (wordmark drawn in paint; preset box centred)
    auto header = content.removeFromTop (52);
    presetArea = header.withSizeKeepingCentre (320, 46);
    {
        auto pb = presetArea;
        presetPrev.setBounds (pb.removeFromLeft (46));
        presetNext.setBounds (pb.removeFromRight (46));
        presetName.setBounds (pb);
    }
    content.removeFromTop (16);

    // FX row at the bottom
    auto fxRow = content.removeFromBottom (176);
    {
        const int gap = 14;
        const int cardW = (fxRow.getWidth() - gap * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto cardR = fxRow.removeFromLeft (i < 3 ? cardW : fxRow.getWidth());
            if (i < 3) fxRow.removeFromLeft (gap);
            fxAreas[(size_t) i] = cardR;
        }

        auto layoutFx = [] (juce::Rectangle<int> cardR, juce::ToggleButton& toggle,
                            Knob& a, Knob& b, Knob& c)
        {
            auto inner = cardR.reduced (14, 12);
            auto top = inner.removeFromTop (24);
            toggle.setBounds (top.removeFromRight (62));
            inner.removeFromTop (6);
            const int kw = inner.getWidth() / 3;
            a.layout (inner.removeFromLeft (kw));
            b.layout (inner.removeFromLeft (kw));
            c.layout (inner);
        };
        layoutFx (fxAreas[0], doublerOn, dblA, dblB, dblC);
        layoutFx (fxAreas[2], reverbOn,  revA, revB, revC);
        layoutFx (fxAreas[3], glitchOn,  glA,  glB,  glC);

        // Delay card: regular knob layout + a compact SYNC toggle in the top
        // row and a division selector that overlays the time slot when synced.
        {
            auto inner = fxAreas[1].reduced (14, 12);
            auto top = inner.removeFromTop (24);
            delayOn.setBounds (top.removeFromRight (62));
            top.removeFromRight (8);
            delaySyncBtn.setBounds (top.removeFromRight (66));
            inner.removeFromTop (6);
            const int kw = inner.getWidth() / 3;
            auto timeCell = inner.removeFromLeft (kw);
            delA.layout (timeCell);
            auto box = timeCell;
            box.removeFromTop (15);     // name row
            box.removeFromBottom (16);  // value row
            delayDivBox.setBounds (box.withSizeKeepingCentre (
                box.getWidth() - 2, juce::jmin (26, box.getHeight())));
            delB.layout (inner.removeFromLeft (kw));
            delC.layout (inner);
        }
    }
    content.removeFromBottom (16);

    // Right card (meters + mix)
    rightCardArea = content.removeFromRight (300);
    content.removeFromRight (22);
    {
        auto rc = rightCardArea.reduced (22);
        inMeterLabel.setBounds (rc.removeFromTop (20));
        inMeter.setBounds (rc.removeFromTop (28));
        rc.removeFromTop (20);
        outMeterLabel.setBounds (rc.removeFromTop (20));
        outMeter.setBounds (rc.removeFromTop (28));
        rc.removeFromTop (30);
        auto mixHeader = rc.removeFromTop (24);
        mixLabel.setBounds (mixHeader.removeFromLeft (mixHeader.getWidth() / 2));
        mixValue.setBounds (mixHeader);
        rc.removeFromTop (4);
        mixSlider.setBounds (rc.removeFromTop (30));
        rc.removeFromTop (4);
        auto cap = rc.removeFromTop (18);
        rawLabel.setBounds (cap.removeFromLeft (cap.getWidth() / 2));
        procLabel.setBounds (cap);

        rc.removeFromTop (24);
        auto outHeader = rc.removeFromTop (24);
        outputLabel.setBounds (outHeader.removeFromLeft (outHeader.getWidth() / 2));
        outputValue.setBounds (outHeader);
        rc.removeFromTop (4);
        outputSlider.setBounds (rc.removeFromTop (30));
    }

    // Dial + vertical sliders
    auto dialAndSliders = content.removeFromTop (300);
    gritDial.setBounds (dialAndSliders.removeFromLeft (300));
    dialAndSliders.removeFromLeft (10);
    {
        auto sliders = dialAndSliders;
        const int sw = sliders.getWidth() / 4;
        driveS  .layout (sliders.removeFromLeft (sw));
        toneS   .layout (sliders.removeFromLeft (sw));
        widthS  .layout (sliders.removeFromLeft (sw));
        formantS.layout (sliders);
    }

    content.removeFromTop (8);

    // Character buttons
    auto charRow = content.removeFromTop (52);
    {
        const int gap = 12;
        const int bw = (charRow.getWidth() - gap * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            charButtons[(size_t) i].setBounds (charRow.removeFromLeft (bw).reduced (0, 6));
            if (i < 3) charRow.removeFromLeft (gap);
        }
    }

    content.removeFromTop (12);

    // Texture pills
    auto pillRow = content.removeFromTop (52);
    {
        const int gap = 12;
        const int bw = (pillRow.getWidth() - gap * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            pills[(size_t) i].setBounds (pillRow.removeFromLeft (bw).reduced (0, 6));
            if (i < 3) pillRow.removeFromLeft (gap);
        }
    }
}
