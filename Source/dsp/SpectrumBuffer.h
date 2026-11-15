#pragma once

#include "Common.h"

namespace rz
{
// Lock-free ring of the most recent mono samples, written by the audio thread and
// read by the UI for the spectrum analyser. Torn reads only cost a visual glitch.
class SpectrumBuffer
{
public:
    static constexpr std::uint32_t size = 16384; // power of two
    static constexpr std::uint32_t mask = size - 1;

    SpectrumBuffer()
    {
        for (auto& s : ring) s.store (0.0f, std::memory_order_relaxed);
    }

    void push (const float* l, const float* r, int n) noexcept
    {
        auto w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
            ring[(w++) & mask].store (0.5f * (l[i] + r[i]), std::memory_order_relaxed);
        writePos.store (w, std::memory_order_release);
    }

    void copyLatest (float* dest, int count) const noexcept
    {
        const auto w = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i)
            dest[i] = ring[(w - (std::uint32_t) count + (std::uint32_t) i) & mask].load (std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<float>, size> ring;
    std::atomic<std::uint32_t> writePos { 0 };
};

// Stereo ring of the most recent output samples for the vectorscope.
class ScopeBuffer
{
public:
    static constexpr std::uint32_t size = 4096; // power of two
    static constexpr std::uint32_t mask = size - 1;

    ScopeBuffer()
    {
        for (auto& s : left) s.store (0.0f, std::memory_order_relaxed);
        for (auto& s : right) s.store (0.0f, std::memory_order_relaxed);
    }

    void push (const float* l, const float* r, int n) noexcept
    {
        auto w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i, ++w)
        {
            left[w & mask].store (l[i], std::memory_order_relaxed);
            right[w & mask].store (r[i], std::memory_order_relaxed);
        }
        writePos.store (w, std::memory_order_release);
    }

    void copyLatest (float* l, float* r, int count) const noexcept
    {
        const auto w = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i)
        {
            const auto idx = (w - (std::uint32_t) count + (std::uint32_t) i) & mask;
            l[i] = left[idx].load (std::memory_order_relaxed);
            r[i] = right[idx].load (std::memory_order_relaxed);
        }
    }

private:
    std::array<std::atomic<float>, size> left, right;
    std::atomic<std::uint32_t> writePos { 0 };
};
} // namespace rz
