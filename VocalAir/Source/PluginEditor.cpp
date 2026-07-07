#include "PluginEditor.h"

//==============================================================================
// Baked-plate geometry. The chassis PNGs are 2048x1360 and the plate fills the
// whole canvas; x/w are fractions of the image width, y/h of its height.
// Ring centres come from least-squares fits of the hot neon cores on the ON
// plate (residual ~1px); mask rects from ON-vs-OFF diff bounds plus margin.
namespace plategeo
{
    // hero rings: hot band r=[0.078,0.088], bloom solid to ~0.096, gone by 0.125
    constexpr float ringLCx = 0.3045f, ringRCx = 0.6903f, ringCy = 0.6390f;
    constexpr float ringSolidR = 0.096f, ringMaxR = 0.125f;
    constexpr float heroDomeDia = 0.121f;          // steel edge/ring ratio 0.65

    // trim ring (bottom right)
    constexpr float trimCx = 0.9175f, trimCy = 0.8559f;
    constexpr float trimSolidR = 0.029f, trimMaxR = 0.039f;
    constexpr float trimDomeDia = 0.0332f;

    // meter tick arc: circle fit of the lit ticks; centre sits below the card.
    // Angles are radians clockwise from 12 o'clock (JUCE pie convention).
    constexpr float arcCx = 0.5030f, arcCy = 1.0315f;
    constexpr float arcA0 = -0.7174f, arcA1 = 0.6789f;
    constexpr float arcSolid0 = 0.525f, arcSolid1 = 0.570f;   // tick band
    constexpr float arcFade0  = 0.505f, arcFade1  = 0.590f;   // radial feather

    // display card inner rect — every meter reveal is clipped to it
    constexpr float cardX0 = 0.0601f, cardY0 = 0.1412f, cardX1 = 0.9385f, cardY1 = 0.4213f;

    // feathered mask rects (x, y, w, h)
    constexpr float linkRect[4]  = { 0.4472f, 0.5966f, 0.0911f, 0.0658f };
    constexpr float aRect[4]     = { 0.6100f, 0.0600f, 0.0497f, 0.0550f };
    constexpr float bRect[4]     = { 0.6599f, 0.0600f, 0.0451f, 0.0550f };
    constexpr float powerRect[4] = { 0.0243f, 0.7824f, 0.0803f, 0.1622f };

    // top-bar hit-area centres (baked chrome glyph centroids), all on y=0.0875
    constexpr float undoCx = 0.532f, redoCx = 0.577f, topCy = 0.0875f;
    constexpr float abACx = 0.629f, abArrowCx = 0.6575f, abBCx = 0.686f;
    constexpr float saveCx = 0.8887f, menuCx = 0.9248f;

    // preset dropdown pill (baked, chevron included)
    constexpr float boxX0 = 0.718f, boxY0 = 0.064f, boxX1 = 0.868f, boxY1 = 0.113f;

    // live value labels: the seat rim shadow ends at y=0.785 and the baked
    // caption starts at 0.834 — the glyph core must centre on 0.8095. The rect
    // is offset up because a label renders its digit core ~0.012H below the
    // rect centre at this font size (measured, not assumed).
    constexpr float valY0 = 0.7737f, valY1 = 0.8497f;

    // trim dB readout, centred under the trim knob assembly
    constexpr float trimValY0 = 0.900f, trimValY1 = 0.950f;
}

