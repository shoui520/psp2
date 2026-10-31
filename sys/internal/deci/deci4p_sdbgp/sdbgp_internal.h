/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): the system debugger protocol.
 * Shared state and helpers of its files.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/modulemgr.h>
#include <sysmem/cp15.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <deci4p_dfmgr/deci4p.h>
#include <deci4p_sdfmgr/sdfmgr.h>
#include <iofilemgr/iofilemgr.h>
#include <iofilemgr/vfs.h>
#include <processmgr/process_info.h>
#include <sysmem/heap_info.h>
#include <sysmem/address_space.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>
#include <threadmgr/threadmgr_debugger.h>

/* the floating-point status register (exception notifications) */
static inline uint32_t readFpscr(void)
{
	uint32_t value;

	__asm__ volatile("vmrs %0, fpscr" : "=r"(value));
	return value;
}

/* the protocol's errors */
#define SDBGP_ERROR_INVALID_ARGUMENT	((int)0x80090001)
#define SDBGP_ERROR_ILLEGAL_ADDRESS	((int)0x80090005)
#define SDBGP_ERROR_PACKET_SIZE		((int)0x80090006)

/* sceSDfMgrExReqSend's "already requested" */
#define SDFMGR_ERROR_ALREADY_REQUESTED	((int)0x80090007)

/* the process ID of the kernel */
#define SDBGP_KERNEL_PROCESS_ID		0x10005

/* dipsw bits */
#define SDBGP_DIPSW_SYSTEM_DEBUG	0xc1
#define SDBGP_DIPSW_NO_TRACE		0xd7
#define SDBGP_DIPSW_BACKGROUND_CHECK	0xe0

/* sceKernelGetProcessStatus: the process is suspended */
#define SDBGP_PROCESS_STATUS_SUSPENDED	0x10

/* the suspend handler slot */
#define SDBGP_SUSPEND_INDEX		22

/* packet groups (SdbgpHeader.group) */
#define SDBGP_GROUP_SYSTEM		0x10
#define SDBGP_GROUP_PROCESS		0x20
#define SDBGP_GROUP_THREAD		0x30
#define SDBGP_GROUP_MODULE		0x40
#define SDBGP_GROUP_NOP			0x50
#define SDBGP_GROUP_FILE		0x60
#define SDBGP_GROUP_MISC		0x70
#define SDBGP_GROUP_TRACE		0x90
#define SDBGP_GROUP_KERNEL		0xf0

/* notifications of the system group */
#define SDBGP_NOTIFY_EXCEPTION		0xc8
#define SDBGP_NOTIFY_BREAK		0xca
#define SDBGP_NOTIFY_CONTINUE		0xcc

/* SdbgpHeader.result of an error reply, and what the checks return */
#define SDBGP_RESULT_INVALID			1
#define SDBGP_RESULT_ALREADY_SET		3
#define SDBGP_RESULT_ILLEGAL_VALUE		7
#define SDBGP_RESULT_NOT_SUPPORTED		8
#define SDBGP_RESULT_PACKET_TOO_SHORT		9
#define SDBGP_RESULT_PACKET_TOO_LARGE		10
#define SDBGP_RESULT_ILLEGAL_PROCESS_ID		0x20011
#define SDBGP_RESULT_ILLEGAL_THREAD_ID		0x20012
#define SDBGP_RESULT_ILLEGAL_ADDRESS		0x20030
#define SDBGP_RESULT_ILLEGAL_ACCESS_SIZE	0x20031
#define SDBGP_RESULT_HWWATCH_NOT_OWNER		0x20040
#define SDBGP_RESULT_HWWATCH_IN_USE		0x20041
#define SDBGP_RESULT_HWBKPT_IN_USE		0x20045
#define SDBGP_RESULT_SWBKPT_NOT_FOUND		0x20046
#define SDBGP_RESULT_PROCESS_NOT_SUSPENDED	0x20050
#define SDBGP_RESULT_ALREADY_ATTACHED		0x20052
#define SDBGP_RESULT_PROCESS_SUSPENDED		0x20056
#define SDBGP_RESULT_THREAD_NOT_SUSPENDED	0x20060
#define SDBGP_RESULT_THREAD_SUSPENDED		0x20061
#define SDBGP_RESULT_ADDRESS_OVERFLOW		0x20070
#define SDBGP_RESULT_ILLEGAL_FD			0x200d0

/* why a core stopped (checkSdbgpTargetAddrStopCause*, swbkptTypeOpcode) */
#define SDBGP_STOP_PABT			1	/* not a debug event */
#define SDBGP_STOP_DABT			2
#define SDBGP_STOP_SWBKPT_USER		16	/* BKPT 0x00-0x7f in user mode */
#define SDBGP_STOP_SWBKPT		17	/* BKPT 0x00-0x7f elsewhere, 0x80-0xbf */
#define SDBGP_STOP_BKPT_E0		18	/* BKPT 0xe0-0xff */
#define SDBGP_STOP_BKPT_C0		19	/* BKPT 0xc0-0xdf, or a BKPT debug event */
#define SDBGP_STOP_STEPPING		32
#define SDBGP_STOP_HWBKPT		32	/* hwbkptCheckStop: it stops here */
#define SDBGP_STOP_HWBKPT_PASS		33	/* not for this context */
#define SDBGP_STOP_HWWTPT		48	/* hwwtptCheckStop: it stops here */
#define SDBGP_STOP_HWWTPT_PASS		50
#define SDBGP_STOP_USER_DEBUG_EVENT	64

/* dipsw: hardware break and watch points */
#define SDBGP_DIPSW_HW_DEBUG		0xe4

