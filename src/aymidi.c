#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include "synth.h"

#include "lv2/lv2plug.in/ns/ext/patch/patch.h"
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#define PLUG_URI "http://lv2plug.in/plugins/eg-params#"

#define AYMIDI_URI "https://github.com/berarma/aymidi"
#define AYMIDI_ATOM_INPUT_PORT 0
#define AYMIDI_AUDIO_OUT_LEFT 1
#define AYMIDI_AUDIO_OUT_RIGHT 2

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
}

typedef struct {
    LV2_URID_Map *urid_map;
    LV2_URID midi_event_uri;
    LV2_Log_Logger logger;
    URIs uris;
    float* ports[3];
    bool active;
    Synth* synth;
} AyMidiHandle;

LV2_Handle aymidi_instantiate(
        const LV2_Descriptor * descriptor,
        double sample_rate,
        const char * bundle_path,
        const LV2_Feature *const * features)
{
    LV2_Log_Log* log;

    AyMidiHandle* handle = (AyMidiHandle*) calloc(sizeof(AyMidiHandle), 1);

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

    handle->synth = calloc(sizeof(Synth), 1);
    if (!synth_init(handle->synth, sample_rate)) {
        lv2_log_warning(&handle->logger, "The sample rate is too low for the clock source.\n");
    }

    synth_set_remove_dc(handle->synth, true);

    return handle;
}

void aymidi_connect_port(
        LV2_Handle instance,
        uint32_t port,
        void * data_location) {
    AyMidiHandle* handle = (AyMidiHandle*) instance;
    handle->ports[port] = data_location;
}

void aymidi_activate(LV2_Handle instance) {
    AyMidiHandle* handle = (AyMidiHandle*) instance;
    handle->active = true;
}

void aymidi_process_midi_event(LV2_Handle instance, LV2_Atom_Event *ev) {
    AyMidiHandle* handle = (AyMidiHandle*) instance;
    uint8_t* msg = (uint8_t *)(ev + 1);
    synth_midi(handle->synth, msg[0], msg + 1);
}

void aymidi_deactivate(LV2_Handle instance) {
    AyMidiHandle* handle = (AyMidiHandle*) instance;
    handle->active = false;
}

void aymidi_cleanup(LV2_Handle instance) {
    AyMidiHandle* handle = (AyMidiHandle*) instance;
    synth_close(handle->synth);
    free(handle->synth);
    free(handle);
}

const void * aymidi_extension_data(const char * uri) {
    return NULL;
}

bool parse_property(AyMidiHandle* handle, const LV2_Atom_Object* obj) {
    const LV2_Atom* property = NULL;
    const LV2_Atom* val = NULL;

    lv2_atom_object_get (obj, handle->uris.patch_property, &property, 0);

    if (!property || property->type != handle->uris.atom_URID) {
        return false;
    }

    lv2_atom_object_get (obj, handle->uris.patch_value, &val, 0);
    const uint32_t key = ((const LV2_Atom_URID*)property)->body;

    if (key == handle->uris.m_param_clock) {
        if (!synth_set_clock(handle->synth, (int)(*((double*)(val + 1)) * 1e6))) {
            lv2_log_warning(&handle->logger, "The sample rate is too low for the clock source.\n");
        }
    } else if (key == handle->uris.m_param_ymmode) {
        synth_set_mode(handle->synth, *((bool*)(val + 1)));
    }

    return true;
}

void aymidi_run(LV2_Handle instance, uint32_t sample_count) {
    AyMidiHandle* handle = (AyMidiHandle*) instance;
    if (!handle->active)
        return;

    LV2_Atom_Sequence* seq = (LV2_Atom_Sequence*) handle->ports[AYMIDI_ATOM_INPUT_PORT];

    int currentFrame = 0;

    LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
        if (ev->time.frames != 0) {
            int max = currentFrame + ev->time.frames;
            max = max < sample_count ? max : sample_count;
            for (int i = currentFrame; i < max; i++) {
                synth_process(handle->synth, &handle->ports[AYMIDI_AUDIO_OUT_LEFT][i], &handle->ports[AYMIDI_AUDIO_OUT_RIGHT][i]);
            }
            currentFrame = max;
        }
        if (ev->body.type == handle->midi_event_uri) {
            aymidi_process_midi_event(handle, ev);
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
        synth_process(handle->synth, &handle->ports[AYMIDI_AUDIO_OUT_LEFT][i], &handle->ports[AYMIDI_AUDIO_OUT_RIGHT][i]);
    }
}

const LV2_Descriptor aymidi_lv2 = {
    AYMIDI_URI,
    aymidi_instantiate,
    aymidi_connect_port,
    aymidi_activate,
    aymidi_run,
    aymidi_deactivate,
    aymidi_cleanup,
    aymidi_extension_data
};

const LV2_Descriptor * lv2_descriptor(uint32_t index)
{
    return &aymidi_lv2;
}

