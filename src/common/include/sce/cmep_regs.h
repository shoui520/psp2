/*
 * The Cmep (the MeP security processor) as the ARM side sees it: the
 * mailbox at physical 0xe0000000 and the controller at 0xe0010000. Names
 * follow the henkaku wiki (Cmep registers) and Sony's (SceSblSmsched:
 * cry2Arm0IntHandler, sceSblSmSchedReadArm2CryMonitorCall, Mbox123).
 *
 * Word 0 of each direction carries the protocol: Cmep messages in
 * cry2Arm[0] (interrupt 200, Cry2Arm0), commands in arm2Cry[0]. Words 1-3
 * are free-form mailboxes for the running secure module (interrupts
 * 201-203, Cry2Arm123).
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define CMEP_MAILBOX_BASE		0xe0000000
#define CMEP_CONTROLLER_BASE		0xe0010000

#define CMEP_MAILBOXES			4	/* 0 the protocol, 1-3 the module's */

/* arm2Cry[0]: a command; the Cmep clears the word when it has taken it */
#define CMEP_COMMAND_VALID		0x1
#define CMEP_COMMAND(id, size)		((size) << 16 | (id) << 8 | CMEP_COMMAND_VALID)

/* cry2Arm[0]: a message in the low half, events in the high half; written
 * back to acknowledge */
#define CMEP_MESSAGE_MASK		0xffff
#define CMEP_EVENT_MASK			0xffff0000
#define CMEP_MESSAGE_ERROR		0x8000	/* low byte: the error */
#define CMEP_MESSAGE_SUCCESS		0x1
#define CMEP_MESSAGE_MAIN_INIT		0x101	/* the secure kernel started */
#define CMEP_MESSAGE_SM_READY		0x102	/* a module can be loaded or resumed */
#define CMEP_MESSAGE_SM_RESUMED		0x103
#define CMEP_MESSAGE_SM_SHUT_DOWN	0x104

typedef struct SceCmepMailboxRegisters {
	/* 0x00: the Cmep's words; a write clears the bits written */
	volatile uint32_t cry2Arm[CMEP_MAILBOXES];
	/* 0x10: the ARM's words; a write to 1-3 sets the bits written */
	volatile uint32_t arm2Cry[CMEP_MAILBOXES];
	volatile uint32_t cry2Debugger[2];		/* 0x20 */
	volatile uint32_t debugger2Cry[2];		/* 0x28 */
	uint32_t reserved30[9];
	/* 0x54: a write clears the bits written in arm2Cry[1-3] */
	volatile uint32_t arm2CryClear[CMEP_MAILBOXES - 1];
	volatile uint32_t debugger2Cry2[2];		/* 0x60 */
} SceCmepMailboxRegisters;

/* reset: bit 0 holds the Cmep in reset */
#define CMEP_RESET			0x1
/* sboot: bit 31 set once the Cmep runs */
#define CMEP_SBOOT_ALIVE		0x80000000

typedef struct SceCmepControllerRegisters {
	volatile uint32_t reset;			/* 0x00 */
	volatile uint32_t sboot;			/* 0x04: crySboot, read-only */
	volatile uint32_t resetPulse;			/* 0x08: a write resets the Cmep */
	volatile uint32_t bigmacReset;			/* 0x0c: bit 0 Bigmac, bit 1 Bignum */
	volatile uint32_t unknownReset;			/* 0x10: Kermit 1.0 ES3.2 and later */
} SceCmepControllerRegisters;

/* one register at its own physical address, as Sony's code addresses them:
 * CMEP_MAILBOX_REG(arm2Cry[0]) */
#define CMEP_MAILBOX_REG(reg)						\
	(*(volatile uint32_t *)(CMEP_MAILBOX_BASE + offsetof(SceCmepMailboxRegisters, reg)))
#define CMEP_CONTROLLER_REG(reg)					\
	(*(volatile uint32_t *)(CMEP_CONTROLLER_BASE + offsetof(SceCmepControllerRegisters, reg)))

_Static_assert(offsetof(SceCmepMailboxRegisters, arm2Cry) == 0x10, "arm2Cry");
_Static_assert(offsetof(SceCmepMailboxRegisters, cry2Debugger) == 0x20, "cry2Debugger");
_Static_assert(offsetof(SceCmepMailboxRegisters, arm2CryClear) == 0x54, "arm2CryClear");
_Static_assert(offsetof(SceCmepMailboxRegisters, debugger2Cry2) == 0x60, "debugger2Cry2");
_Static_assert(offsetof(SceCmepControllerRegisters, unknownReset) == 0x10, "unknownReset");
