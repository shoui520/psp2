/*
 * SceSblSmschedProxy: ARM side of the secure-module scheduler.
 *
 * A session (64 slots, allocated from the module's heap) wraps one secure
 * module invocation on the TrustZone side. Requests go out through the SMC
 * interface with a message in a TrustZone bridge buffer; completions and
 * Cry2Arm notifications come back as software interrupt 4 ("SGI"), whose
 * message names the session, a channel and what to do: set the session's
 * event flag, call the channel's handler, or acknowledge.
 *
 * Every API entry stores TPIDRURO << 16 while it runs and restores it on exit.
 *
 * NSKBL (SMSC_PROXY_NSKBL) runs without a thread manager: a session's event is
 * a word the interrupt handler sets and sceSblSmSchedProxyWait polls, its wait
 * lock a byte taken by compare-and-set; there is no system event handler, stop,
 * kill or F00D register access.
 */

#include <fw.h>
#include <stddef.h>
#include <sysmem/cpu.h>
#include <sysmem/barrier.h>
#include <sysmem/debug.h>
#include <smsc_proxy/smsc_proxy.h>
#include <sysmem/sysroot.h>
#include <smsc_proxy/tzs_bridge.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysevent_ids.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysevent.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysmem.h>

#define SESSION_COUNT	64
#define CHANNEL_COUNT	4

#define STATE_READY	1
#define STATE_STOPPED	2

/* SMC commands */
#define SMC_INVOKE		0x12d
#define SMC_WAIT		0x12e
#define SMC_GET_STATUS		0x12f
#define SMC_CHANGE_F00D_STATUS	0x130
#define SMC_CALL_FUNC		0x133
#define SMC_GET_F00D_REGISTER	0x134
#define SMC_3CE17233		0x135
#define SMC_WRITE_CRY2ARM	0x136
#define SMC_READ_CRY2ARM	0x137
#define SMC_ENABLE_CRY2ARM	0x138
#define SMC_DISABLE_CRY2ARM	0x139
#define SMC_INTR_DONE		0x13a
#define SMC_STOP		0x13b
#define SMC_EXECUTE_F00D	0x13c

/* SessionLookup status */
enum {
	LOOKUP_OK,
	LOOKUP_ID_INVALID,		/* -1 */
	LOOKUP_ID_ZERO,
	LOOKUP_NOT_FOUND,
	LOOKUP_4,			/* never produced */
	LOOKUP_ID_CLEARED,
	LOOKUP_NO_EVENT_FLAG,
	LOOKUP_NO_MUTEX,
};

/*
 * A word of the invoke argument area. The area is filled as words, after every load from the
 * caller's context: both are plain memory to the compiler, so the order of the loads and stores
 * is the order written here.
 */
#define INVOKE_AREA_WORD(field, n) \
	(((SceUInt32 *)buf)[offsetof(SceSblSmSchedProxyInvokeArg, field) / sizeof(SceUInt32) + (n)])

typedef struct Channel {
	SceSblSmSchedCry2ArmHandler handler;
	void *arg;
	SmschedIntrStatus pending;	/* held until a handler exists */
} Channel;

typedef struct Session {
	int used;
	SceUID id;
	SceUInt32 handle;		/* TrustZone side handle */
#if SMSC_PROXY_NSKBL
	SceUInt32 event;		/* the interrupt handler's event bits */
	SceInt8 waitLock;		/* taken by sceSblSmSchedProxyWait */
#else
	SceUID eventFlag;
	SceUID mutex;
#endif
	Channel channel[CHANNEL_COUNT];
} Session;

int SmcCall(SceUInt32 r0, SceUInt32 r1, SceUInt32 r2, SceUInt32 r3, SceUInt32 command);

/*
 * The sessions are read in place: Sony's code loads a field again wherever it uses it again, so the
 * table is volatile. The id counter starts at 0 and is incremented before use in 4.00; the 3.6x
 * builds start it at 1 and increment it after.
 */
