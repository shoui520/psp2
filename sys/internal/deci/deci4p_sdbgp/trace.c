/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): kernel event trace (group 0x90).
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/debug.h>
#include <sysmem/debugled.h>
#include <sysmem/suspend.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>

#include "sdbgp_internal.h"

SceSize strnlen(const char *s, SceSize maxlen);
void ScePamgrForDriver_A9DD2CB8(void *enter, void *leave);

typedef uint16_t u16_alias __attribute__((may_alias));
typedef uint32_t u32_alias __attribute__((may_alias));
typedef uint64_t u64_alias __attribute__((may_alias));

#define TRACE_ENTRY_SIZE	80

/* one core's trace ring buffer state (0x81043640, 64 bytes per core) */
typedef struct SdbgpTraceCpuState {
	int8_t enabled;				/* 0x00 */
	int8_t ringWrap;			/* 0x01: 1 overwrite oldest, 0 stop when full */
	int8_t full;				/* 0x02 */
	int8_t pad;				/* 0x03 */
	uint32_t eventMask;			/* 0x04 */
	uintptr_t bufStart;			/* 0x08 */
	uintptr_t bufEnd;			/* 0x0c */
	uintptr_t writePtr;			/* 0x10 */
	uintptr_t readPtr;			/* 0x14 */
	uintptr_t snapshotPtr;			/* 0x18 */
	uint32_t flags;				/* 0x1c */
	uint32_t bufSize;			/* 0x20 */
	uint32_t ratioMask;			/* 0x24 */
	uint64_t lastTimestamp;			/* 0x28 */
	uint32_t reserved[3];			/* 0x30 */
	int intrState;				/* 0x3c */
} SdbgpTraceCpuState;

_Static_assert(sizeof(SdbgpTraceCpuState) == 0x40, "SdbgpTraceCpuState size");

static uintptr_t s_traceSavedSnapshotPtr[4];
static SdbgpTraceCpuState s_traceCpu[4];
static uintptr_t s_traceSavedReadPtr[4];
static uintptr_t s_traceSavedWritePtr[4];
static uint64_t s_traceLastReadTimestamp;

/* 0x810145dc */
static int traceResetBuffers(void)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (s_traceCpu[i].enabled != 0)
			s_traceCpu[i].enabled = 0;
	}
	for (i = 0; i < 4; i++) {
		uintptr_t start = s_traceCpu[i].bufStart;

		s_traceCpu[i].snapshotPtr = 0;
		s_traceCpu[i].ringWrap = 1;
		s_traceCpu[i].enabled = 0;
		s_traceCpu[i].full = 0;
		s_traceCpu[i].writePtr = start;
		s_traceCpu[i].readPtr = start;
	}
	return 0;
}

/* 0x8101462c */
static uint32_t *traceAllocEntry(uint32_t mask, uint32_t type)
{
	int intr;
	uint32_t cpu;
	SdbgpTraceCpuState *c;

	intr = sceKernelCpuSuspendIntr();
	__asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(cpu));
	cpu &= 3;
	c = &s_traceCpu[cpu];
	c->intrState = intr;
	if ((mask == 0 || (mask & c->eventMask) != 0) && c->full == 0) {
		uint64_t now = sceKernelGetSystemTimeWide();
		uintptr_t wp = c->writePtr;
		uintptr_t end = c->bufEnd;
		uintptr_t next = wp + TRACE_ENTRY_SIZE;
		uint32_t *entry = (uint32_t *)wp;

		c->lastTimestamp = now;
		if (next >= end)
			next = c->bufStart;
		if (c->ringWrap != 0) {
			if (c->readPtr == next) {
				uintptr_t nextRead = next + TRACE_ENTRY_SIZE;

				if (nextRead >= c->bufEnd)
					nextRead = c->bufStart;
				c->readPtr = nextRead;
			}
			*(uint64_t *)entry = now;
			entry[2] = type;
			entry[3] = cpu;
			return entry;
		}
		if (c->readPtr != next) {
			*(uint64_t *)entry = now;
			entry[2] = type;
			entry[3] = cpu;
			return entry;
		}
		now = c->lastTimestamp;
		int8_t zero = c->ringWrap;
		c->full = 1;
		entry[3] = cpu;
		*(uint64_t *)entry = now;
		entry[2] = 62;
		for (int i = 0; i < 4; i++) {
			if (s_traceCpu[i].enabled != 0)
				s_traceCpu[i].enabled = zero;
		}
	}
	sceKernelCpuResumeIntr(intr);
	return NULL;
}

/* 0x810146f0 */
static void traceCommitEntry(uint32_t *entry)
{
	if (entry != NULL) {
		SdbgpTraceCpuState *c;
		int intr;
		uintptr_t next;

		__asm__ volatile("dmb sy" ::: "memory");
		c = &s_traceCpu[entry[3]];
		intr = c->intrState;
		next = c->writePtr + TRACE_ENTRY_SIZE;
		if (next >= c->bufEnd)
			next = c->bufStart;
		c->writePtr = next;
		sceKernelCpuResumeIntr(intr);
	}
}

/* 0x81014720 */
static void tracePowerHook1(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 4) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(4, 49);
	if (e != NULL) {
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		e[7] = a4;
		e[2] |= 0x01000000u;
		traceCommitEntry(e);
	}
}

/* 0x8101476c */
static void tracePowerHook0(uint32_t a1, uint32_t a2, uint32_t a3)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 4) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(4, 49);
	if (e != NULL) {
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		traceCommitEntry(e);
	}
}

/* 0x810147b0 */
static void traceAppMgrHook(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x4000) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0x4000, 51);
	if (e != NULL) {
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		e[7] = a4;
		traceCommitEntry(e);
	}
}

/* 0x810147f8 */
static void traceUndefExcpHook(SceExcpmgrContext *context, uint32_t mode)
{
	const uint32_t *ctx = (const uint32_t *)context;
	int i;
	uint32_t *e;

	(void)mode;
	for (i = 0; i < 4; i++) {
		if (s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0, 56);
	if (e != NULL) {
		uintptr_t th = ctx[24];

		if (th != 0) {
			e[4] = *(const uint32_t *)(th + 56);
			e[5] = *(const uint32_t *)(th + 8);
		} else {
			e[4] = 0;
			e[5] = 0;
		}
		e[6] = ctx[15];
		e[7] = ctx[13];
		e[8] = ctx[17];
		e[9] = ctx[21];
		traceCommitEntry(e);
	}
}

/* 0x81014848 */
static void traceDabtExcpHook(SceExcpmgrContext *context, uint32_t mode)
{
	const uint32_t *ctx = (const uint32_t *)context;
	int i;
	uint32_t *e;

	(void)mode;
	for (i = 0; i < 4; i++) {
		if (s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0, 55);
	if (e != NULL) {
		uintptr_t th = ctx[24];

		if (th != 0) {
			e[4] = *(const uint32_t *)(th + 56);
			e[5] = *(const uint32_t *)(th + 8);
		} else {
			e[4] = 0;
			e[5] = 0;
		}
		e[6] = ctx[15];
		e[7] = ctx[13];
		e[8] = ctx[17];
		e[9] = ctx[28];
		e[10] = ctx[30];
		e[11] = ctx[21];
		traceCommitEntry(e);
	}
}

/* 0x810148a0 */
static void tracePabtExcpHook(SceExcpmgrContext *context, uint32_t mode)
{
	const uint32_t *ctx = (const uint32_t *)context;
	int i;
	uint32_t *e;

	(void)mode;
	for (i = 0; i < 4; i++) {
		if (s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0, 54);
	if (e != NULL) {
		uintptr_t th = ctx[24];

		if (th != 0) {
			e[4] = *(const uint32_t *)(th + 56);
			e[5] = *(const uint32_t *)(th + 8);
		} else {
			e[4] = 0;
			e[5] = 0;
		}
		e[6] = ctx[15];
		e[7] = ctx[13];
		e[8] = ctx[17];
		e[9] = ctx[29];
		e[10] = ctx[31];
		e[11] = ctx[21];
		traceCommitEntry(e);
	}
}

/* 0x810148f8 */
static void traceDebugLedHook1(int a1, int a2, int a3, int a4)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if (s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0, 58);
	if (e != NULL) {
		e[4] = (uint32_t)a1;
		e[5] = (uint32_t)a2;
		e[6] = (uint32_t)a3;
		e[7] = (uint32_t)a4;
		traceCommitEntry(e);
	}
}

/* 0x81014934 */
static void traceSuspendHook6(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 4) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(4, 49);
	if (e != NULL) {
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		e[7] = a4;
		e[2] |= 0x07000000u;
		traceCommitEntry(e);
	}
}

/* 0x81014980 */
static void traceSuspendHook5(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 4) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(4, 49);
	if (e != NULL) {
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		e[7] = a4;
		e[2] |= 0x06000000u;
		traceCommitEntry(e);
	}
}

/* 0x810149cc */
static void traceSuspendHook4(uint32_t a1, uint32_t a2, uint32_t a3)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 4) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(4, 49);
	if (e != NULL) {
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		e[2] |= 0x05000000u;
		traceCommitEntry(e);
	}
}

/* 0x81014a18 */
static void traceSuspendHook3(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 4) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(4, 49);
	if (e != NULL) {
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		e[7] = a4;
		e[2] |= 0x04000000u;
		e[8] = a5;
		traceCommitEntry(e);
	}
}

/* 0x81014a68 */
static void traceSuspendHook2(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 4) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(4, 49);
	if (e != NULL) {
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		e[7] = a4;
		e[2] |= 0x04000000u;
		e[8] = a5;
		traceCommitEntry(e);
	}
}

/* 0x81014ab8 */
static void traceSuspendHook1(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 4) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(4, 49);
	if (e != NULL) {
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		e[7] = a4;
		e[2] |= 0x03000000u;
		traceCommitEntry(e);
	}
}

