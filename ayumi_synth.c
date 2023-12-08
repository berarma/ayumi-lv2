#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <lv2/midi/midi.h>
#include "ayumi.h"
#include "ayumi_synth.h"

#define DEFAULT_CLOCK 2e6 /* clock_rate / (sample_rate * 8 * 8) must be < 1.0 */

#define AYMODE 0
#define YMMODE 1

#define round(n) ((int)(n + 0.5))
#define level(volume, velocity) (round((int)(volume * velocity / 127.0)))

#define max(a,b) \
    ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
    ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


static int note_to_period(AyumiSynth* synth, double note);
static void ayumi_synth_update(AyumiSynth* synth, int cn);
static void ayumi_synth_reset(AyumiSynth* synth);

int ayumi_synth_init(AyumiSynth* synth, double sample_rate) {
    synth->mode = YMMODE;
    synth->clock = DEFAULT_CLOCK;
    synth->sample_rate = sample_rate;
    synth->remove_dc = false;
    synth->ayumi = calloc(sizeof(struct ayumi), 1);

    const bool ok = ayumi_configure(synth->ayumi, synth->mode, synth->clock, (int) synth->sample_rate);

    ayumi_synth_reset(synth);

    return ok;
}

void ayumi_synth_process(AyumiSynth* synth, float* left, float *right) {
    ayumi_process(synth->ayumi);
    if (synth->remove_dc) {
        ayumi_remove_dc(synth->ayumi);
    }
    *left = (float) synth->ayumi->left;
    *right = (float) synth->ayumi->right;
}

void ayumi_synth_close(AyumiSynth* synth) {
    free(synth->ayumi);
}

inline void ayumi_synth_set_remove_dc(AyumiSynth* synth, bool remove_dc) {
    synth->remove_dc = remove_dc;
}

inline int ayumi_synth_set_clock(AyumiSynth* synth, int clock) {
    synth->ayumi->step = clock / (synth->sample_rate * 8 * DECIMATE_FACTOR); // XXX Ayumi internals
    return synth->ayumi->step < 1;
}

inline void ayumi_synth_set_mode(AyumiSynth* synth, bool mode) {
    synth->ayumi->dac_table = mode ? YM_dac_table : AY_dac_table; // XXX Ayumi internals
}

inline void ayumi_synth_set_noise_period(AyumiSynth* synth, int period) {
    ayumi_set_noise(synth->ayumi, period);
}

inline void ayumi_synth_set_envelope_period(AyumiSynth* synth, int period) {
    ayumi_set_envelope(synth->ayumi, period);
}

inline void ayumi_synth_set_envelope_shape(AyumiSynth* synth, int shape) {
    ayumi_set_envelope_shape(synth->ayumi, shape);
}

inline void ayumi_synth_set_envelope_hold(AyumiSynth* synth, bool hold) {
    ayumi_set_envelope_shape(synth->ayumi, (synth->ayumi->envelope_shape & 0xE) | hold);
}

inline void ayumi_synth_set_envelope_alternate(AyumiSynth* synth, bool alternate) {
    ayumi_set_envelope_shape(synth->ayumi, (synth->ayumi->envelope_shape & 0xD) | (alternate << 1));
}

inline void ayumi_synth_set_envelope_attack(AyumiSynth* synth, bool attack) {
    ayumi_set_envelope_shape(synth->ayumi, (synth->ayumi->envelope_shape & 0xB) | (attack << 2));
}

inline void ayumi_synth_set_envelope_continue(AyumiSynth* synth, bool cont) {
    ayumi_set_envelope_shape(synth->ayumi, (synth->ayumi->envelope_shape & 0x7) | (cont << 8));
}

inline void ayumi_synth_set_tone_period(AyumiSynth* synth, int index, int period) {
    ayumi_set_tone(synth->ayumi, index, period);
}

inline void ayumi_synth_set_volume(AyumiSynth* synth, int index, int volume, bool envelope) {
    ayumi_set_volume(synth->ayumi, index, volume);
    ayumi_synth_set_envelope(synth, index, envelope);
}

inline void ayumi_synth_set_envelope(AyumiSynth* synth, int index, bool envelope) {
    synth->ayumi->channels[index].e_on = envelope; // XXX Ayumi internals
}

inline void ayumi_synth_set_mixer(AyumiSynth* synth, int index, bool tone, bool noise) {
    ayumi_synth_set_tone(synth, index, tone);
    ayumi_synth_set_noise(synth, index, noise);
}

inline void ayumi_synth_set_tone(AyumiSynth* synth, int index, bool tone) {
    synth->ayumi->channels[index].t_off = tone ? 0 : 1; // XXX Ayumi internals
}

inline void ayumi_synth_set_noise(AyumiSynth* synth, int index, bool noise) {
    synth->ayumi->channels[index].n_off = noise ? 0 : 1; // XXX Ayumi internals
}

