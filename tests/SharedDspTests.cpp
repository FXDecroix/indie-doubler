#include <indie_shared_dsp/indie_shared_dsp.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
// High-frequency energy of a signal via first-difference (proportional to HF content).
double highFreqEnergy (const std::vector<float>& x)
{
    double e = 0.0;
    for (size_t n = 1; n < x.size(); ++n)
    {
        const double d = (double) x[n] - (double) x[n - 1];
        e += d * d;
    }
    return e;
}
} // namespace

TEST_CASE ("OnePoleFilter passes DC and settles to the input level", "[shared][onepole]")
{
    constexpr double sr = 48000.0;
    indie::OnePoleFilter lp;
    lp.prepare (sr);
    lp.setCutoff (1000.0f);

    float y = 0.0f;
    for (int n = 0; n < (int) sr; ++n) // 1 second of DC = 1.0
        y = lp.processSample (1.0f);

    // A low-pass has unity DC gain: the output must converge to the input.
    CHECK (y == Catch::Approx (1.0f).margin (1.0e-4f));
}

TEST_CASE ("EnvelopeFollower with instant attack tracks a peak immediately", "[shared][envelope]")
{
    constexpr double sr = 48000.0;
    indie::EnvelopeFollower env;
    env.prepare (sr);
    env.setAttackTime (0.0f);       // instant
    env.setReleaseTime (0.050f);

    // First non-zero sample should jump straight to its magnitude.
    const float y = env.processSample (-0.8f);
    CHECK (y == Catch::Approx (0.8f));
}

TEST_CASE ("EnvelopeFollower releases slower with a longer release time", "[shared][envelope]")
{
    constexpr double sr = 48000.0;

    auto valueAfterDecay = [&] (float releaseSeconds)
    {
        indie::EnvelopeFollower env;
        env.prepare (sr);
        env.setAttackTime (0.0f);
        env.setReleaseTime (releaseSeconds);

        env.processSample (1.0f);   // charge to 1.0
        for (int n = 0; n < (int) (0.010 * sr); ++n) // 10 ms of silence
            env.processSample (0.0f);
        return env.getCurrentValue();
    };

    // A longer release time should leave more level after the same decay window.
    CHECK (valueAfterDecay (0.100f) > valueAfterDecay (0.010f));
}

TEST_CASE ("OnePoleFilter attenuates more high frequency at a lower cutoff", "[shared][onepole]")
{
    constexpr double sr = 48000.0;
    constexpr int N = 8192;

    juce::Random rng (11);
    std::vector<float> input ((size_t) N);
    for (auto& s : input)
        s = rng.nextFloat() * 2.0f - 1.0f;

    auto filtered = [&] (float cutoffHz)
    {
        indie::OnePoleFilter lp;
        lp.prepare (sr);
        lp.setCutoff (cutoffHz);

        std::vector<float> out ((size_t) N);
        for (int n = 0; n < N; ++n)
            out[(size_t) n] = lp.processSample (input[(size_t) n]);
        return out;
    };

    // A lower cutoff must remove more high-frequency content.
    CHECK (highFreqEnergy (filtered (1000.0f)) < highFreqEnergy (filtered (10000.0f)));
}
