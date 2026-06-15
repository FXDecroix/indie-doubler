#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace indie
{

/**
    Memoryless soft-clip saturation (tanh waveshaper) with drive and makeup.

    The nonlinearity is normalised for unity small-signal gain: at low drive it is
    nearly transparent, and as drive increases it progressively rounds peaks and adds
    (odd) harmonics. `makeup` is a post gain to bring saturated peaks back up.

    Memoryless and sample-rate independent, so there's nothing to prepare. For alias
    suppression at high drive, run it inside juce::dsp::Oversampling in the consumer.

    Realtime-safe: only scalar coefficients, recomputed in the setters.
*/
class SaturationStage
{
public:
    SaturationStage() = default;

    /** Pre-gain into the nonlinearity. drive >= 1; 1 is nearly clean. */
    void setDrive (float newDrive)
    {
        drive = juce::jmax (1.0f, newDrive);
        updateOutputGain();
    }

    /** Linear post gain applied after the waveshaper. */
    void setMakeup (float linearGain)
    {
        makeup = linearGain;
        updateOutputGain();
    }

    float processSample (float x) const
    {
        // Unity small-signal gain (tanh'(0) = 1, so divide the pre-gain back out);
        // peaks compress as drive rises.
        return outputGain * std::tanh (drive * x);
    }

private:
    void updateOutputGain()
    {
        outputGain = makeup / drive;
    }

    float drive      = 1.0f;
    float makeup     = 1.0f;
    float outputGain = 1.0f;
};

} // namespace indie