SceUID s_heap;
#if FW_SINCE(0x04000000)
volatile SceUID s_lastId;
#else
SceUID s_lastId = 1;
#endif
int s_state;
volatile Session *s_session;
SceKernelSpinlock s_lock;

/* name unknown */
static volatile Session *SessionLookup(SceUID id, int *status)
{
	volatile Session *s;
	int i;

	if (status == NULL)
		return NULL;
	if (id == -1) {
		*status = LOOKUP_ID_INVALID;
		return NULL;
	}
	if (id == 0) {
		*status = LOOKUP_ID_ZERO;
		return NULL;
	}

	for (i = 0; i < SESSION_COUNT; i++) {
		s = &s_session[i];
		if (s->used == 1 && s->id == id)
			break;
	}
	if (i == SESSION_COUNT) {
		*status = LOOKUP_NOT_FOUND;
		return NULL;
	}

	if (s->id == -1) {
		*status = LOOKUP_ID_CLEARED;
		return NULL;
	}
#if !SMSC_PROXY_NSKBL
	if (s->eventFlag == -1) {
		*status = LOOKUP_NO_EVENT_FLAG;
		return NULL;
	}
	if (s->mutex == -1) {
		*status = LOOKUP_NO_MUTEX;
		return NULL;
	}
#endif
	*status = LOOKUP_OK;
	return s;
}

static int softIntrHandler(int intrCode, void *userCtx)
{
	SceSblSmSchedCry2ArmHandler handler;
	SceSize size;
	volatile SmschedIntrMessage *msg;
	SmschedIntrStatus status;
	volatile Session *s;
	SceUID id;
	SceUInt32 handle, flags, channel, eventBits, a, b;
	void *arg;
	int state, lookup, ret;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	if (sceSblTzsBridgePopIntrArea((void **)&msg, &size) < 0)
		SCE_PANIC(964);
	if (size < sizeof(*msg))
		SCE_PANIC(968);

	channel = msg->channel;
	id = msg->id;
	flags = msg->flags;
	eventBits = msg->eventBits;
	status = msg->status;
	b = status.result;
	a = status.state;

	s = SessionLookup(id, &lookup);
	if (s == NULL) {
		if (lookup == LOOKUP_ID_INVALID)
			SCE_PANIC(993);
		else if (lookup == LOOKUP_ID_ZERO)
			SCE_PANIC(996);
		else if (lookup == LOOKUP_NOT_FOUND)
			SCE_PANIC(999);
		else if (lookup == LOOKUP_4)
			SCE_PANIC(1002);
		else if (lookup == LOOKUP_ID_CLEARED)
			SCE_PANIC(1005);
#if !SMSC_PROXY_NSKBL
		else if (lookup == LOOKUP_NO_EVENT_FLAG)
			SCE_PANIC(1009);
		else if (lookup == LOOKUP_NO_MUTEX)
			SCE_PANIC(1012);
#endif
		else
			SCE_PANIC(1015);
	}

	handle = s->handle;
	handler = NULL;
	arg = NULL;
	if (flags & MSG_CALL_HANDLER) {
		handler = s->channel[channel].handler;
		arg = s->channel[channel].arg;
		if (handler == NULL)
			s->channel[channel].pending = status;
	}
	if (flags & MSG_HANDLER_ONLY) {
		handler = s->channel[channel].handler;
		arg = s->channel[channel].arg;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);

#if SMSC_PROXY_NSKBL
	if (flags & MSG_SET_EVENT) {
		state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
		s->event |= eventBits;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
		dsb();
		sev();
	}
#else
	if ((flags & MSG_SET_EVENT) && sceKernelSetEventFlag(s->eventFlag, eventBits) < 0)
		SCE_PANIC(1045);
#endif

	if (handler == NULL) {
		ret = SmcCall(handle, channel, msg->ack, 0, SMC_INTR_DONE);
		if (ret == SCE_SBL_ERROR_SMSCHED_NO_REQUEST) {
			if (flags & MSG_HANDLER_ONLY)
				SCE_PANIC(1081);
			return -1;
		}
	} else {
		if (flags & MSG_CALL_HANDLER)
			handler(id, channel, arg, a, b);
		if (!(flags & MSG_HANDLER_ONLY)) {
			ret = SmcCall(handle, channel, msg->ack, 0, SMC_INTR_DONE);
			if (ret == SCE_SBL_ERROR_SMSCHED_NO_REQUEST)
				return -1;
		} else {
			handler(id, channel, arg, 0, 0);
			ret = SmcCall(handle, channel, msg->ack, 0, SMC_INTR_DONE);
			if (ret == SCE_SBL_ERROR_SMSCHED_NO_REQUEST)
				SCE_PANIC(1081);
		}
	}
	if (ret < 0)
		SCE_PANIC(1081);
	return -1;
}