/* 0x81014b04 */
static void traceSysconHook(int sub, int nargs, ...)
{
	int i, j;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x2000) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0x2000, 52);
	if (e != NULL) {
		va_start(ap, nargs);
		switch (sub) {
		case 0:
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			for (j = 0; j < 32; j++)
				((uint8_t *)e)[24 + j] = (uint8_t)va_arg(ap, uint32_t);
			break;
		case 1:
			e[2] |= 0x010000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			for (j = 0; j < 32; j++)
				((uint8_t *)e)[24 + j] = (uint8_t)va_arg(ap, uint32_t);
			break;
		case 2:
			e[2] |= 0x020000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			for (j = 0; j < 32; j++)
				((uint8_t *)e)[24 + j] = (uint8_t)va_arg(ap, uint32_t);
			break;
		case 3:
			e[2] |= 0x030000u;
			e[4] = va_arg(ap, uint32_t);
			for (j = 0; j < 32; j++)
				((uint8_t *)e)[20 + j] = (uint8_t)va_arg(ap, uint32_t);
			break;
		case 4:
			e[2] |= 0x040000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 5:
			e[2] |= 0x050000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			break;
		case 6:
			e[2] |= 0x060000u;
			e[4] = va_arg(ap, uint32_t);
			break;
		default:
			break;
		}
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x81014c44 */
static void traceVeneziaRpcHook(int sub, int nargs, ...)
{
	int i;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x80) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0x80, 41);
	if (e != NULL) {
		va_start(ap, nargs);
		if (sub == 0) {
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			e[12] = va_arg(ap, uint32_t);
			e[13] = va_arg(ap, uint32_t);
			e[14] = va_arg(ap, uint32_t);
			e[15] = va_arg(ap, uint32_t);
		} else if (sub == 1) {
			e[2] |= 0x010000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
		}
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x81014cf0 */
static void traceVipRpcHook(int sub, int nargs, ...)
{
	int i;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x40) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0x40, 40);
	if (e != NULL) {
		va_start(ap, nargs);
		switch (sub) {
		case 0x05:
		case 0x09:
		case 0x0b:
		case 0x0c:
		case 0x12:
		case 0x24:
		case 0x25:
		case 0x2c:
		case 0x3c:
		case 0x3d:
		case 0x49:
		case 0x4a:
		case 0x4b:
		case 0x4c:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 0x04:
		case 0x0d:
		case 0x13:
		case 0x14:
		case 0x1c:
		case 0x1d:
		case 0x1f:
		case 0x22:
		case 0x26:
		case 0x27:
		case 0x2a:
		case 0x2d:
		case 0x38:
		case 0x3a:
		case 0x3f:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x48:
		case 0x4e:
		case 0x4f:
		case 0x50:
		case 0x51:
		case 0x52:
		case 0x57:
		case 0x58:
		case 0x59:
		case 0x5a:
		case 0x5b:
		case 0x5c:
		case 0x5d:
		case 0x5e:
		case 0x5f:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 0x02:
		case 0x0a:
		case 0x15:
		case 0x2e:
		case 0x2f:
		case 0x30:
		case 0x31:
		case 0x36:
		case 0x3e:
		case 0x46:
		case 0x4d:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			break;
		case 0x06:
		case 0x37:
		case 0x39:
		case 0x3b:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			break;
		case 0x10:
		case 0x2b:
		case 0x47:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			break;
		case 0x03:
		case 0x45:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			break;
		case 0x40:
			e[2] |= 0x400000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			e[12] = va_arg(ap, uint32_t);
			break;
		case 0x11:
			e[2] |= 0x110000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			e[12] = va_arg(ap, uint32_t);
			e[13] = va_arg(ap, uint32_t);
			e[14] = va_arg(ap, uint32_t);
			e[15] = va_arg(ap, uint32_t);
			break;
		default:
			break;
		}
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x810151c8 */
static void traceVfpExcpHook(int sub, int nargs, ...)
{
	int i;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if (s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0, 57);
	if (e != NULL) {
		va_start(ap, nargs);
		switch (sub) {
		case 0:
			e[4] = va_arg(ap, uint32_t);
			break;
		case 1:
			e[2] |= 0x010000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			break;
		case 2:
			e[2] |= 0x020000u;
			break;
		case 3:
			e[2] |= 0x030000u;
			break;
		case 4:
			e[2] |= 0x040000u;
			break;
		case 5:
			e[2] |= 0x050000u;
			e[4] = va_arg(ap, uint32_t);
			break;
		default:
			break;
		}
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x8101527c */
static void traceAlarmHook(int nargs, ...)
{
	int i;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x200) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0x200, 38);
	if (e != NULL) {
		va_start(ap, nargs);
		e[4] = va_arg(ap, uint32_t);
		e[5] = va_arg(ap, uint32_t);
		e[6] = va_arg(ap, uint32_t);
		e[7] = va_arg(ap, uint32_t);
		e[8] = va_arg(ap, uint32_t);
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x810152ec */
static void traceSyncHook(int sub, int nargs, ...)
{
	int i;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x20) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0x20, 1);
	if (e != NULL) {
		va_start(ap, nargs);
		switch (sub) {
		case 0x01:
		case 0x04:
		case 0x05:
		case 0x12:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1e:
		case 0x22:
		case 0x23:
		case 0x30:
		case 0x35:
		case 0x36:
			e[2] |= (uint32_t)sub << 16;
			break;
		case 0x16:
		case 0x1f:
		case 0x34:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			break;
		case 0x03:
		case 0x0a:
		case 0x0b:
		case 0x0c:
		case 0x15:
		case 0x21:
		case 0x28:
		case 0x29:
		case 0x2a:
		case 0x33:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 0x02:
		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0d:
		case 0x14:
		case 0x1a:
		case 0x20:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x2b:
		case 0x32:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 0x0e:
		case 0x0f:
		case 0x13:
		case 0x2c:
		case 0x2d:
		case 0x31:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			break;
		default:
			break;
		}
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x810155c0 */
static void traceSwitchHook(int sub, int nargs, ...)
{
	int i;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 1) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(1, 0);
	if (e != NULL) {
		va_start(ap, nargs);
		e[2] |= (uint32_t)sub << 16;
		e[4] = va_arg(ap, uint32_t);
		e[5] = va_arg(ap, uint32_t);
		e[6] = va_arg(ap, uint32_t);
		e[7] = va_arg(ap, uint32_t);
		e[8] = va_arg(ap, uint32_t);
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x81015638 */
static void traceDebugLedHook0(int a1, int a2, int a3, int a4)
{
	const char *str = (const char *)(uintptr_t)a4;
	char nullStr[8] = "(null)";
	int i;

	for (i = 0; i < 4; i++) {
		if (s_traceCpu[i].enabled == 0)
			return;
	}
	int intr = sceKernelCpuSuspendIntr();
	uint32_t *e = traceAllocEntry(0, 59);
	if (e != NULL) {
		e[12] = (uint32_t)a1;
		e[13] = (uint32_t)a2;
		e[14] = (uint32_t)a3;
		if (str == NULL)
			str = nullStr;
		memset(&e[4], 0, 0x20);
		strncpy((char *)&e[4], str, strnlen(str, 31));
		e[15] = 0;
		if (strnlen(str, 256) > 31) {
			const char *p = str + 31;

			e[15] = 1;
			traceCommitEntry(e);
			e = traceAllocEntry(0, 59);
			if (e == NULL) {
				sceKernelCpuResumeIntr(intr);
				return;
			}
			memset(&e[4], 0, 0x20);
			strncpy((char *)&e[4], p, strnlen(p, 31));
			e[12] = 0;
			e[13] = 1;
			e[2] |= 0x010000u;
			while (strnlen(p, 256) > 31) {
				e[12] = 1;
				e[13] = 0;
				p += 31;
				traceCommitEntry(e);
				e = traceAllocEntry(0, 59);
				if (e == NULL) {
					sceKernelCpuResumeIntr(intr);
					return;
				}
				memset(&e[4], 0, 0x20);
				strncpy((char *)&e[4], p, strnlen(p, 31));
				e[12] = 0;
				e[13] = 1;
				e[2] |= 0x010000u;
			}
		}
		traceCommitEntry(e);
	}
	sceKernelCpuResumeIntr(intr);
}
/* 0x810157a0 */
static void traceIofileHook(int sub, int nargs, ...)
{
	char nullStr[8] = "(null)";
	int i;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x800) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	int intr = sceKernelCpuSuspendIntr();
	uint32_t *e = traceAllocEntry(0x800, 53);
	if (e != NULL) {
		va_start(ap, nargs);
		switch (sub) {
		case 0:
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[8], 0, 0x20);
				strncpy((char *)&e[8], s, strnlen(s, 31));
				e[16] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[16] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 1:
			e[2] |= 0x010000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 2:
			e[2] |= 0x020000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[6], 0, 0x20);
				strncpy((char *)&e[6], s, strnlen(s, 31));
				e[14] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 3:
			e[2] |= 0x030000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 4:
			e[2] |= 0x040000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			break;
		case 5:
			e[2] |= 0x050000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 6:
			e[2] |= 0x060000u;
			e[4] = va_arg(ap, uint32_t);
			break;
		case 7:
			e[2] |= 0x070000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 8:
			e[2] |= 0x080000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			break;
		case 9:
			e[2] |= 0x090000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 10:
			e[2] |= 0x0a0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			break;
		case 11:
			e[2] |= 0x0b0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 12:
			e[2] |= 0x0c0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			break;
		case 13:
			e[2] |= 0x0d0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 14:
			e[2] |= 0x0e0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			break;
		case 15:
			e[2] |= 0x0f0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 16:
			e[2] |= 0x100000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			break;
		case 17:
			e[2] |= 0x110000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 18:
			e[2] |= 0x120000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			e[12] = va_arg(ap, uint32_t);
			e[13] = va_arg(ap, uint32_t);
			break;
		case 19:
			e[2] |= 0x130000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 20:
			e[2] |= 0x140000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			break;
		case 21:
			e[2] |= 0x150000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 22:
			e[2] |= 0x160000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			e[12] = va_arg(ap, uint32_t);
			e[13] = va_arg(ap, uint32_t);
			break;
		case 23:
			e[2] |= 0x170000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 24:
			e[2] |= 0x180000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			break;
		case 25:
			e[2] |= 0x190000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 26:
			e[2] |= 0x1a0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			break;
		case 27:
			e[2] |= 0x1b0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 28:
			e[2] |= 0x1c0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[7], 0, 0x20);
				strncpy((char *)&e[7], s, strnlen(s, 31));
				e[15] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[15] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 29:
			e[2] |= 0x1d0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 30:
			e[2] |= 0x1e0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[6], 0, 0x20);
				strncpy((char *)&e[6], s, strnlen(s, 31));
				e[14] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 31:
			e[2] |= 0x1f0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 32:
			e[2] |= 0x200000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[6], 0, 0x20);
				strncpy((char *)&e[6], s, strnlen(s, 31));
				e[14] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 33:
			e[2] |= 0x210000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 34:
			e[2] |= 0x220000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[6], 0, 0x20);
				strncpy((char *)&e[6], s, strnlen(s, 31));
				e[14] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 35:
			e[2] |= 0x230000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 36:
			e[2] |= 0x240000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[7], 0, 0x10);
				strncpy((char *)&e[7], s, strnlen(s, 15));
				e[11] = 0;
				if (strnlen(s, 256) > 15) {
					const char *p = s + 15;
					e[11] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 37:
			e[2] |= 0x250000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 38:
			e[2] |= 0x260000u;
			e[4] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[5], 0, 0x10);
				strncpy((char *)&e[5], s, strnlen(s, 15));
				e[9] = 0;
				if (strnlen(s, 256) > 15) {
					const char *p = s + 15;
					e[9] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 39:
			e[2] |= 0x270000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			break;
		case 40:
			e[2] |= 0x280000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[6], 0, 0x20);
				strncpy((char *)&e[6], s, strnlen(s, 31));
				e[14] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 41:
			e[2] |= 0x290000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 42:
			e[2] |= 0x2a0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[6], 0, 0x20);
				strncpy((char *)&e[6], s, strnlen(s, 31));
				e[14] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 43:
			e[2] |= 0x2b0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 44:
			e[2] |= 0x2c0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				const char *s2 = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[6], 0, 0x20);
				strncpy((char *)&e[6], s, strnlen(s, 31));
				e[14] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 0;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 0;
					}
				}
				traceCommitEntry(e);
				e = traceAllocEntry(0x800, 53);
				if (e == NULL)
					goto done;
				e[2] |= 0x300000u;
				if (s2 == NULL)
					s2 = nullStr;
				memset(&e[4], 0, 0x20);
				strncpy((char *)&e[4], s2, strnlen(s2, 31));
				e[12] = 0;
				if (strnlen(s2, 256) > 31) {
					const char *p2 = s2 + 31;
					e[12] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p2, strnlen(p2, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p2, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p2 += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p2, strnlen(p2, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 45:
			e[2] |= 0x2d0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 46:
			e[2] |= 0x2e0000u;
			e[4] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				const char *s2 = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[5], 0, 0x20);
				strncpy((char *)&e[5], s, strnlen(s, 31));
				e[13] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[13] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 0;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 0;
					}
				}
				traceCommitEntry(e);
				e = traceAllocEntry(0x800, 53);
				if (e == NULL)
					goto done;
				e[2] |= 0x310000u;
				if (s2 == NULL)
					s2 = nullStr;
				memset(&e[4], 0, 0x20);
				strncpy((char *)&e[4], s2, strnlen(s2, 31));
				e[12] = 0;
				if (strnlen(s2, 256) > 31) {
					const char *p2 = s2 + 31;
					e[12] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p2, strnlen(p2, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p2, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p2 += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p2, strnlen(p2, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 47:
			e[2] |= 0x2f0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 50:
			e[2] |= 0x320000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[6], 0, 0x20);
				strncpy((char *)&e[6], s, strnlen(s, 31));
				e[14] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 51:
			e[2] |= 0x330000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 52:
			e[2] |= 0x340000u;
			e[4] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[5], 0, 0x20);
				strncpy((char *)&e[5], s, strnlen(s, 31));
				e[13] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[13] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 53:
			e[2] |= 0x350000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 54:
			e[2] |= 0x360000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			break;
		case 55:
			e[2] |= 0x370000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 56:
			e[2] |= 0x380000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 57:
			e[2] |= 0x390000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 58:
			e[2] |= 0x3a0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			break;
		case 59:
			e[2] |= 0x3b0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 60:
			e[2] |= 0x3c0000u;
			e[4] = va_arg(ap, uint32_t);
			break;
		case 61:
			e[2] |= 0x3d0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 62:
			e[2] |= 0x3e0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[8], 0, 0x10);
				strncpy((char *)&e[8], s, strnlen(s, 15));
				e[12] = 0;
				if (strnlen(s, 256) > 15) {
					const char *p = s + 15;
					e[12] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 63:
			e[2] |= 0x3f0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 64:
			e[2] |= 0x400000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[7], 0, 0x10);
				strncpy((char *)&e[7], s, strnlen(s, 15));
				e[11] = 0;
				if (strnlen(s, 256) > 15) {
					const char *p = s + 15;
					e[11] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 65:
			e[2] |= 0x410000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 66:
			e[2] |= 0x420000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			break;
		case 67:
			e[2] |= 0x430000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 68:
			e[2] |= 0x440000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			break;
		case 69:
			e[2] |= 0x450000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 70:
			e[2] |= 0x460000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[7], 0, 0x20);
				strncpy((char *)&e[7], s, strnlen(s, 31));
				e[15] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[15] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 71:
			e[2] |= 0x470000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 72:
			e[2] |= 0x480000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 73:
			e[2] |= 0x490000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 74:
			e[2] |= 0x4a0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			break;
		case 75:
			e[2] |= 0x4b0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 76:
			e[2] |= 0x4c0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 77:
			e[2] |= 0x4d0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 78:
			e[2] |= 0x4e0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			break;
		case 79:
			e[2] |= 0x4f0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 80:
			e[2] |= 0x500000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			break;
		case 81:
			e[2] |= 0x510000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 82:
			e[2] |= 0x520000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[6], 0, 0x20);
				strncpy((char *)&e[6], s, strnlen(s, 31));
				e[14] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 83:
			e[2] |= 0x530000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 84:
			e[2] |= 0x540000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[7], 0, 0x20);
				strncpy((char *)&e[7], s, strnlen(s, 31));
				e[15] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[15] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 85:
			e[2] |= 0x550000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 86:
			e[2] |= 0x560000u;
			e[4] = va_arg(ap, uint32_t);
			break;
		case 87:
			e[2] |= 0x570000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 88:
			e[2] |= 0x580000u;
			e[4] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				const char *s2 = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[5], 0, 0x20);
				strncpy((char *)&e[5], s, strnlen(s, 31));
				e[13] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[13] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 0;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 0;
					}
				}
				traceCommitEntry(e);
				e = traceAllocEntry(0x800, 53);
				if (e == NULL)
					goto done;
				e[2] |= 0x590000u;
				if (s2 == NULL)
					s2 = nullStr;
				memset(&e[4], 0, 0x20);
				strncpy((char *)&e[4], s2, strnlen(s2, 31));
				e[12] = 0;
				if (strnlen(s2, 256) > 31) {
					const char *p2 = s2 + 31;
					e[12] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p2, strnlen(p2, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p2, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p2 += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p2, strnlen(p2, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 90:
			e[2] |= 0x5a0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 91:
			e[2] |= 0x5b0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 92:
			e[2] |= 0x5c0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 93:
			e[2] |= 0x5d0000u;
			e[4] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[5], 0, 0x20);
				strncpy((char *)&e[5], s, strnlen(s, 31));
				e[13] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[13] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 94:
			e[2] |= 0x5e0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 95:
			e[2] |= 0x5f0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 96:
			e[2] |= 0x600000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 97:
			e[2] |= 0x610000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[10], 0, 0x10);
				strncpy((char *)&e[10], s, strnlen(s, 15));
				e[14] = 0;
				if (strnlen(s, 256) > 15) {
					const char *p = s + 15;
					e[14] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 98:
			e[2] |= 0x620000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 99:
			e[2] |= 0x630000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 100:
			e[2] |= 0x640000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		case 101:
			e[2] |= 0x650000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			{
				const char *s = va_arg(ap, const char *);
				if (s == NULL)
					s = nullStr;
				memset(&e[7], 0, 0x20);
				strncpy((char *)&e[7], s, strnlen(s, 31));
				e[15] = 0;
				if (strnlen(s, 256) > 31) {
					const char *p = s + 31;
					e[15] = 1;
					traceCommitEntry(e);
					e = traceAllocEntry(0x800, 53);
					if (e == NULL)
						goto done;
					e[2] |= 0x7f0000u;
					memset(&e[4], 0, 0x20);
					strncpy((char *)&e[4], p, strnlen(p, 31));
					e[12] = 0;
					e[13] = 1;
					while (strnlen(p, 256) > 31) {
						e[12] = 1;
						e[13] = 0;
						p += 31;
						traceCommitEntry(e);
						e = traceAllocEntry(0x800, 53);
						if (e == NULL)
							goto done;
						e[2] |= 0x7f0000u;
						memset(&e[4], 0, 0x20);
						strncpy((char *)&e[4], p, strnlen(p, 31));
						e[12] = 0;
						e[13] = 1;
					}
				}
			}
			break;
		case 102:
			e[2] |= 0x660000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			break;
		default:
			break;
		}
		traceCommitEntry(e);
done:
		va_end(ap);
	}
	sceKernelCpuResumeIntr(intr);
}
/* 0x81017da8 */
static void traceSuspendHook0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 4) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(4, 49);
	if (e != NULL) {
		e[7] = a4 & ~1u;
		e[4] = a1;
		e[5] = a2;
		e[6] = a3;
		e[2] |= 0x02000000u;
		e[8] = (uint32_t)sceKernelGetThreadId();
		traceCommitEntry(e);
	}
}

