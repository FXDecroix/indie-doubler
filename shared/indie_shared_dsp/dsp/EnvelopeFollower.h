#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace indie
{

/**
    Peak envelope follower with independent attack and release time constants.

    Rectifies the input and tracks its magnitude with a one-pole that rises at the
    attack rate and falls at the release rate. An attack time of 0 gives an instant
    attack (a true peak follower); equal attack and release give a symmetric average
    follower.

    The building block behind onset detection, compression, gating, level riding and
    metering across the suite.

    Realtime-safe: a single scalar state, coefficients computed in the setters.
*/
class EnvelopeFollower
{
public:
    EnvelopeFollower() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset()
    {
        env = 0.0f;
    }

    /** Attack time in seconds. 0 (or negative) means an instant attack. */
    void setAttackTime (float seconds)
    {
        attackCoeff = coeffForTime (seconds);
    }

    void setReleaseTime (float seconds)
    {
        releaseCoeff = coeffForTime (seconds);
    }

    float processSample (float input)
    {
        const float mag = std::abs (input);
        const float coeff = (mag > env) ? attackCoeff : releaseCoeff;
        env += coeff * (mag - env);
        return env;
    }

    float getCurrentValue() const { return env; }

private:
    float coeffForTime (float seconds) const
    {
        if (seconds <= 0.0f)
            return 1.0f; // instant: env jumps straight to the input magnitude

        // Per-sample smoothing factor for a one-pole with the given time constant.
        return 1.0f - std::exp (-1.0f / (seconds * (float) sampleRate));
    }

    double sampleRate = 44100.0;
    float  attackCoeff  = 1.0f;
    float  releaseCoeff = 1.0f;
    float  env = 0.0f;
};

} // namespace indie