/* the debug registers of a core's CoreSight page (CORESIGHT_DBG_BASE) */
#define DBG_BVR(n)			(0x100 + (n) * 4)
#define DBG_BCR(n)			(0x140 + (n) * 4)
#define DBG_WVR(n)			(0x180 + (n) * 4)
#define DBG_WCR(n)			(0x1c0 + (n) * 4)
#define DBG_LAR				0xfb0	/* lock access */
#define DBG_LAR_KEY			0xc5acce55

/* DBGBCR / DBGWCR */
#define DBGBCR_ENABLE			0x00000001
#define DBGBCR_PMC_MASK			0x00000006	/* privileged mode control */
#define DBGBCR_PMC_PRIVILEGED		0x00000002
#define DBGBCR_PMC_USER			0x00000004
#define DBGBCR_PMC_ANY			0x00000006
#define DBGBCR_BAS_MASK			0x000001e0	/* byte address select */
#define DBGBCR_BAS_THUMB_HIGH		0x00000180
#define DBGBCR_MISMATCH			0x00400000

/* DBGDSCR method of debug entry (bits 2-5) */
#define DBGDSCR_MOE_MASK		0x3c
#define DBGDSCR_MOE_BREAKPOINT		0x04
#define DBGDSCR_MOE_WATCHPOINT		0x08
#define DBGDSCR_MOE_BKPT		0x0c

/* the fault status of a debug event (IFSR/DFSR bits 0-3 and 10) */
#define FSR_STATUS(fsr)			(((fsr) & 0xf) | (((fsr) >> 10) & 1) << 4)
#define FSR_DEBUG_EVENT			2

#define PSR_MODE_MASK			0x1f
#define PSR_MODE_USR			0x10
#define PSR_MODE_SVC			0x13
#define PSR_MODE_SYS			0x1f
#define PSR_THUMB			0x20

/* SdbgpHeader.flags */
#define SDBGP_PACKET_CONTINUED		0x0001

/* every request starts with this */
typedef struct SdbgpRequest {
	SceDeci4pHeader hdr;
	uint8_t type;				/* 0x10 */
	uint8_t group;				/* 0x11: SDBGP_GROUP_* */
	uint8_t command;			/* 0x12 */
	uint8_t tag;				/* 0x13 */
} SdbgpRequest;

/* a thread's priority and affinity mask (SDBGP_GROUP_THREAD 0x40) */
typedef struct SdbgpThreadPriorityRequest {
	SdbgpRequest header;
	SceUID threadId;			/* 0x14 */
	int priority;				/* 0x18 */
	int cpuAffinityMask;			/* 0x1c */
} SdbgpThreadPriorityRequest;

/* a process's control flags (SDBGP_GROUP_PROCESS 0x60) */
typedef struct SdbgpProcessControlRequest {
	SdbgpRequest header;
	SceUID processId;			/* 0x14 */
	uint32_t flags;				/* 0x18 */
} SdbgpProcessControlRequest;

/* the threads of a process (SDBGP_GROUP_THREAD 0x20) */
typedef struct SdbgpThreadListRequest {
	SdbgpRequest header;
	SceUID processId;			/* 0x14 */
	uint32_t mode;				/* 0x18: 1 only */
	uint32_t detail;			/* 0x1c: 0 or 1 */
} SdbgpThreadListRequest;

/* set a software break point (SDBGP_GROUP_SYSTEM) */
typedef struct SdbgpSwbkptSetRequest {
	SdbgpRequest header;
	uint32_t reserved0;			/* 0x14 */
	SceUID processId;			/* 0x18 */
	uint32_t reserved1[2];			/* 0x1c */
	uintptr_t address;			/* 0x24 */
	uint32_t thumb;				/* 0x28 */
	uint32_t attr;				/* 0x2c */
} SdbgpSwbkptSetRequest;

/* every reply starts with this */
typedef struct SdbgpHeader {
	SceDeci4pHeader hdr;
	uint8_t type;				/* 0x10 */
	uint8_t group;				/* 0x11: SDBGP_GROUP_* */
	uint8_t command;			/* 0x12: a reply is the command + 1 */
	uint8_t tag;				/* 0x13: echoed in the reply */
	uint8_t fragment;			/* 0x14: of a reply sent in parts */
	uint8_t fragments;			/* 0x15 */
	uint16_t flags;				/* 0x16: SDBGP_PACKET_* */
	uint32_t result;			/* 0x18 */
} SdbgpHeader;

_Static_assert(sizeof(SdbgpHeader) == 0x1c, "SDBGP header");

/* the reply to a software break point set */
typedef struct SdbgpSwbkptSetReply {
	SdbgpHeader header;
	uint32_t id;				/* 0x1c */
	SceSize size;				/* 0x20: 2 Thumb, 4 ARM */
	uint32_t opcode;			/* 0x24: the code it replaced */
} SdbgpSwbkptSetReply;

/* one software break point of a list */
typedef struct SdbgpSwbkptInfo {
	SceSize size;				/* sizeof(SdbgpSwbkptInfo) */
	SceUID processId;
	uint32_t id;
	SceUID threadId;
	uintptr_t address;
	uint32_t thumb;
	uint32_t attr;
	uint32_t kind;				/* 8 */
	uint32_t reserved;
} SdbgpSwbkptInfo;

typedef struct SdbgpSwbkptListReply {
	SdbgpHeader header;
	uint32_t num;				/* 0x1c */
	SdbgpSwbkptInfo info[];			/* 0x20 */
} SdbgpSwbkptListReply;

