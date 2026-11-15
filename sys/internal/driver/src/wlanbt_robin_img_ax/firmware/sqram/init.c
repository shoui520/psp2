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
#include "robin/btpower.h"
#include "robin/eventpower.h"
#include "robin/osa.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/btcontext.h"
#include "robin/sbc.h"

typedef int (*SysInitFunc)(void);

int wlanInit(void);		/* 0x0001bdc4, not converted */
int rfCalibInitStage(void);	/* 0xc0007506 */
int clockInit(void);	/* 0xc0007540 */
int bufferPoolsInit(void);	/* 0x0001bcce, not converted */
int fmInit(void);		/* 0x0004966a, not converted; creates event flags "FmFlags" */
int socPinInit(void);	/* 0xc0007562 */

static SysInitFunc sysInitTable[] __attribute__((section(".sqram1.rw"))) = {
	romPatchInit,
	romRamInit,
	sysInit,
	btRomHooksInit,
	wlanInit,
	sysTimerSlotInit,
	rfCalibInitStage,
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

/* Powers the radio up, then sets up its clocks, register sequences and power blocks. */
int rfCalibInitStage(void)
{
	rfPowerOn();
	socHookNop4(0);
	socClockInit();
	socHookNop(0);
	rfRestart();
	socClockConfigure();
	socHookNop2();
	rfRegSequenceInit();
	sleepClockSelect();
	socEnableBlockA();
	socClockFilter();
	socPowerPreset();
	return 0;
}

/* Starts the OS layer: its subsystems, the timer pool, the hardware clock and the idle thread. */
int clockInit(void)
{
	osaInitSubsystems();
	osaTimerPoolInit();
	osaInitializeHardwareClock();
	if (osaCreateIdleThread() != 0)
		return 1;
	osaStartIdleThread();
	return 0;
}

/* The event engine, the pin multiplexing words and the power-save entry the link controller is told of. */
int socPinInit(void)
{
	int entry;

	sysEventEngineInit();
	SOC_REG(0x24) = 0x0020000f;
	SOC_REG(0x28) = 0x80808080;
	SOC_REG(0x2c) = 0x80800a80;
	REG32(0x8000a874) &= ~8u;
	SOC_REG(0x60) &= 0xcfffcfff;
	entry = wlanPsSetupEntry();
	btLmpLcScheduleEntry(entry == 3 || entry == 4);
	return 0;
}
