/*
 * SKBL's PervasiveInit (SceLowio's pervasive_init.c): no mappings, the
 * register pointers are the physical addresses. The hardware information is
 * read as SceDriverTzs reads it.
 */
#include <stdint.h>
#include <lowio/pervasive_regs.h>
#include <lowio/mailbox_regs.h>
#include <lowio/uart_regs.h>
#include <lowio/pervasive.h>
#include "pervasive_kbl.h"

/* SceLowio's register pointers (as SceDriverTzs declares them); pervasive_kbl.c
 * reaches the registers through the physical addresses instead */
ScePervasiveMailboxRegisters *s_mailbox;
volatile SceUartClkgenRegisters *s_uartClock;
ScePervasiveVidRegisters *s_video;
ScePervasiveMiscRegisters *s_misc;
ScePervasiveResetRegisters *s_reset;
ScePervasive2Registers *s_pervasive2;
ScePervasiveGateRegisters *s_gate;
ScePervasiveBaseClkRegisters *s_baseClock;

/* what PervasiveInit read, as in SceDriverTzs */
static struct {
	uint8_t *tas[8];
	volatile uint32_t hardwareInfo[5];		/* revision, cacheStatus, hardwareInfo[3] */
} s_pervasive;


int PervasiveInit(void)
{
	unsigned int i;

	s_misc = (ScePervasiveMiscRegisters *)PERVASIVE_MISC_BASE;
	s_reset = (ScePervasiveResetRegisters *)PERVASIVE_RESET_BASE;
	s_gate = (ScePervasiveGateRegisters *)PERVASIVE_GATE_BASE;
	s_baseClock = (ScePervasiveBaseClkRegisters *)PERVASIVE_BASECLK_BASE;
	s_video = (ScePervasiveVidRegisters *)PERVASIVE_VID_BASE;
	s_mailbox = (ScePervasiveMailboxRegisters *)PERVASIVE_MAILBOX_BASE;
	for (i = 0; i < 8; i++)
		s_pervasive.tas[i] = (uint8_t *)PERVASIVE_TAS_BASE(i);
	s_pervasive2 = (ScePervasive2Registers *)PERVASIVE2_BASE;
	s_uartClock = (volatile SceUartClkgenRegisters *)UART_CLKGEN_BASE;
	s_pervasive.hardwareInfo[0] = s_misc->revision;
	s_pervasive.hardwareInfo[1] = s_misc->cacheStatus;
	for (i = 0; i < 3; i++)
		s_pervasive.hardwareInfo[i + 2] = s_misc->hardwareInfo[i];
	LowioPixelClockInit();
	PervasiveMiscInit();
	ScePervasiveForDriver_6AF0196F(3, 3);
	return 0;
}
