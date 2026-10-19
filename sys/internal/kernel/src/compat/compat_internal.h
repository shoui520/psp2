/* SceCompat: the functions compat.c provides to the syscall and PocketStation files. */

#pragma once

#include <scetypes.h>
#include <compat/compat.h>

/* compat.c */
int compatSetRifPath(const signed char *contentId);
int compatCheckPocketStationRif(void);
int sceCompatCacheInternal(int mode, void *base, int size);
int compatAllocCdramWithHole(SceCompatCdram *cdram);
int compatSetDisplayConfig(int resolution, int colorSpaceMode);
int compatFrameBufferInit(SceUID frameBuffer);
int compatIdStorageLookup(short leaf, unsigned int offset, void *buffer, unsigned int size);
int compatDatRead(int index, unsigned int offset, void *buffer, unsigned int size);
int compatReadShared32(int location, int *value);
int compatWriteSharedCtrl(const SceCtrlDataPsp *padData);
int compatWriteShared32(int location, int value);
int compatGetPeripheralState(int mode);
int compatGetPspSystemSoftwareVersion(void);
int compatSuspendResume(int mode);
int compatGetDevInf(SceIoDevInfo *info);
int compatGetStatus(void);
int compatLCDCSync(void);
int compatSetSuspendSema(SceUID suspendSema, SceUID resumeSema);
int compatGetUpdateState(void);
int compatSetUpdateState(int state);
int compatGetTitleListForVsh(char *buffer, unsigned int size);
int compatAvailableColorSpaceSetting(void);
int compatIsPocketStationTitle(const signed char *titleId);
