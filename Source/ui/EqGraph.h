#pragma once

#include "../PluginProcessor.h"
#include "RedzoneLookAndFeel.h"

namespace rz::ui
{
inline juce::Colour bandColour (int b)
{
    static const juce::Colour c[] = { juce::Colour (0xffff4d4d), juce::Colour (0xffff9f40), juce::Colour (0xfff5d442),
                                      juce::Colour (0xff6ee06e), juce::Colour (0xff3fd0c9), juce::Colour (0xff4f9dff),
                                      juce::Colour (0xffa678ff), juce::Colour (0xffff6fcf) };
    return c[(size_t) juce::jlimit (0, 7, b)];
}

// Spectrum analyser + interactive EQ editor.
//   double-click empty area : add band          drag point     : freq / gain
//   double-click point      : delete band       Shift + drag   : fine adjust
//   mouse wheel on point    : Q                 right-click    : type / slope / delete
class EqGraph : public juce::Component, private juce::Timer
{
public:
    std::function<void (int)> onSelectionChanged;

    explicit EqGraph (RedzoneAudioProcessor& p)
        : proc (p), fft (fftOrder), window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false)
    {
        auto get = [&p] (const juce::String& pid) { return p.apvts.getParameter (pid); };
        for (int b = 0; b < numEqBands; ++b)
        {
            auto& r = refs[(size_t) b];
            r.on = get (id::eqOn (b + 1));
            r.type = get (id::eqType (b + 1));
            r.freq = get (id::eqFreq (b + 1));
            r.gain = get (id::eqGain (b + 1));
            r.q = get (id::eqQ (b + 1));
            r.slope = get (id::eqSlope (b + 1));
        }
        bypass = get (id::eqBypass);

        smoothed.fill (floorDb);
        fftData.fill (0.0f);
        setWantsKeyboardFocus (false);
        startTimerHz (30);
    }

    ~EqGraph() override { stopTimer(); }

    int getSelectedBand() const { return selected; }

    void setSelectedBand (int b)
    {
        if (b == selected) return;
        selected = b;
        if (onSelectionChanged) onSelectionChanged (selected);
        repaint();
    }

