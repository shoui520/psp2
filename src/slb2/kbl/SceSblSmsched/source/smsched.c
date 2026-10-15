/*
 * smsched.c: the scheduler. Requests wait in two run queues by priority;
 * one runs on the Cmep at a time. Mailboxes 1-3 belong to the running
 * module: its requests reach the registers, while a module that does not
 * run has its mailboxes saved in its request (SmschedMailbox), which the
 * secure kernel restores when it resumes the module.
 *
 * PARTIAL: only the mailbox operations are written so far.
 */
#include "smsched.h"
#include "sce/barrier.h"

/* PARTIAL: not static until the functions that set them are written */
SmschedRequest *s_running;
int s_initialized;
SmschedLink s_runQueue[2];		/* by priority, 0 first */
SceKernelSpinlock s_lock;
SmschedInstance *s_instance;
SmschedLink s_doneList;			/* exited, killed or stopped */

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
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
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
			if (mailbox != 0 && SmschedStateOnCmep(request->state))
				ret = sceSblSmSchedEnableIntr(mailbox);
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
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
	SmschedRequest *running;
	int intr, mailbox, ret;
	void *arg;

	(void)userCtx;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	running = s_running;
	if (running == NULL) {
		ret = -1;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
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
		ret = running->proxyId;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
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
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	request = SmschedGetRequest(id);
	if (request == NULL) {
		ret = SCE_SBL_ERROR_NO_REQUEST;
	} else {
		request->intr[mailbox].handler = NULL;
		request->intr[mailbox].arg = NULL;
		ret = 0;
		if (mailbox != 0) {
			request->intr[mailbox].enabled = 0;
			if (SmschedStateOnCmep(request->state))
				ret = sceSblSmSchedDisableIntr(mailbox);
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
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
			(void)request->mailbox->cry2Arm[0];
			dsb();
			return 0;
		} else if (mailbox == 2) {
			request->mailbox->cry2Arm[1] &= ~(bits & request->mailbox->cry2Arm[1]);
			dmb();
			(void)request->mailbox->cry2Arm[1];
			dsb();
			return 0;
		} else if (mailbox == 3) {
			request->mailbox->cry2Arm[2] &= ~(bits & request->mailbox->cry2Arm[2]);
			dmb();
			(void)request->mailbox->cry2Arm[2];
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

	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, *intr);
	for (;;) {
		ret = ksceKernelSpinlockLowTrylockCpuSuspendIntr(&s_lock);
		if (ret >= 0) {
			*intr = ret;
			current = request->state;
			if (current != state)
				break;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, ret);
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
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	request = SmschedGetRequest(id);
	if (request == NULL) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
		return SCE_SBL_ERROR_NO_REQUEST;
	}
	ksceKernelSpinlockLowLock(&request->lock);
	state = request->state;
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
	ksceKernelSpinlockLowUnlock(&request->lock);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
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
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	request = SmschedGetRequest(id);
	if (request == NULL) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
		return SCE_SBL_ERROR_NO_REQUEST;
	}
	ksceKernelSpinlockLowLock(&request->lock);
	state = request->state;
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
	ksceKernelSpinlockLowUnlock(&request->lock);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
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
	running->result = result;
	running->state = SMSCHED_STATE_EXITED;
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
		sceKernelFreeUncacheHeapMemoryForTZS(request->context);
		request->context = NULL;
	}
	if (request->vector != NULL) {
		sceKernelFreeUncacheHeapMemoryForTZS(request->vector);
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
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	for (i = 0; i < 2; i++) {
		request = (SmschedRequest *)s_runQueue[i].next;
		while (&request->link != &s_runQueue[i] && request != NULL) {
			if (onlyProxied && request->proxyId == -1)
				continue;
			if (atomicCompareAndSet8(&request->waitFlag, 0, 1) == 0) {
				SmschedLinkRemove(&request->link);
				deleteRequest(request->id);
			} else {
				request->state = SMSCHED_STATE_KILLED;
			}
			request = (SmschedRequest *)s_runQueue[i].next;
		}
	}

	running = s_running;
	if (running == NULL || (onlyProxied && running->proxyId == -1)) {
		ret = 0;
	} else {
		state = running->state;
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
				running->state = SMSCHED_STATE_KILLED;
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
			request->state = SMSCHED_STATE_KILLED;
		}
		request = (SmschedRequest *)s_doneList.next;
	}
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
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
		sceKernelFreeUncacheHeapMemoryForTZS(s_instance->debugArea);
		s_instance->debugArea = NULL;
	}
	if (s_instance->skCmdArea != NULL) {
		sceKernelFreeUncacheHeapMemoryForTZS(s_instance->skCmdArea);
		s_instance->skCmdArea = NULL;
	}
	if (s_instance->self != NULL) {
		sceKernelFreeUncacheHeapMemoryForTZS(s_instance->self);
		s_instance->self = NULL;
	}
	for (i = 0; i < SMSCHED_REQUESTS; i++) {
		request = &s_instance->requests[i];
		if (request->area != NULL) {
			sceKernelFreeUncacheHeapMemoryForTZS(request->area);
			request->area = NULL;
			request->mailbox = NULL;
		}
	}
	if (s_instance != NULL) {
		if (s_instance->requests != NULL) {
			sceKernelFreeHeapMemoryForTZS(s_instance->heap, s_instance->requests);
			s_instance->requests = NULL;
		}
		heap = s_instance->heap;
		sceKernelFreeHeapMemoryForTZS(heap, s_instance);
		s_instance = NULL;
		if (heap >= 0)
			sceKernelDeleteHeapForTZS(heap);
	}
	return ret;
}