/* one hardware break point of a list */
typedef struct SdbgpHwbkptInfo {
	SceSize size;				/* sizeof(SdbgpHwbkptInfo) */
	SceUID processId;
	SceUID threadId;
	uint32_t value;				/* DBGBVR */
	uint32_t control;			/* DBGBCR */
} SdbgpHwbkptInfo;

/* the hardware watch point */
typedef struct SdbgpHwwtptInfo {
	uint32_t size;				/* sizeof(SdbgpHwwtptInfo) */
	SceUID processId;
	SceUID threadId;
	uint32_t id;
	uint32_t value;				/* DBGWVR */
	uint32_t control;			/* DBGWCR */
} SdbgpHwwtptInfo;

/* the hardware break points of a thread or process */
typedef struct SdbgpHwbkptListReply {
	SdbgpHeader header;
	uint32_t num;				/* 0x1c */
	SdbgpHwbkptInfo info[];			/* 0x20 */
} SdbgpHwbkptListReply;

typedef struct SdbgpHwwtptGetReply {
	SdbgpHeader header;
	uint32_t num;				/* 0x1c */
	SdbgpHwwtptInfo info;			/* 0x20 */
} SdbgpHwwtptGetReply;

typedef struct SdbgpHwwtptSetReply {
	SdbgpHeader header;
	uint32_t id;				/* 0x1c */
} SdbgpHwwtptSetReply;

/* the stop details an exception notification carries (excp_handler.c) */
typedef struct SdbgpExcpDetail {
	uint32_t value[5];
} SdbgpExcpDetail;

/* SDBGP_NOTIFY_EXCEPTION */
typedef struct SdbgpExcpNotification {
	SdbgpHeader header;
	SceUID processId;			/* 0x1c */
	SceUID threadId;			/* 0x20 */
	uint32_t pc;				/* 0x24 */
	SdbgpExcpDetail detail;			/* 0x28 */
	uint32_t ifsr;				/* 0x3c */
	uint32_t ifar;				/* 0x40 */
	uint32_t dfsr;				/* 0x44 */
	uint32_t dfar;				/* 0x48 */
	uint32_t cpsr;				/* 0x4c */
	uint32_t fpscr;				/* 0x50 */
	uint32_t reserved[9];			/* 0x54 */
} SdbgpExcpNotification;

_Static_assert(sizeof(SdbgpExcpNotification) == 0x78, "exception notification");

/* the largest packet the link carries, and its payload */
#define SDBGP_PACKET_MAX		0x11000
#define SDBGP_PAYLOAD_MAX		(SDBGP_PACKET_MAX - sizeof(SdbgpHeader))

/* the per-core buffers in the module's memory block */
#define SDBGP_CPU_AREA_SIZE		0x121000
#define SDBGP_RECV_BUF_OFFSET		0x1000
#define SDBGP_SEND_BUF_OFFSET		0x12000
#define SDBGP_WORK_BUF_OFFSET		0x99f3c
#define SDBGP_SEND_BUF_SIZE		0x87f3c
#define SDBGP_WORK_BUF_SIZE		0x88000
#define SDBGP_MEMORY_SIZE		0x485000
#define SDBGP_TRACE_MEMORY_SIZE		0x400000
#define SDBGP_TRACE_CPU_SIZE		0x100000

/* SdbgpCpuState.status */
#define SDBGP_STATUS_SENDING		0x0001
#define SDBGP_STATUS_STOPPED		0x0004

/* the status word of a buffer */
#define SDBGP_BUF_LOCKED		0x0001
#define SDBGP_BUF_FILLED		0x0002
#define SDBGP_BUF_HEADER		0x0004
#define SDBGP_BUF_FRAGMENTED		0x0020

/* one core's protocol state */
typedef struct SdbgpCpuState {
	uint32_t status;			/* 0x00: SDBGP_STATUS_* */
	int handle;				/* 0x04: of the SDfMgr protocol */
	uint32_t flags;				/* 0x08 */
	uint32_t hostMode;			/* 0x0c */
	SceUID stoppedTid;			/* 0x10 */
	uint32_t reserved0;			/* 0x14 */
	uint32_t recvStatus;			/* 0x18: SDBGP_BUF_* */
	SceSize recvSize;			/* 0x1c */
	SdbgpHeader *recvBuf;			/* 0x20 */
	uint32_t reserved1;			/* 0x24 */
	uint32_t sendStatus;			/* 0x28: SDBGP_BUF_* */
	SceSize sendSize;			/* 0x2c */
	SdbgpHeader *sendBuf;			/* 0x30 */
	uint32_t reserved2;			/* 0x34 */
	uint32_t workStatus;			/* 0x38: SDBGP_BUF_* */
	SceSize workSize;			/* 0x3c */
	void *workBuf;				/* 0x40 */
	uint32_t sendResult;			/* 0x44: the first part's result word */
	SceSize sendOffset;			/* 0x48: sent of a fragmented reply */
	uint32_t sendFragment;			/* 0x4c */
	uint32_t sendFragments;			/* 0x50 */
	uint32_t reserved3[20];			/* 0x54 */
	void *traceBuf;				/* 0xa4 */
} SdbgpCpuState;

_Static_assert(sizeof(SdbgpCpuState) == 0xa8, "SDBGP core state");

extern SdbgpCpuState s_sdbgpCpu[4];

/* a software break point */
typedef struct SdbgpSwbkpt {
	uint32_t id;				/* 0x00: 0x20000 + its index */
	uintptr_t address;			/* 0x04 */
	uint32_t thumb;				/* 0x08: bit 0 */
	uint8_t opcode[4];			/* 0x0c: the code the BKPT replaced */
	uint32_t immediate;			/* 0x10: of a BKPT already there */
	uint32_t attr;				/* 0x14: as the host gave it */
	uint32_t reserved;			/* 0x18 */
	uint32_t used;				/* 0x1c */
	SceUID processId;			/* 0x20 */
	SceUID threadId;			/* 0x24 */
} SdbgpSwbkpt;

