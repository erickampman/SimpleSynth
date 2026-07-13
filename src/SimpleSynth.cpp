// SimpleSynth — a CLAP instrument driving the vendored PolygraphModular voice engine.
//
// process() decodes CLAP note/MIDI events into pm::UmpMessage and feeds them to a
// VoiceManager (polyphony, allocation, stealing, mono mode), which renders the v1 voice
// (PitchConverter → Slew → SinOsc → Combine ×ADSR → Final) per active voice and sums them.

#include <clap/clap.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "VoiceManager.hpp"
#include "clap_ump.h"
#include "pm_addresses.h"
#include "pm_params.h"

enum { kParamGain = 0, kParamCount };

static const clap_plugin_descriptor_t s_desc = {
   .clap_version = CLAP_VERSION_INIT,
   .id = "com.example.simplesynth",
   .name = "SimpleSynth",
   .vendor = "example",
   .url = "https://example.com/simplesynth",
   .manual_url = "",
   .support_url = "",
   .version = "0.2.0",
   .description = "A polyphonic CLAP instrument built on the PolygraphModular voice engine.",
   .features = (const char *[]){CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                CLAP_PLUGIN_FEATURE_STEREO, nullptr},
};

struct SimpleSynth {
   clap_plugin_t      plugin;
   const clap_host_t *host = nullptr;
   double             sampleRate = 48000.0;
   float              gain = 0.5f; // master gain (headroom for summed voices)

   std::unique_ptr<VoiceManager> vm;

   void setVMParam(MIDIParamItem item, float v) {
      vm->setVoiceManagerParameter(
         MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, item), v);
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
   info->default_value = 0.5;
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
   case CLAP_EVENT_NOTE_ON:
      s->vm->handleMIDIMessage(pm::fromClapNote(*reinterpret_cast<const clap_event_note_t *>(h), true));
      break;
   case CLAP_EVENT_NOTE_OFF:
   case CLAP_EVENT_NOTE_CHOKE:
      s->vm->handleMIDIMessage(pm::fromClapNote(*reinterpret_cast<const clap_event_note_t *>(h), false));
      break;
   case CLAP_EVENT_MIDI: {
      pm::UmpMessage m = pm::fromClapMidi1(*reinterpret_cast<const clap_event_midi_t *>(h));
      if (pm::isChannelMessage(m))
         s->vm->handleMIDIMessage(m);
      break;
   }
   case CLAP_EVENT_MIDI2:
      s->vm->handleMIDIMessage(pm::fromClapMidi2(*reinterpret_cast<const clap_event_midi2_t *>(h)));
      break;
   case CLAP_EVENT_PARAM_VALUE:
      applyParam(s, reinterpret_cast<const clap_event_param_value_t *>(h));
      break;
   default:
      break;
   }
}

// Render [start,end) by chunking to the module block size; VoiceManager zeroes + sums voices.
static void renderSegment(SimpleSynth *s, float *outL, float *outR, uint32_t start, uint32_t end) {
   const float g = s->gain;
   uint32_t pos = start;
   while (pos < end) {
      uint32_t chunk = end - pos;
      if (chunk > MODULE_BUFF_SIZE)
         chunk = MODULE_BUFF_SIZE;
      s->vm->process(chunk, outL + pos, outR + pos);
      for (uint32_t i = pos; i < pos + chunk; ++i) {
         outL[i] *= g;
         outR[i] *= g;
      }
      pos += chunk;
   }
}

static clap_process_status process(const clap_plugin_t *plugin, const clap_process_t *p) {
   auto *s = static_cast<SimpleSynth *>(plugin->plugin_data);
   if (p->audio_outputs_count < 1 || !s->vm)
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
      renderSegment(s, outL, outR, i, next);
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
   s->vm = std::make_unique<VoiceManager>(sr, MODULE_BUFF_SIZE);
   // VoiceManager's params aren't zero-initialised — set the v1 defaults.
   s->setVMParam(MIDIParamItemVoiceCount, 8);
   s->setVMParam(MIDIParamItemChannelFilterValue, 0);
   s->setVMParam(MIDIParamItemGroupFilterValue, 0);
   s->setVMParam(MIDIParamItemVoiceStealAlgorithm, VoiceStealAlgorithmOldest);
   s->setVMParam(MIDIParamItemFreeRun, 0);
   s->setVMParam(MIDIParamItemLegato, 0);
   return true;
}
static void plugDeactivate(const clap_plugin_t *plugin) {
   static_cast<SimpleSynth *>(plugin->plugin_data)->vm.reset();
}
static bool plugStartProcessing(const clap_plugin_t *) { return true; }
static void plugStopProcessing(const clap_plugin_t *) {}
static void plugReset(const clap_plugin_t *plugin) {
   auto *s = static_cast<SimpleSynth *>(plugin->plugin_data);
   if (s->vm)
      s->vm->reset();
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
