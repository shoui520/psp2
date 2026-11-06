/*
 * shutter_sound.h: the SDK's camera shutter sound (SceShutterSound), Sony's names and values.
 */
#ifndef _SCE_SHUTTER_SOUND_H
#define _SCE_SHUTTER_SOUND_H

#include <scetypes.h>

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define SCE_SHUTTER_SOUND_ERROR_INVALID_ARGUMENT	-2146414079	/* 0x80105201 */
#define SCE_SHUTTER_SOUND_ERROR_INTERNAL		-2146414078	/* 0x80105202 */
#define SCE_SHUTTER_SOUND_ERROR_FATAL			-2146414077	/* 0x80105203 */

#define SCE_SHUTTER_SOUND_TYPE_SAVE_IMAGE		(0)
#define SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_START		(1)
#define SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_END		(2)

SceInt32 sceShutterSoundPlay(SceUInt32 soundType);

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _SCE_SHUTTER_SOUND_H */
