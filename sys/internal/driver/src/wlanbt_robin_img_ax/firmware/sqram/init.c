/*
 * Firmware init table and its runner, kept in SQRAM1 (runner 0xc00074f0,
 * table 0xc000faa8). tx_application_define() runs the entries in order;
 * return values are ignored.
 *
 * Entries marked "not converted" keep provisional names until their
 * subsystems are converted.
 */
#include <stddef.h>
#include "robin/sys.h"
#include "robin/amp.h"
#include "robin/rom.h"

typedef int (*SysInitFunc)(void);

int btRomHooksInit(void);	/* 0x00042e00, not converted */
int wlanInit(void);		/* 0x0001bdc4, not converted */
int rfInit(void);		/* 0xc0007506, not converted */
int clockInit(void);		/* 0xc0007540, not converted */
int bufferPoolsInit(void);	/* 0x0001bcce, not converted */
int btHctInit(void);		/* 0x00042d9a, not converted; creates thread "hct" */
int fmInit(void);		/* 0x0004966a, not converted; creates event flags "FmFlags" */
int btSbcInit(void);		/* 0x0004241c, not converted */
int socPinInit(void);		/* 0xc0007562, not converted */

static SysInitFunc sysInitTable[] __attribute__((section(".sqram1.rw"))) = {
	romPatchInit,
	romRamInit,
	sysInit,
	btRomHooksInit,
	wlanInit,
	sysTimerSlotInit,
	rfInit,
	clockInit,
	sysIrqEnableAll,
	bufferPoolsInit,
	btHctInit,
	fmInit,
	btSbcInit,
	ampPalInit,
	socPinInit,
	NULL,
};

void sysRunInitTable(void)
{
	SysInitFunc *entry;

	for (entry = sysInitTable; *entry != NULL; entry++)
		(*entry)();
}
