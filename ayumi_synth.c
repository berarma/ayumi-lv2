#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <lv2/midi/midi.h>
#include "ayumi.h"
#include "ayumi_synth.h"

#define DEFAULT_CLOCK 2e6 /* clock_rate / (sample_rate * 8 * 8) must be < 1.0 */

#define AYMODE 0
#define YMMODE 1

int note_to_period(AyumiSynth* synth, double note ) {
    double freq = 220.0 * pow(1.059463, note - 45.0);
    return synth->clock / (16.0 * freq);
}

void ayumi_synth_update(AyumiSynth* synth, int cn) {
    ayumi_set_noise(synth->ayumi, synth->noise_period);
    ayumi_set_envelope(synth->ayumi, synth->envelope);
    ayumi_set_envelope_shape(synth->ayumi, synth->envelope_shape);
    for (int i = 0; i < 3; i++) {
        if (cn == -1 || i == cn) {
            if (synth->channels[i].note_on_state) {
                ayumi_set_tone(synth->ayumi, i, synth->channels[i].period);
                ayumi_set_volume(synth->ayumi, i, synth->channels[i].volume);
            }
            ayumi_set_pan(synth->ayumi, i, synth->channels[i].pan, 0);
            ayumi_set_mixer(synth->ayumi, i, synth->channels[i].tone_off, synth->channels[i].noise_off, synth->channels[i].envelope_on);
        }
    }
}

int ayumi_synth_create(AyumiSynth* synth) {
    const bool ok = ayumi_configure(synth->ayumi, synth->mode, synth->clock, (int) synth->sample_rate);

    for (int i = 0; i < 3; i++) {
        if (synth->channels[i].note_on_state) {
            synth->channels[i].period = note_to_period(synth, synth->channels[i].note);
        }
    }

    ayumi_synth_update(synth, -1);

    return ok;
}

