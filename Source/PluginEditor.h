#pragma once

#include "PluginProcessor.h"
#include "ui/MeterPanel.h"
#include "ui/ParamPanel.h"
#include "ui/RedzoneLookAndFeel.h"
#include "ui/EqBandPanel.h"
#include "ui/EqGraph.h"
#include "ui/ImagerTab.h"

class RedzoneAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RedzoneAudioProcessorEditor (RedzoneAudioProcessor&);
    ~RedzoneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RedzoneAudioProcessor& processor;
    rz::ui::RedzoneLookAndFeel lookAndFeel;

    rz::ui::EqGraph eqGraph;
    rz::ui::EqBandPanel* eqBandPanel = nullptr; // owned by tabs
    rz::ui::MeterPanel meters;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    juce::Label inGainLabel;
    juce::Slider inGainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxRight };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedzoneAudioProcessorEditor)
};
