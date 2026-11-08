#pragma once

#include "Common.h"
#include "Parameters.h"
#include "dsp/Imager.h"
#include "dsp/LoudnessMeter.h"
#include "dsp/Maximizer.h"
#include "dsp/MultibandCompressor.h"
#include "dsp/ParametricEq.h"
#include "dsp/SpectrumBuffer.h"

class RedzoneAudioProcessor : public juce::AudioProcessor
{
public:
    RedzoneAudioProcessor();
    ~RedzoneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Redzone"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getParam (const juce::String& paramId) const { return apvts.getRawParameterValue (paramId)->load(); }

    juce::AudioProcessorValueTreeState apvts;

    // Read by the UI.
    rz::LoudnessMeter loudness;
    rz::SpectrumBuffer spectrum;
    rz::Maximizer maximizer;
    rz::MultibandCompressor multiband;

private:
    // Smoothly crossfades a module in/out so toggling bypass never clicks.
    struct BypassFader
    {
        juce::SmoothedValue<float> wet { 1.0f };
        juce::AudioBuffer<float> dry;

        void prepare (double sr, int maxBlock)
        {
            wet.reset (sr, 0.02);
            wet.setCurrentAndTargetValue (1.0f);
            dry.setSize (2, maxBlock);
        }

        template <typename Fn>
        void run (juce::AudioBuffer<float>& buf, bool bypassed, Fn&& fn)
        {
            wet.setTargetValue (bypassed ? 0.0f : 1.0f);
            const bool smoothing = wet.isSmoothing();

            if (! smoothing)
            {
                if (! bypassed) fn (buf);
                return;
            }

            const int n = buf.getNumSamples();
            if (n > dry.getNumSamples())
            {
                wet.skip (n);
                if (! bypassed) fn (buf);
                return;
            }

            for (int ch = 0; ch < 2; ++ch)
                dry.copyFrom (ch, 0, buf, ch, 0, n);

            fn (buf);

            for (int i = 0; i < n; ++i)
            {
                const float w = wet.getNextValue();
                for (int ch = 0; ch < 2; ++ch)
                {
                    float* d = buf.getWritePointer (ch);
                    d[i] = dry.getSample (ch, i) * (1.0f - w) + d[i] * w;
                }
            }
        }
    };

    // Raw parameter pointers cached once so processBlock never builds strings.
    struct Ptrs
    {
        using P = std::atomic<float>*;
        P inGain {}, eqBypass {}, mbBypass {}, mbX1 {}, mbX2 {}, imgBypass {}, imgX1 {}, imgX2 {}, imgMono {},
          maxBypass {}, maxDrive {}, maxCeiling {}, maxRel {};
        std::array<P, rz::numEqBands> eqFreq {}, eqGain {}, eqQ {};
        std::array<P, rz::numMbBands> mbThresh {}, mbRatio {}, mbAtt {}, mbRel {}, mbGain {};
        std::array<P, 3> imgW {};
    } ptrs;

    rz::ParametricEq eq;
    rz::Imager imager;
    BypassFader eqFader, mbFader, imgFader;
    juce::SmoothedValue<float> inGainSm { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedzoneAudioProcessor)
};
