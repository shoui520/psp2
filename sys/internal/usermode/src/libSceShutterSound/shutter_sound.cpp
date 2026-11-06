/*
 * SceShutterSound (vs0:sys/external/libSceShutterSound.suprx): the camera shutter sounds, played
 * by the shell (SceShellSvc's IPMI service). The shell is first sent a notice naming the sound.
 * No Sony record names its files; the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <shutter_sound.h>
#include <appmgr/appmgr.h>
#include <libkernel/libkernel.h>
#include "../../../../../vsh/lowlayer/src/shell_svc/shell_svc_client.h"
#include "shutter_sound_ipc_client.h"

extern "C" SceInt32 sceShutterSoundPlay(SceUInt32 soundType)
{
	AppmgrNotificationRequest notification;
	const char *text;
	unsigned char *p, *end;

	if (soundType != SCE_SHUTTER_SOUND_TYPE_SAVE_IMAGE &&
	    soundType != SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_START &&
	    soundType != SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_END)
		return SCE_SHUTTER_SOUND_ERROR_INVALID_ARGUMENT;
	text = NULL;
	switch (soundType) {
	case SCE_SHUTTER_SOUND_TYPE_SAVE_IMAGE:
		text = "ShutterSound:\nPlay \"IMAGE\" sound";
		break;
	case SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_START:
		text = "ShutterSound:\nPlay \"VIDEO_START\" sound";
		break;
	case SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_END:
		text = "ShutterSound:\nPlay \"VIDEO_END\" sound";
		break;
	}
	if (text != NULL) {
		for (p = (unsigned char *)&notification, end = p + sizeof(notification); p < end; p++)
			*p = 0;
		sceClibStrncpy((char *)notification.payload, text, sizeof(notification.payload));
		notification.payload[sizeof(notification.payload) - 1] = '\0';
		sceAppMgrSendNotificationRequest(&notification, 1);
	}
	return shutterSoundPlay(SceShellSvc_B31E7F1C(), soundType);
}
