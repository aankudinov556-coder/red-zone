#pragma once

#include "Parameters.h"
#include "Splitter.h"

namespace rz
{
// 3-band compressor with linked-stereo detection and a soft knee.
class MultibandCompressor
{
public:
    struct BandParams
    {
        float thresholdDb = -20.0f, ratio = 2.0f, attackMs = 20.0f, releaseMs = 200.0f, gainDb = 0.0f;
    };

    struct Params
    {
        float xover1 = 200.0f, xover2 = 3000.0f;
        std::array<BandParams, numMbBands> bands;
    };

    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        splitter.prepare (sr, maxBlock);
        for (auto& b : bands)
        {
            b.env = -120.0f;
            b.makeup.reset (sr, 0.02);
            b.makeup.setCurrentAndTargetValue (1.0f);
        }
    }

    void setParams (const Params& p)
    {
        splitter.setCrossovers (p.xover1, p.xover2);
        for (int i = 0; i < numMbBands; ++i)
        {
            auto& b = bands[(size_t) i];
            const auto& s = p.bands[(size_t) i];
            b.threshold = s.thresholdDb;
            b.ratio = std::max (1.0f, s.ratio);
            b.attCoeff = std::exp (-1.0f / (std::max (0.1f, s.attackMs) * 0.001f * (float) sampleRate));
            b.relCoeff = std::exp (-1.0f / (std::max (1.0f, s.releaseMs) * 0.001f * (float) sampleRate));
            b.makeup.setTargetValue (juce::Decibels::decibelsToGain (s.gainDb));
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2) return;
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        const int n = buffer.getNumSamples();

        std::array<float, numMbBands> minGr { 0.0f, 0.0f, 0.0f };

        for (int i = 0; i < n; ++i)
        {
            float lo[2], mid[2], hi[2];
            splitter.split (0, L[i], lo[0], mid[0], hi[0]);
            splitter.split (1, R[i], lo[1], mid[1], hi[1]);

            float* sig[numMbBands] = { lo, mid, hi };
            float outL = 0.0f, outR = 0.0f;

            for (int b = 0; b < numMbBands; ++b)
            {
                float grDb = 0.0f;
                const float level = std::max (std::abs (sig[b][0]), std::abs (sig[b][1]));
                const float g = bands[(size_t) b].gain (level, grDb);
                minGr[(size_t) b] = std::min (minGr[(size_t) b], grDb);
                outL += sig[b][0] * g;
                outR += sig[b][1] * g;
            }

            L[i] = outL;
            R[i] = outR;
        }

        for (int b = 0; b < numMbBands; ++b)
            grDb[(size_t) b].store (minGr[(size_t) b]);
    }

    float getGainReductionDb (int band) const { return grDb[(size_t) band].load(); }

private:
    struct Band
    {
        float env = -120.0f, threshold = -20.0f, ratio = 2.0f, attCoeff = 0.99f, relCoeff = 0.999f;
        juce::SmoothedValue<float> makeup { 1.0f };

        float gain (float level, float& grOut) noexcept
        {
            const float levelDb = juce::Decibels::gainToDecibels (level, -120.0f);
            const float c = levelDb > env ? attCoeff : relCoeff;
            env = levelDb + c * (env - levelDb);

            constexpr float knee = 6.0f;
            const float over = env - threshold;
            const float slope = 1.0f / ratio - 1.0f;
            float gr = 0.0f;
            if (2.0f * over < -knee)              gr = 0.0f;
            else if (2.0f * std::abs (over) <= knee)
            {
                const float x = over + knee * 0.5f;
                gr = slope * x * x / (2.0f * knee);
            }
            else                                   gr = slope * over;

            grOut = gr;
            return juce::Decibels::decibelsToGain (gr) * makeup.getNextValue();
        }
    };

    double sampleRate = 44100.0;
    ThreeBandSplitter splitter;
    std::array<Band, numMbBands> bands;
    std::array<std::atomic<float>, numMbBands> grDb {};
};
} // namespace rz
