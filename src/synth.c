#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <lv2/midi/midi.h>
#include "synth.h"
#include "ayumi.h"

#define DEFAULT_CLOCK 2e6

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

extern const double AY_dac_table[];
extern const double YM_dac_table[];

static int note_to_period(Synth* synth, double note);
static void synth_update(Synth* synth, int cn);
static void synth_reset(Synth* synth);
static void synth_update_tone(Synth* synth, int index);
static void synth_set_noise_period(Synth* synth, int period);
static void synth_set_envelope_period(Synth* synth, int period);
static void synth_set_envelope_shape(Synth* synth, int shape);
static void synth_set_tone_period(Synth* synth, int index, int period);
static void synth_set_envelope(Synth* synth, int index, bool envelope);
static void synth_set_volume(Synth* synth, int index, int volume, bool envelope);
static void synth_set_pan(Synth* synth, int index, float pan, int is_eqp);
static void synth_set_tone(Synth* synth, int index, bool tone);
static void synth_set_noise(Synth* synth, int index, bool noise);
static void synth_set_mixer(Synth* synth, int index, bool tone, bool noise);

int synth_init(Synth* synth, double sample_rate) {
    synth->psg = calloc(sizeof(Psg), 1);
    synth->clock = DEFAULT_CLOCK;
    synth->sample_rate = sample_rate;

    const bool ok = ayumi_configure(synth->psg, YMMODE, synth->clock, sample_rate);
    synth_reset(synth);

    return ok;
}

void synth_process(Synth* synth, float* left, float *right) {
    synth->counter++;
    for (int index = 0; index < 3; index++) {
        if (synth->channels[index].modulation) {
            synth_update_tone(synth, index);
        }
    }
    ayumi_process(synth->psg);
    if (synth->remove_dc) {
        ayumi_remove_dc(synth->psg);
    }
    *left = (float) synth->psg->left;
    *right = (float) synth->psg->right;
}

void synth_close(Synth* synth) {
    free(synth);
}

void synth_set_remove_dc(Synth* synth, bool remove_dc) {
    synth->remove_dc = remove_dc;
}

int synth_set_clock(Synth* synth, int clock) {
    synth->clock = clock;
    synth->psg->step = clock / (synth->sample_rate * 8 * DECIMATE_FACTOR); // XXX Ayumi internals
    return synth->psg->step < 1;
}

void synth_set_mode(Synth* synth, bool mode) {
    synth->psg->dac_table = mode ? YM_dac_table : AY_dac_table; // XXX Ayumi internals
}

void synth_midi(Synth* synth, uint8_t status, uint8_t data[]) {
    SynthChannel* channel;

    int index = status & 0xF;
    if (index > 2)
        return;

    channel = &synth->channels[index];

    switch (lv2_midi_message_type(&status)) {
        case LV2_MIDI_MSG_NOTE_OFF: // Note off
            if (channel->note != data[0])
                break; // not at note on state
            synth_set_volume(synth, index, 0, false);
            channel->note = -1;
            break;
        case LV2_MIDI_MSG_NOTE_ON:
            channel->note = data[0];
            channel->velocity = data[1];
            synth_set_volume(synth, index, round(channel->volume * data[1] / 127.0), channel->envelope_on);
            synth_set_envelope_shape(synth, synth->envelope_shape); // This will restart the envelope
            synth_update_tone(synth, index);
            break;
        case LV2_MIDI_MSG_BENDER:
            {
                const int pitch = data[0] + (data[1] << 7) - 0x2000;
                channel->bend = (pitch / 8192.0) * 12;
                synth_update_tone(synth, index);
            }
            break;
        case LV2_MIDI_MSG_PGM_CHANGE: // Mixer
            {
                bool tone = data[0] & 1 ? 0 : 1;
                bool noise = !((data[0] & 3) == 0 || (data[0] & 3) == 3);
                channel->note = -1;
                channel->envelope_on = data[0] > 3;
                synth_set_volume(synth, index, 0, false);
                synth_set_mixer(synth, index, tone, noise);
            }
            break;
        case LV2_MIDI_MSG_RESET:
            synth_reset(synth);
            break;
        case LV2_MIDI_MSG_CONTROLLER:
            switch (data[0]) {
                case LV2_MIDI_CTL_RESET_CONTROLLERS: // Reset controllers
                    if (channel->note != -1) {
                        synth_set_tone_period(synth, index, note_to_period(synth, channel->note));
                        synth_set_volume(synth, index, round(channel->volume * channel->velocity / 127.0), channel->envelope_on);
                    }
                    break;
                case LV2_MIDI_CTL_ALL_SOUNDS_OFF: // All sounds off
                case LV2_MIDI_CTL_ALL_NOTES_OFF: // All notes off
                    if (channel->note == -1)
                        break; // not at note on state
                    synth_set_volume(synth, index, 0, false);
                    channel->note = -1;
                    break;
                case LV2_MIDI_CTL_MSB_MODWHEEL:
                    channel->modulation = data[1] / 127.0 * 0.5;
                    synth_update_tone(synth, index);
                    break;
                case LV2_MIDI_CTL_MSB_PAN: // Pan
                    {
                        const float pan = max(0, data[1] - 1) / 126.0;
                        synth_set_pan(synth, index, pan, 1);
                    }
                    break;
                case LV2_MIDI_CTL_MSB_MAIN_VOLUME: // Amplitude
                    channel->volume = data[1] >> 3;
                    if (channel->note != -1) {
                        synth_set_volume(synth, index, round(channel->volume * channel->velocity / 127.0), channel->envelope_on);
                    }
                    break;
                case LV2_MIDI_CTL_MSB_GENERAL_PURPOSE1: // Noise period
                    synth_set_noise_period(synth, data[1] >> 2);
                    break;
                case LV2_MIDI_CTL_MSB_GENERAL_PURPOSE2: // Env. period coarse
                    synth->envelope_period = (synth->envelope_period & 0x007F) | (data[1] << 7);
                    synth->envelope_period += 0.000000204 * exp(0.001599763 * synth->envelope_period);
                    synth_set_envelope_period(synth, synth->envelope_period);
                    break;
                case LV2_MIDI_CTL_MSB_GENERAL_PURPOSE3: // Env. period fine
                    synth->envelope_period = (synth->envelope_period & 0xFF80) | data[1];
                    synth_set_envelope_period(synth, synth->envelope_period);
                    break;
                case LV2_MIDI_CTL_MSB_GENERAL_PURPOSE4: // Env. shape
                    synth->envelope_shape = (data[1] >> 4) | 8;
                    synth_set_envelope_shape(synth, synth->envelope_shape);
                    break;
            }
            break;
        default:
            break;
    }
}

