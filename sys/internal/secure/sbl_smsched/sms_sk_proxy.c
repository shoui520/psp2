/*
 * sms_sk_proxy.c: the secure kernel's side of the scheduler. Every command
 * goes through the Cmep mailbox: its arguments in the instance's sk cmd
 * area (whose physical address StartSecureKernel gave the Cmep), the
 * command word in arm2Cry[0], which the Cmep clears when it has taken it.
 * A few commands then wait for the secure kernel's reply in cry2Arm[0].
 *
 * Names: smsSecureKernelProxyStartSecureKernel and InstallRevokeList are
 * Sony's (0.990 prototype); the others follow the messages smsched.c
 * prints for them ("RunSecureModule", "SetDebug", ...), or, for the
 * interrupt commands, what calls them.
 */
#include "smsched.h"

/* the secure kernel's commands (arm2Cry[0] bits 8-15) */
#define SK_COMMAND_STOP				1
#define SK_COMMAND_RUN				2
#define SK_COMMAND_RESUME			3
#define SK_COMMAND_SUSPEND			4
#define SK_COMMAND_SOFT_KILL			5
#define SK_COMMAND_HARD_KILL			6
#define SK_COMMAND_SET_DEBUG			9
#define SK_COMMAND_INSTALL_REVOKE_LIST		10
#define SK_COMMAND_GENERATE_INTR0		11	/* 11-15: sceSblSmSchedGenerateMonitorIntr 0-4 */
#define SK_COMMAND_READY			16

/* cry2Arm[0] while the Cmep boots: bit 8 set once it waits, bits 9-10 what for */
#define CMEP_BOOT_WAITING			0x100
#define CMEP_BOOT_STATE_MASK			0x600
#define CMEP_BOOT_STATE_ACKNOWLEDGE		0x200	/* a message to acknowledge first */
#define CMEP_BOOT_STATE_ERROR			0x400
#define CMEP_BOOT_STATE_KERNEL			0x600	/* it waits for the secure kernel */
/* its reply to the secure kernel's address */
#define CMEP_BOOT_KERNEL_ACCEPTED		1
#define CMEP_BOOT_KERNEL_REJECTED		2

/* the arguments of each command, in the sk cmd area; the Cmep's words are
 * only word aligned */
typedef SceUInt64 SkUInt64 __attribute__((aligned(4)));

typedef struct SkRunArg {
	SceUInt32 arg1;
	uintptr_t selfPA;
	uintptr_t areaPA;
	SkUInt64 arg2;
	SceUInt32 arg3;
	SceUInt32 arg4;
	SceUInt32 reserved;
	SmschedInvokeArg invokeArg;
} SkRunArg;

typedef struct SkContextArg {
	SceUInt32 vectorCount;
	uintptr_t vectorPA;
	uintptr_t areaPA;
	uintptr_t mailboxPA;
} SkContextArg;

typedef struct SkAreaArg {
	uintptr_t pa;
	SceSize size;
} SkAreaArg;

typedef struct SkRevokeListArg {
	SceUInt32 count;			/* lists that follow (one) */
	uintptr_t listPA;			/* an SkAreaArg */
} SkRevokeListArg;

_Static_assert(sizeof(SkRunArg) == 0x50, "SkRunArg");

static SmschedInstance *s_proxyInstance;

static inline void SkDmb(void)
{
	__asm__ volatile("dmb sy" : : : "memory");
}

static inline void SkDsb(void)
{
	__asm__ volatile("dsb sy" : : : "memory");
}

/* write an argument area out to the Cmep: a barrier, then a read of its
 * last byte, which completes when the writes before it have */
static inline void SkFlushArea(const void *area, SceSize size)
{
	const volatile uint8_t *volatile last = (const uint8_t *)area + size - 1;

	SkDmb();
	(void)*last;
	SkDsb();
}

static inline void SkSendCommand(SceUInt32 command)
{
	CMEP_MAILBOX_REG(arm2Cry[0]) = command;
	while (CMEP_MAILBOX_REG(arm2Cry[0]) != 0)
		;
}

/* the secure kernel's reply, acknowledged */
static inline SceUInt32 SkWaitReply(void)
{
	SceUInt32 reply;

	do
		reply = CMEP_MAILBOX_REG(cry2Arm[0]);
	while ((reply & CMEP_MESSAGE_MASK) == 0);
	CMEP_MAILBOX_REG(cry2Arm[0]) = reply & CMEP_MESSAGE_MASK;
	return reply;
}

