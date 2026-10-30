/* SceDeci4pDbgp objects owned by the debugger protocol. */
#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <threadmgr/fast_mutex_object.h>
#include <deci4p_dfmgr/deci4p.h>
#include <net_ps/netps.h>
#include <modulemgr/modulemgr_debugger.h>
#include <sysmem/sysroot_dbgp.h>
#include <sysmem/sysmem.h>
#include <processmgr/processmgr.h>

typedef struct DbgpPrefix {
	SceDeci4pHeader hdr;
	SceUInt8 version;
	SceUInt8 group;
	SceUInt8 type;
	SceUInt8 code;
} DbgpPrefix;

typedef struct DbgpRequestHeader {
	DbgpPrefix prefix;
	SceUID pid;
} DbgpRequestHeader;

typedef struct DbgpRegisterRequest {
	DbgpPrefix prefix;
	SceUID pid;
	SceUID thid;
	SceUInt32 count;
	SceUInt32 registers[];
} DbgpRegisterRequest;

_Static_assert(offsetof(DbgpRegisterRequest, registers) == 32,
	       "debugger register request header");

typedef struct DbgpResponseHeader {
	DbgpPrefix prefix;
	SceUInt16 sequence;
	SceUInt16 flags;
	union {
		SceUInt32 result;
		SceUID pid;
	};
} DbgpResponseHeader;

typedef struct DbgpThreadIdentity {
	SceUID pid;
	SceUID thid;
} DbgpThreadIdentity;

_Static_assert(sizeof(DbgpPrefix) == 20, "DBGP command prefix");
_Static_assert(sizeof(DbgpRequestHeader) == 24, "DBGP request header");
_Static_assert(sizeof(DbgpResponseHeader) == 28, "DBGP response header");

/* Receive, send and work buffers share this complete control block. */
typedef struct DbgpBuf {
	struct ThreadmgrFastMutex mutex;
	SceUInt32 status;
	SceUID extendedMemBlock;
	void *base;
	void *extendedBase;
	SceSize extendedSize;
	SceSize size;
	const char *name;
} DbgpBuf;

_Static_assert(sizeof(DbgpBuf) == 0x5c, "debugger buffer control block");
_Static_assert(offsetof(DbgpBuf, status) == 0x40, "debugger buffer status");

/* Result and progress callbacks publish the same seven-word queue record. */
/* CoredumpInfo.type */
#define COREDUMP_INFO_RESULT	0
#define COREDUMP_INFO_PROGRESS	1

typedef struct CoredumpInfo {
	SceUInt32 type;
	SceUID pid;
	int task_id;
	int result;
	const char *path;
	SceSize pathSize;
	SceUInt32 progress;
} CoredumpInfo;

typedef struct CoredumpInfoBuf {
	int write;
	int read;
	int count;
	CoredumpInfo info[16];
} CoredumpInfoBuf;

_Static_assert(sizeof(CoredumpInfo) == 0x1c, "debugger coredump record");
_Static_assert(sizeof(CoredumpInfoBuf) == 0x1cc, "debugger coredump queue");

/* A software breakpoint temporarily removed while a thread is stepped. */
typedef struct StepSwbkptInfo {
	SceUInt32 address;
	SceUInt32 thumb;
	union {
		SceUInt32 opcode;
		SceUInt8 bytes[4];
	} original;
	SceUID pid;
	SceUID thid;
	SceUInt32 active;
} StepSwbkptInfo;

_Static_assert(sizeof(StepSwbkptInfo) == 0x18, "step breakpoint restore record");

/* Event queues copy timestamps as two words, including unaligned records. */
typedef struct DbgpTime {
	SceUInt32 low;
	SceUInt32 high;
} DbgpTime;

typedef struct ExcpInfo {
	SceUInt32 reason;
	SceUInt32 pc;
	DbgpThreadIdentity thread;
	SceUInt32 valid;
	SceUInt32 deferred;
} ExcpInfo;

typedef struct ExcpInfoBuf {
	int write;
	int read;
	int count;
	int cancelled;
	int deferred;
	ExcpInfo info[32];
} ExcpInfoBuf;

typedef struct ThreadInfo {
	SceUID pid;
	SceUID thid;
	SceUInt32 type;
	int exitStatus;
	SceUInt64 time;
} ThreadInfo;