static int note_to_period(Synth* synth, double note) {
    double freq = 220.0 * pow(1.059463, note - 45.0);
    return round(synth->clock / (16.0 * freq));
}

static void synth_reset(Synth* synth) {
    synth->envelope_period = 0;
    synth->envelope_shape = 0;
    synth->noise_period = 0;
    synth_set_envelope_period(synth, synth->envelope_period);
    synth_set_envelope_shape(synth, synth->envelope_shape);
    synth_set_noise_period(synth, synth->noise_period);
    for (int i = 0; i < 3; i++) {
        synth->channels[i].volume = 100 >> 3;
        synth->channels[i].envelope_on = false;
        synth->channels[i].note = -1;
        synth->channels[i].modulation = 0;
        synth_set_volume(synth, i, 0, false);
        synth_set_mixer(synth, i, true, false);
        synth_set_pan(synth, i, 0.5, 0);
    }
}

static void synth_update_tone(Synth* synth, int index) {
    const SynthChannel* channel = &synth->channels[index];
    const float modulation = channel->modulation * sin(M_PI_2 * synth->counter / 2048.0);
    synth_set_tone_period(synth, index, note_to_period(synth, channel->note + channel->bend + modulation));
}

static void synth_set_noise_period(Synth* synth, int period) {
    ayumi_set_noise(synth->psg, period);
}

static void synth_set_envelope_period(Synth* synth, int period) {
    ayumi_set_envelope(synth->psg, period);
}

static void synth_set_envelope_shape(Synth* synth, int shape) {
    ayumi_set_envelope_shape(synth->psg, shape);
}

static void synth_set_tone_period(Synth* synth, int index, int period) {
    ayumi_set_tone(synth->psg, index, period);
}

static void synth_set_envelope(Synth* synth, int index, bool envelope) {
    synth->psg->channels[index].e_on = envelope; // XXX Ayumi internals
}

static void synth_set_volume(Synth* synth, int index, int volume, bool envelope) {
    ayumi_set_volume(synth->psg, index, volume);
    synth_set_envelope(synth, index, envelope);
}

static void synth_set_pan(Synth* synth, int index, float pan, int is_eqp) {
    ayumi_set_pan(synth->psg, index, pan, is_eqp);
}

static void synth_set_tone(Synth* synth, int index, bool tone) {
    synth->psg->channels[index].t_off = tone ? 0 : 1; // XXX Ayumi internals
}

static void synth_set_noise(Synth* synth, int index, bool noise) {
    synth->psg->channels[index].n_off = noise ? 0 : 1; // XXX Ayumi internals
}

static void synth_set_mixer(Synth* synth, int index, bool tone, bool noise) {
    synth_set_tone(synth, index, tone);
    synth_set_noise(synth, index, noise);
}