void ayumi_synth_midi(AyumiSynth* synth, uint8_t status, uint8_t data[]) {
    AyumiSynthChannel* channel;

    int index = status & 0xF;
    if (index > 2)
        return;

    channel = &synth->channels[index];

    switch (lv2_midi_message_type(&status)) {
        case LV2_MIDI_MSG_NOTE_OFF: // Note off
            if (channel->note != data[0])
                break; // not at note on state
            ayumi_synth_set_volume(synth, index, 0, false);
            channel->note = -1;
            break;
        case LV2_MIDI_MSG_NOTE_ON:
            ayumi_synth_set_volume(synth, index, round(channel->volume * data[1] / 127.0), channel->envelope_on);
            ayumi_synth_set_envelope_shape(synth, synth->envelope_shape); // This will restart the envelope
            ayumi_set_tone(synth->ayumi, index, note_to_period(synth, data[0]));
            channel->note = data[0];
            channel->velocity = data[1];
            break;
        case LV2_MIDI_MSG_BENDER:
            {
                const int pitch = data[0] + (data[1] << 7) - 0x2000;
                ayumi_set_tone(synth->ayumi, index, note_to_period(synth, channel->note + (pitch / 8192.0) * 12));
            }
            break;
        case LV2_MIDI_MSG_PGM_CHANGE: // Mixer
            {
                bool tone_off = data[0] & 1 ? 1 : 0;
                bool noise_off = (data[0] & 3) == 0 || (data[0] & 3) == 3 ? 1 : 0;
                channel->envelope_on = data[0] > 3 ? 1 : 0;
                ayumi_set_mixer(synth->ayumi, index, tone_off, noise_off, channel->envelope_on);
            }
            break;
        case LV2_MIDI_MSG_RESET:
            ayumi_synth_reset(synth);
            break;
        case LV2_MIDI_MSG_CONTROLLER:
            switch (data[0]) {
                case LV2_MIDI_CTL_RESET_CONTROLLERS: // Reset controllers
                    if (channel->note != -1) {
                        ayumi_set_tone(synth->ayumi, index, note_to_period(synth, channel->note));
                        ayumi_set_volume(synth->ayumi, index, round(channel->volume * channel->velocity / 127.0));
                    }
                    break;
                case LV2_MIDI_CTL_ALL_SOUNDS_OFF: // All sounds off
                case LV2_MIDI_CTL_ALL_NOTES_OFF: // All notes off
                    if (channel->note == -1)
                        break; // not at note on state
                    ayumi_synth_set_volume(synth, index, 0, false);
                    channel->note = -1;
                    break;
                case LV2_MIDI_CTL_MSB_PAN: // Pan
                    {
                        const float pan = max(0, data[1] - 1) / 126.0;
                        ayumi_set_pan(synth->ayumi, index, pan, 1);
                    }
                    break;
                case LV2_MIDI_CTL_MSB_MAIN_VOLUME: // Amplitude
                    channel->volume = data[1] >> 3;
                    if (channel->note != -1) {
                        ayumi_set_volume(synth->ayumi, index, round(channel->volume * channel->velocity / 127.0));
                    }
                    break;
                case LV2_MIDI_CTL_MSB_GENERAL_PURPOSE1: // Noise period
                    ayumi_set_noise(synth->ayumi, data[1] >> 2);
                    break;
                case LV2_MIDI_CTL_MSB_GENERAL_PURPOSE2: // Env. period coarse
                    synth->envelope_period = (synth->envelope_period & 0x007F) | (data[1] << 7);
                    synth->envelope_period += 0.000000204 * exp(0.001599763 * synth->envelope_period);
                    ayumi_set_envelope(synth->ayumi, synth->envelope_period);
                    break;
                case LV2_MIDI_CTL_MSB_GENERAL_PURPOSE3: // Env. period fine
                    synth->envelope_period = (synth->envelope_period & 0xFF80) | data[1];
                    ayumi_set_envelope(synth->ayumi, synth->envelope_period);
                    break;
                case LV2_MIDI_CTL_MSB_GENERAL_PURPOSE4: // Env. shape
                    synth->envelope_shape = (data[1] >> 4) | 8;
                    ayumi_synth_set_envelope_shape(synth, synth->envelope_shape);
                    break;
            }
            break;
        default:
            break;
    }
}

static int note_to_period(AyumiSynth* synth, double note) {
    double freq = 220.0 * pow(1.059463, note - 45.0);
    return round(synth->clock / (16.0 * freq));
}

static void ayumi_synth_reset(AyumiSynth* synth) {
    synth->envelope_period = 0;
    synth->envelope_shape = 0;
    synth->noise_period = 0;
    ayumi_set_envelope(synth->ayumi, synth->envelope_period);
    ayumi_set_envelope_shape(synth->ayumi, synth->envelope_shape);
    ayumi_set_noise(synth->ayumi, synth->noise_period);
    for (int i = 0; i < 3; i++) {
        synth->channels[i].volume = 100 >> 3;
        synth->channels[i].envelope_on = 0;
        synth->channels[i].note = -1;
        ayumi_synth_set_volume(synth, i, 0, false);
        ayumi_set_mixer(synth->ayumi, i, 0, 1, 0);
        ayumi_set_pan(synth->ayumi, i, 0.5, 0);
    }
}
