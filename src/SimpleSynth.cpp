// SimpleSynth — a CLAP instrument skeleton.
//
// This establishes the exact CLAP surface the PolygraphModular VoiceManager will plug into:
// note-ports in, stereo audio out, a params extension, state, and a sample-accurate event
// loop in process(). The DSP here is a PLACEHOLDER — a monophonic sine with a one-pole AR
// envelope — so the instrument is audible and validator-clean today. Phase 1b replaces the
// placeholder voice with the vendored PM framework behind this same surface.

#include <clap/clap.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ------------------------------------------------------------------- parameters ----------
enum { kParamGain = 0, kParamCount };

// ------------------------------------------------------------------- descriptor ----------
static const clap_plugin_descriptor_t s_desc = {
   .clap_version = CLAP_VERSION_INIT,
   .id = "com.example.simplesynth",
   .name = "SimpleSynth",
   .vendor = "example",
   .url = "https://example.com/simplesynth",
   .manual_url = "",
   .support_url = "",
   .version = "0.1.0",
   .description = "A minimal CLAP instrument (placeholder mono sine; PM framework to follow).",
   .features = (const char *[]){CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                CLAP_PLUGIN_FEATURE_STEREO, nullptr},
};

// ------------------------------------------------------------------- plugin state ---------
struct SimpleSynth {
   clap_plugin_t      plugin;
   const clap_host_t *host = nullptr;

   double sampleRate = 48000.0;

   // parameter
   float gain = 0.8f;

   // placeholder monophonic voice
   double phase    = 0.0;   // 0..1
   double phaseInc = 0.0;   // per-sample
   float  env      = 0.0f;  // 0..1
   bool   gate     = false;
   int    curKey   = -1;    // held MIDI key, -1 = none
   float  envCoeff = 0.001f;

   static double keyToHz(int key) { return 440.0 * std::pow(2.0, (key - 69) / 12.0); }

   void noteOn(int key) {
      curKey   = key;
      gate     = true;
      phaseInc = keyToHz(key) / sampleRate;
   }
   void noteOff(int key) {
      if (key < 0 || key == curKey)
         gate = false;
   }
};

// ------------------------------------------------------------------- audio ports ---------
static uint32_t audioPortsCount(const clap_plugin_t *, bool is_input) { return is_input ? 0 : 1; }
static bool audioPortsGet(const clap_plugin_t *, uint32_t index, bool is_input,
                          clap_audio_port_info_t *info) {
   if (is_input || index != 0)
      return false;
   info->id = 0;
   std::snprintf(info->name, sizeof(info->name), "Output");
   info->channel_count = 2;
   info->flags = CLAP_AUDIO_PORT_IS_MAIN;
   info->port_type = CLAP_PORT_STEREO;
   info->in_place_pair = CLAP_INVALID_ID;
   return true;
}
static const clap_plugin_audio_ports_t s_audio_ports = {audioPortsCount, audioPortsGet};

// ------------------------------------------------------------------- note ports ----------
static uint32_t notePortsCount(const clap_plugin_t *, bool is_input) { return is_input ? 1 : 0; }
static bool notePortsGet(const clap_plugin_t *, uint32_t index, bool is_input,
                         clap_note_port_info_t *info) {
   if (!is_input || index != 0)
      return false;
   info->id = 0;
   std::snprintf(info->name, sizeof(info->name), "Note In");
   info->supported_dialects =
      CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI2;
   info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
   return true;
}
static const clap_plugin_note_ports_t s_note_ports = {notePortsCount, notePortsGet};

