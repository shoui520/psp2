/*
 * The GPU driver's lockup exports (driver/src/gpu/lockup/signal-lockup.c in gpu_es4). Sony's
 * records name neither.
 */
#pragma once

int SceGpuEs4ForDriver_40E292D7(void);	/* vitasdk: ksceGpuSignalWaitLockup */
/* vitasdk: ksceGpuGetRegisterDump */
int SceGpuEs4ForDriver_14ECCFCD(void *destination, unsigned size);