/* 0x81017e04 */
static void traceIntrHandlerExitHook(int nargs, ...)
{
	int i;
	uint8_t allow;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x200) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	allow = (uint8_t)sceKernelIsIntrContext();
	e = traceAllocEntry(0x200, 37);
	if (e != NULL) {
		va_start(ap, nargs);
		e[4] = va_arg(ap, uint32_t);
		e[5] = va_arg(ap, uint32_t);
		e[6] = va_arg(ap, uint32_t);
		e[7] = allow;
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x81017e74 */
static void traceIntrHandlerEnterHook(int nargs, ...)
{
	int i;
	int allow;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x200) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	allow = sceKernelIsIntrContext();
	e = traceAllocEntry(0x200, 36);
	if (e != NULL) {
		va_start(ap, nargs);
		e[4] = va_arg(ap, uint32_t);
		e[5] = va_arg(ap, uint32_t);
		e[6] = va_arg(ap, uint32_t);
		e[7] = va_arg(ap, uint32_t);
		e[8] = (uint32_t)allow;
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x81017ee8 */
static void traceIntrExitHook(const uint32_t *info)
{
	int i;
	int allow;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x200) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	allow = sceKernelIsIntrContext();
	e = traceAllocEntry(0x200, 35);
	if (e != NULL) {
		e[4] = info[0];
		e[5] = info[1];
		e[6] = (uint32_t)allow;
		traceCommitEntry(e);
	}
}

/* 0x81017f2c */
static void traceThreadHook(int sub, int nargs, ...)
{
	int i;
	uint32_t tid;
	uint32_t *e;
	va_list ap;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 2) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	tid = (uint32_t)-1;
	if (sceKernelIsIntrContext() == 0) {
		uintptr_t tp;

		__asm__ volatile("mrc p15, 0, %0, c13, c0, 4" : "=r"(tp));
		tid = *(const uint32_t *)(tp + 8);
	}
	e = traceAllocEntry(2, 2);
	if (e != NULL) {
		va_start(ap, nargs);
		switch (sub) {
		case 0:
			e[4] = tid;
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			break;
		case 1:
			e[4] = tid;
			e[2] |= 0x010000u;
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 2:
			e[4] = tid;
			e[2] |= 0x020000u;
			e[6] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			break;
		case 3:
		case 4:
			e[2] |= (uint32_t)sub << 16;
			e[6] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[4] = va_arg(ap, uint32_t);
			break;
		case 6:
		case 8:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			break;
		case 7:
		case 0x0b:
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			e[2] |= (uint32_t)sub << 16;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		case 9:
			e[2] |= 0x090000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			e[10] = va_arg(ap, uint32_t);
			e[11] = va_arg(ap, uint32_t);
			break;
		case 0x0a:
			e[2] |= 0x0a0000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			e[9] = va_arg(ap, uint32_t);
			break;
		case 0x10:
			e[2] |= 0x100000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[8] = va_arg(ap, uint32_t);
			break;
		case 0x11:
			e[2] |= 0x110000u;
			e[4] = va_arg(ap, uint32_t);
			e[5] = va_arg(ap, uint32_t);
			e[7] = va_arg(ap, uint32_t);
			e[6] = va_arg(ap, uint32_t);
			break;
		default:
			break;
		}
		va_end(ap);
		traceCommitEntry(e);
	}
}

