#pragma once

#include <juce_dsp/juce_dsp.h>

namespace indie
{

/**
    A simple granular (two-tap crossfade) pitch shifter.

    Incoming audio is written to a circular buffer; two read pointers ramp through it
    at a rate set by the pitch ratio, half a grain apart, each windowed so that the
    discontinuity when a pointer wraps is masked by the other tap.

    This replaces the old delay-modulation "micro-pitch", whose symmetric sweep behaved
    like a flanger and was the main source of the metallic comb-filtering at high mix.
    Because the doubler only needs very small shifts (a few cents → ratio ≈ 1), the read
    pointers drift extremely slowly and grains rarely wrap, so the output is essentially
    artifact-free.

    Realtime-safe: the buffer is sized in prepare(); processSample() only does scalar math.
*/
class PitchShifter
{
public:
    PitchShifter() = default;

    void prepare (double sampleRate)
    {
        grainSamples = (float) (grainSeconds * sampleRate);
        bufferLength = (int) (grainSamples * 2.0f) + 4;
        buffer.assign ((size_t) bufferLength, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        phase = 0.0f;
    }

    /** ratio > 1 shifts up, < 1 shifts down. */
    void setPitchRatio (float ratio)
    {
        // Phase increment per sample so the read pointer advances at `ratio`.
        phaseInc = (1.0f - ratio) / grainSamples;
    }

    float processSample (float x)
    {
        buffer[(size_t) writePos] = x;

        phase += phaseInc;
        phase -= std::floor (phase); // wrap to [0, 1)

        float phase1 = phase + 0.5f;
        phase1 -= std::floor (phase1);

        const float read0 = readAt (phase * grainSamples);
        const float read1 = readAt (phase1 * grainSamples);

        // Equal-power-ish crossfade; each tap fades to zero at its own wrap point.
        const float g0 = sinSquared (phase);
        const float out = g0 * read0 + (1.0f - g0) * read1;

        if (++writePos >= bufferLength)
            writePos = 0;

        return out;
    }

private:
    static float sinSquared (float p)
    {
        const float s = std::sin (juce::MathConstants<float>::pi * p);
        return s * s;
    }

    // Reads `delaySamples` behind the write pointer, with linear interpolation.
    float readAt (float delaySamples) const
    {
        float pos = (float) writePos - delaySamples;
        while (pos < 0.0f)
            pos += (float) bufferLength;

        const int i0 = (int) pos;
        const float frac = pos - (float) i0;
        const int i1 = (i0 + 1) % bufferLength;

        return buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
    }

    // Grain length is a tradeoff: longer = smoother but more latency (a tap sits at
    // ~half a grain). The doubler only makes tiny (few-cent) shifts, so wraps are very
    // rare even with a short grain — keep it short to stay a tight double, not a slap.
    static constexpr double grainSeconds = 0.030;

    std::vector<float> buffer;
    int   bufferLength = 1;
    float grainSamples = 1.0f;

    int   writePos = 0;
    float phase    = 0.0f;
    float phaseInc = 0.0f;
};

} // namespace indie
