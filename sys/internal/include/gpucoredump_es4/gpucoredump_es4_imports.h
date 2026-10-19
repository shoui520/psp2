/*
 * The imports of SceGpuEs4CoreDump not declared elsewhere. Names from the
 * vitasdk NID database where known; the rest keep their NID, with the
 * meaning seen at the call sites.
 */

#pragma once

#include <scetypes.h>
#include <gpu/gpu_es4_trace.h>
#include <gpu/lockup.h>
#include <gpu/gpu-mmu.h>
#include <gpu/device_sce.h>
#include <gpu/osfunc_sce.h>
#include <pamgr/pamgr_trace.h>
#include <lowio/pervasive.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <power/power.h>
#include <iofilemgr/iofilemgr.h>
#include <modulemgr/modulemgr.h>
#include <rtc/rtc.h>
#include <regmgr/regmgr.h>

/* ScePamgrForDriver: the running PA trace */
int ScePamgrForDriver_0D04B52A(SceUInt64 id);		/* the trace key of id, < 0: none */
int ScePamgrForDriver_88462719(int key);			/* stops it, 0: done */
int sceKernelPaUnregister(int key);
SceUInt32 ScePamgrForDriver_3ACF4316(void);			/* state; bit 0: running */
SceUInt32 ScePamgrForDriver_FB46C3CD(void);			/* the buffer's write position */
SceSize ScePamgrForDriver_019BD60F(int unit);			/* bytes traced */

/* SceKernelUtilsForDriver: the DES of the dump file's segments */
#include <sysmem/des.h>

