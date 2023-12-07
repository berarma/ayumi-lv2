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

#include "lv2/lv2plug.in/ns/ext/patch/patch.h"
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#define PLUG_URI "http://lv2plug.in/plugins/eg-params#"

#define AYUMI_LV2_URI "https://github.com/atsushieno/ayumi-lv2"
#define AYUMI_LV2_ATOM_INPUT_PORT 0
#define AYUMI_LV2_AUDIO_OUT_LEFT 1
#define AYUMI_LV2_AUDIO_OUT_RIGHT 2
#define AYMODE 0
#define YMMODE 1

#define DEFAULT_CLOCK 2e6 /* clock_rate / (sample_rate * 8 * 8) must be < 1.0 */

typedef struct {
	LV2_URID atom_Blank;
	LV2_URID atom_Object;
	LV2_URID atom_URID;
	LV2_URID atom_Float;
	LV2_URID atom_Bool;
	LV2_URID patch_Set;
	LV2_URID patch_property;
	LV2_URID patch_value;
	LV2_URID m_param_clock;
	LV2_URID m_param_ymmode;
	LV2_URID m_param_noise_period;
	LV2_URID m_param_envelope_period;
	LV2_URID m_param_envelope_hold;
	LV2_URID m_param_envelope_alternate;
	LV2_URID m_param_envelope_attack;
	LV2_URID m_param_envelope_continue;
	LV2_URID m_param_channel_a_period;
	LV2_URID m_param_channel_b_period;
	LV2_URID m_param_channel_c_period;
	LV2_URID m_param_channel_a_level;
	LV2_URID m_param_channel_b_level;
	LV2_URID m_param_channel_c_level;
	LV2_URID m_param_channel_a_mode;
	LV2_URID m_param_channel_b_mode;
	LV2_URID m_param_channel_c_mode;
	LV2_URID m_param_channel_a_tone;
	LV2_URID m_param_channel_b_tone;
	LV2_URID m_param_channel_c_tone;
	LV2_URID m_param_channel_a_noise;
	LV2_URID m_param_channel_b_noise;
	LV2_URID m_param_channel_c_noise;
} URIs;

static void map_uris (LV2_URID_Map* map, URIs* uris) {
	uris->atom_Blank       = map->map (map->handle, LV2_ATOM__Blank);
	uris->atom_Object      = map->map (map->handle, LV2_ATOM__Object);
	uris->atom_URID        = map->map (map->handle, LV2_ATOM__URID);
	uris->atom_Float       = map->map (map->handle, LV2_ATOM__Float);
	uris->atom_Bool        = map->map (map->handle, LV2_ATOM__Bool);
	uris->patch_Set        = map->map (map->handle, LV2_PATCH__Set);
	uris->patch_property   = map->map (map->handle, LV2_PATCH__property);
	uris->patch_value      = map->map (map->handle, LV2_PATCH__value);
	uris->m_param_clock    = map->map (map->handle, PLUG_URI "clock");
	uris->m_param_ymmode   = map->map (map->handle, PLUG_URI "ymmode");
	uris->m_param_noise_period   = map->map (map->handle, PLUG_URI "noise_period");
	uris->m_param_envelope_period   = map->map (map->handle, PLUG_URI "envelope_period");
	uris->m_param_envelope_hold   = map->map (map->handle, PLUG_URI "envelope_hold");
	uris->m_param_envelope_alternate   = map->map (map->handle, PLUG_URI "envelope_alternate");
	uris->m_param_envelope_attack   = map->map (map->handle, PLUG_URI "envelope_attack");
	uris->m_param_envelope_continue   = map->map (map->handle, PLUG_URI "envelope_continue");
	uris->m_param_channel_a_period   = map->map (map->handle, PLUG_URI "channel_a_period");
	uris->m_param_channel_b_period   = map->map (map->handle, PLUG_URI "channel_b_period");
	uris->m_param_channel_c_period   = map->map (map->handle, PLUG_URI "channel_c_period");
	uris->m_param_channel_a_level  = map->map (map->handle, PLUG_URI "channel_a_level");
	uris->m_param_channel_b_level  = map->map (map->handle, PLUG_URI "channel_b_level");
	uris->m_param_channel_c_level  = map->map (map->handle, PLUG_URI "channel_c_level");
	uris->m_param_channel_a_mode  = map->map (map->handle, PLUG_URI "channel_a_mode");
	uris->m_param_channel_b_mode  = map->map (map->handle, PLUG_URI "channel_b_mode");
	uris->m_param_channel_c_mode  = map->map (map->handle, PLUG_URI "channel_c_mode");
	uris->m_param_channel_a_tone = map->map (map->handle, PLUG_URI "channel_a_tone");
	uris->m_param_channel_b_tone = map->map (map->handle, PLUG_URI "channel_b_tone");
	uris->m_param_channel_c_tone = map->map (map->handle, PLUG_URI "channel_c_tone");
	uris->m_param_channel_a_noise = map->map (map->handle, PLUG_URI "channel_a_noise");
	uris->m_param_channel_b_noise = map->map (map->handle, PLUG_URI "channel_b_noise");
	uris->m_param_channel_c_noise = map->map (map->handle, PLUG_URI "channel_c_noise");
}

