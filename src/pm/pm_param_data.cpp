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

// ADSR — 7 params. Times in seconds; sustain is a level 0..1.
static const ModuleParamConstants adsrConstants[] = {
   {"attack",    ADSRParamItemAttack,    0, 0, nullptr, 0.001f, 10.0f, 0.01f, 0},
   {"decay",     ADSRParamItemDecay,     0, 0, nullptr, 0.001f, 10.0f, 1.00f, 0},
   {"sustain",   ADSRParamItemSustain,   0, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"release",   ADSRParamItemRelease,   0, 0, nullptr, 0.001f, 10.0f, 1.00f, 0},
   {"gateInput", ADSRParamItemGateInput, 0, 0, nullptr, 0.0f, 3.4e38f, 0.0f, 0},
   {"legato",    ADSRParamItemLegato,    0, 0, nullptr, 0.0f, 2.0f, 0.0f, 0},
   {"linear",    ADSRParamItemLinear,    0, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
};

const ModuleParamConstantsGroup adsrConstantsGroup = {
   const_cast<char*>("adsr"),
   sizeof(adsrConstants) / sizeof(adsrConstants[0]),
   adsrConstants,
};