/* name unknown: SMC command on a session channel (channel 1-3) */
static int updateMailbox(SceUInt32 command, SceUID id, int channel, SceUInt32 value)
{
	volatile Session *s;
	int status;

	if (s_state != STATE_READY)
		return SCE_SBL_ERROR_SMSCHED_NOT_READY;
	if ((unsigned int)(channel - 1) >= 3)
		return SCE_SBL_ERROR_SMSCHED_INVALID_ARGUMENT;

	s = SessionLookup(id, &status);
	if (s == NULL)
		return SCE_SBL_ERROR_SMSCHED_NO_REQUEST;
	return SmcCall(s->handle, channel, value, 0, command);
}

__attribute__((noinline, noclone))
static int accessMailbox(SceUInt32 command, SceUID id, int channel, SceUInt32 *value)
{
	SceSize size;
	SceUInt32 *buf;
	volatile Session *s;
	int status, index, ret, result;

	if (s_state != STATE_READY)
		return SCE_SBL_ERROR_SMSCHED_NOT_READY;
	if ((unsigned int)(channel - 1) >= 3)
		return SCE_SBL_ERROR_SMSCHED_INVALID_ARGUMENT;

	s = SessionLookup(id, &status);
	if (s == NULL)
		return SCE_SBL_ERROR_SMSCHED_NO_REQUEST;

	index = sceSblTzsBridgeAllocArgArea(4, (void **)&buf, &size);
	if (index < 0)
		return index;
	ret = sceSblTzsBridgePushArgArea(index);
	if (ret < 0) {
		sceSblTzsBridgeFreeArgArea(index);
		return ret;
	}
	result = SmcCall(s->handle, channel, index, 0, command);
	ret = sceSblTzsBridgePopArgArea(index, (void **)&buf, &size);
	if (ret < 0) {
		sceSblTzsBridgeFreeArgArea(index);
		return ret;
	}
	if (size < 4)
		SCE_PANIC(1334);
	*value = buf[0];

	ret = sceSblTzsBridgeFreeArgArea(index);
	return ret < 0 ? ret : result;
}

/* name unknown */
static void SessionFree(SceUID id)
{
	volatile Session *s;
	int status;

	s = SessionLookup(id, &status);
	if (s == NULL)
		return;
#if !SMSC_PROXY_NSKBL
	if (s->eventFlag != -1) {
		sceKernelDeleteEventFlag(s->eventFlag);
		s->eventFlag = -1;
	}
	if (s->mutex != -1) {
		sceKernelDeleteMutex(s->mutex);
		s->mutex = -1;
	}
#endif
	s->id = -1;
	s->used = 0;
}

#if !SMSC_PROXY_NSKBL
/* name unknown */
static int sysEventHandler(int resume, int eventId, void *args, void *opt)
{
	int ret;

	if (resume == 1) {
		if (eventId == SYSEVENT_RESUME_DEVICE(4)) {
			ret = sceSblTzsBridgeStart();
			return ret < 0 ? ret : 0;
		}
	}
	return 0;
}
#endif