typedef struct {
	int period;
	int volume;
	int tone_off;
	int noise_off;
	int envelope_on;
	double pan;
	int note;
	bool note_on_state;
} AyumiLV2Channel;

typedef struct {
	LV2_URID_Map *urid_map;
	LV2_URID midi_event_uri;
	LV2_Log_Logger logger;
	URIs uris;
	struct ayumi* impl;
	int clock;
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

int note_to_period(AyumiLV2Handle* handle, int note ) {
	double freq = 220.0 * pow(1.059463, note - 45.0);

	return handle->clock / (16.0 * freq);
}

void update_ayumi(AyumiLV2Handle* handle, int cn) {
	ayumi_set_noise(handle->impl, handle->noise_period);
	ayumi_set_envelope(handle->impl, handle->envelope);
	ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
	for (int i = 0; i < 3; i++) {
		if (cn == -1 || i == cn) {
			if (handle->channels[i].note_on_state) {
				ayumi_set_tone(handle->impl, i, handle->channels[i].period);
				ayumi_set_volume(handle->impl, i, handle->channels[i].volume);
			}
			ayumi_set_pan(handle->impl, i, handle->channels[i].pan, 0);
			ayumi_set_mixer(handle->impl, i, handle->channels[i].tone_off, handle->channels[i].noise_off, handle->channels[i].envelope_on);
		}
	}
}

void init_ayumi(AyumiLV2Handle* handle) {
	const bool ok = ayumi_configure(handle->impl, handle->mode, handle->clock, (int) handle->sample_rate);

	if (!ok) {
		lv2_log_warning(&handle->logger, "The sample rate is too low for the clock source.\n");
	}

	for (int i = 0; i < 3; i++) {
		if (handle->channels[i].note_on_state) {
			handle->channels[i].period = note_to_period(handle, handle->channels[i].note);
		}
	}
	update_ayumi(handle, -1);
}

LV2_Handle ayumi_lv2_instantiate(
		const LV2_Descriptor * descriptor,
		double sample_rate,
		const char * bundle_path,
		const LV2_Feature *const * features)
{
	LV2_Log_Log* log;

	AyumiLV2Handle* handle = (AyumiLV2Handle*) calloc(sizeof(AyumiLV2Handle), 1);

	handle->urid_map = NULL;
	for (int i = 0; features[i]; i++) {
		const LV2_Feature* f = features[i];
		if (!strcmp(f->URI, LV2_URID__map)) {
			handle->urid_map = (LV2_URID_Map*) f->data;
		} else if (!strcmp (features[i]->URI, LV2_LOG__log)) {
			log = (LV2_Log_Log*)features[i]->data;
		}
	}
	assert(handle->urid_map);
	handle->midi_event_uri = handle->urid_map->map(handle->urid_map->handle, LV2_MIDI__MidiEvent);

	/* Initialise logger (if map is unavailable, will fallback to printf) */
	lv2_log_logger_init (&handle->logger, handle->urid_map, log);

	if (!handle->urid_map) {
		lv2_log_error (&handle->logger, "PropEx.lv2: Host does not support urid:map\n");
		free (handle);
		return NULL;
	}

	map_uris (handle->urid_map, &handle->uris);

	handle->active = false;
	handle->mode = YMMODE;
	handle->clock = DEFAULT_CLOCK;
	handle->sample_rate = sample_rate;
	handle->impl = calloc(sizeof(struct ayumi), 1);

	handle->envelope = 0;
	handle->envelope_shape = 0;
	handle->noise_period = 0;
	for (int i = 0; i < 3; i++) {
		handle->channels[i].period = 0;
		handle->channels[i].volume = 15;
		handle->channels[i].tone_off = 0;
		handle->channels[i].noise_off = 1;
		handle->channels[i].envelope_on = 0;
		handle->channels[i].pan = 0.5;
	}

	init_ayumi(handle);

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
		ayumi_set_envelope_shape(handle->impl, handle->envelope_shape); // This will restart the envelope
		channel->period = note_to_period(handle, msg[1]);
		ayumi_set_tone(handle->impl, cn, channel->period);
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
		case LV2_MIDI_CTL_RESET_CONTROLLERS: // Reset controllers
			handle->envelope = 0;
			handle->envelope_shape = 0;
			handle->noise_period = 0;
			channel->volume = 0;
			channel->pan = 0.5;
			channel->envelope_on = false;
			update_ayumi(handle, cn);
			break;
		case LV2_MIDI_CTL_ALL_SOUNDS_OFF: // All sounds off
		case LV2_MIDI_CTL_ALL_NOTES_OFF: // All notes off
			if (!channel->note_on_state)
				break; // not at note on state
			ayumi_set_volume(handle->impl, cn, 0);
			ayumi_set_mixer(handle->impl, cn, channel->tone_off, channel->noise_off, 0);
			channel->note_on_state = false;
			break;
		case LV2_MIDI_CTL_MSB_PAN: // Pan
			if (msg[2] < 64) {
				channel->pan = msg[2] / 128.0;
			} else {
				channel->pan = (msg[2] - 64) / 127.0 + 0.5;
			}
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
	case LV2_MIDI_MSG_BENDER: handle->pitchbend = (msg[1] << 7) + msg[2];
		break;
	default:
		break;
	}
}

