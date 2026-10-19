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
 */

#include <sysmem/cpu.h>
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

#define SCE_SBL_ERROR_NO_MEMORY		0x800F040C
#define SCE_SBL_ERROR_INVALID_ARGUMENT	0x800F0416
#define SCE_SBL_ERROR_NOT_SUPPORTED	0x800F0425
#define SCE_SBL_ERROR_NOT_READY		0x800F0426
#define SCE_SBL_ERROR_NO_SESSION	0x800F042B
#define SCE_SBL_ERROR_BUSY		0x800F042E

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

typedef struct Channel {
	SceSblSmSchedCry2ArmHandler handler;
	void *arg;
	SceUInt32 pendingB;		/* message words held until a handler exists */
	SceUInt32 pendingA;
} Channel;

typedef struct Session {
	int used;
	SceUID id;
	SceUInt32 handle;		/* TrustZone side handle */
	SceUID eventFlag;
	SceUID mutex;
	Channel channel[CHANNEL_COUNT];
} Session;

typedef struct HeapOpt {
	SceSize size;
	SceUInt32 attr;
	SceUInt32 reserved[5];
} HeapOpt;

int SmcCall(SceUInt32 r0, SceUInt32 r1, SceUInt32 r2, SceUInt32 r3, SceUInt32 command);

static SceUID s_heap;
static SceUID s_lastId;
static int s_state;
static Session *s_session;
static SceKernelSpinlock s_lock;

/* name unknown */
static Session *SessionLookup(SceUID id, int *status)
{
	Session *s;
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
	if (s->eventFlag == -1) {
		*status = LOOKUP_NO_EVENT_FLAG;
		return NULL;
	}
	if (s->mutex == -1) {
		*status = LOOKUP_NO_MUTEX;
		return NULL;
	}
	*status = LOOKUP_OK;
	return s;
}

static int softIntrHandler(int intrCode, void *userCtx)
{
	SceSblSmSchedCry2ArmHandler handler;
	SmschedIntrMessage *msg;
	Session *s;
	SceSize size;
	SceUInt32 handle, flags, channel, a, b;
	void *arg;
	int state, status, ret;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	if (sceSblTzsBridgePopIntrArea((void **)&msg, &size) < 0)
		SCE_PANIC(964);
	if (size < sizeof(*msg))
		SCE_PANIC(968);

	channel = msg->channel;
	flags = msg->flags;
	b = msg->status.result;
	a = msg->status.state;

	s = SessionLookup(msg->id, &status);
	if (s == NULL) {
		switch (status) {
		case LOOKUP_ID_INVALID:		SCE_PANIC(993);
		case LOOKUP_ID_ZERO:		SCE_PANIC(996);
		case LOOKUP_NOT_FOUND:		SCE_PANIC(999);
		case LOOKUP_4:			SCE_PANIC(1002);
		case LOOKUP_ID_CLEARED:		SCE_PANIC(1005);
		case LOOKUP_NO_EVENT_FLAG:	SCE_PANIC(1009);
		case LOOKUP_NO_MUTEX:		SCE_PANIC(1012);
		default:			SCE_PANIC(1015);
		}
	}

	handle = s->handle;
	handler = NULL;
	arg = NULL;
	if (flags & MSG_CALL_HANDLER) {
		handler = s->channel[channel].handler;
		arg = s->channel[channel].arg;
		if (handler == NULL) {
			s->channel[channel].pendingB = b;
			s->channel[channel].pendingA = a;
		}
	}
	if (flags & MSG_HANDLER_ONLY) {
		handler = s->channel[channel].handler;
		arg = s->channel[channel].arg;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);

	if ((flags & MSG_SET_EVENT) && sceKernelSetEventFlag(s->eventFlag, msg->eventBits) < 0)
		SCE_PANIC(1045);

	if (handler == NULL) {
		ret = SmcCall(handle, channel, msg->ack, 0, SMC_INTR_DONE);
		if (ret == SCE_SBL_ERROR_NO_SESSION) {
			if (flags & MSG_HANDLER_ONLY)
				SCE_PANIC(1081);
			return -1;
		}
	} else {
		if (flags & MSG_CALL_HANDLER)
			handler(msg->id, channel, arg, a, b);
		if (!(flags & MSG_HANDLER_ONLY)) {
			ret = SmcCall(handle, channel, msg->ack, 0, SMC_INTR_DONE);
			if (ret == SCE_SBL_ERROR_NO_SESSION)
				return -1;
		} else {
			handler(msg->id, channel, arg, 0, 0);
			ret = SmcCall(handle, channel, msg->ack, 0, SMC_INTR_DONE);
			if (ret == SCE_SBL_ERROR_NO_SESSION)
				SCE_PANIC(1081);
		}
	}
	if (ret < 0)
		SCE_PANIC(1081);
	return -1;
}

