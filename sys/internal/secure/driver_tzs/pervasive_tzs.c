/*
 * pervasive_tzs.c: the secure half of the Pervasive driver. The Non-secure
 * SceLowio asks for the secure devices' resets, gates and clocks through
 * monitor calls 0x110-0x114; the GPU, Venezia/VIP and Allegrex power
 * sequences run here. A core that must run alone parks the other cores on
 * software interrupt 11.
 *
 * Internal names are unknown; they follow SceLowio's (pervasive_reset.c,
 * pervasive_gate.c) where the code is the same.
 */
#include "pervasive_tzs.h"
#include <sysmem/cp15.h>

/* the preload engine operation c11, c2, 1 (ARMv7 PLE), run before parking */
#define plePark()		CP_WRITE(p15, 0, c11, c2, 1, 0)

#define PERVASIVE_INTR_PARK	0xb	/* software interrupt: park this core */

ScePervasiveMailboxRegisters *s_mailbox;
volatile SceUartClkgenRegisters *s_uartClock;
ScePervasiveVidRegisters *s_video;
ScePervasiveMiscRegisters *s_misc;
SceKernelSpinlock s_pervasiveLock;
ScePervasiveResetRegisters *s_reset;
ScePervasive2Registers *s_pervasive2;
ScePervasiveGateRegisters *s_gate;
ScePervasiveBaseClkRegisters *s_baseClock;
volatile int s_parkedCpus;			/* bit n: core n is parked */

/* the mappings and what PervasiveInit read; Sony's code reaches all of it
 * through one base */
static struct {
	uint32_t reserved[16];			/* nothing reads or writes it */
	SceUID uid[16];
	uint8_t *tas[8];
	volatile uint32_t hardwareInfo[5];		/* revision, cacheStatus, hardwareInfo[3] */
} s_pervasive;
static int s_parkCleanCache;			/* parking cores clean their data cache */

static const struct {
	const char *name;
	uint32_t paddr;
	uint32_t size;
	void *base;
} s_pervasiveDevices[16] = {
	{ "ScePervasiveMisc", PERVASIVE_MISC_BASE, PERVASIVE_PAGE_SIZE, &s_misc },
	{ "ScePervasiveReset", PERVASIVE_RESET_BASE, PERVASIVE_PAGE_SIZE, &s_reset },
	{ "ScePervasiveGate", PERVASIVE_GATE_BASE, PERVASIVE_PAGE_SIZE, &s_gate },
	{ "ScePervasiveBaseClk", PERVASIVE_BASECLK_BASE, PERVASIVE_PAGE_SIZE, &s_baseClock },
	{ "ScePervasiveVid", PERVASIVE_VID_BASE, PERVASIVE_PAGE_SIZE, &s_video },
	{ "ScePervasiveMailbox", PERVASIVE_MAILBOX_BASE, PERVASIVE_PAGE_SIZE, &s_mailbox },
	{ "ScePervasiveTas0", PERVASIVE_TAS_BASE(0), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[0] },
	{ "ScePervasiveTas1", PERVASIVE_TAS_BASE(1), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[1] },
	{ "ScePervasiveTas2", PERVASIVE_TAS_BASE(2), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[2] },
	{ "ScePervasiveTas3", PERVASIVE_TAS_BASE(3), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[3] },
	{ "ScePervasiveTas4", PERVASIVE_TAS_BASE(4), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[4] },
	{ "ScePervasiveTas5", PERVASIVE_TAS_BASE(5), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[5] },
	{ "ScePervasiveTas6", PERVASIVE_TAS_BASE(6), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[6] },
	{ "ScePervasiveTas7", PERVASIVE_TAS_BASE(7), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[7] },
	{ "ScePervasive2", PERVASIVE2_BASE, PERVASIVE_PAGE_SIZE, &s_pervasive2 },
	{ "SceUartclkReg", UART_CLKGEN_BASE, PERVASIVE_PAGE_SIZE, &s_uartClock },
};

