#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace indie
{

/**
    Stateless helpers for placing signals in the stereo field and blending dry/wet.

    All constant-power: panning preserves energy as a source sweeps across the field,
    and the dry/wet crossfade preserves energy as the mix sweeps, which keeps the
    perceived level steady for decorrelated material.

    Pure math — no state, nothing to prepare. Realtime-safe.
*/
struct StereoField
{
    /** Constant-power pan. panPos in [-1, 1]: -1 hard left, 0 centre, +1 hard right. */
    static void panGains (float panPos, float& gainL, float& gainR)
    {
        const float angle = (juce::jlimit (-1.0f, 1.0f, panPos) + 1.0f) * 0.5f
                            * juce::MathConstants<float>::halfPi; // [-1,1] -> [0, pi/2]
        gainL = std::cos (angle);
        gainR = std::sin (angle);
    }

    /** Symmetric spread position in [-1, 1] for voice `voiceIndex` of `numVoices`,
        scaled by `width` in [0, 1]. A single voice maps to the +width position. */
    static float spreadPosition (int voiceIndex, int numVoices, float width)
    {
        const float pos = (numVoices <= 1)
                            ? 1.0f
                            : (-1.0f + 2.0f * (float) voiceIndex / (float) (numVoices - 1));
        return pos * width;
    }

    /** Equal-power dry/wet crossfade gains for mix in [0, 1] (0 = dry, 1 = wet). */
    static void crossfadeGains (float mix, float& dryGain, float& wetGain)
    {
        const float m = juce::jlimit (0.0f, 1.0f, mix);
        dryGain = std::sqrt (1.0f - m);
        wetGain = std::sqrt (m);
    }
};

} // namespace indie
