/*
 * smsched.c: the scheduler. Requests wait in two run queues by priority;
 * one runs on the Cmep at a time. Mailboxes 1-3 belong to the running
 * module: its requests reach the registers, while a module that does not
 * run has its mailboxes saved in its request (SmschedMailbox), which the
 * secure kernel restores when it resumes the module.
 */
#include "smsched.h"
#include <sysmem/barrier.h>

static SmschedRequest *s_running;
static int s_initialized;
static SmschedLink s_runQueue[2];	/* by priority, 0 first */
static SceKernelSpinlock s_lock;
static SmschedInstance *s_instance;
static SmschedLink s_doneList;		/* exited, killed or stopped */

/* a word written for the Cmep: made visible before going on */
static inline void SmschedFlushWord(const volatile SceUInt32 *word)
{
	dmb();
	(void)*word;
	dsb();
}

/* a request by its id, or NULL */
static inline SmschedRequest *SmschedGetRequest(SceUInt32 id)
{
	if (id >= SMSCHED_REQUESTS)
		return NULL;
	return &s_instance->requests[id];
}

int sceSblSmSchedEnableIntr(int mailbox)
{
	if (mailbox == 0)
		return sceKernelEnableIntr(SMSCHED_INTR_CRY2ARM0);
	else if (mailbox == 1)
		return sceKernelEnableIntr(SMSCHED_INTR_CRY2ARM0 + 1);
	else if (mailbox == 2)
		return sceKernelEnableIntr(SMSCHED_INTR_CRY2ARM0 + 2);
	else if (mailbox == 3)
		return sceKernelEnableIntr(SMSCHED_INTR_CRY2ARM0 + 3);
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}

int sceSblSmSchedRegisterIntrHandlerInternal(SceUInt32 id, SceUInt32 mailbox,
					     SmschedIntrHandler handler, void *arg)
{
	SmschedRequest *request;
	int intr, ret;

	if (s_initialized != 1)
		return SCE_SBL_ERROR_NOT_READY;
	if (mailbox > 3)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	request = SmschedGetRequest(id);
	if (request == NULL) {
		ret = SCE_SBL_ERROR_NO_REQUEST;
	} else {
		if (request->intr[mailbox].handler != NULL) {
			ret = SCE_SBL_ERROR_BUSY;
		} else {
			request->intr[mailbox].handler = handler;
			request->intr[mailbox].arg = arg;
			request->intr[mailbox].enabled = 1;
			ret = 0;
			if (mailbox != 0 && SmschedStateOnCmep(request->status.state))
				ret = sceSblSmSchedEnableIntr(mailbox);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return ret;
}

/* name unknown (Sony's <name>MonitorCall family): the monitor call that turns
 * a mailbox's interrupt on for the request, without a handler */
int sceSblSmSchedRegisterIntrHandlerMonitorCall(SceUInt32 id, SceUInt32 mailbox)
{
	return sceSblSmSchedRegisterIntrHandlerInternal(id, mailbox, NULL, NULL);
}

int sceSblSmSchedDisableIntr(int mailbox)
{
	if (mailbox == 0)
		return sceKernelDisableIntr(SMSCHED_INTR_CRY2ARM0);
	else if (mailbox == 1)
		return sceKernelDisableIntr(SMSCHED_INTR_CRY2ARM0 + 1);
	else if (mailbox == 2)
		return sceKernelDisableIntr(SMSCHED_INTR_CRY2ARM0 + 2);
	else if (mailbox == 3)
		return sceKernelDisableIntr(SMSCHED_INTR_CRY2ARM0 + 3);
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}

/* name unknown: tell the Non-secure kernel through an interrupt area and
 * software interrupt 4; the mailbox interrupts stay off until it is done */
int sendIntrMessage(const SmschedIntrMessage *message)
{
	SceSize areaSize;
	SmschedIntrMessage *area;
	int index, ret, mailbox;

	if (message->id == -1)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	index = sceSblTzsBridgeAllocIntrArea(sizeof(*area), (void **)&area, &areaSize);
	if (index < 0)
		return 0;
	area->channel = message->channel;
	area->id = message->id;
	area->flags = message->flags;
	area->eventBits = message->eventBits;
	area->ack = index;
	area->status = message->status;
	ret = sceSblTzsBridgePushIntrArea(index);
	if (ret < 0) {
		sceSblTzsBridgeFreeIntrArea(index);
		return ret;
	}
	for (mailbox = 0; mailbox < CMEP_MAILBOXES; mailbox++) {
		ret = sceSblSmSchedDisableIntr(mailbox);
		if (ret < 0) {
			sceSblTzsBridgeFreeIntrArea(index);
			return ret;
		}
	}
	return sceKernelGenerateSoftIntr(SMSCHED_SOFT_INTR_PROXY, 2, 0);
}

int cry2Arm123IntHandler(int intrCode, void *userCtx)
{
	SmschedIntrMessage message;
	SmschedIntrHandler handler;
	volatile SmschedRequest *running;
	int intr, mailbox, ret;
	void *arg;

	(void)userCtx;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	running = s_running;
	if (running == NULL) {
		ret = -1;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
		goto out;
	}
	switch (intrCode) {
	case SMSCHED_INTR_CRY2ARM0 + 1:
		handler = running->intr[1].handler;
		mailbox = 1;
		arg = running->intr[1].arg;
		break;
	case SMSCHED_INTR_CRY2ARM0 + 2:
		handler = running->intr[2].handler;
		mailbox = 2;
		arg = running->intr[2].arg;
		break;
	default:
		SCE_PANIC(2514);
		/* fall through */
	case SMSCHED_INTR_CRY2ARM0 + 3:
		handler = running->intr[3].handler;
		mailbox = 3;
		arg = running->intr[3].arg;
		break;
	}
	if (running->proxyId != -1) {
		message.channel = mailbox;
		message.id = running->proxyId;
		message.flags = MSG_HANDLER_ONLY;
		if (sendIntrMessage(&message) < 0)
			SCE_PANIC(2535);
		ret = -2;
	} else {
		ret = -1;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	dsb();
	sev();
	if (handler != NULL)
		handler(s_running->id, mailbox, arg, 0, 0);
out:
	return ret;
}

int sceSblSmSchedReleaseIntrHandlerInternal(SceUInt32 id, SceUInt32 mailbox)
{
	SmschedRequest *request;
	int intr, ret;

	if (s_initialized != 1)
		return SCE_SBL_ERROR_NOT_READY;
	if (mailbox > 3)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	request = SmschedGetRequest(id);
	if (request == NULL) {
		ret = SCE_SBL_ERROR_NO_REQUEST;
	} else {
		request->intr[mailbox].handler = NULL;
		request->intr[mailbox].arg = NULL;
		ret = 0;
		if (mailbox != 0) {
			request->intr[mailbox].enabled = 0;
			if (SmschedStateOnCmep(request->status.state))
				ret = sceSblSmSchedDisableIntr(mailbox);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return ret;
}

/* name unknown (Sony's <name>MonitorCall family): the monitor call that turns
 * a mailbox's interrupt off for the request */
int sceSblSmSchedReleaseIntrHandlerMonitorCall(SceUInt32 id, SceUInt32 mailbox)
{
	return sceSblSmSchedReleaseIntrHandlerInternal(id, mailbox);
}

/* name unknown: acknowledge (clear) bits of a Cry2Arm mailbox */
int ackCry2Arm(int mailbox, SceUInt32 bits, SmschedRequest *request)
{
	if (request != NULL) {
		if (mailbox == 1) {
			request->mailbox->cry2Arm[0] &= ~(bits & request->mailbox->cry2Arm[0]);
			dmb();
			(void)*(volatile SceUInt32 *)&request->mailbox->cry2Arm[0];
			dsb();
			return 0;
		} else if (mailbox == 2) {
			request->mailbox->cry2Arm[1] &= ~(bits & request->mailbox->cry2Arm[1]);
			dmb();
			(void)*(volatile SceUInt32 *)&request->mailbox->cry2Arm[1];
			dsb();
			return 0;
		} else if (mailbox == 3) {
			request->mailbox->cry2Arm[2] &= ~(bits & request->mailbox->cry2Arm[2]);
			dmb();
			(void)*(volatile SceUInt32 *)&request->mailbox->cry2Arm[2];
			dsb();
			return 0;
		}
	} else {
		if (mailbox == 1) {
			CMEP_MAILBOX_REG(cry2Arm[1]) = bits;
			dmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[1]);
			dsb();
			return 0;
		} else if (mailbox == 2) {
			CMEP_MAILBOX_REG(cry2Arm[2]) = bits;
			dmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[2]);
			dsb();
			return 0;
		} else if (mailbox == 3) {
			CMEP_MAILBOX_REG(cry2Arm[3]) = bits;
			dmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[3]);
			dsb();
			return 0;
		}
	}
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}

/* name unknown: s_lock dropped until the request leaves state (the Cmep's
 * answer comes by interrupt), then held again; the new state in *newState */
void waitStateChange(SceUInt32 state, int *intr, SmschedRequest *request, SceUInt32 *newState)
{
	SceUInt32 current;
	int ret;

	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, *intr);
	for (;;) {
		ret = sceKernelSpinlockLowTrylockCpuSuspendIntr(&s_lock);
		if (ret >= 0) {
			*intr = ret;
			current = request->status.state;
			if (current != state)
				break;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, ret);
		}
		wfe();
	}
	*newState = current;
}

