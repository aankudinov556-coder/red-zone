#pragma once

#include "Biquad.h"

namespace rz
{
// ITU-R BS.1770-4 / EBU R128 loudness (momentary, short-term, integrated),
// plus sample peak and stereo correlation. process() is audio-thread only;
// the public atomics may be read from any thread.
class LoudnessMeter
{
public:
    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f };
    std::atomic<float> peakLeft { -100.0f }, peakRight { -100.0f }, correlation { 0.0f };

    void prepare (double sr)
    {
        sampleRate = sr;
        blockLen = std::max (1, (int) std::lround (sr * 0.1)); // 100 ms hops
        corrCoeff = std::exp (-1.0 / (0.3 * sr));
        design (sr);
        resetNow();
    }

    void requestReset() { resetRequested.store (true); }

    void process (const float* l, const float* r, int n)
    {
        if (resetRequested.exchange (false)) resetNow();

        float pl = peakL, pr = peakR;
        for (int i = 0; i < n; ++i)
        {
            const float xl = l[i], xr = r[i];
            pl = std::max (pl, std::abs (xl));
            pr = std::max (pr, std::abs (xr));

            const double yl = kWeight (0, (double) xl);
            const double yr = kWeight (1, (double) xr);
            energy += yl * yl + yr * yr;

            sLR = corrCoeff * sLR + (1.0 - corrCoeff) * (double) xl * (double) xr;
            sLL = corrCoeff * sLL + (1.0 - corrCoeff) * (double) xl * (double) xl;
            sRR = corrCoeff * sRR + (1.0 - corrCoeff) * (double) xr * (double) xr;

            if (++count >= blockLen)
            {
                finishBlock (energy / (double) blockLen);
                energy = 0.0;
                count = 0;
            }
        }
        peakL = pl;
        peakR = pr;
        peakLeft.store (juce::Decibels::gainToDecibels (pl, -100.0f));
        peakRight.store (juce::Decibels::gainToDecibels (pr, -100.0f));
        correlation.store ((float) (sLR / std::sqrt (sLL * sRR + 1.0e-12)));
    }

private:
    static constexpr int histBins = 800; // 0.1 LU bins from -70 to +10 LUFS

    static double toLoudness (double ms) { return -0.691 + 10.0 * std::log10 (std::max (ms, 1.0e-12)); }
    static double binEnergy (int bin)      { return std::pow (10.0, (((double) bin + 0.5) / 10.0 - 70.0 + 0.691) / 10.0); }

    void design (double fs)
    {
        // Stage 1: high shelf, stage 2: RLB high-pass (coefficients as in libebur128).
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                      2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            rlb = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
    }

    double kWeight (int ch, double x) noexcept
    {
        return stateRlb[ch].process (rlb, stateShelf[ch].process (shelf, x));
    }

    void resetNow()
    {
        for (auto& s : stateShelf) s.reset();
        for (auto& s : stateRlb) s.reset();
        energy = 0.0;
        count = 0;
        ring.fill (0.0);
        ringPos = 0;
        blocksSeen = 0;
        hist.fill (0u);
        peakL = peakR = 0.0f;
        sLR = sLL = sRR = 0.0;
        momentary.store (-100.0f);
        shortTerm.store (-100.0f);
        integrated.store (-100.0f);
        peakLeft.store (-100.0f);
        peakRight.store (-100.0f);
        correlation.store (0.0f);
    }

    double meanOfLast (int k) const
    {
        const int avail = std::min (k, blocksSeen);
        if (avail <= 0) return 0.0;
        double s = 0.0;
        for (int i = 0; i < avail; ++i)
            s += ring[(size_t) ((ringPos - 1 - i + 64 * ringSize) % ringSize)];
        return s / (double) avail;
    }

    void finishBlock (double blockMs)
    {
        ring[(size_t) ringPos] = blockMs;
        ringPos = (ringPos + 1) % ringSize;
        ++blocksSeen;

        const double m400 = meanOfLast (4);
        momentary.store ((float) toLoudness (m400));
        shortTerm.store ((float) toLoudness (meanOfLast (30)));

        if (blocksSeen >= 4)
        {
            const double l = toLoudness (m400);
            if (l > -70.0 && l < 10.0)
                ++hist[(size_t) juce::jlimit (0, histBins - 1, (int) std::floor ((l + 70.0) * 10.0))];
        }
        updateIntegrated();
    }

    void updateIntegrated()
    {
        double sum = 0.0;
        std::uint64_t cnt = 0;
        for (int b = 0; b < histBins; ++b)
            if (hist[(size_t) b] != 0u) { sum += (double) hist[(size_t) b] * binEnergy (b); cnt += hist[(size_t) b]; }

        if (cnt == 0) { integrated.store (-100.0f); return; }

        const double relThr = toLoudness (sum / (double) cnt) - 10.0;
        const int startBin = juce::jlimit (0, histBins - 1, (int) std::ceil ((relThr + 70.0) * 10.0));
        double gSum = 0.0;
        std::uint64_t gCnt = 0;
        for (int b = startBin; b < histBins; ++b)
            if (hist[(size_t) b] != 0u) { gSum += (double) hist[(size_t) b] * binEnergy (b); gCnt += hist[(size_t) b]; }

        integrated.store (gCnt == 0 ? -100.0f : (float) toLoudness (gSum / (double) gCnt));
    }

    static constexpr int ringSize = 30;

    double sampleRate = 44100.0, corrCoeff = 0.999;
    int blockLen = 4410, count = 0;
    double energy = 0.0;

    BiquadCoeffs shelf, rlb;
    std::array<BiquadState, 2> stateShelf, stateRlb;

    std::array<double, ringSize> ring {};
    int ringPos = 0, blocksSeen = 0;
    std::array<std::uint32_t, histBins> hist {};

    float peakL = 0.0f, peakR = 0.0f;
    double sLR = 0.0, sLL = 0.0, sRR = 0.0;
    std::atomic<bool> resetRequested { false };
};
} // namespace rz
