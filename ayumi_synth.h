#include <stdint.h>
#include "ayumi.h"

extern const double AY_dac_table[];
extern const double YM_dac_table[];

typedef struct {
    int volume;
    bool envelope_on;
    int note;
    int velocity;
} AyumiSynthChannel;

typedef struct {
    struct ayumi* ayumi;
    int clock;
    double sample_rate;
    bool remove_dc;
    int mode;
    int noise_period;
    int envelope_period;
    int envelope_shape;
    int32_t pitchbend;
    AyumiSynthChannel channels[3];
} AyumiSynth;

int ayumi_synth_init(AyumiSynth* synth, double sample_rate);
void ayumi_synth_close(AyumiSynth* synth);
void ayumi_synth_process(AyumiSynth* synth, float* left, float *right);
void ayumi_synth_midi(AyumiSynth* synth, uint8_t status, uint8_t data[]);

static inline void ayumi_synth_set_remove_dc(AyumiSynth* synth, bool remove_dc) {
    synth->remove_dc = remove_dc;
}

static inline int ayumi_synth_set_clock(AyumiSynth* synth, int clock) {
    synth->ayumi->step = clock / (synth->sample_rate * 8 * DECIMATE_FACTOR); // XXX Ayumi internals
    return synth->ayumi->step < 1;
}

static inline void ayumi_synth_set_mode(AyumiSynth* synth, bool mode) {
    synth->ayumi->dac_table = mode ? YM_dac_table : AY_dac_table; // XXX Ayumi internals
}

static inline void ayumi_synth_set_noise_period(AyumiSynth* synth, int period) {
    ayumi_set_noise(synth->ayumi, period);
}

static inline void ayumi_synth_set_envelope_period(AyumiSynth* synth, int period) {
    ayumi_set_envelope(synth->ayumi, period);
}

static inline void ayumi_synth_set_envelope_shape(AyumiSynth* synth, int shape) {
    ayumi_set_envelope_shape(synth->ayumi, shape);
}

static inline void ayumi_synth_set_envelope_hold(AyumiSynth* synth, bool hold) {
    ayumi_set_envelope_shape(synth->ayumi, (synth->ayumi->envelope_shape & 0xE) | hold);
}

static inline void ayumi_synth_set_envelope_alternate(AyumiSynth* synth, bool alternate) {
    ayumi_set_envelope_shape(synth->ayumi, (synth->ayumi->envelope_shape & 0xD) | (alternate << 1));
}

static inline void ayumi_synth_set_envelope_attack(AyumiSynth* synth, bool attack) {
    ayumi_set_envelope_shape(synth->ayumi, (synth->ayumi->envelope_shape & 0xB) | (attack << 2));
}

static inline void ayumi_synth_set_envelope_continue(AyumiSynth* synth, bool cont) {
    ayumi_set_envelope_shape(synth->ayumi, (synth->ayumi->envelope_shape & 0x7) | (cont << 8));
}

static inline void ayumi_synth_set_tone_period(AyumiSynth* synth, int index, int period) {
    ayumi_set_tone(synth->ayumi, index, period);
}

static inline void ayumi_synth_set_envelope(AyumiSynth* synth, int index, bool envelope) {
    synth->ayumi->channels[index].e_on = envelope; // XXX Ayumi internals
}

static inline void ayumi_synth_set_volume(AyumiSynth* synth, int index, int volume, bool envelope) {
    ayumi_set_volume(synth->ayumi, index, volume);
    ayumi_synth_set_envelope(synth, index, envelope);
}

static inline void ayumi_synth_set_tone(AyumiSynth* synth, int index, bool tone) {
    synth->ayumi->channels[index].t_off = tone ? 0 : 1; // XXX Ayumi internals
}

static inline void ayumi_synth_set_noise(AyumiSynth* synth, int index, bool noise) {
    synth->ayumi->channels[index].n_off = noise ? 0 : 1; // XXX Ayumi internals
}

static inline void ayumi_synth_set_mixer(AyumiSynth* synth, int index, bool tone, bool noise) {
    ayumi_synth_set_tone(synth, index, tone);
    ayumi_synth_set_noise(synth, index, noise);
}
