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

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;

    ParameterControl voices, timingDrift, variance, detune, width, mix, warmth, decorrelate;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