int module_start(SceSize arglen, const void *argp)
{
	PervasiveModuleStart(arglen, argp);
	GrabModuleStart(arglen, argp);
	EmcModuleStart(arglen, argp);
	PowerModeModuleStart(arglen, argp);
	CrashDumpModuleStart(arglen, argp);
	return 0;
}

/* software interrupt 11: park this core until the one that sent it is done */
static int PervasiveParkIntrHandler(int intrCode, void *userCtx)
{
	unsigned int cpu = readMpidr() & 3;
	unsigned int bit = 1 << cpu;

	(void)intrCode;
	(void)userCtx;
	if ((s_parkedCpus & ~bit) != 0) {
		plePark();
		if (s_parkCleanCache)
			SceCpuForTZS_D4E7413D();
		sceKernelAtomicOrAndGet32(&s_parkedCpus, bit);
		PervasiveDsb();
		__asm__ volatile("sev");
		while ((s_parkedCpus >> cpu) & 1)
			__asm__ volatile("wfe" : : : "memory");
	}
	return -1;
}

/* ===== power sequences ===== */

static int GpuPowerOn(void)
{
	GpuResetEnable();
	GpuIsolationDisable();
	GpuBusIdle();
	CompatRamGateEnable();
	GpuGateEnable(0xf000f);
	CompatRamGateDisable();
	GpuGateDisable(0xf000f);
	GpuPowerClockEnable();
	GpuResetDisable();
	CompatRamGateEnable();
	GpuGateEnable(0xf000f);
	return 0;
}

static int GpuPowerOff(void)
{
	GpuResetEnable();
	GpuGateDisable(0xf000f);
	GpuPowerClockDisable();
	GpuIsolationEnable();
	return 0;
}

static int VeneziaPowerOn(void)
{
	VipResetEnable();
	VipRegisterAccessResetEnable();
	VipGateDisable();
	VeneziaResetEnable();
	VeneziaRegisterAccessResetEnable();
	VeneziaGateDisable();
	VeneziaSecureGateDisable();
	VipClockSet(1);
	VeneziaClockSet(1);
	VipGateEnable();
	VeneziaGateEnable();
	VeneziaSecureGateEnable();
	VeneziaIsolationDisable();
	VeneziaBusIdle();
	VipGateDisable();
	VeneziaGateDisable();
	VeneziaSecureGateDisable();
	VipRegisterAccessResetDisable();
	VeneziaRegisterAccessResetDisable();
	return 0;
}

static int VeneziaPowerOff(void)
{
	VipResetEnable();
	VipRegisterAccessResetEnable();
	VeneziaResetEnable();
	VeneziaRegisterAccessResetEnable();
	VeneziaSecureGateDisable();
	VeneziaGateDisable();
	VipGateDisable();
	VeneziaIsolationEnable();
	return 0;
}

/* Allegrex at 0xa or, fast, at 0xf */
int AllegrexPowerOn(int fast)
{
	AllegrexGateDisable();
	s_misc->allegrexResetHold = 1;
	if (!fast)
		s_baseClock->allegrexClock = (s_baseClock->allegrexClock & 0xff000000) | 0xa;
	else
		s_baseClock->allegrexClock = (s_baseClock->allegrexClock & 0xff000000) | 0xf;
	(void)s_baseClock->allegrexClock;
	PervasiveDmb();
	AllegrexResetEnable();
	s_baseClock->allegrexClockEnable = 1;
	PervasiveDmb();
	s_baseClock->allegrexBusClockEnable = 1;
	PervasiveDmb();
	s_misc->allegrexResetHold = 0;
	(void)s_misc->allegrexResetHold;
	PervasiveDmb();
	AllegrexGateEnable();
	return 0;
}

int AllegrexStart(void)
{
	AllegrexGateDisable();
	AllegrexResetDisable();
	AllegrexGateEnable();
	return 0;
}

