# SimpleSynth

A CLAP **software instrument**, being built as the vehicle for porting selected
[PolygraphModular](https://github.com/) DSP + voice-framework code (the IFS module family)
off AUv3 into a standalone, cross-platform CLAP synth. See `polygraph-clap-plan.md` (in
`~/dev-clap/`) for the full port plan.

## Status

**Phase 1 — CLAP instrument skeleton.** Establishes the surface the PolygraphModular
`VoiceManager` will plug into: note-ports in, stereo audio out, a `params` extension, `state`,
and a sample-accurate `process()` event loop. The DSP is currently a **placeholder**
(monophonic sine + one-pole AR envelope) so the instrument is audible and passes
`clap-validator` today.

Next: **Phase 1b** vendors the PM framework (via the `pm_compat.h` shim) and swaps the
placeholder voice for the real `VoiceManager` behind this same surface.

## Layout

```
src/SimpleSynth.cpp   the CLAP instrument (entry, factory, extensions, placeholder voice)
src/pm_compat.h       Apple→portable shim for the (upcoming) vendored PM framework
test/host_smoke.cpp   in-process lifecycle + note self-test (ctest)
cmake/                per-platform export symbols + macOS Info.plist
external/clap         CLAP headers (submodule, pinned 1.2.9)
```

## Build

```sh
git clone --recurse-submodules <this-repo>
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure     # note-in → audio-out self-test
```

Produces `build/SimpleSynth.clap`. Copy/symlink into `~/.clap` (Linux) or
`~/Library/Audio/Plug-Ins/CLAP/` (macOS), load in any CLAP host, and play — it's a mono sine
for now. Validate with [`clap-validator`](https://github.com/free-audio/clap-validator).

## License

MIT.
