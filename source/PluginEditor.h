#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // One labelled rotary slider bound to an APVTS parameter.
    struct ParameterControl
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addControl (ParameterControl& control, const juce::String& paramID, const juce::String& displayName);
    void setAdvancedVisible (bool shouldBeVisible);

    static constexpr int collapsedWidth  = 460;
    static constexpr int collapsedHeight = 360;
    static constexpr int expandedHeight  = 640;

    PluginProcessor& processorRef;

    // The big "Intensity" macro knob (not indie → very indie).
    juce::Slider intensityKnob;
    juce::Label  notIndieLabel { {}, "not indie" };
    juce::Label  veryIndieLabel { {}, "very indie" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intensityAttachment;
    bool draggingIntensity = false;

    // Collapsible advanced section with every individual parameter.
    juce::TextButton advancedButton;
    bool advancedVisible = false;
    ParameterControl voices, timingDrift, variance, detune, width, mix, warmth, decorrelate;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