/* name unknown: SMC command on a session channel (channel 1-3) */
static int SessionCall(SceUInt32 command, SceUID id, int channel, SceUInt32 value)
{
	Session *s;
	int status;

	if (s_state != STATE_READY)
		return SCE_SBL_ERROR_NOT_READY;
	if ((unsigned int)(channel - 1) >= 3)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;

	s = SessionLookup(id, &status);
	if (s == NULL)
		return SCE_SBL_ERROR_NO_SESSION;
	return SmcCall(s->handle, channel, value, 0, command);
}

static int accessMailbox(SceUInt32 command, SceUID id, int channel, SceUInt32 *value)
{
	SceUInt32 *buf;
	SceSize size;
	Session *s;
	int status, index, ret, result;

	if (s_state != STATE_READY)
		return SCE_SBL_ERROR_NOT_READY;
	if ((unsigned int)(channel - 1) >= 3)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;

	s = SessionLookup(id, &status);
	if (s == NULL)
		return SCE_SBL_ERROR_NO_SESSION;

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
	Session *s;
	int status;

	s = SessionLookup(id, &status);
	if (s == NULL)
		return;
	if (s->eventFlag != -1) {
		sceKernelDeleteEventFlag(s->eventFlag);
		s->eventFlag = -1;
	}
	if (s->mutex != -1) {
		sceKernelDeleteMutex(s->mutex);
		s->mutex = -1;
	}
	s->id = -1;
	s->used = 0;
}

/* name unknown */
static int SysEventHandler(int resume, int eventId, void *args, void *opt)
{
	int ret;

	if (eventId != SYSEVENT_RESUME_DEVICE(4))
		return 0;
	ret = sceSblTzsBridgeStart();
	return ret < 0 ? ret : 0;
}