/* 0x81018128 */
static void tracePaExitHook(const uint32_t *a1, const uint32_t *a2)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x100) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0x100, 33);
	if (e != NULL) {
		uint32_t tid = (uint32_t)-1;

		if (sceKernelIsIntrContext() == 0) {
			uintptr_t tp;

			__asm__ volatile("mrc p15, 0, %0, c13, c0, 4" : "=r"(tp));
			tid = *(const uint32_t *)(tp + 8);
		}
		e[4] = tid;
		e[6] = a2[0];
		e[7] = a2[1];
		e[9] = a1[21];
		e[8] = a1[22];
		e[5] = a1[12];
		traceCommitEntry(e);
	}
}

/* 0x81018188 */
static void tracePaEnterHook(const uint32_t *a1)
{
	int i;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x100) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	e = traceAllocEntry(0x100, 32);
	if (e != NULL) {
		uint32_t tid = (uint32_t)-1;

		if (sceKernelIsIntrContext() == 0) {
			uintptr_t tp;

			__asm__ volatile("mrc p15, 0, %0, c13, c0, 4" : "=r"(tp));
			tid = *(const uint32_t *)(tp + 8);
		}
		e[4] = tid;
		e[12] = a1[14];
		e[6] = a1[0];
		e[7] = a1[1];
		e[8] = a1[2];
		e[9] = a1[3];
		e[10] = a1[22];
		e[11] = a1[21];
		e[5] = a1[12];
		traceCommitEntry(e);
	}
}

/* 0x810181f0 */
static void traceIntrEnterHook(const uint32_t *info)
{
	int i;
	int allow;
	uint32_t tid;
	uint32_t *e;

	for (i = 0; i < 4; i++) {
		if ((s_traceCpu[i].eventMask & 0x200) == 0 || s_traceCpu[i].enabled == 0)
			return;
	}
	allow = sceKernelIsIntrContext();
	if (allow != 0 || SceExcpmgrForKernel_B615A7DA() != 0) {
		tid = (uint32_t)-1;
	} else {
		uintptr_t tp;

		__asm__ volatile("mrc p15, 0, %0, c13, c0, 4" : "=r"(tp));
		tid = *(const uint32_t *)(tp + 8);
	}
	e = traceAllocEntry(0x200, 34);
	if (e != NULL) {
		e[4] = info[2];
		e[5] = info[4];
		e[6] = info[5];
		e[7] = info[3];
		e[8] = info[0];
		e[9] = tid;
		e[10] = (uint32_t)allow;
		traceCommitEntry(e);
	}
}

/* 0x81018258 */
static int traceCrashDumpHook(int type, SceSize size, void *dump)
{
	const uint32_t *src = (const uint32_t *)dump;
	uint64_t frameBuf[2];
	uint32_t hdr3[14];
	uint32_t hdr1[16];
	int i;

	for (i = 0; i < 4; i++) {
		if (s_traceCpu[i].enabled == 0)
			return 0;
	}
	int intr = sceKernelCpuSuspendIntr();
	uint32_t *e = traceAllocEntry(0, 48);
	if (e == NULL) {
		sceKernelCpuResumeIntr(intr);
		return 0;
	}
	if (type == 1) {
		memcpy(hdr1, src, sizeof(hdr1));
		src += 16;
		e[18] = size;
		e[4] = hdr1[0];
		e[5] = hdr1[1];
		e[6] = hdr1[2];
		e[7] = hdr1[3];
		e[8] = hdr1[4];
		e[9] = hdr1[5];
		e[10] = hdr1[6];
		e[11] = hdr1[7];
		e[12] = hdr1[8];
		e[13] = hdr1[9];
		e[14] = hdr1[10];
		uint32_t kframes = hdr1[11];
		uint32_t uframes = hdr1[12];
		uint32_t fsize = hdr1[13];
		e[15] = kframes;
		e[16] = uframes;
		e[17] = fsize;
		traceCommitEntry(e);
		for (uint32_t j = 0; j < kframes; j++) {
			e = traceAllocEntry(0, 48);
			if (e == NULL)
				goto done;
			memcpy(frameBuf, src, fsize);
			src = (const uint32_t *)((const char *)src + fsize);
			e[2] |= 0x020000u;
			*(u64_alias *)&e[4] = frameBuf[0];
			*(u64_alias *)&e[6] = frameBuf[1];
			traceCommitEntry(e);
		}
		for (uint32_t j = 0; j < uframes; j++) {
			e = traceAllocEntry(0, 48);
			if (e == NULL)
				goto done;
			memcpy(frameBuf, src, fsize);
			src = (const uint32_t *)((const char *)src + fsize);
			e[2] |= 0x020000u;
			*(u64_alias *)&e[4] = frameBuf[0];
			*(u64_alias *)&e[6] = frameBuf[1];
			traceCommitEntry(e);
		}
		goto done;
	}
	if (type == 3) {
		e[2] |= 0x010000u;
		memcpy(hdr3, src, sizeof(hdr3));
		src += 14;
		e[18] = size;
		e[4] = hdr3[0];
		e[5] = hdr3[1];
		e[6] = hdr3[2];
		e[7] = hdr3[3];
		e[8] = hdr3[4];
		e[9] = hdr3[5];
		e[10] = hdr3[6];
		e[11] = hdr3[7];
		e[12] = hdr3[8];
		uint32_t kframes = hdr3[9];
		uint32_t uframes = hdr3[10];
		uint32_t fsize = hdr3[11];
		uint32_t words1 = hdr3[12];
		uint32_t words2 = hdr3[13];
		e[13] = kframes;
		e[14] = uframes;
		e[15] = fsize;
		e[16] = words1;
		e[17] = words2;
		traceCommitEntry(e);
		for (uint32_t j = 0; j < kframes; j++) {
			e = traceAllocEntry(0, 48);
			if (e == NULL)
				goto done;
			memcpy(frameBuf, src, fsize);
			src = (const uint32_t *)((const char *)src + fsize);
			e[2] |= 0x020000u;
			*(u64_alias *)&e[4] = frameBuf[0];
			*(u64_alias *)&e[6] = frameBuf[1];
			traceCommitEntry(e);
		}
		for (uint32_t j = 0; j < uframes; j++) {
			e = traceAllocEntry(0, 48);
			if (e == NULL)
				goto done;
			memcpy(frameBuf, src, fsize);
			src = (const uint32_t *)((const char *)src + fsize);
			e[2] |= 0x020000u;
			*(u64_alias *)&e[4] = frameBuf[0];
			*(u64_alias *)&e[6] = frameBuf[1];
			traceCommitEntry(e);
		}
		if (words1 != 0) {
			const uint32_t *p = src;
			for (uint32_t off = 0; off < words1; off += 12, p += 12) {
				e = traceAllocEntry(0, 48);
				if (e == NULL)
					goto done;
				e[2] |= 0x030000u;
				for (int k = 0; k < 12; k++)
					e[4 + k] = p[k];
				traceCommitEntry(e);
			}
		}
		if (words2 != 0) {
			const uint32_t *base2 = src + words1;
			const uint32_t *p = base2;
			for (uint32_t off = 0; off < words2; off += 12, p += 12) {
				e = traceAllocEntry(0, 48);
				if (e == NULL)
					goto done;
				e[2] |= 0x040000u;
				for (int k = 0; k < 12; k++)
					e[4 + k] = p[k];
				traceCommitEntry(e);
			}
			e = traceAllocEntry(0, 48);
			if (e != NULL) {
				const uint32_t *tail = base2 + words2;
				e[2] |= 0x050000u;
				e[4] = tail[0];
				e[5] = tail[1];
				strncpy((char *)&e[6], (const char *)&tail[2], 0x20);
				traceCommitEntry(e);
			}
		}
		goto done;
	}
	traceCommitEntry(e);
done:
	return sceKernelCpuResumeIntr(intr);
}