_Static_assert(sizeof(SdbgpSwbkpt) == 40, "software break point");

/* the BKPT the debugger sets, and the no-op that stands for one */
#define SWBKPT_ARM_BKPT			0xe1200070
#define SWBKPT_ARM_BKPT_DEBUGGER	0xe1200e70	/* BKPT 0xe0 */
#define SWBKPT_ARM_NOP			0xe320f000
#define SWBKPT_THUMB_BKPT		0xbe00
#define SWBKPT_THUMB_BKPT_DEBUGGER	0xbee0
#define SWBKPT_THUMB_NOP		0xbf00

/* a hardware break or watch point */
typedef struct SdbgpHwSlot {
	uint32_t owner;				/* 0x00 */
	uint32_t value;				/* 0x04: DBGBVR / DBGWVR */
	uint32_t control;			/* 0x08: as the host gave it */
	uint32_t used;				/* 0x0c */
	SceUID threadId;			/* 0x10 */
	SceUID processId;			/* 0x14 */
} SdbgpHwSlot;

#define SDBGP_SWBKPT_MAX		20
#define SDBGP_HWBKPT_MAX		6
#define SDBGP_HWWATCH_MAX		1

/* where hwbkptDisableHWBP found the break point it took out */
#define SDBGP_HWBKPT_IN_BP0		0	/* DBGBCR0 */
#define SDBGP_HWBKPT_IN_BP1		1	/* DBGBCR1 */
#define SDBGP_HWBKPT_IN_PROCESS		4	/* the process's PHBP */
#define SDBGP_HWBKPT_IN_THREAD		5	/* the thread's THBP */

/* hwbkptEnableHWBP: forget the break point after putting it back */
#define SDBGP_HWBKPT_ENABLE_CLEAR	0x100

/* the break point taken out while the core stops at it */
typedef struct SdbgpDisabledHwbkpt {
	uint32_t where;				/* 0x00: SDBGP_HWBKPT_IN_* */
	uint32_t value;				/* 0x04 */
	uint32_t control;			/* 0x08 */
	uint32_t reserved;			/* 0x0c */
	SceUID threadId;			/* 0x10 */
	SceUID processId;			/* 0x14 */
} SdbgpDisabledHwbkpt;

/* the watch point taken out while the core stops at it */
typedef struct SdbgpDisabledHwwtpt {
	uint32_t value;				/* 0x00: DBGWVR0 */
	uint32_t control;			/* 0x04: DBGWCR0 */
	uint32_t disabled;			/* 0x08 */
} SdbgpDisabledHwwtpt;

/* the id a hardware watch point is given */
#define SDBGP_HWWTPT_ID			0x30000

/* SdbgpCpuState.hostMode: leave the watch point off after this packet */
#define SDBGP_HOST_HWWTPT_OFF		0x100000

/* set a hardware break point (SDBGP_GROUP_SYSTEM) */
typedef struct SdbgpHwbkptSetRequest {
	SdbgpRequest header;
	uint32_t reserved;			/* 0x14 */
	SceUID processId;			/* 0x18 */
	SceUID threadId;			/* 0x1c */
	uint32_t value;				/* 0x20 */
	uint32_t control;			/* 0x24 */
} SdbgpHwbkptSetRequest;

/* set the hardware watch point (SDBGP_GROUP_SYSTEM) */
typedef struct SdbgpHwwtptSetRequest {
	SdbgpRequest header;
	uint32_t reserved0;			/* 0x14 */
	SceUID processId;			/* 0x18 */
	SceUID threadId;			/* 0x1c */
	uint32_t reserved1;			/* 0x20 */
	uint32_t value;				/* 0x24 */
	uint32_t control;			/* 0x28 */
} SdbgpHwwtptSetRequest;

/* the debugger's global state */
/*
 * The UID and heap logs _leakcheckUID and _leakcheckHeap keep (leakcheck.c): the
 * kernel's UIDs at the first pass, at the current one, and the log of what came
 * since. The log holds LeakCheckUIDLogEntry or, in heap mode, LeakCheckNamedChunk.
 */
#define LEAKCHECK_MODE_UID		2
#define LEAKCHECK_MODE_HEAP		4

typedef struct SdbgpUIDList {
	uint32_t mode;				/* LEAKCHECK_MODE_* of the pass that took it */
	SceUID memblock;
	SceKernelUIDEntryInfo *entries;
	SceSize num;				/* capacity in, then the entries of the first pass */
} SdbgpUIDList;

typedef struct SdbgpLeakCheck {
	SdbgpUIDList base;			/* 0x00: "SceDeci4pSdbgpLCBase" */
	SdbgpUIDList current;			/* 0x10: "SceDeci4pSdbgpLCCurr" */
	SceUID logMemblock;			/* 0x20: "SceDeci4pSdbgpLCLog" */
	void *log;				/* 0x24 */
	SceSize logCount;			/* 0x28 */
	SceSize logMax;				/* 0x2c */
	uint32_t generation;			/* 0x30: passes taken */
} SdbgpLeakCheck;

_Static_assert(sizeof(SdbgpLeakCheck) == 0x34, "leak check state");

