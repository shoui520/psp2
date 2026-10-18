/*
 * The GPU driver's lockup exports (driver/src/gpu/lockup/signal-lockup.c in gpu_es4). Sony's
 * records name neither; the link names are still vitasdk's.
 */
#pragma once

int ksceGpuSignalWaitLockup(void);
int ksceGpuGetRegisterDump(void *destination, unsigned size);
