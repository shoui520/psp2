/*
 * The SBC task (0x0004241c..0x00042488), an init table entry: the handlers
 * the ROM's task loop calls for its events, then the task itself, on a stack
 * painted 0xee. The filename and the names are inferred; the function names
 * are the firmware's placeholders.
 */
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sbc.h"

#define SBC_TASK_STACK_SIZE	1500
#define SBC_TASK_PRIORITY	4
#define SBC_TASK_STACK_FILL	0xee

typedef int (*SbcTaskHandler)(uint32_t events, uintptr_t argument);

/* What the ROM's SBC task reads (SQRAM 0xc0033000, ROM data). */
struct SbcTaskHandlers {
	SbcTaskHandler handlers[12];	/* by event */
	uint32_t state[3];		/* cleared again after the handlers */
};
_Static_assert(sizeof(struct SbcTaskHandlers) == 60, "SBC task handlers");

/* The events the handlers are installed for. */
enum {
	SBC_TASK_ENCODE		= 0,
	SBC_TASK_CONFIGURE	= 1,
	SBC_TASK_DECODE		= 2,
	SBC_TASK_ENCODE_NEXT	= 6,
	SBC_TASK_INSPECT	= 8,
	SBC_TASK_SYNC		= 11,
};

extern struct SbcTaskHandlers sbcTaskHandlers;					/* SQRAM 0xc0033000 */

static uint8_t sbcTaskStack[SBC_TASK_STACK_SIZE] __attribute__((section(".itcm.bss"), aligned(8)));	/* 0x5e8b0 */

int btSbcInit(void)
{
	__aeabi_memclr4(&sbcTaskHandlers, sizeof(sbcTaskHandlers));
	sbcTaskHandlers.handlers[SBC_TASK_CONFIGURE] = btSbcEncoderConfigure;
	sbcTaskHandlers.handlers[SBC_TASK_ENCODE] = btSbcEncoderProcess;
	sbcTaskHandlers.handlers[SBC_TASK_DECODE] = (SbcTaskHandler)btSbcDecoderProcess;
	sbcTaskHandlers.handlers[SBC_TASK_ENCODE_NEXT] = btSbcEncoderProcess;
	sbcTaskHandlers.handlers[SBC_TASK_INSPECT] = btSbcInspectState;
	sbcTaskHandlers.handlers[SBC_TASK_SYNC] = (SbcTaskHandler)btSbcSyncState;
	sbcTaskHandlers.state[0] = 0;
	sbcTaskHandlers.state[1] = 0;
	sbcTaskHandlers.state[2] = 0;
	btSbcManageState();
	__aeabi_memset4(sbcTaskStack, SBC_TASK_STACK_SIZE, SBC_TASK_STACK_FILL);
	return btSbcRomProcessState(sbcTaskStack, SBC_TASK_STACK_SIZE, SBC_TASK_PRIORITY,
				    btSbcRomHandleState);
}