typedef struct SdbgpState {
	SceUID memblock;			/* 0x000 */
	void *base;				/* 0x004 */
	uint32_t flags;				/* 0x008 */
	uint32_t reserved0;			/* 0x00c */
	SdbgpSwbkpt swbkpt[SDBGP_SWBKPT_MAX];	/* 0x010 */
	SdbgpSwbkpt sysSwbkpt;			/* 0x330: one taken out to step over it */
	SdbgpSwbkpt stepSwbkpt;			/* 0x358: step over a call */
	SdbgpHwSlot hwbkpt[SDBGP_HWBKPT_MAX];	/* 0x380 */
	SdbgpDisabledHwbkpt disabledHwbkpt;	/* 0x410: hwbkptDisableHWBP */
	SdbgpHwSlot hwwatch[SDBGP_HWWATCH_MAX];	/* 0x428 */
	uint32_t reserved3;			/* 0x440 */
	SdbgpDisabledHwwtpt disabledHwwtpt;	/* 0x444: hwwtptDisableHWWP */
	uint32_t reserved4[2];			/* 0x450 */
	uint32_t enabled;			/* 0x458 */
	SceUID stoppedTid[4];			/* 0x45c */
	uint32_t stopReason[4];			/* 0x46c: per core, SDBGP_STOP_* */
	uint32_t reserved6[3];			/* 0x47c */
	uint32_t stepCount;			/* 0x488 */
	SdfMgrDebugCpuState *debugCpu;		/* 0x48c: the SDfMgr break records */
	SdbgpLeakCheck leakCheck;		/* 0x490 */
	void *traceBase;			/* 0x4c4 */
	SceUID traceMemblock;			/* 0x4c8 */
	SceSize traceSize;			/* 0x4cc */
} SdbgpState;

_Static_assert(sizeof(SdbgpState) == 0x4d0, "SDBGP state");

extern SdbgpState s_sdbgpState;

/* sdbgp.c */
SdbgpState *sdbgpGetState(void);
SdbgpCpuState *sdbgpGetCpuState(void);
int sdbgpSuspend(int value, void *args);
int sdbgpResume(int value, void *args);
int _sdbgpSendData(int handle, const void *buf, SceSize size);
int sdbgpUpdateBufStatus(uint32_t *status, uint32_t bits, int set);
int _sdbgpFreeSendBuf(SdbgpCpuState *cpu);
int sdbgpSendErrorHeader(SdbgpCpuState *cpu, uint8_t group, uint8_t command,
			 uint8_t tag, uint32_t result);
uint32_t sdbgpGetFlags(SdbgpState *state, uint32_t mask);
void sdbgpUpdateFlags(SdbgpState *state, uint32_t bits, int set);
uint32_t sdbgpGetStatus(SdbgpCpuState *cpu, uint32_t mask);
void sdbgpUpdateStatus(SdbgpCpuState *cpu, uint32_t bits, int set);
SceSize sdbgpGetRecvSize(SdbgpCpuState *cpu);
void *sdbgpAllocSendBuf(SdbgpCpuState *cpu, SceSize size);
int sdbgpSendNotification(SdbgpCpuState *cpu, SceUID pid, SceUID tid, uint32_t pc,
			  uint32_t cpsr, const SdbgpExcpDetail *detail);
void sdbgpControl(int id);
void _sdbgpRecvData(int event, void *arg, void *context);
int sdbgpSendError(SdbgpCpuState *cpu, uint8_t group, uint8_t command, uint8_t tag,
		   uint32_t result);
void sdbgpSetSendSize(SdbgpCpuState *cpu, SceSize size);
void *sdbgpAllocWorkBuf(SdbgpCpuState *cpu, SceSize size);
int sdbgpReadMemory(SceUID pid, const void *addr, SceSize size, void *out);
int sdbgpWriteMemory(SceUID pid, void *addr, SceSize size, const void *in);

