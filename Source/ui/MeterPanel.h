#pragma once

#include "../PluginProcessor.h"
#include "RedzoneLookAndFeel.h"

namespace rz::ui
{
// Loudness (LUFS), peak, correlation and gain-reduction readouts.
class MeterPanel : public juce::Component, private juce::Timer
{
public:
    explicit MeterPanel (RedzoneAudioProcessor& p) : proc (p)
    {
        resetButton.setButtonText ("RESET");
        resetButton.onClick = [this]
        {
            proc.loudness.requestReset();
            holdL = holdR = -100.0f;
        };
        addAndMakeVisible (resetButton);
        startTimerHz (20);
    }

    ~MeterPanel() override { stopTimer(); }

    void resized() override
    {
        resetButton.setBounds (getLocalBounds().reduced (12).removeFromBottom (26));
    }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        g.setColour (panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

        auto area = getLocalBounds().reduced (12);
        area.removeFromBottom (34);

        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.setColour (accent);
        g.drawText ("LOUDNESS", area.removeFromTop (16), juce::Justification::left);

        auto readout = [&] (const juce::String& name, float lufs, bool big)
        {
            auto row = area.removeFromTop (big ? 38 : 22);
            g.setColour (textDim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (name, row.removeFromLeft (70), juce::Justification::centredLeft);
            g.setColour (text);
            g.setFont (juce::FontOptions (big ? 24.0f : 15.0f, juce::Font::bold));
            g.drawText (lufs <= -99.0f ? juce::String ("-inf") : juce::String (lufs, 1), row, juce::Justification::centredRight);
        };
        readout ("Integrated", integrated, true);
        readout ("Short-term", shortTerm, false);
        readout ("Momentary", momentary, false);
        area.removeFromTop (6);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("LUFS", area.removeFromTop (12), juce::Justification::centredRight);
        area.removeFromTop (8);

        // Peaks
        g.setColour (accent);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("PEAK (dBFS)", area.removeFromTop (16), juce::Justification::left);
        drawBar (g, area.removeFromTop (18), "L", peakL, holdL, -60.0f, 0.0f);
        drawBar (g, area.removeFromTop (18), "R", peakR, holdR, -60.0f, 0.0f);
        area.removeFromTop (10);

        // Limiter GR
        g.setColour (accent);
        g.drawText ("LIMITER GR (dB)", area.removeFromTop (16), juce::Justification::left);
        drawBar (g, area.removeFromTop (18), "GR", -limiterGr, -1000.0f, 0.0f, 12.0f);
        area.removeFromTop (10);

        // Multiband GR
        g.setColour (accent);
        g.drawText ("MULTIBAND GR (dB)", area.removeFromTop (16), juce::Justification::left);
        const char* names[] = { "LOW", "MID", "HIGH" };
        for (int i = 0; i < 3; ++i)
            drawBar (g, area.removeFromTop (18), names[i], -mbGr[(size_t) i], -1000.0f, 0.0f, 18.0f);
        area.removeFromTop (10);

        // Correlation
        g.setColour (accent);
        g.drawText ("CORRELATION", area.removeFromTop (16), juce::Justification::left);
        auto cr = area.removeFromTop (18).toFloat();
        g.setColour (panelLight);
        g.fillRoundedRectangle (cr, 3.0f);
        const float cx = cr.getCentreX();
        const float vx = cr.getX() + (correlation * 0.5f + 0.5f) * cr.getWidth();
        g.setColour (correlation >= 0.0f ? juce::Colour (0xff3ec46d) : accent);
        g.fillRect (juce::Rectangle<float> (std::min (cx, vx), cr.getY() + 2.0f, std::abs (vx - cx), cr.getHeight() - 4.0f));
        g.setColour (text);
        g.drawVerticalLine ((int) cx, cr.getY(), cr.getBottom());
    }

private:
    void timerCallback() override
    {
        momentary = proc.loudness.momentary.load();
        shortTerm = proc.loudness.shortTerm.load();
        integrated = proc.loudness.integrated.load();
        peakL = proc.loudness.peakLeft.load();
        peakR = proc.loudness.peakRight.load();
        holdL = std::max (holdL, peakL);
        holdR = std::max (holdR, peakR);
        correlation = proc.loudness.correlation.load();
        limiterGr = std::min (0.0f, proc.maximizer.getGainReductionDb());
        for (int i = 0; i < 3; ++i)
            mbGr[(size_t) i] = std::min (0.0f, proc.multiband.getGainReductionDb (i));
        repaint();
    }

    void drawBar (juce::Graphics& g, juce::Rectangle<int> row, const juce::String& name, float value, float hold,
                  float lo, float hi) const
    {
        using namespace colours;
        g.setColour (textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (name, row.removeFromLeft (34), juce::Justification::centredLeft);
        auto bar = row.removeFromLeft (row.getWidth() - 44).toFloat().reduced (0.0f, 3.0f);
        g.setColour (panelLight);
        g.fillRoundedRectangle (bar, 2.0f);

        const float t = juce::jlimit (0.0f, 1.0f, juce::jmap (value, lo, hi, 0.0f, 1.0f));
        g.setColour (hi == 0.0f && value > -1.0f ? accent : accentSoft.brighter (0.3f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * t), 2.0f);

        if (hold > lo)
        {
            const float ht = juce::jlimit (0.0f, 1.0f, juce::jmap (hold, lo, hi, 0.0f, 1.0f));
            g.setColour (text);
            g.fillRect (bar.getX() + bar.getWidth() * ht - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        }

        g.setColour (text);
        g.drawText (juce::String (hold > -999.0f ? hold : value, 1), row, juce::Justification::centredRight);
    }

    RedzoneAudioProcessor& proc;
    juce::TextButton resetButton;
    float momentary = -100.0f, shortTerm = -100.0f, integrated = -100.0f;
    float peakL = -100.0f, peakR = -100.0f, holdL = -100.0f, holdR = -100.0f;
    float correlation = 0.0f, limiterGr = 0.0f;
    std::array<float, 3> mbGr { 0.0f, 0.0f, 0.0f };
};
} // namespace rz::ui
