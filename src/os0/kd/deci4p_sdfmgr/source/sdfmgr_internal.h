/* SDfMgr state shared by C and the exception/interrupt entry veneers. */
#pragma once
#include "sce/cp15.h"
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <sce/intrmgr.h>
#include <sce/sdfmgr.h>

typedef void (*SdfMgrHook)(void);
typedef void (*SdfMgrControl)(int id);
typedef int (*SdfMgrPrintHandler)(void *context, uint32_t reserved,
				SceUInt32 level, const SceKernelDebugInfo *info,
				const char *format, va_list args);

struct SdfMgrProtocolState;
struct SdfMgrTransport;

typedef struct SdfMgrTransportOperations {
	int (*beginReceive)(struct SdfMgrTransport *transport);
	int (*receive)(struct SdfMgrTransport *transport, void *buffer, SceSize size);
	int (*finishReceive)(struct SdfMgrTransport *transport);
	int (*beginSend)(struct SdfMgrTransport *transport);
	int (*send)(struct SdfMgrTransport *transport, const void *buffer, SceSize size);
	int (*reset)(struct SdfMgrTransport *transport);
	int (*poll)(struct SdfMgrTransport *transport);
	int (*processReceive)(struct SdfMgrTransport *transport);
	int (*pauseReceive)(struct SdfMgrTransport *transport);
	int (*resumeReceive)(struct SdfMgrTransport *transport);
	int (*pause)(struct SdfMgrTransport *transport);
	int (*resume)(struct SdfMgrTransport *transport);
	SdfMgrHook reserved;			/* only cleared */
	SdfMgrHook idle;
	void (*disableInterrupts)(struct SdfMgrTransport *transport);
	void (*enableInterrupts)(struct SdfMgrTransport *transport);
} SdfMgrTransportOperations;

typedef struct SdfMgrTransport {
	uint32_t events;
	uint32_t interruptFlags;
	uint32_t operationFlags;
	struct SdfMgrProtocolState *sendProtocol;
	uint32_t sendSize;
	uint32_t sendProgress;
	SceDeci4pHeader sendHeader;
	uint32_t flags;
	struct SdfMgrProtocolState *receiveProtocol;
	uint32_t receiveSize;
	uint32_t receiveProgress;
	SceDeci4pHeader receiveHeader;
	const char *name;
	SdfMgrTransportOperations operations;
} SdfMgrTransport;

typedef struct SdfMgrProtocolState {
	SceSDfMgrHandler handler;
	SceSDfMgrProtocol registration;
	void *context;
	int handle;
	uint32_t flags;
	uint32_t reserved;
	SceUID semaphore;
	int semaphoreCount;
	SceUID eventFlag;
	uint32_t eventPattern;
} SdfMgrProtocolState;

typedef struct SdfMgrCpuState {
	uint32_t nextHandle;
	uint32_t flags;
	uint32_t lastSendIndex;
	uint32_t reserved;			/* passed to the print handler; never set */
	SdfMgrHook indexedHooks[4];
	SdfMgrHook breakHooks[4];
	SdfMgrControl controlComponents[2];
	void *printContext;
	SdfMgrPrintHandler printHandler;
	SdfMgrProtocolState protocols[8];
	SdfMgrTransport *transport;
} SdfMgrCpuState;

/* SDfMgr step modes (SdfMgrDebugCpuState.stepMode) */
#define SDFMGR_STEP_ARM			0x01	/* break when leaving pc */
#define SDFMGR_STEP_THUMB		0x02	/* break at stepAddress */
#define SDFMGR_STEP_ARM_POLL		0x10	/* also ends the SDIO poll loop */
#define SDFMGR_STEP_THUMB_POLL		0x20

/* the debugger's (deci4p_sdbgp) per-core break record; the rest is its own */
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

typedef struct SdfMgrPacketQueue {
	int write;
	int read;
	int count;
	uint32_t directMode;
	void *packets[16];
	SceSize sizes[16];
} SdfMgrPacketQueue;

typedef struct SdfMgrDcmpCpuState {
	int handle;
	SceUID memblock;
	struct DcmpPacket *receive;
	SceDeci4pHeader *systemError;
	SdfMgrPacketQueue transmit;
} SdfMgrDcmpCpuState;

typedef struct SdfMgrTsmpCpuState {
	int handle;
	uint32_t reserved;
	SceSize headerRemaining;
	const void *header;
	SceSize payloadRemaining;
	const void *payload;
} SdfMgrTsmpCpuState;

typedef struct SdfMgrGlobalState {
	int phase;
	uint32_t flags;
	SceUID debugMemblocks[4];
	struct SdfMgrDebugRegisters *debugRegisters[4];
} SdfMgrGlobalState;