int module_start(SceSize args, void *argp)
{
	HeapOpt opt;
	int ret, i;

	opt.size = sizeof(opt);
	opt.attr = 1;
	s_heap = ret = sceKernelCreateHeap("SceSblSmschedProxy", 0x1000, &opt);
	if (ret < 0)
		return ret;

	s_session = sceKernelAllocHeapMemory(s_heap, SESSION_COUNT * sizeof(Session));
	if (s_session == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;
	for (i = 0; i < SESSION_COUNT; i++)
		s_session[i].used = 0;

	ret = sceSblTzsBridgeStart();
	if (ret < 0)
		return ret;
	ret = sceKernelRegisterIntrHandler(4, "SceSblSmSchedProxySGI", 0x10, softIntrHandler,
					    NULL, 0x80, 0xf, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelRegisterSysEventHandler("SceSblSmSchedProxy", SysEventHandler, NULL);
	if (ret < 0)
		return ret;

	s_lock = 0;
	s_state = STATE_READY;
	return ret;
}

int SceSblSmSchedProxyForKernel_A488D604(void)
{
	if (sceKernelCpuId() != 0)
		return 0;
	return module_start(0, NULL);
}

/* name unknown */
static int Terminate(void)
{
	int ret;

	ret = SmcCall(0, 0, 0, 0, SMC_STOP);
	if (s_heap > 0) {
		if (s_session != NULL) {
			sceKernelFreeHeapMemory(s_heap, s_session);
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

int sceSblSmSchedProxyInvoke(SceUInt32 a, SceUInt32 b, SceUInt32 c, const SceUInt32 *in,
			     const void *param, SceUID *id)
{
	const SceUInt32 *p = param;
	SceUInt32 tls, *buf;
	SceSize size;
	Session *s;
	SceUID prev, newId;
	int state, i, index, ret, result;

	tls = sceSyscallEnter();
	if (s_state != STATE_READY) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_NOT_READY;
	}
	if (id == NULL) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	for (i = 0; i < SESSION_COUNT; i++)
		if (s_session[i].used == 0)
			break;
	if (i == SESSION_COUNT) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_NO_MEMORY;
	}
	s = &s_session[i];
	s->used = 1;
	prev = s_lastId;
	newId = ++s_lastId;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);

	s->id = newId;
	s->handle = -1;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		s->channel[i].handler = NULL;
		s->channel[i].arg = NULL;
		s->channel[i].pendingB = 0;
		s->channel[i].pendingA = 0;
	}
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
	if (prev == -2) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_NO_MEMORY;
	}
	*id = newId;

	index = sceSblTzsBridgeAllocArgArea(0x48, (void **)&buf, &size);
	if (index < 0) {
		SessionFree(newId);
		sceSyscallLeave(tls);
		return index;
	}
	for (i = 0; i < 4; i++)
		buf[i] = in != NULL ? in[i] : 0;
	buf[4] = newId;
	buf[6] = p[0x4 / 4];
	buf[7] = p[0x128 / 4];
	buf[8] = p[0x8 / 4];
	buf[9] = p[0xc / 4];
	for (i = 0; i < 8; i++)
		buf[10 + i] = p[0x18 / 4 + i];

	ret = sceSblTzsBridgePushArgArea(index);
	if (ret < 0)
		goto fail_buffer;
	result = SmcCall(a, b, c, index, SMC_INVOKE);
	ret = sceSblTzsBridgePopArgArea(index, (void **)&buf, &size);
	if (ret < 0)
		goto fail_buffer;
	if (size < 0x48)
		SCE_PANIC(302);
	s->handle = buf[5];

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

fail_session:
	SessionFree(newId);
	sceSyscallLeave(tls);
	return SCE_SBL_ERROR_NO_MEMORY;
}

int sceSblSmSchedProxyWait(SceUID id, SceUInt32 *result)
{
	SceUInt32 tls, *buf;
	SceSize size;
	Session *s;
	int status, index, ret, value;

	tls = sceSyscallEnter();
	if (s_state != STATE_READY) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_NOT_READY;
	}
	if (result == NULL)
		goto invalid;

	s = SessionLookup(id, &status);
	if (s == NULL) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_NO_SESSION;
	}
	if (sceKernelTryLockMutex(s->mutex, 1) < 0)
		goto invalid;

	sceKernelSysrootDisableAutoClockDown();
	ret = sceKernelWaitEventFlag(s->eventFlag, 1, 3, NULL, NULL);
	sceKernelSysrootEnableAutoClockDown();
	if (ret < 0)
		goto out;

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
			result[0] = buf[0];
			result[1] = buf[1];
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
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}

int sceSblSmSchedProxyGetStatus(SceUID id, SceUInt32 *status)
{
	SceUInt32 tls, *buf;
	SceSize size;
	Session *s;
	int st, index, ret, value;

	tls = sceSyscallEnter();
	if (s_state != STATE_READY) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_NOT_READY;
	}
	if (status == NULL) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	s = SessionLookup(id, &st);
	if (s == NULL) {
		sceSyscallLeave(tls);
		return SCE_SBL_ERROR_NO_SESSION;
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
			status[0] = buf[0];
			status[1] = buf[1];
			ret = sceSblTzsBridgeFreeArgArea(index);
			sceSyscallLeave(tls);
			return ret < 0 ? ret : value;
		}
	}
	sceSblTzsBridgeFreeArgArea(index);
	sceSyscallLeave(tls);
	return ret;
}