typedef int (*SmschedMailboxOp)(int mailbox, SceUInt32 value, SmschedRequest *request);
typedef int (*SmschedMailboxReadOp)(int mailbox, SceUInt32 *value, SmschedRequest *request);

/*
 * An operation on mailbox 1-3 of a request: on the Cmep's registers while
 * the module runs, on the request's saved mailbox while it is off the Cmep.
 * A module being started or suspended is waited for.
 */
int updateMailbox(SceUInt32 id, int mailbox, SceUInt32 value, SmschedMailboxOp op)
{
	SmschedRequest *request;
	SceUInt32 state;
	int intr, ret;

	if (s_initialized != 1)
		return SCE_SBL_ERROR_NOT_READY;
	if (mailbox < 1 || mailbox > 3)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	request = SmschedGetRequest(id);
	if (request == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
		return SCE_SBL_ERROR_NO_REQUEST;
	}
	sceKernelSpinlockLowLock(&request->lock);
	state = request->status.state;
	switch (state) {
	case SMSCHED_STATE_SUSPENDING:
		waitStateChange(SMSCHED_STATE_SUSPENDING, &intr, request, &state);
		if (state != SMSCHED_STATE_READY) {
			ret = SCE_SBL_ERROR_ILLEGAL_STATE;
			break;
		}
		/* fall through */
	case SMSCHED_STATE_READY:
		ret = op(mailbox, value, request);
		if (ret < 0)
			SCE_PANIC(2634);
		break;
	case SMSCHED_STATE_STARTING:
		waitStateChange(SMSCHED_STATE_STARTING, &intr, request, &state);
		if (state != SMSCHED_STATE_RUNNING) {
			ret = SCE_SBL_ERROR_ILLEGAL_STATE;
			break;
		}
		/* fall through */
	case SMSCHED_STATE_RUNNING:
	case SMSCHED_STATE_PREEMPTING:
		ret = op(mailbox, value, NULL);
		if (ret < 0)
			SCE_PANIC(2663);
		break;
	default:
		ret = SCE_SBL_ERROR_ILLEGAL_STATE;
		break;
	}
	sceKernelSpinlockLowUnlock(&request->lock);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return ret;
}

/* name unknown (Sony's <name>MonitorCall family): set bits of Arm2Cry 1-3 */
int sceSblSmSchedWriteArm2CryMonitorCall(SceUInt32 id, int mailbox, SceUInt32 bits)
{
	return updateMailbox(id, mailbox, bits, setArm2Cry);
}

/* name unknown (Sony's <name>MonitorCall family): clear bits of Arm2Cry 1-3 */
int sceSblSmSchedClearArm2CryMonitorCall(SceUInt32 id, int mailbox, SceUInt32 bits)
{
	return updateMailbox(id, mailbox, bits, clearArm2Cry);
}

/* name unknown (Sony's <name>MonitorCall family): acknowledge bits of Cry2Arm 1-3 */
int sceSblSmSchedWriteCry2ArmMonitorCall(SceUInt32 id, int mailbox, SceUInt32 bits)
{
	return updateMailbox(id, mailbox, bits, ackCry2Arm);
}

/* A read of mailbox 1-3 of a request, as updateMailbox */
int accessMailbox(SceUInt32 id, int mailbox, SceUInt32 *value, SmschedMailboxReadOp op)
{
	SmschedRequest *request;
	SceUInt32 state;
	int intr, ret;

	if (s_initialized != 1)
		return SCE_SBL_ERROR_NOT_READY;
	if (mailbox < 1 || mailbox > 3)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	request = SmschedGetRequest(id);
	if (request == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
		return SCE_SBL_ERROR_NO_REQUEST;
	}
	sceKernelSpinlockLowLock(&request->lock);
	state = request->status.state;
	switch (state) {
	case SMSCHED_STATE_SUSPENDING:
		waitStateChange(SMSCHED_STATE_SUSPENDING, &intr, request, &state);
		if (state != SMSCHED_STATE_READY) {
			ret = SCE_SBL_ERROR_ILLEGAL_STATE;
			break;
		}
		/* fall through */
	case SMSCHED_STATE_READY:
		ret = op(mailbox, value, request);
		if (ret < 0)
			SCE_PANIC(2756);
		break;
	case SMSCHED_STATE_STARTING:
		waitStateChange(SMSCHED_STATE_STARTING, &intr, request, &state);
		if (state != SMSCHED_STATE_RUNNING) {
			ret = SCE_SBL_ERROR_ILLEGAL_STATE;
			break;
		}
		/* fall through */
	case SMSCHED_STATE_RUNNING:
	case SMSCHED_STATE_PREEMPTING:
		ret = op(mailbox, value, NULL);
		if (ret < 0)
			SCE_PANIC(2785);
		break;
	default:
		ret = SCE_SBL_ERROR_ILLEGAL_STATE;
		break;
	}
	sceKernelSpinlockLowUnlock(&request->lock);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return ret;
}

int sceSblSmSchedReadArm2CryMonitorCall(SceUInt32 id, int mailbox, unsigned int index)
{
	SceSize areaSize;
	SceUInt32 value;
	SceUInt32 *area;
	int ret, err;

	ret = sceSblTzsBridgePopArgArea(index, (void **)&area, &areaSize);
	if (ret < 0)
		return ret;
	if (areaSize < sizeof(*area)) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_FAIL(1053);
#endif
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	ret = accessMailbox(id, mailbox, &value, readArm2Cry);
	*area = value;
	err = sceSblTzsBridgePushArgArea(index);
	if (err < 0)
		return err;
	return ret;
}

int sceSblSmSchedReadCry2ArmMonitorCall(SceUInt32 id, int mailbox, unsigned int index)
{
	SceSize areaSize;
	SceUInt32 value;
	SceUInt32 *area;
	int ret, err;

	ret = sceSblTzsBridgePopArgArea(index, (void **)&area, &areaSize);
	if (ret < 0)
		return ret;
	if (areaSize < sizeof(*area)) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_FAIL(1114);
#endif
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	ret = accessMailbox(id, mailbox, &value, readCry2Arm);
	*area = value;
	err = sceSblTzsBridgePushArgArea(index);
	if (err < 0)
		return err;
	return ret;
}

/* raise interrupt 0-4 on the Cmep for the running module */
int sceSblSmSchedGenerateMonitorIntr(int intr)
{
	if (s_initialized != 1)
		return SCE_SBL_ERROR_NOT_READY;
	if (intr == 0)
		return smsSecureKernelProxyGenerateIntr0();
	else if (intr == 1)
		return smsSecureKernelProxyGenerateIntr1();
	else if (intr == 2)
		return smsSecureKernelProxyGenerateIntr2();
	else if (intr == 3)
		return smsSecureKernelProxyGenerateIntr3();
	else if (intr == 4)
		return smsSecureKernelProxyGenerateIntr4();
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}

