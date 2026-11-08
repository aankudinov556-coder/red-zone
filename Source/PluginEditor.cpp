#include "PluginEditor.h"

using namespace rz::ui;

namespace
{
    constexpr int headerH = 46;
    constexpr int spectrumH = 230;
    constexpr int meterW = 230;

    ParamPanel* makeEqPanel (juce::AudioProcessorValueTreeState& s)
    {
        ControlRow freq, gain, q;
        for (int b = 1; b <= rz::numEqBands; ++b)
        {
            const auto n = juce::String (b);
            freq.push_back ({ rz::id::eqFreq (b), "Freq " + n });
            gain.push_back ({ rz::id::eqGain (b), "Gain " + n });
            q.push_back ({ rz::id::eqQ (b), "Q " + n });
        }
        return new ParamPanel (s, rz::id::eqBypass, { freq, gain, q }, { "FREQ", "GAIN", "Q" });
    }

    ParamPanel* makeMultibandPanel (juce::AudioProcessorValueTreeState& s)
    {
        std::vector<ControlRow> rows;
        rows.push_back ({ { rz::id::mbX1, "Low / Mid X-over" }, { rz::id::mbX2, "Mid / High X-over" } });
        const char* names[] = { "Low", "Mid", "High" };
        for (int b = 1; b <= rz::numMbBands; ++b)
        {
            const juce::String n = names[b - 1];
            rows.push_back ({ { rz::id::mbThresh (b),  n + " Threshold" },
                              { rz::id::mbRatio (b),   n + " Ratio" },
                              { rz::id::mbAttack (b),  n + " Attack" },
                              { rz::id::mbRelease (b), n + " Release" },
                              { rz::id::mbGain (b),    n + " Makeup" } });
        }
        return new ParamPanel (s, rz::id::mbBypass, rows, { "XOVER", "LOW", "MID", "HIGH" });
    }

    ParamPanel* makeImagerPanel (juce::AudioProcessorValueTreeState& s)
    {
        std::vector<ControlRow> rows;
        rows.push_back ({ { rz::id::imgX1, "Low / Mid X-over" }, { rz::id::imgX2, "Mid / High X-over" } });
        rows.push_back ({ { rz::id::imgWidth (1), "Low Width" },
                          { rz::id::imgWidth (2), "Mid Width" },
                          { rz::id::imgWidth (3), "High Width" } });
        rows.push_back ({ { rz::id::imgMonoLow, "MONO LOW BAND", true } });
        return new ParamPanel (s, rz::id::imgBypass, rows, { "XOVER", "WIDTH", "BASS" });
    }

    ParamPanel* makeMaximizerPanel (juce::AudioProcessorValueTreeState& s)
    {
        std::vector<ControlRow> rows;
        rows.push_back ({ { rz::id::maxDrive, "Drive" }, { rz::id::maxCeiling, "Ceiling" }, { rz::id::maxRelease, "Release" } });
        return new ParamPanel (s, rz::id::maxBypass, rows, { "LIMIT" });
    }
} // namespace

RedzoneAudioProcessorEditor::RedzoneAudioProcessorEditor (RedzoneAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), spectrum (p), meters (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (meters);

    const auto tabColour = colours::background;
    tabs.setTabBarDepth (30);
    tabs.addTab ("EQ", tabColour, makeEqPanel (p.apvts), true);
    tabs.addTab ("MULTIBAND", tabColour, makeMultibandPanel (p.apvts), true);
    tabs.addTab ("IMAGER", tabColour, makeImagerPanel (p.apvts), true);
    tabs.addTab ("MAXIMIZER", tabColour, makeMaximizerPanel (p.apvts), true);
    addAndMakeVisible (tabs);

    inGainLabel.setText ("INPUT", juce::dontSendNotification);
    inGainLabel.setJustificationType (juce::Justification::centredRight);
    inGainLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (inGainLabel);
    inGainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 20);
    inGainSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (inGainSlider);
    inGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, rz::id::inGain, inGainSlider);

    setSize (1000, 740);
}

RedzoneAudioProcessorEditor::~RedzoneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void RedzoneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto header = getLocalBounds().removeFromTop (headerH).reduced (14, 0);
    g.setColour (colours::accent);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("REDZONE", header.removeFromLeft (150), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (12.0f));
    g.drawText ("MASTERING SUITE", header.removeFromLeft (200), juce::Justification::centredLeft);
}

void RedzoneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (headerH).reduced (14, 6);
    inGainSlider.setBounds (header.removeFromRight (150));
    inGainLabel.setBounds (header.removeFromRight (60));

    area.reduce (8, 0);
    area.removeFromBottom (8);

    meters.setBounds (area.removeFromRight (meterW));
    area.removeFromRight (8);
    spectrum.setBounds (area.removeFromTop (spectrumH));
    area.removeFromTop (8);
    tabs.setBounds (area);
}
