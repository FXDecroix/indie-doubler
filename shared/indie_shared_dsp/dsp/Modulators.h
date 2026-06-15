#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace indie
{

/**
    Unit-amplitude sine LFO.

    Produces sin(phase) in [-1, 1] and advances. Depth/scaling and any per-note
    re-triggering are the consumer's job — this is just the oscillator.

    Realtime-safe: scalar phase state, increment computed in setFrequency().
*/
class SineLFO
{
public:
    SineLFO() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset()
    {
        phase = 0.0f;
        inc   = 0.0f;
    }

    void setFrequency (float hz)
    {
        inc = juce::MathConstants<float>::twoPi * hz / (float) sampleRate;
    }

    void setPhase (float radians)
    {
        phase = radians;
    }

    float processSample()
    {
        phase += inc;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
        return std::sin (phase);
    }

private:
    double sampleRate = 44100.0;
    float  inc   = 0.0f;
    float  phase = 0.0f;
};

/**
    Bounded random walk — a slow drift that wanders but stays within +/- limit.

    The caller supplies each step's random value in [-1, 1], which keeps the random
    source (and therefore reproducibility) under the consumer's control. Useful for
    analog-style drift / humanization.

    Realtime-safe: a single scalar accumulator.
*/
class RandomWalk
{
public:
    RandomWalk() = default;

    void reset (float initial = 0.0f) { value = initial; }
    void setStepSize (float newStep)  { step = newStep; }
    void setLimit (float newLimit)    { limit = newLimit; }

    float processSample (float bipolarNoise)
    {
        value += bipolarNoise * step;
        value = juce::jlimit (-limit, limit, value);
        return value;
    }

    float getValue() const { return value; }

private:
    float value = 0.0f;
    float step  = 0.0f;
    float limit = 0.0f;
};

} // namespace indie
