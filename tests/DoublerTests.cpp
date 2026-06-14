#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include "dsp/DoublerEngine.h"
#include "dsp/OnsetDetector.h"
#include "dsp/PitchShifter.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstring>

namespace
{
juce::dsp::ProcessSpec makeSpec (double sampleRate, int blockSize, int channels)
{
    return { sampleRate, (juce::uint32) blockSize, (juce::uint32) channels };
}

// Fills a stereo buffer with a reproducible pseudo-random signal.
void fillNoise (juce::AudioBuffer<float>& buffer, juce::Random& rng, float amplitude = 0.5f)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int n = 0; n < buffer.getNumSamples(); ++n)
            buffer.setSample (ch, n, (rng.nextFloat() * 2.0f - 1.0f) * amplitude);
}
} // namespace

TEST_CASE ("Parameters exist with expected defaults", "[doubler][params]")
{
    PluginProcessor proc;
    auto& apvts = proc.apvts;

    CHECK (apvts.getRawParameterValue ("intensity")   != nullptr);
    CHECK (apvts.getRawParameterValue ("voices")      != nullptr);
    CHECK (apvts.getRawParameterValue ("timingDrift") != nullptr);
    CHECK (apvts.getRawParameterValue ("variance")    != nullptr);
    CHECK (apvts.getRawParameterValue ("detune")      != nullptr);
    CHECK (apvts.getRawParameterValue ("width")       != nullptr);
    CHECK (apvts.getRawParameterValue ("mix")         != nullptr);
    CHECK (apvts.getRawParameterValue ("warmth")      != nullptr);
    CHECK (apvts.getRawParameterValue ("decorrelate") != nullptr);

    CHECK (apvts.getRawParameterValue ("voices")->load()      == 2.0f);
    CHECK (apvts.getRawParameterValue ("timingDrift")->load() == Catch::Approx (12.0f));
    CHECK (apvts.getRawParameterValue ("variance")->load()    == Catch::Approx (0.5f));
    CHECK (apvts.getRawParameterValue ("detune")->load()      == Catch::Approx (8.0f));
    CHECK (apvts.getRawParameterValue ("width")->load()       == Catch::Approx (0.8f));
    CHECK (apvts.getRawParameterValue ("mix")->load()         == Catch::Approx (0.5f));
}

TEST_CASE ("Fully dry mix is a passthrough", "[doubler][engine]")
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;

    indie::DoublerEngine engine;
    engine.prepare (makeSpec (sr, block, 2));

    indie::DoublerEngine::Parameters params;
    params.mix = 0.0f;
    engine.setParameters (params);

    juce::Random rng (1);

    // Warm up so the mix smoothing settles fully to zero.
    for (int i = 0; i < 4; ++i)
    {
        juce::AudioBuffer<float> warm (2, block);
        fillNoise (warm, rng);
        engine.process (warm);
    }

    juce::AudioBuffer<float> input (2, block);
    fillNoise (input, rng);

    juce::AudioBuffer<float> output;
    output.makeCopyOf (input);
    engine.setParameters (params);
    engine.process (output);

    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < block; ++n)
            REQUIRE (output.getSample (ch, n) == Catch::Approx (input.getSample (ch, n)).margin (1.0e-6));
}

TEST_CASE ("Wet mix produces a delayed double", "[doubler][engine]")
{
    constexpr double sr = 48000.0;
    constexpr int block = 4096;

    indie::DoublerEngine engine;
    engine.prepare (makeSpec (sr, block, 2));

    indie::DoublerEngine::Parameters params;
    params.numVoices = 2;
    params.mix = 1.0f;       // pure wet so any output is the generated double
    params.width = 0.0f;
    params.variance = 0.3f;
    engine.setParameters (params);

    juce::AudioBuffer<float> buffer (2, block);
    buffer.clear();

    // A short burst at the start, silence afterwards.
    const int burstLen = 480; // 10 ms
    juce::Random rng (7);
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < burstLen; ++n)
            buffer.setSample (ch, n, (rng.nextFloat() * 2.0f - 1.0f) * 0.7f);

    engine.process (buffer);

    // The double is delayed (timing offset plus the pitch-shifter grain latency), so
    // there must be energy in the silent region that follows the burst.
    float tailPeak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = burstLen + 16; n < block; ++n)
            tailPeak = juce::jmax (tailPeak, std::abs (buffer.getSample (ch, n)));

    CHECK (tailPeak > 0.01f);
}

TEST_CASE ("Engine output is deterministic for a fixed seed", "[doubler][engine]")
{
    constexpr double sr = 48000.0;
    constexpr int block = 1024;

    auto run = [&] (juce::AudioBuffer<float>& out)
    {
        indie::DoublerEngine engine;
        engine.prepare (makeSpec (sr, block, 2));

        indie::DoublerEngine::Parameters params;
        params.numVoices = 4;
        params.variance = 1.0f;
        params.detuneCents = 25.0f;
        params.mix = 1.0f;
        engine.setParameters (params);

        juce::Random rng (123); // identical input each run
        fillNoise (out, rng);
        engine.process (out);
    };

    juce::AudioBuffer<float> a (2, block), b (2, block);
    run (a);
    run (b);

    // Bit-exact comparison (memcmp avoids the unsafe float == warning).
    for (int ch = 0; ch < 2; ++ch)
        REQUIRE (std::memcmp (a.getReadPointer (ch), b.getReadPointer (ch),
                              (size_t) block * sizeof (float)) == 0);
}