//==============================================================================
VocalAirEditor::VocalAirEditor (VocalAirProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("air-chassis@2x.png");
    chassisOnImg = skin::image ("air-chassis-on@2x.png");
    const bool baked = chassisImg.isValid() && chassisOnImg.isValid();
    laf.plate = baked;
    if (baked)
        plateCrop = skin::plateBounds (chassisImg);

    // ---- branding ----
    // The wordmark, sub-tagline and display label are painted directly (two-tone
    // wordmark + underline, letter-spaced small caps) in paint(); the Label
    // components are kept purely as layout anchors.
    addAndMakeVisible (brand);
    addAndMakeVisible (brandSub);
    addAndMakeVisible (displayLabel);

    // ---- top-bar icon buttons ----
    undoBtn.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::undo (g, r, c); });
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    addAndMakeVisible (undoBtn);

    redoBtn.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::redo (g, r, c); });
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    addAndMakeVisible (redoBtn);

    saveBtn.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::save (g, r, c); });
    saveBtn.onClick = [this]
    {
        // Stamp the current settings into the active A/B slot.
        abState[abSlot] = proc.apvts.copyState();
    };
    addAndMakeVisible (saveBtn);

    menuBtn.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::hamburger (g, r, c); });
    menuBtn.onClick = [this]
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuBtn),
                         [this] (int r) { if (r > 0) proc.setCurrentProgram (r - 1); });
    };
    addAndMakeVisible (menuBtn);

    // ---- A/B compare ----
    abState[0] = proc.apvts.copyState();
    abState[1] = proc.apvts.copyState();

    for (auto* b : { &abA, &abB })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*b);
    }
    abA.setToggleState (true, juce::dontSendNotification);
    abA.onClick = [this] { selectABSlot (0); };
    abB.onClick = [this] { selectABSlot (1); };

    abArrow.setDrawer ([] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
                       { icons::chevron (g, r, c); });
    abArrow.onClick = [this]
    {
        // copy the active slot's live settings into the other slot
        abState[1 - abSlot] = proc.apvts.copyState();
    };
    addAndMakeVisible (abArrow);

    // ---- preset combo ----
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram())
            proc.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);

    // ---- meter ----
    addAndMakeVisible (meter);

    // ---- big knobs ----
    midKnob.setCaption ("mid air");
    highKnob.setCaption ("high air");
    addAndMakeVisible (midKnob);
    addAndMakeVisible (highKnob);

    midAtt  = std::make_unique<SliderAtt> (proc.apvts, "midAir",  midKnob);
    highAtt = std::make_unique<SliderAtt> (proc.apvts, "highAir", highKnob);

    midKnob.onValueChange  = [this] { mirrorLink (true);  };
    highKnob.onValueChange = [this] { mirrorLink (false); };

    auto setupRange = [this] (juce::Label& l, const juce::String& txt)
    {
        l.setText (txt, juce::dontSendNotification);
        l.setFont (theme::font (13.0f, false));
        l.setColour (juce::Label::textColourId, theme::inkSoft);
        addAndMakeVisible (l);
    };
    setupRange (midLo,  "0");   midLo.setJustificationType (juce::Justification::centredLeft);
    setupRange (midHi,  "100"); midHi.setJustificationType (juce::Justification::centredRight);
    setupRange (highLo, "0");   highLo.setJustificationType (juce::Justification::centredLeft);
    setupRange (highHi, "100"); highHi.setJustificationType (juce::Justification::centredRight);

    // ---- link pill ----
    linkBtn.setClickingTogglesState (true);
    linkBtn.setDrawer ([this] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour)
    {
        const bool on = linkBtn.getToggleState();
        auto rr = r.reduced (1.0f);
        const float radius = rr.getHeight() * 0.5f;
        if (on)
        {
            g.setColour (theme::accentSoft);
            g.fillRoundedRectangle (rr.expanded (3.0f), radius + 3.0f);
            g.setColour (theme::accent);
            g.fillRoundedRectangle (rr, radius);
        }
        else
        {
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (rr, radius);
            g.setColour (theme::cardLine);
            g.drawRoundedRectangle (rr, radius, 1.2f);
        }

        const juce::Colour fg = on ? juce::Colours::white : theme::ink;
        const float ih = rr.getHeight() * 0.5f;
        auto iconR = juce::Rectangle<float> (rr.getX() + 14.0f, rr.getCentreY() - ih * 0.5f, ih * 1.4f, ih);
        icons::chain (g, iconR, fg, 1.5f);

        g.setColour (fg);
        g.setFont (theme::font (rr.getHeight() * 0.42f, false));
        g.drawText ("link",
                    juce::Rectangle<float> (iconR.getRight() + 4.0f, rr.getY(),
                                            rr.getRight() - iconR.getRight() - 14.0f, rr.getHeight()),
                    juce::Justification::centredLeft);
    });
    linkAtt = std::make_unique<ButtonAtt> (proc.apvts, "link", linkBtn);
    addAndMakeVisible (linkBtn);

    // ---- power ----
    powerLabel.setText ("power", juce::dontSendNotification);
    powerLabel.setFont (theme::font (13.0f, false));
    powerLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    addAndMakeVisible (powerLabel);

    powerBtn.setClickingTogglesState (true);
    powerBtn.setDrawer ([this] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour)
    {
        const bool on = powerBtn.getToggleState();
        auto d = r.reduced (4.0f);
        const float radius = juce::jmin (d.getWidth(), d.getHeight()) * 0.5f;
        const auto c = d.getCentre();

        juce::Rectangle<float> disc (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);

        // soft cast shadow kept within the button bounds
        juce::Path sp; sp.addEllipse (disc.translated (0.0f, radius * 0.12f));
        juce::DropShadow (juce::Colours::black.withAlpha (0.16f),
                          (int) juce::jlimit (4.0f, 8.0f, radius * 0.4f), { 0, 2 }).drawForPath (g, sp);

        // white domed face
        AirKnob::paintDome (g, disc);
        if (on)
        {
            g.setColour (theme::accent.withAlpha (0.85f));
            g.drawEllipse (disc.reduced (0.6f), 1.6f);
        }

        icons::power (g, d, on ? theme::accent : theme::inkSoft, 2.0f);
    });
    powerAtt = std::make_unique<ButtonAtt> (proc.apvts, "power", powerBtn);
    addAndMakeVisible (powerBtn);

    // ---- trim ----
    trimLabel.setText ("trim", juce::dontSendNotification);
    trimLabel.setFont (theme::font (13.0f, false));
    trimLabel.setColour (juce::Label::textColourId, theme::inkSoft);
    trimLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (trimLabel);

    addAndMakeVisible (trimKnob);
    trimAtt = std::make_unique<SliderAtt> (proc.apvts, "trim", trimKnob);

    // ------------------------------------------------------------------
    // Baked plate: everything static lives on the chassis image. Buttons
    // become invisible hit areas, knobs paint steel dome sprites, and lit
    // states (rings, ticks, pills) are masked from the ON plate.
    if (baked)
    {
        midKnob.setPlateMode (true);
        highKnob.setPlateMode (true);
        trimKnob.setPlateMode (true);

        // icon buttons: the chrome art is baked — drop the drawers so they
        // paint nothing but stay clickable
        for (auto* ib : { &undoBtn, &redoBtn, &saveBtn, &menuBtn, &abArrow,
                          &linkBtn, &powerBtn })
            ib->setDrawer (nullptr);

        for (auto* b : { &abA, &abB })
        {
            b->setComponentID ("hit");
            b->setButtonText ({});
        }

        // captions / range labels / brand text are baked into the plate
        for (auto* c : std::initializer_list<juce::Component*> {
                 &midLo, &midHi, &highLo, &highHi, &powerLabel, &trimLabel,
                 &brand, &brandSub, &displayLabel })
            c->setVisible (false);

        meter.setVisible (false);   // revealed from the ON plate instead

        for (auto* v : { &midVal, &highVal })
        {
            v->setJustificationType (juce::Justification::centred);
            v->setFont (theme::font (26.0f, false));
            v->setColour (juce::Label::textColourId, theme::ink);
            v->setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*v);
        }

        trimVal.setJustificationType (juce::Justification::centred);
        trimVal.setFont (theme::font (12.0f, false));
        trimVal.setColour (juce::Label::textColourId, theme::inkSoft);
        trimVal.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (trimVal);
    }

    startTimerHz (30);
    // plate mode: the window shows ONLY the plate (cropped at the chrome edge),
    // and must match the crop's aspect exactly or circular dome sprites sit in
    // vertically-stretched (elliptical) baked grooves
    if (baked)
        setSize (1024, juce::roundToInt (1024.0f * (float) plateCrop.getHeight()
                                                 / (float) plateCrop.getWidth()));
    else
        setSize (1024, 640);

    // License overlay sits on top of everything; it shows itself until activated.
    addChildComponent (licenseOverlay);
    licenseOverlay.setBounds (getLocalBounds());
}

