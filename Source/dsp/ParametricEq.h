#pragma once

#include "Biquad.h"
#include "Parameters.h"

namespace rz
{
// 8-slot parametric EQ. Each slot can be switched on/off (created/deleted on the
// graph) and has its own type: Bell, Low/High Shelf, Low/High Cut (12/24/48 dB/oct)
// or Notch. Switching a band on/off is crossfaded, parameter moves are smoothed.
class ParametricEq
{
public:
    static constexpr int maxStages = 4;

    struct BandParams
    {
        bool on = false;
        int type = eqBell;
        float freq = 1000.0f, gainDb = 0.0f, q = 1.0f;
        int slope = 1;
    };

    struct Stages
    {
        std::array<BiquadCoeffs, maxStages> c;
        int n = 1;
    };

    static Stages makeStages (double sr, const BandParams& p)
    {
        Stages s;
        switch (p.type)
        {
            case eqLowShelf:  s.c[0] = BiquadCoeffs::lowShelf (sr, p.freq, p.gainDb, p.q);  break;
            case eqHighShelf: s.c[0] = BiquadCoeffs::highShelf (sr, p.freq, p.gainDb, p.q); break;
            case eqNotch:     s.c[0] = BiquadCoeffs::notch (sr, p.freq, p.q);               break;
            case eqLowCut:
            case eqHighCut:
            {
                // Butterworth cascades; Q acts as resonance on the sharpest stage.
                static constexpr double q12[] = { 0.70710678 };
                static constexpr double q24[] = { 0.54119610, 1.30656296 };
                static constexpr double q48[] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
                const double* qs = p.slope <= 0 ? q12 : (p.slope == 1 ? q24 : q48);
                s.n = p.slope <= 0 ? 1 : (p.slope == 1 ? 2 : 4);
                const double reso = std::max (0.1, (double) p.q) / 0.70710678;
                for (int i = 0; i < s.n; ++i)
                {
                    const double q = (i == s.n - 1) ? qs[i] * reso : qs[i];
                    s.c[(size_t) i] = p.type == eqLowCut ? BiquadCoeffs::highPass (sr, p.freq, q)
                                                         : BiquadCoeffs::lowPass (sr, p.freq, q);
                }
                break;
            }
            case eqBell:
            default:          s.c[0] = BiquadCoeffs::peak (sr, p.freq, p.gainDb, p.q); break;
        }
        return s;
    }

    static double magnitudeDb (const Stages& s, double f, double sr)
    {
        double db = 0.0;
        for (int i = 0; i < s.n; ++i)
            db += s.c[(size_t) i].magnitudeDb (f, sr);
        return db;
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& b : bands)
        {
            b.freq.reset (sr, 0.03);
            b.gain.reset (sr, 0.03);
            b.q.reset (sr, 0.03);
            b.mix.reset (sr, 0.02);
            b.mix.setCurrentAndTargetValue (0.0f);
            b.initialised = false;
            b.dirty = true;
            b.resetState();
        }
    }

    void setBand (int index, const BandParams& p)
    {
        auto& b = bands[(size_t) index];

        if (! b.initialised || (! p.on && b.mix.getCurrentValue() == 0.0f))
        {
            // Inactive: jump straight to the new values (no audible sweep when enabled).
            b.freq.setCurrentAndTargetValue (p.freq);
            b.gain.setCurrentAndTargetValue (p.gainDb);
            b.q.setCurrentAndTargetValue (p.q);
            b.dirty = true;
            b.initialised = true;
        }
        else
        {
            b.freq.setTargetValue (p.freq);
            b.gain.setTargetValue (p.gainDb);
            b.q.setTargetValue (p.q);
        }

        if (p.type != b.type || p.slope != b.slope)
        {
            b.type = p.type;
            b.slope = p.slope;
            b.dirty = true;
            b.resetState();
        }
        b.mix.setTargetValue (p.on ? 1.0f : 0.0f);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        constexpr int chunk = 32;

        for (int start = 0; start < n; start += chunk)
        {
            const int len = juce::jmin (chunk, n - start);

            for (auto& b : bands)
            {
                const float m0 = b.mix.getCurrentValue();
                const float m1 = b.mix.skip (len);
                if (m0 == 0.0f && m1 == 0.0f)
                {
                    b.active = false;
                    continue;
                }
                if (! b.active)
                {
                    b.resetState();
                    b.active = true;
                }

                if (b.dirty || b.freq.isSmoothing() || b.gain.isSmoothing() || b.q.isSmoothing())
                {
                    BandParams p;
                    p.type = b.type;
                    p.slope = b.slope;
                    p.freq = b.freq.skip (len);
                    p.gainDb = b.gain.skip (len);
                    p.q = b.q.skip (len);
                    b.stages = makeStages (sampleRate, p);
                    b.dirty = false;
                }

                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* d = buffer.getWritePointer (ch) + start;
                    auto& st = b.state[(size_t) ch];
                    for (int i = 0; i < len; ++i)
                    {
                        double y = (double) d[i];
                        for (int s = 0; s < b.stages.n; ++s)
                            y = st[(size_t) s].process (b.stages.c[(size_t) s], y);

                        if (m0 == 1.0f && m1 == 1.0f)
                            d[i] = (float) y;
                        else
                        {
                            const float m = m0 + (m1 - m0) * (float) (i + 1) / (float) len;
                            d[i] += m * ((float) y - d[i]);
                        }
                    }
                }
            }
        }
    }

private:
    struct Band
    {
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freq { 1000.0f }, q { 1.0f };
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gain { 0.0f }, mix { 0.0f };
        int type = eqBell, slope = 1;
        bool initialised = false, dirty = true, active = false;
        Stages stages;
        std::array<std::array<BiquadState, maxStages>, 2> state;

        void resetState()
        {
            for (auto& ch : state)
                for (auto& s : ch)
                    s.reset();
        }
    };

    double sampleRate = 44100.0;
    std::array<Band, numEqBands> bands;
};
} // namespace rz
