#include "PluginProcessor.h"
#include "PluginEditor.h"

RedzoneAudioProcessor::RedzoneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "REDZONE", rz::createParameterLayout())
{
    auto P = [this] (const juce::String& pid) { return apvts.getRawParameterValue (pid); };
    ptrs.inGain = P (rz::id::inGain);
    ptrs.eqBypass = P (rz::id::eqBypass);
    ptrs.mbBypass = P (rz::id::mbBypass);
    ptrs.mbX1 = P (rz::id::mbX1);
    ptrs.mbX2 = P (rz::id::mbX2);
    ptrs.imgBypass = P (rz::id::imgBypass);
    ptrs.imgX1 = P (rz::id::imgX1);
    ptrs.imgX2 = P (rz::id::imgX2);
    ptrs.imgMono = P (rz::id::imgMonoLow);
    ptrs.maxBypass = P (rz::id::maxBypass);
    ptrs.maxDrive = P (rz::id::maxDrive);
    ptrs.maxCeiling = P (rz::id::maxCeiling);
    ptrs.maxRel = P (rz::id::maxRelease);
    for (int b = 0; b < rz::numEqBands; ++b)
    {
        ptrs.eqFreq[(size_t) b] = P (rz::id::eqFreq (b + 1));
        ptrs.eqGain[(size_t) b] = P (rz::id::eqGain (b + 1));
        ptrs.eqQ[(size_t) b] = P (rz::id::eqQ (b + 1));
    }
    for (int b = 0; b < rz::numMbBands; ++b)
    {
        ptrs.mbThresh[(size_t) b] = P (rz::id::mbThresh (b + 1));
        ptrs.mbRatio[(size_t) b] = P (rz::id::mbRatio (b + 1));
        ptrs.mbAtt[(size_t) b] = P (rz::id::mbAttack (b + 1));
        ptrs.mbRel[(size_t) b] = P (rz::id::mbRelease (b + 1));
        ptrs.mbGain[(size_t) b] = P (rz::id::mbGain (b + 1));
    }
    for (int b = 0; b < 3; ++b)
        ptrs.imgW[(size_t) b] = P (rz::id::imgWidth (b + 1));
}

bool RedzoneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RedzoneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    eq.prepare (sampleRate);
    multiband.prepare (sampleRate, samplesPerBlock);
    imager.prepare (sampleRate, samplesPerBlock);
    maximizer.prepare (sampleRate, samplesPerBlock);
    loudness.prepare (sampleRate);

    eqFader.prepare (sampleRate, samplesPerBlock);
    mbFader.prepare (sampleRate, samplesPerBlock);
    imgFader.prepare (sampleRate, samplesPerBlock);

    inGainSm.reset (sampleRate, 0.02);
    inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (ptrs.inGain->load()));

    setLatencySamples (maximizer.getLatencySamples());
}

void RedzoneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    const int n = buffer.getNumSamples();

    // ---- Input gain --------------------------------------------------------
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (ptrs.inGain->load()));
    if (inGainSm.isSmoothing())
    {
        float* l = buffer.getWritePointer (0);
        float* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float g = inGainSm.getNextValue();
            l[i] *= g;
            r[i] *= g;
        }
    }
    else
    {
        buffer.applyGain (inGainSm.getTargetValue());
    }

    // ---- EQ ----------------------------------------------------------------
    for (int b = 0; b < rz::numEqBands; ++b)
        eq.setBand (b, { ptrs.eqFreq[(size_t) b]->load(), ptrs.eqGain[(size_t) b]->load(), ptrs.eqQ[(size_t) b]->load() });
    eqFader.run (buffer, ptrs.eqBypass->load() > 0.5f, [this] (juce::AudioBuffer<float>& b) { eq.process (b); });

    // ---- Multiband compressor -----------------------------------------------
    {
        rz::MultibandCompressor::Params p;
        p.xover1 = ptrs.mbX1->load();
        p.xover2 = ptrs.mbX2->load();
        for (int b = 0; b < rz::numMbBands; ++b)
        {
            auto& bp = p.bands[(size_t) b];
            bp.thresholdDb = ptrs.mbThresh[(size_t) b]->load();
            bp.ratio = ptrs.mbRatio[(size_t) b]->load();
            bp.attackMs = ptrs.mbAtt[(size_t) b]->load();
            bp.releaseMs = ptrs.mbRel[(size_t) b]->load();
            bp.gainDb = ptrs.mbGain[(size_t) b]->load();
        }
        multiband.setParams (p);
        mbFader.run (buffer, ptrs.mbBypass->load() > 0.5f, [this] (juce::AudioBuffer<float>& b) { multiband.process (b); });
    }

    // ---- Imager ------------------------------------------------------------
    {
        rz::Imager::Params p;
        p.xover1 = ptrs.imgX1->load();
        p.xover2 = ptrs.imgX2->load();
        for (int b = 0; b < 3; ++b)
            p.width[(size_t) b] = ptrs.imgW[(size_t) b]->load() * 0.01f;
        p.monoLow = ptrs.imgMono->load() > 0.5f;
        imager.setParams (p);
        imgFader.run (buffer, ptrs.imgBypass->load() > 0.5f, [this] (juce::AudioBuffer<float>& b) { imager.process (b); });
    }

    // ---- Maximizer (always runs: constant latency) ---------------------------
    maximizer.setParams (ptrs.maxDrive->load(), ptrs.maxCeiling->load(), ptrs.maxRel->load(),
                         ptrs.maxBypass->load() > 0.5f);
    maximizer.process (buffer);

    // ---- Metering ----------------------------------------------------------
    loudness.process (buffer.getReadPointer (0), buffer.getReadPointer (1), n);
    spectrum.push (buffer.getReadPointer (0), buffer.getReadPointer (1), n);
}

juce::AudioProcessorEditor* RedzoneAudioProcessor::createEditor()
{
    return new RedzoneAudioProcessorEditor (*this);
}

void RedzoneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RedzoneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RedzoneAudioProcessor();
}