VocalAirEditor::~VocalAirEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void VocalAirEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void VocalAirEditor::selectABSlot (int slot)
{
    if (slot == abSlot) return;
    abState[abSlot] = proc.apvts.copyState();   // remember the slot we are leaving
    abSlot = slot;
    proc.apvts.replaceState (abState[abSlot].createCopy());

    abA.setToggleState (abSlot == 0, juce::dontSendNotification);
    abB.setToggleState (abSlot == 1, juce::dontSendNotification);
}

void VocalAirEditor::mirrorLink (bool fromMid)
{
    if (linking) return;
    if (! linkBtn.getToggleState()) return;

    linking = true;
    if (fromMid)
    {
        if (auto* h = proc.apvts.getParameter ("highAir"))
            h->setValueNotifyingHost (h->getNormalisableRange().convertTo0to1 ((float) midKnob.getValue()));
    }
    else
    {
        if (auto* m = proc.apvts.getParameter ("midAir"))
            m->setValueNotifyingHost (m->getNormalisableRange().convertTo0to1 ((float) highKnob.getValue()));
    }
    linking = false;
}

//==============================================================================
void VocalAirEditor::timerCallback()
{
    meter.setLevel (proc.getOutputLevel());

    const int cur = proc.getCurrentProgram();
    if (presetBox.getSelectedId() != cur + 1)
        presetBox.setSelectedId (cur + 1, juce::dontSendNotification);

    if (chassisImg.isValid())
    {
        midVal.setText (juce::String (juce::roundToInt (midKnob.getValue())),
                        juce::dontSendNotification);
        highVal.setText (juce::String (juce::roundToInt (highKnob.getValue())),
                         juce::dontSendNotification);

        const double tv = trimKnob.getValue();
        trimVal.setText ((tv > 0.049 ? "+" : "") + juce::String (tv, 1) + " dB",
                         juce::dontSendNotification);

        // dirty-region repaints: only invalidate what actually changed this tick
        using namespace plategeo;
        // radius fractions are of image WIDTH; vertical extents need the aspect factor
        const float ar = (float) chassisImg.getWidth() / (float) chassisImg.getHeight();
        auto ringBox = [&] (float cx, float cy, float maxR)
        {
            return plateFracRect (cx - maxR, cy - maxR * ar,
                                  maxR * 2.0f, maxR * 2.0f * ar).expanded (2);
        };
        struct KnobRegion { juce::Slider* s; float cx, cy, maxR; };
        const KnobRegion regions[] = {
            { &midKnob,  ringLCx, ringCy, ringMaxR },
            { &highKnob, ringRCx, ringCy, ringMaxR },
            { &trimKnob, trimCx,  trimCy, trimMaxR },
        };
        for (size_t i = 0; i < 3; ++i)
        {
            const double v = regions[i].s->getValue();
            if (v != shownKnob[i])
            {
                shownKnob[i] = v;
                repaint (ringBox (regions[i].cx, regions[i].cy, regions[i].maxR));
            }
        }

        const int fpx = juce::roundToInt ((float) getWidth() * 0.010f) + 2;
        if (abSlot != shownAB)
        {
            shownAB = abSlot;
            repaint (plateFracRect (aRect[0], aRect[1], aRect[2], aRect[3]).expanded (fpx));
            repaint (plateFracRect (bRect[0], bRect[1], bRect[2], bRect[3]).expanded (fpx));
        }
        if (linkBtn.getToggleState() != shownLink)
        {
            shownLink = linkBtn.getToggleState();
            repaint (plateFracRect (linkRect[0], linkRect[1], linkRect[2], linkRect[3]).expanded (fpx));
        }
        if (powerBtn.getToggleState() != shownPower)
        {
            shownPower = powerBtn.getToggleState();
            repaint (plateFracRect (powerRect[0], powerRect[1], powerRect[2], powerRect[3]).expanded (fpx));
        }

        // the meter animates continuously — repaint only the display card
        repaint (plateFracRect (cardX0, cardY0, cardX1 - cardX0, cardY1 - cardY0).expanded (2));
    }
}

