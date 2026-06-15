#pragma once

#include <juce_dsp/juce_dsp.h>
#include <indie_shared_dsp/indie_shared_dsp.h>
#include "DoubleVoice.h"

namespace indie
{

/**
    The doubling engine: owns the onset detector and a fixed pool of DoubleVoices,
    and mixes the dry signal with the generated doubles.

    Per sample it sums the input to mono, runs onset detection, re-rolls every active
    voice's per-note variance on each onset, then spreads the voices across the stereo
    field (by `width`) and blends them with the dry signal (by `mix`).

    Realtime-safe: the full voice pool is allocated up front in prepare(); process()
    allocates nothing.
*/
class DoublerEngine
{
public:
    static constexpr int kMaxVoices = 4;

    struct Parameters
    {
        int   numVoices   = 2;
        float timingDriftMs = 12.0f;
        float variance    = 0.5f;
        float detuneCents = 8.0f;
        float width       = 0.8f;
        float mix         = 0.5f;
        float warmth      = 0.35f;
        float decorrelate = 0.5f;
    };

    DoublerEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        onsetDetector.prepare (spec.sampleRate);

        for (int i = 0; i < kMaxVoices; ++i)
        {
            voices[(size_t) i].setVoiceIndex (i);
            voices[(size_t) i].prepare (spec);
        }

        smoothedMix.reset (sampleRate, 0.02);
        smoothedMix.setCurrentAndTargetValue (params.mix);
        reset();
    }

    void reset()
    {
        onsetDetector.reset();
        for (auto& v : voices)
            v.reset();
    }

    void setParameters (const Parameters& newParams)
    {
        params = newParams;
        params.numVoices = juce::jlimit (1, kMaxVoices, params.numVoices);

        for (int i = 0; i < params.numVoices; ++i)
            voices[(size_t) i].setParameters (params.timingDriftMs, params.variance, params.detuneCents,
                                              params.warmth, params.decorrelate);

        smoothedMix.setTargetValue (params.mix);
        updatePanGains();
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh      = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const int active     = params.numVoices;

        // The sqrt2 makeup compensates for the constant-power pan split in
        // updatePanGains(): without it, a fully wet channel sits at -3dB.
        const float wetNorm = juce::MathConstants<float>::sqrt2 / std::sqrt ((float) active);

        for (int n = 0; n < numSamples; ++n)
        {
            const float dry0 = numCh > 0 ? buffer.getSample (0, n) : 0.0f;
            const float dry1 = numCh > 1 ? buffer.getSample (1, n) : dry0;
            const float monoDry = 0.5f * (dry0 + dry1);

            if (onsetDetector.processSample (monoDry))
                for (int v = 0; v < active; ++v)
                    voices[(size_t) v].onNoteOnset();

            float wetL = 0.0f, wetR = 0.0f;
            for (int v = 0; v < active; ++v)
            {
                const float w = voices[(size_t) v].processSample (monoDry);
                wetL += w * panL[(size_t) v];
                wetR += w * panR[(size_t) v];
            }
            wetL *= wetNorm;
            wetR *= wetNorm;

            const float m = smoothedMix.getNextValue();

            // Equal-power crossfade: dry and wet are largely decorrelated, so a linear
            // (1-m)/m blend would dip in level through the middle of the mix range.
            const float dryGain = std::sqrt (1.0f - m);
            const float wetGain = std::sqrt (m);

            if (numCh >= 2)
            {
                buffer.setSample (0, n, dry0 * dryGain + wetL * wetGain);
                buffer.setSample (1, n, dry1 * dryGain + wetR * wetGain);
            }
            else if (numCh == 1)
            {
                // No stereo field in mono — use the summed (centre) wet signal.
                const float wetMono = 0.5f * (wetL + wetR);
                buffer.setSample (0, n, dry0 * dryGain + wetMono * wetGain);
            }
        }
    }

private:
    void updatePanGains()
    {
        const int   active = params.numVoices;
        const float halfPi = juce::MathConstants<float>::halfPi;

        for (int v = 0; v < active; ++v)
        {
            // Spread voices symmetrically across the stereo field, scaled by width.
            float panPos = (active == 1) ? 1.0f
                                         : (-1.0f + 2.0f * (float) v / (float) (active - 1));
            panPos *= params.width;

            const float angle = (panPos + 1.0f) * 0.5f * halfPi; // [-1,1] -> [0, pi/2]
            panL[(size_t) v] = std::cos (angle);
            panR[(size_t) v] = std::sin (angle);
        }
    }

    double sampleRate = 44100.0;

    OnsetDetector onsetDetector;
    std::array<DoubleVoice, (size_t) kMaxVoices> voices;
    std::array<float, (size_t) kMaxVoices> panL { };
    std::array<float, (size_t) kMaxVoices> panR { };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix;
    Parameters params;
};

} // namespace indie