/* Queued event records start after three words, on a four-byte boundary. */
typedef struct __attribute__((packed, aligned(4))) ThreadInfoBuf {
	int write;
	int read;
	int count;
	ThreadInfo info[64];
} ThreadInfoBuf;

/* a process event; for a spawn the parent is the process that exits to it */
typedef struct ProcessInfo {
	SceUInt32 type;			/* PROCESS_INFO_* */
	SceUID pid;
	SceUID parentPid;
	int exitStatus;			/* PROCESS_INFO_EXIT_SPAWN: the spawn's result */
	SceUInt64 time;
} ProcessInfo;

#define PROCESS_INFO_START		0
#define PROCESS_INFO_EXIT		1
#define PROCESS_INFO_KILL		2
#define PROCESS_INFO_SUSPEND		3
#define PROCESS_INFO_RESUME		4
#define PROCESS_INFO_SPONTANEOUS_EXIT	5
#define PROCESS_INFO_EXIT_TO_SPAWN	6
#define PROCESS_INFO_APP_ABORT		7
#define PROCESS_INFO_EXIT_SPAWN		8

/* the process a debugged process exits to, and its PRX debug flags */
typedef struct DbgpSpawnInfo {
	SceUID pid;
	SceUInt32 prxDebugFlag;
} DbgpSpawnInfo;

typedef struct __attribute__((packed, aligned(4))) ProcessInfoBuf {
	int write;
	int read;
	int count;
	ProcessInfo info[4];
} ProcessInfoBuf;

/* ModuleInfo.type */
#define MODULE_INFO_START	0
#define MODULE_INFO_STOP	1

typedef struct ModuleInfo {
	SceUInt32 type;
	SceUID pid;
	SceUID modid;
	DbgpTime time;
} ModuleInfo;

typedef struct ModuleInfoBuf {
	int write;
	int read;
	int count;
	ModuleInfo info[8];
} ModuleInfoBuf;

_Static_assert(sizeof(ExcpInfoBuf) == 0x314, "debugger exception queue");

/* the size of each of a process's receive, send and work buffers */
#define DBGP_WORK_BUF_SIZE	0x11000

/* the most threads the debugger reports for a process */
#define DBGP_THREAD_MAX		192

/* the thread list procGetThreadInfo reads from SceThreadmgr: 192 entries of 0x128 */
#define DBGP_THREAD_LIST_SIZE	0xde00

/* why a thread stopped (procGetThreadDbgpStopReason) */
typedef struct DbgpStopReason {
	SceSize size;
	SceUInt32 reason;
	SceUInt32 data;			/* the breakpoint or watchpoint */
	SceUInt32 reserved[2];
} DbgpStopReason;

/* a process as the host sees it, followed by its path and unwind tables */
typedef struct DbgpProcessInfo {
	SceSize size;
	SceUID pid;
	SceUInt32 attr;			/* DBGP_PROCESS_ATTR_* */
	char name[32];			/* 0x0c */
	SceUInt32 param;		/* 0x2c */
	SceUInt32 cpuAffinityMask;
	void *entryPoint;
	SceUInt32 fingerprint;		/* 0x38 */
	SceUID parentPid;		/* -1 for the kernel */
	DbgpStopReason stopReason;	/* 0x40 */
	SceSize pathLength;		/* 0x54: with its NUL */
	char path[];			/* padded to 4, then DbgpProcessUnwind */
} DbgpProcessInfo;

/* the main module's unwind tables, after the padded path */
typedef struct DbgpProcessUnwind {
	void *exidxTop;
	void *exidxBtm;
	void *extabTop;
	void *extabBtm;
} DbgpProcessUnwind;

/* a 32-byte name, copied whole (Sony copies it word by word) */
typedef struct __attribute__((may_alias)) DbgpName {
	char name[32];
} DbgpName;

/* DbgpProcessInfo.attr */
#define DBGP_PROCESS_ATTR_VALID		0x1
#define DBGP_PROCESS_ATTR_SUSPENDED	0x2	/* process status 2 */
#define DBGP_PROCESS_ATTR_SYSTEM	0x4
#define DBGP_PROCESS_ATTR_STOPPED	0x8	/* process status 3 */
#define DBGP_PROCESS_ATTR_DEBUGGABLE	0x10000
#define DBGP_PROCESS_ATTR_ATTACHED	0x20000
#define DBGP_PROCESS_ATTR_ULT_INFO	0x1000000

_Static_assert(sizeof(DbgpProcessInfo) == 88, "DBGP process record");

