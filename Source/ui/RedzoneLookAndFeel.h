#pragma once

#include "Common.h"

namespace rz::ui
{
namespace colours
{
    inline const juce::Colour background { 0xff0e0f12 };
    inline const juce::Colour panel      { 0xff17191e };
    inline const juce::Colour panelLight { 0xff22252c };
    inline const juce::Colour accent     { 0xffe0222d };
    inline const juce::Colour accentSoft { 0xff8f1a21 };
    inline const juce::Colour text       { 0xffe7e8ea };
    inline const juce::Colour textDim    { 0xff8b8f99 };
    inline const juce::Colour grid       { 0xff2a2d35 };
} // namespace colours

class RedzoneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RedzoneLookAndFeel()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::TextButton::buttonColourId, panelLight);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        using namespace colours;
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float arcR = radius - 3.0f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (grid);
        g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (slider.isEnabled() ? accent : accentSoft);
        g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float bodyR = arcR - 6.0f;
        g.setColour (panelLight);
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -bodyR + 2.0f, 3.0f, bodyR * 0.55f, 1.5f);
        g.setColour (text);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        using namespace colours;
        auto r = b.getLocalBounds().toFloat().reduced (2.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? accent : panelLight);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (highlighted ? text : textDim);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }

    juce::Font getLabelFont (juce::Label&) override { return juce::FontOptions (12.0f); }
};
} // namespace rz::ui
