/*
 * Interrupt lines of the host interface: the table of handlers the WLAN init
 * stage installs, and the lines it enables. The filename is inferred.
 */
#include <stddef.h>
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/wlan.h"

struct WlanIrqLine {
	SysIrqHandler handler;
	uint32_t line;
};

/* Handlers not converted yet. */
int cbProcScheduleEntry(int line, uint32_t pending);			/* 0x1c7a0, line 10 */

int wlanHostLineMaskIrq(int line, uint32_t pending);

/* Lines installed by wlanIrqRegisterAll(), terminated by a NULL handler. */
static struct WlanIrqLine wlanIrqTable[] __attribute__((section(".itcm.rw"))) = {
	{ sysOsTimerIrq,	5 },
	{ cbProcScheduleEntry,	10 },
	{ wlanHostLineMaskIrq,	12 },
	{ NULL,			0 },
};

void wlanIrqRegisterAll(void)
{
	unsigned int i;

	for (i = 0; wlanIrqTable[i].handler != NULL; ++i)
		sysIrqRegister(wlanIrqTable[i].line, wlanIrqTable[i].handler);
}

/* Unmasks lines 5, 8 and 22. */
void wlanIrqEnableHostLines(void)
{
	SOC_IRQ_ENABLE_SET = 0x400120;
}

void wlanIrqUnregisterAll(void)
{
	unsigned int i;

	for (i = 0; wlanIrqTable[i].handler != NULL; ++i)
		sysIrqUnregister(wlanIrqTable[i].line);
}

/* Handler of line 12: masks the line again. */
int wlanHostLineMaskIrq(int line, uint32_t pending)
{
	SOC_IRQ_ENABLE_CLEAR = 0x1000;
	return 1;
}
