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
// Baked-plate geometry. The chassis PNGs are 2048x1360 and the plate fills the
// whole canvas; every coordinate is a fraction of the full image, measured by
// diffing the ON plate against the OFF plate and circle-fitting the neon ring.
namespace plategeo
{
    // hero dial: centre + radius from a per-angle least-squares fit of the hot
    // neon ring core on the ON plate (residual 2.5px). Hot band r=[0.154,0.169],
    // groove min r=0.164; bloom is solid to ~0.178 and faint past 0.19. The
    // reveal stops at 0.205 (pill glow starts at y=0.774, r=0.2086 from here).
    constexpr float dialCx = 0.5028f, dialCy = 0.4594f;
    constexpr float dialWedgeR = 0.205f, dialGlowSolidR = 0.178f;
    // Sized so the visible steel edge lands at ~0.107W: dome/ring ratio 0.66,
    // matching the approved reference (0.64) instead of crowding the groove.
    constexpr float dialDomeDia = 0.240f;

    // mode pills: seam-to-seam mask rects covering the FULL glow extent
    // (x limits are gap midpoints / where the outermost glow dies; y covers
    // the bloom above and below the pill bodies) so no hard cuts show.
    constexpr float pillX[4][2] = { { 0.1724f, 0.3400f }, { 0.3400f, 0.5064f },
                                    { 0.5064f, 0.6719f }, { 0.6719f, 0.8286f } };
    constexpr float pillY0 = 0.7745f, pillY1 = 0.9140f;

    // preset capsule: baked arrow button centres (dark-glyph centroids)
    constexpr float arrowL = 0.3804f, arrowR = 0.6279f, arrowCy = 0.115f;
}

//==============================================================================
VocalKnobEditor::VocalKnobEditor (VocalKnobProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    chassisImg   = skin::image ("knob-chassis@2x.png");
    chassisOnImg = skin::image ("knob-chassis-on@2x.png");
    const bool baked = chassisImg.isValid() && chassisOnImg.isValid();
    laf.plate = baked;

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
    prevBtn.setComponentID ("navL");
    nextBtn.setComponentID ("navR");
    prevBtn.setButtonText ({});
    nextBtn.setButtonText ({});
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

    // ------------------------------------------------------------------
    // Baked plate: everything static lives on the chassis image; buttons
    // become invisible hit areas and the dial paints the steel dome sprite.
    // ------------------------------------------------------------------
    if (baked)
    {
        dial.setPlateMode (true);

        for (auto& b : modeBtns) { b.setButtonText (""); b.setComponentID ("hit"); }
        for (auto* b : { &prevBtn, &nextBtn }) b->setComponentID ("hit");

        // wordmark, tagline and captions are baked into the plate
        for (auto* c : std::initializer_list<juce::Component*> {
                 &subtitle, &brand, &brandSub,
                 &capLabels[0], &capLabels[1], &capLabels[2], &capLabels[3] })
            c->setVisible (false);

        presetName.setFont (theme::font (15.0f, false));
        presetName.setColour (juce::Label::textColourId, theme::ink);
    }

    startTimerHz (30);
    setSize (baked ? 960 : 780, baked ? 638 : 640);

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

    if (chassisImg.isValid())
        repaint();   // pill mask + ring wedge live in paintPlate
}

//==============================================================================
juce::Rectangle<int> VocalKnobEditor::plateFracRect (float fx, float fy, float fw, float fh) const
{
    return juce::Rectangle<float> (fx * (float) getWidth(),  fy * (float) getHeight(),
                                   fw * (float) getWidth(),  fh * (float) getHeight())
               .toNearestInt();
}

// Blit the matching region of the lit plate over the base plate — pixel
// registration is guaranteed because both images share the same canvas.
void VocalKnobEditor::maskFromOn (juce::Graphics& g, juce::Rectangle<int> screenRect)
{
    const float iw = (float) chassisOnImg.getWidth(), ih = (float) chassisOnImg.getHeight();
    g.drawImage (chassisOnImg,
                 screenRect.getX(), screenRect.getY(), screenRect.getWidth(), screenRect.getHeight(),
                 juce::roundToInt (((float) screenRect.getX()      / (float) getWidth())  * iw),
                 juce::roundToInt (((float) screenRect.getY()      / (float) getHeight()) * ih),
                 juce::roundToInt (((float) screenRect.getWidth()  / (float) getWidth())  * iw),
                 juce::roundToInt (((float) screenRect.getHeight() / (float) getHeight()) * ih));
}

