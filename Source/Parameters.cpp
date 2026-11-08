#include "Parameters.h"

namespace rz
{
namespace
{
    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float s, float e, float v) { return s * std::pow (e / s, v); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
    }

    juce::NormalisableRange<float> linRange (float lo, float hi, float step = 0.0f)
    {
        return { lo, hi, step };
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    auto addFloat = [&params] (const String& pid, const String& name, NormalisableRange<float> range,
                               float def, const String& unit, int decimals)
    {
        auto attrs = AudioParameterFloatAttributes()
                         .withLabel (unit)
                         .withStringFromValueFunction ([unit, decimals] (float v, int)
                                                       { return String (v, decimals) + (unit.isEmpty() ? String() : " " + unit); });
        params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, range, def, attrs));
    };

    auto addFreq = [&params] (const String& pid, const String& name, float lo, float hi, float def)
    {
        auto attrs = AudioParameterFloatAttributes()
                         .withLabel ("Hz")
                         .withStringFromValueFunction ([] (float v, int)
                                                       { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (v, 0) + " Hz"; })
                         .withValueFromStringFunction ([] (const String& t)
                                                       {
                                                           auto v = t.getFloatValue();
                                                           return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
                                                       });
        params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, logRange (lo, hi), def, attrs));
    };

    auto addBool = [&params] (const String& pid, const String& name, bool def)
    {
        params.push_back (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def));
    };

    // ---- Global -----------------------------------------------------------
    addFloat (id::inGain, "Input Gain", linRange (-12.0f, 12.0f, 0.1f), 0.0f, "dB", 1);

    // ---- EQ ---------------------------------------------------------------
    addBool (id::eqBypass, "EQ Bypass", false);
    const float eqDefFreq[numEqBands] = { 40.0f, 150.0f, 500.0f, 1500.0f, 5000.0f, 12000.0f };
    const float eqDefQ[numEqBands]    = { 0.71f, 1.0f, 1.0f, 1.0f, 1.0f, 0.71f };
    for (int b = 1; b <= numEqBands; ++b)
    {
        const auto n = String (b);
        addFreq (id::eqFreq (b), "EQ " + n + " Freq", 20.0f, 20000.0f, eqDefFreq[b - 1]);
        addFloat (id::eqGain (b), "EQ " + n + " Gain", linRange (-18.0f, 18.0f, 0.1f), 0.0f, "dB", 1);
        addFloat (id::eqQ (b), "EQ " + n + " Q", logRange (0.3f, 8.0f), eqDefQ[b - 1], "", 2);
    }

    // ---- Multiband compressor --------------------------------------------
    addBool (id::mbBypass, "Multiband Bypass", false);
    addFreq (id::mbX1, "MB Crossover 1", 50.0f, 1000.0f, 200.0f);
    addFreq (id::mbX2, "MB Crossover 2", 1000.0f, 12000.0f, 3000.0f);
    const float mbDefAtt[numMbBands] = { 30.0f, 15.0f, 5.0f };
    const float mbDefRel[numMbBands] = { 250.0f, 180.0f, 120.0f };
    for (int b = 1; b <= numMbBands; ++b)
    {
        const auto n = String (b);
        addFloat (id::mbThresh (b),  "MB " + n + " Threshold", linRange (-60.0f, 0.0f, 0.1f), -20.0f, "dB", 1);
        addFloat (id::mbRatio (b),   "MB " + n + " Ratio",     logRange (1.0f, 20.0f), 1.0f, ": 1", 2);
        addFloat (id::mbAttack (b),  "MB " + n + " Attack",    logRange (1.0f, 100.0f), mbDefAtt[b - 1], "ms", 1);
        addFloat (id::mbRelease (b), "MB " + n + " Release",   logRange (20.0f, 1000.0f), mbDefRel[b - 1], "ms", 0);
        addFloat (id::mbGain (b),    "MB " + n + " Makeup",    linRange (-12.0f, 12.0f, 0.1f), 0.0f, "dB", 1);
    }

    // ---- Imager -----------------------------------------------------------
    addBool (id::imgBypass, "Imager Bypass", false);
    addFreq (id::imgX1, "Imager Crossover 1", 50.0f, 1000.0f, 250.0f);
    addFreq (id::imgX2, "Imager Crossover 2", 1000.0f, 12000.0f, 4000.0f);
    addBool (id::imgMonoLow, "Mono Low Band", false);
    for (int b = 1; b <= 3; ++b)
        addFloat (id::imgWidth (b), "Width " + String (b), linRange (0.0f, 200.0f, 1.0f), 100.0f, "%", 0);

    // ---- Maximizer --------------------------------------------------------
    addBool (id::maxBypass, "Maximizer Bypass", false);
    addFloat (id::maxDrive,   "Drive",   linRange (0.0f, 20.0f, 0.1f), 0.0f, "dB", 1);
    addFloat (id::maxCeiling, "Ceiling", linRange (-12.0f, 0.0f, 0.1f), -0.3f, "dB", 1);
    addFloat (id::maxRelease, "Release", logRange (10.0f, 500.0f), 100.0f, "ms", 0);

    return { params.begin(), params.end() };
}
} // namespace rz
