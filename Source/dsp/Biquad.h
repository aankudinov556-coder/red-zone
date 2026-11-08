#pragma once

#include "Common.h"

namespace rz
{
// RBJ cookbook biquad. Real-time safe (no allocation), double precision state.
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    static BiquadCoeffs peak (double sr, double f, double gainDb, double q)
    {
        const auto p = prepare (sr, f, gainDb, q);
        const double alpha = p.sinW / (2.0 * p.q);
        return normalise (1.0 + alpha * p.A, -2.0 * p.cosW, 1.0 - alpha * p.A,
                          1.0 + alpha / p.A, -2.0 * p.cosW, 1.0 - alpha / p.A);
    }

    static BiquadCoeffs lowShelf (double sr, double f, double gainDb, double q)
    {
        const auto p = prepare (sr, f, gainDb, q);
        const double alpha = p.sinW / (2.0 * p.q);
        const double t = 2.0 * std::sqrt (p.A) * alpha;
        const double A = p.A, c = p.cosW;
        return normalise (A * ((A + 1) - (A - 1) * c + t),
                          2.0 * A * ((A - 1) - (A + 1) * c),
                          A * ((A + 1) - (A - 1) * c - t),
                          (A + 1) + (A - 1) * c + t,
                          -2.0 * ((A - 1) + (A + 1) * c),
                          (A + 1) + (A - 1) * c - t);
    }

    static BiquadCoeffs highShelf (double sr, double f, double gainDb, double q)
    {
        const auto p = prepare (sr, f, gainDb, q);
        const double alpha = p.sinW / (2.0 * p.q);
        const double t = 2.0 * std::sqrt (p.A) * alpha;
        const double A = p.A, c = p.cosW;
        return normalise (A * ((A + 1) + (A - 1) * c + t),
                          -2.0 * A * ((A - 1) + (A + 1) * c),
                          A * ((A + 1) + (A - 1) * c - t),
                          (A + 1) - (A - 1) * c + t,
                          2.0 * ((A - 1) - (A + 1) * c),
                          (A + 1) - (A - 1) * c - t);
    }

    // Magnitude response in dB at frequency f.
    double magnitudeDb (double f, double sr) const
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const std::complex<double> z1 = std::polar (1.0, -w);
        const std::complex<double> z2 = z1 * z1;
        const auto num = b0 + b1 * z1 + b2 * z2;
        const auto den = 1.0 + a1 * z1 + a2 * z2;
        return 20.0 * std::log10 (std::max (1.0e-9, std::abs (num / den)));
    }

private:
    struct Pre { double A, cosW, sinW, q; };

    static Pre prepare (double sr, double f, double gainDb, double q)
    {
        f = juce::jlimit (10.0, sr * 0.45, f);
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
        return { std::pow (10.0, gainDb / 40.0), std::cos (w), std::sin (w), std::max (0.05, q) };
    }

    static BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }
};

// Transposed direct form II state.
struct BiquadState
{
    double z1 = 0.0, z2 = 0.0;

    double process (const BiquadCoeffs& c, double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0; }
};
} // namespace rz