    void deleteBand (int b)
    {
        if (b < 0) return;
        setValue (refs[(size_t) b].on, 0.0f);
        if (selected == b) setSelectedBand (-1);
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        g.setColour (panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

        const auto area = plotArea();

        // grid: frequencies
        g.setFont (juce::Font (10.0f));
        const float freqs[] = { 30, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        for (float f : freqs)
        {
            const float x = xForFreq (f);
            g.setColour (grid);
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            g.setColour (textDim);
            g.drawText (f >= 1000 ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x - 18.0f, area.getBottom() - 13.0f, 36.0f, 12.0f), juce::Justification::centred);
        }
        // grid: EQ gain
        for (int db = -12; db <= 12; db += 6)
        {
            const float y = yForGain ((float) db);
            g.setColour (db == 0 ? grid.brighter (0.4f) : grid);
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
            g.setColour (textDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (area.getRight() - 30.0f, y - 12.0f, 28.0f, 12.0f),
                        juce::Justification::right);
        }

        // spectrum
        g.setColour (accent.withAlpha (0.18f));
        g.fillPath (spectrumFill);
        g.setColour (accent.withAlpha (0.55f));
        g.strokePath (spectrumLine, juce::PathStrokeType (1.0f));

        const bool eqBypassed = bypass->getValue() > 0.5f;

        // individual band curves
        for (int b = 0; b < numEqBands; ++b)
        {
            if (bandCurves[(size_t) b].isEmpty()) continue;
            const auto col = bandColour (b);
            if (b == selected)
            {
                g.setColour (col.withAlpha (0.18f));
                g.fillPath (bandFills[(size_t) b]);
            }
            g.setColour (col.withAlpha (b == selected ? 0.9f : 0.35f));
            g.strokePath (bandCurves[(size_t) b], juce::PathStrokeType (b == selected ? 1.5f : 1.0f));
        }

        // total curve
        g.setColour (eqBypassed ? textDim : text);
        g.strokePath (totalCurve, juce::PathStrokeType (2.0f));

        // points
        for (int b = 0; b < numEqBands; ++b)
        {
            const auto bp = readBand (b);
            if (! bp.on) continue;
            const auto pt = pointFor (b);
            const bool sel = b == selected, hot = b == hovered;
            const float r = sel ? 9.0f : (hot ? 8.0f : 7.0f);
            g.setColour (bandColour (b));
            g.fillEllipse (pt.x - r, pt.y - r, r * 2.0f, r * 2.0f);
            g.setColour (sel ? juce::Colours::white : juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (pt.x - r, pt.y - r, r * 2.0f, r * 2.0f, sel ? 2.0f : 1.0f);
            g.setColour (juce::Colours::black);
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (pt.x - r, pt.y - r, r * 2.0f, r * 2.0f), juce::Justification::centred);
        }

        // tooltip
        const int tipBand = dragging >= 0 ? dragging : hovered;
        if (tipBand >= 0 && readBand (tipBand).on)
        {
            const auto bp = readBand (tipBand);
            const auto pt = pointFor (tipBand);
            juce::String txt = formatFreq (bp.freq);
            if (hasGain (bp.type)) txt << "  " << (bp.gainDb > 0 ? "+" : "") << juce::String (bp.gainDb, 1) << " dB";
            txt << "  Q " << juce::String (bp.q, 2);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            const float w = (float) g.getCurrentFont().getStringWidth (txt) + 14.0f;
            auto box = juce::Rectangle<float> (pt.x - w * 0.5f, pt.y - 34.0f, w, 20.0f);
            if (box.getY() < area.getY()) box.setY (pt.y + 14.0f);
            box = box.constrainedWithin (area);
            g.setColour (background.withAlpha (0.9f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (bandColour (tipBand));
            g.drawRoundedRectangle (box, 4.0f, 1.0f);
            g.setColour (text);
            g.drawText (txt, box, juce::Justification::centred);
        }

        if (noBands)
        {
            g.setColour (textDim);
            g.setFont (juce::Font (13.0f));
            g.drawText (juce::String::fromUTF8 ("Double-click to add an EQ band"), area.withTrimmedTop (area.getHeight() * 0.15f).removeFromTop (20.0f),
                        juce::Justification::centred);
        }
    }

    void resized() override { rebuildPaths(); }

    //==============================================================================
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = hitTestBand (e.position);
        if (h != hovered) { hovered = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hovered != -1) { hovered = -1; repaint(); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int h = hitTestBand (e.position);

        if (e.mods.isPopupMenu())
        {
            if (h >= 0)
            {
                setSelectedBand (h);
                showBandMenu (h);
            }
            return;
        }

        setSelectedBand (h);
        if (h < 0) return;

        dragging = h;
        dragStart = e.position;
        const auto bp = readBand (h);
        startFreq = bp.freq;
        startGain = bp.gainDb;
        lastFine = e.mods.isShiftDown();
        refs[(size_t) h].freq->beginChangeGesture();
        refs[(size_t) h].gain->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0) return;

        // re-anchor when Shift is pressed/released mid-drag so the point does not jump
        const bool fine = e.mods.isShiftDown();
        if (fine != lastFine)
        {
            const auto bp = readBand (dragging);
            startFreq = bp.freq;
            startGain = bp.gainDb;
            dragStart = e.position;
            lastFine = fine;
        }

        const float k = fine ? 0.2f : 1.0f;
        const auto area = plotArea();
        const float dx = (e.position.x - dragStart.x) * k;
        const float dy = (e.position.y - dragStart.y) * k;

        const float f = startFreq * std::pow (maxFreq / minFreq, dx / area.getWidth());
        setValue (refs[(size_t) dragging].freq, juce::jlimit (minFreq, maxFreq, f));

        if (hasGain (readBand (dragging).type))
            setValue (refs[(size_t) dragging].gain, juce::jlimit (-eqRangeDb, eqRangeDb, startGain - dy / pixelsPerDb()));

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging >= 0)
        {
            refs[(size_t) dragging].freq->endChangeGesture();
            refs[(size_t) dragging].gain->endChangeGesture();
        }
        dragging = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = hitTestBand (e.position);
        if (h >= 0)
        {
            gesture (refs[(size_t) h].on, 0.0f);
            setSelectedBand (-1);
            hovered = -1;
            return;
        }

        int slot = -1;
        for (int b = 0; b < numEqBands; ++b)
            if (! readBand (b).on) { slot = b; break; }
        if (slot < 0) return; // all 8 bands in use

        const auto area = plotArea();
        if (! area.contains (e.position)) return;

        auto& r = refs[(size_t) slot];
        const float f = freqForX (e.position.x);
        const float gdb = juce::jlimit (-eqRangeDb, eqRangeDb, gainForY (e.position.y));
        gesture (r.type, (float) eqBell);
        gesture (r.slope, 1.0f);
        gesture (r.q, 1.0f);
        gesture (r.freq, f);
        gesture (r.gain, std::abs (gdb) < 0.3f ? 0.0f : gdb);
        gesture (r.on, 1.0f);
        setSelectedBand (slot);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        int b = hitTestBand (e.position);
        if (b < 0) b = selected;
        if (b < 0 || ! readBand (b).on) return;
        const float q = readBand (b).q * std::exp (w.deltaY * (e.mods.isShiftDown() ? 0.5f : 2.0f));
        gesture (refs[(size_t) b].q, q);
        repaint();
    }

private:
    struct Refs
    {
        juce::RangedAudioParameter *on = nullptr, *type = nullptr, *freq = nullptr, *gain = nullptr, *q = nullptr, *slope = nullptr;
    };

    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr float floorDb = -100.0f;
    static constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
    static constexpr float topDb = 0.0f, bottomDb = -90.0f;
    static constexpr float eqRangeDb = 18.0f;

    static bool hasGain (int type) { return type == eqBell || type == eqLowShelf || type == eqHighShelf; }

    static juce::String formatFreq (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz" : juce::String (f, 0) + " Hz";
    }

    static void setValue (juce::RangedAudioParameter* p, float v)
    {
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->convertTo0to1 (v)));
    }

    static void gesture (juce::RangedAudioParameter* p, float v)
    {
        p->beginChangeGesture();
        setValue (p, v);
        p->endChangeGesture();
    }

    ParametricEq::BandParams readBand (int b) const
    {
        const auto& r = refs[(size_t) b];
        ParametricEq::BandParams p;
        p.on = r.on->getValue() > 0.5f;
        p.type = (int) std::lround (r.type->convertFrom0to1 (r.type->getValue()));
        p.freq = r.freq->convertFrom0to1 (r.freq->getValue());
        p.gainDb = r.gain->convertFrom0to1 (r.gain->getValue());
        p.q = r.q->convertFrom0to1 (r.q->getValue());
        p.slope = (int) std::lround (r.slope->convertFrom0to1 (r.slope->getValue()));
        return p;
    }

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (6.0f); }

    float xForFreq (float f) const
    {
        const auto a = plotArea();
        return a.getX() + a.getWidth() * std::log (f / minFreq) / std::log (maxFreq / minFreq);
    }

    float freqForX (float x) const
    {
        const auto a = plotArea();
        return juce::jlimit (minFreq, maxFreq, minFreq * std::pow (maxFreq / minFreq, (x - a.getX()) / a.getWidth()));
    }

    float pixelsPerDb() const { return plotArea().getHeight() * 0.42f / eqRangeDb; }
    float yForGain (float db) const { return plotArea().getCentreY() - db * pixelsPerDb(); }
    float gainForY (float y) const { return (plotArea().getCentreY() - y) / pixelsPerDb(); }

    float yForSpectrumDb (float db) const
    {
        const auto a = plotArea();
        return juce::jmap (juce::jlimit (bottomDb, topDb, db), bottomDb, topDb, a.getBottom(), a.getY());
    }

    juce::Point<float> pointFor (int b) const
    {
        const auto bp = readBand (b);
        return { xForFreq (bp.freq), yForGain (hasGain (bp.type) ? bp.gainDb : 0.0f) };
    }

    int hitTestBand (juce::Point<float> pos) const
    {
        int best = -1;
        float bestD = 12.0f;
        for (int b = 0; b < numEqBands; ++b)
        {
            if (! readBand (b).on) continue;
            const float d = pointFor (b).getDistanceFrom (pos);
            if (d < bestD) { bestD = d; best = b; }
        }
        return best;
    }

    void showBandMenu (int b)
    {
        const auto bp = readBand (b);
        juce::PopupMenu m, slopes;
        const auto types = eqTypeNames();
        for (int i = 0; i < types.size(); ++i)
            m.addItem (100 + i, types[i], true, bp.type == i);

        const auto sn = eqSlopeNames();
        for (int i = 0; i < sn.size(); ++i)
            slopes.addItem (200 + i, sn[i], true, bp.slope == i);
        m.addSubMenu ("Slope", slopes, bp.type == eqLowCut || bp.type == eqHighCut);
        m.addSeparator();
        m.addItem (1, "Delete band");

        juce::Component::SafePointer<EqGraph> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe, b] (int result)
        {
            if (safe == nullptr || result == 0) return;
            auto& r = safe->refs[(size_t) b];
            if (result == 1)                         safe->deleteBand (b);
            else if (result >= 100 && result < 200)  gesture (r.type, (float) (result - 100));
            else if (result >= 200)                  gesture (r.slope, (float) (result - 200));
            safe->repaint();
        });
    }

    void timerCallback() override
    {
        proc.spectrum.copyLatest (fftData.data(), fftSize);
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int i = 0; i < fftSize / 2; ++i)
        {
            // Hann coherent gain 0.5 -> amplitude = 4 * |X| / N
            const float db = juce::Decibels::gainToDecibels (fftData[(size_t) i] * 4.0f / (float) fftSize, floorDb);
            auto& s = smoothed[(size_t) i];
            s = db > s ? db : s - 1.5f;
        }
        rebuildPaths();
        repaint();
    }

    void rebuildPaths()
    {
        const auto area = plotArea();
        if (area.isEmpty()) return;

        const double sr = juce::jmax (8000.0, proc.getSampleRate());
        const float binHz = (float) sr / (float) fftSize;
        const int w = juce::jmax (1, (int) area.getWidth());

        // spectrum
        spectrumLine.clear();
        for (int px = 0; px <= w; ++px)
        {
            const float f = minFreq * std::pow (maxFreq / minFreq, (float) px / (float) w);
            const float binPos = f / binHz;
            const int b0 = juce::jlimit (0, fftSize / 2 - 2, (int) binPos);
            const float frac = juce::jlimit (0.0f, 1.0f, binPos - (float) b0);
            float db = smoothed[(size_t) b0] * (1.0f - frac) + smoothed[(size_t) b0 + 1] * frac;
            if (db > floorDb + 1.0f)
                db += 4.5f * std::log2 (f / 1000.0f); // pink-ish display tilt
            else
                db = bottomDb;                         // silence stays at the bottom
            const float x = area.getX() + (float) px;
            const float y = yForSpectrumDb (db);
            if (px == 0) spectrumLine.startNewSubPath (x, y); else spectrumLine.lineTo (x, y);
        }
        spectrumFill = spectrumLine;
        spectrumFill.lineTo (area.getRight(), area.getBottom());
        spectrumFill.lineTo (area.getX(), area.getBottom());
        spectrumFill.closeSubPath();

        // EQ curves
        std::vector<double> total ((size_t) w + 1, 0.0);
        noBands = true;
        const float zeroY = yForGain (0.0f);
        for (int b = 0; b < numEqBands; ++b)
        {
            auto& curve = bandCurves[(size_t) b];
            auto& fill = bandFills[(size_t) b];
            curve.clear();
            fill.clear();
            const auto bp = readBand (b);
            if (! bp.on) continue;
            noBands = false;

            const auto st = ParametricEq::makeStages (sr, bp);
            for (int px = 0; px <= w; ++px)
            {
                const double f = (double) minFreq * std::pow ((double) maxFreq / minFreq, (double) px / (double) w);
                const double db = ParametricEq::magnitudeDb (st, f, sr);
                total[(size_t) px] += db;
                const float x = area.getX() + (float) px;
                const float y = juce::jlimit (area.getY(), area.getBottom(), yForGain ((float) db));
                if (px == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
            }
            fill = curve;
            fill.lineTo (area.getRight(), zeroY);
            fill.lineTo (area.getX(), zeroY);
            fill.closeSubPath();
        }

        totalCurve.clear();
        for (int px = 0; px <= w; ++px)
        {
            const float x = area.getX() + (float) px;
            const float y = juce::jlimit (area.getY(), area.getBottom(), yForGain ((float) total[(size_t) px]));
            if (px == 0) totalCurve.startNewSubPath (x, y); else totalCurve.lineTo (x, y);
        }
    }

    RedzoneAudioProcessor& proc;
    std::array<Refs, numEqBands> refs;
    juce::RangedAudioParameter* bypass = nullptr;

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize * 2> fftData;
    std::array<float, fftSize / 2> smoothed;
    juce::Path spectrumLine, spectrumFill, totalCurve;
    std::array<juce::Path, numEqBands> bandCurves, bandFills;
    bool noBands = true;

    int selected = -1, hovered = -1, dragging = -1;
    juce::Point<float> dragStart;
    float startFreq = 1000.0f, startGain = 0.0f;
    bool lastFine = false;
};
} // namespace rz::ui