/* a thread as the host sees it (from SceThreadmgr's debugger information) */
typedef struct DbgpThreadInfo {
	SceSize size;
	SceUID thid;
	char name[32];
	SceUInt32 initAttr;		/* 0x28 */
	SceUInt32 attr;
	SceUInt32 status;		/* 0x30: DBGP_THREAD_STATUS_* | THREAD_STATUS_* */
	SceUInt32 entry;
	SceUInt32 stackBase;		/* 0x38: the user stack */
	SceSize stackSize;
	SceSize stackMaxUsed;
	SceSize argSize;		/* 0x44 */
	SceUInt32 argBlock;
	int initPriority;
	int currentPriority;		/* 0x50 */
	SceUInt32 initCpuAffinityMask;
	SceUInt32 cpuAffinityMask;
	int lastCpuId;			/* 0x5c */
	SceUInt32 waitType;
	SceUID waitId;
	SceUInt32 runClocksLow;		/* 0x68 */
	SceUInt32 runClocksHigh;
	DbgpStopReason stopReason;	/* 0x70 */
	int exitStatus;			/* 0x84 */
	SceUInt32 intrPreemptCount;
	SceUInt32 threadPreemptCount;
	SceUInt32 threadReleaseCount;	/* 0x90 */
	SceUInt32 changeCpuCount;
	SceUInt32 valid;		/* 0x98: always 1 */
	SceUInt32 pc;
	SceUInt32 timeout;		/* 0xa0 */
	SceUInt32 waitDetail[2];	/* by wait type */
} DbgpThreadInfo;

/* the extended record (DBGP thread type 2) */
typedef struct DbgpThreadInfoEx {
	DbgpThreadInfo info;
	SceUInt32 waitDetail[9];	/* 0xac: by wait type, in place of info.waitDetail */
} DbgpThreadInfoEx;

/* DbgpThreadInfo.status bits besides the thread's own */
#define DBGP_THREAD_STATUS_STOPPED	0x1		/* the thread the process stopped on */
#define DBGP_THREAD_STATUS_SYSCALL	0x80000000	/* in a system call: pc is the caller's */

/* the threads part of a stop notification, followed by the stop time */
typedef struct DbgpStopThreads {
	SceUInt32 reserved;
	SceUID thid;
	SceUInt32 count;
	DbgpThreadInfo info[];
} DbgpStopThreads;

_Static_assert(sizeof(DbgpThreadInfo) == 172, "DBGP thread record");
_Static_assert(sizeof(DbgpThreadInfoEx) == 208, "DBGP extended thread record");
_Static_assert(offsetof(DbgpThreadInfo, stopReason) == 0x70, "DBGP thread stop reason");
_Static_assert(sizeof(DbgpStopThreads) == 12, "DBGP stop thread list");
_Static_assert(sizeof(ThreadInfoBuf) == 0x60c, "debugger thread queue");
_Static_assert(sizeof(ProcessInfoBuf) == 0x6c, "debugger process queue");
_Static_assert(sizeof(ModuleInfoBuf) == 0xac, "debugger module queue");

/* Persistent debugger bookkeeping, separate from the live CPU registers. */
typedef struct SwbkptInfo {
	SceUInt32 id;
	void *address;
	SceUInt32 attr;
	union {
		SceUInt32 opcode;
		SceUInt8 bytes[4];
	} original;
	SceUInt32 requestData;
	SceUInt32 hitCount;
	SceUInt32 active;
	SceUInt32 reserved;
	SceUID pid;
	SceUID thid;
	SceUID userThid;
	SceUID modid;
} SwbkptInfo;

/* A software breakpoint as the host sets and reads it. */
typedef struct DbgpSwbrkptInfo {
	SceUInt32 size;
	SceUInt32 id;
	SceUID thid;
	void *address;
	SceUInt32 attr;
	SceUInt32 data;
	SceUInt32 extSize;
	SceUInt32 ext;
} DbgpSwbrkptInfo;

typedef struct DbgpSetSwbrkptRequest {
	DbgpRequestHeader header;
	DbgpSwbrkptInfo info;
} DbgpSetSwbrkptRequest;

typedef struct DbgpSetSwbrkptResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUInt32 id;
	SceUInt32 opcodeBits;
	SceUInt32 opcode;
} DbgpSetSwbrkptResponse;

typedef struct DbgpClearSwbrkptRequest {
	DbgpRequestHeader header;
	SceUInt32 id;
} DbgpClearSwbrkptRequest;

typedef struct DbgpGetSwbrkptResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUInt32 count;
	DbgpSwbrkptInfo info[];
} DbgpGetSwbrkptResponse;

_Static_assert(sizeof(DbgpSwbrkptInfo) == 32, "breakpoint packet record");
_Static_assert(offsetof(DbgpGetSwbrkptResponse, info) == 36,
	       "breakpoint list response");

/* A breakpoint a stepped thread lifts and puts back. */
typedef struct SwbkptSpecificInfo {
	SceSize size;
	void *address;
	SceUID pid;
	SceUID thid;
	SceUInt32 thumb;
	union {
		SceUInt32 opcode;
		SceUInt8 bytes[4];
	} original;
	SceUInt32 index;
} SwbkptSpecificInfo;

_Static_assert(sizeof(SwbkptSpecificInfo) == 28, "step breakpoint record");

/* A thread list a resume command names (one process). */
typedef struct DbgpThreadListRequest {
	DbgpRequestHeader header;
	int count;
	SceUID thid[];
} DbgpThreadListRequest;

typedef struct DbgpThreadListResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUInt32 count;
	SceUID thid[];
} DbgpThreadListResponse;

_Static_assert(offsetof(DbgpThreadListRequest, thid) == 28, "thread list request");
_Static_assert(offsetof(DbgpThreadListResponse, thid) == 36, "thread list response");

typedef struct HwwtptInfo {
	SceUInt32 id;
	void *address;
	SceUInt32 control;
	SceUInt32 active;
	SceUID pid;
	SceUInt32 reserved;
} HwwtptInfo;

/* A hardware watchpoint as the host sets and reads it. */
typedef struct DbgpHwwatchptInfo {
	SceUInt32 size;
	SceUInt32 id;
	void *address;
	SceUInt32 control;
} DbgpHwwatchptInfo;

typedef struct DbgpGetHwwatchptResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUInt32 count;
	DbgpHwwatchptInfo info[];
} DbgpGetHwwatchptResponse;

_Static_assert(sizeof(DbgpHwwatchptInfo) == 16, "watchpoint packet record");

typedef struct DbgpProcCB {
	SceKernelSpinlock lock;
	SceUInt32 status;
	SceUID bufferMemBlock;
	DbgpBuf buf[3];
	SceUID fragmentPid;
	SceSize fragmentOffset;
	SceUInt32 fragmentSequence;
	int skipIndex;
	SceUID pid;
	SceUID prxModid;
	SceUInt32 prxStopReason;
	SceUInt32 intrStopReason;
	DbgpTime time;
	SceUInt32 prxDebugFlag;
	SceUID eventFlag;
	SceUID prxDebugEventFlag;
	SceUID prxSema;
	SceUID spontaneousExitSema;
	SceUID workerThread;
	SceUID stoppedThread;
	SceUID currentThread;
	SceUInt32 faultAddress;
	SceUInt32 ulobjmgrFlag;
	SwbkptInfo swbkpt[256];
	StepSwbkptInfo stepSwbkpt[3];
	HwwtptInfo hwwtpt[3];
	const DbgpThreadListRequest *resumePacket;
	SceUID resumePid;
	SceUID resumeThid;
	ExcpInfoBuf stopInfoBuf;
	ThreadInfoBuf threadInfoBuf;
	ProcessInfoBuf procInfoBuf;
	ModuleInfoBuf prxInfoBuf;
	char coredumpPath[0x534];
	CoredumpInfoBuf coredumpInfoBuf;
} DbgpProcCB;

typedef struct DbgpProcessDebugInfo {
	SceUID pid;
	void *codeBase;
	SceSize codeSize;
} DbgpProcessDebugInfo;

_Static_assert(sizeof(DbgpProcessDebugInfo) == 12,
	       "debugger executable allocation record");

typedef struct DbgpCB {
	SceKernelSpinlock lock;
	struct ThreadmgrFastMutex mutex;
	SceUInt32 status;
	int protocol;
	SceUID heap;
	SceUID recvPid;
	SceUID sendPid;
	SceUID deferredPid;
	SceUID sendQueue[32];
	SceKernelSpinlock sendLock;
	int sendWrite;
	int sendRead;
	int sendCount;
	int procCount;
	int procActive[17];
	DbgpProcCB *procCB[17];
	DbgpSpawnInfo spawn;
	SceKernelSpinlock procLock;
	DbgpProcessDebugInfo *processDebugInfo;
} DbgpCB;

