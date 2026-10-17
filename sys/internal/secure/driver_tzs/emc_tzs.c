/*
 * emc_tzs.c: the CDRAM's external memory controller (SceEmcTop). SceLowio's
 * SceCdramForDriver calls monitor calls 0x117 (sceCdramEnable: program the
 * controller and initialize the CDRAM, or only restart it after a wake up
 * from suspend, when the CDRAM kept its contents in self refresh), 0x118
 * and 0x119 (enter and exit self refresh; Sony's names spell it
 * "SelfResresh").
 *
 * Internal names are unknown; the monitor calls follow the henkaku wiki.
 */
#include "driver_tzs.h"

#define EMC_INTR		0x22
#define EMC_ERROR		(-1)

#define ERNIE_WAKEUP_RESUME	0x80	/* woken from suspend: the CDRAM is in self refresh */

#define EMC_CONTROL_RUN		0x600
#define EMC_CONTROL_HOLD	0x200

static struct {
	SceEmcRegisters *regs;
	uint32_t control;
	uint32_t enableTiming[2];		/* eventTiming at enable */
	uint32_t eventTiming[2];		/* eventTiming after the controller's event */
	SceKernelSpinlock lock;
	SceUID uid;
	int enabled;
} s_emc;


/* 0x118 */
static int CdramEnterSelfRefreshMonitorCall(void)
{
	SceEmcRegisters *regs = s_emc.regs;

	if (regs == NULL || (regs->command & EMC_COMMAND_IN_SELF_REFRESH) != 0)
		return 0;
	regs->command = EMC_COMMAND_SELF_REFRESH;
	PervasiveDmb();
	while ((regs->command & EMC_COMMAND_IN_SELF_REFRESH) == 0)
		;
	return 0;
}

/* 0x119 */
static int CdramExitSelfRefreshMonitorCall(void)
{
	SceEmcRegisters *regs = s_emc.regs;

	if (regs != NULL && (regs->command & EMC_COMMAND_IN_SELF_REFRESH) != 0) {
		regs->command = 0;
		PervasiveDmb();
	}
	return 0;
}

static int EmcIntrHandler(int intrCode, void *userCtx)
{
	SceEmcRegisters *regs = s_emc.regs;
	uint32_t status;
	int state;

	(void)intrCode;
	(void)userCtx;
	state = SceCpuForDriver_D32ACE9E(&s_emc.lock);
	status = regs->intrStatus;
	regs->intrClear = 1;
	if (status & EMC_INTR_EVENT) {
		regs->eventTiming[1] = s_emc.eventTiming[1];
		regs->eventTiming[0] = s_emc.eventTiming[0];
		regs->intrEnable = 0;
		PervasiveDmb();
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_emc.lock, state);
	return -1;
}

/* run one controller command and wait for it */
static inline int EmcCommand(uint32_t data, uint32_t command)
{
	SceEmcRegisters *regs = s_emc.regs;

	if (regs == NULL)
		return EMC_ERROR;
	regs->commandData = data;
	regs->command = (regs->command & EMC_COMMAND_KEEP) | command;
	PervasiveDmb();
	while (regs->command & EMC_COMMAND_START)
		;
	return 0;
}

static inline void EmcSetTiming(SceEmcRegisters *regs, uint32_t control)
{
	regs->control = control;
	regs->timing[0] = 0x72233;
	regs->timing[1] = 0x7725245;
	regs->timing[2] = 0x1414;
	regs->timing[3] = 0x1f0704;
	regs->timing[4] = 0x1f0c0f;
	regs->timing[5] = 0x20b;
	regs->timing[6] = 6;
	regs->eventTiming[0] = s_emc.enableTiming[0];
	regs->eventTiming[1] = s_emc.enableTiming[1];
	regs->refresh = 0x15fff;
	PervasiveDmb();
}

/* 0x117: sceCdramEnable */
static int CdramEnableMonitorCall(void)
{
	SceEmcRegisters *regs;
	uint32_t control;

	if (s_emc.enabled)
		return 0;
	s_emc.enabled = 1;
	if ((sceKernelSysrootGetErnieWakeupFactorForTZS() & ERNIE_WAKEUP_RESUME) == 0) {
		regs = s_emc.regs;
		sceKernelDisableIntr(EMC_INTR);
		control = s_emc.control & ~EMC_CONTROL_HOLD;
		EmcSetTiming(regs, control);
		EmcCommand(0xe0000, EMC_COMMAND_START);
		sceKernelUsleep(200000);
		regs->command = EMC_COMMAND_KEEP;
		regs->command = 0;
		PervasiveDsb();
		if (EmcCommand(0x40400, EMC_COMMAND_START) == 0
		    && EmcCommand(0x20000, EMC_COMMAND_START) == 0
		    && EmcCommand(0x20000, EMC_COMMAND_START) == 0
		    && EmcCommand(0x31, EMC_COMMAND_START) == 0)
			EmcCommand(0x200000, EMC_COMMAND_START | EMC_COMMAND_LAST);
	} else {
		regs = s_emc.regs;
		sceKernelDisableIntr(EMC_INTR);
		control = s_emc.control & ~EMC_CONTROL_HOLD;
		EmcSetTiming(regs, control);
		regs->command = 0;
		PervasiveDsb();
		EmcCommand(0xe0000, EMC_COMMAND_START);
		sceKernelUsleep(200000);
	}
	regs->control = control | EMC_CONTROL_RUN;
	PervasiveDmb();
	regs->intrEnable = 0;
	regs->intrClear = 1;
	PervasiveDmb();
	regs->intrMode = 0;
	regs->intrEnable = 1;
	PervasiveDmb();
	sceKernelEnableIntr(EMC_INTR);
	return 0;
}

int EmcModuleStart(SceSize arglen, const void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	int ret;

	(void)arglen;
	(void)argp;
	s_emc.regs = NULL;
	s_emc.lock = 0;
	s_emc.control = 0x2251;
	s_emc.enableTiming[0] = 0x8c;
	s_emc.enableTiming[1] = 0x69462300;
	s_emc.eventTiming[0] = 0x23;
	s_emc.eventTiming[1] = 0x18100800;
	s_emc.uid = 0;
	s_emc.enabled = 0;
	memset(&opt, 0, sizeof(opt));
	opt.size = 0x58;
	opt.attr = 2;
	opt.paddr = EMC_TOP_BASE;
	ret = SceSysmemForDriver_C94850C9("SceEmcTop", 0x20100806, EMC_TOP_SIZE, &opt);
	if (ret < 0)
		return 2;
	s_emc.uid = ret;
	SceSysmemForDriver_A841EDDA(ret, &base);
	s_emc.regs = base;
	sceKernelRegisterIntrHandler(EMC_INTR, "SceEmcTop", 0, EmcIntrHandler, NULL,
		0x50, ALL_CPUS, NULL);
	sceKernelRegisterMonitorCallHandler(CDRAM_MONCALL_ENABLE,
		(SceKernelMonitorCallHandler)CdramEnableMonitorCall);
	sceKernelRegisterMonitorCallHandler(CDRAM_MONCALL_ENTER_SELF_REFRESH,
		(SceKernelMonitorCallHandler)CdramEnterSelfRefreshMonitorCall);
	sceKernelRegisterMonitorCallHandler(CDRAM_MONCALL_EXIT_SELF_REFRESH,
		(SceKernelMonitorCallHandler)CdramExitSelfRefreshMonitorCall);
	return 0;
}
