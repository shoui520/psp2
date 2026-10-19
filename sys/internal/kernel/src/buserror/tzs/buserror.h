/* SceKernelBusError, secure-world build (slb2:kbl/SceKernelBusError). */
#pragma once

/* the ForTZS imports under their os0 names, before any declaration */
#include <sysmem/debug_tzs.h>

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
