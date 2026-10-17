/* SceKernelBusError, secure-world build (slb2:kbl/SceKernelBusError). */
#pragma once

/* ForTZS imports: names unknown, the os0 function each one stands for */
#define ksceKernelPrintfLevelWithInfo		SceDebugForTZS_D26EB4BB
#define ksceKernelGetAssertLevel		SceDebugForTZS_F851BA4F
#define ksceKernelAssertLevel			SceDebugForTZS_618F89E6

#include <psp2kern/types.h>
#include <psp2kern/kernel/modulemgr.h>
#include <sysmem/debug.h>
#include <intrmgr/intrmgr_tzs.h>

/* the bus error record in the sysroot shared memory */
#define BUSERROR_SHM_RECORD	0x6000
#define BUSERROR_SHM_RECORD_SIZE	0x4000

/* monitor call services of this module (names unknown) */
#define BUSERROR_MONCALL_COLLECT	0x10c
#define BUSERROR_MONCALL_COLLECT_CLEAN	0x10d
#define BUSERROR_MONCALL_NOP		0x10e

/* ForTZS imports, names unknown */
void *SceSysrootForTZS_1C96096F(void);			/* sysroot shared memory */
void SceCpuForTZS_7A5373EB(void *start, SceSize size);	/* data cache clean */

/* buserror_es3.c */
int MoncallCollectBusErrorInfo_ES3(int arg0, int arg1, int arg2, int arg3);
int MoncallCollectCleanBusErrorInfo_ES3(int arg0, int arg1, int arg2, int arg3);
int MoncallNop_ES3(int arg0, int arg1, int arg2, int arg3);
int RegisterBusErrorHandler_ES3(void);