extern DbgpCB g_dbgp;

int _dbgpWorkerThread(SceSize args, void *argp);
SceSize _dbgpGetRecvSize(DbgpProcCB *proc);
void *_dbgpAllocSendBuf(DbgpProcCB *proc, SceSize size);
void _dbgpSetSendSize(DbgpProcCB *proc, SceSize size);
void *_dbgpAllocWorkBuf(DbgpProcCB *proc, SceSize size);
void dbgpFreeWorkBuf(DbgpProcCB *proc);
void dbgpFlushCache(void *address);
SceUInt32 dbgpKernelErrToDbgpErr(int error);
DbgpProcCB *dbgpGetProcCB(SceUID pid);
DbgpCB *dbgpGetDbgpCB(void);
int dbgpIsApplicationActive(void);
int dbgpCheckSwbrkptSize(const SceSize *size);
int dbgpCheckDuplicateSwbrkpt(DbgpProcCB *proc, SceUID pid,
			     SceUID userThid, const void *address);
int checkInvalidRegisterId(SceUInt32 id);
int checkInvalidMemorySize(SceSize size);
int checkInvalidWatchpointControl(SceUInt32 control);
int checkInvalidNetInterface(SceUInt32 ifnum);
int checkInvalidNetEmulationQuery(SceNetPsDebugEmulationSelector query);
int checkInvalidNetEmulationRequest(SceNetPsDebugEmulationRequest request);
int _dbgpEventRecv(int event, void *arg, void *ctx);
int dbgpCreateWorkerThread(SceUID pid);
int dbgpDeleteWorkerThread(SceUID pid);
int dbgpDetachAllProcess(DbgpCB *dbgp);
int dbgpClearPrxDebug(DbgpProcCB *proc);
void dbgpSendError(DbgpProcCB *proc, SceUInt8 group, SceUInt8 type,
		   SceUInt8 code, SceUInt32 result);
void dbgpFillResponseHeader(DbgpProcCB *proc, SceUInt8 group, SceUInt8 type,
			    SceUInt8 code, SceUInt32 result);
SceUInt32 dbgpTestStatus(const DbgpCB *dbgp, SceUInt32 mask);
SceUInt32 dbgpTestProcStatus(const DbgpProcCB *proc, SceUInt32 mask);
void dbgpSetProcStatus(DbgpProcCB *proc, SceUInt32 mask, int set);
void dbgpFreeRecvBuf(DbgpProcCB *proc);
void _dbgpFreeWorkBuf(DbgpProcCB *proc);
void _dbgpFreeBuf(DbgpProcCB *proc, unsigned int which);
int _dbgpReqSend(DbgpProcCB *proc);
int _dbgpParseThreadInfo(DbgpProcCB *proc);
int _dbgpParseModuleInfo(DbgpProcCB *proc);
int _dbgpParseProcessInfo(DbgpProcCB *proc);

int excpHandlerInit(DbgpCB *dbgp);
int intrHandlerInit(DbgpCB *dbgp);
int threadHandlerInit(DbgpCB *dbgp);
int processHandlerInit(DbgpCB *dbgp);
int moduleHandlerInit(DbgpCB *dbgp);
int procInit(DbgpCB *dbgp);
int swbkptInit(DbgpCB *dbgp);
int threadAppObjectInit(DbgpCB *dbgp);
int threadAppObjectIsSupported(SceUID pid);

int processHandlerStartHandler(SceUID parentPid, SceUID pid, SceUInt64 time);
int processHandlerExitHandler(SceUID pid, int exitStatus, SceUInt64 time);
int processHandlerKillHandler(SceUID pid, SceUInt64 time);
int processHandlerSpontaneousExitHandler(SceUID pid, SceUInt32 status, SceUInt64 time);
int processHandlerExitToSpawnHandler(SceUID pid, SceUID spawnPid, SceUInt64 time);
int processHandlerExitSpawnHandler(SceUID pid, SceUID spawnPid, int result,
				   SceUInt64 time);
