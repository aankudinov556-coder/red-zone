#pragma once

#include "../PluginProcessor.h"
#include "ParamPanel.h"

namespace rz::ui
{
// Interactive 3-band width display.
//   drag a divider line   : move crossover        drag inside a band : width (up = wider)
//   double-click a band   : reset width to 100%   Shift              : fine adjust
class ImagerDisplay : public juce::Component, private juce::Timer
{
public:
    explicit ImagerDisplay (juce::AudioProcessorValueTreeState& s)
    {
        x1 = s.getParameter (id::imgX1);
        x2 = s.getParameter (id::imgX2);
        mono = s.getParameter (id::imgMonoFreq);
        for (int b = 0; b < 3; ++b) w[(size_t) b] = s.getParameter (id::imgWidth (b + 1));
        startTimerHz (20);
    }

    ~ImagerDisplay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        g.setColour (panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
        const auto a = area();

        // frequency grid
        g.setFont (juce::Font (10.0f));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xForFreq (f);
            g.setColour (grid);
            g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
            g.setColour (textDim);
            g.drawText (f >= 1000 ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x - 16.0f, a.getBottom() - 12.0f, 32.0f, 12.0f), juce::Justification::centred);
        }

        // 100 % line
        const float y100 = yForWidth (100.0f);
        g.setColour (grid.brighter (0.5f));
        g.drawHorizontalLine ((int) y100, a.getX(), a.getRight());
        g.setColour (textDim);
        g.drawText ("100%", juce::Rectangle<float> (a.getX() + 2.0f, y100 - 13.0f, 40.0f, 12.0f), juce::Justification::left);

        // bands
        const float edges[] = { a.getX(), xForFreq (val (x1)), xForFreq (effectiveX2()), a.getRight() };
        const char* names[] = { "LOW", "MID", "HIGH" };
        for (int b = 0; b < 3; ++b)
        {
            const float width = val (w[(size_t) b]);
            const float y = yForWidth (width);
            auto zone = juce::Rectangle<float>::leftTopRightBottom (edges[b] + 2.0f, std::min (y, y100), edges[b + 1] - 2.0f, std::max (y, y100));
            const auto col = width >= 100.0f ? accent : juce::Colour (0xff4f9dff);
            g.setColour (col.withAlpha (b == hotBand ? 0.45f : 0.3f));
            g.fillRect (zone);
            g.setColour (col);
            g.drawHorizontalLine ((int) y, edges[b] + 2.0f, edges[b + 1] - 2.0f);

            g.setColour (text);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            auto label = juce::Rectangle<float> (edges[b], a.getY() + 4.0f, edges[b + 1] - edges[b], 16.0f);
            g.drawText (juce::String (names[b]) + "  " + juce::String (juce::roundToInt (width)) + "%", label, juce::Justification::centred);
        }

        // crossover handles
        for (int i = 1; i <= 2; ++i)
        {
            g.setColour (hotLine == i ? text : textDim);
            g.fillRect (edges[i] - 1.0f, a.getY(), 2.0f, a.getHeight());
        }

        // bass mono region
        const float m = val (mono);
        if (m >= 20.0f)
        {
            const float xm = xForFreq (m);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (a.getX(), a.getY(), xm, a.getBottom()));
            g.setColour (textDim);
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText ("MONO", juce::Rectangle<float> (a.getX(), a.getBottom() - 28.0f, xm - a.getX(), 12.0f), juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        hotLine = lineAt (e.position.x);
        hotBand = hotLine == 0 ? bandAt (e.position.x) : -1;
        setMouseCursor (hotLine ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override { hotLine = 0; hotBand = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragLine = lineAt (e.position.x);
        dragBand = dragLine == 0 ? bandAt (e.position.x) : -1;
        dragParam = dragLine == 1 ? x1 : dragLine == 2 ? x2 : (dragBand >= 0 ? w[(size_t) dragBand] : nullptr);
        if (dragParam == nullptr) return;
        dragParam->beginChangeGesture();
        anchor (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragParam == nullptr) return;
        if (e.mods.isShiftDown() != lastFine) anchor (e);
        const float k = lastFine ? 0.2f : 1.0f;

        if (dragLine > 0)
        {
            const float f = startValue * std::pow (maxFreq / minFreq, (e.position.x - startPos.x) * k / area().getWidth());
            set (dragParam, f);
        }
        else
        {
            const float perPx = 200.0f / area().getHeight();
            set (dragParam, startValue - (e.position.y - startPos.y) * perPx * k);
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragParam != nullptr) dragParam->endChangeGesture();
        dragParam = nullptr;
        dragLine = 0;
        dragBand = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (lineAt (e.position.x) != 0) return;
        const int b = bandAt (e.position.x);
        if (b < 0) return;
        auto* p = w[(size_t) b];
        p->beginChangeGesture();
        set (p, 100.0f);
        p->endChangeGesture();
        repaint();
    }

private:
    static constexpr float minFreq = 20.0f, maxFreq = 20000.0f;

    void timerCallback() override { repaint(); }

    static float val (juce::RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); }
    static void set (juce::RangedAudioParameter* p, float v) { p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->convertTo0to1 (v))); }

    float effectiveX2() const { return std::max (val (x2), val (x1) * 1.5f); }

    void anchor (const juce::MouseEvent& e)
    {
        startPos = e.position;
        startValue = val (dragParam);
        lastFine = e.mods.isShiftDown();
    }

    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (8.0f); }

    float xForFreq (float f) const
    {
        const auto a = area();
        return a.getX() + a.getWidth() * std::log (f / minFreq) / std::log (maxFreq / minFreq);
    }

    float yForWidth (float wPercent) const
    {
        const auto a = area().withTrimmedTop (22.0f).withTrimmedBottom (14.0f);
        return juce::jmap (juce::jlimit (0.0f, 200.0f, wPercent), 0.0f, 200.0f, a.getBottom(), a.getY());
    }

    int lineAt (float x) const
    {
        if (std::abs (x - xForFreq (val (x1))) < 6.0f) return 1;
        if (std::abs (x - xForFreq (effectiveX2())) < 6.0f) return 2;
        return 0;
    }

    int bandAt (float x) const
    {
        if (x < xForFreq (val (x1))) return 0;
        if (x < xForFreq (effectiveX2())) return 1;
        return 2;
    }

    juce::RangedAudioParameter *x1 = nullptr, *x2 = nullptr, *mono = nullptr;
    std::array<juce::RangedAudioParameter*, 3> w {};

    int hotLine = 0, hotBand = -1, dragLine = 0, dragBand = -1;
    juce::RangedAudioParameter* dragParam = nullptr;
    juce::Point<float> startPos;
    float startValue = 0.0f;
    bool lastFine = false;
};

