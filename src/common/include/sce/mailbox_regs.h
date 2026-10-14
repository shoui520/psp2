#pragma once
#include <stdint.h>
#include <stddef.h>

/* Lowio maps this peripheral at physical 0xe3106000. Names descriptive. */
typedef struct ScePervasiveMailboxRegisters {
	uint32_t reservedControl[4];
	volatile uint32_t receive[16];
	volatile uint32_t debugReceive[2];
	uint32_t reservedReceive[30];
	volatile uint32_t debugAcknowledge[2];
	uint32_t reservedAcknowledge[14];
	volatile uint32_t send[16];
	volatile uint32_t debugSend[2];
	uint32_t reservedSendControl[30];
	/* Generic debug writes accept these registers; their meanings are unknown. */
	volatile uint32_t unidentifiedDebugWriteControl[2];
	uint32_t reservedEnd[906];
} ScePervasiveMailboxRegisters;

_Static_assert(offsetof(ScePervasiveMailboxRegisters, receive) == 0x10,
	"mailbox receive registers");
_Static_assert(offsetof(ScePervasiveMailboxRegisters, send) == 0x110,
	"mailbox send registers");
_Static_assert(offsetof(ScePervasiveMailboxRegisters, debugReceive) == 0x50,
	"debug mailbox receive registers");
_Static_assert(offsetof(ScePervasiveMailboxRegisters, debugAcknowledge) == 0xd0,
	"debug mailbox acknowledge registers");
_Static_assert(offsetof(ScePervasiveMailboxRegisters, debugSend) == 0x150,
	"debug mailbox send registers");
_Static_assert(offsetof(ScePervasiveMailboxRegisters, unidentifiedDebugWriteControl) == 0x1d0,
	"unidentified debug mailbox write control registers");
_Static_assert(sizeof(ScePervasiveMailboxRegisters) == 0x1000,
	"mailbox register page");
