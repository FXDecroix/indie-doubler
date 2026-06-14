#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Big "Intensity" macro knob.
    intensityKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    intensityKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (intensityKnob);
    intensityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "intensity", intensityKnob);

    // Only re-map the individual parameters on an actual user drag — not when the knob
    // value is set programmatically (state restore / automation), which would otherwise
    // clobber any manual tweaks the user made in the advanced section.
    intensityKnob.onDragStart = [this] { draggingIntensity = true; };
    intensityKnob.onDragEnd   = [this] { draggingIntensity = false; };
    intensityKnob.onValueChange = [this]
    {
        if (draggingIntensity)
            processorRef.applyIntensity ((float) intensityKnob.getValue());
    };

    auto setupScaleLabel = [this] (juce::Label& l, juce::Justification j)
    {
        l.setJustificationType (j);
        l.setFont (juce::Font (12.0f));
        l.setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (l);
    };
    setupScaleLabel (notIndieLabel, juce::Justification::centredLeft);
    setupScaleLabel (veryIndieLabel, juce::Justification::centredRight);

    // Advanced section toggle.
    advancedButton.onClick = [this] { setAdvancedVisible (! advancedVisible); };
    addAndMakeVisible (advancedButton);

    addControl (voices,      "voices",      "Voices");
    addControl (timingDrift, "timingDrift", "Timing");
    addControl (variance,    "variance",    "Variance");
    addControl (detune,      "detune",      "Detune");
    addControl (width,       "width",       "Width");
    addControl (mix,         "mix",         "Mix");
    addControl (warmth,      "warmth",      "Warmth");
    addControl (decorrelate, "decorrelate", "Decorr");

    addChildComponent (inspectButton);
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    setAdvancedVisible (false); // also sets the size
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::addControl (ParameterControl& control, const juce::String& paramID, const juce::String& displayName)
{
    control.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    addChildComponent (control.slider);

    control.label.setText (displayName, juce::dontSendNotification);
    control.label.setJustificationType (juce::Justification::centred);
    addChildComponent (control.label);

    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, paramID, control.slider);
}

void PluginEditor::setAdvancedVisible (bool shouldBeVisible)
{
    advancedVisible = shouldBeVisible;
    advancedButton.setButtonText (advancedVisible ? "Advanced  v" : "Advanced  >");

    for (auto* c : { &voices, &timingDrift, &variance, &detune, &width, &mix, &warmth, &decorrelate })
    {
        c->slider.setVisible (advancedVisible);
        c->label.setVisible (advancedVisible);
    }
    inspectButton.setVisible (advancedVisible);

    setSize (collapsedWidth, advancedVisible ? expandedHeight : collapsedHeight);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds().reduced (12);

    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    auto title = juce::String (PRODUCT_NAME_WITHOUT_VERSION) + " v" VERSION;
    g.drawText (title, area.removeFromTop (28), juce::Justification::centred, false);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawText ("INTENSITY", area.removeFromTop (20), juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (28); // title (painted)
    area.removeFromTop (20); // INTENSITY caption (painted)

    auto knobArea = area.removeFromTop (180);
    intensityKnob.setBounds (knobArea.withSizeKeepingCentre (180, 180));

    auto scaleRow = area.removeFromTop (20);
    notIndieLabel.setBounds (scaleRow.removeFromLeft (scaleRow.getWidth() / 2));
    veryIndieLabel.setBounds (scaleRow);

    area.removeFromTop (8);
    advancedButton.setBounds (area.removeFromTop (32).withSizeKeepingCentre (170, 28));

    if (! advancedVisible)
        return;

    area.removeFromTop (10);
    inspectButton.setBounds (area.removeFromBottom (40).withSizeKeepingCentre (120, 30));

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
}
