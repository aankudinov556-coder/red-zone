#pragma once

#include "Common.h"

namespace rz
{
// Phase-coherent 3-band split built from two Linkwitz-Riley (LR4) crossovers.
// The low band is run through an all-pass at the second crossover so that
// low + mid + high sums back to a flat (all-pass) response.
class ThreeBandSplitter
{
public:
    void prepare (double sr, int maxBlock)
    {
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
        xo1.prepare (spec);
        xo2.prepare (spec);
        ap.prepare (spec);
        xo1.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        xo2.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        ap.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        lastF1 = lastF2 = -1.0f;
        reset();
    }

    void reset()
    {
        xo1.reset();
        xo2.reset();
        ap.reset();
    }

    void setCrossovers (float f1, float f2)
    {
        f2 = std::max (f2, f1 * 1.5f);
        if (f1 != lastF1) { xo1.setCutoffFrequency (f1); lastF1 = f1; }
        if (f2 != lastF2) { xo2.setCutoffFrequency (f2); ap.setCutoffFrequency (f2); lastF2 = f2; }
    }

    void split (int ch, float x, float& lo, float& mid, float& hi)
    {
        float l = 0.0f, h = 0.0f, m = 0.0f, hh = 0.0f;
        xo1.processSample (ch, x, l, h);
        xo2.processSample (ch, h, m, hh);
        lo  = ap.processSample (ch, l);
        mid = m;
        hi  = hh;
    }

private:
    juce::dsp::LinkwitzRileyFilter<float> xo1, xo2, ap;
    float lastF1 = -1.0f, lastF2 = -1.0f;
};
} // namespace rz