int ayumi_synth_init(AyumiSynth* synth, double sample_rate) {
    synth->mode = YMMODE;
    synth->clock = DEFAULT_CLOCK;
    synth->sample_rate = sample_rate;
    synth->remove_dc = false;
    synth->ayumi = calloc(sizeof(struct ayumi), 1);

    synth->envelope = 0;
    synth->envelope_shape = 0;
    synth->noise_period = 0;
    for (int i = 0; i < 3; i++) {
        synth->channels[i].period = 0;
        synth->channels[i].volume = 15;
        synth->channels[i].tone_off = 0;
        synth->channels[i].noise_off = 1;
        synth->channels[i].envelope_on = 0;
        synth->channels[i].pan = 0.5;
    }

    return ayumi_synth_create(synth);
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

inline void ayumi_synth_set_tone(AyumiSynth* synth, int index, bool tone) {
    synth->ayumi->channels[index].t_off = tone; // XXX Ayumi internals
}

inline void ayumi_synth_set_noise(AyumiSynth* synth, int index, bool noise) {
    synth->ayumi->channels[index].n_off = noise; // XXX Ayumi internals
}

void ayumi_synth_midi(AyumiSynth* synth, uint8_t status, uint8_t data[]) {
    AyumiSynthChannel* channel;

    int index = status & 0xF;
    if (index > 2)
        return;

    channel = &synth->channels[index];

    switch (lv2_midi_message_type(&status)) {
        case LV2_MIDI_MSG_NOTE_OFF: // Note off
            if (!channel->note_on_state || channel->note != data[0])
                break; // not at note on state
            ayumi_synth_set_volume(synth, index, 0, false);
            channel->note_on_state = false;
            break;
        case LV2_MIDI_MSG_NOTE_ON:
            ayumi_synth_set_volume(synth, index, (int)(channel->volume * data[1] / 127.0), channel->envelope_on);
            ayumi_set_envelope_shape(synth->ayumi, synth->envelope_shape); // This will restart the envelope
            channel->period = note_to_period(synth, data[0]);
            ayumi_set_tone(synth->ayumi, index, channel->period);
            channel->note_on_state = true;
            channel->note = data[0];
            break;
        case LV2_MIDI_MSG_BENDER:
            {
                const int pitch = data[0] + (data[1] << 7) - 0x2000;
                channel->period = note_to_period(synth, channel->note + (pitch / 8192.0) * 12);
                ayumi_set_tone(synth->ayumi, index, channel->period);
            }
            break;
        case LV2_MIDI_MSG_PGM_CHANGE: // Mixer
            channel->tone_off = data[0] & 1 ? 1 : 0;
            channel->noise_off = data[0] == 0 || data[0] == 3 ? 1 : 0;
            ayumi_set_mixer(synth->ayumi, index, channel->tone_off, channel->noise_off, channel->note_on_state ? channel->envelope_on : 0);
            break;
        case LV2_MIDI_MSG_CONTROLLER:
            switch (data[0]) {
                case LV2_MIDI_CTL_RESET_CONTROLLERS: // Reset controllers
                    channel->volume = 100 >> 3;
                    channel->pan = 0.5;
                    channel->envelope_on = false;
                    ayumi_synth_set_volume(synth, index, channel->volume, channel->envelope_on);
                    ayumi_set_pan(synth->ayumi, index, channel->pan, 0);
                    channel->period = note_to_period(synth, channel->note);
                    ayumi_set_tone(synth->ayumi, index, channel->period);
                    break;
                case LV2_MIDI_CTL_ALL_SOUNDS_OFF: // All sounds off
                case LV2_MIDI_CTL_ALL_NOTES_OFF: // All notes off
                    if (!channel->note_on_state)
                        break; // not at note on state
                    ayumi_synth_set_volume(synth, index, 0, false);
                    channel->note_on_state = false;
                    break;
                case LV2_MIDI_CTL_MSB_PAN: // Pan
                    if (data[2] < 64) {
                        channel->pan = data[2] / 128.0;
                    } else {
                        channel->pan = (data[2] - 64) / 127.0 + 0.5;
                    }
                    ayumi_set_pan(synth->ayumi, index, channel->pan, 0);
                    break;
                case LV2_MIDI_CTL_MSB_MAIN_VOLUME: // Amplitude
                    channel->volume = data[2] >> 3;
                    if (channel->note_on_state) {
                        ayumi_set_volume(synth->ayumi, index, channel->volume);
                    }
                    break;
                case LV2_MIDI_CTL_SC1_SOUND_VARIATION: // Env. on/off
                    channel->envelope_on = data[2] >= 64;
                    if (channel->note_on_state) {
                        ayumi_synth_set_envelope(synth, index, channel->envelope_on);
                    }
                    break;
                case LV2_MIDI_CTL_SC2_TIMBRE: // Env. Hold
                    synth->envelope_shape = (synth->envelope_shape & 0xE) | (data[2] >= 64 ? 1 : 0);
                    ayumi_set_envelope_shape(synth->ayumi, synth->envelope_shape);
                    break;
                case LV2_MIDI_CTL_SC3_RELEASE_TIME: // Env. Alternate
                    synth->envelope_shape = (synth->envelope_shape & 0xD) | (data[2] >= 64 ? 2 : 0);
                    ayumi_set_envelope_shape(synth->ayumi, synth->envelope_shape);
                    break;
                case LV2_MIDI_CTL_SC4_ATTACK_TIME: // Env. Attack
                    synth->envelope_shape = (synth->envelope_shape & 0xB) | (data[2] >= 64 ? 4 : 0);
                    ayumi_set_envelope_shape(synth->ayumi, synth->envelope_shape);
                    break;
                case LV2_MIDI_CTL_SC5_BRIGHTNESS: // Env. Continue
                    synth->envelope_shape = (synth->envelope_shape & 0x7) | (data[2] >= 64 ? 8 : 0);
                    ayumi_set_envelope_shape(synth->ayumi, synth->envelope_shape);
                    break;
                case LV2_MIDI_CTL_SC6: // Env. period coarse
                    synth->envelope = (synth->envelope & 0x01FF) | (data[2] << 9);
                    ayumi_set_envelope(synth->ayumi, synth->envelope);
                    break;
                case LV2_MIDI_CTL_SC7: // Env. period fine
                    synth->envelope = (synth->envelope & 0xFE03) | (data[2] << 2);
                    ayumi_set_envelope(synth->ayumi, synth->envelope);
                    break;
                case LV2_MIDI_CTL_SC8: // Env. period extra-fine
                    synth->envelope = (synth->envelope & 0xFFFC) | (data[2] >> 5);
                    ayumi_set_envelope(synth->ayumi, synth->envelope);
                    break;
                case LV2_MIDI_CTL_SC9: // Noise period
                    synth->noise_period = data[2] >> 2;
                    ayumi_set_noise(synth->ayumi, synth->noise_period);
                    break;
            }
            break;
        default:
            break;
    }
}