TEST_CASE ("OnsetDetector fires once per note and never on silence", "[doubler][onset]")
{
    constexpr double sr = 48000.0;
    indie::OnsetDetector detector;
    detector.prepare (sr);

    SECTION ("silence")
    {
        int onsets = 0;
        for (int n = 0; n < (int) sr; ++n)
            if (detector.processSample (0.0f))
                ++onsets;

        CHECK (onsets == 0);
    }

    SECTION ("single note burst")
    {
        int onsets = 0;

        // 100 ms of silence, then a 200 ms sustained note.
        for (int n = 0; n < (int) (0.1 * sr); ++n)
            if (detector.processSample (0.0f))
                ++onsets;

        const double freq = 220.0;
        const int noteSamples = (int) (0.2 * sr);
        for (int n = 0; n < noteSamples; ++n)
        {
            const float s = 0.5f * std::sin (juce::MathConstants<float>::twoPi * (float) freq * (float) n / (float) sr);
            if (detector.processSample (s))
                ++onsets;
        }

        CHECK (onsets == 1);
    }
}

TEST_CASE ("PitchShifter shifts pitch in the expected direction", "[doubler][pitch]")
{
    constexpr double sr = 48000.0;
    constexpr double freq = 200.0;
    constexpr int N = (int) sr; // 1 second

    // Counts output zero-crossings (proportional to pitch) after a warm-up period.
    auto countZeroCrossings = [&] (float ratio)
    {
        indie::PitchShifter ps;
        ps.prepare (sr);
        ps.setPitchRatio (ratio);

        int crossings = 0;
        float prev = 0.0f;
        for (int n = 0; n < N; ++n)
        {
            const float in = std::sin (juce::MathConstants<float>::twoPi * (float) freq * (float) n / (float) sr);
            const float out = ps.processSample (in);
            if (n > 8000 && (out > 0.0f) != (prev > 0.0f))
                ++crossings;
            prev = out;
        }
        return crossings;
    };

    const int base = countZeroCrossings (1.0f);
    const int up   = countZeroCrossings (1.5f);
    const int down = countZeroCrossings (0.7f);

    CHECK (up > base);
    CHECK (down < base);
}

TEST_CASE ("Warmth reduces wet high-frequency content", "[doubler][warmth]")
{
    constexpr double sr = 48000.0;
    constexpr int block = 8192;

    // Measures the high-frequency energy (via first-difference) of the pure-wet output.
    auto wetHighFreqEnergy = [&] (float warmth)
    {
        indie::DoublerEngine engine;
        engine.prepare (makeSpec (sr, block, 2));

        indie::DoublerEngine::Parameters params;
        params.numVoices = 2;
        params.mix = 1.0f;
        params.warmth = warmth;
        engine.setParameters (params);

        juce::Random rng (5);
        juce::AudioBuffer<float> buffer (2, block);
        fillNoise (buffer, rng);
        engine.process (buffer);

        double energy = 0.0;
        const float* d = buffer.getReadPointer (0);
        for (int n = 1; n < block; ++n)
        {
            const double diff = (double) d[n] - (double) d[n - 1];
            energy += diff * diff;
        }
        return energy;
    };

    CHECK (wetHighFreqEnergy (0.9f) < wetHighFreqEnergy (0.0f));
}

TEST_CASE ("Engine stays finite and bounded under extreme settings", "[doubler][rt]")
{
    const double sampleRates[] { 44100.0, 48000.0, 96000.0 };
    const int blockSizes[] { 32, 512 };

    for (double sr : sampleRates)
    {
        for (int block : blockSizes)
        {
            indie::DoublerEngine engine;
            engine.prepare (makeSpec (sr, block, 2));

            indie::DoublerEngine::Parameters params;
            params.numVoices = indie::DoublerEngine::kMaxVoices;
            params.variance = 1.0f;
            params.detuneCents = 25.0f;
            params.timingDriftMs = 40.0f;
            params.width = 1.0f;
            params.mix = 1.0f;
            engine.setParameters (params);

            juce::Random rng (99);
            for (int b = 0; b < 32; ++b)
            {
                juce::AudioBuffer<float> buffer (2, block);
                fillNoise (buffer, rng);
                engine.process (buffer);

                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < block; ++n)
                    {
                        const float s = buffer.getSample (ch, n);
                        REQUIRE (std::isfinite (s));
                        REQUIRE (std::abs (s) < 8.0f);
                    }
            }
        }
    }
}
