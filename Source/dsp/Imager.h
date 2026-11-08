#pragma once

#include "Splitter.h"

namespace rz
{
// 3-band mid/side stereo imager. Width 1.0 = unchanged, 0 = mono, 2.0 = double side.
class Imager
{
public:
    struct Params
    {
        float xover1 = 250.0f, xover2 = 4000.0f;
        std::array<float, 3> width { 1.0f, 1.0f, 1.0f };
        bool monoLow = false;
    };

    void prepare (double sr, int maxBlock)
    {
        splitter.prepare (sr, maxBlock);
        for (auto& w : widthSm)
        {
            w.reset (sr, 0.02);
            w.setCurrentAndTargetValue (1.0f);
        }
    }

    void setParams (const Params& p)
    {
        splitter.setCrossovers (p.xover1, p.xover2);
        widthSm[0].setTargetValue (p.monoLow ? 0.0f : p.width[0]);
        widthSm[1].setTargetValue (p.width[1]);
        widthSm[2].setTargetValue (p.width[2]);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2) return;
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        const int n = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            float lo[2], mid[2], hi[2];
            splitter.split (0, L[i], lo[0], mid[0], hi[0]);
            splitter.split (1, R[i], lo[1], mid[1], hi[1]);

            applyWidth (lo[0],  lo[1],  widthSm[0].getNextValue());
            applyWidth (mid[0], mid[1], widthSm[1].getNextValue());
            applyWidth (hi[0],  hi[1],  widthSm[2].getNextValue());

            L[i] = lo[0] + mid[0] + hi[0];
            R[i] = lo[1] + mid[1] + hi[1];
        }
    }

private:
    static void applyWidth (float& l, float& r, float w) noexcept
    {
        const float m = 0.5f * (l + r);
        const float s = 0.5f * (l - r) * w;
        l = m + s;
        r = m - s;
    }

    ThreeBandSplitter splitter;
    std::array<juce::SmoothedValue<float>, 3> widthSm;
};
} // namespace rz
