#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include "ayumi_synth.h"

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
    LV2_URID_Map *urid_map;
    LV2_URID midi_event_uri;
    LV2_Log_Logger logger;
    URIs uris;
    float* ports[3];
    bool active;
    AyumiSynth* synth;
} AyumiLV2Handle;

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

    handle->synth = calloc(sizeof(AyumiSynth), 1);
    if (!ayumi_synth_init(handle->synth, sample_rate)) {
        lv2_log_warning(&handle->logger, "The sample rate is too low for the clock source.\n");
    }

    ayumi_synth_set_remove_dc(handle->synth, true);

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

void ayumi_lv2_process_midi_event(LV2_Handle instance, LV2_Atom_Event *ev) {
    AyumiLV2Handle* handle = (AyumiLV2Handle*) instance;
    uint8_t* msg = (uint8_t *)(ev + 1);
    ayumi_synth_midi(handle->synth, msg[0], msg + 1);
}

void ayumi_lv2_deactivate(LV2_Handle instance) {
    AyumiLV2Handle* handle = (AyumiLV2Handle*) instance;
    handle->active = false;
}

void ayumi_lv2_cleanup(LV2_Handle instance) {
    AyumiLV2Handle* handle = (AyumiLV2Handle*) instance;
    ayumi_synth_close(handle->synth);
    free(handle->synth);
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
        if (!ayumi_synth_set_clock(handle->synth, (int)(*((double*)(val + 1)) * 1e6))) {
            lv2_log_warning(&handle->logger, "The sample rate is too low for the clock source.\n");
        }
    } else if (key == handle->uris.m_param_ymmode) {
        ayumi_synth_set_mode(handle->synth, *((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_noise_period) {
        ayumi_synth_set_noise_period(handle->synth, *((int*)(val + 1)));
    } else if (key == handle->uris.m_param_envelope_period) {
        ayumi_synth_set_envelope_period(handle->synth, *((int*)(val + 1)));
    } else if (key == handle->uris.m_param_envelope_hold) {
        ayumi_synth_set_envelope_hold(handle->synth, *((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_envelope_alternate) {
        ayumi_synth_set_envelope_alternate(handle->synth, *((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_envelope_attack) {
        ayumi_synth_set_envelope_attack(handle->synth, *((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_envelope_continue) {
        ayumi_synth_set_envelope_continue(handle->synth, *((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_a_period) {
        ayumi_synth_set_tone_period(handle->synth, 0, *((int*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_b_period) {
        ayumi_synth_set_tone_period(handle->synth, 1, *((int*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_c_period) {
        ayumi_synth_set_tone_period(handle->synth, 2, *((int*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_a_level) {
        ayumi_synth_set_volume(handle->synth, 0, *((int*)(val + 1)), 0);
    } else if (key == handle->uris.m_param_channel_b_level) {
        ayumi_synth_set_volume(handle->synth, 1, *((int*)(val + 1)), 0);
    } else if (key == handle->uris.m_param_channel_c_level) {
        ayumi_synth_set_volume(handle->synth, 2, *((int*)(val + 1)), 0);
    } else if (key == handle->uris.m_param_channel_a_mode) {
        ayumi_synth_set_envelope(handle->synth, 0, *((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_b_mode) {
        ayumi_synth_set_envelope(handle->synth, 1, *((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_c_mode) {
        ayumi_synth_set_envelope(handle->synth, 2, *((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_a_tone) {
        ayumi_synth_set_tone(handle->synth, 0, !*((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_b_tone) {
        ayumi_synth_set_tone(handle->synth, 1, !*((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_c_tone) {
        ayumi_synth_set_tone(handle->synth, 2, !*((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_a_noise) {
        ayumi_synth_set_noise(handle->synth, 0, !*((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_b_noise) {
        ayumi_synth_set_noise(handle->synth, 1, !*((bool*)(val + 1)));
    } else if (key == handle->uris.m_param_channel_c_noise) {
        ayumi_synth_set_noise(handle->synth, 2, !*((bool*)(val + 1)));
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
                ayumi_synth_process(handle->synth, &handle->ports[AYUMI_LV2_AUDIO_OUT_LEFT][i], &handle->ports[AYUMI_LV2_AUDIO_OUT_RIGHT][i]);
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
        ayumi_synth_process(handle->synth, &handle->ports[AYUMI_LV2_AUDIO_OUT_LEFT][i], &handle->ports[AYUMI_LV2_AUDIO_OUT_RIGHT][i]);
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

