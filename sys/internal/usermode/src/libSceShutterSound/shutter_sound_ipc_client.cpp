/*
 * The shell's shutter sound method over SceShellSvc's IPMI connection: a synchronous call that
 * returns the call's error or the method's result. Sony's build keeps it out of the module's
 * file (after sceShutterSoundPlay, not inlined); the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "shutter_sound_ipc_client.h"

#define SHUTTER_SOUND_METHOD_PLAY	0x140000

int shutterSoundPlay(IPMI::Client *client, SceUInt32 soundType)
{
	int result = 0;
	SceUInt32 type = soundType;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &type;
	in[0].dataSize = sizeof(type);
	ret = client->invokeSyncMethod(SHUTTER_SOUND_METHOD_PLAY, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