/* a reply as the command's result: the message, or an error */
static inline int SkReplyResult(SceUInt32 reply)
{
	if (reply & CMEP_MESSAGE_ERROR)
		return SCE_SBL_ERROR_CMEP(reply);
	return reply & CMEP_MESSAGE_MASK;
}

static inline void SkWaitMessage(SceUInt32 message)
{
	while ((CMEP_MAILBOX_REG(cry2Arm[0]) & CMEP_MESSAGE_MASK) != message)
		;
	CMEP_MAILBOX_REG(cry2Arm[0]) = message;
}

int smsSecureKernelProxySetInstance(SmschedInstance *instance)
{
	s_proxyInstance = instance;
	return 0;
}

/* Start the secure kernel at enpPA once the Cmep's second loader asks for
 * it, then give it the sk cmd area. */
int smsSecureKernelProxyStartSecureKernel(uintptr_t enpPA, SceSize enpSize)
{
	uintptr_t cmdAreaPA;
	SceUInt32 status, reply;
	int ret;

	do
		status = CMEP_MAILBOX_REG(cry2Arm[0]);
	while (!(status & CMEP_BOOT_WAITING));
	status &= CMEP_BOOT_STATE_MASK;
	if (status == CMEP_BOOT_STATE_ERROR) {
		return SCE_SBL_ERROR_SECURE_KERNEL_START;
	} else if (status == CMEP_BOOT_STATE_ACKNOWLEDGE) {
		CMEP_MAILBOX_REG(cry2Arm[0]) = CMEP_MAILBOX_REG(cry2Arm[0]);
	} else if (status == CMEP_BOOT_STATE_KERNEL) {
		CMEP_CONTROLLER_REG(reset) = CMEP_RESET;
		CMEP_CONTROLLER_REG(reset) = 0;
		while (CMEP_CONTROLLER_REG(reset) != 0)
			;
		while (!(CMEP_CONTROLLER_REG(sboot) & CMEP_SBOOT_ALIVE))
			;
		CMEP_MAILBOX_REG(arm2Cry[0]) = (enpPA & ~3) | CMEP_COMMAND_VALID;
		for (;;) {
			reply = CMEP_MAILBOX_REG(cry2Arm[0]);
			if (reply == CMEP_BOOT_KERNEL_REJECTED)
				return SCE_SBL_ERROR_SECURE_KERNEL_START;
			if (reply == CMEP_BOOT_KERNEL_ACCEPTED)
				break;
		}
		CMEP_MAILBOX_REG(cry2Arm[0]) = reply;
	}

	SkWaitMessage(CMEP_MESSAGE_MAIN_INIT);
	ret = sceKernelVAtoPA(s_proxyInstance->skCmdArea, &cmdAreaPA);
	if (ret < 0)
		return ret;
	CMEP_MAILBOX_REG(arm2Cry[0]) = cmdAreaPA & ~3;
	SkSendCommand(CMEP_COMMAND_VALID);
	SkWaitMessage(CMEP_MESSAGE_SM_READY);
	return 0;
}

int smsSecureKernelProxyStopSecureKernel(void)
{
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_STOP, 0));
	return 0;
}

int smsSecureKernelProxyInstallRevokeList(uintptr_t listPA, SceSize listSize)
{
	SceAllocOpt opt;
	SkRevokeListArg *cmd;
	SkAreaArg *list;
	SceUInt32 reply;
	uintptr_t pa;
	int ret;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 64;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	list = sceKernelAllocUncacheHeapMemoryWithOption(sizeof(*list), &opt);
	if (list == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;
	list->pa = listPA;
	list->size = listSize;
	SkFlushArea(list, sizeof(*list));
	ret = sceKernelVAtoPA(list, &pa);
	if (ret < 0) {
		sceKernelFreeUncacheHeapMemory(list);
		return ret;
	}
	cmd = s_proxyInstance->skCmdArea;
	cmd->count = 1;
	cmd->listPA = pa;
	SkFlushArea(cmd, sizeof(*cmd));
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_INSTALL_REVOKE_LIST, sizeof(*cmd)));
	reply = SkWaitReply();
	sceKernelFreeUncacheHeapMemory(list);
	return SkReplyResult(reply);
}

