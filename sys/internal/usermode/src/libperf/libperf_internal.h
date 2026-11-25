/*
 * ScePerf internal declarations.
 */

#ifndef LIBPERF_INTERNAL_H
#define LIBPERF_INTERNAL_H

#include <stddef.h>
#include <libperf.h>
#include <libperf/razor.h>
#include <sceerror.h>
#include <kernel/error.h>
#include <kernel/modulemgr.h>
#include <kernel/threadmgr.h>
#include <kernel/libkernel.h>

/* SceLibKernel clib imports */
int sceClibPrintf(const char *fmt, ...);
void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);

/* ScePamgr user-mode syscall imports */
int _sceKernelPaAddBusTraceByKey(SceUInt32 key, const void *param);
int _sceKernelPaAddGpuTraceByKey(SceUInt32 key, const void *param);
int _sceKernelPaAddArmTraceByKey(SceUInt32 key, const void *param);
int _sceKernelPaGetGpuSampledData(void *data);
int _sceKernelPaAddCounterTraceByKey(SceUInt32 key, const void *param);
int sceKernelPerfArmPmonReset(SceUID threadId);
int _sceKernelPaSetupTraceBufferByKey(SceUInt32 key, const void *param);
int sceKernelPerfArmPmonSelectEvent(SceUID threadId, SceUInt32 counter, SceUInt8 eventCode);
int sceKernelPerfArmPmonStop(SceUID threadId);
SceUInt32 sceKernelPaGetIoBaseAddress(void);
int sceKernelPerfArmPmonStart(SceUID threadId);
SceUInt32 sceKernelPaGetTimebaseFrequency(void);
int sceKernelPerfArmPmonSetCounterValue(SceUID threadId, SceUInt32 counter, SceUInt32 value);

/* SceDTrace client imports */
int sceDTraceClientOpen(const char *path, int flags);
int sceDTraceClientIoctl(int fd, SceUInt32 cmd, void *buf, SceSize size);
int sceDTraceClientClose(int fd);

/* SceLibKernel PMON imports */
int sceKernelGetPMUSERENR(void);
int sceKernelPMonThreadGetCounter(SceUID threadId, SceUInt32 counter, SceUInt32 *pValue);

/* ARM assembly helpers (razor_debug_marker.S / razor_hooks.S) */
void _razorCpuDebugMarker(void);
void _razorCpuSelectPmonCounter(SceUInt32 counter);
void _razorCpuSyncHook(void);
void _razorCpuStopCaptureHook(void);
void _razorCpuStartCaptureHook(void);
void _razorCpuStopAfterFramesHook(void);

/* Shared PA MMIO base address */
extern volatile SceUInt32 *g_paIoBase;

/* Internal initialization called from module_start */
int razorCpuInit(SceUInt32 numCounters);

#endif /* LIBPERF_INTERNAL_H */
