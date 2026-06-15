#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "EnvelopeFollower.h"

namespace indie
{

/**
    Lightweight transient / note-onset detector.

    Composes a fast (instant-attack) and a slow envelope follower. When the fast
    envelope rises sharply above the slow envelope (by a ratio threshold) and a minimum
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
        // Fast follower: instant attack, quick release, so it spikes on a transient.
        fastFollower.prepare (sampleRate);
        fastFollower.setAttackTime (0.0f);
        fastFollower.setReleaseTime (fastReleaseSeconds);

        // Slow follower: symmetric, tracks the longer-term running level.
        slowFollower.prepare (sampleRate);
        slowFollower.setAttackTime (slowReleaseSeconds);
        slowFollower.setReleaseTime (slowReleaseSeconds);

        refractorySamples = juce::jmax (1, (int) (refractorySeconds * sampleRate));
        reset();
    }

    void reset()
    {
        fastFollower.reset();
        slowFollower.reset();
        samplesSinceOnset = refractorySamples; // ready to fire immediately
        armed = true;
    }

    /** Feeds one (already summed-to-mono) input sample. Returns true on an onset. */
    bool processSample (float input)
    {
        const float fastEnv = fastFollower.processSample (input);
        const float slowEnv = slowFollower.processSample (input);

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
    // Tunables. Hysteresis (high to fire, low to re-arm) prevents a sustained note —
    // whose slow envelope lags the attack — from re-triggering during catch-up.
    static constexpr float fastReleaseSeconds = 0.005f;
    static constexpr float slowReleaseSeconds = 0.060f;
    static constexpr float refractorySeconds  = 0.025f; // debounce between onsets
    static constexpr float thresholdHigh      = 2.0f;   // fire when fast exceeds slow by this
    static constexpr float thresholdLow       = 1.1f;   // re-arm once the ratio falls back here

    EnvelopeFollower fastFollower;
    EnvelopeFollower slowFollower;
    int   refractorySamples = 1;

    int   samplesSinceOnset = 0;
    bool  armed = true;
};

} // namespace indie
