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

static inline void SmschedDmb(void)
{
	__asm__ volatile("dmb sy" : : : "memory");
}

static inline void SmschedDsb(void)
{
	__asm__ volatile("dsb sy" : : : "memory");
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
