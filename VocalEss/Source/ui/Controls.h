#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include "Theme.h"

//==============================================================================
// Editable numeric value box (e.g. the Frequency field). Drag up/down to scrub,
// double-click to type a value. Talks straight to a RangedAudioParameter.
//==============================================================================
class FreqBox : public juce::Component
{
public:
    explicit FreqBox (juce::RangedAudioParameter& p) : param (p)
    {
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, theme::ink);
        label.setFont (theme::font (17.0f, true));
        label.setInterceptsMouseClicks (false, false);
        label.onEditorHide = [this] { editing = false; refresh(); };
        label.onTextChange  = [this] { commit(); };
        addAndMakeVisible (label);
        refresh();
    }

    void refresh()
    {
        if (editing) return;
        const float hz = param.convertFrom0to1 (param.getValue());
        label.setText (juce::String (juce::roundToInt (hz)), juce::dontSendNotification);
        repaint();
    }

    void resized() override { label.setBounds (getLocalBounds().withTrimmedRight (26)); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (r, 12.0f, 1.2f);

        g.setColour (theme::inkSoft);
        g.setFont (theme::font (12.0f, false));
        g.drawText ("Hz", r.removeFromRight (26).withTrimmedRight (6),
                    juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragStartNorm = param.getValue();
        param.beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float delta = (float) -e.getDistanceFromDragStartY() * 0.004f;
        param.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStartNorm + delta));
        refresh();
    }

    void mouseUp (const juce::MouseEvent&) override { param.endChangeGesture(); }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        editing = true;
        const float hz = param.convertFrom0to1 (param.getValue());
        label.setText (juce::String (juce::roundToInt (hz)), juce::dontSendNotification);
        label.showEditor();
    }

private:
    void commit()
    {
        const float hz = label.getText().getFloatValue();
        if (hz > 0.0f)
            param.setValueNotifyingHost (param.convertTo0to1 (hz));
        editing = false;
        refresh();
    }

    juce::RangedAudioParameter& param;
    juce::Label label;
    float dragStartNorm = 0.0f;
    bool  editing = false;
};

//==============================================================================
// Sidechain filter selector: draws the current filter's curve + a chevron,
// click opens a menu (High Pass / Bell / High Shelf).
//==============================================================================
class SCFilterButton : public juce::Component,
                       private juce::Timer
{
public:
    explicit SCFilterButton (juce::RangedAudioParameter& p) : param (p) {}

    std::function<void()> onChange;

    int index() const { return (int) std::round (param.convertFrom0to1 (param.getValue())); }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat().reduced (1.0f);
        auto iconR = full.removeFromLeft (full.getWidth() * 0.62f);
        full.removeFromLeft (6.0f);
        auto chevR = full;

        // icon box (pink outline)
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (iconR, 12.0f);
        g.setColour (theme::accent);
        g.drawRoundedRectangle (iconR, 12.0f, 1.6f);

        // curve
        auto c = iconR.reduced (12.0f, 14.0f);
        juce::Path curve;
        const float y0 = c.getY(), y1 = c.getBottom();
        const float x0 = c.getX(), x1 = c.getRight();
        switch (index())
        {
            case 1: // bell
                curve.startNewSubPath (x0, y1);
                curve.quadraticTo (c.getCentreX() - c.getWidth() * 0.18f, y1,
                                   c.getCentreX(), y0);
                curve.quadraticTo (c.getCentreX() + c.getWidth() * 0.18f, y1, x1, y1);
                break;
            case 2: // high shelf
                curve.startNewSubPath (x0, y1);
                curve.lineTo (c.getCentreX() - 4.0f, y1);
                curve.quadraticTo (c.getCentreX(), y1, c.getCentreX() + 6.0f, y0 + (y1 - y0) * 0.25f);
                curve.lineTo (x1, y0 + (y1 - y0) * 0.25f);
                break;
            default: // high pass
                curve.startNewSubPath (x0, y1);
                curve.lineTo (c.getCentreX() - 6.0f, y1);
                curve.quadraticTo (c.getCentreX() + 2.0f, y1, c.getCentreX() + 8.0f, y0);
                curve.lineTo (x1, y0);
                break;
        }
        g.setColour (theme::accent);
        g.strokePath (curve, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // chevron box
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (chevR, 12.0f);
        g.setColour (theme::cardLine);
        g.drawRoundedRectangle (chevR, 12.0f, 1.2f);
        juce::Path tri;
        const auto cc = chevR.getCentre();
        tri.addTriangle (cc.x - 5.0f, cc.y - 3.0f, cc.x + 5.0f, cc.y - 3.0f, cc.x, cc.y + 4.0f);
        g.setColour (theme::inkSoft);
        g.fillPath (tri);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        scale = 0.9f; applyScale(); startTimerHz (60);

        juce::PopupMenu m;
        const char* names[] = { "High Pass", "Bell", "High Shelf" };
        for (int i = 0; i < 3; ++i)
            m.addItem (i + 1, names[i], true, i == index());

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [this] (int r)
            {
                if (r > 0)
                {
                    param.setValueNotifyingHost (param.convertTo0to1 ((float) (r - 1)));
                    refresh();
                    if (onChange) onChange();
                }
            });
    }

    void refresh() { repaint(); }

private:
    void timerCallback() override
    {
        scale += (1.0f - scale) * 0.30f;
        if (std::abs (1.0f - scale) < 0.004f) { scale = 1.0f; stopTimer(); }
        applyScale();
    }

    void applyScale()
    {
        const auto b = getBounds().toFloat();
        setTransform (juce::AffineTransform::scale (scale, scale, b.getCentreX(), b.getCentreY()));
    }

    juce::RangedAudioParameter& param;
    float scale = 1.0f;
};
