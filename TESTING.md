# Testing SimpleSynth

The project ships a suite of in-process self-tests (via CTest) plus the CLAP conformance
validator. Everything runs headless on Linux/macOS — no DAW or audio device required.

## Prerequisites

- CMake ≥ 3.17 and a C++20 compiler (gcc or clang).
- The CLAP headers submodule:

  ```sh
  git submodule update --init --recursive
  ```

## Build and run all tests

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Add `-DSIMPLESYNTH_WERROR=ON` at configure time to treat warnings as errors (this is what CI
uses). Vendored PolygraphModular headers have their own warning style, so that flag suppresses
a small, fixed set of vendored-only warnings while keeping our own glue strict.

## What each test covers

| CTest name       | Executable               | Verifies |
|------------------|--------------------------|----------|
| `smoke`          | `simplesynth-smoke`      | Full CLAP lifecycle + a note produces audio and decays — now through the real VoiceManager |
| `pm`             | `pm-selftest`            | UMP MIDI layer: predicates, raw-word decode, microtonal pitch attribute, CLAP→UMP decoders |
| `pm-module`      | `pm-module-test`         | `Module` + `ModuleMap` core: param read/write, process proc, map add/get |
| `pm-sinosc`      | `pm-sinosc-test`         | The vendored sine oscillator produces an ~A440 sine |
| `pm-adsr`        | `pm-adsr-test`           | ADSR envelope: note-on drives attack, note-off drives release to ~0 |
| `pm-modules`     | `pm-modules-test`        | Combine / Final / Slew compile+run; PitchConverter maps note→pitch |
| `pm-factory`     | `pm-factory-test`        | ModuleFactory + the trimmed init-specs build all six v1 modules |
| `pm-voice`       | `pm-voice-test`          | One full voice renders a note end-to-end and releases |
| `pm-voicemanager`| `pm-voicemanager-test`   | Polyphony: a 3-note chord allocates 3 voices and frees them on release |

## Running a single test

```sh
ctest --test-dir build -R pm-voice --output-on-failure   # by name (regex)
./build/pm-voice-test                                     # or run the binary directly
```

Each test binary prints diagnostic values (e.g. `Voice: peak=0.87 tail=0.0000`) and ends with
`ALL PASS`; CTest keys success on that string.

## CLAP conformance

Validate the built plugin against the CLAP spec with
[`clap-validator`](https://github.com/free-audio/clap-validator):

```sh
clap-validator validate build/SimpleSynth.clap
```

## Continuous integration

`.github/workflows/ci.yml` runs the configure + build + `ctest` on every push across
Linux (gcc/clang) and macOS (clang) with `-DSIMPLESYNTH_WERROR=ON`.
