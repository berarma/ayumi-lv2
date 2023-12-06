#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include "ayumi.h"

#define AYUMI_LV2_URI "https://github.com/atsushieno/ayumi-lv2"
#define AYUMI_LV2_ATOM_INPUT_PORT 0
#define AYUMI_LV2_AUDIO_OUT_LEFT 1
#define AYUMI_LV2_AUDIO_OUT_RIGHT 2
#define AYMODE 0
#define YMMODE 1

#define CLOCK_FREQ 2e6 /* clock_rate / (sample_rate * 8 * 8) must be < 1.0 */

typedef struct {
	int tone_off;
	int noise_off;
	int envelope_on;
	int volume;
	int pan;
	int note;
	bool note_on_state;
} AyumiLV2Channel;

typedef struct {
	LV2_URID_Map *urid_map;
	LV2_URID midi_event_uri;
	struct ayumi* impl;
	float* ports[3];
	double sample_rate;
	bool active;
	int mode;
	int noise_period;
	int32_t envelope;
	int envelope_shape;
	int32_t pitchbend;
	AyumiLV2Channel channels[3];
} AyumiLV2Handle;

void init_ayumi(AyumiLV2Handle* handle) {
	ayumi_configure(handle->impl, handle->mode, CLOCK_FREQ, (int) handle->sample_rate);

	ayumi_set_noise(handle->impl, handle->noise_period);
	ayumi_set_envelope(handle->impl, handle->envelope);
	ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
	for (int i = 0; i < 3; i++) {
		handle->channels[i].note_on_state = false;
		ayumi_set_pan(handle->impl, i, handle->channels[i].pan, 0);
		ayumi_set_mixer(handle->impl, i, handle->channels[i].tone_off, handle->channels[i].noise_off, handle->channels[i].envelope_on);
		ayumi_set_volume(handle->impl, i, handle->channels[i].volume);
	}
}

LV2_Handle ayumi_lv2_instantiate(
		const LV2_Descriptor * descriptor,
		double sample_rate,
		const char * bundle_path,
		const LV2_Feature *const * features) {
	AyumiLV2Handle* handle = (AyumiLV2Handle*) calloc(sizeof(AyumiLV2Handle), 1);
	handle->active = false;
	handle->mode = YMMODE;
	handle->sample_rate = sample_rate;
	handle->impl = calloc(sizeof(struct ayumi), 1);

	handle->envelope = 0;
	handle->envelope_shape = 0;
	handle->noise_period = 0;
	for (int i = 0; i < 3; i++) {
		handle->channels[i].volume = 15;
		handle->channels[i].tone_off = 0;
		handle->channels[i].noise_off = 1;
		handle->channels[i].envelope_on = 0;
	}

	init_ayumi(handle);

	handle->urid_map = NULL;
	for (int i = 0; features[i]; i++) {
		const LV2_Feature* f = features[i];
		if (!strcmp(f->URI, LV2_URID__map))
			handle->urid_map = (LV2_URID_Map*) f->data;
	}
	assert(handle->urid_map);
	handle->midi_event_uri = handle->urid_map->map(handle->urid_map->handle, LV2_MIDI__MidiEvent);

	return handle;
}

void ayumi_lv2_connect_port(
		LV2_Handle instance,
		uint32_t port,
		void * data_location) {
	AyumiLV2Handle* handle = (AyumiLV2Handle*) instance;
	handle->ports[port] = data_location;
}

void ayumi_lv2_activate(LV2_Handle instance) {
	AyumiLV2Handle* handle = (AyumiLV2Handle*) instance;
	handle->active = true;
}

double key_to_freq(double key) {
	// We use equal temperament
	// https://pages.mtu.edu/~suits/NoteFreqCalcs.html
	double ret = 220.0 * pow(1.059463, key - 45.0);
	return ret;
}