int sceSblSmSchedProxyModuleStart(SceSize args, void *argp)
{
	SceKernelHeapCreateOpt opt;
	int ret, i;

	opt.size = sizeof(opt);
	opt.attr = 1;
	s_heap = ret = sceKernelCreateHeap("SceSblSmschedProxy", 0x1000, &opt);
	if (ret < 0)
		return ret;

	s_session = sceKernelAllocHeapMemory(s_heap, SESSION_COUNT * sizeof(Session));
	if (s_session == NULL)
		return SCE_SBL_ERROR_SMSCHED_NO_MEMORY;
	for (i = 0; i < SESSION_COUNT; i++)
		s_session[i].used = 0;

	ret = sceSblTzsBridgeStart();
	if (ret < 0)
		return ret;
	ret = sceKernelRegisterIntrHandler(4, "SceSblSmSchedProxySGI", 0x10, softIntrHandler,
					    NULL, 0x80, 0xf, NULL);
	if (ret < 0)
		return ret;
#if !SMSC_PROXY_NSKBL
	ret = sceKernelRegisterSysEventHandler("SceSblSmSchedProxy", sysEventHandler, NULL);
	if (ret < 0)
		return ret;
#endif

	s_lock = 0;
	s_state = STATE_READY;
	return ret;
}

int SceSblSmSchedProxyForKernel_A488D604(void)
{
	if (sceKernelCpuId() != 0)
		return 0;
	return sceSblSmSchedProxyModuleStart(0, NULL);
}

#if !SMSC_PROXY_NSKBL
/* name unknown */
__attribute__((noinline)) static int Terminate(void)
{
	int ret;

	ret = SmcCall(0, 0, 0, 0, SMC_STOP);
	if (s_heap > 0) {
		if (s_session != NULL) {
			sceKernelFreeHeapMemory(s_heap, (void *)s_session);
			s_session = NULL;
		}
		sceKernelDeleteHeap(s_heap);
		s_heap = -1;
	}
	s_state = STATE_STOPPED;
	return ret;
}

int SceSblSmSchedProxyForKernel_33A3A1E2(void)
{
	if (sceKernelCpuId() != 0)
		return 0;
	return Terminate();
}
#endif

int sceSblSmSchedProxyInvoke(SceUInt32 a, SceUInt32 b, SceUInt32 c, const SceUInt32 *in,
			     const SelfAuthRequest *param, SceUID *id)
{
	SceUInt32 tls;
	SceSblSmSchedProxyInvokeArg *buf;
	SceSize size;
	volatile Session *s;
	SceUID newId;
	const SceUInt32 *programAuthorityId, *capability;
	SceUInt32 selfType, mediaType, paid0, paid1, cap0, cap1, cap2, cap3;
	int state, i, index, ret, result;

	tls = sceSyscallEnter();
	if (s_state != STATE_READY) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_NOT_READY;
	}
	if (id == NULL) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_INVALID_ARGUMENT;
	}

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	for (i = 0; i < SESSION_COUNT; i++)
		if (s_session[i].used == 0)
			break;
	if (i == SESSION_COUNT) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_NO_MEMORY;
	}
	s = &s_session[i];
	s->used = 1;
#if FW_SINCE(0x04000000)
	newId = ++s_lastId;
#else
	newId = s_lastId++;
#endif
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);

	s->id = newId;
	s->handle = -1;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		s->channel[i].handler = NULL;
		s->channel[i].arg = NULL;
		s->channel[i].pending.result = 0;
		s->channel[i].pending.state = 0;
	}
#if SMSC_PROXY_NSKBL
	s->waitLock = 0;
	s->event = 0;
#else
	s->eventFlag = -1;
	s->mutex = -1;

	ret = sceKernelCreateEventFlag("SceSblSmsProxy", 0, 0, NULL);
	if (ret < 0)
		goto fail_session;
	s->eventFlag = ret;
	ret = sceKernelCreateMutex("SceSblSmsProxyWait", 0, 0, NULL);
	if (ret < 0)
		goto fail_session;
	s->mutex = ret;
