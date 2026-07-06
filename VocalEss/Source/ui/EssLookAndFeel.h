#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../../../common/ui/Skin.h"

//==============================================================================
// Premium light look: soft white neumorphic dome knobs with a glowing accent
// value ring, recessed slider grooves with dimensional white-dome thumbs, and
// clean white pill toggles that fill with the accent gradient when active.
// Shared family styling with VocalGrit.
//
// Plate mode (baked chassis present): linear sliders paint only the steel stud
// thumb (track + fill live on the plates); components tagged "hit" become
// invisible hit areas.
//==============================================================================
class EssLookAndFeel : public juce::LookAndFeel_V4
{
public:
    bool plate = false;

    EssLookAndFeel()
    {
        thumbImg = skin::cropToDome (skin::image ("slider-thumb@2x.png"),
                                     0.4994f, 0.4989f, 0.626f);
       #if VG_HAS_BUNDLED_FONT
        setDefaultSansSerifTypeface (theme::bundledTypeface (false));
       #else
        setDefaultSansSerifTypefaceName (theme::fontFamily);
       #endif

        setColour (juce::ResizableWindow::backgroundColourId, theme::bg);
        setColour (juce::Label::textColourId, theme::ink);
        setColour (juce::TextButton::textColourOffId, theme::ink);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colours::white);
        setColour (juce::PopupMenu::textColourId, theme::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        const auto f = label.getFont();
        return theme::font (f.getHeight(), f.isBold());
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return theme::font (juce::jmin (15.0f, (float) buttonHeight * 0.5f), false);
    }

    juce::Font getPopupMenuFont() override { return theme::font (14.0f, false); }

    //==========================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider&) override
    {
        const bool horizontal = (style == juce::Slider::LinearHorizontal);

        if (plate && thumbImg.isValid())
        {
            // stud thumb only — the recessed groove is baked into the plate and
            // the pink fill is masked from the ON plate by the editor
            const float dia = (float) (horizontal ? height : width);
            const float cx = horizontal ? sliderPos : (float) x + (float) width * 0.5f;
            const float cy = horizontal ? (float) y + (float) height * 0.5f : sliderPos;
            skin::drawInRect (g, thumbImg, { cx - dia * 0.5f, cy - dia * 0.5f, dia, dia });
            return;
        }

        const float trackThick = horizontal ? 14.0f : 16.0f;
        const float thumbR     = horizontal ? 11.0f : 13.0f;

        if (horizontal)
        {
            const float cy = (float) y + height * 0.5f;
            juce::Rectangle<float> tr ((float) x, cy - trackThick * 0.5f, (float) width, trackThick);
            theme::recess (g, tr, trackThick * 0.5f);

            juce::Rectangle<float> fill ((float) x, cy - trackThick * 0.5f,
                                         sliderPos - (float) x, trackThick);
            paintAccentFill (g, fill, trackThick * 0.5f);
            drawThumb (g, sliderPos, cy, thumbR);
        }
        else
        {
            const float cx = (float) x + width * 0.5f;
            juce::Rectangle<float> tr (cx - trackThick * 0.5f, (float) y, trackThick, (float) height);
            theme::recess (g, tr, trackThick * 0.5f);

            // pink fill from the thumb downward (de-esser threshold feel)
            juce::Rectangle<float> fill (cx - trackThick * 0.5f, sliderPos,
                                         trackThick, (float) (y + height) - sliderPos);
            paintAccentFill (g, fill, trackThick * 0.5f);
            drawThumb (g, cx, sliderPos, thumbR);
        }
    }

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float pos, float startAngle, float endAngle,
                           juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                          .reduced (5.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        drawRotaryBody (g, centre, radius, pos, startAngle, endAngle, angle,
                        s.isMouseOverOrDragging (true));
    }

