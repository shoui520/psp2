/* ScePervasive register mappings and power-event initialization. */
#include "lowio_internal.h"
#include <stddef.h>
#include <string.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/dipsw.h>
#include <sce/threadmgr.h>
#include <sce/sysevent.h>

extern uint32_t s_hardwareInfo[5];
void LowioPixelClockInit(void);
void LowioIdleEntry(void);
uint32_t ScePervasiveForDriver_6AF0196F(unsigned first, unsigned second);

static SceKernelFastMutex s_pervasiveMutex;
static SceUID s_pervasiveUids[16];

static const struct {
	const char *name;
	uint32_t paddr;
	uint8_t **base;
} s_pervasiveDevices[16] = {
	{ "ScePervasiveMisc", 0xe3100000, &s_misc },
	{ "ScePervasiveReset", 0xe3101000, &s_reset },
	{ "ScePervasiveGate", 0xe3102000, &s_gate },
	{ "ScePervasiveBaseClk", 0xe3103000, &s_baseClock },
	{ "ScePervasiveVid", 0xe3104000, &s_video },
	{ "ScePervasiveMailbox", 0xe3106000, &s_mailbox },
	{ "ScePervasiveTas0", 0xe3108000, &s_tas[0] },
	{ "ScePervasiveTas1", 0xe3109000, &s_tas[1] },
	{ "ScePervasiveTas2", 0xe310a000, &s_tas[2] },
	{ "ScePervasiveTas3", 0xe310b000, &s_tas[3] },
	{ "ScePervasiveTas4", 0xe310c000, &s_tas[4] },
	{ "ScePervasiveTas5", 0xe310d000, &s_tas[5] },
	{ "ScePervasiveTas6", 0xe310e000, &s_tas[6] },
	{ "ScePervasiveTas7", 0xe310f000, &s_tas[7] },
	{ "ScePervasive2", 0xe3110000, &s_pervasive2 },
	{ "SceUartclkReg", 0xe3105000, &s_uartClock },
};

static void PervasiveClockInit(void)
{
	unsigned mode;

	LowioPixelClockInit();
	mode = ksceKernelCheckDipsw(0xec) == 0 ? 3 : 5;
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

int LowioPervasiveInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned i;
	int ret;

	ret = ksceKernelInitializeFastMutex(&s_pervasiveMutex, "ScePervasive",
		2, NULL);
	if (ret < 0)
		return ret;
	for (i = 0; i < 16; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_pervasiveDevices[i].paddr;
		ret = ksceKernelAllocMemBlock(s_pervasiveDevices[i].name,
			i == 5 ? 0x20100206 : 0x20100806, 0x1000, &opt);
		if (ret < 0) {
			unsigned j;

			/* Sony frees the whole UID array, including untouched entries. */
			for (j = 0; j < 16; j++)
				ksceKernelFreeMemBlock(s_pervasiveUids[j]);
			ksceKernelFinalizeFastMutex(&s_pervasiveMutex);
			return ret;
		}
		s_pervasiveUids[i] = ret;
		ksceKernelGetMemBlockBase(ret, (void **)s_pervasiveDevices[i].base);
	}
	s_hardwareInfo[0] = *(volatile uint32_t *)s_misc;
	s_hardwareInfo[1] = *(volatile uint32_t *)(s_misc + 4);
	for (i = 0; i < 3; i++)
		s_hardwareInfo[i + 2] = *(volatile uint32_t *)(s_misc + 0x300 + i * 4);
	PervasiveClockInit();
	ksceKernelRegisterSysEventHandler("ScePervasive", PervasiveSysEvent, NULL);
	SceThreadmgrForDriver_B4A05763(LowioIdleEntry);
	return 0;
}