// Same reveal but with a soft alpha ramp along the rect border, so the slight
// global tone drift between the two plates never shows as a hard rectangle.
void VocalKnobEditor::maskFromOnFeathered (juce::Graphics& g, juce::Rectangle<int> screenRect,
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

// Reveal the lit neon ring as an annular wedge clipped to the value sweep
// (6 o'clock -> 6 o'clock) with a feathered leading edge. The dome disc is
// carved out so the glow never tints the steel.
void VocalKnobEditor::drawDialWedge (juce::Graphics& g)
{
    using namespace plategeo;
    const float W = (float) getWidth();
    const juce::Point<float> c (dialCx * W, dialCy * (float) getHeight());
    const float R = dialWedgeR * W;
    const float domeR = dialDomeDia * 0.5f * W;

    const float prop = juce::jlimit (0.0f, 1.0f,
                                     (float) dial.valueToProportionOfLength (dial.getValue()));
    const float a0 = juce::MathConstants<float>::pi;
    const float a1 = a0 + prop * juce::MathConstants<float>::twoPi;
    const bool full = prop >= 0.995f;
    if (! full && prop <= 0.002f) return;

    const float solidR = dialGlowSolidR * W;

    const juce::Rectangle<int> box ((int) std::floor (c.x - R), (int) std::floor (c.y - R),
                                    (int) std::ceil (R * 2.0f), (int) std::ceil (R * 2.0f));

    // annular wedge reveal: [rIn, rOut] radial band, [from, to] angular span
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

    const float feather = full ? 0.0f : juce::jmin (0.22f, (a1 - a0) * 0.5f);
    const float aEnd = full ? a0 + juce::MathConstants<float>::twoPi : a1;

    // solid core band (ring + tight bloom), angular feather on the leading edge
    wedge (a0, aEnd - feather, domeR, solidR, 1.0f);
    constexpr int aSteps = 10;
    for (int i = 0; i < aSteps; ++i)
        wedge (aEnd - feather * (1.0f - (float) i / aSteps),
               aEnd - feather * (1.0f - (float) (i + 1) / aSteps),
               domeR, solidR,
               1.0f - ((float) i + 0.5f) / aSteps);

    // Outer bloom fades radially (never a circular cut) and eases in over the
    // first stretch after 6 o'clock so the start edge is only crisp on the
    // neon tube itself, not on the wide glow.
    const float sf = full ? 0.0f : juce::jmin (0.14f, (a1 - a0) * 0.25f);
    constexpr int rSteps = 4;
    for (int i = 0; i < rSteps; ++i)
    {
        const float alpha = 0.85f * (1.0f - ((float) i + 0.5f) / rSteps);
        const float rIn  = solidR + (R - solidR) * (float) i / rSteps;
        const float rOut = solidR + (R - solidR) * (float) (i + 1) / rSteps;
        wedge (a0 + sf, aEnd - feather * 0.5f, rIn, rOut, alpha);
        wedge (a0,             a0 + sf * 0.5f, rIn, rOut, alpha * 0.33f);
        wedge (a0 + sf * 0.5f, a0 + sf,        rIn, rOut, alpha * 0.66f);
    }
}

void VocalKnobEditor::paintPlate (juce::Graphics& g)
{
    using namespace plategeo;

    g.drawImage (chassisImg, getLocalBounds().toFloat(),
                 juce::RectanglePlacement::stretchToFit);

    // active mode pill lights up (feathered so the plate tone drift is hidden)
    const int mode = juce::jlimit (0, 3, (int) proc.apvts.getRawParameterValue ("mode")->load());
    maskFromOnFeathered (g, plateFracRect (pillX[mode][0], pillY0,
                                           pillX[mode][1] - pillX[mode][0], pillY1 - pillY0),
                         juce::roundToInt ((float) getWidth() * 0.012f));

    drawDialWedge (g);
}

//==============================================================================
void VocalKnobEditor::paint (juce::Graphics& g)
{
    if (chassisImg.isValid())
    {
        paintPlate (g);
        return;
    }

    // Pure white studio backdrop — the panel reads as a moulded object floating
    // on white (matches the product render), not a card on a grey gradient.
    g.fillAll (juce::Colours::white);

    {
        // Vector fallback: floating moulded device — white face lifted by a wide,
        // very soft three-layer shadow. No hard border: on pure white the shadow
        // alone defines the panel, which reads far more premium.
        auto rf = cardArea.toFloat();
        const float corner = 34.0f;
        const juce::Colour sh (0xff20242e);
        juce::Path pp; pp.addRoundedRectangle (rf, corner);
        juce::DropShadow (sh.withAlpha (0.09f), 60, { 0, 22 }).drawForPath (g, pp);
        juce::DropShadow (sh.withAlpha (0.06f), 28, { 0, 10 }).drawForPath (g, pp);
        juce::DropShadow (sh.withAlpha (0.04f), 8,  { 0, 3  }).drawForPath (g, pp);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (rf, corner);
        theme::topHighlight (g, rf, corner);
    }

    // mood: a whisper of accent bloom behind the hero dial — kept faint so the
    // panel stays clean white and the arc's own halo carries the colour.
    theme::accentBloom (g, dial.getBounds().getCentre().toFloat(),
                        (float) dial.getWidth() * 0.5f, 0.045f);

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

    if (chassisImg.isValid())
    {
        using namespace plategeo;
        const float W = (float) getWidth();

        auto knobRect = [&] (float cx, float cy, float sideFracW)
        {
            const int side = juce::roundToInt (sideFracW * W);
            return juce::Rectangle<int> (juce::roundToInt (cx * W - side * 0.5f),
                                         juce::roundToInt (cy * (float) getHeight() - side * 0.5f),
                                         side, side);
        };

        // hero dial: bounds sized so the sprite dome matches the ring seat
        dial.setBounds (knobRect (dialCx, dialCy, dialDomeDia / skin::croppedDomeFrac()));

        // preset arrows (baked chrome) + name between them. The label rect is
        // centred on the CAPSULE midline (inner border y 0.082..0.138 ->
        // 0.1096), nudged up 0.0026 because the label's font box renders the
        // cap-height core slightly below the rect centre.
        prevBtn.setBounds (knobRect (arrowL, arrowCy, 0.050f));
        nextBtn.setBounds (knobRect (arrowR, arrowCy, 0.050f));
        {
            const float nx0 = arrowL + 0.030f, nx1 = arrowR - 0.030f;
            presetName.setBounds (plateFracRect (nx0, 0.1074f - 0.031f,
                                                 nx1 - nx0, 0.062f));
        }

        // mode pills: invisible hit areas over the baked pills
        for (int i = 0; i < 4; ++i)
            modeBtns[(size_t) i].setBounds (plateFracRect (pillX[i][0], pillY0,
                                                           pillX[i][1] - pillX[i][0],
                                                           pillY1 - pillY0));
        return;
    }

    auto r = getLocalBounds().reduced (26);
    cardArea = r;

    auto inner = r.reduced (22);

    // ---- top bar ----
    auto top = inner.removeFromTop (36);
    brand.setBounds (top.getX(), top.getY() + 2, 96, 30);
    brandSub.setBounds (brand.getRight() + 4, top.getY() + 8, 124, 20);

    // preset selector centred in the top bar, but never allowed to slide under
    // the wordmark / "VOCAL TEXTURE" label on the left.
    const int pillW = 220, pillH = 32;
    int pillX = inner.getCentreX() - pillW / 2;
    pillX = juce::jmax (pillX, brandSub.getRight() + 14);
    presetPill = juce::Rectangle<int> (pillX, top.getY(), pillW, pillH);
    prevBtn.setBounds (presetPill.getX() + 3, presetPill.getY() + 3, 28, pillH - 6);
    nextBtn.setBounds (presetPill.getRight() - 31, presetPill.getY() + 3, 28, pillH - 6);
    presetName.setBounds (prevBtn.getRight(), presetPill.getY(),
                          nextBtn.getX() - prevBtn.getRight(), pillH);

    // ---- bottom rows (build from the bottom up) ----
    inner.removeFromBottom (18);            // intentional bottom breathing margin
    auto caps = inner.removeFromBottom (18);
    inner.removeFromBottom (4);
    auto pills = inner.removeFromBottom (48);
    inner.removeFromBottom (14);
    auto sub = inner.removeFromBottom (22);
    subtitle.setBounds (sub);

    // four puffy pills centred, with the caption labels directly beneath each.
    // Bounds are taller than the visible pill so the soft drop shadow has room.
    const int pw = 124, gap = 14;
    const int rowW = pw * 4 + gap * 3;
    int x = inner.getCentreX() - rowW / 2;
    for (int i = 0; i < 4; ++i)
    {
        modeBtns[(size_t) i].setBounds (x, pills.getY(), pw, 46);
        capLabels[(size_t) i].setBounds (x, caps.getY(), pw, 18);
        x += pw + gap;
    }

    // ---- dial fills the remaining centre ----
    dial.setBounds (inner.withSizeKeepingCentre (juce::jmin (inner.getWidth(), 352),
                                                 juce::jmin (inner.getHeight(), 352)));
}
