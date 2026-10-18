/*
 * audioin.h: the audio input service's error codes, port types and functions, under Sony's names
 * and with Sony's definitions (SDK Release 03.570.011, target/include/audioin.h). The audioin
 * driver defines the functions.
 */
#ifndef _SCE_AUDIO_AUDIOIN_H
#define _SCE_AUDIO_AUDIOIN_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* error codes */
#define SCE_AUDIO_IN_ERROR_FATAL			-2144993024	/* 0x80260100 */
#define SCE_AUDIO_IN_ERROR_INVALID_PORT			-2144993023	/* 0x80260101 */
#define SCE_AUDIO_IN_ERROR_INVALID_SIZE			-2144993022	/* 0x80260102 */
#define SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ		-2144993021	/* 0x80260103 */
#define SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE		-2144993020	/* 0x80260104 */
#define SCE_AUDIO_IN_ERROR_INVALID_POINTER		-2144993019	/* 0x80260105 */
#define SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAMETER	-2144993018	/* 0x80260106 */
#define SCE_AUDIO_IN_ERROR_PORT_FULL			-2144993017	/* 0x80260107 */
#define SCE_AUDIO_IN_ERROR_OUT_OF_MEMORY		-2144993016	/* 0x80260108 */
#define SCE_AUDIO_IN_ERROR_NOT_OPENED			-2144993015	/* 0x80260109 */
#define SCE_AUDIO_IN_ERROR_BUSY				-2144993014	/* 0x8026010A */
#define SCE_AUDIO_IN_ERROR_INVALID_PARAMETER		-2144993013	/* 0x8026010B */

/* sceAudioInOpenPort's param: the sample format */
#define SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO	(0)

/* sceAudioInGetStatus's select */
#define SCE_AUDIO_IN_GETSTATUS_MUTE		(1)

/* port types */
#define SCE_AUDIO_IN_PORT_TYPE_VOICE		(0)
#define SCE_AUDIO_IN_PORT_TYPE_RAW		(0x0002)

int sceAudioInOpenPort(int portType, int grain, int freq, int param);
int sceAudioInReleasePort(int port);
int sceAudioInInput(int port, void *destPtr);
int sceAudioInGetAdopt(int portType);
int sceAudioInGetStatus(int select);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_AUDIO_AUDIOIN_H */