typedef struct SdfMgrState {
	uint32_t *exceptionDepth;
	uint8_t reserved[0x42fc];
#if FW_DEBUG
	SdfMgrGlobalState global;
#else
	SdfMgrDebugCpuState *debugCpu;
#endif
	SdfMgrCpuState cpu[4];
#if FW_DEBUG
	uint32_t lock;
	SdfMgrDebugCpuState *debugCpu;
#else
	SdfMgrGlobalState global;
	uint32_t lock;
#endif
	SdfMgrDcmpCpuState dcmp[4];
	SdfMgrTsmpCpuState tsmp[4];
	SdfMgrTransport sdio[4];
} SdfMgrState;

_Static_assert(sizeof(SdfMgrCpuState) == 0x244, "per-core SDfMgr layout");
_Static_assert(offsetof(SdfMgrCpuState, printContext) == 0x38, "print context");
_Static_assert(offsetof(SdfMgrCpuState, printHandler) == 0x3c, "print callback");
_Static_assert(offsetof(SdfMgrTransport, flags) == 0x28, "transport flags");
_Static_assert(offsetof(SdfMgrTransport, operations.poll) == 0x64, "transport poll callback");
_Static_assert(offsetof(SdfMgrTransport, receiveSize) == 0x30, "receive packet size");
_Static_assert(offsetof(SdfMgrTransport, receiveProtocol) == 0x2c, "receive protocol");
_Static_assert(offsetof(SdfMgrTransport, receiveHeader) == 0x38, "receive header");
_Static_assert(sizeof(SdfMgrTransportOperations) == 0x40, "transport operations");
_Static_assert(offsetof(SdfMgrTransport, operations.receive) == 0x50, "receive callback");
_Static_assert(offsetof(SdfMgrTransport, operations.beginSend) == 0x58, "send callback");
_Static_assert(offsetof(SdfMgrTransport, operations.send) == 0x5c, "payload send callback");
_Static_assert(sizeof(SceDeci4pHeader) == 16, "wire header size");
_Static_assert(offsetof(SceDeci4pHeader, reserved2) == 14, "wire reserved fields");
_Static_assert(sizeof(SdfMgrProtocolState) == 0x40, "protocol record stride");
_Static_assert(sizeof(SceSDfMgrProtocol) == 0x1c, "registration record size");
_Static_assert(offsetof(SdfMgrProtocolState, registration) == 4, "registration snapshot");
_Static_assert(offsetof(SdfMgrProtocolState, context) == 0x20, "protocol context");
_Static_assert(offsetof(SdfMgrProtocolState, handle) == 0x24, "protocol handle");
_Static_assert(offsetof(SdfMgrProtocolState, semaphore) == 0x30, "pending semaphore");
_Static_assert(offsetof(SdfMgrProtocolState, eventFlag) == 0x38, "pending event flag");
_Static_assert(offsetof(SceSDfMgrProtocol, destination) == 0x14, "protocol destination");
_Static_assert(offsetof(SdfMgrCpuState, protocols) == 0x40, "protocol records");
_Static_assert(offsetof(SdfMgrCpuState, transport) == 0x240, "transport slot");
_Static_assert(offsetof(SdfMgrState, cpu) == (FW_DEBUG ? 0x4328 : 0x4304),
	       "firmware per-core state");
_Static_assert(sizeof(SdfMgrDebugCpuState) == 0x390, "external debug record stride");
_Static_assert(offsetof(SdfMgrDebugCpuState, pc) == 0x194, "break pc");
_Static_assert(offsetof(SdfMgrDebugCpuState, stepAddress) == 0x1b0, "Thumb step address");
_Static_assert(offsetof(SdfMgrState, debugCpu) == (FW_DEBUG ? 0x4c3c : 0x4300),
	       "external debug records slot");
_Static_assert(sizeof(SdfMgrGlobalState) == 0x28, "global control reset span");
_Static_assert(offsetof(SdfMgrGlobalState, debugMemblocks) == 8,
	       "debug mapping IDs");
_Static_assert(offsetof(SdfMgrGlobalState, debugRegisters) == 0x18,
	       "debug mapping bases");
_Static_assert(offsetof(SdfMgrState, global.phase) == (FW_DEBUG ? 0x4300 : 0x4c14),
	       "firmware phase slot");
_Static_assert(offsetof(SdfMgrState, global.flags) == (FW_DEBUG ? 0x4304 : 0x4c18),
	       "firmware global flags slot");
_Static_assert(offsetof(SdfMgrState, lock) == (FW_DEBUG ? 0x4c38 : 0x4c3c),
	       "firmware spinlock slot");
