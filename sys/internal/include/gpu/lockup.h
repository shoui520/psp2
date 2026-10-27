/*
 * The GPU driver's lockup exports (driver/src/gpu/lockup/signal-lockup.c and the lockup history
 * in gpu_es4). Sony's records name none of them.
 */
#pragma once

/*
 * A lockup handler pair (gpucoredump_es4's): begin() runs before the register dump is taken and
 * returns a token, end(token) after the analysis when no recovery follows.
 */
typedef unsigned (*GpuLockupBeginCallback)(void);
typedef void (*GpuLockupEndCallback)(unsigned token);

/* signal-lockup.c */
int sceGpuSignalWaitLockup(void);
/* vitasdk: ksceGpuGetRegisterDump */
int SceGpuEs4ForDriver_14ECCFCD(void *destination, unsigned size);
void SceGpuEs4ForDriver_26B03DCE(GpuLockupBeginCallback begin, GpuLockupEndCallback end);
unsigned SceGpuEs4ForDriver_F001AE90(void);
unsigned SceGpuEs4ForDriver_18C90B42(void);

/* lockup-history.c */
unsigned SceGpuEs4ForDriver_395F7689(void);
unsigned SceGpuEs4ForDriver_8A3929D2(void *destination, unsigned size);
