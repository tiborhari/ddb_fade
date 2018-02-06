#define _POSIX_C_SOURCE 199309L

#include <deadbeef/deadbeef.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

DB_functions_t *deadbeef;
static DB_dsp_t plugin;

// Constants
#define STOP 0
#define START 1
#define SEEK 2

// Global variables
int status = START;  // 0: stop, 1: start, 2: seek stop, 3: seek start
int processing_buffer_interval = 0;
float fade_ratio = 0.0;

// Global settings
int start_interval = 500;
int stop_interval = 500;
int seek_interval = 100;

enum {
    FADE_PARAM_START = 0,
    FADE_PARAM_STOP = 1,
    FADE_PARAM_SEEK = 2,
    FADE_PARAM_COUNT
};

typedef struct {
    ddb_dsp_context_t ctx;
    uint16_t start;
    uint16_t stop;
    uint16_t seek;
} ddb_fade_t;

void update_global_settings (ddb_dsp_context_t *ctx) {
    ddb_fade_t *fade = (ddb_fade_t*)ctx;
    start_interval = fade->start;
    stop_interval = fade->stop;
    seek_interval = fade->seek;
}

ddb_dsp_context_t*
ddb_fade_open (void) {
    ddb_fade_t *fade = malloc (sizeof (ddb_fade_t));
    DDB_INIT_DSP_CONTEXT (fade, ddb_fade_t, &plugin);

    fade->start = 500;
    fade->stop = 500;
    fade->seek = 100;
    return (ddb_dsp_context_t *)fade;
}

void
ddb_fade_close (ddb_dsp_context_t *ctx) {
    ddb_fade_t *fade = (ddb_fade_t*)ctx;
    free (fade);
}

float
ddb_fade_get_value (float ratio) {
    return exp(log(2)*ratio) - 1.0;
}

int
ddb_get_interval(int status) {
    if (status & SEEK) return seek_interval;
    else if (status & START) return start_interval;
    else return stop_interval;
}

int
ddb_fade_process (ddb_dsp_context_t *ctx, float *samples, int frames, int maxframes, ddb_waveformat_t *fmt, float *ratio) {
    float interval_frames;
    int current_status = status;  // Make a copy to prevent race conditions
    int interval = ddb_get_interval(current_status);
    // The duration (in ms) of the received buffer
    int new_buffer_interval = (float)frames / (float)fmt->samplerate * 1000.0;

    if (new_buffer_interval > processing_buffer_interval) {
        processing_buffer_interval = new_buffer_interval;
    }

    interval_frames = (int)((float)interval * (float)fmt->samplerate / 1000.0);
    for (int i = 0; i < frames; ++i) {
        for (int s = 0; s < fmt->channels; ++s) {
            samples[i*fmt->channels + s] *= ddb_fade_get_value(fade_ratio);
        }

        if (current_status & START) fade_ratio += 1.0 / interval_frames;
        else fade_ratio -= 1.0 / interval_frames;
        if (1.0 < fade_ratio) fade_ratio = 1.0;
        if (fade_ratio < 0.0) fade_ratio = 0.0;
    }

    return frames;
}

void
sleep_millis (int milliseconds) {
    struct timespec req, rem;
    int sleep_time;

    while (milliseconds > 0) {
        if (milliseconds >= 10) sleep_time = 10;
        else sleep_time = milliseconds;
        req.tv_sec = 0;
        req.tv_nsec = sleep_time * 1000000;
        nanosleep(&req, &rem);
        milliseconds -= sleep_time;
    }
}

static int ddb_fade_message (uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    DB_output_t *output = deadbeef->get_output();

    switch (id) {
        case DB_EV_SEEK:
            // Seek started, fade-out
            status = STOP | SEEK;
            break;
        case DB_EV_SEEKED:
            // Seek finished, fade-in
            status = START | SEEK;
            break;
        case DB_EV_PAUSED:
            if (p1) status = STOP;
            else status = START;
            break;
        case DB_EV_TOGGLE_PAUSE:
            if (output->state() == OUTPUT_STATE_PLAYING) status = STOP;
            else status = START;
            break;
        case DB_EV_STOP:
        case DB_EV_PAUSE:
        case DB_EV_PLAY_CURRENT:
        case DB_EV_PLAY_NUM:
        case DB_EV_PLAY_RANDOM:
        case DB_EV_NEXT:
        case DB_EV_PREV:
            status = STOP;
            break;
        case DB_EV_SONGSTARTED:
            status = START;
            break;
        default:
            break;
    }

    if (!(status & START)) {
        // Wait until the audio is faded out
        if (fade_ratio > 0.0) {
            sleep_millis(ddb_get_interval(status)*fade_ratio + processing_buffer_interval*2);
        }
        fade_ratio = 0.0;
    }

    return 0;
}