#endif
	if (newId == -1) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_NO_MEMORY;
	}
	*id = newId;

	index = sceSblTzsBridgeAllocArgArea(0x48, (void **)&buf, &size);
	if (index < 0) {
		SessionFree(newId);
		sceSyscallLeave(tls);
		return index;
	}
	if (in != NULL) {
		buf->arg2Low = in[0];
		buf->arg2High = in[1];
		buf->arg3 = in[2];
		buf->arg4 = in[3];
	} else {
		buf->arg2Low = 0;
		buf->arg2High = 0;
		buf->arg3 = 0;
		buf->arg4 = 0;
	}
	programAuthorityId = (const SceUInt32 *)&param->caller.programAuthorityId;
	capability = (const SceUInt32 *)&param->caller.capability;
	selfType = param->selfType;
	mediaType = param->mediaType;
	paid0 = programAuthorityId[0];
	paid1 = programAuthorityId[1];
	cap0 = capability[0];
	cap1 = capability[1];
	cap2 = capability[2];
	cap3 = capability[3];
	INVOKE_AREA_WORD(proxyId, 0) = newId;
	INVOKE_AREA_WORD(invokeArg.selfType, 0) = selfType;
	INVOKE_AREA_WORD(invokeArg.mediaType, 0) = mediaType;
	INVOKE_AREA_WORD(invokeArg.programAuthorityId, 0) = paid0;
	INVOKE_AREA_WORD(invokeArg.programAuthorityId, 1) = paid1;
	INVOKE_AREA_WORD(invokeArg.capability, 0) = cap0;
	INVOKE_AREA_WORD(invokeArg.capability, 1) = cap1;
	INVOKE_AREA_WORD(invokeArg.capability, 2) = cap2;
	INVOKE_AREA_WORD(invokeArg.capability, 3) = cap3;
	cap0 = capability[4];
	cap1 = capability[5];
	cap2 = capability[6];
	cap3 = capability[7];
	INVOKE_AREA_WORD(invokeArg.capability, 4) = cap0;
	INVOKE_AREA_WORD(invokeArg.capability, 5) = cap1;
	INVOKE_AREA_WORD(invokeArg.capability, 6) = cap2;
	INVOKE_AREA_WORD(invokeArg.capability, 7) = cap3;

	ret = sceSblTzsBridgePushArgArea(index);
	if (ret < 0)
		goto fail_buffer;
	result = SmcCall(a, b, c, index, SMC_INVOKE);
	ret = sceSblTzsBridgePopArgArea(index, (void **)&buf, &size);
	if (ret < 0)
		goto fail_buffer;
	if (size < 0x48)
		SCE_PANIC(302);
	s->handle = buf->requestId;

	ret = sceSblTzsBridgeFreeArgArea(index);
	if (ret >= 0) {
		sceSyscallLeave(tls);
		return result;
	}
	SessionFree(newId);
	sceSyscallLeave(tls);
	return ret;

fail_buffer:
	sceSblTzsBridgeFreeArgArea(index);
	SessionFree(newId);
	sceSyscallLeave(tls);
	return ret;

#if !SMSC_PROXY_NSKBL
fail_session:
	SessionFree(newId);
	sceSyscallLeave(tls);
	return SCE_SBL_ERROR_SMSCHED_NO_MEMORY;
#endif
}

int sceSblSmSchedProxyWait(SceUID id, SceUInt32 *result)
{
	SceUInt32 tls;
	SceSize size;
	SmschedIntrStatus *buf;
	volatile Session *s;
	int status, index, ret, value;
#if SMSC_PROXY_NSKBL
	int state;
#endif

	tls = sceSyscallEnter();
	if (s_state != STATE_READY) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_NOT_READY;
	}
	if (result == NULL)
		goto invalid;

	s = SessionLookup(id, &status);
	if (s == NULL) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_NO_REQUEST;
	}
