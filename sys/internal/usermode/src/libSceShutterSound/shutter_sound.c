/*
 * SceShutterSound (vs0:sys/external/libSceShutterSound.suprx): plays the
 * camera shutter sounds through the shell.  The shell is asked over ShellSvc
 * to play the sound; on a development kit a notification names it first.
 */

#ifdef __SNC__
/* built without automatic inlining (SNC -Xautoinlinesize=0) */
#pragma control autoinlinesize=0
#endif

#include <psp2/types.h>
#include <vsh/lowlayer/src/shell_svc/shellsvc.h>

#define SCE_SHUTTER_SOUND_ERROR_INVALID_ARGUMENT	0x80105201

#define SCE_SHUTTER_SOUND_TYPE_SAVE_IMAGE		0
#define SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_START		1
#define SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_END		2

#define SHELLSVC_REQUEST_SHUTTER_SOUND			0x140000

/* the notification text SceAppMgr shows (structure name unknown) */
typedef struct ShutterSoundNotification {
	SceUInt32 unk00;
	char text[60];
} ShutterSoundNotification;

int sceAppMgrSendNotificationRequest(ShutterSoundNotification *notification, int flag);
char *sceClibStrncpy(char *dst, const char *src, SceSize len);

/* asks the shell to play the sound of type (name unknown) */
static int requestShutterSound(struct ShellSvcInterface *svc, SceUInt32 type)
{
	ShellSvcRequest request = (ShellSvcRequest)svc->methods[5];
	struct ShellSvcBuffer in;
	int result = 0;
	int ret;

	in.address = &type;
	in.size = sizeof(type);
	ret = request(svc, SHELLSVC_REQUEST_SHUTTER_SOUND, &in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int sceShutterSoundPlay(SceUInt32 type)
{
	ShutterSoundNotification notification;
	const char *text = NULL;
	char *p, *end;

	if (type != SCE_SHUTTER_SOUND_TYPE_SAVE_IMAGE &&
	    type != SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_START &&
	    type != SCE_SHUTTER_SOUND_TYPE_SAVE_VIDEO_END)
		return SCE_SHUTTER_SOUND_ERROR_INVALID_ARGUMENT;

	switch (type) {
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
		for (p = (char *)&notification, end = p + sizeof(notification); p < end; p++)
			*p = 0;
		sceClibStrncpy(notification.text, text, sizeof(notification.text));
		notification.text[sizeof(notification.text) - 1] = '\0';
		sceAppMgrSendNotificationRequest(&notification, 1);
	}
	return requestShutterSound(SceShellSvc_B31E7F1C(), type);
}