// Goniometer: vertical = mid (mono), horizontal = side (stereo).
class Vectorscope : public juce::Component, private juce::Timer
{
public:
    explicit Vectorscope (RedzoneAudioProcessor& p) : proc (p) { startTimerHz (30); }
    ~Vectorscope() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        g.setColour (panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

        auto sq = getLocalBounds().toFloat().reduced (10.0f);
        const float side = std::min (sq.getWidth(), sq.getHeight());
        sq = sq.withSizeKeepingCentre (side, side);
        const auto c = sq.getCentre();
        const float r = side * 0.5f;

        g.setColour (grid);
        g.drawEllipse (sq, 1.0f);
        g.drawLine (c.x, sq.getY(), c.x, sq.getBottom());
        g.drawLine (sq.getX(), c.y, sq.getRight(), c.y);
        g.drawLine (c.x - r * 0.707f, c.y - r * 0.707f, c.x + r * 0.707f, c.y + r * 0.707f);
        g.drawLine (c.x + r * 0.707f, c.y - r * 0.707f, c.x - r * 0.707f, c.y + r * 0.707f);
        g.setColour (textDim);
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText ("L", juce::Rectangle<float> (c.x - r * 0.707f - 14.0f, c.y - r * 0.707f - 14.0f, 12.0f, 12.0f), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (c.x + r * 0.707f + 2.0f, c.y - r * 0.707f - 14.0f, 12.0f, 12.0f), juce::Justification::centred);
        g.drawText ("M", juce::Rectangle<float> (c.x - 6.0f, sq.getY() - 2.0f, 12.0f, 12.0f), juce::Justification::centred);

        float peak = 1.0e-4f;
        for (int i = 0; i < numPoints; ++i)
            peak = std::max (peak, std::max (std::abs (l[(size_t) i]), std::abs (r_[(size_t) i])));
        scale = std::max (peak, scale * 0.97f); // auto-zoom with slow release
        const float k = r * 0.95f / (scale * 1.4142f);

        g.setColour (accent.withAlpha (0.6f));
        for (int i = 0; i < numPoints; i += 2)
        {
            const float s = (l[(size_t) i] - r_[(size_t) i]) * 0.7071f;
            const float m = (l[(size_t) i] + r_[(size_t) i]) * 0.7071f;
            g.fillRect (c.x + s * k, c.y - m * k, 1.5f, 1.5f);
        }
    }

private:
    static constexpr int numPoints = 2048;

    void timerCallback() override
    {
        proc.scope.copyLatest (l.data(), r_.data(), numPoints);
        repaint();
    }

    RedzoneAudioProcessor& proc;
    std::array<float, numPoints> l {}, r_ {};
    float scale = 0.1f;
};

class ImagerTab : public juce::Component
{
public:
    ImagerTab (RedzoneAudioProcessor& p)
        : display (p.apvts), scope (p),
          knobs (p.apvts, id::imgBypass,
                 { { { id::imgX1, "Low / Mid" }, { id::imgX2, "Mid / High" },
                     { id::imgWidth (1), "Low Width" }, { id::imgWidth (2), "Mid Width" }, { id::imgWidth (3), "High Width" },
                     { id::imgMonoFreq, "Bass Mono" }, { id::imgAutoGain, "AUTO GAIN", true } } })
    {
        addAndMakeVisible (knobs);
        addAndMakeVisible (display);
        addAndMakeVisible (scope);
    }

    void resized() override
    {
        auto a = getLocalBounds();
        knobs.setBounds (a.removeFromBottom (juce::jmin (150, a.getHeight() / 2)));
        a.removeFromBottom (6);
        scope.setBounds (a.removeFromRight (a.getHeight()));
        a.removeFromRight (6);
        display.setBounds (a);
    }

private:
    ImagerDisplay display;
    Vectorscope scope;
    ParamPanel knobs;
};
} // namespace rz::ui