#if SMSC_PROXY_NSKBL
	if (sceKernelAtomicCompareAndSet8((SceInt8 *)&s->waitLock, 0, 1) != 0)
		goto invalid;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	if (!(s->event & 1)) {
		do {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
			do
				wfe();
			while (!(s->event & 1));
			state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
		} while (!(s->event & 1));
		s->event = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
#else
	if (sceKernelTryLockMutex(s->mutex, 1) < 0)
		goto invalid;

	sceKernelSysrootDisableAutoClockDown();
	ret = sceKernelWaitEventFlag(s->eventFlag, 1, 3, NULL, NULL);
	sceKernelSysrootEnableAutoClockDown();
	if (ret < 0)
		goto out;
#endif

	ret = index = sceSblTzsBridgeAllocArgArea(8, (void **)&buf, &size);
	if (index < 0)
		goto out;
	ret = sceSblTzsBridgePushArgArea(index);
	if (ret >= 0) {
		value = SmcCall(s->handle, index, 0, 0, SMC_WAIT);
		ret = sceSblTzsBridgePopArgArea(index, (void **)&buf, &size);
		if (ret >= 0) {
			if (size < 8)
				SCE_PANIC(421);
			*(SmschedIntrStatus *)result = *buf;
			ret = sceSblTzsBridgeFreeArgArea(index);
			ret = ret < 0 ? ret : value;
			goto out;
		}
	}
	sceSblTzsBridgeFreeArgArea(index);
out:
	SessionFree(id);
	sceSyscallLeave(tls);
	return ret;

invalid:
	sceSyscallLeave(tls);
	return SCE_SBL_ERROR_SMSCHED_INVALID_ARGUMENT;
}

int sceSblSmSchedProxyGetStatus(SceUID id, SceUInt32 *status)
{
	SceUInt32 tls;
	SceSize size;
	SmschedIntrStatus *buf;
	volatile Session *s;
	int st, index, ret, value;

	tls = sceSyscallEnter();
	if (s_state != STATE_READY) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_NOT_READY;
	}
	if (status == NULL) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_INVALID_ARGUMENT;
	}
	s = SessionLookup(id, &st);
	if (s == NULL) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_NO_REQUEST;
	}

	index = sceSblTzsBridgeAllocArgArea(8, (void **)&buf, &size);
	if (index < 0) {
		sceSyscallLeave(tls);
		return index;
	}
	ret = sceSblTzsBridgePushArgArea(index);
	if (ret >= 0) {
		value = SmcCall(s->handle, index, 0, 0, SMC_GET_STATUS);
		ret = sceSblTzsBridgePopArgArea(index, (void **)&buf, &size);
		if (ret >= 0) {
			if (size < 8)
				SCE_PANIC(523);
			*(SmschedIntrStatus *)status = *buf;
			ret = sceSblTzsBridgeFreeArgArea(index);
			if (ret < 0) {
				sceSyscallLeave(tls);
				return ret;
			}
			sceSyscallLeave(tls);
			return value;
		}
	}
	sceSblTzsBridgeFreeArgArea(index);
	sceSyscallLeave(tls);
	return ret;
}

#if !SMSC_PROXY_NSKBL
int sceSblSmSchedProxyKill(SceUID id)
{
	SceUInt32 tls = sceSyscallEnter();
	volatile Session *s;
	int status, ret;

	if (s_state != STATE_READY) {
		ret = SCE_SBL_ERROR_SMSCHED_NOT_READY;
	} else {
		s = SessionLookup(id, &status);
		if (s == NULL)
			ret = SCE_SBL_ERROR_SMSCHED_NO_REQUEST;
		else
			ret = SmcCall(s->handle, 0, 0, 0, SMC_CHANGE_F00D_STATUS);
	}
	sceSyscallLeave(tls);
	return ret;
}

int SceSblSmSchedProxyForKernel_984EC9D1(void)
{
	sceSyscallLeave(sceSyscallEnter());
	return SCE_SBL_ERROR_SMSCHED_NOT_SUPPORTED;
}

int SceSblSmSchedProxyForKernel_1DFC8624(void)
{
	sceSyscallLeave(sceSyscallEnter());
	return SCE_SBL_ERROR_SMSCHED_NOT_SUPPORTED;
}

#endif

int SceSblSmSchedProxyForKernel_723B382F(SceUID id, int channel, SceUInt32 value)
{
	SceUInt32 tls = sceSyscallEnter();
	int ret = updateMailbox(SMC_CALL_FUNC, id, channel, value);

	sceSyscallLeave(tls);
	return ret;
}