//==============================================================================
// plategeo fractions are of the FULL generated canvas; the window shows only
// the plateCrop region, so map full-canvas fraction -> cropped screen px.
juce::Rectangle<int> VocalAirEditor::plateFracRect (float fx, float fy, float fw, float fh) const
{
    const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
    const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
    const float sy = (float) getHeight() / (float) plateCrop.getHeight();
    return juce::Rectangle<float> ((fx * iw - (float) plateCrop.getX()) * sx,
                                   (fy * ih - (float) plateCrop.getY()) * sy,
                                   fw * iw * sx,
                                   fh * ih * sy).toNearestInt();
}

// Blit the matching region of the lit plate over the base plate. Both scaled
// caches share the editor's coordinate space, so this is a 1:1 copy — cheap,
// and registration is exact by construction.
void VocalAirEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
{
    g.drawImage (plateOnScaled,
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight(),
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight());
}

// Same reveal but with a soft alpha ramp along the rect border, so the slight
// global tone drift between the two plates never shows as a hard rectangle.
void VocalAirEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
                                          int featherPx)
{
    constexpr int n = 4;
    const float s = (float) featherPx / (float) n;
    const auto r = screenRect.toFloat();

    g.saveState();
    g.reduceClipRegion (r.reduced ((float) featherPx).toNearestInt());
    maskFromOn (g, screenRect);
    g.restoreState();

    for (int j = 0; j < n; ++j)     // j = 0 is the outermost, faintest band
    {
        juce::Path band;
        band.addRectangle (r.reduced (s * (float) j));
        band.addRectangle (r.reduced (s * (float) (j + 1)));
        band.setUsingNonZeroWinding (false);
        g.saveState();
        g.reduceClipRegion (band);
        g.setOpacity (((float) j + 0.5f) / (float) n);
        maskFromOn (g, screenRect);
        g.restoreState();
    }
}