// ------------------------------------------------------------------- params --------------
static uint32_t paramsCount(const clap_plugin_t *) { return kParamCount; }
static bool paramsGetInfo(const clap_plugin_t *, uint32_t index, clap_param_info_t *info) {
   if (index != kParamGain)
      return false;
   std::memset(info, 0, sizeof(*info));
   info->id = kParamGain;
   info->flags = CLAP_PARAM_IS_AUTOMATABLE;
   info->min_value = 0.0;
   info->max_value = 1.0;
   info->default_value = 0.8;
   std::snprintf(info->name, sizeof(info->name), "Gain");
   return true;
}
static bool paramsGetValue(const clap_plugin_t *plugin, clap_id id, double *out) {
   auto *s = static_cast<SimpleSynth *>(plugin->plugin_data);
   if (id != kParamGain)
      return false;
   *out = s->gain;
   return true;
}
static bool paramsValueToText(const clap_plugin_t *, clap_id id, double value, char *out,
                              uint32_t cap) {
   if (id != kParamGain)
      return false;
   std::snprintf(out, cap, "%.2f", value);
   return true;
}
static bool paramsTextToValue(const clap_plugin_t *, clap_id id, const char *text, double *out) {
   if (id != kParamGain)
      return false;
   *out = std::atof(text);
   return true;
}
static void applyParam(SimpleSynth *s, const clap_event_param_value_t *ev) {
   if (ev->param_id == kParamGain)
      s->gain = static_cast<float>(ev->value);
}
static void paramsFlush(const clap_plugin_t *plugin, const clap_input_events_t *in,
                        const clap_output_events_t *) {
   auto *s = static_cast<SimpleSynth *>(plugin->plugin_data);
   const uint32_t n = in->size(in);
   for (uint32_t i = 0; i < n; ++i) {
      const clap_event_header_t *h = in->get(in, i);
      if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE)
         applyParam(s, reinterpret_cast<const clap_event_param_value_t *>(h));
   }
}
static const clap_plugin_params_t s_params = {paramsCount,       paramsGetInfo,   paramsGetValue,
                                              paramsValueToText, paramsTextToValue, paramsFlush};

// ------------------------------------------------------------------- state ---------------
static bool stateSave(const clap_plugin_t *plugin, const clap_ostream_t *stream) {
   auto *s = static_cast<SimpleSynth *>(plugin->plugin_data);
   const uint8_t *p = reinterpret_cast<const uint8_t *>(&s->gain);
   uint64_t remaining = sizeof(s->gain);
   while (remaining) {
      int64_t w = stream->write(stream, p, remaining);
      if (w <= 0)
         return false;
      p += w;
      remaining -= static_cast<uint64_t>(w);
   }
   return true;
}
static bool stateLoad(const clap_plugin_t *plugin, const clap_istream_t *stream) {
   auto *s = static_cast<SimpleSynth *>(plugin->plugin_data);
   uint8_t buf[sizeof(float)];
   uint8_t *p = buf;
   uint64_t remaining = sizeof(buf);
   while (remaining) {
      int64_t r = stream->read(stream, p, remaining);
      if (r <= 0)
         return false;
      p += r;
      remaining -= static_cast<uint64_t>(r);
   }
   std::memcpy(&s->gain, buf, sizeof(s->gain));
   return true;
}
static const clap_plugin_state_t s_state = {stateSave, stateLoad};

// ------------------------------------------------------------------- process -------------
static void handleEvent(SimpleSynth *s, const clap_event_header_t *h) {
   if (h->space_id != CLAP_CORE_EVENT_SPACE_ID)
      return;
   switch (h->type) {
   case CLAP_EVENT_NOTE_ON: {
      auto *ev = reinterpret_cast<const clap_event_note_t *>(h);
      s->noteOn(ev->key);
      break;
   }
   case CLAP_EVENT_NOTE_OFF:
   case CLAP_EVENT_NOTE_CHOKE: {
      auto *ev = reinterpret_cast<const clap_event_note_t *>(h);
      s->noteOff(ev->key);
      break;
   }
   case CLAP_EVENT_PARAM_VALUE:
      applyParam(s, reinterpret_cast<const clap_event_param_value_t *>(h));
      break;
   default:
      break;
   }
}

static void renderBlock(SimpleSynth *s, float *outL, float *outR, uint32_t start, uint32_t end) {
   const float g = s->gain;
   for (uint32_t i = start; i < end; ++i) {
      const float target = s->gate ? 1.0f : 0.0f;
      s->env += s->envCoeff * (target - s->env);
      const float smp = static_cast<float>(std::sin(s->phase * 2.0 * M_PI)) * s->env * g;
      outL[i] = smp;
      outR[i] = smp;
      s->phase += s->phaseInc;
      if (s->phase >= 1.0)
         s->phase -= 1.0;
   }
}

