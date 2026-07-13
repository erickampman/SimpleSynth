# SimpleSynth

A CLAP **software instrument**, being built as the vehicle for porting selected
[PolygraphModular](https://github.com/) DSP + voice-framework code (the IFS module family)
off AUv3 into a standalone, cross-platform CLAP synth. See `polygraph-clap-plan.md` (in
`~/dev-clap/`) for the full port plan.

## Status

**Polyphonic synth running the real PolygraphModular voice engine.** The PM framework has been
vendored off AUv3 and wired behind the CLAP surface: `process()` decodes CLAP note/MIDI/MIDI2
events and drives a `VoiceManager` (polyphony, note-stealing, mono mode) rendering the v1 voice
**PitchConverter → Slew → SinOsc → Combine ×ADSR → Final** per voice. Passes `clap-validator`.

Next: **Phase 3** drops in the IFS modules (IFSOsc/IFSEnv/IFSLFO) and a 24 dB/oct LPF
(pure module additions, no architectural change); **Phase 4** is the GUI. Full plan in
`~/dev-clap/polygraph-clap-plan.md`.

## Layout

Mirrors PolygraphModular's structure:

```
src/SimpleSynth.cpp   the CLAP plugin: entry, factory, extensions, VoiceManager host adapter
src/Common/           shim + MIDI + params + utilities
                        pm_compat.h (Apple→portable), pm_midi.* (spec UMP), clap_ump.h (CLAP↔UMP),
                        pm_params.h / pm_param_data.* / pm_addresses.h, ParamAddressMacros.h,
                        CPPTypes.hpp, CPPProcs.hpp, KPBuffPairHelper.hpp, PitchConverter.hpp
src/Module/           Module core + the six v1 modules
                        Module.hpp, ModuleFactory.*, SinOscillatorModule.*, ADSR{Helper,Module}.*,
                        CombineModule.*, FinalModule.*, SlewModule.*, PitchConverterModule.*
src/Voice/            voice engine
                        Voice.*, VoiceStateManager.*, VoiceManager.*, ModuleMap.*, ModuleGraph.hpp,
                        ModuleInitSpecs.hpp, pm_module_specs.cpp, ConnectionCommandQueue.hpp
src/UI/              (empty — the plugin GUI will live here, Phase 4)
test/                 in-process self-tests (see TESTING.md)
cmake/                per-platform export symbols + macOS Info.plist
external/clap         CLAP headers (submodule, pinned 1.2.9)
```

Vendored PolygraphModular files keep their original `#include "X.hpp"` form; every framework
folder is on the include path so they resolve across `Common/` `Module/` `Voice/`.

## Build

```sh
git clone --recurse-submodules <this-repo>
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure     # see TESTING.md for the full suite
```

Produces `build/SimpleSynth.clap`. Copy/symlink into `~/.clap` (Linux) or
`~/Library/Audio/Plug-Ins/CLAP/` (macOS), load in any CLAP host, and play — it's polyphonic
(8 voices by default; drop the **Gain** param if chords clip). Validate with
[`clap-validator`](https://github.com/free-audio/clap-validator). Test details in
[TESTING.md](TESTING.md).

## License

MIT.
