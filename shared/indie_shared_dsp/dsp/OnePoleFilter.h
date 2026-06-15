#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace indie
{

/**
    First-order (one-pole) low-pass filter.

    The classic leaky integrator: y += coeff * (x - y). The coefficient is derived
    from a cutoff frequency, so callers think in Hz rather than raw smoothing factors.
    Higher-level tone mappings (e.g. a 0..1 "warmth" knob → cutoff curve) belong in the
    consumer, not here — this is just the primitive.

    Realtime-safe: a single scalar state, coefficient computed in setCutoff().
*/
class OnePoleFilter
{
public:
    OnePoleFilter() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset()
    {
        state = 0.0f;
    }

    /** Sets the -3dB cutoff frequency in Hz. */
    void setCutoff (float cutoffHz)
    {
        const double fc = (double) juce::jlimit (1.0f, (float) (sampleRate * 0.5), cutoffHz);
        coeff = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * fc / sampleRate));
    }

    float processSample (float x)
    {
        state += coeff * (x - state);
        return state;
    }

private:
    double sampleRate = 44100.0;
    float  coeff = 1.0f;
    float  state = 0.0f;
};

} // namespace indie
