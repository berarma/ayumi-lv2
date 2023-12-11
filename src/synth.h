#include <stdint.h>

#define AYMODE 0
#define YMMODE 1

typedef struct ayumi Psg;

typedef struct {
    int volume;
    bool envelope_on;
    int note;
    int velocity;
    float bend;
    float modulation;
} SynthChannel;

typedef struct {
    Psg* psg;
    int clock;
    double sample_rate;
    bool remove_dc;
    int noise_period;
    int envelope_period;
    int envelope_shape;
    int32_t pitchbend;
    uint32_t counter;
    SynthChannel channels[3];
} Synth;

int synth_init(Synth* synth, double sample_rate);
void synth_close(Synth* synth);
void synth_process(Synth* synth, float* left, float *right);
void synth_midi(Synth* synth, uint8_t status, uint8_t data[]);
void synth_set_remove_dc(Synth* synth, bool remove_dc);
int synth_set_clock(Synth* synth, int clock);
void synth_set_mode(Synth* synth, bool mode);

