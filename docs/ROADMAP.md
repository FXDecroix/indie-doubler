# Indie Vocal Suite — Roadmap

This tracks the multi-repo plan for the Indie plugin suite (by Dr Fix Audio). It spans
three repos:

- `FXDecroix/indie-doubler` — the first plugin (Indie Doubler), and the source of the
  reusable DSP primitives.
- `FXDecroix/indie-shared-dsp` — standalone JUCE module of reusable DSP building blocks,
  consumed as a git submodule by every plugin.
- `FXDecroix/indie-plugin-template` — stripped-down starting point for new plugins.

Approved architecture: **repo-per-plugin**. Each plugin is its own repo, consuming
`indie_shared_dsp` as a git submodule via `juce_add_module` (not `add_subdirectory` —
the module repo has no CMakeLists of its own). New plugins are spun up from
`indie-plugin-template` + `./rename.sh`.

## Phase 0 — extract shared DSP, stand up the template (DONE)

### Phase 0a — extract DSP primitives (done)

Five DSP primitives were extracted from Indie Doubler into `shared/indie_shared_dsp/`
(a JUCE-module-shaped folder: folder name == module ID == `indie_shared_dsp`,
`BEGIN_JUCE_MODULE_DECLARATION` header present, umbrella header
`indie_shared_dsp.h`): `OnePoleFilter`, `EnvelopeFollower`, `StereoField`,
`SaturationStage`, `Modulators`. The doubler was rewired to consume them, and a Drive
control was added using `SaturationStage`. Tests pass (366k+ assertions).
Landed on `indie-doubler` branch `claude/plugin-reusable-components-t8wgjw`.

### Phase 0b — promote indie_shared_dsp to its own repo (done)

- History preserved with `git subtree split --prefix=shared` (the `shared/` folder in
  indie-doubler contained only `indie_shared_dsp/`, so splitting on `shared` directly
  produces the desired `indie_shared_dsp/indie_shared_dsp.h` nested layout — mirroring
  melatonin_inspector's repo-contains-folder convention).
- Merged into `FXDecroix/indie-shared-dsp` with `--allow-unrelated-histories`.
- Added README + LICENSE at repo root. No CMakeLists inside the module repo — it's
  consumed via `juce_add_module`, never `add_subdirectory`.
- Branch promoted to `main` (the repo's default/integration branch).

### Phase 0c — migrate indie-doubler to consume the submodule (done)

- `git rm -r shared/indie_shared_dsp`; `git submodule add` of
  `FXDecroix/indie-shared-dsp` at `modules/indie_shared_dsp`, tracking `main`.
- `CMakeLists.txt`: removed the old glob block that included `shared/indie_shared_dsp`,
  added `juce_add_module(modules/indie_shared_dsp/indie_shared_dsp)`, added
  `indie_shared_dsp` to the `SharedCode` `target_link_libraries` list.
- Verification gate passed: configure, build, `ctest` all green (366k+ assertions).
- Old `shared/` include dir confirmed fully removed (no ambiguous headers).
- CI checkout already uses `submodules: recursive`.

### Phase 0d — populate indie-plugin-template (done)

- Stripped copy of indie-doubler: doubler-specific `source/dsp/` (DoublerEngine,
  DoubleVoice) and doubler-specific tests (`DoublerTests.cpp`, `SharedDspTests.cpp`)
  removed.
- `PluginProcessor`/`PluginEditor` replaced with a minimal generic gain plugin that
  demonstrates wiring `indie::OnePoleFilter` from the shared module, as an example for
  new plugins to swap out.
- `indie_shared_dsp` submodule added/wired the same way as in indie-doubler. CI
  workflows, packaging, and other CMake modules carried over intact.
- `CMakeLists.txt` placeholders set to generic template values (`IndiePluginTemplate`,
  "Indie Plugin Template", `com.drfixaudio.indieplugintemplate`, `PLUGIN_CODE Tmpl`).
- Added `rename.sh ProjectName "Product Name" com.company.product MnfCode PlugCode` —
  sed-replaces `PROJECT_NAME`, `PRODUCT_NAME`, `BUNDLE_ID`, `PLUGIN_MANUFACTURER_CODE`,
  `PLUGIN_CODE`, `PRODUCT_NAME_WITHOUT_VERSION` in `CMakeLists.txt`. Verified standalone.
- Verification gate passed: submodules initialized, configure, build, `ctest` all green.
- Pushed to `claude/relaxed-einstein-m0zors`.

## Phase 1 — build Indie Slap from the template (NEXT)

Goal: stand up `FXDecroix/indie-slap`, the second plugin in the suite, proving the
template + shared-DSP workflow end to end for a brand new plugin.

Suggested steps (to be refined when this phase starts):

1. Create the `FXDecroix/indie-slap` repo (e.g. via GitHub "use this template" on
   `indie-plugin-template`, or clone + push to a fresh repo).
2. Run `./rename.sh IndieSlap "Indie Slap" com.drfixaudio.indieslap Drfx <PlugCode>`
   to set the plugin identity.
3. Initialize submodules (`git submodule update --init --recursive`); confirm a clean
   build/test of the unmodified template under the new identity (verification gate,
   same as Phase 0d).
4. Design and implement Indie Slap's actual DSP in `source/`, reusing
   `indie_shared_dsp` primitives where applicable (e.g. `EnvelopeFollower`,
   `SaturationStage`) instead of duplicating logic.
5. Add plugin-specific tests/benchmarks (mirroring `DoublerTests.cpp`'s role for
   Indie Doubler) and replace the placeholder `PluginBasics.cpp` name check.
6. Decide whether any new DSP primitives built for Indie Slap are generic enough to be
   promoted back into `indie-shared-dsp` (keeping the shared module growing
   incrementally rather than duplicating across plugins).

Open questions for whoever picks this up: What is Indie Slap's actual DSP concept (the
name suggests a transient/slap-back or percussive effect, but this hasn't been
specified yet)? Confirm with the user before designing the DSP.
