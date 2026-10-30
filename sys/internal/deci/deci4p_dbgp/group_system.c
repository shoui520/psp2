/*
 * The DBGP system group: the debugger's configuration, a thread's
 * registers, process memory, and software breakpoints and hardware
 * watchpoints (Sony's group_system.c).
 */

#include <kernel/error.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadcb.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include <threadmgr/threadmgr_vfp.h>
#include "dbgp_internal.h"

/* the register groups of a register ID: group | index */
#define DBGP_REG_GROUP(id)	((id) >> 16 << 16)
#define DBGP_REG_INDEX(id)	((id) & 0xffff)
#define DBGP_REG_GPR		0x10000		/* r0-r15 */
#define DBGP_REG_STATUS		0x20000		/* cpsr, fpscr, fpexc */
#define DBGP_REG_CPSR		0x20000
#define DBGP_REG_FPSCR		0x20001
#define DBGP_REG_FPEXC		0x20002
#define DBGP_REG_SYSTEM		0x30000		/* CP15 */
#define DBGP_REG_TLS		0x30000
#define DBGP_REG_TPIDRURW	0x30001
#define DBGP_REG_CPACR		0x30002
#define DBGP_REG_DACR		0x30003
#define DBGP_REG_FAULT		0x40000		/* the debug unit and the fault registers */
#define DBGP_REG_DBGDSCR	0x40000
#define DBGP_REG_IFSR		0x40001
#define DBGP_REG_IFAR		0x40002
#define DBGP_REG_DFSR		0x40003
#define DBGP_REG_DFAR		0x40004
#define DBGP_REG_QUAD		0x60000		/* q0-q15 */

/* the DBGDSCR bits the debugger reports, and the fixed ones it adds */
#define DBGP_DBGDSCR_MASK	0x1fc
#define DBGP_DBGDSCR_FIXED	0x1371002

/* the value of a VFP register of a thread that never used the VFP */
#define DBGP_VFP_UNUSED		0x7ff8dead7f80deadULL

#define CPSR_MODE_MASK		0x1f
#define CPSR_MODE_USER		0x10

/* the debugger's configuration (DBGP system type 1) */
typedef struct DbgpConfResponse {
	DbgpResponseHeader header;
	SceSize size;			/* of the fields from here */
	SceUInt32 count;		/* the fields from here */
	SceUInt32 swbkptMax;
	SceUInt32 hwbkptMax;
	SceUInt32 reserved;
	SceUInt32 hwwtptMax;
	SceUInt32 sdkVersion;		/* of the system software */
} DbgpConfResponse;

/* what systemGetRegCmd reads a thread's registers into */
typedef struct DbgpRegisterWork {
	ThreadmgrThreadInfoForDebugger info;
	ThreadmgrDebugRegisters regs[2];	/* switched out, then the user frame */
	union {
		SceUInt64 d[32];
		SceUInt32 s[64];
	} vfp;
} DbgpRegisterWork;

typedef struct DbgpRegisterValue {
	SceUInt32 value[4];
} DbgpRegisterValue;

typedef struct DbgpRegisterResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUID thid;
	SceUInt32 count;
	DbgpRegisterValue registers[];
} DbgpRegisterResponse;

typedef struct DbgpMemoryRequest {
	DbgpPrefix prefix;
	SceUID pid;
	void *address;
	SceSize size;
	SceUInt8 data[];		/* systemWrmemCmd */
} DbgpMemoryRequest;

typedef struct DbgpMemoryResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceSize size;
	SceUInt8 data[];		/* systemRdmemCmd, padded to 4 */
} DbgpMemoryResponse;

/* the breakpoint or watchpoint a get or clear command names */
typedef struct DbgpPointIdRequest {
	DbgpRequestHeader header;
	int id;				/* -1: all of the process's */
} DbgpPointIdRequest;

typedef struct DbgpPointIdResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUInt32 id;
} DbgpPointIdResponse;

typedef struct DbgpSetHwwatchptRequest {
	DbgpRequestHeader header;
	DbgpHwwatchptInfo info;
} DbgpSetHwwatchptRequest;

