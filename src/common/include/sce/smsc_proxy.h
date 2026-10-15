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

/* eventBits: the session's event flag */
#define SMSCHED_EVENT_DONE	0x1	/* the module exited or was killed */
#define SMSCHED_EVENT_STATE	0x2	/* its state changed */

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

/* the secure module start arguments the Non-secure kernel passes, from
 * its SceSblSmCommContext130 (names from the henkaku wiki) */
typedef struct SmschedInvokeArg {
	SceUInt32 selfType;
	SceUInt32 mediaType;
	SceUInt64 programAuthorityId;		/* the spawner's */
	SceUInt64 capability[4];		/* the spawner's */
} SmschedInvokeArg;

_Static_assert(sizeof(SmschedInvokeArg) == 48, "SmschedInvokeArg");

/* selfType: where the secure module comes from */
#define SMSCHED_INVOKE_SELF_MASK		0xf000
#define SMSCHED_INVOKE_SELF_PA			0x0000	/* the caller's, at selfPA */
#define SMSCHED_INVOKE_SELF_INSTANCE		0x1000	/* the scheduler's own: kprx_auth_sm */

/* the argument area of SMC 0x12D (sceSblSmSchedInvokeMonitorCall) */
typedef struct SceSblSmSchedProxyInvokeArg {
	SceUInt32 arg2Low;			/* the invoke input words */
	SceUInt32 arg2High;
	SceUInt32 arg3;
	SceUInt32 arg4;
	SceUID proxyId;				/* the session's id */
	SceUInt32 requestId;			/* out: the scheduler's request */
	SmschedInvokeArg invokeArg;
} SceSblSmSchedProxyInvokeArg;

_Static_assert(sizeof(SceSblSmSchedProxyInvokeArg) == 0x48, "SceSblSmSchedProxyInvokeArg");

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