int smsSecureKernelProxyRunSecureModule(uintptr_t selfPA, SceUInt32 arg1, SceUInt64 arg2,
					SceUInt32 arg3, SceUInt32 arg4,
					const SmschedInvokeArg *invokeArg, uintptr_t areaPA)
{
	SkRunArg *cmd = s_proxyInstance->skCmdArea;

	cmd->arg2 = arg2;
	cmd->arg1 = arg1;
	cmd->selfPA = selfPA;
	cmd->areaPA = areaPA;
	cmd->arg3 = arg3;
	cmd->arg4 = arg4;
	cmd->invokeArg = *invokeArg;
	SkFlushArea(cmd, sizeof(*cmd));
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_RUN, sizeof(*cmd)));
	return 0;
}

int smsSecureKernelProxySuspendSecureModule(uintptr_t vectorPA, SceUInt32 vectorCount,
					    uintptr_t areaPA, uintptr_t mailboxPA)
{
	SkContextArg *cmd = s_proxyInstance->skCmdArea;

	cmd->vectorCount = vectorCount;
	cmd->vectorPA = vectorPA;
	cmd->areaPA = areaPA;
	cmd->mailboxPA = mailboxPA;
	SkFlushArea(cmd, sizeof(*cmd));
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_SUSPEND, sizeof(*cmd)));
	return 0;
}

int smsSecureKernelProxyResumeSecureModule(uintptr_t vectorPA, SceUInt32 vectorCount,
					   uintptr_t areaPA, uintptr_t mailboxPA)
{
	SkContextArg *cmd = s_proxyInstance->skCmdArea;

	cmd->vectorCount = vectorCount;
	cmd->vectorPA = vectorPA;
	cmd->areaPA = areaPA;
	cmd->mailboxPA = mailboxPA;
	SkFlushArea(cmd, sizeof(*cmd));
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_RESUME, sizeof(*cmd)));
	return 0;
}

/* Stop the running module, acknowledging the events it raises meanwhile,
 * until the secure kernel says it is shut down and ready again. */
int smsSecureKernelProxyHardKillSecureModule(void)
{
	SceUInt32 events;

	CMEP_MAILBOX_REG(arm2Cry[0]) = CMEP_COMMAND(SK_COMMAND_HARD_KILL, 0);
	do {
		events = CMEP_MAILBOX_REG(cry2Arm[0]) & CMEP_EVENT_MASK;
		if (events != 0)
			CMEP_MAILBOX_REG(cry2Arm[0]) = events;
	} while (CMEP_MAILBOX_REG(arm2Cry[0]) != 0);
	do {
		events = CMEP_MAILBOX_REG(cry2Arm[0]) & CMEP_EVENT_MASK;
		if (events != 0)
			CMEP_MAILBOX_REG(cry2Arm[0]) = events;
	} while (CMEP_MAILBOX_REG(cry2Arm[0]) != CMEP_MESSAGE_SM_SHUT_DOWN);
	CMEP_MAILBOX_REG(cry2Arm[0]) = CMEP_MESSAGE_SM_SHUT_DOWN;
	while (CMEP_MAILBOX_REG(cry2Arm[0]) != CMEP_MESSAGE_SM_READY)
		;
	CMEP_MAILBOX_REG(cry2Arm[0]) = CMEP_MESSAGE_SM_READY;
	return 0;
}

int smsSecureKernelProxySoftKillSecureModule(void)
{
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_SOFT_KILL, 0));
	return 0;
}

int smsSecureKernelProxySetDebug(const void *area, SceSize size)
{
	SkAreaArg *cmd;
	uintptr_t pa;
	int ret;

	ret = sceKernelVAtoPA(area, &pa);
	if (ret < 0)
		return ret;
	cmd = s_proxyInstance->skCmdArea;
	cmd->pa = pa;
	cmd->size = size;
	SkFlushArea(cmd, sizeof(*cmd));
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_SET_DEBUG, sizeof(*cmd)));
	return SkReplyResult(SkWaitReply());
}

int smsSecureKernelProxyGenerateIntr0(void)
{
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_GENERATE_INTR0, 0));
	return 0;
}

int smsSecureKernelProxyGenerateIntr1(void)
{
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_GENERATE_INTR0 + 1, 0));
	return 0;
}

int smsSecureKernelProxyGenerateIntr2(void)
{
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_GENERATE_INTR0 + 2, 0));
	return 0;
}

int smsSecureKernelProxyGenerateIntr3(void)
{
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_GENERATE_INTR0 + 3, 0));
	return 0;
}

int smsSecureKernelProxyGenerateIntr4(void)
{
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_GENERATE_INTR0 + 4, 0));
	return 0;
}

int smsSecureKernelProxyReady(void)
{
	SkSendCommand(CMEP_COMMAND(SK_COMMAND_READY, 0));
	return 0;
}
