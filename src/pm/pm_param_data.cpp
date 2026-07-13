#include "pm_param_data.h"

// SinOsc — 11 params. All defaults 0 (→ produces A440 with no pitch input connected).
// name / item / unit / flags / valueStrings / min / max / def / info
static const ModuleParamConstants sinOscConstants[] = {
   {"pitch",         SinOscParamItemPitch,         0, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"cents",         SinOscParamItemPitchFine,     0, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"pitchInput",    SinOscParamItemPitchInput,    0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"modPitch",      SinOscParamItemModPitch,      0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"modPitch2",     SinOscParamItemModPitch2,     0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"modPhase",      SinOscParamItemModPhase,      0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"freqModLevel",  SinOscParamItemFreqModLevel,  0, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"phaseModLevel", SinOscParamItemPhaseModLevel, 0, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"freqModLevel2", SinOscParamItemFreqModLevel2, 0, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"modSync",       SinOscParamItemModSync,       0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"modSyncType",   SinOscParamItemModSyncType,   0, 0, nullptr, 0.0f, 2.0f, 0.0f, 0},
};

const ModuleParamConstantsGroup sinOscConstantsGroup = {
   const_cast<char*>("sinOsc"),
   sizeof(sinOscConstants) / sizeof(sinOscConstants[0]),
   sinOscConstants,
};