_Static_assert(sizeof(SdfMgrState) == 0x5150, "SDfMgr storage");
_Static_assert(sizeof(SdfMgrPacketQueue) == 0x90, "packet queue layout");
_Static_assert(sizeof(SdfMgrDcmpCpuState) == 0xa0, "DCMP CPU stride");
_Static_assert(offsetof(SdfMgrDcmpCpuState, receive) == 8, "DCMP receive buffer");
_Static_assert(offsetof(SdfMgrDcmpCpuState, systemError) == 12, "DCMP direct buffer");
_Static_assert(offsetof(SdfMgrDcmpCpuState, transmit) == 0x10, "DCMP transmit queue");
_Static_assert(offsetof(SdfMgrState, dcmp) == 0x4c40, "DCMP CPU records");
_Static_assert(sizeof(SdfMgrTsmpCpuState) == 0x18, "TSMP CPU stride");
_Static_assert(offsetof(SdfMgrState, tsmp) == 0x4ec0, "TSMP CPU records");
_Static_assert(sizeof(SdfMgrTransport) == 0x8c, "SDIO transport stride");
_Static_assert(offsetof(SdfMgrTransport, sendHeader) == 0x18, "send header");
_Static_assert(offsetof(SdfMgrState, sdio) == 0x4f20, "SDIO CPU records");

extern SdfMgrState s_sdfMgrState;
extern const uint32_t s_sdfMgrCpuTargets[4];
SdfMgrDebugCpuState **SceDeci4pSDfMgrForKernel_BFB482D8(void);
void sdfMgrPollCurrentBreakHooks(void);
void sdfMgrReportPowerState(int state);
void busErrorHandler(uint32_t status0, uint32_t status1, uint32_t status2);
int sceSDfMgrIf2MgrEvent(SdfMgrTransport *transport, int event,
			uint32_t arg, uint32_t protocol, uint32_t destination);
uint32_t sdfMgrGetCpuFlags(uint32_t mask);
int sdfMgrUpdateCpuFlags(uint32_t mask, int enabled);
int sdfMgrUpdateGlobalFlags(uint32_t mask, int enabled);
int sdfMgrSetTransport(void *unused, SdfMgrTransport *transport);
int sdfMgrSetProtocolStatus(SdfMgrProtocolState *protocol, uint32_t mask,
			    int enabled);
void _execControlComponent(SdfMgrCpuState *cpu, unsigned id);
void sdfMgrPollBreakHooks(SdfMgrCpuState *cpu);
void sdfMgrHandleControlInterrupt(void);
int sdfMgrWaitForControl(void);
void sdfMgrScheduleSend(SdfMgrCpuState *cpu);
int sdfMgrQueuePacket(SdfMgrPacketQueue *queue, const void *packet, SceSize size,
		     int direct);
void dcmpSendError(SdfMgrProtocolState *protocol, uint32_t kind,
		   const SceDeci4pHeader *header);
uint32_t sdfMgrCpuAddress(void);
void dcmpSendStatusSpace(uint32_t protocol);
void dcmpSendStatusProtocol(uint32_t status, const SceSDfMgrProtocol *registration);
void dcmpSendStatusProtocolAlias(const SceSDfMgrProtocol *registration);
void dcmpSendStatusPower(uint32_t operation, uint32_t state, uint32_t reason);
void sdfMgrConnect(void);
void sdfMgrDisconnect(void);
int sdfmgrStartProtocol(uint32_t protocol);
int sdfmgrStopProtocol(uint32_t protocol);
void *SceDeci4pSDfMgrForKernel_5996E2DB(void *destination, const void *source,
				   SceSize size);
unsigned ksceKernelCpuSuspendIntr(void);
void ksceKernelCpuResumeIntr(unsigned state);
int ksceKernelCheckDipsw(unsigned bit);
unsigned ksceKernelCpuId(void);

static inline unsigned sdfMgrCoreIndex(void)
{
	return readMpidr() & 3;
}

#if FW_DEBUG
SdfMgrProtocolState *_getPdCB(unsigned index);
#else
static inline SdfMgrProtocolState *_getPdCB(unsigned index)
{
	return &s_sdfMgrState.cpu[sdfMgrCoreIndex()].protocols[index];
}
#endif

#if FW_DEBUG
#define SDFMGR_DEBUG_INFO(_line, _lineHash, _funcHash, _func) ({ \
	static const SceKernelDebugInfo info = { \
		.fileHash = 0x6364a003, .lineHash = (_lineHash), \
		.funcHash = (_funcHash), .file = "sdfmgr.c", \
		.line = (_line), .func = (_func), \
	}; \
	&info; \
})
#endif