/* the command handlers (group_*.c, trace.c) */
int _getFixedHeapInfo(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int _getHeapInfoAll(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_00(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_08(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_0a(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_10(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_12(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_1a(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_1c(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_30(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_32(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_34(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_40(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_42(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_44(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_48(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_4a(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_4c(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_50(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_52(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_54(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_62(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_64(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_72(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_90(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_92(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_94(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_96(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_98(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd10_9a(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_00(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_10(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_12(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_20(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_22(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_52(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_56(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_58(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_60(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_80(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_82(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd20_84(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command);
int sdbgpCmd60_00(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd60_12(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd60_14(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd60_16(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd60_18(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd60_1a(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd70_00(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd70_02(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd70_04(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd70_06(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd30_00(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd30_02(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd30_10(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd30_12(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd30_20(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd30_40(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd30_50(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd40_00(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd40_10(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd40_12(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd40_30(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd90_00(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd90_02(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmd90_04(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_02(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_08(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_0a(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_0c(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_0e(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_10(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_12(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_14(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_16(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_18(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_1a(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_1c(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_20(SdbgpCpuState *cpu, SdbgpHeader *packet);
int sdbgpCmdF0_30(SdbgpCpuState *cpu, SdbgpHeader *packet);

/* check.c */
int sdbgpIsNotification(uint8_t group, uint8_t command);

/* check.c */
int isIllegalBreakpointAttribute(void);
int isSwbkptAlreadySet(SceUID pid, SceUID tid, uintptr_t address);
int isIllegalSize(SceSize size);
int isIllegalThreadPriority(const SdbgpThreadPriorityRequest *request);
int isIllegalProcessControl(const SdbgpProcessControlRequest *request);
int isIllegalThreadListMode(const SdbgpThreadListRequest *request);
int isIllegalMountNum(unsigned int num);
int isIllegalKernelApiId(int id);
int isIllegalMountList(SceVfsMount *const *mounts, int num, VfsMountListEntry *list);
int isIllegalMountAccess(SceVfsMount *const *mounts, int num, void *info);
int isIllegalProcessId(SceUID pid);
int isIllegalThreadId(SceUID tid);
int isIllegalThreadIdOrAll(SceUID tid);
int isIllegalGetRegisterState(void);
int isIllegalGetRegisterIndex(void);
int isIllegalSetRegisterState(void);
int isIllegalSetRegisterIndex(void);
int isIllegalAddress(SceUID pid, uintptr_t address, SceSize size, uint32_t mode);
int isIllegalMemoryType(const void *address);
int isIllegalAlignment(void);
int isIllegalAccessSize(SceSize size);
int isHwwatchNotOwner(SceUID owner);
int isHwwatchInUse(void);
int isHwbkptInUse(SceUID pid, SceUID tid);
int isSwbkptNotFound(uint32_t id);
int isProcessNotSuspended(SceUID pid);
int isAlreadyAttached(SdbgpCpuState *cpu, SceUID pid);
int isIllegalAttachState(void);
int isIllegalProcessStatus(SceUID pid);
int isThreadNotSuspended(SceUID tid);
int isThreadIsSuspendedAlready(SceUID tid);
int isAddressOverflow(uintptr_t address, SceSize size);
int isIllegalFd(SceUID pid, SceUID fd, SceIofileInfo *info);
int isIllegalSysFd(SceUID fd, SceIofileInfo *info);
/* the context an exception handler passes to checkSdbgpTargetAddrStopCause* */
typedef struct SdbgpExcpInfo {
	SceExcpmgrContext *context;		/* 0x00 */
	SceUID processId;			/* 0x04 */
	SceUID threadId;			/* 0x08 */
	uintptr_t pc;				/* 0x0c */
	uint32_t cpsr;				/* 0x10 */
	int addressSpace;			/* 0x14: 1 intr, 16 kernel, 32 user */
	uint32_t stopReason;			/* 0x18 */
} SdbgpExcpInfo;

_Static_assert(sizeof(SdbgpExcpInfo) == 0x1c, "SDBGP exception info");

int checkSdbgpTargetAddrStopCausePabt(SdbgpExcpInfo info, int mode);
int checkSdbgpTargetAddrStopCauseDabt(uint32_t dfar, SdbgpExcpInfo info, int mode);
int getCpuStopReason(SdbgpCpuState *cpu, uint32_t *reason);

/* swbkpt.c */
int swbkptTypeOpcode(uint32_t opcode, int thumb, uint32_t cpsr);
int swbkptFindByAddress(SceUID pid, uintptr_t address);
int _swbkptRemoveKernel(SdbgpState *state, int index);
int _swbkptRemoveUser(SdbgpState *state, int index);
int swbkptCount(void);
int swbkptGetList(SdbgpSwbkptListReply *reply, uint32_t id);
int swbkptSetKernel(const SdbgpSwbkptSetRequest *request, SdbgpSwbkptSetReply *reply);
int swbkptSetUser(const SdbgpSwbkptSetRequest *request, SdbgpSwbkptSetReply *reply);
int swbkptClearById(uint32_t id);
uint32_t swbkptGetImmediate(uintptr_t address, uint32_t cpsr);
int swbkptDisableSWBP(SdbgpSwbkpt bkpt);
int swbkptEnableSWBP(void);
int swbkptEnableSysSWBP(void);
int swbkptWriteBkpt(const SdbgpSwbkpt *bkpt);
int swbkptRemove(SdbgpSwbkpt *bkpt);
int swbkptMakeEntry(SceUID pid, SceUID tid, uintptr_t address, uint32_t cpsr,
		    int readImmediate, SdbgpSwbkpt *bkpt);
int swbkptIsBkpt(SceUID pid, uintptr_t address, uint32_t cpsr, int *isBkpt);
int swbkptHideInBuffer(SceUID pid, uintptr_t start, SceSize size, uint8_t *buf);
int swbkptClear(int index);

/* leakcheck.c */
#define LEAKCHECK_PROCESS_MAX		16
#define LEAKCHECK_PHYMEMPART_MAX	16
#define LEAKCHECK_FIXED_HEAP_MAX	8	/* per object heap */
#define LEAKCHECK_HEAP_MAX		128
#define LEAKCHECK_HEAP_BLOCK_MAX	16
#define LEAKCHECK_CLASS_MAX		20

/* the UIDs of one class (_GetResourceGUIDInfo), in the leak check heap */
typedef struct LeakCheckGUIDVector {
	SceSize num;
	SceUID *uid;
} LeakCheckGUIDVector;

/* a heap's info with room for its block list */
typedef struct LeakCheckHeapInfo {
	HeapInfo info;
	struct malloc_segment_range blocks[LEAKCHECK_HEAP_BLOCK_MAX];
} LeakCheckHeapInfo;

/*
 * sceSdbgpGetSystemResourceSnapshot's record of the kernel's resources, one
 * memblock of its own. Per process: its info, address space, physical memory
 * partitions, object heaps (0: the process's, 1: its second) and their fixed
 * heaps; then the kernel's UID classes, fixed heaps and heaps.
 */
typedef struct SdbgpSystemResourceSnapshot {
	SceUInt32 level;
	int processCount;
	SceKernelProcessInfo process[LEAKCHECK_PROCESS_MAX];			/* 0x00008 */
	AddressSpaceInfo addressSpace[LEAKCHECK_PROCESS_MAX];			/* 0x00e88 */
	SceKernelPhyMemPartInfoForDebugger
		phyMemPart[LEAKCHECK_PROCESS_MAX][LEAKCHECK_PHYMEMPART_MAX];	/* 0x073c8 */
	ObjectHeapInfo objectHeap[LEAKCHECK_PROCESS_MAX][2];			/* 0x123c8 */
	SceKernelFixedHeapInfo
		fixedHeap[LEAKCHECK_PROCESS_MAX][2][LEAKCHECK_FIXED_HEAP_MAX];	/* 0x13cc8 */
	HeapInfo processHeap[LEAKCHECK_HEAP_MAX];				/* 0x1ccc8 */
	struct malloc_segment_range processHeapBlocks[LEAKCHECK_HEAP_BLOCK_MAX];	/* 0x204c8 */
	uint32_t reserved[4064];						/* 0x20548 */
	SceKernelUIDEntryHeapInfo uidEntryHeap[LEAKCHECK_PROCESS_MAX];		/* 0x244c8 */
	SceKernelNameHeapInfo nameHeap;						/* 0x24648 */
	LeakCheckGUIDVector guid[LEAKCHECK_CLASS_MAX];				/* 0x2465c */
	SceKernelFixedHeapInfo kernelFixedHeap[6];	/* 0x246fc: 32 to 512 bytes, GUID entries */
	LeakCheckHeapInfo kernelHeap[2];		/* 0x2485c: cached, uncached */
	int heapCount;							/* 0x2493c */
	HeapInfo heap[LEAKCHECK_HEAP_MAX];					/* 0x24940 */
	struct malloc_segment_range heapBlocks[LEAKCHECK_HEAP_MAX][LEAKCHECK_HEAP_BLOCK_MAX];
} SdbgpSystemResourceSnapshot;

_Static_assert(offsetof(SdbgpSystemResourceSnapshot, processHeap) == 117960, "snapshot heaps");
_Static_assert(offsetof(SdbgpSystemResourceSnapshot, uidEntryHeap) == 148680, "snapshot UID heaps");
_Static_assert(offsetof(SdbgpSystemResourceSnapshot, heapCount) == 150588, "snapshot heap count");
_Static_assert(sizeof(SdbgpSystemResourceSnapshot) == 181312, "snapshot size");

/* a section run with interrupts masked (the CPU long-mask hook) */
typedef struct SdbgpCpuMaskEntry {
	int kind;
	uint32_t duration;			/* microseconds */
	uint32_t core;
	uintptr_t lockedCaller;
	const void *resumeCaller;
} SdbgpCpuMaskEntry;

/* CpuMaskShow's log (kernel_util.c) */
typedef struct SdbgpCpuMaskLog {
	uint32_t shown;
	struct CpuLongIrqHandlerRecord hook;
	SceUID memblock;
	SdbgpCpuMaskEntry *entries;
	SceSize count;
	SceSize max;
} SdbgpCpuMaskLog;

extern SdbgpCpuMaskLog sdbgpCpuMaskLog;

/* _leakcheckUID's records */
typedef struct LeakCheckUIDRecord {
	SceSize size;				/* 80 */
	SceUID uid;
	char name[32];
	char className[32];
	SceUInt32 attr;
	uint32_t generation;
} LeakCheckUIDRecord;

/* _leakcheckHeap's records */
typedef struct LeakCheckHeapRecord {
	SceSize size;				/* 60 */
	SceUID heap;
	char name[32];
	void *ptr;
	SceSize chunkSize;
	const void *caller;
	SceUInt32 func;
	uint32_t generation;
} LeakCheckHeapRecord;

int sdbgpCpuMaskLogAdd(int kind, uint32_t duration, unsigned core, uintptr_t lockedCaller,
		       const void *resumeCaller);
int LeakCheckHeapInit(void);
int sceKernelHeapLoggingStart(void);
int chunkAll(int all, uint32_t generation);
void LeakCheckInit(void);
int LeakCheckHeapFilerAdd(const char *name);
int sceSdbgpGetSystemResourceSnapshot(SceUInt32 level, SdbgpSystemResourceSnapshot *snap);
int sceKernelSnapshot(void);
int LeakCheckUIDSnapshot(void);
int LeakCheckHeapSnapshot(void);
int _leakcheckUID(LeakCheckUIDRecord *out, uint32_t *generation, SceSize *count, SceSize *size);
int _leakcheckHeap(LeakCheckHeapRecord *out, uint32_t *generation, SceSize *count, SceSize *size);
int _leakcheckReset(void);

/* group_system.c */
int sdbgpDecodeOpcode(int thumb, const void *buf, uint32_t *opcode);
int sdbgpCheckStepOverCall(SdbgpState *state, SceUID pid, uintptr_t pc, uint32_t cpsr,
			   int *isCall);

/* hwbkpt.c */
int hwbkptWriteAllCpu(uint32_t value, uint32_t control);
int hwbkptCheckStop(SceUID pid, SceUID tid, uintptr_t pc, uint32_t cpsr);
void hwbkptClear(int index);
int _getHwbkpt(SdbgpHwbkptListReply *reply, SceUID pid, SceUID tid,
	       ThreadmgrThreadInfoForDebugger *info);
int hwbkptGetHwbkptAll(SdbgpHwbkptInfo *info, SceSize *num);
int hwbkptSetHwbkpt(const SdbgpHwbkptSetRequest *request);
int clearHwbkpt(SceUID pid, SceUID tid, uint32_t value);
int hwbkptClearHwbkptAll(ThreadmgrThreadInfoForDebugger *threads);
int hwbkptDisableHWBP(void);
int hwbkptEnableHWBP(int flags);
uint32_t hwbkptMakeControl(uint32_t control);
int hwwtptWriteAllCpu(uint32_t value, uint32_t control);
int hwwtptCheckStop(SceUID tid, uintptr_t address, int mode);
void hwwtptClear(int index);
int hwwtptIsSet(void);
int _getHwwtpt(SdbgpHwwtptGetReply *reply, uint32_t id);
uint32_t hwwtptMakeControl(uint32_t control);
int hwwtptSetHwwtpt(const SdbgpHwwtptSetRequest *request, SdbgpHwwtptSetReply *reply);
int clearHwwtpt(uint32_t id);
int hwwtptDisableHWWP(void);
int hwwtptEnableHWWP(void);
void hwwtptDisable(void);
void hwwtptEnable(SdbgpCpuState *cpu);

/* phymem.c */
int sceKernelPhysicalMemReadForDebugger(void *dest, uintptr_t psrc, SceSize size, SceSize width);
int sceKernelPhysicalMemWriteForDebugger(uintptr_t pdest, const void *src, SceSize size,
					 SceSize width);
int sdbgpInitPhysicalAccess(int unused);

/* kernel_util.c */
extern struct SysrootProfiler s_sdbgpProfiler;
void sdbgpInitProfiler(struct SysrootProfiler *profiler);
int KernelUtilsFixedHeapLeakCheckStart(SceUID uid, void **ctx);
int KernelUtilsFixedHeapLeakCheck(void *ctx);
int kernelUtility(int id);
int NameHeapLogSaveCore(void);

/* thread.c, sync.c */
int sdbgpGetKernelWorkQueueInfo(SdbgpCpuState *cpu, void *reply,
				ThreadmgrWorkQueueInfoFull *work, SceSize size, int kernelOnly);
int sdbgpGetProcessWorkQueueInfo(SdbgpCpuState *cpu, void *reply,
				 ThreadmgrWorkQueueInfoFull *work, SceSize size);
int sdbgpGetThreadInfoList(SdbgpCpuState *cpu, void *out, void *work, SceUID pid,
			   uint32_t detail, SceSize *size, SceSize *count);
int sdbgpGetThreadWaypointTrace(SceUID *pid, void *work, void *out, SceSize *count,
				SceSize *size);
int sdbgpGetSyncInfoList(const SdbgpThreadListRequest *req, void *out, void *work,
			 SceSize *size);
SceSize sdbgpGetSyncAllCount(const SdbgpThreadListRequest *req, void *work);
int sdbgpGetSyncAllInfo(const SdbgpThreadListRequest *req, void *out, void *work);

/* SceCodecEngineWrapperForDebugger (os0:kd/vnz_wrapper.skprx) */
int SceCodecEngineWrapperForDebugger_771190E7(unsigned int cmd, void *buf);

/* bg_check.c */
int bgCheckStart(void);

/* sysmem_address_space.c, sysmem_partition.c, cpu_mmu.c */
typedef struct SceKernelMMUL1Info {
	uint32_t vbase;
	uint32_t pbase;
	uint32_t type;
} SceKernelMMUL1Info;

typedef struct SceKernelMMUL1AllInfo {
	SceSize size;				/* 0x400c */
	uintptr_t split;
	uint32_t l2Count;
	uint32_t table[4096];
} SceKernelMMUL1AllInfo;

typedef struct SceKernelMMUL2AllInfo {
	SceSize size;				/* 0x404 */
	uint32_t table[256];
} SceKernelMMUL2AllInfo;

int sceKernelAddressSpaceGetMMUL1Info(SceUID asid, uintptr_t va, SceKernelMMUL1Info *info);
int sceKernelAddressSpaceGetMMUL1AllInfo(SceUID asid, SceKernelMMUL1AllInfo *info);
int sceKernelAddressSpaceGetMMUL2AllInfo(SceUID asid, uintptr_t va, SceKernelMMUL2AllInfo *info);
int PartitionGetMMUL2AllInfo(Partition *part, uintptr_t va,
			     SceKernelMMUL2AllInfo *info);
int L1PageTableGetInfo(L1PageTable *l1pt, uintptr_t va, SceKernelMMUL1Info *info);
int L1PageTableGetAllInfo(L1PageTable *l1pt, SceKernelMMUL1AllInfo *info);
int sceKernelMMUL1GetInfo(const uint32_t *table, uintptr_t va, SceKernelMMUL1Info *info);

/* excp_handler.c, excp_info.c */
int excpHandlerInit(SdbgpCpuState *cpu);
int excpGetExcpStopReason(SdbgpCpuState *cpu, SdbgpExcpDetail *detail, uint32_t stopReason);

/* trace.c */
int traceInit(void);

/* the probe loads and stores (probe.S) the exception manager resumes */
extern const char sdbgpProbeStart[], sdbgpProbeEnd[];
int sceKernelVMRead8(uint8_t *dst, const void *src);
int sceKernelVMRead16(uint16_t *dst, const void *src);
int sceKernelVMRead32(uint32_t *dst, const void *src);
int sceKernelVMWrite8(void *dst, uint8_t value);
int sceKernelVMWrite16(void *dst, uint16_t value);
int sceKernelVMWrite32(void *dst, uint32_t value);

/* cpu_regs.S */
void sdbgpReadSvcRegister(uint32_t code, uint32_t *value);
void sdbgpReadAbtRegister(uint32_t code, uint32_t *value);
void sdbgpReadUndRegister(uint32_t code, uint32_t *value);
void sdbgpReadIrqRegister(uint32_t code, uint32_t *value);
void sdbgpReadFiqRegister(uint32_t code, uint32_t *value);
int sdbgpCallSecureMonitor(uint32_t flags, uint32_t ttbr0, uint32_t vbar, void *sysroot);