void ayumi_lv2_process_midi_event(AyumiLV2Handle *handle, LV2_Atom_Event *ev) {
	uint8_t* msg = (uint8_t *)(ev + 1);
	AyumiLV2Channel* channel;
	int cn = msg[0] & 0xF;
	if (cn > 2)
		return;

	channel = &handle->channels[cn];

	switch (lv2_midi_message_type(msg)) {
	case LV2_MIDI_MSG_NOTE_OFF: // Note off
		if (!channel->note_on_state || channel->note != msg[1])
			break; // not at note on state
		ayumi_set_volume(handle->impl, cn, 0);
		ayumi_set_mixer(handle->impl, cn, channel->tone_off, channel->noise_off, 0);
		channel->note_on_state = false;
		break;
	case LV2_MIDI_MSG_NOTE_ON:
		ayumi_set_volume(handle->impl, cn, channel->volume);
		ayumi_set_mixer(handle->impl, cn, channel->tone_off, channel->noise_off, channel->envelope_on);
		ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
		ayumi_set_tone(handle->impl, cn, CLOCK_FREQ / (16.0 * key_to_freq(msg[1])));
		channel->note_on_state = true;
		channel->note = msg[1];
		break;
	case LV2_MIDI_MSG_PGM_CHANGE: // Mixer
		channel->tone_off = msg[1] & 1 ? 1 : 0;
		channel->noise_off = msg[1] == 0 || msg[1] == 3 ? 1 : 0;
		ayumi_set_mixer(handle->impl, cn, channel->tone_off, channel->noise_off, channel->note_on_state ? channel->envelope_on : 0);
		break;
	case LV2_MIDI_MSG_CONTROLLER:
		switch (msg[1]) {
		case LV2_MIDI_CTL_ALL_NOTES_OFF: // All notes off
			if (!channel->note_on_state)
				break; // not at note on state
			ayumi_set_volume(handle->impl, cn, 0);
			ayumi_set_mixer(handle->impl, cn, channel->tone_off, channel->noise_off, 0);
			channel->note_on_state = false;
			break;
		case LV2_MIDI_CTL_MSB_PAN: // Pan
			channel->pan = msg[2] / 127.0;
			ayumi_set_pan(handle->impl, cn, channel->pan, 0);
			break;
		case LV2_MIDI_CTL_MSB_MAIN_VOLUME: // Amplitude
			channel->volume = msg[2] >> 3;
			if (channel->note_on_state) {
				ayumi_set_volume(handle->impl, cn, channel->volume);
			}
			break;
		case LV2_MIDI_CTL_SC1_SOUND_VARIATION: // Env. on/off
			channel->envelope_on = msg[2] >= 64;
			if (channel->note_on_state) {
				ayumi_set_mixer(handle->impl, cn, channel->tone_off, channel->noise_off, channel->envelope_on);
			}
			break;
		case LV2_MIDI_CTL_SC2_TIMBRE: // Env. Hold
			handle->envelope_shape = (handle->envelope_shape & 0xE) | (msg[2] >= 64 ? 1 : 0);
			ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
			break;
		case LV2_MIDI_CTL_SC3_RELEASE_TIME: // Env. Alternate
			handle->envelope_shape = (handle->envelope_shape & 0xD) | (msg[2] >= 64 ? 2 : 0);
			ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
			break;
		case LV2_MIDI_CTL_SC4_ATTACK_TIME: // Env. Attack
			handle->envelope_shape = (handle->envelope_shape & 0xB) | (msg[2] >= 64 ? 4 : 0);
			ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
			break;
		case LV2_MIDI_CTL_SC5_BRIGHTNESS: // Env. Continue
			handle->envelope_shape = (handle->envelope_shape & 0x7) | (msg[2] >= 64 ? 8 : 0);
			ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
			break;
		case LV2_MIDI_CTL_SC6: // Env. period coarse
			handle->envelope = (handle->envelope & 0x01FF) | (msg[2] << 9);
			ayumi_set_envelope(handle->impl, handle->envelope);
			break;
		case LV2_MIDI_CTL_SC7: // Env. period fine
			handle->envelope = (handle->envelope & 0xFE03) | (msg[2] << 2);
			ayumi_set_envelope(handle->impl, handle->envelope);
			break;
		case LV2_MIDI_CTL_SC8: // Env. period extra-fine
			handle->envelope = (handle->envelope & 0xFFFC) | (msg[2] >> 5);
			ayumi_set_envelope(handle->impl, handle->envelope);
			break;
		case LV2_MIDI_CTL_SC9: // Noise period
			handle->noise_period = msg[2] >> 2;
			ayumi_set_noise(handle->impl, handle->noise_period);
			break;
		case LV2_MIDI_CTL_SC10: // AY/YM mode
			{
				int mode = msg[2] >= 64 ? 1 : 0;
				if (mode != handle->mode) {
					handle->mode = mode;
					init_ayumi(handle);
				}
			}
			break;
		}
		break;
	case LV2_MIDI_MSG_BENDER:
		handle->pitchbend = (msg[1] << 7) + msg[2];
		break;
	default:
		break;
	}
}

void ayumi_lv2_run(LV2_Handle instance, uint32_t sample_count) {
	AyumiLV2Handle* a = (AyumiLV2Handle*) instance;
	if (!a->active)
		return;

	LV2_Atom_Sequence* seq = (LV2_Atom_Sequence*) a->ports[AYUMI_LV2_ATOM_INPUT_PORT];

	int currentFrame = 0;

	LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
		if (ev->time.frames != 0) {
			int max = currentFrame + ev->time.frames;
			max = max < sample_count ? max : sample_count;
			for (int i = currentFrame; i < max; i++) {
				ayumi_process(a->impl);
				ayumi_remove_dc(a->impl);
				a->ports[AYUMI_LV2_AUDIO_OUT_LEFT][i] = (float) a->impl->left;
				a->ports[AYUMI_LV2_AUDIO_OUT_RIGHT][i] = (float) a->impl->right;
			}
			currentFrame = max;
		}
		if (ev->body.type == a->midi_event_uri) {
			ayumi_lv2_process_midi_event(a, ev);
		}
	}

	for (int i = currentFrame; i < sample_count; i++) {
		ayumi_process(a->impl);
		ayumi_remove_dc(a->impl);
		a->ports[AYUMI_LV2_AUDIO_OUT_LEFT][i] = (float) a->impl->left;
		a->ports[AYUMI_LV2_AUDIO_OUT_RIGHT][i] = (float) a->impl->right;
	}
}

void ayumi_lv2_deactivate(LV2_Handle instance) {
	AyumiLV2Handle* a = (AyumiLV2Handle*) instance;
	a->active = false;
}

void ayumi_lv2_cleanup(LV2_Handle instance) {
	AyumiLV2Handle* a = (AyumiLV2Handle*) instance;
	free(a->impl);
	free(a);
}

const void * ayumi_lv2_extension_data(const char * uri) {
	return NULL;
}

const LV2_Descriptor ayumi_lv2 = {
	AYUMI_LV2_URI,
	ayumi_lv2_instantiate,
	ayumi_lv2_connect_port,
	ayumi_lv2_activate,
	ayumi_lv2_run,
	ayumi_lv2_deactivate,
	ayumi_lv2_cleanup,
	ayumi_lv2_extension_data
};

const LV2_Descriptor * lv2_descriptor(uint32_t index)
{
	return &ayumi_lv2;
}

