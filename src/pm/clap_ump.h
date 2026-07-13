// clap_ump.h — the CLAP boundary: decode CLAP note / MIDI2 events into pm::UmpMessage.
// This is the single seam where host events become the framework's internal MIDI type
// (and where a future netmidi2 *network* UMP stream would decode in the same way).

#ifndef PM_CLAP_UMP_H
#define PM_CLAP_UMP_H

#include <clap/clap.h>

#include "pm_midi.h"

namespace pm {

// clap_event_note_t → UMP (integer-key path; no microtonal attribute).
inline UmpMessage fromClapNote(const clap_event_note_t& ev, bool on) {
   UmpMessage m;
   m.messageType   = kUmpTypeChannelVoice2;
   m.status        = on ? kStatusNoteOn : kStatusNoteOff;
   m.channel       = ev.channel < 0 ? 0 : static_cast<uint8_t>(ev.channel & 0x0F);
   m.note          = ev.key < 0 ? 0 : static_cast<uint8_t>(ev.key & 0x7F);
   double v        = ev.velocity < 0.0 ? 0.0 : (ev.velocity > 1.0 ? 1.0 : ev.velocity);
   m.velocity      = on ? static_cast<uint16_t>(v * 65535.0 + 0.5) : 0;
   m.attributeType = kAttrNone;
   return m;
}

// clap_event_midi2_t → UMP (raw words; carries the 7.9 pitch attribute → microtonal).
inline UmpMessage fromClapMidi2(const clap_event_midi2_t& ev) {
   return decodeUmp(ev.data); // ev.data is uint32_t[4]; the channel-voice message is words 0..1
}

// clap_event_midi_t (MIDI 1.0, 3 bytes) → UMP note/CC. messageType left 0 (ignored) if unhandled.
inline UmpMessage fromClapMidi1(const clap_event_midi_t& ev) {
   UmpMessage m;
   const uint8_t statusNibble = ev.data[0] & 0xF0;
   m.channel = ev.data[0] & 0x0F;
   m.note = ev.data[1] & 0x7F;
   const uint8_t data2 = ev.data[2] & 0x7F;
   if (statusNibble == 0x90 && data2 > 0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusNoteOn;
      m.velocity = static_cast<uint16_t>(data2 << 9); // 7-bit → 16-bit
   } else if (statusNibble == 0x80 || (statusNibble == 0x90 && data2 == 0)) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusNoteOff;
   } else if (statusNibble == 0xB0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusControlChange;
      m.ccIndex = ev.data[1] & 0x7F;
      m.ccData = static_cast<uint32_t>(data2) << 25; // 7-bit → 32-bit
   }
   return m;
}

} // namespace pm

#endif // PM_CLAP_UMP_H
