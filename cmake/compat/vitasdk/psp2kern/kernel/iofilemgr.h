/* SDK include compatibility: the complete file-info definition is shared. */
#pragma once

#define SceIofileInfo SceIofileInfo_vitasdk
#define SceIoFdInfo SceIoFdInfo_vitasdk
#define ksceIoGetFileInfo ksceIoGetFileInfo_vitasdk
#include_next <psp2kern/kernel/iofilemgr.h>
#undef SceIofileInfo
#undef SceIoFdInfo
#undef ksceIoGetFileInfo

#include <iofilemgr/iofilemgr.h>