_Static_assert(sizeof(DbgpPointIdResponse) == 0x24, "DBGP point response");
_Static_assert(sizeof(DbgpSetHwwatchptRequest) == 0x28, "DBGP watchpoint request");
_Static_assert(sizeof(DbgpMemoryRequest) == 0x20, "DBGP memory request");
_Static_assert(sizeof(DbgpMemoryResponse) == 0x24, "DBGP memory response");
_Static_assert(sizeof(DbgpConfResponse) == 0x38, "DBGP configuration");
_Static_assert(offsetof(DbgpRegisterWork, vfp) == 0x1b8, "register work vfp");
_Static_assert(sizeof(DbgpRegisterResponse) == 0x28, "DBGP register response");

static int _setRegister(DbgpRegisterWork *work,
			const DbgpRegisterRequest *request);

int systemGetConfCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	DbgpConfResponse *conf;

	if (_dbgpGetRecvSize(proc) < sizeof(DbgpPrefix)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	conf = _dbgpAllocSendBuf(proc, sizeof(*conf));
	if (conf == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	_dbgpSetSendSize(proc, sizeof(*conf));
	memset(conf, 0, sizeof(*conf));
	dbgpFillResponseHeader(proc, packet->group, 1, 0, 0);
	conf->size = sizeof(*conf) - offsetof(DbgpConfResponse, size);
	conf->count = (sizeof(*conf) - offsetof(DbgpConfResponse, size)) /
		      sizeof(SceUInt32);
	conf->swbkptMax = 256;
	conf->hwbkptMax = 0;
	conf->reserved = 0;
	conf->hwwtptMax = 3;
	conf->sdkVersion = FW_SDK_VERSION;
	return 0;
}

int systemGetRegCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpRegisterRequest *request = (const DbgpRegisterRequest *)packet;
	DbgpRegisterResponse *response;
	DbgpRegisterValue *value;
	DbgpRegisterWork *work;
	const ThreadmgrDebugRegisters *user, *kernel;
	const ThreadRegisterContext *context;
	const struct ThreadmgrVfpContext *vfp;
	const SceUInt32 *vfpQuad;
	SceUInt32 count, id, i;
	SceUID pid, thid, guid;
	SceSize size;
	int result;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	count = request->count;
	thid = request->thid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	if (checkInvalidThread(pid, thid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20012);
		return (int)0x80080001;
	}
	if (checkInvalidRegisterId(count) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
		return (int)0x80080001;
	}
	if (checkInvalidGetRegisterIds(request, count) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20020);
		return (int)0x80080001;
	}
	result = checkIllegalProcessStatus(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(work, 0, DBGP_WORK_BUF_SIZE);
	size = sizeof(*response) + count * sizeof(DbgpRegisterValue);
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, size);
	_dbgpSetSendSize(proc, size);
	guid = scePUIDtoGUID(pid, thid);
	work->info.size = sizeof(work->info);
	result = SceThreadmgrForDriver_555CA6BB(guid, 0, &work->info);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 223, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20000);
		return result;
	}
	result = SceThreadmgrForDriver_5022689D(guid, work->regs);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 232, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20000);
		return (int)0x80080001;
	}
	result = sceKernelGetVfpRegisterForDebugger(guid, work->vfp.s);
	if (result == SCE_KERNEL_ERROR_ILLEGAL_PERMISSION) {
		for (i = 0; i < 32; i++)
			work->vfp.d[i] = DBGP_VFP_UNUSED;
	} else if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 249, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20000);
		return (int)0x80080001;
	}
	context = (const ThreadRegisterContext *)work->info.registers;
	vfp = (const struct ThreadmgrVfpContext *)work->info.vfpContext;
	user = &work->regs[0];
	kernel = &work->regs[1];
	for (i = 0; i < request->count; i++) {
		id = request->registers[i];
		value = &response->registers[i];
		switch (DBGP_REG_GROUP(id)) {
		case DBGP_REG_GPR:
			if ((user->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER)
				value->value[0] = user->r[DBGP_REG_INDEX(id)];
			else
				value->value[0] = kernel->r[DBGP_REG_INDEX(id)];
			break;
		case DBGP_REG_STATUS:
			switch (id) {
			case DBGP_REG_CPSR:
				if ((user->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER)
					value->value[0] = user->cpsr;
				else
					value->value[0] = kernel->cpsr;
				break;
			case DBGP_REG_FPSCR:
				if ((user->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER)
					value->value[0] = user->fpscr;
				else
					value->value[0] = kernel->fpscr;
				break;
			case DBGP_REG_FPEXC:
				value->value[0] = vfp->fpexc;
				break;
			}
			break;
		case DBGP_REG_SYSTEM:
			switch (id) {
			case DBGP_REG_TLS:
				value->value[0] = work->info.tls;
				break;
			case DBGP_REG_TPIDRURW:
				value->value[0] = context->tpidrurw;
				break;
			case DBGP_REG_CPACR:
				value->value[0] = context->cpacr;
				break;
			case DBGP_REG_DACR:
				value->value[0] = context->userDacr;
				break;
			}
			break;
		case DBGP_REG_FAULT:
			switch (id) {
			case DBGP_REG_DBGDSCR:
				value->value[0] = (context->dbgdscr & DBGP_DBGDSCR_MASK) |
						  DBGP_DBGDSCR_FIXED;
				break;
			case DBGP_REG_IFSR:
				value->value[0] = context->ifsr;
				break;
			case DBGP_REG_IFAR:
				value->value[0] = context->ifar;
				break;
			case DBGP_REG_DFSR:
				value->value[0] = context->dfsr;
				break;
			case DBGP_REG_DFAR:
				value->value[0] = context->dfar;
				break;
			}
			break;
		case DBGP_REG_QUAD:
			vfpQuad = &work->vfp.s[DBGP_REG_INDEX(id) * 4];
			value->value[0] = vfpQuad[0];
			value->value[1] = vfpQuad[1];
			value->value[2] = vfpQuad[2];
			value->value[3] = vfpQuad[3];
			break;
		}
	}
	dbgpFillResponseHeader(proc, packet->group, 5, 0, 0);
	response->pid = pid;
	response->thid = thid;
	response->count = count;
	return 0;
}

int systemSetRegCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpRegisterRequest *request = (const DbgpRegisterRequest *)packet;
	DbgpRegisterResponse *response;
	DbgpRegisterWork *work;
	SceUInt32 count;
	SceUID pid, thid, guid;
	int result;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	thid = request->thid;
	count = request->count;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	if (checkInvalidThread(pid, thid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20012);
		return (int)0x80080001;
	}
	if (checkInvalidRegisterId(count) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
		return (int)0x80080001;
	}
	if (checkInvalidSetRegisterIds(request, count) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20020);
		return (int)0x80080001;
	}
	if (checkReadOnlyRegisters(request) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20022);
		return (int)0x80080001;
	}
	if (checkIllegalProcessStatus(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(work, 0, DBGP_WORK_BUF_SIZE);
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	guid = scePUIDtoGUID(pid, thid);
	work->info.size = sizeof(work->info);
	result = SceThreadmgrForDriver_555CA6BB(guid, 0, &work->info);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 426, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20000);
		return result;
	}
	result = _setRegister(work, request);
	if (result == SCE_KERNEL_ERROR_ILLEGAL_PERMISSION) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20022);
		return result;
	}
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 439, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20022);
		return result;
	}
	dbgpFillResponseHeader(proc, packet->group, 7, 0, 0);
	response->pid = pid;
	response->thid = thid;
	response->count = count;
	return 0;
}

int systemRdmemCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpMemoryRequest *request = (const DbgpMemoryRequest *)packet;
	DbgpMemoryResponse *response;
	SceUInt8 *work;
	SceSize size, padding;
	SceUID pid;
	int alreadySuspended = 0;
	int result, ret, i;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	if (checkInvalidMemorySize(request->size) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
		return (int)0x80080001;
	}
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (processSuspendProcessByDBGP(pid, &alreadySuspended) < 0)
		return (int)0x80080001;
	ret = processWaitThreadSuspendForProcess(pid, (SceUID *)work, 1);
	if (ret < 0) {
		result = (int)0x80080001;
		SCE_PRINTF_WITH_INFO(15, 540, "0x%08x\n", ret);
		goto resume;
	}
	if (checkInvalidMemoryRange(pid, request->address, request->size,
				    0x10) != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20030);
		goto resume;
	}
	result = checkInvalidAddressOverflow(request->address, request->size);
	if (result != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20070);
		goto resume;
	}
	padding = request->size & 3;
	if (padding != 0)
		padding = 4 - padding;
	size = sizeof(*response) + request->size + padding;
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL) {
		result = SCE_KERNEL_ERROR_NO_MEMORY;
		goto resume;
	}
	memset(response, 0, size);
	_dbgpSetSendSize(proc, size);
	memset(work, 0, request->size);
	ret = sceKernelCopyFromUserProc(pid, work, request->address,
					request->size);
	if (ret < 0) {
		result = ret;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20030);
		goto resume;
	}
	for (i = 0; i < (int)request->size; i++)
		response->data[i] = work[i];
	dbgpFillResponseHeader(proc, packet->group, 0x11, 0, 0);
	response->pid = pid;
	response->size = request->size;
	if (processResumeProcessByDBGP(pid, alreadySuspended) < 0)
		return (int)0x80080001;
	return result;