/* name unknown: set bits of an Arm2Cry mailbox */
int setArm2Cry(int mailbox, SceUInt32 bits, SmschedRequest *request)
{
	if (request != NULL) {
		if (mailbox == 1) {
			request->mailbox->arm2Cry[0] |= bits;
			return 0;
		} else if (mailbox == 2) {
			request->mailbox->arm2Cry[1] |= bits;
			return 0;
		} else if (mailbox == 3) {
			request->mailbox->arm2Cry[2] |= bits;
			return 0;
		}
	} else {
		if (mailbox == 1) {
			CMEP_MAILBOX_REG(arm2Cry[1]) = bits;
			return 0;
		} else if (mailbox == 2) {
			CMEP_MAILBOX_REG(arm2Cry[2]) = bits;
			return 0;
		} else if (mailbox == 3) {
			CMEP_MAILBOX_REG(arm2Cry[3]) = bits;
			return 0;
		}
	}
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}

/* name unknown: read an Arm2Cry mailbox */
int readArm2Cry(int mailbox, SceUInt32 *value, SmschedRequest *request)
{
	if (request != NULL) {
		if (mailbox == 1) {
			*value = request->mailbox->arm2Cry[0];
			return 0;
		} else if (mailbox == 2) {
			*value = request->mailbox->arm2Cry[1];
			return 0;
		} else if (mailbox == 3) {
			*value = request->mailbox->arm2Cry[2];
			return 0;
		}
	} else {
		if (mailbox == 1) {
			*value = CMEP_MAILBOX_REG(arm2Cry[1]);
			return 0;
		} else if (mailbox == 2) {
			*value = CMEP_MAILBOX_REG(arm2Cry[2]);
			return 0;
		} else if (mailbox == 3) {
			*value = CMEP_MAILBOX_REG(arm2Cry[3]);
			return 0;
		}
	}
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}

/* name unknown: clear bits of an Arm2Cry mailbox */
int clearArm2Cry(int mailbox, SceUInt32 bits, SmschedRequest *request)
{
	if (request != NULL) {
		if (mailbox == 1) {
			request->mailbox->arm2Cry[0] &= ~bits;
			return 0;
		} else if (mailbox == 2) {
			request->mailbox->arm2Cry[1] &= ~bits;
			return 0;
		} else if (mailbox == 3) {
			request->mailbox->arm2Cry[2] &= ~bits;
			return 0;
		}
	} else {
		if (mailbox == 1) {
			CMEP_MAILBOX_REG(arm2CryClear[0]) = bits;
			return 0;
		} else if (mailbox == 2) {
			CMEP_MAILBOX_REG(arm2CryClear[1]) = bits;
			return 0;
		} else if (mailbox == 3) {
			CMEP_MAILBOX_REG(arm2CryClear[2]) = bits;
			return 0;
		}
	}
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}

void disableMbox123(void)
{
	int mailbox;

	for (mailbox = 1; mailbox < CMEP_MAILBOXES; mailbox++) {
		if (sceSblSmSchedDisableIntr(mailbox) < 0)
			SCE_PANIC(2373);
	}
}

/* name unknown: the running module exited with the Cmep's message (its
 * result, or an error), reported if the request asked for it */
void exitRunningRequest(SceUInt32 message, int disable, volatile SmschedIntrMessage *notify)
{
	SmschedRequest *running = s_running;
	SceInt32 result;

	result = message;
	if (message & CMEP_MESSAGE_ERROR)
		result = SCE_SBL_ERROR_CMEP(message);
	running->status.result = result;
	running->status.state = SMSCHED_STATE_EXITED;
	if (disable)
		disableMbox123();
	if (s_running->notifyMask & SMSCHED_NOTIFY_EXITED) {
		notify->id = s_running->proxyId;
		notify->flags |= MSG_SET_EVENT;
		notify->eventBits |= SMSCHED_EVENT_DONE;
		notify->eventBits |= SMSCHED_EVENT_STATE;
	}
}

/* name unknown: read a Cry2Arm mailbox */
int readCry2Arm(int mailbox, SceUInt32 *value, SmschedRequest *request)
{
	if (request != NULL) {
		if (mailbox == 1) {
			*value = request->mailbox->cry2Arm[0];
			return 0;
		} else if (mailbox == 2) {
			*value = request->mailbox->cry2Arm[1];
			return 0;
		} else if (mailbox == 3) {
			*value = request->mailbox->cry2Arm[2];
			return 0;
		}
	} else {
		if (mailbox == 1) {
			*value = CMEP_MAILBOX_REG(cry2Arm[1]);
			dmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[1]);
			dsb();
			return 0;
		} else if (mailbox == 2) {
			*value = CMEP_MAILBOX_REG(cry2Arm[2]);
			dmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[2]);
			dsb();
			return 0;
		} else if (mailbox == 3) {
			*value = CMEP_MAILBOX_REG(cry2Arm[3]);
			dmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[3]);
			dsb();
			return 0;
		}
	}
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}

/* name unknown: the saved context dropped; the module runs anew */
void freeRequestContext(SmschedRequest *request)
{
	if (request->context != NULL) {
		sceKernelFreeUncacheHeapMemory(request->context);
		request->context = NULL;
	}
	if (request->vector != NULL) {
		sceKernelFreeUncacheHeapMemory(request->vector);
		request->vector = NULL;
	}
	request->contextType = SMSCHED_CONTEXT_NEW;
}

/* name unknown (cf. deleteAllRequest): a request back to the free list */
int deleteRequest(SceUInt32 id)
{
	SmschedRequest *request;

	request = SmschedGetRequest(id);
	if (request != NULL) {
		if (request->contextType == SMSCHED_CONTEXT_PREEMPTED)
			freeRequestContext(request);
		request->id = id | SMSCHED_REQUEST_FREE;
		SmschedLinkAddTail(&s_instance->freeList, &request->link);
	}
	return 0;
}

/* every queued and done request deleted, and the running one killed;
 * onlyProxied keeps the secure kernel's own requests (proxyId -1) */
int deleteAllRequest(int onlyProxied)
{
	SmschedRequest *request, *running;
	int intr;
	SceUInt32 state;
	int ret, i;

	disableMbox123();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	for (i = 0; i < 2; i++) {
		request = (SmschedRequest *)s_runQueue[i].next;
		while (&request->link != &s_runQueue[i] && request != NULL) {
			if (onlyProxied && request->proxyId == -1)
				continue;
			if (atomicCompareAndSet8(&request->waitFlag, 0, 1) == 0) {
				SmschedLinkRemove(&request->link);
				deleteRequest(request->id);
			} else {
				request->status.state = SMSCHED_STATE_KILLED;
			}
			request = (SmschedRequest *)s_runQueue[i].next;
		}
	}

	running = s_running;
	if (running == NULL || (onlyProxied && running->proxyId == -1)) {
		ret = 0;
	} else {
		state = running->status.state;
		switch (state) {
		case SMSCHED_STATE_STARTING:
			waitStateChange(SMSCHED_STATE_STARTING, &intr, running, &state);
			if (state != SMSCHED_STATE_RUNNING) {
				ret = SCE_SBL_ERROR_ILLEGAL_STATE;
				goto out;
			}
			/* fall through */
		case SMSCHED_STATE_RUNNING:
			ret = smsSecureKernelProxyHardKillSecureModule();
			if (ret < 0)
				goto out;
			deleteRequest(running->id);
			s_running = NULL;
			break;
		case SMSCHED_STATE_SUSPENDING:
			waitStateChange(SMSCHED_STATE_SUSPENDING, &intr, running, &state);
			if (state != SMSCHED_STATE_READY) {
				ret = SCE_SBL_ERROR_ILLEGAL_STATE;
				goto out;
			}
			ret = atomicCompareAndSet8(&running->waitFlag, 0, 1);
			if (ret == 0) {
				SmschedLinkRemove(&running->link);
				deleteRequest(running->id);
			} else {
				ret = 0;
				running->status.state = SMSCHED_STATE_KILLED;
			}
			break;
		case SMSCHED_STATE_STOPPING:
		case SMSCHED_STATE_KILLING:
		case SMSCHED_STATE_EXITING:
			waitStateChange(state, &intr, running, &state);
			if (state < SMSCHED_STATE_EXITED || state > SMSCHED_STATE_STOPPED) {
				ret = SCE_SBL_ERROR_ILLEGAL_STATE;
				goto out;
			}
			ret = atomicCompareAndSet8(&running->waitFlag, 0, 1);
			if (ret == 0) {
				SmschedLinkRemove(&running->link);
				deleteRequest(running->id);
			} else {
				ret = 0;
			}
			break;
		default:
			SCE_PANIC(657);
			ret = SCE_SBL_ERROR_ILLEGAL_STATE;
			goto out;
		}
	}

	request = (SmschedRequest *)s_doneList.next;
	while (&request->link != &s_doneList && request != NULL) {
		if (onlyProxied && request->proxyId == -1)
			continue;
		if (atomicCompareAndSet8(&request->waitFlag, 0, 1) == 0) {
			SmschedLinkRemove(&request->link);
			deleteRequest(request->id);
		} else {
			request->status.state = SMSCHED_STATE_KILLED;
		}
		request = (SmschedRequest *)s_doneList.next;
	}
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return ret;
}

