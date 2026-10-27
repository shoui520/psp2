/*
 * SceDeci4pSDfMgr (os0:kd/deci4p_sdfmgr.skprx): the DECI4P manager usable from
 * any context (polling, per-CPU), as used so far.
 */

#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <sysmem/debug.h>
#include <deci4p_dfmgr/deci4p.h>

/* the manager's errors (names unknown) */
#define SDFMGR_ERROR_INVALID_ARGUMENT	((int)0x80090001)
#define SDFMGR_ERROR_NO_SLOT		((int)0x80090003)
#define SDFMGR_ERROR_BUSY		((int)0x80090007)

/* protocol registration (sceSDfMgrOpen) */
typedef struct SceSDfMgrProtocol {
	SceSize size;				/* sizeof(SceSDfMgrProtocol) */
	SceUInt32 protocol;
	SceUInt32 alias;			/* as in SceDfMgrProtocol */
	SceUInt32 version;
	SceUInt32 subVersion;
	union {
		SceUInt32 destinationAndFlags;
		struct {
			SceUInt16 destination;
			SceUInt16 flags;
		};
	};
	const char *name;
} SceSDfMgrProtocol;

typedef void (*SceSDfMgrHandler)(int event, void *arg, void *ctx);

int sceSDfMgrExRecv(int handle, void *buf, SceSize size);
int sceSDfMgrExSend(int handle, const void *buf, SceSize size);
int sceSDfMgrPoll(void);
int _threadSignal(int handle);

/* names unknown (NID) */
int sceSDfMgrOpen(SceSDfMgrHandler handler, const SceSDfMgrProtocol *protocol,
				       void *ctx);					/* open */
void SceDeci4pSDfMgrForKernel_8A3B06F2(void *dst, int c, SceSize size);			/* memset */
SceSize SceDeci4pSDfMgrForKernel_68903C1D(const SceDeci4pHeader *hdr);			/* get size */
void SceDeci4pSDfMgrForKernel_FFC95B63(SceDeci4pHeader *hdr, SceSize size);		/* set size */
int SceDeci4pSDfMgrForKernel_501CE61D(SceUInt32 level, const SceKernelDebugInfo *info,
				       const char *fmt, ...);				/* printf */
int sceSDfMgrClose(int handle);					/* close */
int SceDeci4pSDfMgrForKernel_B22B42FB(void (*hook)(void));				/* RegisterBreakHook */
int SceDeci4pSDfMgrForKernel_E53400AE(void (*hook)(void));				/* UnregisterBreakHook */
void SceDeci4pSDfMgrForKernel_98AEC60E(void);						/* poll */
SceUInt32 SceDeci4pSDfMgrForKernel_35238B8A(void);					/* a memory type */
int sceSDfMgrExReqSend(int handle);
int sceSDfMgrExClose(int handle);
int sceSDfMgrExOpen(SceSDfMgrHandler handler, const SceSDfMgrProtocol *protocol,
		   void *context);

/* system break / continue for the debugger (os0:kd/deci4p_sdeci2p.skprx) */
int sceSDfMgrBreakSystem(void);
int sceSDfMgrContinueSystem(void);
int SceDeci4pSDfMgrForKernel_3A25594F(void (*control)(int id));		/* RegisterControlComponent */
int SceDeci4pSDfMgrForKernel_59C077B3(void (*control)(int id));		/* UnregisterControlComponent */

/* SDfMgr step modes (SdfMgrDebugCpuState.stepMode) */
#define SDFMGR_STEP_ARM			0x01	/* break when leaving pc */
#define SDFMGR_STEP_THUMB		0x02	/* break at stepAddress */
#define SDFMGR_STEP_ARM_POLL		0x10	/* also ends the SDIO poll loop */
#define SDFMGR_STEP_THUMB_POLL		0x20

/* the debugger's per-core break records (deci4p_sdbgp allocates them; the rest is its own) */
typedef struct SdfMgrDebugCpuState {
	uint8_t reserved0[0x18c];
	SceUID pid;				/* of the stopped thread */
	SceUID tid;
	uint32_t pc;				/* where it stopped */
	uint32_t reserved1;
	uint32_t stepMode;			/* SDFMGR_STEP_* */
	uint8_t reserved2[0x10];
	uint32_t stepAddress;			/* Thumb step; bit 1: the upper halfword */
	uint8_t reserved3[0x1dc];
} SdfMgrDebugCpuState;

SdfMgrDebugCpuState **SceDeci4pSDfMgrForKernel_BFB482D8(void);
void SceDeci4pSDfMgrForKernel_4EA65698(uint32_t value);
uint32_t SceDeci4pSDfMgrForKernel_C38EA67A(void);
uint32_t SceDeci4pSDfMgrForKernel_0A6437B6(void);
int SceDeci4pSDfMgrForKernel_682D0EFB(void);
int SceDeci4pSDfMgrForKernel_58D7597A(void);
int SceDeci4pSDfMgrForKernel_6BF5553C(void (*hook)(void), unsigned index);
void *SceDeci4pSDfMgrForKernel_5996E2DB(void *destination, const void *source,
				   SceSize size);					/* memcpy */
