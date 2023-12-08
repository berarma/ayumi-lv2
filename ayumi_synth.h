#include <stdint.h>

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
void ayumi_synth_set_remove_dc(AyumiSynth* synth, bool remove_dc);
int ayumi_synth_set_clock(AyumiSynth* synth, int clock);
void ayumi_synth_set_mode(AyumiSynth* synth, bool mode);
void ayumi_synth_set_noise_period(AyumiSynth* synth, int period);
void ayumi_synth_set_envelope_period(AyumiSynth* synth, int period);
void ayumi_synth_set_envelope_shape(AyumiSynth* synth, int period);
void ayumi_synth_set_envelope_hold(AyumiSynth* synth, bool hold);
void ayumi_synth_set_envelope_alternate(AyumiSynth* synth, bool alternate);
void ayumi_synth_set_envelope_attack(AyumiSynth* synth, bool attack);
void ayumi_synth_set_envelope_continue(AyumiSynth* synth, bool cont);
void ayumi_synth_set_tone_period(AyumiSynth* synth, int index, int period);
void ayumi_synth_set_volume(AyumiSynth* synth, int index, int volume, bool envelope);
void ayumi_synth_set_envelope(AyumiSynth* synth, int index, bool envelope);
void ayumi_synth_set_mixer(AyumiSynth* synth, int index, bool tone, bool noise);
void ayumi_synth_set_tone(AyumiSynth* synth, int index, bool tone);
void ayumi_synth_set_noise(AyumiSynth* synth, int index, bool noise);