int processHandlerSuspendHandler(SceUID pid);
int processHandlerResumeHandler(SceUID pid);
int processHandlerAppAbortHandler(SceUID pid);
int moduleHandlerStartHandler(SceUID pid, SceUID modid, SceUInt64 time);
int moduleHandlerStopHandler(SceUID pid, SceUID modid, SceUInt64 time);
int dbgpIsDebugTarget(SceUID pid);
int dbgpIsPsButtonCaptured(void);
int swbkptGetOriginalOpcode(SceUID pid, void *address, SceUInt32 *opcode);
int swbkptSetNewOpcode(SceUID pid, SceUInt32 address, SceUInt32 instruction);
int procGetPrxDebugFlag(SceUID pid, SceUInt32 *flag);
int _dbgpStopProcessForPrxDebug(SceUID pid, SceUID modid,
			      SceUInt32 reason, SceUInt64 time);
int dbgpIsProcessAttached(SceUID pid);
int hwwtptEnableHwwtpt(const SysrootDbgpOps *ops);
int hwwtptDisableHwwtpt(const SysrootDbgpOps *ops);
int _dbgpUpdateUlobjmgrFlagForDbgp(DbgpCB *dbgp, DbgpProcCB *proc,
				 SceUInt32 set, SceUInt32 clear, SceUInt32 *flag);
int _dbgpUpdateUlobjmgrFlagForLibfiber(SceUID pid, SceUInt32 *flag);

/* thread_info.c */
int threadInfoSave(DbgpProcCB *proc, const ThreadInfo *info);
int threadInfoLoad(DbgpProcCB *proc, ThreadInfo *info);
int threadInfoCreateThreadNotification(DbgpProcCB *proc, void *packet,
				       const ThreadInfo *info);
int threadInfoCreateExitNotification(DbgpProcCB *proc, void *packet,
				     const ThreadInfo *info);

/* module_info.c */
int moduleInfoSave(DbgpProcCB *proc, const ModuleInfo *info);
int moduleInfoLoad(DbgpProcCB *proc, ModuleInfo *info);
int _createModuleLoadNotification(DbgpProcCB *proc, void *packet,
				  const ModuleInfo *info);

/* process_info.c and process.c */
int procSuspendProcess(DbgpProcCB *proc, SceUID pid, SceUID thid,
		       SceUInt32 reason);
int processInfoSave(DbgpProcCB *proc, const ProcessInfo *info);
int processInfoLoad(DbgpProcCB *proc, ProcessInfo *info);
int processInfoCreateProcessNotification(DbgpProcCB *proc, void *packet,
					 const ProcessInfo *info);
int procDebugSuspendProcess(SceUID pid);
int procCreateStopNotification(DbgpProcCB *proc, SceUInt32 reason);

int checkInvalidProcess(SceUID pid);
int checkUnknownProcess(SceUID pid);
int checkInvalidThread(SceUID pid, SceUID thid);
int checkInvalidModule(SceUID pid, SceUID modid);
int checkInvalidOptionalThread(SceUID pid, SceUID thid);
int checkInvalidGetRegisterIds(const DbgpRegisterRequest *request,
			       SceUInt32 count);
int checkInvalidSetRegisterIds(const DbgpRegisterRequest *request,
			       SceUInt32 count);
int checkReadOnlyRegisters(const DbgpRegisterRequest *request);
int checkInvalidMemoryRange(SceUID pid, const void *address, SceSize size,
			    SceUInt32 mode);
int checkInvalidAddressAlignment(const void *address, SceUInt32 type);
int checkInvalidWatchpointId(const DbgpProcCB *proc, int id);
int checkNoFreeWatchpoint(const DbgpProcCB *proc);
int checkNotExecutable(SceUID pid, const void *address,
		       SceKernelModuleInfoForDebugger *modules);
int checkInvalidSwbrkptId(const DbgpProcCB *proc, int id);
int checkNoFreeSwbrkpt(const DbgpProcCB *proc);
int checkIllegalProcessStatus(SceUID pid);
int checkIllegalProcessStatusResumeProcess(SceUID pid);
int checkAttachedProcess(const DbgpProcCB *proc, SceUID pid);
int checkAlreadyAttachedProcess(SceUID pid);
int checkModuleInfoProcess(SceUID pid);
int checkProcessIsSuspendedAlready(SceUID pid);
int checkIllegalThreadStatusResumeThread(SceUID pid, SceUID thid);
int checkIllegalThreadStatusResumeProcess(SceUID pid, SceUID thid,
					 int *isBreakpoint);
