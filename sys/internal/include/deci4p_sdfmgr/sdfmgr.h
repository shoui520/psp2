/*
 * SceDeci4pSDfMgr (os0:kd/deci4p_sdfmgr.skprx): the DECI4P manager usable from
 * any context (polling, per-CPU), as used so far.
 */

#pragma once

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
int SceDeci4pSDfMgrForKernel_86CFC8C6(int handle);					/* close */
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