resume:
	processResumeProcessByDBGP(pid, alreadySuspended);
	return result;
}

int systemWrmemCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpMemoryRequest *request = (const DbgpMemoryRequest *)packet;
	DbgpMemoryResponse *response;
	SceUID *work;
	SceUID pid;
	int alreadySuspended = 0;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	if (checkInvalidMemorySize(request->size) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (processSuspendProcessByDBGP(pid, &alreadySuspended) < 0)
		return (int)0x80080001;
	ret = processWaitThreadSuspendForProcess(pid, work, 1);
	if (ret < 0) {
		result = (int)0x80080001;
		SCE_PRINTF_WITH_INFO(15, 728, "0x%08x\n", ret);
		goto resume;
	}
	if (checkInvalidMemoryRange(pid, request->address, request->size,
				    0x10) != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20030);
		goto resume;
	}
	result = checkInvalidAddressOverflow(request->address, request->size);
	if (result != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20070);
		goto resume;
	}
	ret = sceKernelCopyToUserProcTextDomainForDBGP(pid, request->address,
						       request->data,
						       request->size);
	if (ret < 0) {
		result = ret;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20030);
		goto resume;
	}
	sceKernelL1IcacheInvalidateEntireAllCore();
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL) {
		result = SCE_KERNEL_ERROR_NO_MEMORY;
		goto resume;
	}
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	dbgpFillResponseHeader(proc, packet->group, 0x13, 0, 0);
	response->pid = pid;
	response->size = request->size;
	if (processResumeProcessByDBGP(pid, alreadySuspended) < 0)
		return (int)0x80080001;
	return result;

resume:
	processResumeProcessByDBGP(pid, alreadySuspended);
	return result;
}

int systemGetSwbrkptCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpPointIdRequest *request = (const DbgpPointIdRequest *)packet;
	DbgpGetSwbrkptResponse *response;
	SceUID *work;
	SceUID pid;
	SceSize size;
	SceUInt32 count;
	int alreadySuspended = 0;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->header.pid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (processSuspendProcessByDBGP(pid, &alreadySuspended) < 0)
		return (int)0x80080001;
	ret = processWaitThreadSuspendForProcess(pid, work, 1);
	if (ret < 0) {
		result = (int)0x80080001;
		SCE_PRINTF_WITH_INFO(15, 898, "0x%08x\n", ret);
		goto resume;
	}
	if (checkInvalidSwbrkptId(proc, request->id) != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20046);
		goto resume;
	}
	if (request->id == -1) {
		count = swbkptGetSwbrkptCount(proc, pid);
		size = sizeof(*response) + count * sizeof(DbgpSwbrkptInfo);
	} else {
		size = sizeof(*response) + sizeof(DbgpSwbrkptInfo);
		count = 1;
	}
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL) {
		result = SCE_KERNEL_ERROR_NO_MEMORY;
		goto resume;
	}
	memset(response, 0, size);
	_dbgpSetSendSize(proc, size);
	response->pid = pid;
	response->count = count;
	result = swbkptGetSwbrkpt(proc, response, request->id);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 943, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 0);
		goto resume;
	}
	dbgpFillResponseHeader(proc, packet->group, 0x49, 0, 0);
	if (processResumeProcessByDBGP(pid, alreadySuspended) < 0)
		return (int)0x80080001;
	return 0;