int SceSblSmSchedProxyForKernel_DE4EAC3C(SceUID id)
{
	SceUInt32 tls = sceSyscallEnter();
	Session *s;
	int status, ret;

	if (s_state != STATE_READY) {
		ret = SCE_SBL_ERROR_NOT_READY;
	} else {
		s = SessionLookup(id, &status);
		if (s == NULL)
			ret = SCE_SBL_ERROR_NO_SESSION;
		else
			ret = SmcCall(s->handle, 0, 0, 0, SMC_CHANGE_F00D_STATUS);
	}
	sceSyscallLeave(tls);
	return ret;
}

int SceSblSmSchedProxyForKernel_984EC9D1(void)
{
	sceSyscallLeave(sceSyscallEnter());
	return SCE_SBL_ERROR_NOT_SUPPORTED;
}

int SceSblSmSchedProxyForKernel_1DFC8624(void)
{
	sceSyscallLeave(sceSyscallEnter());
	return SCE_SBL_ERROR_NOT_SUPPORTED;
}

int SceSblSmSchedProxyForKernel_723B382F(SceUID id, int channel, SceUInt32 value)
{
	SceUInt32 tls = sceSyscallEnter();
	int ret = SessionCall(SMC_CALL_FUNC, id, channel, value);

	sceSyscallLeave(tls);
	return ret;
}

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
	int ret = SessionCall(SMC_3CE17233, id, channel, value);

	sceSyscallLeave(tls);
	return ret;
}

int sceSblSmSchedProxyWriteCry2Arm(SceUID id, int channel, SceUInt32 value)
{
	SceUInt32 tls = sceSyscallEnter();
	int ret = SessionCall(SMC_WRITE_CRY2ARM, id, channel, value);

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

int SceSblSmSchedProxyForKernel_8B84AC2A(SceUID id, int channel,
					      SceSblSmSchedCry2ArmHandler handler, void *arg)
{
	SceUInt32 tls = sceSyscallEnter();
	SceUInt32 a, b;
	Channel *ch;
	Session *s;
	int state, status, ret;

	if (s_state != STATE_READY) {
		ret = SCE_SBL_ERROR_NOT_READY;
		goto out;
	}
	if ((unsigned int)channel >= CHANNEL_COUNT || handler == NULL) {
		ret = SCE_SBL_ERROR_INVALID_ARGUMENT;
		goto out;
	}

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	s = SessionLookup(id, &status);
	if (s == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
		ret = SCE_SBL_ERROR_NO_SESSION;
		goto out;
	}
	ch = &s->channel[channel];
	if (ch->handler != NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
		ret = SCE_SBL_ERROR_BUSY;
		goto out;
	}
	ch->handler = handler;
	ch->arg = arg;
	b = ch->pendingB;
	a = ch->pendingA;
	ch->pendingB = 0;
	ch->pendingA = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);

	if (a != 0)
		handler(id, channel, arg, a, b);
	ret = SmcCall(s->handle, channel, 0, 0, SMC_ENABLE_CRY2ARM);
out:
	sceSyscallLeave(tls);
	return ret;
}

int SceSblSmSchedProxyForKernel_85EDA5FC(SceUID id, int channel)
{
	SceUInt32 tls = sceSyscallEnter();
	Session *s;
	int state, status, ret;

	if (s_state != STATE_READY) {
		ret = SCE_SBL_ERROR_NOT_READY;
		goto out;
	}
	if ((unsigned int)channel >= CHANNEL_COUNT) {
		ret = SCE_SBL_ERROR_INVALID_ARGUMENT;
		goto out;
	}

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	s = SessionLookup(id, &status);
	if (s == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
		ret = SCE_SBL_ERROR_NO_SESSION;
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
		ret = SCE_SBL_ERROR_NOT_READY;
	sceSyscallLeave(tls);
	return ret;
}
