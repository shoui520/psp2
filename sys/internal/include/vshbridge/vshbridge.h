/*
 * SceVshBridge's system calls a user program reaches (vshbridge's SceVshBridge library),
 * declared as vshbridge.c defines them: whether the console is a test, tool, development,
 * retail, Vita or PlayStation TV unit, its PS code, and whether it is in development mode. A
 * call with a leading underscore is reached through the stub library's wrapper of the same name
 * without it. The header's name is ours.
 */
#ifndef VSHBRIDGE_VSHBRIDGE_H
#define VSHBRIDGE_VSHBRIDGE_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

SceUInt32 vshSblAimgrIsTest(void);
SceUInt32 vshSblAimgrIsTool(void);
SceUInt32 vshSblAimgrIsDEX(void);
SceUInt32 vshSblAimgrIsCEX(void);
SceUInt32 vshSblAimgrIsVITA(void);
SceUInt32 vshSblAimgrIsDolce(void);
SceUInt32 vshSblSsIsDevelopmentMode(void);

int _vshSblAimgrGetPscode(void *code);
int vshSblAimgrGetPscode(void *code);

#ifdef __cplusplus
}
#endif

#endif
