# Indie Vocal Suite — Roadmap

The plan for the **Indie** suite of audio plugins (by Dr Fix Audio) and the repositories
that make it up. This document is the source of truth for cross-repo phase status; the
per-repo `CLAUDE.md` files link here.

## Repositories

- **indie-shared-dsp** — reusable, realtime-safe DSP building blocks packaged as a JUCE
  module and consumed by every plugin as a `modules/` submodule. Current primitives:
  `OnePoleFilter`, `DelayLine`, `EnvelopeFollower`, `StereoField`, `SaturationStage`,
  `Modulators` (`SineLFO`, `RandomWalk`), `PitchShifter`, `Diffuser`, `OnsetDetector`.
- **indie-plugin-template** — the starting point for new plugins. Bundles the JUCE/CMake
  build system, CI/CD, and the `indie_shared_dsp` submodule. `rename.sh` stamps a new
  plugin's identity.
- **indie-doubler** — the first suite plugin: a vocal/instrument doubler.
- **indie-slap** — created from the template; a classic slapback delay (Phase 1).

## Phases

### Phase 0 — Foundations ✅ (a–d done)

- **a.** Stand up `indie-shared-dsp` as a JUCE module with the first reusable primitives.
- **b.** Build `indie-doubler` on top of those primitives.
- **c.** Extract the reusable scaffolding into `indie-plugin-template` (build system, CI/CD,
  shared-DSP submodule wiring).
- **d.** Factor common DSP up out of the doubler into the shared module
  (`SaturationStage`, `Modulators`, etc.) so later plugins start from primitives, not scratch.

### Phase 1 — Indie Slap ✅ (done)

A classic slapback delay for guitar/vocals: a single (or few) short, tight echo(es)
(~60–180 ms) with analog-style tone shaping on the repeat(s) — darker/warmer tone, subtle
tape-style pitch wobble, placed in the stereo field.

- Created `indie-slap` from the template and stamped its identity via `rename.sh`
  (`IndieSlap` / "Indie Slap" / `com.drfixaudio.indieslap` / `Drfx` / `Slap`).
- **Promoted a new `DelayLine` primitive into `indie-shared-dsp`** — a mono fractional
  delay line with a `prepare()`-sized circular buffer and optional feedback. It's generic
  enough that other delay/echo-based plugins in the suite will reuse it, so it lives in the
  shared module rather than in the plugin.
- Implemented the slapback processor from the new `DelayLine` plus existing primitives:
  `OnePoleFilter` (damping/tone), `SaturationStage` (warmth on the repeat), `Modulators`
  (`SineLFO` + `RandomWalk` for the wobble) and `StereoField` (placement). The repeat is
  damped and saturated *inside* the feedback loop, so each successive echo gets
  progressively darker and warmer.
- Parameters: Time, Repeats (feedback), Tone (damping), Wobble, Width, Mix.
- Tests/benchmarks added (DelayLine tap/feedback/silence, dry passthrough at mix=0,
  delayed-echo location, feedback stability, processBlock benchmark) and the placeholder
  name check fixed.

### Future phases

Additional Indie plugins, built the same way — start from `indie-plugin-template`, reuse
`indie-shared-dsp` primitives, and promote any genuinely reusable new DSP back up into the
shared module.
