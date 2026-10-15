/*
 * SceSblSmschedProxy (os0:kd/smsc_proxy.skprx): SceSblSmSchedProxyForKernel.
 *
 * Runs secure-module sessions on the TrustZone side (SMC) and relays the
 * secure scheduler's software interrupts (Cry2Arm) back to ARM callers.
 */

#pragma once

#include <psp2kern/types.h>

/* interrupt message flags */
#define MSG_SET_EVENT		0x1
#define MSG_CALL_HANDLER	0x2
#define MSG_HANDLER_ONLY	0x4

/*
 * The message the secure world's SceSblSmsched pushes into a bridge
 * interrupt area before it raises software interrupt 4.
 */
/* the request's result and state, as the handler gets them */
typedef struct SmschedIntrStatus {
	SceUInt32 result;
	SceUInt32 state;
} SmschedIntrStatus;

typedef struct SmschedIntrMessage {
	SceUInt32 channel;		/* the mailbox, 0-3 */
	SceUID id;			/* the session's id */
	SceUInt32 flags;		/* MSG_* */
	SceUInt32 eventBits;
	SceUInt32 ack;			/* the interrupt area's index */
	SmschedIntrStatus status;
} SmschedIntrMessage;

typedef void (*SceSblSmSchedCry2ArmHandler)(SceUID id, int channel, void *arg,
					     SceUInt32 a, SceUInt32 b);

int ksceSblSmSchedProxyInitialize(void);
int ksceSblSmSchedProxyUninitialize(void);
int sceSblSmSchedProxyInvoke(SceUInt32 a, SceUInt32 b, SceUInt32 c, const SceUInt32 *in,
			     const void *param, SceUID *id);
int sceSblSmSchedProxyWait(SceUID id, SceUInt32 *result);
int sceSblSmSchedProxyGetStatus(SceUID id, SceUInt32 *status);
int ksceSblSmSchedProxyChangeF00DStatus(SceUID id);
int ksceSblSmSchedCallFunc(SceUID id, int channel, SceUInt32 value);
int ksceSblSmSchedProxyGetCommandF00DRegister(SceUID id, int channel, SceUInt32 *value);
int ksceSblSmSchedProxyWriteCry2Arm(SceUID id, int channel, SceUInt32 value);
int ksceSblSmSchedProxyReadCry2Arm(SceUID id, int channel, SceUInt32 *value);
int ksceSblSmSchedProxyEnableCry2ArmInterrupt(SceUID id, int channel,
					      SceSblSmSchedCry2ArmHandler handler, void *arg);
int ksceSblSmSchedProxyDisableCry2ArmInterrupt(SceUID id, int channel);
int ksceSblSmSchedProxyExecuteF00DCommand(SceUInt32 a, SceUInt32 b, SceUInt32 c, SceUInt32 d);

/* names unknown (NID) */
int SceSblSmSchedProxyForKernel_984EC9D1(void);
int SceSblSmSchedProxyForKernel_1DFC8624(void);
int SceSblSmSchedProxyForKernel_3CE17233(SceUID id, int channel, SceUInt32 value);
