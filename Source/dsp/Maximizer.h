#pragma once

#include "Common.h"

namespace rz
{
// Look-ahead brickwall limiter running at 2x oversampling (inter-sample peak aware).
//
// Gain computer: per-sample target gain -> release smoothing (instant attack) ->
// sliding minimum over the look-ahead window -> moving average over the same window.
// The min+average combination guarantees the applied gain never exceeds the target
// at the sample that needs it, with a smooth (click free) attack.
// The module always runs (also when bypassed) so reported latency never changes.
class Maximizer
{
public:
    void prepare (double sr, int maxBlock)
    {
        osRate = sr * 2.0;
        os.initProcessing ((size_t) maxBlock);
        os.reset();

        N = std::max (2, (int) std::lround (0.0015 * osRate)); // 1.5 ms look-ahead
        for (auto& d : delay) d.assign ((size_t) N, 0.0f);
        minVal.assign ((size_t) N + 2, 1.0f);
        minIdx.assign ((size_t) N + 2, 0);
        avg.assign ((size_t) N, 1.0f);
        avgSum = (double) N;
        dqHead = dqCount = 0;
        sampleCounter = 0;
        wpos = apos = 0;
        relEnv = 1.0f;

        driveSm.reset (osRate, 0.02);
        driveSm.setCurrentAndTargetValue (1.0f);
        setParams (0.0f, -0.3f, 100.0f, false);
    }

    void reset()
    {
        os.reset();
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (avg.begin(), avg.end(), 1.0f);
        avgSum = (double) N;
        dqHead = dqCount = 0;
        relEnv = 1.0f;
    }

    void setParams (float driveDb, float ceilingDb, float releaseMs, bool isBypassed)
    {
        bypassed = isBypassed;
        driveSm.setTargetValue (isBypassed ? 1.0f : juce::Decibels::decibelsToGain (driveDb));
        ceilingLin = juce::Decibels::decibelsToGain (ceilingDb);
        const float relSamples = std::max (1.0f, releaseMs * 0.001f * (float) osRate);
        relStep = 1.0f - std::exp (-1.0f / relSamples);
    }

    int getLatencySamples() const
    {
        return (int) std::ceil (os.getLatencyInSamples() + 0.5f * (float) (N - 1));
    }

    float getGainReductionDb() const { return grDb.load(); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2) return;

        juce::dsp::AudioBlock<float> block (buffer);
        auto up = os.processSamplesUp (block);
        float* l = up.getChannelPointer (0);
        float* r = up.getChannelPointer (1);
        const int m = (int) up.getNumSamples();

        float minG = 1.0f;
        for (int i = 0; i < m; ++i)
            minG = std::min (minG, tick (l[i], r[i]));

        os.processSamplesDown (block);

        // The down-sampling filter can overshoot by a fraction of a dB; clamp so the
        // ceiling is a hard guarantee (only active while the limiter is engaged).
        if (! bypassed)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    d[i] = juce::jlimit (-ceilingLin, ceilingLin, d[i]);
            }
        }
        grDb.store (juce::Decibels::gainToDecibels (minG, -60.0f));
    }

private:
    // Processes one oversampled stereo frame, returns the applied gain.
    float tick (float& l, float& r) noexcept
    {
        const float drive = driveSm.getNextValue();
        const float xl = l * drive;
        const float xr = r * drive;

        // 1. target gain
        const float peak = std::max (std::abs (xl), std::abs (xr));
        const float target = (bypassed || peak <= ceilingLin) ? 1.0f : ceilingLin / peak;

        // 2. release smoothing, instant attack
        relEnv = std::min (target, relEnv + (1.0f - relEnv) * relStep);

        // 3. sliding minimum over the last N values (monotonic deque, circular)
        const size_t cap = minVal.size();
        while (dqCount > 0)
        {
            const size_t back = (dqHead + (size_t) dqCount - 1) % cap;
            if (minVal[back] >= relEnv) --dqCount; else break;
        }
        {
            const size_t pos = (dqHead + (size_t) dqCount) % cap;
            minVal[pos] = relEnv;
            minIdx[pos] = sampleCounter;
            ++dqCount;
        }
        while (minIdx[dqHead] <= sampleCounter - (std::int64_t) N)
        {
            dqHead = (dqHead + 1) % cap;
            --dqCount;
        }
        const float windowMin = minVal[dqHead];
        ++sampleCounter;

        // 4. moving average of the minimum (smooth attack)
        avgSum += (double) windowMin - (double) avg[(size_t) apos];
        avg[(size_t) apos] = windowMin;
        if (++apos >= N) apos = 0;
        const float g = std::min (1.0f, (float) (avgSum / (double) N));

        // 5. delay audio by N-1 samples and apply the gain
        delay[0][(size_t) wpos] = xl;
        delay[1][(size_t) wpos] = xr;
        const int rpos = (wpos + 1) % N;
        l = delay[0][(size_t) rpos] * g;
        r = delay[1][(size_t) rpos] * g;
        if (++wpos >= N) wpos = 0;

        return g;
    }

    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };

    double osRate = 88200.0;
    int N = 132;
    std::array<std::vector<float>, 2> delay;
    std::vector<float> minVal, avg;
    std::vector<std::int64_t> minIdx;
    size_t dqHead = 0;
    int dqCount = 0;
    std::int64_t sampleCounter = 0;
    int wpos = 0, apos = 0;
    double avgSum = 0.0;

    float relEnv = 1.0f, relStep = 0.001f, ceilingLin = 0.966f;
    bool bypassed = false;
    juce::SmoothedValue<float> driveSm { 1.0f };
    std::atomic<float> grDb { 0.0f };
};
} // namespace rz