void ayumi_lv2_deactivate(LV2_Handle instance) {
	AyumiLV2Handle* handle = (AyumiLV2Handle*) instance;
	handle->active = false;
}

void ayumi_lv2_cleanup(LV2_Handle instance) {
	AyumiLV2Handle* handle = (AyumiLV2Handle*) instance;
	free(handle->impl);
	free(handle);
}

const void * ayumi_lv2_extension_data(const char * uri) {
	return NULL;
}

static inline bool parse_property (AyumiLV2Handle* handle, const LV2_Atom_Object* obj) {
	const LV2_Atom* property = NULL;
	const LV2_Atom* val = NULL;

	lv2_atom_object_get (obj, handle->uris.patch_property, &property, 0);

	if (!property || property->type != handle->uris.atom_URID) {
		return false;
	}

	lv2_atom_object_get (obj, handle->uris.patch_value, &val, 0);
	const uint32_t key = ((const LV2_Atom_URID*)property)->body;

	if (key == handle->uris.m_param_clock) {
		handle->clock = (int)(*((double*)(val + 1)) * 1e6);
		lv2_log_error (&handle->logger, "parse_property %d\n", handle->clock);
		init_ayumi(handle);
	} else if (key == handle->uris.m_param_ymmode) {
		handle->mode = *((bool*)(val + 1));
		init_ayumi(handle);
	} else if (key == handle->uris.m_param_noise_period) {
		handle->noise_period = *((int*)(val + 1));
		ayumi_set_noise(handle->impl, handle->noise_period);
	} else if (key == handle->uris.m_param_envelope_period) {
		handle->envelope = *((int*)(val + 1));
		ayumi_set_envelope(handle->impl, handle->envelope);
	} else if (key == handle->uris.m_param_envelope_hold) {
		handle->envelope_shape = handle->envelope_shape & 0xE | *((bool*)(val + 1));
		ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
	} else if (key == handle->uris.m_param_envelope_alternate) {
		handle->envelope_shape = handle->envelope_shape & 0xD | (*((bool*)(val + 1)) << 1);
		ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
	} else if (key == handle->uris.m_param_envelope_attack) {
		handle->envelope_shape = handle->envelope_shape & 0xB | (*((bool*)(val + 1)) << 2);
		ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
	} else if (key == handle->uris.m_param_envelope_continue) {
		handle->envelope_shape = handle->envelope_shape & 0x7 | (*((bool*)(val + 1)) << 3);
		ayumi_set_envelope_shape(handle->impl, handle->envelope_shape);
	} else if (key == handle->uris.m_param_channel_a_period) {
		handle->channels[0].period = *((int*)(val + 1));
		ayumi_set_tone(handle->impl, 0,  handle->channels[0].period);
	} else if (key == handle->uris.m_param_channel_b_period) {
		handle->channels[1].period = *((int*)(val + 1));
		ayumi_set_tone(handle->impl, 1,  handle->channels[1].period);
	} else if (key == handle->uris.m_param_channel_c_period) {
		handle->channels[2].period = *((int*)(val + 1));
		ayumi_set_tone(handle->impl, 2,  handle->channels[2].period);
	} else if (key == handle->uris.m_param_channel_a_level) {
		handle->channels[0].volume = *((int*)(val + 1));
		ayumi_set_volume(handle->impl, 0, handle->channels[0].volume);
	} else if (key == handle->uris.m_param_channel_b_level) {
		handle->channels[1].volume = *((int*)(val + 1));
		ayumi_set_volume(handle->impl, 1, handle->channels[1].volume);
	} else if (key == handle->uris.m_param_channel_c_level) {
		handle->channels[2].volume = *((int*)(val + 1));
		ayumi_set_volume(handle->impl, 2, handle->channels[2].volume);
	} else if (key == handle->uris.m_param_channel_a_mode) {
		handle->channels[0].envelope_on = *((bool*)(val + 1));
		ayumi_set_mixer(handle->impl, 0, handle->channels[0].tone_off, handle->channels[0].noise_off, handle->channels[0].envelope_on);
	} else if (key == handle->uris.m_param_channel_b_mode) {
		handle->channels[1].envelope_on = *((bool*)(val + 1));
		ayumi_set_mixer(handle->impl, 1, handle->channels[1].tone_off, handle->channels[1].noise_off, handle->channels[1].envelope_on);
	} else if (key == handle->uris.m_param_channel_c_mode) {
		handle->channels[2].envelope_on = *((bool*)(val + 1));
		ayumi_set_mixer(handle->impl, 2, handle->channels[2].tone_off, handle->channels[2].noise_off, handle->channels[2].envelope_on);
	} else if (key == handle->uris.m_param_channel_a_tone) {
		handle->channels[0].tone_off = !*((bool*)(val + 1));
		ayumi_set_mixer(handle->impl, 0, handle->channels[0].tone_off, handle->channels[0].noise_off, handle->channels[0].envelope_on);
	} else if (key == handle->uris.m_param_channel_b_tone) {
		handle->channels[1].tone_off = !*((bool*)(val + 1));
		ayumi_set_mixer(handle->impl, 1, handle->channels[1].tone_off, handle->channels[1].noise_off, handle->channels[1].envelope_on);
	} else if (key == handle->uris.m_param_channel_c_tone) {
		handle->channels[2].tone_off = !*((bool*)(val + 1));
		ayumi_set_mixer(handle->impl, 2, handle->channels[2].tone_off, handle->channels[2].noise_off, handle->channels[2].envelope_on);
	} else if (key == handle->uris.m_param_channel_a_noise) {
		handle->channels[0].noise_off = !*((bool*)(val + 1));
		ayumi_set_mixer(handle->impl, 0, handle->channels[0].tone_off, handle->channels[0].noise_off, handle->channels[0].envelope_on);
	} else if (key == handle->uris.m_param_channel_b_noise) {
		handle->channels[1].noise_off = !*((bool*)(val + 1));
		ayumi_set_mixer(handle->impl, 1, handle->channels[1].tone_off, handle->channels[1].noise_off, handle->channels[1].envelope_on);
	} else if (key == handle->uris.m_param_channel_c_noise) {
		handle->channels[2].noise_off = !*((bool*)(val + 1));
		ayumi_set_mixer(handle->impl, 2, handle->channels[2].tone_off, handle->channels[2].noise_off, handle->channels[2].envelope_on);
	}

	return true;
}

