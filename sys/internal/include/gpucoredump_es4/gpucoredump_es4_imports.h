/*
 * The imports of SceGpuEs4CoreDump not declared elsewhere. Names from the
 * vitasdk NID database where known; the rest keep their NID, with the
 * meaning seen at the call sites.
 */

#pragma once

#include <scetypes.h>
#include <gpu/gpu_es4_trace.h>
#include <pamgr/pamgr_trace.h>
#include <lowio/pervasive.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <power/power.h>
#include <iofilemgr/iofilemgr.h>
#include <modulemgr/modulemgr.h>
#include <rtc/rtc.h>
#include <regmgr/regmgr.h>

/* SceGpuEs4ForDriver */
/* registers the PA-trace stop and the core-dump callbacks */
int SceGpuEs4ForDriver_26B03DCE(int (*stopTrace)(void), void (*coreDump)(int dumpTrace));
/* the process of a GPU memory context, -1 if none */
int SceGpuEs4ForDriver_7AA79658(SceUInt32 context, SceUID *pid);
/* the PowerVR services of SceGpuEs4 */
typedef struct PvrsrvDeviceIdentifier {
	SceUInt32 type;				/* 7: SGX */
	SceUInt32 cls;				/* 0: 3D */
	SceUInt32 index;
	const char *pdumpDevName;
	const char *pdumpRegName;
} PvrsrvDeviceIdentifier;

#define PVRSRV_DEVICE_TYPE_SGX		7
#define PVRSRV_DEVICE_CLASS_3D		0

int PVRSRVEnumerateDevicesKM(SceUInt32 *count, PvrsrvDeviceIdentifier *devices);
int PVRSRVAcquireDeviceDataKM(SceUInt32 index, SceUInt32 type, void **node);
void *OSMemSet(void *ptr, int value, SceSize size);

/* ScePamgrForDriver: the running PA trace */
int ScePamgrForDriver_0D04B52A(SceUInt64 id);		/* the trace key of id, < 0: none */
int ScePamgrForDriver_88462719(int key);			/* stops it, 0: done */
int sceKernelPaUnregister(int key);
SceUInt32 ScePamgrForDriver_3ACF4316(void);			/* state; bit 0: running */
SceUInt32 ScePamgrForDriver_FB46C3CD(void);			/* the buffer's write position */
SceSize ScePamgrForDriver_019BD60F(int unit);			/* bytes traced */

/* SceKernelUtilsForDriver: the DES of the dump file's segments */
#include <sysmem/des.h>