// Reveal a knob's lit neon ring as an annular wedge clipped to the value sweep
// (6 o'clock -> 6 o'clock), with an angular feather on the leading edge and a
// radial fade on the outer bloom so no hard cut ever shows.
void VocalAirEditor::drawRingWedge (juce::Graphics& g, juce::Slider& s, float cxFrac, float cyFrac,
                                    float domeRFrac, float solidRFrac, float maxRFrac)
{
    // fractions are of the full canvas; convert through the crop mapping
    const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
    const float sx = (float) getWidth() / (float) plateCrop.getWidth();
    const juce::Point<float> c ((cxFrac * iw - (float) plateCrop.getX()) * sx,
                                (cyFrac * ih - (float) plateCrop.getY())
                                    * (float) getHeight() / (float) plateCrop.getHeight());
    const float domeR  = domeRFrac  * iw * sx;
    const float solidR = solidRFrac * iw * sx;
    const float R      = maxRFrac   * iw * sx;

    const float prop = juce::jlimit (0.0f, 1.0f,
                                     (float) s.valueToProportionOfLength (s.getValue()));
    const float a0 = juce::MathConstants<float>::pi;
    const bool full = prop >= 0.995f;
    if (! full && prop <= 0.002f) return;
    const float a1 = a0 + prop * juce::MathConstants<float>::twoPi;

    const juce::Rectangle<int> box ((int) std::floor (c.x - R), (int) std::floor (c.y - R),
                                    (int) std::ceil (R * 2.0f), (int) std::ceil (R * 2.0f));

    auto wedge = [&] (float from, float to, float rIn, float rOut, float alpha)
    {
        if (to - from <= 0.0005f || rOut - rIn <= 0.5f) return;
        juce::Path p;
        p.addPieSegment (c.x - rOut, c.y - rOut, rOut * 2.0f, rOut * 2.0f, from, to, rIn / rOut);
        g.saveState();
        g.reduceClipRegion (p);
        g.setOpacity (alpha);
        maskFromOn (g, box);
        g.restoreState();
    };

    // both ends of the arc are feathered so there is never a hard radial cut:
    // fIn fades the start (6 o'clock) in, fOut fades the leading edge out. At
    // full value the ring is a seamless uninterrupted annulus.
    const float span = a1 - a0;
    const float fOut = full ? 0.0f : juce::jmin (0.22f, span * 0.40f);
    const float fIn  = full ? 0.0f : juce::jmin (0.10f, span * 0.20f);
    const float aEnd = full ? a0 + juce::MathConstants<float>::twoPi : a1;

    wedge (a0 + fIn, aEnd - fOut, domeR, solidR, 1.0f);
    constexpr int aSteps = 10;
    for (int i = 0; i < aSteps; ++i)
    {
        const float t0 = (float) i / aSteps, t1 = (float) (i + 1) / aSteps;
        // leading edge fade-out
        wedge (aEnd - fOut * (1.0f - t0), aEnd - fOut * (1.0f - t1),
               domeR, solidR, 1.0f - (t0 + t1) * 0.5f);
        // start edge fade-in
        wedge (a0 + fIn * t0, a0 + fIn * t1,
               domeR, solidR, (t0 + t1) * 0.5f);
    }

    // outer bloom band, faded radially and softened at both angular ends
    constexpr int rSteps = 4;
    for (int i = 0; i < rSteps; ++i)
    {
        const float alpha = 0.85f * (1.0f - ((float) i + 0.5f) / rSteps);
        const float rIn  = solidR + (R - solidR) * (float) i / rSteps;
        const float rOut = solidR + (R - solidR) * (float) (i + 1) / rSteps;
        wedge (a0 + fIn, aEnd - fOut * 0.5f, rIn, rOut, alpha);
        wedge (a0,            a0 + fIn * 0.5f, rIn, rOut, alpha * 0.33f);
        wedge (a0 + fIn * 0.5f, a0 + fIn,      rIn, rOut, alpha * 0.66f);
    }
}