void ayumi_lv2_run(LV2_Handle instance, uint32_t sample_count) {
	AyumiLV2Handle* handle = (AyumiLV2Handle*) instance;
	if (!handle->active)
		return;

	LV2_Atom_Sequence* seq = (LV2_Atom_Sequence*) handle->ports[AYUMI_LV2_ATOM_INPUT_PORT];

	int currentFrame = 0;

	LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
		if (ev->time.frames != 0) {
			int max = currentFrame + ev->time.frames;
			max = max < sample_count ? max : sample_count;
			for (int i = currentFrame; i < max; i++) {
				ayumi_process(handle->impl);
				ayumi_remove_dc(handle->impl);
				handle->ports[AYUMI_LV2_AUDIO_OUT_LEFT][i] = (float) handle->impl->left;
				handle->ports[AYUMI_LV2_AUDIO_OUT_RIGHT][i] = (float) handle->impl->right;
			}
			currentFrame = max;
		}
		if (ev->body.type == handle->midi_event_uri) {
			ayumi_lv2_process_midi_event(handle, ev);
		}
		if (ev->body.type == handle->uris.atom_Object) {
			const LV2_Atom_Object* obj = (LV2_Atom_Object*)&ev->body;
			if (obj->body.otype == handle->uris.patch_Set) {
				/* then apply the change and continue */
				parse_property (handle, obj);
			}
		}
	}

	for (int i = currentFrame; i < sample_count; i++) {
		ayumi_process(handle->impl);
		ayumi_remove_dc(handle->impl);
		handle->ports[AYUMI_LV2_AUDIO_OUT_LEFT][i] = (float) handle->impl->left;
		handle->ports[AYUMI_LV2_AUDIO_OUT_RIGHT][i] = (float) handle->impl->right;
	}
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

