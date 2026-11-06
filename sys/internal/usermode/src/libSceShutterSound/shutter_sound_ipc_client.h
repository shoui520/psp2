/* the shell's shutter sound method (shutter_sound_ipc_client.cpp; the names are ours) */
#ifndef LIBSCESHUTTERSOUND_SHUTTER_SOUND_IPC_CLIENT_H
#define LIBSCESHUTTERSOUND_SHUTTER_SOUND_IPC_CLIENT_H

#include <scetypes.h>
#include "../../../../../vsh/lowlayer/src/ipmi/client.h"

int shutterSoundPlay(IPMI::Client *client, SceUInt32 soundType);

#endif