// Reveal the lit tick arc in the display up to the current meter position.
// The arc's circle centre sits below the card; everything is clipped to the
// card's inner rect so the reveal can never spill onto the plate.
void VocalAirEditor::drawMeterArc (juce::Graphics& g)
{
    using namespace plategeo;
    // fractions are of the full canvas; convert through the crop mapping
    const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
    const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
    const float sy = (float) getHeight() / (float) plateCrop.getHeight();
    const juce::Point<float> c ((arcCx * iw - (float) plateCrop.getX()) * sx,
                                (arcCy * ih - (float) plateCrop.getY()) * sy);

    const float t = meter.getT();
    if (t <= 0.004f) return;

    const auto card = plateFracRect (cardX0, cardY0, cardX1 - cardX0, cardY1 - cardY0);
    const float aTo = arcA0 + t * (arcA1 - arcA0);
    const float feather = juce::jmin (0.10f, (aTo - arcA0) * 0.5f);

    auto wedge = [&] (float from, float to, float rInF, float rOutF, float alpha)
    {
        if (to - from <= 0.0005f) return;
        const float rIn = rInF * iw * sx, rOut = rOutF * iw * sx;
        juce::Path p;
        p.addPieSegment (c.x - rOut, c.y - rOut, rOut * 2.0f, rOut * 2.0f, from, to, rIn / rOut);
        g.saveState();
        g.reduceClipRegion (p);
        g.reduceClipRegion (card);
        g.setOpacity (alpha);
        maskFromOn (g, card);
        g.restoreState();
    };

    auto band = [&] (float from, float to, float alpha)
    {
        // solid tick band + radial fades either side (display tone drifts a bit)
        wedge (from, to, arcSolid0, arcSolid1, alpha);
        wedge (from, to, arcFade0,  arcSolid0, alpha * 0.45f);
        wedge (from, to, arcSolid1, arcFade1,  alpha * 0.45f);
    };

    band (arcA0, aTo - feather, 1.0f);
    constexpr int steps = 6;
    for (int i = 0; i < steps; ++i)
        band (aTo - feather * (1.0f - (float) i / steps),
              aTo - feather * (1.0f - (float) (i + 1) / steps),
              1.0f - ((float) i + 0.5f) / steps);
}

void VocalAirEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImageAt (plateScaled, 0, 0);   // cached 1:1 blit — no per-frame rescale

    const int fpx = juce::roundToInt ((float) getWidth() * 0.010f);

    // active A/B slot lights up
    if (abSlot == 0)
        maskFromOnFeathered (g, plateFracRect (aRect[0], aRect[1], aRect[2], aRect[3]), fpx);
    else
        maskFromOnFeathered (g, plateFracRect (bRect[0], bRect[1], bRect[2], bRect[3]), fpx);

    if (linkBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (linkRect[0], linkRect[1], linkRect[2], linkRect[3]), fpx);

    if (powerBtn.getToggleState())
        maskFromOnFeathered (g, plateFracRect (powerRect[0], powerRect[1], powerRect[2], powerRect[3]), fpx);

    drawMeterArc (g);

    drawRingWedge (g, midKnob,  ringLCx, ringCy, heroDomeDia * 0.5f, ringSolidR, ringMaxR);
    drawRingWedge (g, highKnob, ringRCx, ringCy, heroDomeDia * 0.5f, ringSolidR, ringMaxR);
    drawRingWedge (g, trimKnob, trimCx,  trimCy, trimDomeDia * 0.5f, trimSolidR, trimMaxR);
}

