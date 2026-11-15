#pragma once

#include "EqGraph.h"

namespace rz::ui
{
// Controls for the band currently selected on the EQ graph.
class EqBandPanel : public juce::Component
{
public:
    std::function<void (int)> onDeleteBand;

    explicit EqBandPanel (juce::AudioProcessorValueTreeState& s) : state (s)
    {
        bypassButton.setButtonText ("BYPASS");
        addAndMakeVisible (bypassButton);
        bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id::eqBypass, bypassButton);

        hint.setText (juce::String::fromUTF8 ("Double-click the graph to add a band.  Drag a point to move it, "
                                              "mouse wheel = Q, Shift = fine, right-click = type / delete, "
                                              "double-click a point to delete it."),
                      juce::dontSendNotification);
        hint.setJustificationType (juce::Justification::centred);
        hint.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (hint);

        title.setFont (juce::Font (16.0f, juce::Font::bold));
        addChildComponent (title);

        typeBox.addItemList (eqTypeNames(), 1);
        slopeBox.addItemList (eqSlopeNames(), 1);
        typeBox.onChange = [this] { updateVisibility(); };
        addChildComponent (typeBox);
        addChildComponent (slopeBox);

        onButton.setButtonText ("ON");
        addChildComponent (onButton);

        deleteButton.setButtonText ("DELETE");
        deleteButton.onClick = [this] { if (onDeleteBand && band >= 0) onDeleteBand (band); };
        addChildComponent (deleteButton);

        for (auto* k : { &freq, &gain, &q })
        {
            k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
            addChildComponent (*k);
        }
        const char* names[] = { "Freq", "Gain", "Q" };
        for (int i = 0; i < 3; ++i)
        {
            labels[(size_t) i].setText (names[i], juce::dontSendNotification);
            labels[(size_t) i].setJustificationType (juce::Justification::centred);
            labels[(size_t) i].setColour (juce::Label::textColourId, colours::textDim);
            addChildComponent (labels[(size_t) i]);
        }
    }

    void setBand (int b)
    {
        band = b;
        freqAtt.reset(); gainAtt.reset(); qAtt.reset();
        typeAtt.reset(); slopeAtt.reset(); onAtt.reset();

        const bool has = b >= 0;
        if (has)
        {
            const int n = b + 1;
            using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
            using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
            using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
            freqAtt = std::make_unique<SA> (state, id::eqFreq (n), freq);
            gainAtt = std::make_unique<SA> (state, id::eqGain (n), gain);
            qAtt = std::make_unique<SA> (state, id::eqQ (n), q);
            typeAtt = std::make_unique<CA> (state, id::eqType (n), typeBox);
            slopeAtt = std::make_unique<CA> (state, id::eqSlope (n), slopeBox);
            onAtt = std::make_unique<BA> (state, id::eqOn (n), onButton);
            freq.setDoubleClickReturnValue (false, 0.0);
            gain.setDoubleClickReturnValue (true, 0.0);
            q.setDoubleClickReturnValue (true, 1.0);
            title.setText ("BAND " + juce::String (n), juce::dontSendNotification);
            title.setColour (juce::Label::textColourId, bandColour (b));
        }

        hint.setVisible (! has);
        for (juce::Component* c : { (juce::Component*) &title, (juce::Component*) &typeBox, (juce::Component*) &onButton,
                                    (juce::Component*) &deleteButton, (juce::Component*) &freq, (juce::Component*) &q,
                                    (juce::Component*) &labels[0], (juce::Component*) &labels[2] })
            c->setVisible (has);
        updateVisibility();
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        auto top = area.removeFromTop (28);
        bypassButton.setBounds (top.removeFromLeft (90));

        hint.setBounds (area.reduced (20, 0));

        auto row = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 760), juce::jmin (area.getHeight(), 130));
        auto left = row.removeFromLeft (200).reduced (6);
        title.setBounds (left.removeFromTop (26));
        typeBox.setBounds (left.removeFromTop (26));
        left.removeFromTop (6);
        slopeBox.setBounds (left.removeFromTop (26));

        auto right = row.removeFromRight (120).reduced (6);
        onButton.setBounds (right.removeFromTop (30));
        right.removeFromTop (8);
        deleteButton.setBounds (right.removeFromTop (30));

        const int kw = row.getWidth() / 3;
        juce::Slider* knobs[] = { &freq, &gain, &q };
        for (int i = 0; i < 3; ++i)
        {
            auto cell = row.removeFromLeft (kw);
            labels[(size_t) i].setBounds (cell.removeFromTop (16));
            knobs[i]->setBounds (cell);
        }
    }

private:
    void updateVisibility()
    {
        const bool has = band >= 0;
        const int t = typeBox.getSelectedItemIndex();
        const bool cut = t == eqLowCut || t == eqHighCut;
        const bool gainType = t == eqBell || t == eqLowShelf || t == eqHighShelf;
        slopeBox.setVisible (has && cut);
        gain.setVisible (has && gainType);
        labels[1].setVisible (has && gainType);
    }

    juce::AudioProcessorValueTreeState& state;
    int band = -1;

    juce::ToggleButton bypassButton, onButton;
    juce::Label hint, title;
    juce::ComboBox typeBox, slopeBox;
    juce::TextButton deleteButton;
    juce::Slider freq, gain, q;
    std::array<juce::Label, 3> labels;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, onAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, slopeAtt;
};
} // namespace rz::ui