int sceSblSmSchedFini(int stop)
{
	SmschedRequest *request;
	SceUID heap;
	int intrCode, ret, i;

	for (intrCode = SMSCHED_INTR_CRY2ARM0; intrCode < SMSCHED_INTR_CRY2ARM0 + CMEP_MAILBOXES; intrCode++) {
		sceKernelDisableIntr(intrCode);
		sceKernelReleaseIntrHandler(intrCode);
	}
	ret = stop;
	if (ret == 0)
		return ret;
	ret = smsSecureKernelProxyStopSecureKernel();
	if (s_instance == NULL)
		return ret;
	if (s_instance->debugArea != NULL) {
		sceKernelFreeUncacheHeapMemory(s_instance->debugArea);
		s_instance->debugArea = NULL;
	}
	if (s_instance->skCmdArea != NULL) {
		sceKernelFreeUncacheHeapMemory(s_instance->skCmdArea);
		s_instance->skCmdArea = NULL;
	}
	if (s_instance->self != NULL) {
		sceKernelFreeUncacheHeapMemory(s_instance->self);
		s_instance->self = NULL;
	}
	for (i = 0; i < SMSCHED_REQUESTS; i++) {
		request = &s_instance->requests[i];
		if (request->area != NULL) {
			sceKernelFreeUncacheHeapMemory(request->area);
			request->area = NULL;
			request->mailbox = NULL;
		}
	}
	if (s_instance != NULL) {
		if (s_instance->requests != NULL) {
			sceKernelFreeHeapMemory(s_instance->heap, s_instance->requests);
			s_instance->requests = NULL;
		}
		heap = s_instance->heap;
		sceKernelFreeHeapMemory(heap, s_instance);
		s_instance = NULL;
		if (heap >= 0)
			sceKernelDeleteHeap(heap);
	}
	return ret;
}

/* the module's saved mailboxes 1-3 into the Cmep's registers */
static inline void SmschedRestoreMailbox(const SmschedRequest *request)
{
	CMEP_MAILBOX_REG(arm2CryClear[0]) = 0xffffffff;
	CMEP_MAILBOX_REG(arm2CryClear[1]) = 0xffffffff;
	CMEP_MAILBOX_REG(arm2CryClear[2]) = 0xffffffff;
	CMEP_MAILBOX_REG(arm2Cry[1]) = request->mailbox->arm2Cry[0];
	CMEP_MAILBOX_REG(arm2Cry[2]) = request->mailbox->arm2Cry[1];
	CMEP_MAILBOX_REG(arm2Cry[3]) = request->mailbox->arm2Cry[2];
}

/*
 * The first request of the run queues (priority 0 first) onto the Cmep: run
 * anew or resumed when nothing runs; when a module runs, it is preempted
 * (its context saved by the secure kernel) if the request has the higher
 * priority, and the request waits at the head of its queue.
 */
void reschedule(void)
{
	SmschedRequest *request, *running;
	SmschedLink *queue;
	SceKernelVARange range;
	SceKernelPAVector paVector;
	SceAllocOpt opt;
	SceKernelPARange *vector;
	uintptr_t vectorPA;
	void *context;
	int ret;

	queue = &s_runQueue[0];
	request = (SmschedRequest *)queue->next;
	if (&request->link == queue) {
		queue = &s_runQueue[1];
		request = (SmschedRequest *)queue->next;
		if (&request->link == queue)
			return;
	}
	SmschedLinkRemove(&request->link);

	running = s_running;
	if (running == NULL) {
		request->status.state = SMSCHED_STATE_STARTING;
		switch (request->contextType) {
		case SMSCHED_CONTEXT_NEW:
		case 4:
			SmschedRestoreMailbox(request);
			request->mailbox->arm2Cry[0] = 0;
			request->mailbox->arm2Cry[1] = 0;
			request->mailbox->arm2Cry[2] = 0;
			request->mailbox->cry2Arm[0] = 0;
			request->mailbox->cry2Arm[1] = 0;
			request->mailbox->cry2Arm[2] = 0;
			ret = smsSecureKernelProxyRunSecureModule(request->selfPA, request->arg1,
								  (SceUInt64)request->arg2High << 32 | request->arg2Low,
								  request->arg3,
								  request->arg4, &request->invokeArg,
								  request->areaPA);
			if (ret < 0)
				SCE_PANIC(1898);
			break;
		case 2:
		case SMSCHED_CONTEXT_PREEMPTED:
			SmschedRestoreMailbox(request);
			ret = smsSecureKernelProxyResumeSecureModule(request->vectorPA,
								     request->vectorCount,
								     request->areaPA,
								     request->mailboxPA);
			if (ret < 0)
				SCE_PANIC(1930);
			break;
		default:
			SCE_PANIC(1938);
		}
		s_running = request;
		return;
	}

	/* back to the head of its queue */
	SmschedLinkAddTail(queue->next, &request->link);
	if (running->status.state == SMSCHED_STATE_STARTING ||
	    running->status.state == SMSCHED_STATE_SUSPENDING ||
	    running->status.state == SMSCHED_STATE_STOPPING ||
	    running->status.state == SMSCHED_STATE_KILLING ||
	    running->status.state == SMSCHED_STATE_EXITED ||
	    running->status.state == SMSCHED_STATE_STOPPED ||
	    running->status.state == SMSCHED_STATE_KILLED ||
	    running->status.state == SMSCHED_STATE_READY ||
	    running->status.state == SMSCHED_STATE_PREEMPTING ||
	    running->status.state == SMSCHED_STATE_SOFT_KILLING ||
	    running->status.state == SMSCHED_STATE_EXITING)
		return;
	if (running->status.state != SMSCHED_STATE_RUNNING)
		SCE_PANIC(1966);
	if (request->priority >= running->priority)
		return;

	running->status.state = SMSCHED_STATE_PREEMPTING;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 64;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	context = sceKernelAllocUncacheHeapMemoryWithOption(SMSCHED_CONTEXT_SIZE, &opt);
	if (context == NULL)
		goto fail;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 64;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	vector = sceKernelAllocUncacheHeapMemoryWithOption(SMSCHED_CONTEXT_RANGES * sizeof(*vector), &opt);
	if (vector == NULL) {
		sceKernelFreeHeapMemory(s_instance->heap, context);
		goto fail;
	}
	range.addr = (uint32_t)context;
	range.size = SMSCHED_CONTEXT_SIZE;
	paVector.size = sizeof(paVector);
	paVector.capacity = SMSCHED_CONTEXT_RANGES;
	paVector.pRanges = vector;
	if (sceKernelVARangeToPAVector(&range, &paVector) < 0)
		goto free;
	SmschedFlushWord(&vector[0].addr);
	SmschedFlushWord(&vector[0].size);
	if (sceKernelVAtoPA(vector, &vectorPA) < 0)
		goto free;
	running->contextType = SMSCHED_CONTEXT_PREEMPTED;
	running->context = context;
	running->vector = vector;
	running->vectorPA = vectorPA;
	running->vectorCount = paVector.total;
	ret = smsSecureKernelProxySuspendSecureModule(s_running->vectorPA, s_running->vectorCount,
						      s_running->areaPA, s_running->mailboxPA);
	if (ret < 0)
		SCE_PANIC(1997);
	return;
free:
	sceKernelFreeUncacheHeapMemory(context);
	sceKernelFreeUncacheHeapMemory(vector);
fail:
	SCE_PANIC(1987);
}

