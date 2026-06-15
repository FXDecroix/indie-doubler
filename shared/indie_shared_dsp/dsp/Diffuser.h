#pragma once

#include <juce_dsp/juce_dsp.h>

namespace indie
{

/**
    A short cascade of Schroeder all-pass filters used to decorrelate a voice from the
    dry signal (and from the other voices).

    All-pass filters leave the magnitude response essentially flat but scramble phase,
    so when a double is summed with the dry signal the comb notches are smeared and
    diffused instead of stacking into fixed metallic resonances — this is the "glue"
    that makes the doubles sit like separately recorded takes.

    Each voice seeds its own randomized delay lengths and coefficients so the voices
    decorrelate from one another too. Realtime-safe: all buffers sized in prepare().
*/
class Diffuser
{
public:
    static constexpr int kStages = 4;

    Diffuser() = default;

    /** seed makes each voice's diffusion pattern unique but reproducible. */
    void prepare (double sampleRate, juce::int64 seed)
    {
        juce::Random rng (seed);

        for (auto& stage : stages)
        {
            // Short, prime-ish delays in the few-millisecond range.
            const double ms = 1.0 + rng.nextDouble() * 12.0;
            stage.length = juce::jmax (1, (int) (ms * 0.001 * sampleRate));
            stage.coeff  = 0.5f + rng.nextFloat() * 0.2f; // 0.5 .. 0.7
            stage.buffer.assign ((size_t) stage.length, 0.0f);
            stage.pos = 0;
        }

        reset();
    }

    void reset()
    {
        for (auto& stage : stages)
        {
            std::fill (stage.buffer.begin(), stage.buffer.end(), 0.0f);
            stage.pos = 0;
        }
    }

    /** amount 0 = bypass (dry), 1 = fully diffused. */
    float processSample (float x, float amount)
    {
        float y = x;
        for (auto& stage : stages)
            y = stage.process (y);

        return x + amount * (y - x);
    }

private:
    struct Stage
    {
        std::vector<float> buffer;
        int   length = 1;
        int   pos    = 0;
        float coeff  = 0.5f;

        float process (float x)
        {
            const float delayed = buffer[(size_t) pos];
            const float v = x + coeff * delayed; // all-pass feedback node
            buffer[(size_t) pos] = v;
            if (++pos >= length)
                pos = 0;

            return delayed - coeff * v;
        }
    };

    std::array<Stage, (size_t) kStages> stages;
};

} // namespace indie
