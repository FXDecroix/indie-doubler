#pragma once

#include <juce_dsp/juce_dsp.h>
#include <indie_shared_dsp/indie_shared_dsp.h>

namespace indie
{

/**
    A single generated "double" — one extra performance take of the dry signal.

    Signal chain (per sample):
        dry → timing delay → pitch shift → diffusion (decorrelate) → warmth (low-pass) → gain

    Per-note variance is what makes it sound human rather than processed:
      - on each note onset the voice re-rolls a new micro-timing offset, level, and a
        *held* pitch offset (a few cents sharp/flat for the whole note), plus a fresh
        vibrato rate/depth/phase,
      - a slow bounded random walk drifts the timing across notes (timing intent).

    The pitch is shifted by a real (granular) PitchShifter rather than delay modulation,
    so the doubles are genuinely decorrelated from the dry signal and don't comb-filter
    into the metallic character that delay-modulation "detune" produces at high mix.

    All randomness comes from a seeded juce::Random so output is reproducible. The voice
    is mono; the engine pans it. Realtime-safe: everything is sized in prepare().
*/
class DoubleVoice
{
public:
    DoubleVoice() = default;

    void setVoiceIndex (int index)
    {
        voiceIndex = index;
        seed = (juce::int64) (index + 1) * 2654435761LL;
        rng.setSeed (seed);
    }

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxDelaySamples = (int) (maxDelaySeconds * sampleRate) + 4;

        juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };
        delayLine.prepare (monoSpec);
        delayLine.setMaximumDelayInSamples (maxDelaySamples);

        pitchShifter.prepare (sampleRate);
        diffuser.prepare (sampleRate, seed);
        warmthFilter.prepare (sampleRate);
        vibratoLfo.prepare (sampleRate);
        timingDrift.setStepSize (driftStepSamples);

        const double rampSeconds = 0.030;
        smoothedDelay.reset (sampleRate, rampSeconds);
        smoothedGain.reset (sampleRate, rampSeconds);
        smoothedCents.reset (sampleRate, rampSeconds);

        reset();
    }

    void reset()
    {
        delayLine.reset();
        pitchShifter.reset();
        diffuser.reset();
        warmthFilter.reset();
        vibratoLfo.reset();
        timingDrift.reset();

        lastMicroOffset = 0.0f;
        lastGain        = defaultGain;
        vibratoDepthCents = 0.0f;

        const float base = nominalDelaySamples();
        smoothedDelay.setCurrentAndTargetValue (base);
        smoothedGain.setCurrentAndTargetValue (defaultGain);
        smoothedCents.setCurrentAndTargetValue (0.0f);
    }

    /** Per-block parameter update. timingDrift in ms; variance, detune (cents),
        warmth and decorrelate are 0..1 / cents raw params. */
    void setParameters (float timingDriftMs, float variance, float detuneCents,
                        float warmth, float decorrelate)
    {
        // Stagger voices so multiple doubles don't sit on top of each other.
        const float stagger = 1.0f + 0.25f * (float) voiceIndex;
        baseDelaySamples = (timingDriftMs / 1000.0f) * (float) sampleRate * stagger;
        currentVariance  = variance;
        currentDetune    = detuneCents;
        decorrelateAmount = decorrelate;

        timingDrift.setLimit (maxDriftSamples() * currentVariance);

        // Warmth maps to a one-pole low-pass cutoff on the wet signal.
        const double cutoffHz = 18000.0 * std::pow (0.14, (double) warmth); // ~18k → ~2.5k
        warmthFilter.setCutoff ((float) cutoffHz);

        smoothedDelay.setTargetValue (baseDelaySamples + lastMicroOffset);
    }

    /** Re-roll per-note targets. Called by the engine on each detected onset. */
    void onNoteOnset()
    {
        // Micro-timing: a new offset around the base, scaled by variance.
        lastMicroOffset = nextBipolar() * maxMicroTimingSamples() * currentVariance;
        smoothedDelay.setTargetValue (baseDelaySamples + lastMicroOffset);

        // Articulation: a small per-note level difference.
        const float gainDb = nextBipolar() * maxGainVarianceDb * currentVariance;
        lastGain = juce::Decibels::decibelsToGain (gainDb);
        smoothedGain.setTargetValue (lastGain);

        // Held pitch offset for this note (a real, sustained few-cents detune).
        const float heldCents = nextBipolar() * currentDetune;
        smoothedCents.setTargetValue (heldCents);

        // Fresh, independent vibrato for this note (decorrelates takes on sustains).
        const float vibHz = 4.0f + rng.nextFloat() * 2.5f;
        vibratoLfo.setFrequency (vibHz);
        vibratoDepthCents = (2.0f + rng.nextFloat() * 4.0f) * (0.5f + 0.5f * currentVariance);
        vibratoLfo.setPhase (rng.nextFloat() * juce::MathConstants<float>::twoPi);
    }

    float processSample (float dry)
    {
        // Slow random walk of the base timing (drifts across and within notes).
        const float driftSamples = timingDrift.processSample (nextBipolar());

        float delaySamples = smoothedDelay.getNextValue() + driftSamples;
        delaySamples = juce::jlimit (1.0f, (float) (maxDelaySamples - 1), delaySamples);

        delayLine.pushSample (0, dry);
        delayLine.setDelay (delaySamples);
        float s = delayLine.popSample (0);

        // Pitch: held per-note offset plus independent vibrato.
        const float cents = smoothedCents.getNextValue() + vibratoDepthCents * vibratoLfo.processSample();
        pitchShifter.setPitchRatio (std::exp2 (cents / 1200.0f));
        s = pitchShifter.processSample (s);

        // Decorrelate so the double doesn't comb with the dry signal, then warm it.
        s = diffuser.processSample (s, decorrelateAmount);
        s = warmthFilter.processSample (s);

        return s * smoothedGain.getNextValue();
    }

private:
    float nominalDelaySamples() const
    {
        return juce::jlimit (1.0f, (float) (maxDelaySamples - 1), baseDelaySamples);
    }

    float nextBipolar() { return rng.nextFloat() * 2.0f - 1.0f; }

    float maxMicroTimingSamples() const { return 0.006f * (float) sampleRate; } // up to ~6 ms
    float maxDriftSamples()       const { return 0.004f * (float) sampleRate; }  // up to ~4 ms

    // Tunables.
    static constexpr float maxDelaySeconds   = 0.20f;
    static constexpr float defaultGain       = 1.0f;
    static constexpr float maxGainVarianceDb = 2.5f;
    static constexpr float driftStepSamples  = 0.02f;

    int    voiceIndex = 0;
    juce::int64 seed = 1;
    double sampleRate = 44100.0;
    int    maxDelaySamples = 1;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine { 1 << 15 };
    PitchShifter  pitchShifter;
    Diffuser      diffuser;
    OnePoleFilter warmthFilter;
    SineLFO       vibratoLfo;
    RandomWalk    timingDrift;
    juce::Random  rng;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelay;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedCents;

    float baseDelaySamples  = 0.0f;
    float currentVariance   = 0.5f;
    float currentDetune     = 8.0f;
    float decorrelateAmount = 0.5f;

    float lastMicroOffset = 0.0f;
    float lastGain        = defaultGain;

    float vibratoDepthCents = 0.0f;
};

} // namespace indie
