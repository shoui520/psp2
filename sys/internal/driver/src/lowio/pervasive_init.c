/* ScePervasive register mappings and power-event initialization. */
#include "lowio_internal.h"
#include <stddef.h>
#include <string.h>
#include <sysmem/sysmem.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/dipsw.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysevent.h>
#include <lowio/pervasive.h>

extern uint32_t s_hardwareInfo[5];
void LowioIdleEntry(void);

static SceKernelFastMutex s_pervasiveMutex;
static SceUID s_pervasiveUids[16];

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
	{ "ScePervasiveTas0", PERVASIVE_TAS_BASE(0), PERVASIVE_PAGE_SIZE, &s_tas[0] },
	{ "ScePervasiveTas1", PERVASIVE_TAS_BASE(1), PERVASIVE_PAGE_SIZE, &s_tas[1] },
	{ "ScePervasiveTas2", PERVASIVE_TAS_BASE(2), PERVASIVE_PAGE_SIZE, &s_tas[2] },
	{ "ScePervasiveTas3", PERVASIVE_TAS_BASE(3), PERVASIVE_PAGE_SIZE, &s_tas[3] },
	{ "ScePervasiveTas4", PERVASIVE_TAS_BASE(4), PERVASIVE_PAGE_SIZE, &s_tas[4] },
	{ "ScePervasiveTas5", PERVASIVE_TAS_BASE(5), PERVASIVE_PAGE_SIZE, &s_tas[5] },
	{ "ScePervasiveTas6", PERVASIVE_TAS_BASE(6), PERVASIVE_PAGE_SIZE, &s_tas[6] },
	{ "ScePervasiveTas7", PERVASIVE_TAS_BASE(7), PERVASIVE_PAGE_SIZE, &s_tas[7] },
	{ "ScePervasive2", PERVASIVE2_BASE, PERVASIVE_PAGE_SIZE, &s_pervasive2 },
	{ "SceUartclkReg", UART_CLKGEN_BASE, PERVASIVE_PAGE_SIZE, &s_uartClock },
};

static inline __attribute__((always_inline)) void PervasiveClockInit(void)
{
	unsigned mode;

	LowioPixelClockInit();
	PervasiveMiscInit();
	mode = sceKernelCheckDipsw(DIPSW_GPU_OVERCLOCK) == 0 ? 3 : 5;
	ScePervasiveForDriver_6AF0196F(mode, mode);
}

static int PervasiveSysEvent(int resume, int event, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (resume == 1 && event == 0x10000)
		PervasiveClockInit();
	return 0;
}

static __attribute__((noinline)) int PervasiveInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned i;
	int ret;

	ret = sceKernelInitializeFastMutex(&s_pervasiveMutex, "ScePervasive",
		2, NULL);
	if (ret < 0)
		return ret;
	for (i = 0; i < 16; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_pervasiveDevices[i].paddr;
		ret = sceKernelAllocMemBlockForKernel(s_pervasiveDevices[i].name,
			i == 5 ? 0x20100206 : 0x20100806,
			s_pervasiveDevices[i].size, &opt);
		if (ret < 0) {
			unsigned j;

			/* Sony frees the whole UID array, including untouched entries. */
			for (j = 0; j < 16; j++)
				sceKernelFreeMemBlock(s_pervasiveUids[j]);
			sceKernelFinalizeFastMutex(&s_pervasiveMutex);
			return ret;
		}
		s_pervasiveUids[i] = ret;
		sceKernelGetMemBlockBase(ret, (void **)s_pervasiveDevices[i].base);
	}
	s_hardwareInfo[0] = s_misc->revision;
	s_hardwareInfo[1] = s_misc->cacheStatus;
	for (i = 0; i < 3; i++)
		s_hardwareInfo[i + 2] = s_misc->hardwareInfo[i];
	PervasiveClockInit();
	sceKernelRegisterSysEventHandler("ScePervasive", PervasiveSysEvent, NULL);
	SceThreadmgrForDriver_B4A05763(LowioIdleEntry);
	return 0;
}

int LowioPervasiveInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (PervasiveInit() < 0)
		return 2;
	return 0;
}
