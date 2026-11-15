#pragma once

#include "Common.h"

namespace rz
{
// 3-band stereo imager that only touches the Side signal.
//
//  * Mid (L+R) is never filtered, so there is no phase smearing of the mono content.
//  * Side is split with complementary (subtractive) filters: lo + mid + hi == S exactly,
//    so width = 100% on every band reproduces the input bit for bit.
//  * Optional "bass mono": side content below the chosen frequency is removed.
//  * Optional auto gain keeps the overall level constant when widening/narrowing.
class Imager
{
public:
    struct Params
    {
        float xover1 = 250.0f, xover2 = 4000.0f;
        std::array<float, 3> width { 1.0f, 1.0f, 1.0f };
        float monoFreq = 0.0f; // < 20 Hz = off
        bool autoGain = true;
    };

    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 1 };
        for (auto* f : { &lp1, &lp2, &lpMono })
        {
            f->prepare (spec);
            f->setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            f->reset();
        }
        lastX1 = lastX2 = lastMono = -1.0f;

        for (auto& w : widthSm)
        {
            w.reset (sr, 0.03);
            w.setCurrentAndTargetValue (1.0f);
        }
        monoMix.reset (sr, 0.03);
        monoMix.setCurrentAndTargetValue (0.0f);

        powerCoeff = (float) std::exp (-1.0 / (0.3 * sr));
        gainCoeff = (float) std::exp (-1.0 / (0.05 * sr));
        powerIn = powerOut = 0.0f;
        gain = 1.0f;
    }

    void setParams (const Params& p)
    {
        const float x1 = juce::jlimit (20.0f, 2000.0f, p.xover1);
        const float x2 = std::max (p.xover2, x1 * 1.5f);
        if (x1 != lastX1) { lp1.setCutoffFrequency (x1); lastX1 = x1; }
        if (x2 != lastX2) { lp2.setCutoffFrequency (x2); lastX2 = x2; }

        const bool monoOn = p.monoFreq >= 20.0f;
        if (monoOn && p.monoFreq != lastMono) { lpMono.setCutoffFrequency (p.monoFreq); lastMono = p.monoFreq; }
        monoMix.setTargetValue (monoOn ? 1.0f : 0.0f);

        for (size_t b = 0; b < 3; ++b)
            widthSm[b].setTargetValue (juce::jlimit (0.0f, 2.0f, p.width[b]));
        autoGain = p.autoGain;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2) return;
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        const int n = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            const float s = 0.5f * (L[i] - R[i]);

            // complementary split of the side signal
            const float sLo  = lp1.processSample (0, s);
            const float rest = s - sLo;
            const float sMid = lp2.processSample (0, rest);
            const float sHi  = rest - sMid;

            const float w1 = widthSm[0].getNextValue();
            const float w2 = widthSm[1].getNextValue();
            const float w3 = widthSm[2].getNextValue();
            float sOut = s + (w1 - 1.0f) * sLo + (w2 - 1.0f) * sMid + (w3 - 1.0f) * sHi;

            // bass mono: remove side content below monoFreq
            const float mm = monoMix.getNextValue();
            const float sLow = lpMono.processSample (0, sOut);
            sOut -= mm * sLow;

            // auto gain: match output power to input power (slow, limited)
            const float pIn  = m * m + s * s;
            const float pOut = m * m + sOut * sOut;
            powerIn  = powerCoeff * powerIn  + (1.0f - powerCoeff) * pIn;
            powerOut = powerCoeff * powerOut + (1.0f - powerCoeff) * pOut;
            float target = 1.0f;
            if (autoGain && powerOut > 1.0e-9f && powerIn > 1.0e-9f && powerIn != powerOut)
                target = juce::jlimit (0.5f, 1.41f, std::sqrt (powerIn / powerOut));
            gain = target + gainCoeff * (gain - target);
            if (std::abs (gain - target) < 1.0e-6f) gain = target; // snap (float rounding can stall)

            const bool neutral = (w1 == 1.0f && w2 == 1.0f && w3 == 1.0f && mm == 0.0f && gain == 1.0f);
            if (! neutral)
            {
                L[i] = (m + sOut) * gain;
                R[i] = (m - sOut) * gain;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    juce::dsp::LinkwitzRileyFilter<float> lp1, lp2, lpMono;
    float lastX1 = -1.0f, lastX2 = -1.0f, lastMono = -1.0f;
    std::array<juce::SmoothedValue<float>, 3> widthSm;
    juce::SmoothedValue<float> monoMix;
    bool autoGain = true;
    float powerCoeff = 0.9999f, gainCoeff = 0.999f, powerIn = 0.0f, powerOut = 0.0f, gain = 1.0f;
};
} // namespace rz