resume:
	processResumeProcessByDBGP(pid, alreadySuspended);
	return result;
}

int systemSetSwbrkptCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpSetSwbrkptRequest *request =
		(const DbgpSetSwbrkptRequest *)packet;
	DbgpSetSwbrkptResponse *response;
	SceUID *work;
	SceUID pid, thid;
	int alreadySuspended = 0;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->header.pid;
	thid = request->info.thid;
	if (dbgpCheckSwbrkptSize(&request->info.size) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 1);
		return (int)0x80080001;
	}
	if (dbgpCheckDuplicateSwbrkpt(proc, pid, thid,
				      request->info.address) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 3);
		return (int)0x80080001;
	}
	if (checkInvalidProcess(pid) != 0)
		goto invalid;
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = processSuspendProcessByDBGP(pid, &alreadySuspended);
	if (ret == SCE_KERNEL_ERROR_WAIT_DELETE)
		goto invalid;
	if (ret < 0)
		return (int)0x80080001;
	ret = processWaitThreadSuspendForProcess(pid, work, 1);
	if (ret < 0) {
		result = (int)0x80080001;
		SCE_PRINTF_WITH_INFO(15, 1079, "0x%08x\n", ret);
		goto resume;
	}
	if (checkInvalidOptionalThread(pid, thid) != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20012);
		goto resume;
	}
	if ((request->info.attr & 1) != 0) {
		/* Thumb */
		if (checkInvalidMemoryRange(pid, request->info.address, 2,
					    0x10) != 0)
			goto range;
		result = checkInvalidAddressAlignment(request->info.address,
						      0x10);
	} else {
		if (checkInvalidMemoryRange(pid, request->info.address, 4,
					    0x10) != 0)
			goto range;
		result = checkInvalidAddressAlignment(request->info.address, 1);
	}
	if (result != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20031);
		goto resume;
	}
	if (checkNoFreeSwbrkpt(proc) != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20047);
		goto resume;
	}
	if (checkNotExecutable(pid, request->info.address,
			       (SceKernelModuleInfoForDebugger *)work) != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20048);
		goto resume;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL) {
		result = SCE_KERNEL_ERROR_NO_MEMORY;
		goto resume;
	}
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	result = swbkptSetSwbrkpt(proc, request, response);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1172, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20030);
		goto resume;
	}
	dbgpFillResponseHeader(proc, packet->group, 0x4b, 0, 0);
	if (processResumeProcessByDBGP(pid, alreadySuspended) < 0)
		return (int)0x80080001;
	return 0;

range:
	result = (int)0x80080001;
	dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
		      0x20030);
resume:
	processResumeProcessByDBGP(pid, alreadySuspended);
	return result;

invalid:
	dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
		      0x20011);
	return (int)0x80080001;
}

int systemClearSwbrkptCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpClearSwbrkptRequest *request =
		(const DbgpClearSwbrkptRequest *)packet;
	DbgpPointIdResponse *response;
	SceUID *work;
	SceUID pid;
	int alreadySuspended = 0;
	int deleted = 0;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->header.pid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = processSuspendProcessByDBGP(pid, &alreadySuspended);
	if (ret == SCE_KERNEL_ERROR_WAIT_DELETE) {
		/* the process is going: only the records are cleared */
		deleted = 1;
	} else {
		if (ret < 0)
			return (int)0x80080001;
		ret = processWaitThreadSuspendForProcess(pid, work, 1);
		if (ret < 0) {
			result = (int)0x80080001;
			SCE_PRINTF_WITH_INFO(15, 1290, "0x%08x\n", ret);
			goto resume;
		}
	}
	if (checkInvalidSwbrkptId(proc, request->id) != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20046);
		goto resume;
	}
	ret = swbkptClearSwbrkpt(proc, request, deleted);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 1308, "0x%08x\n", ret);
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      ret);
		goto resume;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL) {
		result = SCE_KERNEL_ERROR_NO_MEMORY;
		goto resume;
	}
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	dbgpFillResponseHeader(proc, packet->group, 0x4d, 0, 0);
	response->pid = pid;
	response->id = request->id;
	if (deleted == 0 && processResumeProcessByDBGP(pid, alreadySuspended) < 0)
		return (int)0x80080001;
	return 0;

