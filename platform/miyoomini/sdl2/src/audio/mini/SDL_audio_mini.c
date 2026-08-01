// LGPL-2.1 License
// (C) 2025 Steward Fu <steward.fu@gmail.com>

#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_MINI

/* Upstream included string.h, math.h, errno.h, fcntl.h, signal.h, sys/time.h,
   sys/ioctl.h, sys/stat.h and linux/soundcard.h here. Every one of them was
   reachable only from the system-volume code that opened and ioctl'd
   /dev/mi_ao; none is used now. */
#include <stdio.h>
#include <unistd.h>

#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"

#include "mi_sys.h"
#include "mi_common_datatype.h"
#include "mi_ao.h"

#include "../../video/mini/SDL_video_mini.h"
#include "SDL_audio_mini.h"

static MI_AUDIO_Attr_t stSetAttr;
static MI_AUDIO_Attr_t stGetAttr;
static MI_AO_CHN AoChn = 0;
static MI_AUDIO_DEV AoDevId = 0;

static void MINI_CloseDevice(_THIS)
{
    SDL_free(this->hidden->mixbuf);
    SDL_free(this->hidden);

    MI_AO_DisableChn(AoDevId, AoChn);
    MI_AO_Disable(AoDevId);
}

/* SDL 2.24 dropped the handle and iscapture parameters; capture is reported
   through the AudioBootStrap instead, and this driver has never offered it. */
static int MINI_OpenDevice(_THIS, const char *devname)
{
    MI_S32 miret = 0;
    MI_S32 s32SetVolumeDb = 0;
    MI_S32 s32GetVolumeDb = 0;
    MI_SYS_ChnPort_t stAoChn0OutputPort0;

    this->hidden = (struct SDL_PrivateAudioData *)SDL_malloc((sizeof * this->hidden));
    if(this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(this->hidden);

    this->hidden->mixlen = this->spec.samples * 2 * this->spec.channels;
    this->hidden->mixbuf = (Uint8 *) SDL_malloc(this->hidden->mixlen);
    if(this->hidden->mixbuf == NULL) {
        return SDL_OutOfMemory();
    }

    stSetAttr.eBitwidth = E_MI_AUDIO_BIT_WIDTH_16;
    stSetAttr.eWorkmode = E_MI_AUDIO_MODE_I2S_MASTER;
    stSetAttr.u32FrmNum = 6;
    stSetAttr.u32PtNumPerFrm = this->spec.samples;
    stSetAttr.u32ChnCnt = this->spec.channels;
    stSetAttr.eSoundmode = this->spec.channels == 2 ? E_MI_AUDIO_SOUND_MODE_STEREO : E_MI_AUDIO_SOUND_MODE_MONO;
    stSetAttr.eSamplerate = (MI_AUDIO_SampleRate_e)this->spec.freq;
    //printf(PREFIX"Freq:%d, Sample:%d, Channels:%d\n", this->spec.freq, this->spec.samples, this->spec.channels);
    miret = MI_AO_SetPubAttr(AoDevId, &stSetAttr);
    if(miret != MI_SUCCESS) {
        //printf(PREFIX"failed to set PubAttr\n");
        return -1;
    }
    miret = MI_AO_GetPubAttr(AoDevId, &stGetAttr);
    if(miret != MI_SUCCESS) {
        //printf(PREFIX"failed to get PubAttr\n");
        return -1;
    }
    miret = MI_AO_Enable(AoDevId);
    if(miret != MI_SUCCESS) {
        //printf(PREFIX"failed to enable AO\n");
        return -1;
    }
    miret = MI_AO_EnableChn(AoDevId, AoChn);
    if(miret != MI_SUCCESS) {
        //printf(PREFIX"failed to enable Channel\n");
        return -1;
    }
    miret = MI_AO_SetVolume(AoDevId, s32SetVolumeDb);
    if(miret != MI_SUCCESS) {
        //printf(PREFIX"failed to set Volume\n");
        return -1;
    }
    MI_AO_GetVolume(AoDevId, &s32GetVolumeDb);
    stAoChn0OutputPort0.eModId = E_MI_MODULE_ID_AO;
    stAoChn0OutputPort0.u32DevId = AoDevId;
    stAoChn0OutputPort0.u32ChnId = AoChn;
    stAoChn0OutputPort0.u32PortId = 0;
    MI_SYS_SetChnOutputPortDepth(&stAoChn0OutputPort0, 12, 13);

    /* The system volume is deliberately not touched here.
     *
     * Upstream overwrote it on every open, via ioctl on /dev/mi_ao, with a
     * value it read out of /appconfigs/system.json — which is the only reason
     * this driver ever linked json-c. That is a different knob from the
     * MI_AO_SetVolume above: this one is the AO device's own gain, set to
     * unity as part of bringing the device up, and the ioctl one is the
     * system-wide level the firmware's volume control owns.
     *
     * Re-asserting the system volume from a config file is the firmware's job,
     * not an audio backend's. Leaving it alone means whatever the user set with
     * the volume buttons is what plays, and it removes upstream's worst
     * behaviour along with it: its fallback when the file was missing was 0,
     * and 0 drove the level to MIN_RAW_VALUE and muted the device.
     */

    return 0;
}

static void MINI_PlayDevice(_THIS)
{
    MI_AUDIO_Frame_t aoTestFrame;

    aoTestFrame.eBitwidth = stGetAttr.eBitwidth;
    aoTestFrame.eSoundmode = stGetAttr.eSoundmode;
    aoTestFrame.u32Len = this->hidden->mixlen;
    aoTestFrame.apVirAddr[0] = this->hidden->mixbuf;
    aoTestFrame.apVirAddr[1] = NULL;
    MI_AO_SendFrame(AoDevId, AoChn, &aoTestFrame, 1);
    usleep(((stSetAttr.u32PtNumPerFrm * 1000) / stSetAttr.eSamplerate - 10) * 1000);
}

static uint8_t *MINI_GetDeviceBuf(_THIS)
{
    return (this->hidden->mixbuf);
}

/* SDL 2.24 turned this into SDL_bool, along with demand_only below. */
static SDL_bool MINI_Init(SDL_AudioDriverImpl *impl)
{
    impl->OpenDevice = MINI_OpenDevice;
    impl->PlayDevice = MINI_PlayDevice;
    impl->GetDeviceBuf = MINI_GetDeviceBuf;
    impl->CloseDevice = MINI_CloseDevice;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    return SDL_TRUE;
}

AudioBootStrap Mini_AudioDriver = {
    "Miyoo Mini",
    "Miyoo Mini Audio Driver",
    MINI_Init,
    SDL_FALSE
};

#endif

