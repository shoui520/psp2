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

/* PARTIAL: not static until the functions that set them are written */
SmschedRequest *s_running;
int s_initialized;
SmschedLink s_runQueue[2];		/* by priority, 0 first */
SceKernelSpinlock s_lock;
SmschedInstance *s_instance;

/* a request by its id, or NULL */
static inline SmschedRequest *SmschedGetRequest(SceUInt32 id)
{
	if (id >= SMSCHED_REQUESTS)
		return NULL;
	return &s_instance->requests[id];
}

static inline void SmschedDmb(void)
{
	__asm__ volatile("dmb sy" : : : "memory");
}

static inline void SmschedDsb(void)
{
	__asm__ volatile("dsb sy" : : : "memory");
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
	__asm__ volatile("dsb sy\n\tsev" : : : "memory");
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
			SmschedDmb();
			(void)request->mailbox->cry2Arm[0];
			SmschedDsb();
			return 0;
		} else if (mailbox == 2) {
			request->mailbox->cry2Arm[1] &= ~(bits & request->mailbox->cry2Arm[1]);
			SmschedDmb();
			(void)request->mailbox->cry2Arm[1];
			SmschedDsb();
			return 0;
		} else if (mailbox == 3) {
			request->mailbox->cry2Arm[2] &= ~(bits & request->mailbox->cry2Arm[2]);
			SmschedDmb();
			(void)request->mailbox->cry2Arm[2];
			SmschedDsb();
			return 0;
		}
	} else {
		if (mailbox == 1) {
			CMEP_MAILBOX_REG(cry2Arm[1]) = bits;
			SmschedDmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[1]);
			SmschedDsb();
			return 0;
		} else if (mailbox == 2) {
			CMEP_MAILBOX_REG(cry2Arm[2]) = bits;
			SmschedDmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[2]);
			SmschedDsb();
			return 0;
		} else if (mailbox == 3) {
			CMEP_MAILBOX_REG(cry2Arm[3]) = bits;
			SmschedDmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[3]);
			SmschedDsb();
			return 0;
		}
	}
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
			SmschedDmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[1]);
			SmschedDsb();
			return 0;
		} else if (mailbox == 2) {
			*value = CMEP_MAILBOX_REG(cry2Arm[2]);
			SmschedDmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[2]);
			SmschedDsb();
			return 0;
		} else if (mailbox == 3) {
			*value = CMEP_MAILBOX_REG(cry2Arm[3]);
			SmschedDmb();
			(void)CMEP_MAILBOX_REG(cry2Arm[3]);
			SmschedDsb();
			return 0;
		}
	}
	return SCE_SBL_ERROR_INVALID_ARGUMENT;
}