static clap_process_status process(const clap_plugin_t *plugin, const clap_process_t *p) {
   auto *s = static_cast<SimpleSynth *>(plugin->plugin_data);
   if (p->audio_outputs_count < 1)
      return CLAP_PROCESS_ERROR;

   float *outL = p->audio_outputs[0].data32[0];
   float *outR = p->audio_outputs[0].data32[1];

   const uint32_t nframes = p->frames_count;
   const uint32_t nev = p->in_events->size(p->in_events);
   uint32_t evIndex = 0;
   uint32_t next = nev > 0 ? 0 : nframes;

   for (uint32_t i = 0; i < nframes;) {
      while (evIndex < nev && next == i) {
         const clap_event_header_t *h = p->in_events->get(p->in_events, evIndex);
         if (h->time != i) {
            next = h->time;
            break;
         }
         handleEvent(s, h);
         if (++evIndex == nev) {
            next = nframes;
            break;
         }
      }
      renderBlock(s, outL, outR, i, next);
      i = next;
   }
   return CLAP_PROCESS_CONTINUE;
}

// ------------------------------------------------------------------- plugin vtable -------
static bool plugInit(const clap_plugin_t *) { return true; }
static void plugDestroy(const clap_plugin_t *plugin) {
   delete static_cast<SimpleSynth *>(plugin->plugin_data);
}
static bool plugActivate(const clap_plugin_t *plugin, double sr, uint32_t, uint32_t) {
   auto *s = static_cast<SimpleSynth *>(plugin->plugin_data);
   s->sampleRate = sr;
   s->envCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (0.005 * sr))); // ~5ms AR
   return true;
}
static void plugDeactivate(const clap_plugin_t *) {}
static bool plugStartProcessing(const clap_plugin_t *) { return true; }
static void plugStopProcessing(const clap_plugin_t *) {}
static void plugReset(const clap_plugin_t *plugin) {
   auto *s = static_cast<SimpleSynth *>(plugin->plugin_data);
   s->phase = 0.0;
   s->env = 0.0f;
   s->gate = false;
   s->curKey = -1;
}
static const void *plugGetExtension(const clap_plugin_t *, const char *id) {
   if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
      return &s_audio_ports;
   if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS))
      return &s_note_ports;
   if (!std::strcmp(id, CLAP_EXT_PARAMS))
      return &s_params;
   if (!std::strcmp(id, CLAP_EXT_STATE))
      return &s_state;
   return nullptr;
}
static void plugOnMainThread(const clap_plugin_t *) {}

static clap_plugin_t *createPlugin(const clap_host_t *host) {
   auto *s = new SimpleSynth();
   s->host = host;
   s->plugin.desc = &s_desc;
   s->plugin.plugin_data = s;
   s->plugin.init = plugInit;
   s->plugin.destroy = plugDestroy;
   s->plugin.activate = plugActivate;
   s->plugin.deactivate = plugDeactivate;
   s->plugin.start_processing = plugStartProcessing;
   s->plugin.stop_processing = plugStopProcessing;
   s->plugin.reset = plugReset;
   s->plugin.process = process;
   s->plugin.get_extension = plugGetExtension;
   s->plugin.on_main_thread = plugOnMainThread;
   return &s->plugin;
}

// ------------------------------------------------------------------- factory + entry -----
static uint32_t factoryCount(const clap_plugin_factory_t *) { return 1; }
static const clap_plugin_descriptor_t *factoryDesc(const clap_plugin_factory_t *, uint32_t i) {
   return i == 0 ? &s_desc : nullptr;
}
static const clap_plugin_t *factoryCreate(const clap_plugin_factory_t *, const clap_host_t *host,
                                          const char *id) {
   if (!clap_version_is_compatible(host->clap_version) || std::strcmp(id, s_desc.id))
      return nullptr;
   return createPlugin(host);
}
static const clap_plugin_factory_t s_factory = {factoryCount, factoryDesc, factoryCreate};

static bool entryInit(const char *) { return true; }
static void entryDeinit(void) {}
static const void *entryGetFactory(const char *id) {
   return std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &s_factory;
}

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
   .clap_version = CLAP_VERSION_INIT,
   .init = entryInit,
   .deinit = entryDeinit,
   .get_factory = entryGetFactory,
};
