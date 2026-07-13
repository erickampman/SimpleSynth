// pm_compat.h — portability shim for vendoring the PolygraphModular DSP/voice framework
// off Apple platforms. The PM framework is only shallowly coupled to Apple: a few scalar
// typedefs, UMP MIDI, and os_log. This header collapses that surface so the framework
// compiles under a plain C++20 toolchain (Linux/clang/gcc) with no AudioToolbox/CoreMIDI.
//
// STATUS: foundation. The scalar typedefs + logging no-ops below are final. The UMP
// MIDIUniversalMessage struct is added when the framework is vendored (Phase 1b) — it must
// mirror the exact field access the PM modules use (message.channelVoice2.note.number,
// .velocity, .attributeType, .attribute, .status; message.type, .group; kMIDICV* constants).

#ifndef PM_COMPAT_H
#define PM_COMPAT_H

#include <cstdint>
#include <cstddef>

#if defined(__APPLE__)
// On Apple, use the real SDK types — the framework can build against AudioToolbox as before.
#  import <AudioToolbox/AudioToolbox.h>
#  import <CoreMIDI/CoreMIDI.h>
#else

// --- scalar typedefs (Apple aliases → plain C++ types) ---------------------------------
typedef float    AUValue;
typedef float    AudioUnitParameterValue;
typedef uint32_t AudioUnitParameterUnit;
typedef uint32_t AudioUnitParameterOptions;
typedef uint64_t AUParameterAddress;
typedef uint32_t AUAudioFrameCount;
typedef int64_t  AUEventSampleTime;
typedef unsigned long NSUInteger;
typedef long          NSInteger;

// --- logging (os_log → no-op; framework LOG_* macros funnel through these) ---------------
#  define os_log_with_type(...)   ((void)0)
#  define OS_LOG_TYPE_INFO        0

// --- UMP MIDI --------------------------------------------------------------------------
// TODO (Phase 1b): define a MIDIUniversalMessage struct compatible with the PM modules'
// field access, or adapt netmidi2's UMP types. Deliberately omitted until the framework
// files are vendored so it can be validated field-by-field against real usage.

#endif // !__APPLE__

#endif // PM_COMPAT_H
