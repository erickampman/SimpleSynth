// host_smoke — drives SimpleSynth through its CLAP lifecycle plus a note, in-process (no
// dlopen), and checks that a note produces audio and note-off decays it. Prints "ALL PASS".

#include <clap/clap.h>

#include <cmath>
#include <cstdio>
#include <cstring>

extern "C" const clap_plugin_entry_t clap_entry;

#define CHECK(cond, msg)                                                                           \
   do {                                                                                            \
      if (!(cond)) {                                                                               \
         std::fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);                    \
         return 1;                                                                                 \
      }                                                                                            \
   } while (0)

static const void *hostGetExtension(const clap_host_t *, const char *) { return nullptr; }
static void hostNoop(const clap_host_t *) {}
static const clap_host_t g_host = {CLAP_VERSION_INIT, nullptr, "smoke", "test", "", "0",
                                   hostGetExtension,  hostNoop, hostNoop, hostNoop};

// one event carried by the input list
static clap_event_note_t g_note;
static bool              g_have_note = false;

static uint32_t inSize(const clap_input_events_t *) { return g_have_note ? 1 : 0; }
static const clap_event_header_t *inGet(const clap_input_events_t *, uint32_t i) {
   return (g_have_note && i == 0) ? &g_note.header : nullptr;
}
static bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }

static float rms(const float *b, uint32_t n) {
   double s = 0;
   for (uint32_t i = 0; i < n; ++i)
      s += double(b[i]) * b[i];
   return float(std::sqrt(s / n));
}

int main() {
   CHECK(clap_entry.init("smoke"), "entry.init");
   auto *factory =
      static_cast<const clap_plugin_factory_t *>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
   CHECK(factory && factory->get_plugin_count(factory) == 1, "factory");
   const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
   CHECK(desc, "descriptor");
   std::printf("instrument: %s (%s)\n", desc->name, desc->id);

   const clap_plugin_t *plugin = factory->create_plugin(factory, &g_host, desc->id);
   CHECK(plugin && plugin->init(plugin), "create/init");

   const uint32_t N = 512;
   CHECK(plugin->activate(plugin, 48000.0, 1, N), "activate");
   CHECK(plugin->start_processing(plugin), "start");

   float L[512], R[512];
   float *chans[2] = {L, R};
   clap_audio_buffer_t out = {chans, nullptr, 2, 0, 0};
   clap_input_events_t inEv = {nullptr, inSize, inGet};
   clap_output_events_t outEv = {nullptr, outPush};
   clap_process_t proc = {0, N, nullptr, nullptr, &out, 0, 1, &inEv, &outEv};

   // Note On at frame 0 (key 69 = A440), render a block.
   std::memset(&g_note, 0, sizeof(g_note));
   g_note.header.size = sizeof(g_note);
   g_note.header.type = CLAP_EVENT_NOTE_ON;
   g_note.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
   g_note.note_id = -1;
   g_note.port_index = 0;
   g_note.channel = 0;
   g_note.key = 69;
   g_note.velocity = 1.0;
   g_have_note = true;

   CHECK(plugin->process(plugin, &proc) == CLAP_PROCESS_CONTINUE, "process(note on)");
   // let the envelope open over a few blocks
   g_have_note = false;
   for (int b = 0; b < 8; ++b)
      plugin->process(plugin, &proc);
   const float loud = rms(L, N);
   CHECK(loud > 0.01f, "note produces audio");
   std::printf("sounding RMS = %.4f\n", loud);

   // Note Off, let it decay.
   g_note.header.type = CLAP_EVENT_NOTE_OFF;
   g_have_note = true;
   plugin->process(plugin, &proc);
   g_have_note = false;
   for (int b = 0; b < 64; ++b)
      plugin->process(plugin, &proc);
   const float quiet = rms(L, N);
   CHECK(quiet < loud, "note off decays");
   std::printf("released RMS = %.4f\n", quiet);

   plugin->stop_processing(plugin);
   plugin->deactivate(plugin);
   plugin->destroy(plugin);
   clap_entry.deinit();

   std::printf("ALL PASS\n");
   return 0;
}