void
ddb_fade_reset (ddb_dsp_context_t *ctx) { /* noop */ }

int
ddb_fade_num_params (void) {
    return FADE_PARAM_COUNT;
}

const char *
ddb_fade_get_param_name (int p) {
    switch (p) {
    case FADE_PARAM_START:
        return "Fade-in duration on playback start";
    case FADE_PARAM_STOP:
        return "Fade-out duration on playback stop";
    case FADE_PARAM_SEEK:
        return "Fade-in/fade-out duration when seeking";
    default:
        fprintf (stderr, "ddb_fade_get_param_name: invalid param index (%d)\n", p);
        return "";
    }
}

void
ddb_fade_set_param (ddb_dsp_context_t *ctx, int p, const char *val) {
    ddb_fade_t *fade = (ddb_fade_t*)ctx;

    switch (p) {
    case FADE_PARAM_START:
        fade->start = (uint16_t)atof(val);
        break;
    case FADE_PARAM_STOP:
        fade->stop = (uint16_t)atof(val);
        break;
    case FADE_PARAM_SEEK:
        fade->seek = (uint16_t)atof(val);
        break;
    default:
        fprintf (stderr, "ddb_fade_set_param: invalid param index (%d)\n", p);
    }
    update_global_settings(ctx);
}

void
ddb_fade_get_param (ddb_dsp_context_t *ctx, int p, char *val, int sz) {
    ddb_fade_t *fade = (ddb_fade_t*)ctx;

    switch (p) {
    case FADE_PARAM_START:
        snprintf (val, sz, "%f", (float)fade->start);
        break;
    case FADE_PARAM_STOP:
        snprintf (val, sz, "%f", (float)fade->stop);
        break;
    case FADE_PARAM_SEEK:
        snprintf (val, sz, "%f", (float)fade->seek);
        break;
    default:
        fprintf (stderr, "ddb_fade_get_param: invalid param index (%d)\n", p);
    }
}

static const char ddb_fade_configdialog[] =
    "property \"Start duration (ms)\" spinbtn[0,5000,50] 0 500;\n"
    "property \"Stop duration (ms)\" spinbtn[0,5000,50] 1 500;\n"
    "property \"Seek duration (ms)\" spinbtn[0,2000,50] 2 100;\n"
;

static DB_dsp_t plugin = {
    .plugin.api_vmajor = 1,
    .plugin.api_vminor = 0,
    .plugin.version_major = 0,
    .plugin.version_minor = 1,
    .plugin.type = DB_PLUGIN_DSP,
    .plugin.id = "ddb_fade",
    .plugin.name = "Audio fade-in/fade-out",
    .plugin.descr =
        "Audio fading plugin, that provides audio fade-in, "
        "and fade-out effect, when starting, stopping, or seeking.\n",
    .plugin.copyright =
        "Copyright (c) 2017, Tibor Hári\n"
        "\n"
        "Redistribution and use in source and binary forms, with or without\n"
        "modification, are permitted provided that the following conditions are met:\n"
        "\n"
        "1. Redistributions of source code must retain the above copyright notice,\n"
        "   this list of conditions and the following disclaimer.\n"
        "\n"
        "2. Redistributions in binary form must reproduce the above copyright notice,\n"
        "   this list of conditions and the following disclaimer in the documentation\n"
        "   and/or other materials provided with the distribution.\n"
        "\n"
        "3. Neither the name of the copyright holder nor the names of its contributors\n"
        "   may be used to endorse or promote products derived from this software\n"
        "   without specific prior written permission.\n"
        "\n"
        "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n"
        "AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
        "IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n"
        "DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE\n"
        "FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
        "DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR\n"
        "SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER\n"
        "CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,\n"
        "OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
        "OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n",
    .plugin.website = "https://github.com/tiborhari/ddb_fade",
    .plugin.message = ddb_fade_message,
    .num_params = ddb_fade_num_params,
    .get_param_name = ddb_fade_get_param_name,
    .set_param = ddb_fade_set_param,
    .get_param = ddb_fade_get_param,
    .configdialog = ddb_fade_configdialog,
    .open = ddb_fade_open,
    .close = ddb_fade_close,
    .process = ddb_fade_process,
    .reset = ddb_fade_reset
};

DB_plugin_t *
ddb_fade_load (DB_functions_t *api) {
    deadbeef = api;
    return DB_PLUGIN (&plugin);
}
