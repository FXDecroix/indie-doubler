#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace indie
{

/**
    Lightweight transient / note-onset detector.

    Tracks a fast and a slow peak-envelope follower. When the fast envelope
    rises sharply above the slow envelope (by a ratio threshold) and a minimum
    re-trigger interval (refractory time) has elapsed, an onset is reported.

    This drives the doubler's per-note variance: each detected onset re-rolls the
    timing / level / pitch targets of every voice, so the doubles diverge from the
    dry signal note-by-note (phrasing) rather than via a fixed periodic LFO.

    Realtime-safe: only scalar float state, all coefficients computed in prepare().
*/
class OnsetDetector
{
public:
    OnsetDetector() = default;

    void prepare (double sampleRate)
    {
        fastCoeff = coeffForTime (fastReleaseSeconds, sampleRate);
        slowCoeff = coeffForTime (slowReleaseSeconds, sampleRate);
        refractorySamples = juce::jmax (1, (int) (refractorySeconds * sampleRate));
        reset();
    }

    void reset()
    {
        fastEnv = 0.0f;
        slowEnv = 0.0f;
        samplesSinceOnset = refractorySamples; // ready to fire immediately
        armed = true;
    }

    /** Feeds one (already summed-to-mono) input sample. Returns true on an onset. */
    bool processSample (float input)
    {
        const float mag = std::abs (input);

        // Fast follower attacks instantly, releases quickly.
        fastEnv = (mag > fastEnv) ? mag : fastEnv + fastCoeff * (mag - fastEnv);
        // Slow follower tracks the longer-term level (and catches up after an attack).
        slowEnv = slowEnv + slowCoeff * (mag - slowEnv);

        if (samplesSinceOnset < refractorySamples)
            ++samplesSinceOnset;

        const float floorLevel = 1.0e-4f; // ignore near-silence
        if (fastEnv <= floorLevel)
        {
            armed = true; // silence always re-arms
            return false;
        }

        const float ratio = fastEnv / juce::jmax (slowEnv, floorLevel);

        bool onset = false;
        if (armed && ratio > thresholdHigh && samplesSinceOnset >= refractorySamples)
        {
            // A transient: fast envelope jumped well above the running level.
            onset = true;
            armed = false;
            samplesSinceOnset = 0;
        }
        else if (! armed && ratio < thresholdLow)
        {
            // Level has settled (or dipped) — re-arm for the next note.
            armed = true;
        }

        return onset;
    }

private:
    static float coeffForTime (float seconds, double sampleRate)
    {
        // Per-sample smoothing factor for a one-pole follower with the given time constant.
        return 1.0f - std::exp (-1.0f / (juce::jmax (1.0e-5f, seconds) * (float) sampleRate));
    }

    // Tunables. Hysteresis (high to fire, low to re-arm) prevents a sustained note —
    // whose slow envelope lags the attack — from re-triggering during catch-up.
    static constexpr float fastReleaseSeconds = 0.005f;
    static constexpr float slowReleaseSeconds = 0.060f;
    static constexpr float refractorySeconds  = 0.025f; // debounce between onsets
    static constexpr float thresholdHigh      = 2.0f;   // fire when fast exceeds slow by this
    static constexpr float thresholdLow       = 1.1f;   // re-arm once the ratio falls back here

    float fastCoeff = 0.0f;
    float slowCoeff = 0.0f;
    int   refractorySamples = 1;

    float fastEnv = 0.0f;
    float slowEnv = 0.0f;
    int   samplesSinceOnset = 0;
    bool  armed = true;
};

} // namespace indie
