#pragma once

#include "Biquad.h"
#include "Parameters.h"

namespace rz
{
// 6-band parametric EQ: low shelf, 4 peaking bands, high shelf.
class ParametricEq
{
public:
    struct BandParams
    {
        float freq = 1000.0f, gainDb = 0.0f, q = 1.0f;
    };

    static BiquadCoeffs makeCoeffs (int band, double sr, const BandParams& p)
    {
        if (band == 0)               return BiquadCoeffs::lowShelf (sr, p.freq, p.gainDb, p.q);
        if (band == numEqBands - 1)  return BiquadCoeffs::highShelf (sr, p.freq, p.gainDb, p.q);
        return BiquadCoeffs::peak (sr, p.freq, p.gainDb, p.q);
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        for (int b = 0; b < numEqBands; ++b)
        {
            freqSm[b].reset (sr, 0.03);
            gainSm[b].reset (sr, 0.03);
            qSm[b].reset (sr, 0.03);
            dirty[b] = true;
            for (auto& s : state[b]) s.reset();
        }
        initialised = false;
    }

    void setBand (int b, const BandParams& p)
    {
        if (! initialised)
        {
            freqSm[b].setCurrentAndTargetValue (p.freq);
            gainSm[b].setCurrentAndTargetValue (p.gainDb);
            qSm[b].setCurrentAndTargetValue (p.q);
            dirty[b] = true;
            if (b == numEqBands - 1) initialised = true;
            return;
        }
        freqSm[b].setTargetValue (p.freq);
        gainSm[b].setTargetValue (p.gainDb);
        qSm[b].setTargetValue (p.q);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        constexpr int chunk = 32;

        for (int start = 0; start < n; start += chunk)
        {
            const int len = juce::jmin (chunk, n - start);

            for (int b = 0; b < numEqBands; ++b)
            {
                if (dirty[b] || freqSm[b].isSmoothing() || gainSm[b].isSmoothing() || qSm[b].isSmoothing())
                {
                    BandParams p;
                    p.freq   = freqSm[b].skip (len);
                    p.gainDb = gainSm[b].skip (len);
                    p.q      = qSm[b].skip (len);
                    coeffs[b] = makeCoeffs (b, sampleRate, p);
                    dirty[b] = false;
                }
            }

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch) + start;
                for (int b = 0; b < numEqBands; ++b)
                {
                    const auto& c = coeffs[b];
                    auto& s = state[b][ch];
                    for (int i = 0; i < len; ++i)
                        d[i] = (float) s.process (c, (double) d[i]);
                }
            }
        }
    }

private:
    double sampleRate = 44100.0;
    bool initialised = false;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, numEqBands> freqSm, qSm;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, numEqBands> gainSm;
    std::array<BiquadCoeffs, numEqBands> coeffs;
    std::array<std::array<BiquadState, 2>, numEqBands> state;
    std::array<bool, numEqBands> dirty {};
};
} // namespace rz
