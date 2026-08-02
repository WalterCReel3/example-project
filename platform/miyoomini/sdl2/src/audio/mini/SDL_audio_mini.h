// LGPL-2.1 License
// (C) 2025 Steward Fu <steward.fu@gmail.com>

#ifndef __SDL_AUDIO_MINI_H__
#define __SDL_AUDIO_MINI_H__

#include "../../SDL_internal.h"
#include "../SDL_sysaudio.h"

/* This file is included last, after SDL_video_mini.h has pulled in
   SDL_sysvideo.h and left _THIS meaning SDL_VideoDevice. Both of SDL's own
   headers define it, so redefining is the only way to get an audio _THIS here —
   but it has to be undefined first or the compiler is right to complain. */
#undef _THIS
#define _THIS SDL_AudioDevice *this

#define MI_AUDIO_SAMPLE_PER_FRAME   768
#define FUDGE_TICKS                 10
#define FREQ                        44100
#define CHANNELS                    2

/* The system-volume machinery was here: MAX_VOLUME, MIN_RAW_VALUE,
   MAX_RAW_VALUE, the three MI_AO_* ioctl numbers, and JSON_APP_FILE /
   JSON_VOL_KEY for the config file it read them for. All of it is gone with the
   code that used it — see the note in MINI_OpenDevice. */

struct SDL_PrivateAudioData {
    int mixlen;
    int audio_fd;
    uint8_t *mixbuf;
};

#endif