int checkThreadIsSuspendedAlready(SceUID pid, SceUID thid);
int checkInvalidAddressOverflow(const void *address, SceSize size);
int checkIllegalApplicationStatus(SceUID pid);
int checkApplicationIsSuspendedAlready(SceUID pid);
int checkApplicationIsSuspended(SceUID pid);
int checkInvalidAllocPageSize(SceSize size);
int checkGetTargetAddrOpcode(const void *address, SceUInt32 cpsr, SceUID pid,
			     SceUInt32 *armOpcode, SceUInt16 *thumbOpcode,
			     int *isThumb);
int checkGetTargetAddrStopCause(DbgpProcCB *proc, const void *address,
				SceUInt32 cpsr, SceUID pid, SceUInt32 *flags);
int checkGetTargetAddrStopCauseDabt(DbgpProcCB *proc, const void *address,
				    SceUInt32 cpsr, SceUID pid);
int isDbgpThreadPcBkpt(SceUID pid, const void *address, SceUInt32 cpsr);
int swbkptHasAddress(const DbgpProcCB *proc, SceUID pid, const void *address);
int processSuspendProcessByDBGP(SceUID pid, int *alreadySuspended);
int processResumeProcessByDBGP(SceUID pid, int alreadySuspended);
void processClearDebugInfo(SceUID pid);
int processWaitThreadSuspendForProcess(SceUID pid, SceUID *threads, int mode);
int processChangeStatusThreadResumeForProcess(SceUID pid, SceUID *threads);
int processThreadStatusIntegration(SceUID pid, int mode, SceUID *threads);
int swbkptClearSwbrkptDetach(DbgpProcCB *proc, SceUID pid);
int swbkptGetAlignId(const DbgpProcCB *proc, const void *address,
		     DbgpThreadIdentity thread, SceUInt32 *stopCause);
int swbkptGetSwbrkptCount(const DbgpProcCB *proc, SceUID pid);
int swbkptGetSwbrkpt(const DbgpProcCB *proc, DbgpGetSwbrkptResponse *reply,
		     int id);
int swbkptSetSwbrkpt(DbgpProcCB *proc, const DbgpSetSwbrkptRequest *request,
		     DbgpSetSwbrkptResponse *reply);
int swbkptSetSpecificSwbkpt(DbgpProcCB *proc, SwbkptSpecificInfo info);
int swbkptClearSwbrkpt(DbgpProcCB *proc, const DbgpClearSwbrkptRequest *request,
		       int keep);
int swbkptClearSwbrkptProcess(DbgpProcCB *proc, SceUID pid);
int swbkptClearSwbrkptModule(DbgpProcCB *proc, SceUID pid, SceUID modid);
int swbkptGetSpecificSwbkpt(const DbgpProcCB *proc, SceUID pid, SceUID thid,
			    void *address, SwbkptSpecificInfo *info);
int excpInfoCancel(DbgpProcCB *proc, SceUID pid, SceUID thid,
		   const void *pc);
int hwwtptClearHwwtpt(DbgpProcCB *proc, SceUID pid, int id, SceUInt32 flags);
int hwwtptGetHitId(const DbgpProcCB *proc, DbgpThreadIdentity thread);
int hwwtptGetHwwtptCount(const DbgpProcCB *proc, SceUID pid);
int hwwtptGetHwwtpt(const DbgpProcCB *proc, DbgpGetHwwatchptResponse *reply,
		    int id);
int hwwtptSetHwwtpt(DbgpProcCB *proc, const DbgpHwwatchptInfo *info,
		    SceUID pid, SceUInt32 *id);
int isProcessNotification(SceUInt8 group, SceUInt8 type);

int stepThreadSpecificBpPre(DbgpProcCB *proc);
int stepThreadSpecificBpPost(DbgpProcCB *proc);
int stepAutoSkipBpPre(DbgpProcCB *proc, const DbgpThreadListRequest *packet);
int stepAutoSkipBpMid(DbgpProcCB *proc);
int stepAutoSkipBpPost(DbgpProcCB *proc);
int excpInfoSave(DbgpProcCB *proc, const DbgpThreadIdentity *thread,
		 SceUInt32 reason, SceUInt32 pc, int kind);
int excpInfoLoad(DbgpProcCB *proc, DbgpThreadIdentity *thread,
		 SceUInt32 *reason, SceUInt32 *pc);
int excpInfoClearAll(DbgpProcCB *proc, SceUID pid);
int excpInfoGetCount(DbgpProcCB *proc);
int excpInfoCreateStopNotification(DbgpProcCB *proc,
				   const DbgpThreadIdentity *thread,
				   SceUInt32 reason, SceUInt32 pc);
