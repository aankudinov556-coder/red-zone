#pragma once

#include "../PluginProcessor.h"
#include "RedzoneLookAndFeel.h"

namespace rz::ui
{
// Real-time spectrum of the plugin output with the EQ response drawn on top.
class SpectrumComponent : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumComponent (RedzoneAudioProcessor& p)
        : proc (p), fft (fftOrder), window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false)
    {
        smoothed.fill (floorDb);
        fftData.fill (0.0f);
        startTimerHz (30);
    }

    ~SpectrumComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        auto b = getLocalBounds().toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (b.reduced (2.0f), 6.0f);

        const auto area = plotArea();

        // grid
        g.setFont (juce::Font (10.0f));
        const float freqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        for (float f : freqs)
        {
            const float x = xForFreq (f, area);
            g.setColour (grid);
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            g.setColour (textDim);
            g.drawText (f >= 1000 ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x - 18.0f, area.getBottom() - 14.0f, 36.0f, 12.0f), juce::Justification::centred);
        }
        for (int db = -80; db <= 0; db += 20)
        {
            const float y = yForDb ((float) db, area);
            g.setColour (grid);
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
            g.setColour (textDim);
            g.drawText (juce::String (db), juce::Rectangle<float> (area.getX() + 2.0f, y - 12.0f, 28.0f, 12.0f), juce::Justification::left);
        }

        g.setColour (accent.withAlpha (0.28f));
        g.fillPath (spectrumFill);
        g.setColour (accent);
        g.strokePath (spectrumLine, juce::PathStrokeType (1.4f));

        g.setColour (text.withAlpha (0.9f));
        g.strokePath (eqCurve, juce::PathStrokeType (1.8f));
    }

    void resized() override { rebuildPaths(); }

private:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr float floorDb = -100.0f;
    static constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
    static constexpr float topDb = 0.0f, bottomDb = -90.0f;
    static constexpr float eqRangeDb = 18.0f;

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (6.0f); }

    static float xForFreq (float f, juce::Rectangle<float> a)
    {
        return a.getX() + a.getWidth() * std::log (f / minFreq) / std::log (maxFreq / minFreq);
    }

    static float yForDb (float db, juce::Rectangle<float> a)
    {
        return juce::jmap (juce::jlimit (bottomDb, topDb, db), bottomDb, topDb, a.getBottom(), a.getY());
    }

    void timerCallback() override
    {
        proc.spectrum.copyLatest (fftData.data(), fftSize);
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int i = 0; i < fftSize / 2; ++i)
        {
            // Hann window coherent gain is 0.5 -> amplitude = 4 * |X| / N.
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

        spectrumLine.clear();
        const int w = (int) area.getWidth();
        for (int px = 0; px <= w; ++px)
        {
            const float f = minFreq * std::pow (maxFreq / minFreq, (float) px / (float) w);
            const float binPos = f / binHz;
            const int b0 = juce::jlimit (0, fftSize / 2 - 2, (int) binPos);
            const float frac = binPos - (float) b0;
            float db = smoothed[(size_t) b0] * (1.0f - frac) + smoothed[(size_t) b0 + 1] * frac;
            db += 4.5f * std::log2 (f / 1000.0f); // pink-ish display tilt
            const float x = area.getX() + (float) px;
            const float y = yForDb (db, area);
            if (px == 0) spectrumLine.startNewSubPath (x, y); else spectrumLine.lineTo (x, y);
        }

        spectrumFill = spectrumLine;
        spectrumFill.lineTo (area.getRight(), area.getBottom());
        spectrumFill.lineTo (area.getX(), area.getBottom());
        spectrumFill.closeSubPath();

        // EQ response
        eqCurve.clear();
        if (proc.getParam (rz::id::eqBypass) > 0.5f) return;

        std::array<rz::BiquadCoeffs, rz::numEqBands> c;
        for (int b = 0; b < rz::numEqBands; ++b)
            c[(size_t) b] = rz::ParametricEq::makeCoeffs (b, sr, { proc.getParam (rz::id::eqFreq (b + 1)),
                                                                    proc.getParam (rz::id::eqGain (b + 1)),
                                                                    proc.getParam (rz::id::eqQ (b + 1)) });
        const float cy = area.getCentreY();
        const float scale = area.getHeight() * 0.35f / eqRangeDb;
        for (int px = 0; px <= w; ++px)
        {
            const double f = (double) minFreq * std::pow ((double) maxFreq / minFreq, (double) px / (double) w);
            double db = 0.0;
            for (auto& k : c) db += k.magnitudeDb (f, sr);
            const float x = area.getX() + (float) px;
            const float y = cy - (float) db * scale;
            if (px == 0) eqCurve.startNewSubPath (x, y); else eqCurve.lineTo (x, y);
        }
    }

    RedzoneAudioProcessor& proc;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize * 2> fftData;
    std::array<float, fftSize / 2> smoothed;
    juce::Path spectrumLine, spectrumFill, eqCurve;
};
} // namespace rz::ui