int AllegrexPowerOff(void)
{
	AllegrexGateDisable();
	s_misc->allegrexResetHold = 1;
	AllegrexResetEnable();
	SonyRegbusResetEnable();
	SonyRegbusGateDisable();
	s_baseClock->allegrexClockEnable = 0;
	PervasiveDmb();
	s_baseClock->allegrexBusClockEnable = 0;
	PervasiveDmb();
	SonyRegbusGateEnable();
	SonyRegbusResetDisable();
	return 0;
}

/* monitor call 0x110: power the GPU (0), Venezia and VIP (1) or Allegrex (2) */
static int PervasivePowerMonitorCall(int device, int mode, int arg2, int arg3)
{
	(void)arg2;
	(void)arg3;
	switch (device) {
	case 0:
		if (mode)
			return GpuPowerOn();
		return GpuPowerOff();
	case 1:
		if (mode)
			return VeneziaPowerOn();
		return VeneziaPowerOff();
	case 2:
		switch (mode) {
		case 1:
			return AllegrexPowerOn(0);
		case 2:
			return AllegrexPowerOn(1);
		case 3:
			return AllegrexStart();
		}
		return AllegrexPowerOff();
	}
	return PERVASIVE_ERROR;
}

/* ===== initialization ===== */

static __attribute__((noinline)) int PervasiveInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned int i, mode;
	int ret;

	for (i = 0; i < 16; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_pervasiveDevices[i].paddr;
		ret = sceKernelAllocMemBlockForKernel(s_pervasiveDevices[i].name,
			i != 5 ? 0x20100806 : 0x20100206,
			s_pervasiveDevices[i].size, &opt);
		if (ret < 0)
			goto error;
		s_pervasive.uid[i] = ret;
		sceKernelGetMemBlockBase(ret, (void **)s_pervasiveDevices[i].base);
	}
	s_pervasive.hardwareInfo[0] = s_misc->revision;
	s_pervasive.hardwareInfo[1] = s_misc->cacheStatus;
	for (i = 0; i < 3; i++)
		s_pervasive.hardwareInfo[i + 2] = s_misc->hardwareInfo[i];
	PixelClockInit();
	PervasiveMiscInit();
	mode = sceKernelCheckDipsw(DIPSW_GPU_OVERCLOCK) == 0 ? 3 : 5;
	GpuClockSet(mode, mode);
	sceKernelRegisterMonitorCallHandler(PERVASIVE_MONCALL_POWER, PervasivePowerMonitorCall);
	sceKernelRegisterMonitorCallHandler(PERVASIVE_MONCALL_RESET, PervasiveResetMonitorCall);
	sceKernelRegisterMonitorCallHandler(PERVASIVE_MONCALL_GATE, PervasiveGateMonitorCall);
	sceKernelRegisterMonitorCallHandler(PERVASIVE_MONCALL_CLOCK,
		(SceKernelMonitorCallHandler)PervasiveClockMonitorCall);
	sceKernelRegisterIntrHandler(PERVASIVE_INTR_PARK, "ScePervasive", 0,
		PervasiveParkIntrHandler, NULL, 0x50, ALL_CPUS, NULL);
	return 0;

error:
	/* Sony frees the whole UID array, including untouched entries. */
	for (i = 0; i < 16; i++)
		sceKernelFreeMemBlock(s_pervasive.uid[i]);
	return ret;
}

__attribute__((noinline)) int PervasiveModuleStart(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (PervasiveInit() < 0)
		return 2;
	return 0;
}

unsigned int GetSoCRevision(void)
{
	return s_pervasive.hardwareInfo[0];
}

/* Park the other cores, cleaning their data caches first if asked. */
int ParkOtherCpus(int cleanCache)
{
	uint32_t state;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	s_parkCleanCache = cleanCache;
	plePark();
	s_parkedCpus = 1 << (readMpidr() & 3);
	PervasiveDsb();
	sceKernelGenerateSoftIntr(PERVASIVE_INTR_PARK, 1, 0);
	while (s_parkedCpus != ALL_CPUS)
		__asm__ volatile("wfe" : : : "memory");
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

int ReleaseOtherCpus(void)
{
	s_parkedCpus = 0;
	PervasiveDsb();
	__asm__ volatile("sev");
	return 0;
}