//==============================================================================
void VocalAirEditor::paint (juce::Graphics& g)
{
    if (chassisImg.isValid())
    {
        paintPlate (g);
        return;
    }

    // warm off-white gradient backdrop + soft top light
    theme::backdrop (g, getLocalBounds());

    // the wide "air display" floating card
    {
        auto rf = displayCard.toFloat();
        theme::elevate (g, rf, 18.0f);
        g.setColour (theme::card);
        g.fillRoundedRectangle (rf, 18.0f);
        theme::topHighlight (g, rf, 18.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (rf, 18.0f, 1.0f);
    }

    // two-tone wordmark: ink "vocal" + accent "air", with an accent underline.
    {
        auto b = brand.getBounds().toFloat();
        auto wm = theme::font (24.0f, true);
        g.setFont (wm);
        const float vw = juce::GlyphArrangement::getStringWidth (wm, "vocal");
        g.setColour (theme::ink);
        g.drawText ("vocal", b, juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.drawText ("air", b.withTrimmedLeft (vw), juce::Justification::centredLeft);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (b.getX(), b.getBottom() - 5.0f, 22.0f, 2.5f, 1.25f);
    }

    // letter-spaced sub-tagline
    theme::spacedText (g, "VOCAL AIR", brandSub.getBounds().toFloat(),
                       theme::inkSoft, 9.5f, 2.4f, false, juce::Justification::left);

    // letter-spaced card label
    theme::spacedText (g, "AIR DISPLAY", displayLabel.getBounds().toFloat(),
                       theme::inkSoft, 10.5f, 2.2f, true, juce::Justification::left);
}

//==============================================================================
void VocalAirEditor::resized()
{
    licenseOverlay.setBounds (getLocalBounds());

    if (chassisImg.isValid())
    {
        using namespace plategeo;

        // rebuild the scaled plate caches for the new size (1:1 blits per frame)
        plateScaled   = skin::renderPlate (chassisImg,   plateCrop, getWidth(), getHeight());
        plateOnScaled = skin::renderPlate (chassisOnImg, plateCrop, getWidth(), getHeight());

        const float iw = (float) chassisImg.getWidth(), ih = (float) chassisImg.getHeight();
        const float sx = (float) getWidth()  / (float) plateCrop.getWidth();
        const float sy = (float) getHeight() / (float) plateCrop.getHeight();

        auto knobRect = [&] (float cx, float cy, float sideFracW)
        {
            const int side = juce::roundToInt (sideFracW * iw * sx);
            return juce::Rectangle<int> (juce::roundToInt ((cx * iw - (float) plateCrop.getX()) * sx) - side / 2,
                                         juce::roundToInt ((cy * ih - (float) plateCrop.getY()) * sy) - side / 2,
                                         side, side);
        };

        // hero + trim knobs: bounds sized so the dome sprite hugs its seat
        midKnob.setBounds  (knobRect (ringLCx, ringCy, heroDomeDia / skin::croppedDomeFrac()));
        highKnob.setBounds (knobRect (ringRCx, ringCy, heroDomeDia / skin::croppedDomeFrac()));
        trimKnob.setBounds (knobRect (trimCx,  trimCy, trimDomeDia / skin::croppedDomeFrac()));

        // live values between ring bottom and the baked captions
        midVal.setBounds  (plateFracRect (ringLCx - 0.06f, valY0, 0.12f, valY1 - valY0));
        highVal.setBounds (plateFracRect (ringRCx - 0.06f, valY0, 0.12f, valY1 - valY0));
        trimVal.setBounds (plateFracRect (trimCx - 0.045f, trimValY0, 0.09f,
                                          trimValY1 - trimValY0));

        // top bar hit areas over the baked chrome
        undoBtn.setBounds (knobRect (undoCx, topCy, 0.032f));
        redoBtn.setBounds (knobRect (redoCx, topCy, 0.032f));
        abA.setBounds     (knobRect (abACx,  topCy, 0.036f));
        abArrow.setBounds (knobRect (abArrowCx, topCy, 0.020f));
        abB.setBounds     (knobRect (abBCx,  topCy, 0.036f));
        saveBtn.setBounds (knobRect (saveCx, topCy, 0.030f));
        menuBtn.setBounds (knobRect (menuCx, topCy, 0.030f));

        presetBox.setBounds (plateFracRect (boxX0, boxY0, boxX1 - boxX0, boxY1 - boxY0));

        // link pill + power button hit areas
        linkBtn.setBounds  (plateFracRect (linkRect[0] + 0.008f, linkRect[1] + 0.008f,
                                           linkRect[2] - 0.016f, linkRect[3] - 0.016f));
        powerBtn.setBounds (knobRect (0.0676f, 0.8522f, 0.055f));

        // meter is invisible but keeps its bounds for level updates
        meter.setBounds (plateFracRect (cardX0, cardY0, cardX1 - cardX0, cardY1 - cardY0));
        return;
    }

    auto r = getLocalBounds().reduced (24);

    // ---- top bar ----
    auto top = r.removeFromTop (44);
    brand.setBounds (top.getX(), top.getY() + 4, 130, 34);
    brandSub.setBounds (brand.getRight() + 6, top.getY() + 13, 90, 20);

    // right-aligned cluster
    int x = top.getRight();
    const int icon = 30, gap = 10;

    menuBtn.setBounds (x - icon, top.getY() + 6, icon, icon);            x -= icon + gap;
    saveBtn.setBounds (x - icon, top.getY() + 6, icon, icon);            x -= icon + gap + 6;

    const int comboW = 170, comboH = 34;
    presetBox.setBounds (x - comboW, top.getY() + 4, comboW, comboH);    x -= comboW + gap + 8;

    // A > B cluster
    const int abW = 26;
    abB.setBounds (x - abW, top.getY() + 6, abW, icon);                  x -= abW + 2;
    abArrow.setBounds (x - 22, top.getY() + 6, 22, icon);               x -= 22 + 2;
    abA.setBounds (x - abW, top.getY() + 6, abW, icon);                 x -= abW + gap + 8;

    redoBtn.setBounds (x - icon, top.getY() + 6, icon, icon);            x -= icon + gap;
    undoBtn.setBounds (x - icon, top.getY() + 6, icon, icon);

    r.removeFromTop (16);

    // ---- display card with arc meter ----
    displayCard = r.removeFromTop (230);
    displayLabel.setBounds (displayCard.getX() + 22, displayCard.getY() + 12, 120, 22);
    meter.setBounds (displayCard.reduced (24).withTrimmedTop (10));

    r.removeFromTop (24);

    // ---- knob row ----
    auto knobRow = r.removeFromTop (240);
    const int knobSize = 230;
    const int centreGap = 150;

    auto leftCol  = knobRow.removeFromLeft (knobRow.getWidth() / 2);
    auto rightCol = knobRow;

    midKnob.setBounds (leftCol.getRight() - centreGap / 2 - knobSize,
                       leftCol.getY(), knobSize, knobSize);
    highKnob.setBounds (rightCol.getX() + centreGap / 2,
                        rightCol.getY(), knobSize, knobSize);

    // range labels beneath each knob
    const int lblY = midKnob.getBottom() - 18;
    midLo.setBounds (midKnob.getX() - 6, lblY, 40, 20);
    midHi.setBounds (midKnob.getRight() - 34, lblY, 40, 20);
    highLo.setBounds (highKnob.getX() - 6, lblY, 40, 20);
    highHi.setBounds (highKnob.getRight() - 34, lblY, 40, 20);

    // link pill centred between the knobs
    const int linkW = 86, linkH = 36;
    linkBtn.setBounds ((midKnob.getRight() + highKnob.getX()) / 2 - linkW / 2,
                       midKnob.getBounds().getCentreY() - linkH / 2, linkW, linkH);

    // ---- bottom row: power (left) + trim (right) ----
    auto bottom = getLocalBounds().reduced (24).removeFromBottom (70);
    powerLabel.setBounds (bottom.getX(), bottom.getY(), 70, 20);
    powerBtn.setBounds (bottom.getX(), bottom.getY() + 22, 44, 44);

    trimLabel.setBounds (bottom.getRight() - 70, bottom.getY(), 70, 20);
    trimKnob.setBounds (bottom.getRight() - 60, bottom.getY() + 16, 54, 54);
}