int procGetProcInfoForPacket(DbgpProcCB *proc, SceUID pid);
int procGetProcessInfo(DbgpProcCB *proc, void *info, SceUID pid,
		       SceUInt32 reason, SceUInt32 pc);
int procGetProcessAttribute(DbgpProcCB *proc, SceUID pid, SceUInt32 *attr);
void procGetProcessDbgpStopReason(DbgpProcCB *proc, DbgpStopReason *stop,
				  SceUID pid, SceUInt32 reason, SceUInt32 pc);
int procGetThreadDbgpStopReason(DbgpProcCB *proc, DbgpStopReason *reason,
				SceUID thid, SceUID pid, SceUInt32 stopReason);
int procGetThreadInfo(DbgpProcCB *proc, DbgpThreadInfo *info, int *count,
		      SceUID pid, void *work, SceUInt32 reason);
int threadInfoGetCount(DbgpProcCB *proc);
int processInfoGetCount(DbgpProcCB *proc);
int moduleInfoGetCount(DbgpProcCB *proc);
int moduleInfoSendPrxLoadNotificationFake(DbgpProcCB *proc);
int coredumpInfoSave(DbgpProcCB *proc, const CoredumpInfo *info);
int coredumpInfoLoad(DbgpProcCB *proc, CoredumpInfo *info);
int coredumpInfoGetCount(DbgpProcCB *proc);
int coredumpInfoCreateCoredumpNotification(DbgpProcCB *proc, void *packet,
					 const CoredumpInfo *info);

/* Command handlers read the packet retained in receive buffer zero. */
int systemGetConfCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemGetRegCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemSetRegCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemRdmemCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemWrmemCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemGetHwwatchptCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemSetHwwatchptCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemClearHwwatchptCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemGetSwbrkptCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemSetSwbrkptCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int systemClearSwbrkptCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processGetProcessInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processAttachProcessCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processDetachProcessCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processSuspendProcessCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processResumeProcessCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processSuspendApplicationCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processResumeApplicationCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processAllocCodePageCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processAllocDataPageCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int processGetMemBlockInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int _getThreadInfoNormal(DbgpProcCB *proc, const DbgpPrefix *packet);
int _getThreadInfoEx(DbgpProcCB *proc, const DbgpPrefix *packet);
int threadSuspendThreadCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int threadResumeThreadCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int threadGetThreadNamesCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int _getStackInfo(DbgpProcCB *proc, const DbgpPrefix *packet);
int _getSyncPrimitiveInfo(DbgpProcCB *proc, const DbgpPrefix *packet);
int threadGetAppObjectCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int threadGetFiberStackInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int _getUltSyncObjInfoSizeOfType(SceUID pid, int type);
int _getUltSyncObjInfo(SceUID pid, int type, SceUInt32 *buf);
int _getUltSyncObjInfoSize(SceUID pid, SceUInt32 num, const int *types);
int moduleGetModInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int moduleGetMetaDataCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int moduleGetModuleListCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int fileGetFdInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int GetNetSystemInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int GetNetSockInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int GetNetInterfaceInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int GetNetResolverInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int GetNetEmulationSettingCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int SetNetEmulationSettingCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int coredumpHandlerResultCallback(int task_id, SceUID pid, int result,
				  const char *path, SceSize path_len,
				  int encrypted);
int coredumpHandlerProgressCallback(int task_id, SceUID pid, int progress);
int coredumpTriggerCoredumpCmd(DbgpProcCB *proc, const DbgpPrefix *packet);
int coredumpCancelCoredumpCmd(DbgpProcCB *proc, const DbgpPrefix *packet);

_Static_assert(sizeof(SwbkptInfo) == 0x30, "software breakpoint record");
_Static_assert(sizeof(HwwtptInfo) == 0x18, "hardware watchpoint record");
_Static_assert(sizeof(DbgpProcCB) == 0x4344, "debugger process control block");
_Static_assert(offsetof(DbgpProcCB, ulobjmgrFlag) == 0x16c,
	       "debugger user-level object flags");
_Static_assert(offsetof(DbgpProcCB, coredumpInfoBuf) == 0x4178,
	       "debugger coredump queue position");
_Static_assert(sizeof(DbgpCB) == 0x188, "debugger control block");
_Static_assert(offsetof(DbgpCB, procLock) == 0x180,
	       "debugger process registry lock");