static struct SysrootDebugNotifiers {
	uint32_t size;
	void *vipRpc;
	void *veneziaRpc;
	void *syscon;
	void *appMgr;
	void *power[2];
} s_traceSysrootNotifiers = {
	sizeof(s_traceSysrootNotifiers),
	traceVipRpcHook,
	traceVeneziaRpcHook,
	traceSysconHook,
	traceAppMgrHook,
	{ tracePowerHook0, tracePowerHook1 },
};

/* 0x810185ec */
int traceInit(void)
{
	int cpuId = sceKernelCpuId();
	SdbgpCpuState *cpu = sdbgpGetCpuState();
	SdbgpTraceCpuState *tc = &s_traceCpu[cpuId];
	uintptr_t buf = (uintptr_t)cpu->traceBuf;
	int i;

	memset(tc, 0, sizeof(*tc));
	tc->bufSize = SDBGP_TRACE_CPU_SIZE;
	tc->bufStart = buf;
	tc->bufEnd = buf + 1048560;
	tc->eventMask = 0;
	tc->flags = 0;
	tc->ratioMask = 0;
	for (i = 0; i < 4; i++) {
		s_traceSavedReadPtr[i] = 0;
		s_traceSavedSnapshotPtr[i] = 0;
		s_traceSavedWritePtr[i] = 0;
	}
	traceResetBuffers();
	SceSysrootForDriver_FF2DD7AB((struct SysrootDebugNotifiers *)&s_traceSysrootNotifiers);
	return 0;
}

