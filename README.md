# Indie Doubler

**Intelligent double tracking for guitar and vocals**, by Dr Fix Audio.

Most doubling plugins just delay, detune, or chorus a signal — which sounds obviously fake.
Real doubles have timing intent, phrasing variance, articulation differences, and (for guitar)
pick dynamics. Indie Doubler aims to model that performance realism instead of faking it with
simple DSP tricks.

## Status

Early development. Built on the [Pamplejuce](https://github.com/sudara/pamplejuce) JUCE plugin
template (C++23, CMake, Catch2).

## Building

See [CLAUDE.md](CLAUDE.md) for build commands and project structure.

```bash
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Builds --config Debug
ctest --test-dir Builds --verbose --output-on-failure
```