/* acknowledge a message or event of the secure kernel */
static inline void SmschedAckMessage(SceUInt32 bits)
{
	CMEP_MAILBOX_REG(cry2Arm[0]) = bits;
	SmschedFlushWord(&CMEP_MAILBOX_REG(cry2Arm[0]));
}

static inline void enableMbox123(void)
{
	int mailbox;

	for (mailbox = 1; mailbox < CMEP_MAILBOXES; mailbox++) {
		if (s_running->intr[mailbox].enabled) {
			if (sceSblSmSchedEnableIntr(mailbox) < 0)
				SCE_PANIC(2353);
		}
	}
}

/*
 * Interrupt 200: a message or event of the secure kernel about the running
 * module moves its request on (see SMSCHED_STATE_*), reported to the
 * Non-secure kernel as notifyMask asks; a module off the Cmep lets the next
 * request run.
 */
int cry2Arm0IntHandler(int intrCode, void *userCtx)
{
	volatile SmschedIntrMessage message;
	SmschedIntrHandler handler;
	SmschedRequest *running;
	SceUInt32 reg, msg;
	void *arg;
	int intr, ret, disable;

	(void)intrCode;
	(void)userCtx;
	message.channel = 0;
	message.id = -1;
	message.flags = 0;
	message.eventBits = 0;
	message.ack = 0;
	message.status.result = 0;
	message.status.state = 0;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	reg = CMEP_MAILBOX_REG(cry2Arm[0]);
	if (reg & CMEP_EVENT_SK_PRINT) {
		SCE_DBG_PRINTF(0, 2077, "s:%s", (const char *)s_instance->debugArea);
		SmschedAckMessage(CMEP_EVENT_SK_PRINT);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
		return -1;
	}
	if (reg & CMEP_EVENT_SK_PANIC) {
		SCE_PRINTF_WITH_INFO(0, 2092, "s:%s", (const char *)s_instance->debugArea);
		SmschedAckMessage(CMEP_EVENT_SK_PANIC);
		SCE_PANIC(2098);
	}

	if (reg & CMEP_EVENT_SM_EXITED)
		msg = CMEP_EVENT_SM_EXITED;
	else if (reg & CMEP_MESSAGE_MASK)
		msg = reg & CMEP_MESSAGE_MASK;
	else if (reg & CMEP_EVENT_FATAL1)
		msg = CMEP_EVENT_FATAL1;
	else if (reg & CMEP_EVENT_FATAL2)
		msg = CMEP_EVENT_FATAL2;
	else if (reg & CMEP_EVENT_FATAL3)
		msg = CMEP_EVENT_FATAL3;
	else
		SCE_PANIC(2070);

	if (msg & CMEP_EVENT_FATAL1) {
		SCE_PRINTF_WITH_INFO(0, 2102, "s:%s", (const char *)s_instance->debugArea);
		SmschedAckMessage(msg);
		SCE_PANIC(2108);
	}
	if (msg & CMEP_EVENT_FATAL2) {
		SCE_PRINTF_WITH_INFO(0, 2112, "s:%s", (const char *)s_instance->debugArea);
		SmschedAckMessage(msg);
		SCE_PANIC(2118);
	}
	if (msg & CMEP_EVENT_FATAL3) {
		SCE_PRINTF_WITH_INFO(0, 2122, "s:%s", (const char *)s_instance->debugArea);
		SmschedAckMessage(CMEP_EVENT_FATAL3);
		SCE_PANIC(2128);
	}
	SmschedAckMessage(msg);

	if (s_running == NULL)
		SCE_PANIC(2145);
	handler = NULL;
	arg = NULL;
	switch (s_running->status.state) {
	case SMSCHED_STATE_STARTING:
		if (s_running->contextType == SMSCHED_CONTEXT_PREEMPTED)
			freeRequestContext(s_running);
		if (msg == CMEP_EVENT_SM_EXITED) {
			s_running->status.state = SMSCHED_STATE_EXITING;
			s_running->status.result = s_running->area->result;
			handler = s_running->intr[0].handler;
			arg = s_running->intr[0].arg;
			message.id = s_running->proxyId;
			message.flags |= MSG_CALL_HANDLER;
			message.status = s_running->status;
		} else if (msg == CMEP_MESSAGE_SM_STARTED) {
			s_running->status.state = SMSCHED_STATE_RUNNING;
			handler = s_running->intr[0].handler;
			arg = s_running->intr[0].arg;
			message.id = s_running->proxyId;
			message.flags |= MSG_CALL_HANDLER;
			message.status = s_running->status;
			enableMbox123();
			if (s_running->notifyMask & SMSCHED_NOTIFY_RUNNING) {
				message.id = s_running->proxyId;
				message.flags |= MSG_SET_EVENT;
				message.eventBits |= SMSCHED_EVENT_STATE;
			}
			goto reschedule;
		} else {
			exitRunningRequest(msg, 1, &message);
			handler = s_running->intr[0].handler;
			arg = s_running->intr[0].arg;
			message.id = s_running->proxyId;
			message.flags |= MSG_CALL_HANDLER;
			message.status = s_running->status;
			running = s_running;
			SmschedLinkAddTail(&s_doneList, &running->link);
		}
		break;
	case SMSCHED_STATE_SUSPENDING:
		if (msg != CMEP_MESSAGE_SM_SHUT_DOWN) {
			disable = 0;
			goto exit;
		}
		s_running->status.state = SMSCHED_STATE_READY;
		if (s_running->notifyMask & SMSCHED_NOTIFY_READY) {
			message.id = s_running->proxyId;
			message.flags |= MSG_SET_EVENT;
			message.eventBits |= SMSCHED_EVENT_STATE;
		}
		/* back to the head of its queue */
		running = s_running;
		SmschedLinkAddTail(s_runQueue[running->priority].next, &running->link);
		break;
	case SMSCHED_STATE_READY:
	case SMSCHED_STATE_EXITED:
	case SMSCHED_STATE_KILLED:
	case SMSCHED_STATE_STOPPED:
		if (msg != CMEP_MESSAGE_SM_READY)
			SCE_PANIC(2297);
		s_running = NULL;
		goto reschedule;
	case SMSCHED_STATE_RUNNING:
		if (msg == CMEP_EVENT_SM_EXITED) {
			s_running->status.state = SMSCHED_STATE_EXITED;
			disableMbox123();
			if (s_running->notifyMask & SMSCHED_NOTIFY_EXITED) {
				message.id = s_running->proxyId;
				message.flags |= MSG_SET_EVENT;
				message.eventBits |= SMSCHED_EVENT_DONE;
				message.eventBits |= SMSCHED_EVENT_STATE;
			}
			s_running->status.result = s_running->area->result;
			running = s_running;
			SmschedLinkAddTail(&s_doneList, &running->link);
			break;
		}
		if (msg != CMEP_MESSAGE_SM_SHUT_DOWN)
			SCE_PANIC(2199);
killed:
		s_running->status.state = SMSCHED_STATE_KILLED;
		disableMbox123();
		if (s_running->notifyMask & SMSCHED_NOTIFY_KILLED)
			goto notify;
		goto done;
	case SMSCHED_STATE_STOPPING:
		if (msg == CMEP_EVENT_SM_EXITED)
			goto exiting;
		if (msg != CMEP_MESSAGE_SM_SHUT_DOWN)
			goto exit1;
		s_running->status.state = SMSCHED_STATE_STOPPED;
		disableMbox123();
		if (s_running->notifyMask & SMSCHED_NOTIFY_STOPPED)
			goto notify;
		goto done;
	case SMSCHED_STATE_KILLING:
		if (msg != CMEP_MESSAGE_SM_SHUT_DOWN)
			goto exit1;
		goto killed;
	case SMSCHED_STATE_EXITING:
		if (msg != CMEP_MESSAGE_SM_STARTED && msg != CMEP_MESSAGE_SM_EXIT_FAILED)
			SCE_PANIC(2278);
		s_running->status.state = SMSCHED_STATE_EXITED;
		disableMbox123();
		if (s_running->notifyMask & SMSCHED_NOTIFY_EXITED)
			goto notify;
		goto done;
	case SMSCHED_STATE_PREEMPTING:
		if (msg == CMEP_EVENT_SM_EXITED)
			goto exiting;
		if (msg != CMEP_MESSAGE_SM_SUSPENDING)
			goto exit1;
		s_running->status.state = SMSCHED_STATE_SUSPENDING;
		disableMbox123();
		break;
	case SMSCHED_STATE_SOFT_KILLING:
		if (msg == CMEP_EVENT_SM_EXITED)
			goto exiting;
		if (msg == CMEP_MESSAGE_SM_KILLING) {
			s_running->status.state = SMSCHED_STATE_KILLING;
			break;
		}
		goto exit1;
	default:
		SCE_PANIC(2304);
	}
	goto out;

exiting:
	/* the module returned; the secure kernel unloads it */
	s_running->status.state = SMSCHED_STATE_EXITING;
	s_running->status.result = s_running->area->result;
	goto out;
reschedule:
	reschedule();
	goto out;
exit1:
	disable = 1;
exit:
	exitRunningRequest(msg, disable, &message);
	goto done;
notify:
	message.id = s_running->proxyId;
	message.flags |= MSG_SET_EVENT;
	message.eventBits |= SMSCHED_EVENT_DONE;
	message.eventBits |= SMSCHED_EVENT_STATE;
done:
	running = s_running;
	SmschedLinkAddTail(&s_doneList, &running->link);
out:
	if (message.id != -1) {
		if (sendIntrMessage((const SmschedIntrMessage *)&message) < 0)
			SCE_PANIC(2314);
		ret = -2;
	} else {
		ret = message.id;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	if (handler != NULL)
		handler(message.id, 0, arg, message.status.state, message.status.result);
	return ret;
}

/*
 * A request for a secure module, queued by priority; the Non-secure
 * kernel's (proxyId other than -1) are told when it is done.
 */
int sceSblSmSchedInvokeInternal(SceUInt32 priority, uintptr_t selfPA, SceUInt32 arg1,
				SceUInt32 arg2Low, SceUInt32 arg2High, SceUInt32 arg3,
				SceUInt32 arg4, SceInt32 proxyId,
				const SmschedInvokeArg *invokeArg, SceUInt32 *id)
{
	volatile SmschedRequest *request;
	SmschedLink *link;
	SceUInt32 requestId;
	int intr;

	if (s_initialized != 1)
		return SCE_SBL_ERROR_NOT_READY;
	if (priority > 1)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (invokeArg == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (id == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	switch (invokeArg->selfType & SMSCHED_INVOKE_SELF_MASK) {
	case SMSCHED_INVOKE_SELF_PA:
		if (selfPA == 0)
			return SCE_SBL_ERROR_INVALID_ARGUMENT;
		break;
	case SMSCHED_INVOKE_SELF_INSTANCE:
		if (selfPA != 0)
			return SCE_SBL_ERROR_INVALID_ARGUMENT;
		if (arg1 != 0)
			return SCE_SBL_ERROR_INVALID_ARGUMENT;
		selfPA = s_instance->selfPA;
		arg1 = 1;
		break;
	default:
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	link = s_instance->freeList.next;
	if (link == &s_instance->freeList)
		goto nomem;
	link->next->prev = link->prev;
	link->prev->next = link->next;
	request = (SmschedRequest *)link;
	request->id &= ~SMSCHED_REQUEST_FREE;
	request->waitFlag = 0;
	requestId = request->id;
	request->priority = 0;
	request->selfPA = 0;
	request->arg1 = 0;
	request->arg2Low = 0;
	request->arg2High = 0;
	request->arg3 = 0;
	request->arg4 = 0;
	request->status.result = 0;
	request->status.state = SMSCHED_STATE_READY;
	request->contextType = SMSCHED_CONTEXT_NEW;
	request->vectorPA = 0;
	request->vectorCount = 0;
	request->context = NULL;
	request->vector = NULL;
	request->proxyId = -1;
	request->notifyMask = 0;
	request->invokeArg.selfType = 0;
	request->invokeArg.mediaType = 0;
	request->invokeArg.programAuthorityId = 0;
	request->invokeArg.capability[0] = 0;
	request->invokeArg.capability[1] = 0;
	request->invokeArg.capability[2] = 0;
	request->invokeArg.capability[3] = 0;
	request->intr[0].handler = NULL;
	request->intr[0].arg = NULL;
	request->intr[0].enabled = 1;
	request->intr[1].handler = NULL;
	request->intr[1].arg = NULL;
	request->intr[1].enabled = 0;
	request->intr[2].handler = NULL;
	request->intr[2].arg = NULL;
	request->intr[2].enabled = 0;
	request->intr[3].handler = NULL;
	request->intr[3].arg = NULL;
	request->intr[3].enabled = 0;
	request->lock = 0;
	link->next = link;
	link->prev = link;
	request->mailbox->arm2Cry[0] = 0;
	request->mailbox->arm2Cry[1] = 0;
	request->mailbox->arm2Cry[2] = 0;
	request->mailbox->cry2Arm[0] = 0;
	request->mailbox->cry2Arm[1] = 0;
	request->mailbox->cry2Arm[2] = 0;
	dmb();
	(void)*(volatile SceUInt32 *)&request->mailbox->cry2Arm[2];
	dsb();
	if (requestId == (SceUInt32)-1)
		goto nomem;

	request->priority = priority;
	request->selfPA = selfPA;
	request->arg1 = arg1;
	request->arg2Low = arg2Low;
	request->arg2High = arg2High;
	request->arg3 = arg3;
	request->arg4 = arg4;
	request->proxyId = proxyId;
	request->invokeArg = *invokeArg;
	request->notifyMask = proxyId != -1 ? SMSCHED_NOTIFY_DONE : 0;
	*id = requestId;
	SmschedLinkAddTail(&s_runQueue[priority], link);
	reschedule();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return 0;
nomem:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return SCE_SBL_ERROR_NO_MEMORY;
}

/* name unknown: a PA the Non-secure kernel may pass (0 for none): DRAM above
 * the secure kernel's, SPAD32K, SPAD128K and 0x20000000-0x2fffffff */
static inline int SmschedIsNonSecurePA(uintptr_t pa)
{
	if (pa == 0)
		return 1;
	if (pa >= 0x40200000 && pa < 0x80000000)
		return 1;
	if (pa >= 0x80000000 && pa < 0xc0000000)
		return 1;
	if (pa >= 0x1f000000 && pa < 0x1f008000)
		return 1;
	if (pa >= 0x1f840000 && pa < 0x1f860000)
		return 1;
	if (pa >= 0x20000000 && pa < 0x30000000)
		return 1;
	return 0;
}

int sceSblSmSchedInvokeMonitorCall(SceUInt32 priority, uintptr_t selfPA, SceUInt32 arg1,
				   unsigned int index)
{
	SceSize areaSize;
	SceSblSmSchedProxyInvokeArg *area;
	int ret, err;

	ret = sceSblTzsBridgePopArgArea(index, (void **)&area, &areaSize);
	if (ret < 0)
		return ret;
	if (areaSize < sizeof(*area)) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_FAIL(860);
#endif
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	if (!SmschedIsNonSecurePA(selfPA))
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	ret = sceSblSmSchedInvokeInternal(priority, selfPA, arg1, area->arg2Low, area->arg2High,
					  area->arg3, area->arg4, area->proxyId,
					  &area->invokeArg, &area->requestId);
	err = sceSblTzsBridgePushArgArea(index);
	if (err < 0)
		return err;
	return ret;
}

/* name unknown (smc 0x13b, the henkaku wiki's "Uninitialize"): every
 * request of the Non-secure kernel dropped */
int sceSblSmSchedStopMonitorCall(void)
{
	int ret;

	ret = deleteAllRequest(1);
	reschedule();
	return ret;
}

/* name unknown (smc 0x13a): the Non-secure kernel has taken a message of
 * sendIntrMessage; the interrupts it disabled come back */
int sceSblSmSchedIntrDoneMonitorCall(SceUInt32 id, int mailbox, unsigned int index)
{
	volatile SmschedRequest *request;
	int intr, ret;

	(void)mailbox;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	ret = sceSblTzsBridgeFreeIntrArea(index);
	if (ret < 0)
		goto out;
	ret = sceKernelEnableIntr(SMSCHED_INTR_CRY2ARM0);
	if (ret < 0)
		goto out;
	if (id == (SceUInt32)-1)
		goto out;
	request = SmschedGetRequest(id);
	if (request == NULL) {
		ret = SCE_SBL_ERROR_NO_REQUEST;
		goto out;
	}
	if (SmschedStateOnCmep(request->status.state)) {
		for (mailbox = 1; mailbox < CMEP_MAILBOXES; mailbox++) {
			if (request->intr[mailbox].enabled) {
				ret = sceSblSmSchedEnableIntr(mailbox);
				if (ret < 0)
					goto out;
			}
		}
	}
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return ret;
}

/* smc 0x131, reserved: checks its PA and does nothing */
int sceSblSmSchedReserved131MonitorCall(SceUInt32 arg0, uintptr_t pa)
{
	(void)arg0;
	if (!SmschedIsNonSecurePA(pa))
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_NOT_SUPPORTED;
}

/* smc 0x132, reserved: the same */
int sceSblSmSchedReserved132MonitorCall(uintptr_t pa)
{
	if (!SmschedIsNonSecurePA(pa))
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_NOT_SUPPORTED;
}

int sceSblSmSchedWaitMonitorCall(SceUInt32 id, unsigned int index);
int sceSblSmSchedGetStatusMonitorCall(SceUInt32 id, unsigned int index);
int sceSblSmSchedKillMonitorCall(SceUInt32 id);

/* the Non-secure kernel's calls (smsc_proxy), by SMC number */
typedef struct SmschedMonitorCall {
	SceUInt32 callId;
	SceKernelMonitorCallHandler handler;
} SmschedMonitorCall;

#define SMSCHED_MONITOR_CALL(id, handler)	{ (id), (SceKernelMonitorCallHandler)(handler) }

static const SmschedMonitorCall s_monitorCalls[] = {
	SMSCHED_MONITOR_CALL(0x12d, sceSblSmSchedInvokeMonitorCall),
	SMSCHED_MONITOR_CALL(0x12e, sceSblSmSchedWaitMonitorCall),
	SMSCHED_MONITOR_CALL(0x12f, sceSblSmSchedGetStatusMonitorCall),
	SMSCHED_MONITOR_CALL(0x130, sceSblSmSchedKillMonitorCall),
	SMSCHED_MONITOR_CALL(0x131, sceSblSmSchedReserved131MonitorCall),
	SMSCHED_MONITOR_CALL(0x132, sceSblSmSchedReserved132MonitorCall),
	SMSCHED_MONITOR_CALL(0x133, sceSblSmSchedWriteArm2CryMonitorCall),
	SMSCHED_MONITOR_CALL(0x134, sceSblSmSchedReadArm2CryMonitorCall),
	SMSCHED_MONITOR_CALL(0x135, sceSblSmSchedClearArm2CryMonitorCall),
	SMSCHED_MONITOR_CALL(0x136, sceSblSmSchedWriteCry2ArmMonitorCall),
	SMSCHED_MONITOR_CALL(0x137, sceSblSmSchedReadCry2ArmMonitorCall),
	SMSCHED_MONITOR_CALL(0x138, sceSblSmSchedRegisterIntrHandlerMonitorCall),
	SMSCHED_MONITOR_CALL(0x139, sceSblSmSchedReleaseIntrHandlerMonitorCall),
	SMSCHED_MONITOR_CALL(0x13a, sceSblSmSchedIntrDoneMonitorCall),
	SMSCHED_MONITOR_CALL(0x13b, sceSblSmSchedStopMonitorCall),
	SMSCHED_MONITOR_CALL(0x13c, sceSblSmSchedGenerateMonitorIntr),
};

/*
 * The scheduler's instance and its 64 requests, the secure kernel started
 * (with the revocation list the boot loader passed), then the Cmep's
 * interrupts and the monitor calls.
 */
int sceSblSmSchedModuleStart(SceSize args, void *argp)
{
	SceKernelHeapCreateOpt heapOpt;
	SceAllocOpt opt;
	const SceKblParam *kblParam;
	SmschedInstance *instance;
	SmschedRequest *request;
	SceKernelPARange kprxAuthSm;
	uintptr_t skAddr, rvkAddr;
	SceSize skSize, rvkSize;
	SmschedArea *area;
	SceUID heap;
	int ret, i;

	(void)args;
	(void)argp;
	kblParam = sceKernelSysrootGetKblParam();
	skAddr = kblParam->secure_kernel.addr;
	skSize = kblParam->secure_kernel.size;
	rvkAddr = kblParam->prog_rvk.addr;
	rvkSize = kblParam->prog_rvk.size;
	kprxAuthSm = kblParam->kprx_auth_sm;
	if (s_initialized) {
		ret = SCE_SBL_ERROR_ILLEGAL_STATE;
		goto fail;
	}

	heapOpt.size = sizeof(heapOpt);
	heapOpt.flags = HEAP_AUTO_EXTEND;
	heap = sceKernelCreateHeap("SceSmsched", 0x1000, &heapOpt);
	if (heap < 0) {
		ret = heap;
		goto fail;
	}
	instance = sceKernelAllocHeapMemory(heap, sizeof(*instance));
	if (instance == NULL) {
		ret = SCE_SBL_ERROR_NO_MEMORY;
		sceKernelDeleteHeap(heap);
		goto fail;
	}
	instance->heap = heap;
	instance->debugArea = NULL;
	instance->skCmdArea = NULL;
	instance->self = NULL;
	s_instance = instance;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 64;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	instance->debugArea = sceKernelAllocUncacheHeapMemoryWithOption(SMSCHED_DEBUG_AREA_SIZE, &opt);
	if (instance->debugArea == NULL)
		goto nomem;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 64;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	instance->skCmdArea = sceKernelAllocUncacheHeapMemoryWithOption(SMSCHED_SK_CMD_AREA_SIZE, &opt);
	if (instance->skCmdArea == NULL)
		goto nomem;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 64;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	instance->self = sceKernelAllocUncacheHeapMemoryWithOption(sizeof(*instance->self), &opt);
	if (instance->self == NULL)
		goto nomem;
	ret = sceKernelVAtoPA(instance->self, &instance->selfPA);
	if (ret < 0)
		goto fail;
	*instance->self = kprxAuthSm;
	dmb();
	(void)*(volatile SceSize *)&instance->self->size;
	dsb();

	instance->requests = sceKernelAllocHeapMemory(instance->heap,
							    SMSCHED_REQUESTS * sizeof(SmschedRequest));
	if (instance->requests == NULL)
		goto nomem;
	instance->freeList.next = &instance->freeList;
	instance->freeList.prev = &instance->freeList;
	for (i = 0; i < SMSCHED_REQUESTS; i++) {
		request = &instance->requests[i];
		request->id = i | SMSCHED_REQUEST_FREE;
		opt.size = sizeof(opt);
		opt.mmapKind = 0;
		opt.align = 64;
		opt.mmapBase = NULL;
		opt.mmapSize = 0;
		area = sceKernelAllocUncacheHeapMemoryWithOption(sizeof(*area), &opt);
		if (area == NULL)
			goto nomem;
		request->area = area;
		ret = sceKernelVAtoPA(request->area, &request->areaPA);
		if (ret < 0)
			goto fail;
		request->mailbox = &area->mailbox;
		ret = sceKernelVAtoPA(request->mailbox, &request->mailboxPA);
		if (ret < 0)
			goto fail;
		SmschedLinkAddTail(&instance->freeList, &request->link);
	}

	ret = smsSecureKernelProxySetInstance(s_instance);
	if (ret < 0)
		goto fail;
	ret = smsSecureKernelProxyStartSecureKernel(skAddr, skSize);
	if (ret < 0)
		goto fail;
	if (rvkSize != 0)
		smsSecureKernelProxyInstallRevokeList(rvkAddr, rvkSize);
	ret = smsSecureKernelProxySetDebug(s_instance->debugArea, SMSCHED_DEBUG_AREA_SIZE);
	if (ret < 0)
		goto fail;
	sceKernelSysrootSetModulePrivate(13, s_instance);

	s_runQueue[0].next = &s_runQueue[0];
	s_runQueue[0].prev = &s_runQueue[0];
	s_runQueue[1].next = &s_runQueue[1];
	s_runQueue[1].prev = &s_runQueue[1];
	s_lock = 0;
	s_running = NULL;
	s_doneList.next = &s_doneList;
	s_doneList.prev = &s_doneList;
	ret = sceKernelRegisterIntrHandler(SMSCHED_INTR_CRY2ARM0, "SceSblSmSchedCry2Arm0", 0,
					   cry2Arm0IntHandler, NULL, 128, 8, NULL);
	if (ret < 0)
		goto fail;
	ret = sceKernelEnableIntr(SMSCHED_INTR_CRY2ARM0);
	if (ret < 0)
		goto fail;
	for (i = SMSCHED_INTR_CRY2ARM0 + 1; i < SMSCHED_INTR_CRY2ARM0 + CMEP_MAILBOXES; i++) {
		ret = sceKernelRegisterIntrHandler(i, "SceSblSmSchedCry2Arm123", 0,
						   cry2Arm123IntHandler, NULL, 128, 8, NULL);
		if (ret < 0)
			goto fail;
	}
	for (i = 0; i < (int)(sizeof(s_monitorCalls) / sizeof(s_monitorCalls[0])); i++) {
		ret = sceKernelRegisterMonitorCallHandler(s_monitorCalls[i].callId,
							  s_monitorCalls[i].handler);
		if (ret < 0)
			goto fail;
	}
	s_initialized = 1;
	smsSecureKernelProxyReady();
	return ret;

nomem:
	ret = SCE_SBL_ERROR_NO_MEMORY;
fail:
	sceSblSmSchedFini(0);
	return ret;
}

int sceSblSmSchedInitForTZS(void)
{
	return 0;
}

int sceSblSmSchedStopForTZS(void)
{
	if (sceKernelCpuId() == 0) {
		deleteAllRequest(0);
		sceSblSmSchedFini(0);
	}
	return 0;
}

/* a request's exit waited for, once: its status taken and the request freed */
int sceSblSmSchedWait(SceUInt32 id, SmschedIntrStatus *status)
{
	SmschedRequest *request;
	SceUInt32 state;
	int intr;

	if (s_initialized != 1)
		return SCE_SBL_ERROR_NOT_READY;
	if (status == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	request = SmschedGetRequest(id);
	if (request == NULL)
		return SCE_SBL_ERROR_NO_REQUEST;
	if (atomicCompareAndSet8(&request->waitFlag, 0, 1) != 0)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	state = request->status.state;
	switch (state) {
	case SMSCHED_STATE_READY:
	case SMSCHED_STATE_RUNNING:
	case SMSCHED_STATE_STARTING:
	case SMSCHED_STATE_SUSPENDING:
	case SMSCHED_STATE_STOPPING:
	case SMSCHED_STATE_KILLING:
	case SMSCHED_STATE_EXITING:
		do {
			waitStateChange(state, &intr, request, &state);
		} while (state == SMSCHED_STATE_SUSPENDING || state == SMSCHED_STATE_READY ||
			 state == SMSCHED_STATE_STARTING || state == SMSCHED_STATE_RUNNING ||
			 state == SMSCHED_STATE_KILLING || state == SMSCHED_STATE_STOPPING ||
			 state == SMSCHED_STATE_EXITING);
		if (state != SMSCHED_STATE_EXITED && state != SMSCHED_STATE_KILLED &&
		    state != SMSCHED_STATE_STOPPED) {
			SCE_PANIC(289);
			return SCE_SBL_ERROR_NOT_READY;
		}
		/* fall through */
	case SMSCHED_STATE_EXITED:
	case SMSCHED_STATE_KILLED:
	case SMSCHED_STATE_STOPPED:
		SmschedLinkRemove(&request->link);
		*status = request->status;
		deleteRequest(id);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
		return 0;
	default:
		SCE_PANIC(305);
	}
}

int sceSblSmSchedWaitMonitorCall(SceUInt32 id, unsigned int index)
{
	SceSize areaSize;
	SmschedIntrStatus status;
	SmschedIntrStatus *area;
	int ret, err;

	ret = sceSblTzsBridgePopArgArea(index, (void **)&area, &areaSize);
	if (ret < 0)
		return ret;
	if (areaSize < sizeof(*area)) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_FAIL(923);
#endif
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	ret = sceSblSmSchedWait(id, &status);
	*area = status;
	err = sceSblTzsBridgePushArgArea(index);
	if (err < 0)
		return err;
	return ret;
}

/* a request's result and state */
int sceSblSmSchedGetStatus(SceUInt32 id, SmschedIntrStatus *status)
{
	SmschedRequest *request;
	int intr, ret;

	if (s_initialized != 1)
		return SCE_SBL_ERROR_NOT_READY;
	if (status == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	request = SmschedGetRequest(id);
	if (request == NULL) {
		ret = SCE_SBL_ERROR_NO_REQUEST;
	} else {
		*status = request->status;
		ret = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return ret;
}

int sceSblSmSchedGetStatusMonitorCall(SceUInt32 id, unsigned int index)
{
	SceSize areaSize;
	SmschedIntrStatus status;
	SmschedIntrStatus *area;
	int ret, err;

	ret = sceSblTzsBridgePopArgArea(index, (void **)&area, &areaSize);
	if (ret < 0)
		return ret;
	if (areaSize < sizeof(*area)) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_FAIL(963);
#endif
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	ret = sceSblSmSchedGetStatus(id, &status);
	*area = status;
	err = sceSblTzsBridgePushArgArea(index);
	if (err < 0)
		return err;
	return ret;
}

/*
 * A request killed: soft-killed on the Cmep while it runs, moved to the
 * done list while it waits. A module being started or suspended is waited
 * for. The Non-secure kernel's request is told it is done.
 */
int sceSblSmSchedKill(SceUInt32 id)
{
	volatile SmschedIntrMessage message;
	SmschedIntrHandler handler;
	SmschedRequest *request;
	SceUInt32 state;
	void *arg;
	int intr, ret;

	if (s_initialized != 1)
		return SCE_SBL_ERROR_NOT_READY;
	message.channel = 0;
	message.id = -1;
	message.flags = 0;
	message.eventBits = 0;
	message.ack = 0;
	message.status.result = 0;
	message.status.state = 0;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	request = SmschedGetRequest(id);
	if (request == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
		return SCE_SBL_ERROR_NO_REQUEST;
	}
	sceKernelSpinlockLowLock(&request->lock);
	state = request->status.state;
	switch (state) {
	case SMSCHED_STATE_STARTING:
		waitStateChange(SMSCHED_STATE_STARTING, &intr, request, &state);
		if (state != SMSCHED_STATE_RUNNING)
			goto illegal;
		/* fall through */
	case SMSCHED_STATE_RUNNING:
		request->status.state = SMSCHED_STATE_SOFT_KILLING;
		ret = smsSecureKernelProxySoftKillSecureModule();
		if (ret < 0)
			goto unlock;
		handler = NULL;
		arg = NULL;
		break;
	case SMSCHED_STATE_SUSPENDING:
		waitStateChange(SMSCHED_STATE_SUSPENDING, &intr, request, &state);
		if (state != SMSCHED_STATE_READY)
			goto illegal;
		/* fall through */
	case SMSCHED_STATE_READY:
		ret = 0;
		request->status.state = SMSCHED_STATE_KILLED;
		handler = request->intr[0].handler;
		arg = request->intr[0].arg;
		SmschedLinkRemove(&request->link);
		SmschedLinkAddTail(&s_doneList, &request->link);
		message.id = request->proxyId;
		message.flags |= MSG_CALL_HANDLER;
		/* as Sony's: 0x104 and 3, not the request's own status */
		message.status.result = 0x104;
		message.status.state = 3;
		break;
	default:
		handler = NULL;
		arg = NULL;
		ret = SCE_SBL_ERROR_ILLEGAL_STATE;
		break;
	}
	if (message.id != -1) {
		ret = sendIntrMessage((const SmschedIntrMessage *)&message);
		if (ret < 0)
			SCE_PANIC(496);
	}
	sceKernelSpinlockLowUnlock(&request->lock);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	if (handler != NULL)
		handler(message.id, message.flags, arg, message.status.state,
			message.status.result);
	return ret;

illegal:
	ret = SCE_SBL_ERROR_ILLEGAL_STATE;
unlock:
	sceKernelSpinlockLowUnlock(&request->lock);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return ret;
}

int sceSblSmSchedKillMonitorCall(SceUInt32 id)
{
	return sceSblSmSchedKill(id);
}