/* 0x81018670 */
int sdbgpCmd90_00(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SdbgpState *state = sdbgpGetState();
	const uint32_t *req = (const uint32_t *)packet;
	uint32_t eventMask;
	uint32_t reqBufSize;
	uint32_t flags;
	uint32_t ratioMask;
	uint32_t replyResult;
	void *base;
	int i;

	if (sdbgpGetRecvSize(cpu) <= 0x27) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	eventMask = req[6];
	reqBufSize = req[7];
	flags = req[8];
	ratioMask = req[9];
	if ((flags & 4) != 0) {
		for (i = 0; i < 4; i++) {
			if (s_traceCpu[i].enabled != 0)
				s_traceCpu[i].enabled = 0;
		}
		for (i = 0; i < 4; i++) {
			s_traceSavedReadPtr[i] = s_traceCpu[i].readPtr;
			s_traceSavedSnapshotPtr[i] = s_traceCpu[i].snapshotPtr;
			s_traceSavedWritePtr[i] = s_traceCpu[i].writePtr;
		}
	} else {
		int fullCount = 0;

		for (i = 0; i < 4; i++) {
			if (s_traceCpu[i].full != 0)
				fullCount++;
		}
		if (fullCount != 0)
			traceResetBuffers();
	}
	if ((flags & 2) != 0) {
		traceResetBuffers();
		if (state->traceMemblock != 0) {
			for (i = 0; i < 4; i++) {
				typedef struct { uint32_t w[20]; } TraceEntry;
				TraceEntry *start = (TraceEntry *)s_traceCpu[i].bufStart;
				TraceEntry *end = (TraceEntry *)s_traceCpu[i].bufEnd;

				if (end - start > 0)
					memset(start, 0, end - start);
			}
		}
	}
	if ((flags & 8) != 0) {
		for (i = 0; i < 4; i++) {
			if (s_traceCpu[i].eventMask != 0) {
				replyResult = 4;
				goto send_reply;
			}
		}
		if (state->traceMemblock != 0 &&
		    sceKernelFreeMemBlock(state->traceMemblock) < 0) {
			replyResult = 6;
			goto send_reply;
		}
		state->traceMemblock = 0;
		state->traceBase = NULL;
		state->traceSize = 0;
		for (i = 0; i < 4; i++) {
			s_traceSavedReadPtr[i] = 0;
			s_traceSavedSnapshotPtr[i] = 0;
			s_traceSavedWritePtr[i] = 0;
			s_traceCpu[i].bufSize = 0;
			s_traceCpu[i].bufStart = 0;
			s_traceCpu[i].bufEnd = 0;
			s_traceCpu[i].eventMask = 0;
			s_traceCpu[i].flags = 0;
			s_traceCpu[i].ratioMask = 0;
		}
	}
	if (state->traceMemblock == 0 && eventMask != 0 && reqBufSize == 0)
		reqBufSize = SDBGP_TRACE_CPU_SIZE;
	if (reqBufSize != 0) {
		for (i = 0; i < 4; i++) {
			if (s_traceCpu[i].eventMask != 0) {
				replyResult = 4;
				goto send_reply;
			}
		}
		if (state->traceSize != reqBufSize * 4) {
			if ((reqBufSize & 0xfffffu) != 0) {
				replyResult = 7;
				goto send_reply;
			}
			for (i = 0; i < 4; i++) {
				if (s_traceCpu[i].enabled != 0)
					s_traceCpu[i].enabled = 0;
			}
			if (state->traceMemblock != 0 &&
			    sceKernelFreeMemBlock(state->traceMemblock) < 0) {
				replyResult = 6;
				goto send_reply;
			}
			SceSize totalSize = reqBufSize * 4;
			SceUID mb = sceKernelAllocMemBlockForKernel("SceDeci4pSDbgpTrace",
							    SceDeci4pSDfMgrForKernel_35238B8A(),
							    totalSize, NULL);
			if (mb < 0) {
				state->traceMemblock = 0;
				state->traceBase = NULL;
				state->traceSize = 0;
				replyResult = 6;
				goto send_reply;
			}
			state->traceMemblock = mb;
			if (sceKernelGetMemBlockBase(mb, &base) < 0) {
				state->traceMemblock = 0;
				state->traceBase = NULL;
				state->traceSize = 0;
				replyResult = 7;
				goto send_reply;
			}
			state->traceSize = totalSize;
			state->traceBase = base;
			uintptr_t p = (uintptr_t)base;
			for (i = 0; i < 4; i++) {
				s_traceSavedReadPtr[i] = 0;
				s_traceSavedSnapshotPtr[i] = 0;
				s_traceSavedWritePtr[i] = 0;
				s_traceCpu[i].bufStart = p;
				s_traceCpu[i].bufEnd = p + (reqBufSize / 80) * 80;
				s_traceCpu[i].bufSize = reqBufSize;
				s_traceCpu[i].eventMask = 0;
				s_traceCpu[i].flags = 1;
				s_traceCpu[i].ratioMask = 0;
				p += reqBufSize;
			}
			traceResetBuffers();
		}
	}
	if (state->traceMemblock == 0) {
		replyResult = 0;
		goto send_reply;
	}
	if (ratioMask != 0) {
		uint32_t weights[4];
		uint32_t weightSum = 0;

		if (ratioMask - 1 > 0xfffeu) {
			replyResult = 6;
			goto send_reply;
		}
		for (i = 0; i < 4; i++) {
			if (s_traceCpu[i].enabled == 1) {
				replyResult = 4;
				goto send_reply;
			}
		}
		for (i = 0; i < 4; i++) {
			weights[i] = (ratioMask >> (12 - i * 4)) & 0xf;
			weightSum += weights[i];
		}
		uint32_t unit = state->traceSize / weightSum;
		uintptr_t offset = 0;
		for (i = 0; i < 4; i++) {
			uint32_t csize = unit * weights[i];

			s_traceCpu[i].bufSize = csize;
			if ((csize & 3) != 0) {
				replyResult = 6;
				goto send_reply;
			}
			uintptr_t start = (uintptr_t)state->traceBase + offset;
			s_traceCpu[i].ratioMask = ratioMask;
			s_traceCpu[i].snapshotPtr = 0;
			s_traceCpu[i].writePtr = start;
			s_traceCpu[i].readPtr = start;
			s_traceCpu[i].bufStart = start;
			s_traceCpu[i].bufEnd = start + (csize / 80) * 80;
			offset += csize;
		}
	}
	for (i = 0; i < 4; i++) {
		if (reqBufSize == 0)
			reqBufSize = s_traceCpu[i].bufSize;
		if (ratioMask == 0)
			ratioMask = s_traceCpu[i].ratioMask;
		s_traceCpu[i].eventMask = eventMask;
		s_traceCpu[i].flags = flags;
		s_traceCpu[i].bufSize = reqBufSize;
		s_traceCpu[i].ratioMask = ratioMask;
		s_traceCpu[i].enabled = (eventMask != 0);
		s_traceCpu[i].ringWrap = ((flags & 1) == 0);
		s_traceCpu[i].snapshotPtr = 0;
	}
	{
		uint32_t *e = traceAllocEntry(0, 61);
		if (e != NULL) {
			e[4] = eventMask;
			e[5] = reqBufSize;
			e[6] = flags;
			e[7] = ratioMask;
			traceCommitEntry(e);
		}
	}
	if ((flags & 4) != 0) {
		for (i = 0; i < 4; i++) {
			s_traceSavedReadPtr[i] = s_traceCpu[i].readPtr;
			s_traceSavedSnapshotPtr[i] = s_traceCpu[i].snapshotPtr;
			s_traceSavedWritePtr[i] = s_traceCpu[i].writePtr;
		}
	}
	if ((eventMask & 0x100) != 0)
		ScePamgrForDriver_A9DD2CB8(tracePaEnterHook, tracePaExitHook);
	else
		ScePamgrForDriver_A9DD2CB8(NULL, NULL);
	SceThreadmgrForKernel_8C07C8EC((eventMask & 1) ? (ThreadmgrTraceHook)traceSwitchHook : NULL);
	SceThreadmgrForKernel_F55FDEFB((eventMask & 0x20) ? (ThreadmgrTraceHook)traceSyncHook : NULL);
	SceThreadmgrForKernel_E2DACBC2((eventMask & 2) ? (ThreadmgrTraceHook)traceThreadHook : NULL);
	if ((eventMask & 0x200) != 0) {
		SceIntrmgrForKernel_8D6E1289(traceIntrEnterHook, traceIntrExitHook);
		SceIntrmgrForKernel_04DBA732(traceIntrHandlerEnterHook, traceIntrHandlerExitHook);
		SceThreadmgrForKernel_ABBE9BA5((ThreadmgrTraceHook)traceAlarmHook);
	} else {
		SceIntrmgrForKernel_8D6E1289(NULL, NULL);
		SceIntrmgrForKernel_04DBA732(NULL, NULL);
		SceThreadmgrForKernel_ABBE9BA5(NULL);
	}
	SceIofilemgrForDriver_F4F8C59A((eventMask & 0x800) ? traceIofileHook : NULL);
	if ((eventMask & 4) != 0) {
		SceKernelSuspendForDriver_D6124071(traceSuspendHook0);
		SceKernelSuspendForDriver_B5C58EE8(traceSuspendHook1);
		SceKernelSuspendForDriver_F2B07167(traceSuspendHook2);
		SceKernelSuspendForDriver_8B3F02B8(traceSuspendHook3);
		SceKernelSuspendForDriver_4E5A3A23(traceSuspendHook4);
		SceKernelSuspendForDriver_C00826AC(traceSuspendHook5);
		SceKernelSuspendForDriver_0DE3CC02(traceSuspendHook6);
	} else {
		SceKernelSuspendForDriver_D6124071(NULL);
		SceKernelSuspendForDriver_B5C58EE8(NULL);
		SceKernelSuspendForDriver_F2B07167(NULL);
		SceKernelSuspendForDriver_8B3F02B8(NULL);
		SceKernelSuspendForDriver_4E5A3A23(NULL);
		SceKernelSuspendForDriver_C00826AC(NULL);
		SceKernelSuspendForDriver_0DE3CC02(NULL);
	}
	if ((eventMask & 0x1000) != 0) {
		SceDebugLedForDriver_F62154E7(traceDebugLedHook1);
		SceDebugLedForDriver_24173819(traceDebugLedHook0);
	} else {
		SceDebugLedForDriver_F62154E7(NULL);
		SceDebugLedForDriver_24173819(NULL);
	}
	replyResult = 0;
	SceExcpmgrForKernel_58F7212B((SceExcpmgrCallback)tracePabtExcpHook);
	SceExcpmgrForKernel_9EE59C6E((SceExcpmgrCallback)traceDabtExcpHook);
	SceExcpmgrForKernel_1FBF5654((SceExcpmgrCallback)traceUndefExcpHook);
	SceThreadmgrForKernel_10853CCD((ThreadmgrTraceHook)traceVfpExcpHook);
	SceDebugForKernel_4C62F8CC(traceCrashDumpHook);

send_reply:
	{
		void *sendBuf = sdbgpAllocSendBuf(cpu, sizeof(SdbgpHeader));
		if (sendBuf == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		SceDeci4pSDfMgrForKernel_8A3B06F2(sendBuf, 0, sizeof(SdbgpHeader));
		sdbgpSetSendSize(cpu, sizeof(SdbgpHeader));
		sdbgpSendErrorHeader(cpu, packet->group, 1, 0, replyResult);
		return 0;
	}
}

/* 0x81018d40 */
int sdbgpCmd90_02(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SdbgpState *state = sdbgpGetState();
	uint32_t *sendBuf;

	if (sdbgpGetRecvSize(cpu) <= 0x13) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	sendBuf = (uint32_t *)sdbgpAllocSendBuf(cpu, 0x30);
	if (sendBuf == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(sendBuf, 0, 0x30);
	sdbgpSetSendSize(cpu, 0x30);
	sendBuf[7] = 20;
	sendBuf[8] = s_traceCpu[3].eventMask;
	sendBuf[9] = state->traceSize >> 2;
	sendBuf[10] = s_traceCpu[3].flags;
	sendBuf[11] = s_traceCpu[3].ratioMask;
	sdbgpSendErrorHeader(cpu, packet->group, 3, 0, 0);
	return 0;
}

/* 0x81018de4 */
int sdbgpCmd90_04(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SdbgpState *state = sdbgpGetState();
	uint8_t *workBuf;
	SceSize totalSize = 32;
	SceSize payloadEnd = 28;
	int i;

	s_traceLastReadTimestamp = 0;
	if (sdbgpGetRecvSize(cpu) <= 0x13) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	workBuf = (uint8_t *)sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	SceDeci4pSDfMgrForKernel_8A3B06F2(workBuf, 0, SDBGP_PACKET_MAX);
	if (state->traceMemblock != 0) {
		uint32_t emptyMask = 0;

		for (i = 0; i < 4; i++) {
			if (s_traceCpu[i].writePtr == s_traceCpu[i].readPtr &&
			    s_traceCpu[i].full != 1)
				emptyMask |= 1u << i;
		}
		if (emptyMask != 15) {
			for (i = 0; i < 4; i++) {
				if (s_traceCpu[i].snapshotPtr == 0) {
					uint32_t *e = traceAllocEntry(0, 60);
					if (e != NULL) {
						traceCommitEntry(e);
					} else if (s_traceCpu[i].full == 0) {
						continue;
					}
					s_traceCpu[i].snapshotPtr = s_traceCpu[i].writePtr;
				}
			}
			uint8_t *dst = workBuf;
			uint32_t prevLo = 0;
			uint32_t prevHi = 0;
			int firstEntry = 1;
			while (1) {
				uint32_t doneMask = 0;
				for (i = 0; i < 4; i++) {
					if (s_traceCpu[i].writePtr == s_traceCpu[i].readPtr &&
					    s_traceCpu[i].full != 1)
						doneMask |= 1u << i;
				}
				if (doneMask == 15) {
					totalSize = payloadEnd + 4;
					goto finish;
				}
				uint64_t bestTs = UINT64_MAX;
				int bestCpu = 0;
				for (i = 0; i < 4; i++) {
					if (s_traceCpu[i].readPtr != s_traceCpu[i].writePtr ||
					    s_traceCpu[i].full == 1) {
						uint64_t ts = *(const uint64_t *)s_traceCpu[i].readPtr;
						if (i == 0 || bestTs == UINT64_MAX || ts < bestTs) {
							bestTs = ts;
							bestCpu = i;
						}
					}
				}
				if (bestTs == UINT64_MAX) {
					totalSize = payloadEnd + 4;
					goto finish;
				}
				SdbgpTraceCpuState *bc = &s_traceCpu[bestCpu];
				if (bc->lastTimestamp < s_traceLastReadTimestamp) {
					totalSize = payloadEnd + 4;
					goto finish;
				}
				const uint32_t *e = (const uint32_t *)bc->readPtr;
				uint32_t tsLo = e[0];
				uint32_t tsHi = e[1];
				s_traceLastReadTimestamp = ((uint64_t)tsHi << 32) | tsLo;
				if (bestCpu == 4) {
					totalSize = payloadEnd + 4;
					goto finish;
				}
				uint32_t evType = e[2];
				uint8_t hdrBuf[12];
				uint8_t recBuf[64];
				memset(hdrBuf, 0, sizeof(hdrBuf));
				memset(recBuf, 0, sizeof(recBuf));
				uint8_t hdrFlags;
				uint32_t hdrLen;
				if (tsHi == prevHi && !firstEntry) {
					int32_t diff = (int32_t)(tsLo - prevLo);
					if (diff < 0) {
						*(u16_alias *)&hdrBuf[2] = 0;
						hdrFlags = 0x40;
						hdrLen = 4;
					} else if (diff < 0x10000) {
						*(u16_alias *)&hdrBuf[2] = (uint16_t)diff;
						hdrFlags = 0x40;
						hdrLen = 4;
					} else {
						*(u16_alias *)&hdrBuf[2] = 0;
						*(u32_alias *)&hdrBuf[4] = (uint32_t)diff;
						hdrFlags = 0x80;
						hdrLen = 8;
					}
				} else {
					*(u16_alias *)&hdrBuf[2] = 0;
					*(u32_alias *)&hdrBuf[4] = tsLo;
					*(u32_alias *)&hdrBuf[8] = tsHi;
					hdrFlags = 0xc0;
					hdrLen = 12;
				}
				uint32_t bodyLen = 0;
				uint16_t sub = (uint16_t)(evType >> 16);
				recBuf[0] = (uint8_t)e[3];
				*(u16_alias *)&recBuf[2] = sub;
				switch ((int16_t)evType) {
				case 0:
					bodyLen = 12;
					recBuf[0] = (uint8_t)(e[3] | ((evType >> 16) & 0xff) * 16);
					recBuf[1] = (uint8_t)e[5];
					recBuf[2] = (uint8_t)e[7];
					recBuf[3] = (uint8_t)e[8];
					*(u32_alias *)&recBuf[4] = e[4];
					*(u32_alias *)&recBuf[8] = e[6];
					break;
				case 1:
					switch (sub) {
					case 0x00:
					case 0x01:
					case 0x04:
					case 0x05:
					case 0x12:
					case 0x17:
					case 0x18:
					case 0x19:
					case 0x1e:
					case 0x22:
					case 0x23:
					case 0x30:
					case 0x35:
					case 0x36:
						bodyLen = 4;
						break;
					case 0x1f:
						bodyLen = 4;
						recBuf[1] = (uint8_t)e[4];
						break;
					case 0x16:
					case 0x34:
						bodyLen = 8;
						*(u32_alias *)&recBuf[4] = e[4];
						break;
					case 0x03:
					case 0x0a:
					case 0x21:
					case 0x28:
						bodyLen = 8;
						recBuf[1] = (uint8_t)e[5];
						*(u32_alias *)&recBuf[4] = e[4];
						break;
					case 0x15:
					case 0x33:
						bodyLen = 8;
						recBuf[1] = (uint8_t)e[4];
						*(u32_alias *)&recBuf[4] = e[5];
						break;
					case 0x0b:
					case 0x0c:
					case 0x29:
					case 0x2a:
						bodyLen = 12;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						break;
					case 0x14:
					case 0x32:
						bodyLen = 12;
						recBuf[1] = (uint8_t)e[5];
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[6];
						break;
					case 0x02:
					case 0x07:
					case 0x08:
					case 0x09:
					case 0x0d:
					case 0x1a:
					case 0x20:
					case 0x25:
					case 0x26:
					case 0x27:
					case 0x2b:
						bodyLen = 16;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						*(u32_alias *)&recBuf[12] = e[6];
						break;
					case 0x0e:
					case 0x2c:
						bodyLen = 16;
						recBuf[1] = (uint8_t)e[7];
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						*(u32_alias *)&recBuf[12] = e[6];
						break;
					case 0x0f:
					case 0x2d:
						bodyLen = 16;
						recBuf[1] = (uint8_t)e[6];
						recBuf[4] = (uint8_t)e[7];
						*(u32_alias *)&recBuf[8] = e[4];
						*(u32_alias *)&recBuf[12] = e[5];
						break;
					case 0x13:
					case 0x31:
						bodyLen = 20;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						*(u32_alias *)&recBuf[12] = e[6];
						*(u32_alias *)&recBuf[16] = e[7];
						break;
					default:
						totalSize = payloadEnd + 4;
						goto finish;
					}
					break;
				case 2:
					switch (sub) {
					case 0x00:
						bodyLen = 20;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5] & ~1u;
						*(u32_alias *)&recBuf[12] = e[6];
						*(u32_alias *)&recBuf[16] = e[7];
						break;
					case 0x01:
					case 0x03:
					case 0x04:
					case 0x07:
					case 0x0b:
					case 0x0c:
					case 0x0d:
					case 0x0e:
					case 0x0f:
						bodyLen = 16;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						*(u32_alias *)&recBuf[12] = e[6];
						break;
					case 0x02:
					case 0x10:
						bodyLen = 24;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						*(u32_alias *)&recBuf[12] = e[6];
						*(u32_alias *)&recBuf[16] = e[7];
						*(u32_alias *)&recBuf[20] = e[8];
						break;
					case 0x06:
					case 0x08:
						bodyLen = 8;
						*(u32_alias *)&recBuf[4] = e[4];
						break;
					case 0x09:
						bodyLen = 36;
						for (int k = 0; k < 8; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x0a:
						bodyLen = 28;
						for (int k = 0; k < 6; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x11:
						bodyLen = 20;
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					default:
						totalSize = payloadEnd + 4;
						goto finish;
					}
					break;
				case 32:
					bodyLen = 40;
					*(u16_alias *)&recBuf[2] = 0;
					for (int k = 0; k < 8; k++)
						*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
					*(u32_alias *)&recBuf[36] = e[12] & ~1u;
					break;
				case 33:
					bodyLen = 28;
					*(u16_alias *)&recBuf[2] = 0;
					for (int k = 0; k < 6; k++)
						*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
					break;
				case 34:
					bodyLen = 20;
					recBuf[0] = (uint8_t)((e[3] & 3) | ((uint8_t)e[10] << 4));
					recBuf[1] = (uint8_t)e[6];
					*(u16_alias *)&recBuf[2] = (uint16_t)e[4];
					*(u32_alias *)&recBuf[4] = e[5];
					*(u32_alias *)&recBuf[8] = e[7] & ~1u;
					*(u32_alias *)&recBuf[12] = e[8];
					*(u32_alias *)&recBuf[16] = e[9];
					break;
				case 35:
					bodyLen = 8;
					recBuf[0] = (uint8_t)((e[3] & 3) | ((uint8_t)e[6] << 4));
					*(u16_alias *)&recBuf[2] = (uint16_t)e[5];
					*(u32_alias *)&recBuf[4] = e[4];
					break;
				case 36:
					bodyLen = 16;
					recBuf[0] = (uint8_t)((e[3] & 3) | ((uint8_t)e[8] << 4));
					*(u16_alias *)&recBuf[2] = (uint16_t)e[4];
					*(u32_alias *)&recBuf[4] = e[5];
					*(u32_alias *)&recBuf[8] = e[6];
					*(u32_alias *)&recBuf[12] = e[7];
					break;
				case 37:
					bodyLen = 12;
					recBuf[0] = (uint8_t)((e[3] & 3) | ((uint8_t)e[7] << 4));
					*(u16_alias *)&recBuf[2] = (uint16_t)e[4];
					*(u32_alias *)&recBuf[4] = e[5];
					*(u32_alias *)&recBuf[8] = e[6];
					break;
				case 38:
					bodyLen = 24;
					*(u16_alias *)&recBuf[2] = 0;
					for (int k = 0; k < 5; k++)
						*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
					break;
				case 40:
					switch (sub) {
					case 0x05:
					case 0x0b:
					case 0x24:
					case 0x3d:
					case 0x4a:
					case 0x54:
					case 0x09:
					case 0x12:
					case 0x4c:
					case 0x56:
					case 0x0c:
					case 0x25:
					case 0x4b:
					case 0x55:
					case 0x2c:
					case 0x3c:
					case 0x49:
					case 0x53:
						bodyLen = 12;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						break;
					case 0x04:
					case 0x13:
					case 0x1c:
					case 0x22:
					case 0x27:
					case 0x43:
					case 0x50:
					case 0x57:
					case 0x5a:
					case 0x5d:
					case 0x0d:
					case 0x14:
					case 0x1d:
					case 0x1f:
					case 0x26:
					case 0x2a:
					case 0x2d:
					case 0x38:
					case 0x3a:
					case 0x3f:
					case 0x41:
					case 0x42:
					case 0x44:
					case 0x48:
					case 0x4e:
					case 0x4f:
					case 0x51:
					case 0x52:
					case 0x58:
					case 0x59:
					case 0x5b:
					case 0x5c:
					case 0x5e:
					case 0x5f:
						bodyLen = 16;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						*(u32_alias *)&recBuf[12] = e[6];
						break;
					case 0x02:
					case 0x15:
					case 0x2e:
					case 0x30:
					case 0x31:
					case 0x3e:
					case 0x0a:
					case 0x2f:
					case 0x36:
					case 0x4d:
						bodyLen = 20;
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x06:
					case 0x37:
					case 0x39:
					case 0x3b:
						bodyLen = 24;
						for (int k = 0; k < 5; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x10:
					case 0x2b:
					case 0x47:
						bodyLen = 28;
						for (int k = 0; k < 6; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x03:
					case 0x45:
						bodyLen = 32;
						for (int k = 0; k < 7; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x46:
						bodyLen = 32;
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x40:
						bodyLen = 40;
						for (int k = 0; k < 9; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x11:
						bodyLen = 52;
						for (int k = 0; k < 12; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					default:
						goto next_entry;
					}
					break;
				case 41:
					if (sub == 0) {
						bodyLen = 48;
						recBuf[1] = (uint8_t)e[4];
						for (int k = 0; k < 11; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[5 + k];
					} else if (sub == 1) {
						bodyLen = 16;
						recBuf[1] = (uint8_t)e[4];
						for (int k = 0; k < 3; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[5 + k];
					} else {
						goto next_entry;
					}
					break;
				case 48:
					switch (sub) {
					case 0:
					case 1:
						bodyLen = 64;
						*(u32_alias *)&recBuf[4] = e[18];
						for (int k = 0; k < 14; k++)
							*(u32_alias *)&recBuf[8 + k * 4] = e[4 + k];
						break;
					case 2:
						bodyLen = 20;
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 3:
					case 4:
						bodyLen = 52;
						for (int k = 0; k < 12; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 5:
						bodyLen = 44;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						strncpy((char *)&recBuf[12], (const char *)&e[6], 0x20);
						break;
					default:
						goto next_entry;
					}
					break;
				case 49:
					*(u16_alias *)&recBuf[2] = 0;
					recBuf[1] = (uint8_t)(evType >> 24);
					switch (evType >> 24) {
					case 0:
					case 5:
						bodyLen = 16;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						*(u32_alias *)&recBuf[12] = e[6];
						break;
					case 1:
					case 3:
					case 6:
						bodyLen = 20;
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 2:
					case 4:
					case 8:
						bodyLen = 24;
						for (int k = 0; k < 5; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 7:
						bodyLen = 20;
						*(u32_alias *)&recBuf[4] = e[5];
						*(u32_alias *)&recBuf[8] = e[4];
						*(u32_alias *)&recBuf[12] = e[6];
						*(u32_alias *)&recBuf[16] = e[7];
						break;
					default:
						goto next_entry;
					}
					break;
				case 51:
				case 58:
				case 61:
					bodyLen = 20;
					*(u16_alias *)&recBuf[2] = 0;
					for (int k = 0; k < 4; k++)
						*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
					break;
				case 52:
					switch (sub) {
					case 0:
					case 1:
					case 2:
						bodyLen = 44;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						memcpy(&recBuf[12], &e[6], 32);
						break;
					case 3:
						bodyLen = 40;
						*(u32_alias *)&recBuf[4] = e[4];
						memcpy(&recBuf[8], &e[5], 32);
						break;
					case 4:
						bodyLen = 12;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						break;
					case 5:
						bodyLen = 20;
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 6:
						bodyLen = 8;
						*(u32_alias *)&recBuf[4] = e[4];
						break;
					default:
						goto next_entry;
					}
					break;
				case 53:
					switch (sub) {
					case 0x00:
						bodyLen = 52;
						recBuf[1] = (uint8_t)e[16];
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						strncpy((char *)&recBuf[20], (const char *)&e[8], 0x20);
						break;
					case 0x01:
					case 0x03:
					case 0x05:
					case 0x07:
					case 0x09:
					case 0x0d:
					case 0x11:
					case 0x15:
					case 0x1d:
					case 0x1f:
					case 0x21:
					case 0x23:
					case 0x25:
					case 0x29:
					case 0x2b:
					case 0x2d:
					case 0x2f:
					case 0x33:
					case 0x35:
					case 0x37:
					case 0x38:
					case 0x39:
					case 0x3b:
					case 0x3d:
					case 0x3f:
					case 0x41:
					case 0x43:
					case 0x45:
					case 0x47:
					case 0x49:
					case 0x4b:
					case 0x4d:
					case 0x4f:
					case 0x51:
					case 0x53:
					case 0x55:
					case 0x57:
					case 0x5a:
					case 0x5b:
					case 0x5c:
					case 0x5e:
					case 0x5f:
					case 0x60:
					case 0x62:
					case 0x63:
					case 0x64:
					case 0x66:
						bodyLen = 12;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						break;
					case 0x02:
					case 0x1e:
					case 0x20:
					case 0x22:
					case 0x28:
					case 0x2a:
					case 0x2c:
					case 0x32:
					case 0x52:
						bodyLen = 44;
						recBuf[1] = (uint8_t)e[14];
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[5];
						strncpy((char *)&recBuf[12], (const char *)&e[6], 0x20);
						break;
					case 0x04:
					case 0x3a:
					case 0x50:
						bodyLen = 20;
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x06:
					case 0x3c:
					case 0x56:
						bodyLen = 8;
						*(u32_alias *)&recBuf[4] = e[4];
						break;
					case 0x08:
					case 0x0c:
					case 0x18:
					case 0x27:
					case 0x4e:
						bodyLen = 28;
						for (int k = 0; k < 6; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x0a:
					case 0x0e:
					case 0x10:
					case 0x14:
					case 0x42:
						bodyLen = 36;
						for (int k = 0; k < 8; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x0b:
					case 0x0f:
					case 0x13:
					case 0x17:
					case 0x19:
					case 0x1b:
					case 0x48:
					case 0x4c:
						bodyLen = 16;
						for (int k = 0; k < 3; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x12:
					case 0x16:
						bodyLen = 44;
						for (int k = 0; k < 10; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x1a:
					case 0x4a:
						bodyLen = 24;
						for (int k = 0; k < 5; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x1c:
					case 0x46:
					case 0x54:
					case 0x65:
						bodyLen = 48;
						recBuf[1] = (uint8_t)e[15];
						for (int k = 0; k < 3; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						strncpy((char *)&recBuf[16], (const char *)&e[7], 0x20);
						break;
					case 0x24:
						bodyLen = 32;
						recBuf[1] = (uint8_t)e[11];
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[6];
						*(u32_alias *)&recBuf[12] = e[5];
						strncpy((char *)&recBuf[16], (const char *)&e[7], 0x10);
						break;
					case 0x26:
						bodyLen = 24;
						recBuf[1] = (uint8_t)e[9];
						*(u32_alias *)&recBuf[4] = e[4];
						strncpy((char *)&recBuf[8], (const char *)&e[5], 0x10);
						break;
					case 0x2e:
					case 0x34:
					case 0x58:
					case 0x5d:
						bodyLen = 40;
						recBuf[1] = (uint8_t)e[13];
						*(u32_alias *)&recBuf[4] = e[4];
						strncpy((char *)&recBuf[8], (const char *)&e[5], 0x20);
						break;
					case 0x30:
					case 0x31:
					case 0x59:
						bodyLen = 36;
						recBuf[1] = (uint8_t)e[12];
						strncpy((char *)&recBuf[4], (const char *)&e[4], 0x20);
						break;
					case 0x36:
						bodyLen = 20;
						*(u32_alias *)&recBuf[4] = e[4];
						*(u32_alias *)&recBuf[8] = e[6];
						*(u32_alias *)&recBuf[12] = e[7];
						*(u32_alias *)&recBuf[16] = e[8];
						break;
					case 0x3e:
						bodyLen = 36;
						recBuf[1] = (uint8_t)e[12];
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						strncpy((char *)&recBuf[20], (const char *)&e[8], 0x10);
						break;
					case 0x40:
						bodyLen = 32;
						recBuf[1] = (uint8_t)e[11];
						for (int k = 0; k < 3; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						strncpy((char *)&recBuf[16], (const char *)&e[7], 0x10);
						break;
					case 0x44:
						bodyLen = 32;
						for (int k = 0; k < 7; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						break;
					case 0x61:
						bodyLen = 44;
						recBuf[1] = (uint8_t)e[14];
						for (int k = 0; k < 6; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
						strncpy((char *)&recBuf[28], (const char *)&e[10], 0x10);
						break;
					case 0x7f:
						bodyLen = 40;
						recBuf[1] = (uint8_t)e[12];
						*(u32_alias *)&recBuf[4] = e[13];
						strncpy((char *)&recBuf[8], (const char *)&e[4], 0x20);
						break;
					default:
						goto next_entry;
					}
					break;
				case 54:
				case 55:
					bodyLen = 36;
					*(u16_alias *)&recBuf[2] = 0;
					for (int k = 0; k < 8; k++)
						*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
					break;
				case 56:
					bodyLen = 28;
					*(u16_alias *)&recBuf[2] = 0;
					for (int k = 0; k < 6; k++)
						*(u32_alias *)&recBuf[4 + k * 4] = e[4 + k];
					break;
				case 57:
					switch (sub) {
					case 0:
					case 5:
						bodyLen = 8;
						*(u32_alias *)&recBuf[4] = e[4];
						break;
					case 1:
						bodyLen = 20;
						recBuf[1] = (uint8_t)e[4];
						for (int k = 0; k < 4; k++)
							*(u32_alias *)&recBuf[4 + k * 4] = e[5 + k];
						break;
					case 2:
					case 3:
					case 4:
						bodyLen = 4;
						break;
					default:
						totalSize = payloadEnd + 4;
						goto finish;
					}
					break;
				case 59:
					if (sub == 0) {
						bodyLen = 48;
						recBuf[1] = (uint8_t)e[15];
						*(u32_alias *)&recBuf[4] = e[12];
						*(u32_alias *)&recBuf[8] = e[13];
						*(u32_alias *)&recBuf[12] = e[14];
						strncpy((char *)&recBuf[16], (const char *)&e[4], 0x20);
					} else if (sub == 1) {
						bodyLen = 40;
						recBuf[1] = (uint8_t)e[12];
						*(u32_alias *)&recBuf[4] = e[13];
						strncpy((char *)&recBuf[8], (const char *)&e[4], 0x20);
					} else {
						goto next_entry;
					}
					break;
				case 60:
				case 62:
					bodyLen = 4;
					*(u16_alias *)&recBuf[2] = 0;
					break;
				default:
					totalSize = payloadEnd + 4;
					goto finish;
				}
				{
					uint32_t recTotal = bodyLen + hdrLen;
					hdrBuf[0] = (uint8_t)recTotal;
					hdrBuf[1] = (uint8_t)(hdrFlags | (uint8_t)evType);
					if (payloadEnd + hdrLen + bodyLen > 0x10ffcu) {
						totalSize = payloadEnd + 4;
						SceDeci4pSDfMgrForKernel_8A3B06F2(dst, 0, 4);
						goto finish;
					}
					payloadEnd += recTotal;
					SceDeci4pSDfMgrForKernel_5996E2DB(dst, hdrBuf, hdrLen);
					SceDeci4pSDfMgrForKernel_5996E2DB(dst + hdrLen, recBuf, bodyLen);
					dst += recTotal;
					prevLo = tsLo;
					prevHi = tsHi;
					firstEntry = 0;
				}
next_entry:
				{
					uintptr_t nextRp = (uintptr_t)(e + 20);
					if (nextRp >= bc->bufEnd)
						nextRp = bc->bufStart;
					if (bc->snapshotPtr == nextRp)
						bc->snapshotPtr = 0;
					bc->readPtr = nextRp;
				}
			}
		} else {
			for (i = 0; i < 4; i++) {
				if (s_traceSavedReadPtr[i] != 0 && s_traceSavedWritePtr[i] != 0) {
					s_traceCpu[i].writePtr = s_traceSavedWritePtr[i];
					s_traceCpu[i].readPtr = s_traceSavedReadPtr[i];
					s_traceCpu[i].snapshotPtr = s_traceSavedSnapshotPtr[i];
				}
			}
		}
	}

finish:
	for (i = 0; i < 4; i++) {
		if (s_traceCpu[i].full != 0)
			s_traceCpu[i].full = 2;
	}
	{
		uint8_t *sendBuf = (uint8_t *)sdbgpAllocSendBuf(cpu, totalSize);
		if (sendBuf == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		SceDeci4pSDfMgrForKernel_8A3B06F2(sendBuf + sizeof(SdbgpHeader), 0, payloadEnd - 24);
		sdbgpSetSendSize(cpu, totalSize);
		SceDeci4pSDfMgrForKernel_5996E2DB(sendBuf + sizeof(SdbgpHeader), workBuf, payloadEnd - 24);
		sdbgpSendErrorHeader(cpu, packet->group, 5, 0, 0);
		return 0;
	}
}
