#pragma once

#include "Common.h"

namespace rz
{
constexpr int numEqBands = 6;
constexpr int numMbBands = 3;

// Parameter IDs. Banded IDs are 1-based (eq1_freq ... eq6_freq).
namespace id
{
    inline constexpr const char* inGain      = "in_gain";

    inline constexpr const char* eqBypass    = "eq_bypass";
    inline juce::String eqFreq (int b)       { return "eq" + juce::String (b) + "_freq"; }
    inline juce::String eqGain (int b)       { return "eq" + juce::String (b) + "_gain"; }
    inline juce::String eqQ    (int b)       { return "eq" + juce::String (b) + "_q"; }

    inline constexpr const char* mbBypass    = "mb_bypass";
    inline constexpr const char* mbX1        = "mb_x1";
    inline constexpr const char* mbX2        = "mb_x2";
    inline juce::String mbThresh (int b)     { return "mb" + juce::String (b) + "_thresh"; }
    inline juce::String mbRatio  (int b)     { return "mb" + juce::String (b) + "_ratio"; }
    inline juce::String mbAttack (int b)     { return "mb" + juce::String (b) + "_att"; }
    inline juce::String mbRelease(int b)     { return "mb" + juce::String (b) + "_rel"; }
    inline juce::String mbGain   (int b)     { return "mb" + juce::String (b) + "_gain"; }

    inline constexpr const char* imgBypass   = "img_bypass";
    inline constexpr const char* imgX1       = "img_x1";
    inline constexpr const char* imgX2       = "img_x2";
    inline constexpr const char* imgMonoLow  = "img_mono_low";
    inline juce::String imgWidth (int b)     { return "img_w" + juce::String (b); }

    inline constexpr const char* maxBypass   = "max_bypass";
    inline constexpr const char* maxDrive    = "max_drive";
    inline constexpr const char* maxCeiling  = "max_ceiling";
    inline constexpr const char* maxRelease  = "max_rel";
} // namespace id

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace rz