resume:
	processResumeProcessByDBGP(pid, alreadySuspended);
	return result;
}

int systemGetHwwatchptCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpPointIdRequest *request = (const DbgpPointIdRequest *)packet;
	DbgpGetHwwatchptResponse *response;
	SceUID *work;
	SceUID pid;
	SceSize size;
	SceUInt32 count;
	int alreadySuspended = 0;
	int result, ret, id;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->header.pid;
	id = request->id;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (processSuspendProcessByDBGP(pid, &alreadySuspended) < 0)
		return (int)0x80080001;
	ret = processWaitThreadSuspendForProcess(pid, work, 1);
	if (ret < 0) {
		result = (int)0x80080001;
		SCE_PRINTF_WITH_INFO(15, 1437, "0x%08x\n", ret);
		goto resume;
	}
	if (checkInvalidWatchpointId(proc, id) != 0)
		goto error;
	if (id == -1) {
		count = hwwtptGetHwwtptCount(proc, pid);
		size = sizeof(*response) + count * sizeof(DbgpHwwatchptInfo);
	} else {
		size = sizeof(*response) + sizeof(DbgpHwwatchptInfo);
		count = 1;
	}
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL) {
		result = SCE_KERNEL_ERROR_NO_MEMORY;
		goto resume;
	}
	memset(response, 0, size);
	_dbgpSetSendSize(proc, size);
	response->pid = pid;
	response->count = count;
	if (hwwtptGetHwwtpt(proc, response, id) < 0)
		goto error;
	dbgpFillResponseHeader(proc, packet->group, 0x31, 0, 0);
	if (processResumeProcessByDBGP(pid, alreadySuspended) < 0)
		return (int)0x80080001;
	return 0;

error:
	result = (int)0x80080001;
	dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
		      0x20040);
resume:
	processResumeProcessByDBGP(pid, alreadySuspended);
	return result;
}

int systemSetHwwatchptCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpSetHwwatchptRequest *request =
		(const DbgpSetHwwatchptRequest *)packet;
	DbgpPointIdResponse *response;
	SceUID *work;
	SceUID pid;
	SceUInt32 id;
	int alreadySuspended = 0;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->header.pid;
	if (checkInvalidProcess(pid) != 0)
		goto invalid;
	if (checkInvalidWatchpointControl(request->info.control) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = processSuspendProcessByDBGP(pid, &alreadySuspended);
	if (ret == SCE_KERNEL_ERROR_WAIT_DELETE)
		goto invalid;
	if (ret < 0)
		return (int)0x80080001;
	ret = processWaitThreadSuspendForProcess(pid, work, 1);
	if (ret < 0) {
		result = (int)0x80080001;
		SCE_PRINTF_WITH_INFO(15, 1605, "0x%08x\n", ret);
		goto resume;
	}
	if (checkInvalidMemoryRange(pid, request->info.address, 4, 0x10) != 0)
		goto range;
	if (checkInvalidAddressAlignment(request->info.address, 1) != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20031);
		goto resume;
	}
	if (checkNoFreeWatchpoint(proc) != 0) {
		result = (int)0x80080001;
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20041);
		goto resume;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL) {
		result = SCE_KERNEL_ERROR_NO_MEMORY;
		goto resume;
	}
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	if (hwwtptSetHwwtpt(proc, &request->info, pid, &id) < 0)
		goto range;
	response->pid = pid;
	response->id = id;
	dbgpFillResponseHeader(proc, packet->group, 0x33, 0, 0);
	if (processResumeProcessByDBGP(pid, alreadySuspended) < 0)
		return (int)0x80080001;
	return 0;

