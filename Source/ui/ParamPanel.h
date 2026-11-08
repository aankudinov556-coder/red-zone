#pragma once

#include "RedzoneLookAndFeel.h"

namespace rz::ui
{
struct ControlSpec
{
    juce::String paramId;
    juce::String label;
    bool toggle = false;
};

using ControlRow = std::vector<ControlSpec>;

// A grid of knobs/toggles bound to parameters, with an optional bypass switch.
class ParamPanel : public juce::Component
{
public:
    ParamPanel (juce::AudioProcessorValueTreeState& state, const juce::String& bypassId,
                std::vector<ControlRow> rowsIn, juce::StringArray titles = {})
        : rows (std::move (rowsIn)), rowTitles (std::move (titles))
    {
        if (bypassId.isNotEmpty())
        {
            bypassButton.setButtonText ("BYPASS");
            addAndMakeVisible (bypassButton);
            bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, bypassId, bypassButton);
        }

        for (size_t r = 0; r < rows.size(); ++r)
        {
            for (const auto& spec : rows[r])
            {
                auto c = std::make_unique<Control>();
                c->row = (int) r;
                c->label.setText (spec.label, juce::dontSendNotification);
                c->label.setJustificationType (juce::Justification::centred);
                c->label.setColour (juce::Label::textColourId, colours::textDim);
                addAndMakeVisible (c->label);

                if (spec.toggle)
                {
                    c->button = std::make_unique<juce::ToggleButton> (spec.label);
                    c->button->setButtonText (spec.label);
                    addAndMakeVisible (*c->button);
                    c->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, spec.paramId, *c->button);
                    c->label.setVisible (false);
                }
                else
                {
                    c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                    c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
                    c->slider->setDoubleClickReturnValue (true, state.getParameter (spec.paramId)->convertFrom0to1 (
                                                                    state.getParameter (spec.paramId)->getDefaultValue()));
                    addAndMakeVisible (*c->slider);
                    c->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, spec.paramId, *c->slider);
                }
                controls.push_back (std::move (c));
            }
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

        g.setColour (colours::accent);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        for (size_t r = 0; r < rowTitles.size(); ++r)
            if (r < rowBounds.size())
                g.drawText (rowTitles[(int) r], rowBounds[r].withWidth (titleWidth).toFloat(), juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        bypassButton.setBounds (area.removeFromTop (28).removeFromLeft (90));

        const int numRows = (int) rows.size();
        if (numRows == 0) return;

        int maxCols = 1;
        for (auto& r : rows) maxCols = std::max (maxCols, (int) r.size());

        const int left = rowTitles.isEmpty() ? 0 : titleWidth;
        auto grid = area.withTrimmedLeft (left);
        const int rowH = grid.getHeight() / numRows;
        const int colW = grid.getWidth() / maxCols;

        rowBounds.clear();
        for (int r = 0; r < numRows; ++r)
            rowBounds.push_back ({ area.getX(), area.getY() + r * rowH, area.getWidth(), rowH });

        std::vector<int> colIndex ((size_t) numRows, 0);
        for (auto& c : controls)
        {
            const int rowSize = (int) rows[(size_t) c->row].size();
            const int offset = (maxCols - rowSize) * colW / 2;
            const int col = colIndex[(size_t) c->row]++;
            juce::Rectangle<int> cell (grid.getX() + offset + col * colW, grid.getY() + c->row * rowH, colW, rowH);

            if (c->button)
            {
                c->button->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 8, 130), 28));
            }
            else
            {
                c->label.setBounds (cell.removeFromTop (16));
                c->slider->setBounds (cell);
            }
        }
    }

private:
    struct Control
    {
        int row = 0;
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };

    static constexpr int titleWidth = 56;

    std::vector<ControlRow> rows;
    juce::StringArray rowTitles;
    std::vector<std::unique_ptr<Control>> controls;
    std::vector<juce::Rectangle<int>> rowBounds;

    juce::ToggleButton bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
};
} // namespace rz::ui