#if !SMSC_PROXY_NSKBL
int SceSblSmSchedProxyForKernel_F70C04EC(SceUID id, int channel, SceUInt32 *value)
{
	SceUInt32 tls = sceSyscallEnter();
	int ret = accessMailbox(SMC_GET_F00D_REGISTER, id, channel, value);

	sceSyscallLeave(tls);
	return ret;
}

int SceSblSmSchedProxyForKernel_3CE17233(SceUID id, int channel, SceUInt32 value)
{
	SceUInt32 tls = sceSyscallEnter();
	int ret = updateMailbox(SMC_3CE17233, id, channel, value);

	sceSyscallLeave(tls);
	return ret;
}

#endif

int sceSblSmSchedProxyWriteCry2Arm(SceUID id, int channel, SceUInt32 value)
{
	SceUInt32 tls = sceSyscallEnter();
	int ret = updateMailbox(SMC_WRITE_CRY2ARM, id, channel, value);

	sceSyscallLeave(tls);
	return ret;
}

int sceSblSmSchedProxyReadCry2Arm(SceUID id, int channel, SceUInt32 *value)
{
	SceUInt32 tls = sceSyscallEnter();
	int ret = accessMailbox(SMC_READ_CRY2ARM, id, channel, value);

	sceSyscallLeave(tls);
	return ret;
}

int sceSblSmSchedProxyRegisterIntrHandler(SceUID id, int channel,
					      SceSblSmSchedCry2ArmHandler handler, void *arg)
{
	SceUInt32 tls = sceSyscallEnter();
	SmschedIntrStatus pending;
	volatile Session *s;
	int state, status, ret;

	if (s_state != STATE_READY) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_NOT_READY;
	}
	if ((unsigned int)channel >= CHANNEL_COUNT) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_INVALID_ARGUMENT;
	}
	if (handler == NULL) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_INVALID_ARGUMENT;
	}

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	s = SessionLookup(id, &status);
	if (s == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_NO_REQUEST;
	}
	if (s->channel[channel].handler != NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_SMSCHED_BUSY;
	}
	s->channel[channel].handler = handler;
	s->channel[channel].arg = arg;
	pending = s->channel[channel].pending;
	s->channel[channel].pending.result = 0;
	s->channel[channel].pending.state = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);

	if (pending.state != 0)
		handler(id, channel, arg, pending.state, pending.result);
	ret = SmcCall(s->handle, channel, 0, 0, SMC_ENABLE_CRY2ARM);
	sceSyscallLeave(tls);
	return ret;
}

int sceSblSmSchedProxyReleaseIntrHandler(SceUID id, int channel)
{
	SceUInt32 tls = sceSyscallEnter();
	volatile Session *s;
	int state, status, ret;

	if (s_state != STATE_READY) {
		ret = SCE_SBL_ERROR_SMSCHED_NOT_READY;
		goto out;
	}
	if ((unsigned int)channel >= CHANNEL_COUNT) {
		ret = SCE_SBL_ERROR_SMSCHED_INVALID_ARGUMENT;
		goto out;
	}

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	s = SessionLookup(id, &status);
	if (s == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
		ret = SCE_SBL_ERROR_SMSCHED_NO_REQUEST;
		goto out;
	}
	s->channel[channel].handler = NULL;
	s->channel[channel].arg = NULL;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
	ret = SmcCall(s->handle, channel, 0, 0, SMC_DISABLE_CRY2ARM);
out:
	sceSyscallLeave(tls);
	return ret;
}

int SceSblSmSchedProxyForKernel_7894B6F0(SceUInt32 a, SceUInt32 b, SceUInt32 c, SceUInt32 d)
{
	SceUInt32 tls = sceSyscallEnter();
	int ret;

	if (s_state == STATE_READY)
		ret = SmcCall(a, b, c, d, SMC_EXECUTE_F00D);
	else
		ret = SCE_SBL_ERROR_SMSCHED_NOT_READY;
	sceSyscallLeave(tls);
	return ret;
}
