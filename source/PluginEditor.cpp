#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addControl (voices,      "voices",      "Voices");
    addControl (timingDrift, "timingDrift", "Timing");
    addControl (variance,    "variance",    "Variance");
    addControl (detune,      "detune",      "Detune");
    addControl (width,       "width",       "Width");
    addControl (mix,         "mix",         "Mix");
    addControl (warmth,      "warmth",      "Warmth");
    addControl (decorrelate, "decorrelate", "Decorr");

    addAndMakeVisible (inspectButton);

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    setSize (640, 340);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::addControl (ParameterControl& control, const juce::String& paramID, const juce::String& displayName)
{
    control.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible (control.slider);

    control.label.setText (displayName, juce::dontSendNotification);
    control.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (control.label);

    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, paramID, control.slider);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    auto title = juce::String (PRODUCT_NAME_WITHOUT_VERSION) + " v" VERSION;
    g.drawText (title, getLocalBounds().removeFromTop (34), juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (38);                       // title
    auto buttonRow = area.removeFromBottom (44);

    // Two rows of four controls.
    ParameterControl* controls[] { &voices, &timingDrift, &variance, &detune,
                                   &width, &mix, &warmth, &decorrelate };
    constexpr int cols = 4;

    auto layoutRow = [] (juce::Rectangle<int> row, ParameterControl** first)
    {
        const int cellWidth = row.getWidth() / cols;
        for (int i = 0; i < cols; ++i)
        {
            auto cell = row.removeFromLeft (cellWidth).reduced (8);
            first[i]->label.setBounds (cell.removeFromTop (20));
            first[i]->slider.setBounds (cell);
        }
    };

    const int rowHeight = area.getHeight() / 2;
    layoutRow (area.removeFromTop (rowHeight), &controls[0]);
    layoutRow (area, &controls[4]);

    inspectButton.setBounds (buttonRow.withSizeKeepingCentre (120, 32));
}
