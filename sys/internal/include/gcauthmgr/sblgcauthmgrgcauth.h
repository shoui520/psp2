/*
 * SceSblGcAuthMgrGcAuth: the game card's authentication. Not written in C yet: the
 * prototype follows vitasdk's psp2kern/kernel/gcauthmgr/gc_auth.h.
 */
#pragma once

#include <scetypes.h>

/* vitasdk: ksceSblGcAuthMgrGcAuthCartAuthentication */
int SceSblGcAuthMgrGcAuthForDriver_68781760(SceUInt32 keyId);
