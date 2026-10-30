/*
 * dtracep_syscalls.c: SceDTrace, the syscalls a process's DTrace library
 * makes. Each runs DTrace's code at kernel access level 0x80 and returns to
 * the caller's level after.
 */
#include <sys/dtrace.h>
#include <dtrace/logging.h>
#include <dtrace/utility.h>
#include <dtrace/sce/solaris_cred.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <sceerror.h>
#include "dtrace_client.h"
#include "dtrace_client_list.h"
#include "dtracep_command.h"
#include "dtracep_syscalls_cddl.h"

#define DTRACE_ACCESS_LEVEL		0x80
#define DTRACE_CLIENT_PATH_SIZE		256
#define DTRACE_CLIENT_IOCTL_STACK_SIZE	0x2000

/* sceDTraceClientIoctl's arguments, for the call on a stack of its own */
typedef struct DtraceClientIoctlArgs {
	int token;
	int cmd;
	uintptr_t arg;
	int *rv;
} DtraceClientIoctlArgs;

static int sceDTraceClientIoctlForUserCore(void *argp)
{
	DtraceClientIoctlArgs *args = argp;
	int token = args->token;
	int cmd = args->cmd;
	uintptr_t arg = args->arg;
	SceUInt32 state = sceSyscallEnter();
	DtraceClientCB *client;
	cred_t *cr;
	int level, res, rv = 0;

	cr = CRED();
	level = sceKernelSetThreadAccessLevel(DTRACE_ACCESS_LEVEL);
	if (level < 0) {
		DTRACE_LOG_ERROR_AT(107, "failed to set thread access level (0x%08x)", level);
		sceSyscallLeave(state);
		return level;
	}

	client = dtraceFindClientByToken(dtraceClients, token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(116, "(%d) %s - invalid token", token, __func__);
		res = SCE_KERNEL_ERROR_NO_SUCH_NAME;
	} else if (cmd == DTRACEIOC_GO && !dtracepCheckStateVariables(client)) {
		/* Sony's record of the refusal: the condition it passes always holds */
		res = SCE_ERROR_ERRNO_EINVAL;
		SCE_DBG_ASSERT_CHECK(1, 127);
	} else {
		res = dtrace_ioctl(client->dtraceDev, cmd, arg, cr, &rv);
		sceKernelSetThreadAccessLevel(level);
		sceSyscallLeave(state);
		return res < 0 ? res : rv;
	}
	sceKernelSetThreadAccessLevel(level);
	sceSyscallLeave(state);
	return res;
}

int sceDTraceClientOpenForUser(const char *path, int flags)
{
	char kpath[DTRACE_CLIENT_PATH_SIZE];
	SceUInt32 state = sceSyscallEnter();
	SceUID processId = sceKernelGetProcessId();
	int res, level;

	res = sceKernelStrncpyFromUser(kpath, path, sizeof(kpath));
	if (res == sizeof(kpath)) {
		res = SCE_ERROR_ERRNO_ENAMETOOLONG;
	} else if (res >= 0) {
		level = sceKernelSetThreadAccessLevel(DTRACE_ACCESS_LEVEL);
		if (level < 0) {
			res = level;
			DTRACE_LOG_ERROR_AT(41, "failed to set thread access level (0x%08x)", level);
		} else {
			res = sceDTraceClientOpen(kpath, processId, SCE_UID_INVALID_UID, flags);
			sceKernelSetThreadAccessLevel(level);
		}
		sceSyscallLeave(state);
		return res;
	}
	DTRACE_LOG_ERROR_AT(33, "failed to copy in path (0x%0x)", res);
	sceSyscallLeave(state);
	return res;
}

int sceDTraceClientCloseForUser(int token)
{
	SceUInt32 state = sceSyscallEnter();
	int level, res;

	level = sceKernelSetThreadAccessLevel(DTRACE_ACCESS_LEVEL);
	if (level < 0) {
		DTRACE_LOG_ERROR_AT(68, "failed to set thread access level (0x%08x)", level);
		sceSyscallLeave(state);
		return level;
	}
	res = sceDTraceClientClose(token);
	sceKernelSetThreadAccessLevel(level);
	sceSyscallLeave(state);
	return res;
}

int sceDTraceClientIoctl(int token, int cmd, uintptr_t arg, int *rv)
{
	DtraceClientIoctlArgs args = { token, cmd, arg, rv };

	return SceThreadmgrForDriver_E54FD746(DTRACE_CLIENT_IOCTL_STACK_SIZE,
					      sceDTraceClientIoctlForUserCore, &args);
}

int sceDTraceHelperIoctl(int cmd, void *arg)
{
	SceUInt32 state = sceSyscallEnter();
	int level, res, rv = 0;

	level = sceKernelSetThreadAccessLevel(DTRACE_ACCESS_LEVEL);
	if (level < 0) {
		DTRACE_LOG_ERROR_AT(176, "failed to set thread access level (0x%08x)", level);
		sceSyscallLeave(state);
		return level;
	}
	res = dtrace_ioctl_helper(dtrace_get_current_process(), cmd, (uintptr_t)arg, &rv);
	sceKernelSetThreadAccessLevel(level);
	sceSyscallLeave(state);
	return res < 0 ? res : rv;
}

int sceDTraceAddHelperDof(SceUIntPtr dof)
{
	SceUInt32 state = sceSyscallEnter();
	int level, res;

	level = sceKernelSetThreadAccessLevel(DTRACE_ACCESS_LEVEL);
	if (level < 0) {
		DTRACE_LOG_ERROR_AT(200, "failed to set thread access level (0x%08x)", level);
		sceSyscallLeave(state);
		return -1;
	}
	res = dtrace_add_helper_dof(dof);
	sceKernelSetThreadAccessLevel(level);
	sceSyscallLeave(state);
	return res;
}

int sceDTraceRemoveHelperDof(int token)
{
	SceUInt32 state = sceSyscallEnter();
	int level, res;

	level = sceKernelSetThreadAccessLevel(DTRACE_ACCESS_LEVEL);
	if (level < 0) {
		DTRACE_LOG_ERROR_AT(220, "failed to set thread access level (0x%08x)", level);
		sceSyscallLeave(state);
		return level;
	}
	res = dtrace_remove_helper_dof(token);
	sceKernelSetThreadAccessLevel(level);
	sceSyscallLeave(state);
	return res;
}
