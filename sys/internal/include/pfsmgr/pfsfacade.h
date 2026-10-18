#pragma once

#include <scetypes.h>
#include <stdint.h>
#include <iofilemgr/iofilemgr.h>

int ScePfsFacadeForKernel_BD5B21F6(void);

int ScePfsFacadeForKernel_04352D52(IofilemgrChstatByFdParam *param);
int ScePfsFacadeForKernel_4238D2D2(IofilemgrPreadParam *param);
int ScePfsFacadeForKernel_58E643C5(IofilemgrPwriteParam *param);
int ScePfsFacadeForKernel_7F9F4E49(IofilemgrChstatParam *param);
int ScePfsFacadeForKernel_CB622FFE(IofilemgrWriteParam *param);
