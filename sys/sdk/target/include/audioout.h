/*
 * audioout.h: the audio output service's error codes, port types, formats, volumes and functions,
 * under Sony's names and with Sony's definitions (SDK Release 03.570.011,
 * target/include/audioout.h). The audio driver defines the functions.
 */
#ifndef _SCE_AUDIO_AUDIOOUT_H
#define _SCE_AUDIO_AUDIOOUT_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* error codes */
#define SCE_AUDIO_OUT_ERROR_NOT_OPENED			-2144993279	/* 0x80260001 */
#define SCE_AUDIO_OUT_ERROR_BUSY			-2144993278	/* 0x80260002 */
#define SCE_AUDIO_OUT_ERROR_INVALID_PORT		-2144993277	/* 0x80260003 */
#define SCE_AUDIO_OUT_ERROR_INVALID_POINTER		-2144993276	/* 0x80260004 */
#define SCE_AUDIO_OUT_ERROR_PORT_FULL			-2144993275	/* 0x80260005 */
#define SCE_AUDIO_OUT_ERROR_INVALID_SIZE		-2144993274	/* 0x80260006 */
#define SCE_AUDIO_OUT_ERROR_INVALID_FORMAT		-2144993273	/* 0x80260007 */
#define SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ		-2144993272	/* 0x80260008 */
#define SCE_AUDIO_OUT_ERROR_INVALID_VOLUME		-2144993271	/* 0x80260009 */
#define SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE		-2144993270	/* 0x8026000A */
#define SCE_AUDIO_OUT_ERROR_INVALID_CONF_TYPE		-2144993268	/* 0x8026000C */
#define SCE_AUDIO_OUT_ERROR_OUT_OF_MEMORY		-2144993267	/* 0x8026000D */
#define SCE_AUDIO_OUT_ERROR_INVALID_FX_TYPE		-2144992768	/* 0x80260200 */

/* port types */
#define SCE_AUDIO_OUT_PORT_TYPE_MAIN		(0)
#define SCE_AUDIO_OUT_PORT_TYPE_BGM		(1)
#define SCE_AUDIO_OUT_PORT_TYPE_VOICE		(2)

/* param: the sample format and the effect */
#define SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO	(0)
#define SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO	(1)
#define SCE_AUDIO_OUT_PARAM_FORMAT_MASK		(0x000000FF)
#define SCE_AUDIO_OUT_PARAM_FORMAT_SHIFT	(0)
#define SCE_AUDIO_OUT_PARAM_FX_TYPE_THRU	(0)
#define SCE_AUDIO_OUT_PARAM_FX_TYPE_MASK	(0x0000FF00)
#define SCE_AUDIO_OUT_PARAM_FX_TYPE_SHIFT	(8)

/* the grain, in samples */
#define SCE_AUDIO_MIN_LEN		64u
#define SCE_AUDIO_MAX_LEN		(64u*1023)

/* volumes */
#define SCE_AUDIO_VOLUME_SHIFT		15
#define SCE_AUDIO_VOLUME_0DB		(1<<SCE_AUDIO_VOLUME_SHIFT)
#define SCE_AUDIO_VOLUME_0dB		SCE_AUDIO_VOLUME_0DB	/* for PSP compatible */
#define SCE_AUDIO_VOLUME_FLAG_L_CH	(0x0001)
#define SCE_AUDIO_VOLUME_FLAG_R_CH	(0x0002)

/* sceAudioOutGetConfig's configType */
#define SCE_AUDIO_OUT_CONFIG_TYPE_LEN	(0)
#define SCE_AUDIO_OUT_CONFIG_TYPE_FREQ	(1)
#define SCE_AUDIO_OUT_CONFIG_TYPE_PARAM	(2)

/* sceAudioOutSetAlcMode's mode */
#define SCE_AUDIO_ALC_OFF		0
#define SCE_AUDIO_ALC_MODE1		1
#define SCE_AUDIO_ALC_MODE_MAX		2

int sceAudioOutOpenPort(int portType, int len, int freq, int param);
int sceAudioOutReleasePort(int port);
int sceAudioOutOutput(int port, const void *ptr);
int sceAudioOutSetVolume(int port, int flag, int *vol);
int sceAudioOutSetConfig(int port, int len, int freq, int param);
int sceAudioOutGetConfig(int port, int configType);
int sceAudioOutGetRestSample(int port);
int sceAudioOutGetAdopt(int portType);
int sceAudioOutSetAlcMode(int mode);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_AUDIO_AUDIOOUT_H */