range:
	result = (int)0x80080001;
	dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
		      0x20030);
resume:
	processResumeProcessByDBGP(pid, alreadySuspended);
	return result;

invalid:
	dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
		      0x20011);
	return (int)0x80080001;
}

int systemClearHwwatchptCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpPointIdRequest *request = (const DbgpPointIdRequest *)packet;
	DbgpPointIdResponse *response;
	SceUID *work;
	SceUID pid;
	int alreadySuspended = 0;
	int deleted = 0;
	int result, ret, id;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->header.pid;
	id = request->id;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = processSuspendProcessByDBGP(pid, &alreadySuspended);
	if (ret == SCE_KERNEL_ERROR_WAIT_DELETE) {
		/* the process is going: only the records are cleared */
		deleted = 1;
	} else {
		if (ret < 0)
			return (int)0x80080001;
		ret = processWaitThreadSuspendForProcess(pid, work, 1);
		if (ret < 0) {
			result = (int)0x80080001;
			SCE_PRINTF_WITH_INFO(15, 1778, "0x%08x\n", ret);
			goto resume;
		}
	}
	if (checkInvalidWatchpointId(proc, id) != 0)
		goto error;
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL) {
		result = SCE_KERNEL_ERROR_NO_MEMORY;
		goto resume;
	}
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	if (hwwtptClearHwwtpt(proc, pid, id, deleted) < 0)
		goto error;
	response->pid = pid;
	response->id = id;
	dbgpFillResponseHeader(proc, packet->group, 0x35, 0, 0);
	if (deleted == 0 && processResumeProcessByDBGP(pid, alreadySuspended) < 0)
		return (int)0x80080001;
	return 0;

error:
	result = (int)0x80080001;
	dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
		      0x20040);
resume:
	processResumeProcessByDBGP(pid, alreadySuspended);
	return result;
}

/* Writes the requested registers of a thread: r0-r15, cpsr, fpscr, q0-q15. */
static int _setRegister(DbgpRegisterWork *work,
			const DbgpRegisterRequest *request)
{
	const DbgpRegisterValue *values;
	SceUInt32 cpuMask, vfpMask, count, id, i;
	SceUInt32 *vfpQuad;
	int result;

	cpuMask = 0;
	vfpMask = 0;
	count = request->count;
	values = (const DbgpRegisterValue *)&request->registers[count];
	for (i = 0; i < count; i++) {
		id = request->registers[i];
		switch (DBGP_REG_GROUP(id)) {
		case DBGP_REG_GPR:
			cpuMask += 1 << DBGP_REG_INDEX(id);
			work->regs[0].r[DBGP_REG_INDEX(id)] = values[i].value[0];
			break;
		case DBGP_REG_STATUS:
			if (id == DBGP_REG_CPSR) {
				cpuMask += 0x10000;
				work->regs[0].cpsr = values[i].value[0];
			} else if (id == DBGP_REG_FPSCR) {
				cpuMask += 0x20000;
				work->regs[0].fpscr = values[i].value[0];
			}
			break;
		case DBGP_REG_QUAD:
			vfpMask += 1 << DBGP_REG_INDEX(id);
			vfpQuad = &work->vfp.s[DBGP_REG_INDEX(id) * 4];
			vfpQuad[0] = values[i].value[0];
			vfpQuad[1] = values[i].value[1];
			vfpQuad[2] = values[i].value[2];
			vfpQuad[3] = values[i].value[3];
			break;
		}
	}
	if (vfpMask != 0) {
		result = sceKernelSetVfpRegisterForDebugger(work->info.threadId,
							    work->vfp.s, vfpMask);
		if (result < 0) {
			if (result != SCE_KERNEL_ERROR_ILLEGAL_PERMISSION)
				SCE_PRINTF_WITH_INFO(15, 2014, "0x%08x\n", result);
			return result;
		}
	}
	if (cpuMask != 0) {
		result = sceKernelSetCpuRegisterForDebugger(work->info.threadId,
							    &work->regs[0], cpuMask);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 2023, "0x%08x\n", result);
			return result;
		}
	}
	return 0;
}
