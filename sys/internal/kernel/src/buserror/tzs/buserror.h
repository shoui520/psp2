/* SceKernelBusError, secure-world build (slb2:kbl/SceKernelBusError). */
#pragma once

/* ForTZS imports: the os0 name of each one's function */
#define SceDebugForDriver_D9703808		SceDebugForTZS_D26EB4BB
#define SceDebugForDriver_374B7868		SceDebugForTZS_F851BA4F
#define SceDebugForDriver_FD753E7A			SceDebugForTZS_618F89E6

#include <scetypes.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysroot.h>
#include <intrmgr/intrmgr_tzs.h>

/* the bus error record in the sysroot shared memory */
#define BUSERROR_SHM_RECORD	0x6000
#define BUSERROR_SHM_RECORD_SIZE	0x4000

/* monitor call services of this module (names unknown) */
#define BUSERROR_MONCALL_COLLECT	0x10c
#define BUSERROR_MONCALL_COLLECT_CLEAN	0x10d
#define BUSERROR_MONCALL_NOP		0x10e

/* buserror_es3.c */
int MoncallCollectBusErrorInfo_ES3(int arg0, int arg1, int arg2, int arg3);
int MoncallCollectCleanBusErrorInfo_ES3(int arg0, int arg1, int arg2, int arg3);
int MoncallNop_ES3(int arg0, int arg1, int arg2, int arg3);
int RegisterBusErrorHandler_ES3(void);