    // Shared rotary renderer: a soft white neumorphic dome with a glowing accent
    // value ring. No indicator dots — the lit ring shows position. All shadows
    // are sized to fit inside the caller's bounds so nothing gets clipped.
    static void drawRotaryBody (juce::Graphics& g, juce::Point<float> centre, float radius,
                                float pos, float startAngle, float endAngle, float angle,
                                bool hover = false)
    {
        const float ringThick = juce::jmax (2.5f, radius * 0.10f);
        const float ringR     = radius * 0.80f;                 // pulled in: room for glow
        const float capR      = ringR - ringThick * 0.5f - radius * 0.07f;
        const float glowR     = radius * 0.12f;

        juce::Rectangle<float> disc (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);

        // ---- soft cast shadow (kept well within bounds)
        {
            const int blur = (int) juce::jlimit (4.0f, 16.0f, capR * 0.30f);
            juce::Path sp; sp.addEllipse (disc.translated (0.0f, capR * 0.12f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.20f), blur,
                              { 0, (int) (capR * 0.10f) }).drawForPath (g, sp);
        }

        // ---- ring track + glowing value arc
        juce::Path base;
        base.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (theme::ringTrack);
        g.strokePath (base, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        if (angle > startAngle + 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, angle, true);
            juce::Path arcStroke;
            juce::PathStrokeType (ringThick).createStrokedPath (arcStroke, arc);
            theme::glowPath (g, arcStroke, hover ? 0.8f : 0.45f, (int) (glowR * (hover ? 1.4f : 1.0f)));
            g.setColour (hover ? theme::accentHi : theme::accent);
            g.strokePath (arc, juce::PathStrokeType (ringThick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        paintWhiteDome (g, disc);
        juce::ignoreUnused (pos);
    }

    // A soft, domed white knob face lit from the top — the core of the look.
    static void paintWhiteDome (juce::Graphics& g, juce::Rectangle<float> disc)
    {
        const float capR = disc.getWidth() * 0.5f;

        // body: radial gradient, brightest near the top, shading to a cool edge
        juce::ColourGradient body (juce::Colours::white,
                                   disc.getCentreX(), disc.getY() + capR * 0.62f, // light centre, high
                                   theme::capLo, disc.getCentreX(), disc.getBottom(), true);
        body.addColour (0.6, theme::capMid);
        body.addColour (0.85, juce::Colour (0xffd9dbe4));
        g.setGradientFill (body);
        g.fillEllipse (disc);

        // bottom inner shading crescent so the face reads as a sphere
        juce::ColourGradient lower (juce::Colours::transparentBlack, disc.getCentreX(), disc.getCentreY(),
                                    juce::Colours::black.withAlpha (0.10f), disc.getCentreX(), disc.getBottom(), false);
        g.setGradientFill (lower);
        g.fillEllipse (disc);

        // top specular catch
        auto hi = juce::Rectangle<float> (disc.getWidth() * 0.7f, disc.getHeight() * 0.42f)
                      .withCentre ({ disc.getCentreX(), disc.getY() + disc.getHeight() * 0.24f });
        juce::ColourGradient sg (juce::Colours::white.withAlpha (0.9f), hi.getCentreX(), hi.getY(),
                                 juce::Colours::transparentWhite, hi.getCentreX(), hi.getBottom(), false);
        g.setGradientFill (sg);
        g.fillEllipse (hi);

        // rim: bright top hairline + soft cool outer edge
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawEllipse (disc.reduced (0.7f).translated (0.0f, -0.5f), 1.1f);
        g.setColour (theme::cardLine);
        g.drawEllipse (disc, 1.0f);
    }

    //==========================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                           bool highlighted, bool /*down*/) override
    {
        if (plate && b.getComponentID() == "hit") return;   // plate carries the visuals
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const float radius = r.getHeight() * 0.5f;
        const bool on = b.getToggleState();

        paintPill (g, r, radius, on, highlighted);

        g.setColour (on ? juce::Colours::white : theme::ink);
        g.setFont (theme::font (13.0f, false));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }

    //==========================================================================
    // Clean white pill: filled with the accent gradient when active, crisp
    // hairline outline when not. No drop shadow, no status dot, centred text.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour&, bool highlighted, bool /*down*/) override
    {
        if (plate && b.getComponentID() == "hit") return;   // invisible hit area
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const float radius = r.getHeight() * 0.5f;
        paintPill (g, r, radius, b.getToggleState(), highlighted);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b,
                         bool highlighted, bool down) override
    {
        if (plate && b.getComponentID() == "hit") return;   // text is baked
        LookAndFeel_V4::drawButtonText (g, b, highlighted, down);
    }

private:
    juce::Image thumbImg;

    //==========================================================================
    static void paintAccentFill (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        if (r.getWidth() < 1.0f || r.getHeight() < 1.0f) return;
        juce::Path p; p.addRoundedRectangle (r, radius);
        theme::glowPath (g, p, 0.25f, 8);
        juce::ColourGradient ag (theme::accentHi, r.getX(), r.getY(),
                                 theme::accentLo, r.getX(), r.getBottom(),
                                 r.getHeight() > r.getWidth());
        if (r.getWidth() >= r.getHeight())
            ag = juce::ColourGradient (theme::accentHi, r.getX(), r.getY(),
                                       theme::accentLo, r.getX(), r.getBottom(), false);
        g.setGradientFill (ag);
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.drawLine (r.getX() + radius, r.getY() + 0.7f, r.getRight() - radius, r.getY() + 0.7f, 1.0f);
    }

    static void paintPill (juce::Graphics& g, juce::Rectangle<float> r, float radius,
                           bool on, bool highlighted)
    {
        if (on)
        {
            juce::ColourGradient ag (theme::accentHi, r.getX(), r.getY(),
                                     theme::accentLo, r.getX(), r.getBottom(), false);
            g.setGradientFill (ag);
            g.fillRoundedRectangle (r, radius);
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawLine (r.getX() + radius, r.getY() + 1.2f, r.getRight() - radius, r.getY() + 1.2f, 1.2f);
            g.setColour (theme::accentLo.withAlpha (0.55f));
            g.drawRoundedRectangle (r, radius, 1.0f);
        }
        else
        {
            // clean white pill: subtle top-down sheen + crisp hairline, no shadow
            juce::ColourGradient wg (juce::Colours::white, r.getX(), r.getY(),
                                     juce::Colour (0xfff4f5f8), r.getX(), r.getBottom(), false);
            g.setGradientFill (wg);
            g.fillRoundedRectangle (r, radius);
            g.setColour (juce::Colours::white);
            g.drawLine (r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.0f);
            g.setColour (highlighted ? theme::accent.withAlpha (0.55f) : theme::cardLine);
            g.drawRoundedRectangle (r, radius, 1.2f);
        }
    }

    static void drawThumb (juce::Graphics& g, float cx, float cy, float r)
    {
        juce::Rectangle<float> disc (cx - r, cy - r, r * 2.0f, r * 2.0f);

        // soft cast shadow (within bounds)
        juce::Path sp; sp.addEllipse (disc.translated (0.0f, r * 0.18f));
        juce::DropShadow (juce::Colours::black.withAlpha (0.22f),
                          (int) juce::jlimit (4.0f, 10.0f, r * 0.5f), { 0, 2 }).drawForPath (g, sp);

        paintWhiteDome (g, disc);
    }
};
