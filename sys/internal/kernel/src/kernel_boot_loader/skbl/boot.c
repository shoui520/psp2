/*
 * SKBL: boot(), which every CPU runs from the reset handler with the MMU off:
 * CPU 0 brings the SoC up (DIP switches, debug console, clocks and resets,
 * the LPDDR2 controllers, the access controls), builds the kernel's page
 * tables and boot parameters, then every CPU turns the MMU and caches on and
 * enters sceKblMain (boot.c in Sony's debug records).
 *
 * NSKBL is built from this file too (BOOT_NSKBL), as Sony's was (the same
 * debug records): the same exception handlers and ASLR helpers, and its own
 * boot(), which maps the kernel's DRAM and starts it in Non-secure state.
 * The rest of SKBL (memcpy_chk.c, memory_test.c, the kernel library it
 * links) is not written yet.
 */

#include <skeleton.h>
#include <stdint.h>
#include <kernel_boot_loader/kbl_param.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/cp15.h>
#include <sysmem/debug.h>
#include <excpmgr/excpmgr_cpu.h>
#include <compat/grab_regs.h>
#include <kernel_boot_loader/kernel_boot_param.h>
#include <buserror/lpddr2_regs.h>
#include <lowio/pervasive_regs.h>
#include <buserror/ssx_firewall_regs.h>
#include <systimer/timer_regs.h>
#include "boot.h"
#include "../../sysmem/sysmem_internal.h"
#include <lowio/pervasive.h>
#include <sysmem/dipsw.h>
#include <sysmem/debugled.h>

/* the code each handler shows on the GPO LEDs: 0x8n in SKBL, 0xan in NSKBL */
#ifdef BOOT_NSKBL
#define BOOT_GPO(code)		(0xa0 | (code))
#else
#define BOOT_GPO(code)		(0x80 | (code))
#endif

#define SSX_REGISTER_TARGET(xbar) ((SceSsxRegisterTarget *)SSX_REGISTER_TARGET_BASE(xbar))
#define LPDDR0_FIREWALL		((SceSsxFirewallRegisters *)SSX_FIREWALL_LPDDR0_BASE)
#define LPDDR1_FIREWALL		((SceSsxFirewallRegisters *)SSX_FIREWALL_LPDDR1_BASE)

/* the other boot loader parts boot() calls */
int KblGetBuildNumber(void);
int sceKernelMMUMapSmallPage(uint32_t *l2, uint32_t type, uintptr_t va, uintptr_t offset,
			     uintptr_t pa);
uint32_t SceCpuForKernel_9818A20B(uint32_t value);	/* mmu.c: a page table's TTBR value */
void CpuL2CacheSetEventMonitorBus(int enable);
void CpuL2InvalidateAll(void);
void CpuL2CacheEnable(void);
uint32_t CpuSctlrInit(void);
uint32_t CpuActlrInit(void);

/* test hooks a build may link in; this one has none */
extern void BootTestEarly(void) __attribute__((weak));
extern void BootTest(void) __attribute__((weak));
/* kbl_main.c */
extern int sceKblMain(SceKernelBootParam *param) __attribute__((weak));

/* reset.S: the vector page and its monitor vector table */
extern char _start[], vectorsEnd[];
extern const uint32_t bootDefaultDipsw[8];
/* the build's revision and date (the file before memory_test.c) */
extern const char bootRevision[], bootBuildDate[];
extern const uint32_t monitorVectors[], setMonitorStack[];

/* the vector page as the kernel's exception entry holds it */
typedef struct BootVectorPage {
	SceUInt32 vectors[0x20];	/* reset.S's page; the monitor vectors from 0x40 */
	SceUInt32 defaultDipsw[8];	/* 0x80 */
	SceUInt32 magic;		/* 0xa0 */
	SceUInt32 reserved[0x17];
} BootVectorPage;

_Static_assert(sizeof(BootVectorPage) == 0x100, "vector page");

/* linker script: the .bss bounds */
extern uint32_t _fbss[], _end[];

#define LPDDR2_MAIN		((SceLpddr2Registers *)LPDDR2_MAIN_REG_BASE)
#define LPDDR2_SUB		((SceLpddr2Registers *)LPDDR2_SUB_REG_BASE)
#define TIMER_LT5		((SceLongTimerRegisters *)TIMER_LONG_REG_BASE(5))
#define CPU_L2CC		((void *)0x1a002000)
#define CPU_PRIVATE_BASE	0x1a000000

#define SCE_KBL_PARAM_ADDRESS	0x100		/* where second_loader leaves it */
#define BOOT_VECTOR_PAGE_MAGIC	0x9e3199b7

#define KBL_IMAGE_ADDRESS	0x51000000	/* sceKblMain's TrustZone kernel */
#define KBL_IMAGE_SIZE		0x1000000

/* page table entry types (the memblock codes SceSysmem uses) */
#define BOOT_MAP_CODE		0x1060d007
#define BOOT_MAP_REGISTERS	0x20100206
#define BOOT_MAP_DATA		0x1061d006
#define BOOT_MAP_EXCEPTION	0x1061d005
#define BOOT_MAP_GLOBAL		0x10000
#define BOOT_DOMAIN_KERNEL	0xc

static struct {
	SceKernelBootParam param;
	SceKernelBootParam *current;
} s_bootParam;

static uint8_t s_aslrUsed[0x100];		/* the first megabyte's pages ASLR placed */
#ifdef BOOT_NSKBL
static uint8_t s_megaAslrUsed[0x10];		/* the megabytes of the first 16 MiB ASLR placed */
SceKblParam s_kblParam;				/* NSKBL's kbl_main.c reads it too */
#else
static SceKblParam s_kblParam;
#endif

#define QA_LEVEL(_on, _off)	((s_kblParam.qa_flags[0xd] & 1) ? (_on) : (_off))

/* the monitor vectors follow the vector page's words, 32-byte aligned */
#define MONITOR_VECTORS_OFFSET	((uintptr_t)vectorsEnd - (uintptr_t)_start)
#define ALIGN32(_x)		(((_x) + 0x1f) & ~0x1f)

static inline uint32_t ror4(uint32_t value)
{
	return (value >> 4) | (value << 28);
}

static inline uint32_t ror5(uint32_t value)
{
	return (value >> 5) | (value << 27);
}

#ifndef BOOT_NSKBL

/* first in SKBL's data (skbl.ld), the boot save area */
BootSaveArea bootSaveArea __attribute__((section(".data.bootSaveArea"))) = {
	.corelock = { .lock = 1, .coresLeft = 4, .turn = 4 },
	.kblCorelock = { .lock = 1, .coresLeft = 4, .turn = 4 },
};

/* name unknown: the boot-time memory test of the product line */
void KblMemoryTest(void)
{
	KblSimpleMemoryTest(0x40100000, 0xff00000, 0xca77eb01, 0x1000, 0x2000000);
}

__attribute__((noinline))
int MapRegisterSection(SceKernelBootParam *param, uintptr_t pa, SceSize size);

/* name unknown: the interconnects' windows, mapped by section */
int MapRegisters(SceKernelBootParam *param)
{
	MapRegisterSection(param, SSX_XBAR_BASE(SSX_MAIN_XBAR), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_XBAR_BASE(SSX_CENTER_XBAR), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_XBAR_BASE(SSX_VIDEO_XBAR), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_XBAR_BASE(SSX_IFTU_XBAR), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_XBAR_BASE(SSX_VIP_XBAR), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_XBAR_BASE(SSX_DEBUG_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_XBAR_BASE(SSX_DMAC_XBAR), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_IO_SLAVE_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_IO_MASTER_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_PERVASIVE_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_MISC_IO_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_NORTH_IO_MASTER_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_AUDIO_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_NORTH_IO_SLAVE_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_SRC_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_GPU_REGISTER_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_IFTU_REGISTER_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_CAMERA_SLAVE_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_CAMERA_MASTER_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_USB_SLAVE_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_USB_MASTER_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_SUBLCD_SLAVE_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_SUBLCD_MASTER_BUS), SSX_WINDOW_SIZE);
	MapRegisterSection(param, SSX_BUS_BASE(SSX_CAMERA_REGISTER_BUS), SSX_WINDOW_SIZE);
	return 0;
}

/* name unknown: the interconnects' timeouts and region 2 of the LPDDR2
 * firewalls, matched on each controller's own register block (0x38 in the
 * match word's low, non-address bits); the Debug Bus (and the TOOL units'
 * LPDDR2 on it) only when it is out of reset */
int SsxInit(void)
{
	volatile uint32_t *debugBusReset = &PERVASIVE_RESET->debugBus;

	if (*debugBusReset == 0) {
		SSX_REGISTER_TARGET(SSX_DEBUG_BUS)->networkControl = 0x400;
		LPDDR1_FIREWALL->region[2].addrMatch = LPDDR2_SUB_REG_BASE | 0x38;
		LPDDR1_FIREWALL->region[2].reqInfoPermission = 1;
	}
	SSX_REGISTER_TARGET(SSX_IFTU_XBAR)->networkControl = 0;
	SSX_REGISTER_TARGET(SSX_MAIN_XBAR)->networkControl = 0;
	SSX_REGISTER_TARGET(SSX_CENTER_XBAR)->networkControl = 0;
	SSX_REGISTER_TARGET(SSX_VIDEO_XBAR)->networkControl = 0;
	SSX_REGISTER_TARGET(SSX_VIP_XBAR)->networkControl = 0;
	LPDDR0_FIREWALL->region[2].addrMatch = LPDDR2_MAIN_REG_BASE | 0x38;
	LPDDR0_FIREWALL->region[2].reqInfoPermission = 1;
	/* read back: the write has reached the firewall */
	(void)LPDDR0_FIREWALL->region[2].reqInfoPermission;
	__asm__ volatile("dsb sy" : : : "memory");
	return 0;
}

#else

/* the CPUs' boot steps and the kernel boot loader's, taken before .bss is cleared */
static SceCorelockContext s_bootCorelock = { .lock = 1, .coresLeft = 4, .turn = 4 };
static SceCorelockContext s_kblCorelock = { .lock = 1, .coresLeft = 4, .turn = 4 };

/* reset.S: has the secure monitor unmap the secure kernel boot loader */
void UnmapKbl(void);

#endif /* BOOT_NSKBL */

/* name unknown: sections in TTBR0's or TTBR1's half, global */
#ifdef BOOT_NSKBL
static inline __attribute__((always_inline))
#endif
int MapSections(SceKernelBootParam *param, uint32_t type, uintptr_t va, SceSize size,
		uintptr_t pa)
{
	uint32_t *l1;

	if (param->ttbr0MaxAddress >= va)
		l1 = (uint32_t *)param->ttbr0.pbase;
	else
		l1 = (uint32_t *)param->ttbr1.pbase;
	return sceKernelMMUMapSections(l1, type | BOOT_MAP_GLOBAL, BOOT_DOMAIN_KERNEL, va, size,
				       pa);
}

/* name unknown: marks pages of the first megabyte used, unless one already is */
int ReserveASLR(uintptr_t offset, SceSize size)
{
	unsigned int first = offset >> 12;
	unsigned int count = size >> 12;
	unsigned int page;

	if (count == 0)
		return 0;
	for (page = first; page != first + count; page++) {
		if (s_aslrUsed[(uint8_t)page] == 1)
			return SKELETON_ERROR;
	}
	for (page = first; page != first + count; page++)
		s_aslrUsed[(uint8_t)page] = 1;
	return 0;
}

/* a free, size-aligned place in the first megabyte, chosen by the seed */
uintptr_t MapASLR(SceSize size, uint32_t seed)
{
	unsigned int slots = 0x100 / (size >> 12);
	unsigned int slot = seed % slots;
	unsigned int end = slot + slots;
	uintptr_t offset;

	for (; slot < end; slot++) {
		offset = (slot * size) & 0xfffff;
		if (ReserveASLR(offset, size) >= 0)
			return offset;
	}
	SCE_DBG_PRINTF(0xb, 700, "Failed\n");
	for (;;)
		;
}

int IsAllowKernelDebug(void)
{
	return s_kblParam.qa_flags[0xd] & 1;
}

/* name unknown: whether the KBL Param asks for a safe mode boot */
int IsSafeModeBoot(void)
{
	uint32_t factor;
	uint32_t hold;

	if (s_kblParam.boot_flags[3] != 0xff)
		return 1;
	factor = s_kblParam.wakeup_factor & 0x7f;
	if (factor == 0xb)
		goto hold;
	if (factor == 4) {
		if (s_kblParam.suspendinfo_adr == 0)
			return 0;
		goto hold;
	}
	return factor == 0x1f || factor - 0x18 <= 1;
hold:
	hold = ~s_kblParam.hold_ctrl;
	if (((hold >> 8) & 0x54) != 0x54)
		return 0;
	return (hold & 0xc0) == 0;
}

#ifndef BOOT_NSKBL

/* name unknown: one register section, physical = virtual */
__attribute__((noinline))
int MapRegisterSection(SceKernelBootParam *param, uintptr_t pa, SceSize size)
{
	MapSections(param, BOOT_MAP_REGISTERS, pa, size, pa);
	return 0;
}

#endif /* BOOT_NSKBL */

/* the board's hardware dependent words, by the KBL Param's hardware info */
static inline void SetHardwareDependent(SceKernelBootParam *param)
{
	switch (s_kblParam.hardware_info & 0xff0000) {
	case 0x720000:
	case 0x721000:
		param->hardwareDependent[0] = 0x20110;
		param->hardwareDependent[1] = 0x232;
		break;
	case 0x600000:
		param->hardwareDependent[0] = 0x10150;
		param->hardwareDependent[1] = 0x604;
		break;
	case 0x700000:
	case 0x510000:
		param->hardwareDependent[0] = 0x20100;
		param->hardwareDependent[1] = 0x1232;
		break;
	case 0x800000:
		param->hardwareDependent[0] = 0x10200;
		param->hardwareDependent[1] = 0x604;
		break;
	case 0x820000:
		param->hardwareDependent[0] = 0x10220;
		param->hardwareDependent[1] = 0x604;
		break;
	default:
		param->hardwareDependent[0] = 0x10100;
		param->hardwareDependent[1] = 0x604;
		break;
	}
	if (param->kblParam->boot_flags[0] != 0xff || IsSafeModeBoot())
		sceKernelClearDipsw(DIPSW_PLATFORM_DOLCE);
	if (sceKernelCheckDipsw(DIPSW_PLATFORM_DOLCE)) {
		param->hardwareDependent[0] = 0x20101;
		param->hardwareDependent[1] = 0x252;
	}
	if (s_kblParam.hardware_flags[0] & 9)
		param->hardwareDependent[1] |= 0x100;
	else if (param->hardwareDependent[0] != 0x20100 && param->hardwareDependent[0] != 0x20110)
		param->hardwareDependent[1] |= 0x80;
	if (s_kblParam.hardware_flags[1] & 2)
		param->hardwareDependent[1] |= 0x2000;
	if (param->hardwareDependent[0] == 0x20110) {
		if (s_kblParam.hardware_flags[1] & 1)
			param->hardwareDependent[1] &= ~0x1000;
		else
			param->hardwareDependent[1] |= 0x1000;
		sceKernelPrintfLevel(QA_LEVEL(1, 2), "\tmodel: DOLCE\n");
	} else if ((param->hardwareDependent[0] >> 16) == 2) {
		sceKernelPrintfLevel(QA_LEVEL(1, 2), "\tmodel: DOLCE\n");
	} else {
		sceKernelPrintfLevel(QA_LEVEL(1, 2), "\tmodel: VITA\n");
	}
}

#ifndef BOOT_NSKBL

/* the second LPDDR2 channel of the development kits */
static inline void EnableLpddr2Sub(void)
{
	if ((int)s_bootParam.current->socRevision < 0) {
		sceKernelPrintfLevel(QA_LEVEL(0, 2), "Disabled LPDDR2SUB\n");
		return;
	}
	PERVASIVE_BASECLK->dramSubClock = 1;
	Lpddr2IsolationDisable(1);
	Lpddr2GateEnable(1);
	Lpddr2ResetDisable(1);
	/* read back: the controller is out of reset */
	(void)LPDDR2_SUB->config;
	LPDDR2_SUB->config = 0x130b0011;
	LPDDR2_SUB->control[25].value = 0x1fff;
	LPDDR2_SUB->control[6].value = 0xa28;
	LPDDR2_SUB->control[7].value = 5;
	LPDDR2_SUB->control[8].value = 4;
	LPDDR2_SUB->control[9].value = 0x2b;
	LPDDR2_SUB->control[10].value = 5;
	LPDDR2_SUB->control[11].value = 3;
	LPDDR2_SUB->control[14].value = 7;
	LPDDR2_SUB->control[12].value = 0x18;
	LPDDR2_SUB->control[13].value = 3;
	LPDDR2_SUB->control[17].value = 0xc;
	LPDDR2_SUB->control[18].value = 0xc;
	LPDDR2_SUB->control[19].value = 0x10;
	LPDDR2_SUB->control[22].value = 6;
	LPDDR2_SUB->control[28].value = 0x2e;
	LPDDR2_SUB->control[29].value = 0x2e;
	LPDDR2_SUB->control[30].value = 2;
	LPDDR2_SUB->control[40].value = 4;
	LPDDR2_SUB->control[42].value = 2;
	LPDDR2_SUB->control[43].value = 2;
	LPDDR2_SUB->control[44].value = 0xb;
	LPDDR2_SUB->control[45].value = 5;
	LPDDR2_SUB->control[47].value = 6;
	LPDDR2_SUB->control[21].value = 0x2475;
	LPDDR2_SUB->control[53].value = 3;
	LPDDR2_SUB->control[55].value = 3;
	LPDDR2_SUB->control[64].value = 0x72;
	LPDDR2_SUB->control[65].value = 0x72;
	LPDDR2_SUB->control[68].value = 0x60;
	LPDDR2_SUB->control[69].value = 0x60;
	LPDDR2_SUB->control[76].value = 0x70;
	if (s_kblParam.wakeup_factor & 0x80) {
		LPDDR2_SUB->control[LPDDR2_CONTROL_COMMAND(0)].value = LPDDR2_COMMAND_RESET;
		LPDDR2_SUB->control[LPDDR2_CONTROL_COMMAND(1)].value = LPDDR2_COMMAND_RESET;
		while (LPDDR2_SUB->control[LPDDR2_CONTROL_STATUS].value != LPDDR2_STATUS_RESET_DONE)
			;
	}
	LPDDR2_SUB->control[LPDDR2_CONTROL_COMMAND(0)].value = LPDDR2_COMMAND_INIT;
	LPDDR2_SUB->control[LPDDR2_CONTROL_COMMAND(1)].value = LPDDR2_COMMAND_INIT;
	while (LPDDR2_SUB->control[LPDDR2_CONTROL_STATUS].value != LPDDR2_STATUS_INIT_DONE)
		;
	LPDDR2_SUB->control[48].value = 1;
	LPDDR2_SUB->control[32].value = 0x302;
	LPDDR2_SUB->control[33].value = 0x302;
	/* the bank's first word */
	*(volatile uint32_t *)0x80000000 = 0xdeadcafe;
}

/* CPU 0: the SoC, the KBL Param and the boot parameters */
static inline uint32_t BootCpu0Init(void *lr)
{
	SceKernelBootParam *param = &s_bootParam.param;
	uint32_t *p;
	uint32_t revision, kermit, aslr;
	const char *chip;
	unsigned int i;
	int n;

	LedModuleStart();
	sceKernelSetGPO(0x81);
	for (p = _fbss; p < _end; p++)
		*p = 0;
	param->socRevision = PERVASIVE_MISC->revision;
	param->socCacheStatus = PERVASIVE_MISC->cacheStatus;
	memcpy(&s_kblParam, (void *)SCE_KBL_PARAM_ADDRESS, sizeof(s_kblParam));
	param->kblParam = &s_kblParam;
	s_kblParam.magic = SCE_KBL_PARAM_MAGIC;

	/* a development kit whose CP has not written the switches yet */
	if ((s_kblParam.pscode.product_code & 0xfeff) == 1 && s_kblParam.dipswWords[1] == 0) {
		for (i = 0; i < 8; i++)
			s_kblParam.dipswWords[i] = bootDefaultDipsw[i] | s_kblParam.dipswWords[i];
	}
	DipswSetState(&s_kblParam.dipsw);
	if (s_kblParam.boot_flags[1] == 0xff) {
		sceKernelClearDipsw(DIPSW_UART0_CONSOLE);
		sceKernelClearDipsw(DIPSW_USER_UART);
	} else {
		sceKernelSetDipsw(DIPSW_UART0_CONSOLE);
		sceKernelSetDipsw(DIPSW_USER_UART);
	}
	if (!(s_kblParam.boot_flags[4] & 1))
		sceKernelSetDipsw(DIPSW_SHELL_CRON);

	if ((s_kblParam.pscode.product_code & 0xfeff) == 1 &&
	    !sceKernelCheckDipsw(DIPSW_DISABLE_RECONFIGURATION)) {
		if (sceKernelCheckDipsw(DIPSW_DEVELOPMENT_MODE)) {
			sceKernelSetDipsw(DIPSW_DMAC6);
			sceKernelSetDipsw(DIPSW_SYSTEM_DECI);
			sceKernelSetDipsw(DIPSW_DECI);
			sceKernelSetDipsw(DIPSW_UART1_CONSOLE);
			sceKernelSetDipsw(DIPSW_SYSTEM_TTY);
			sceKernelSetDipsw(DIPSW_TTY_STDIO);
			sceKernelSetDipsw(DIPSW_STOP_ON_ASSERT);
			sceKernelSetDipsw(DIPSW_ASSERT_LEVEL_1);
			sceKernelSetDipsw(DIPSW_DEBUG_LEVEL_1);
			sceKernelSetDipsw(DIPSW_CRITICAL_ERROR);
			sceKernelSetDipsw(DIPSW_HARDWARE_BREAKPOINT);
			sceKernelSetDipsw(DIPSW_USER_PA_MAPPING);
			revision = param->socRevision;
			if (revision & 0x1ff00) {
				if ((revision & 0x30000000) == 0x20000000)
					sceKernelSetDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART);
				else
					sceKernelClearDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART);
			} else if ((int)revision >= 0) {
				sceKernelSetDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART);
			} else {
				sceKernelClearDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART);
			}
			revision = param->socRevision;
			if ((revision & 0x1ff00) == 0x100 && (int)revision < 0 &&
			    (revision & 0x30000000) == 0x20000000)
				sceKernelSetDipsw(DIPSW_USER_PA_MAPPING_ADDRESS);
			else
				sceKernelClearDipsw(DIPSW_USER_PA_MAPPING_ADDRESS);
			if (param->kblParam->boot_type_indicator_2 & 0x40)
				sceKernelClearDipsw(DIPSW_STOP_ON_ASSERT);
		} else {
			sceKernelClearDipsw(DIPSW_DMAC6);
			sceKernelClearDipsw(DIPSW_SYSTEM_DECI);
			sceKernelClearDipsw(DIPSW_DECI);
			sceKernelClearDipsw(DIPSW_SYSTEM_TTY);
			sceKernelClearDipsw(DIPSW_TTY_STDIO);
			sceKernelClearDipsw(DIPSW_STOP_ON_ASSERT);
			sceKernelClearDipsw(DIPSW_ASSERT_LEVEL_1);
			sceKernelClearDipsw(DIPSW_ASSERT_LEVEL_2);
			sceKernelClearDipsw(DIPSW_DEBUG_LEVEL_1);
			sceKernelClearDipsw(DIPSW_DEBUG_LEVEL_2);
			sceKernelClearDipsw(DIPSW_CRITICAL_ERROR);
			sceKernelClearDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART);
			sceKernelClearDipsw(DIPSW_HARDWARE_BREAKPOINT);
			sceKernelClearDipsw(DIPSW_USER_PA_MAPPING);
			sceKernelClearDipsw(DIPSW_MEMORY_SIZE);
			sceKernelClearDipsw(DIPSW_USER_PA_MAPPING_ADDRESS);
			if (sceKernelCheckDipsw(DIPSW_RELEASE_CHECK_MODE)) {
				sceKernelSetDipsw(DIPSW_UART1_CONSOLE);
				sceKernelSetDipsw(DIPSW_USER_UART);
			} else {
				sceKernelClearDipsw(DIPSW_UART1_CONSOLE);
				sceKernelClearDipsw(DIPSW_USER_UART);
			}
		}
		sceKernelSetDipsw(DIPSW_CONSOLE_LOGGING);
		if (sceKernelCheckDipsw(DIPSW_UART0_CONSOLE))
			sceKernelClearDipsw(DIPSW_SYSTEM_TTY);
	}

	InitializeDebugPrintLevel();
	InitializeDebugBuffer();
	if (sceKernelCheckDipsw(DIPSW_UART1_CONSOLE) || sceKernelCheckDipsw(DIPSW_UART0_CONSOLE) ||
	    sceKernelCheckDipsw(DIPSW_CONSOLE_LOGGING)) {
		SerialInit(sceKernelCheckDipsw(DIPSW_UART0_CONSOLE));
		sceKernelSetGPO(0x82);
		SceDebugForKernel_22546577((uintptr_t)SerialDebugPutchar, 0);
	}
	SCE_PRINTF_WITH_INFO_FUNC(0, 1868, "boot",
				  "===================================================\n");
	SCE_PRINTF_WITH_INFO_FUNC(0, 1871, "boot",
				  "Starting PSP2 Kernel Boot Loader [0x%08x]: %d\n",
				  FW_VERSION, KblGetBuildNumber());
	SCE_PRINTF_WITH_INFO_FUNC(0, 1873, "boot", "revision   : %s\nbuild date : %s\n",
				  bootRevision, bootBuildDate);
	if (s_kblParam.dipswWords[1] != 0)
		SCE_PRINTF_WITH_INFO_FUNC(0, 1882, "boot", "cp info.   : bid.%x ver.%04x\n",
					  s_kblParam.dipswWords[1] >> 16,
					  s_kblParam.dipswWords[1] & 0xffff);
	else
		SCE_PRINTF_WITH_INFO_FUNC(0, 1884, "boot", "cp info.   : (N/A)\n");
	if (s_kblParam.boot_type_indicator_1 & BOOT_TYPE_PRODUCT_MODE)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1889, "boot", "\tProduct Mode : [ YES ]\n");
	if (((s_kblParam.boot_type_indicator_1 & BOOT_TYPE_PRODUCT_MODE) ||
	     (s_kblParam.qa_flags[0xd] & 1)) &&
	    (s_kblParam.boot_type_indicator_1 & BOOT_TYPE_MEMORY_TEST))
		KblMemoryTest();

	kermit = param->socRevision & 0x1ffff;
	s_bootParam.current = param;
	param->kermitRevision = kermit;
	param->magic = SCE_KERNEL_BOOT_PARAM_MAGIC;
	param->corelock = &bootSaveArea.kblCorelock;
	param->size = sizeof(*param);
	param->secure = 1;
	if (kermit == 0) {
		SCE_DBG_ASSERT_CHECK_LR_FUNC(0, 1932, "boot", lr);
		kermit = s_bootParam.current->kermitRevision;
	}
	if (s_kblParam.qa_flags[0xd] & 1)
		chip = (kermit & 0x1ff00) ? "Kermit1.5" : "Kermit1.0";
	else
		chip = "";
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1941, "boot", "\t%s Revision=%d.%d\n", chip,
				  (kermit >> 4) & 0xf, kermit & 0xf);
	switch (s_bootParam.current->kermitRevision) {
	case 0x00:
	case 0x11:
	case 0x20:
	case 0x30:
	case 0x32:
		SCE_DBG_ASSERT_CHECK_LR_FUNC(0, 1950, "boot", lr);
		break;
	case 0x40:
	case 0x42:
	case 0x110:
	case 0x111:
	case 0x112:
	case 0x113:
	case 0x114:
	case 0x115:
		if (s_bootParam.current->socCacheStatus & 2)
			sceKernelPrintfLevel(QA_LEVEL(0, 2), "\tL2 Cache is defective\n");
		break;
	default:
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1968, "boot", "\tUnknown chip revision [rev=%x]\n",
					  s_bootParam.current->kermitRevision);
		break;
	}

	aslr = s_kblParam.dipsw.aslr_seed;
	sceKernelPrintfLevel(QA_LEVEL(1, 2), "\tASLR: 0x%08x\n", aslr);
	SCE_PRINTF_WITH_INFO_FUNC(0, 1993, "boot", "session ID : %02X%02X%02X%02X%02X%02X%02X%02X"
				  "%02X%02X%02X%02X%02X%02X%02X%02X\n",
			     s_kblParam.session_id[0], s_kblParam.session_id[1],
			     s_kblParam.session_id[2], s_kblParam.session_id[3],
			     s_kblParam.session_id[4], s_kblParam.session_id[5],
			     s_kblParam.session_id[6], s_kblParam.session_id[7],
			     s_kblParam.session_id[8], s_kblParam.session_id[9],
			     s_kblParam.session_id[10], s_kblParam.session_id[11],
			     s_kblParam.session_id[12], s_kblParam.session_id[13],
			     s_kblParam.session_id[14], s_kblParam.session_id[15]);

	s_bootParam.current->numMemory = 1;
	s_bootParam.current->memory[0].pbase = 0x40000000;
	s_bootParam.current->memory[0].psize = 0x200000;
	for (n = 0; n < s_bootParam.current->numMemory; n++)
		sceKernelPrintfLevel(QA_LEVEL(1, 2), "\tmemory[%d] base: 0x%08x-0x%08x [0x%08x]\n",
				      n, s_bootParam.current->memory[n].pbase,
				      s_bootParam.current->memory[n].pbase +
				      s_bootParam.current->memory[n].psize - 1,
				      s_bootParam.current->memory[n].psize);
	sceKernelPrintfLevel(QA_LEVEL(1, 2), "\twakeupFactor: 0x%08x\n",
			      s_kblParam.wakeup_factor);
	sceKernelPrintfLevel(QA_LEVEL(1, 2), "\thardwareInfo: 0x%08x\n",
			      s_kblParam.hardware_info);

	SetHardwareDependent(param);

	sceKernelSetGPO(0x83);
	PervasiveInit();
	TimerGateEnable();
	TimerResetDisable();
	LPDDR2_MAIN->ioConfig[0] = 0x07000001;
	LPDDR2_MAIN->ioConfig[1] = 0x00ec0604;
	__asm__ volatile("dmb sy" : : : "memory");
	CompatRamPowerOn();
	SonyRegbusPowerOn();
	SONY_REGBUS->accessMode[4] = 10;
	__asm__ volatile("dmb sy" : : : "memory");
	if ((s_bootParam.current->kermitRevision & 0x1fff0) == 0x30)
		PERVASIVE_MISC->es3Config |= 8;
	BootDeviceGateEnable();
	BootDeviceResetDisable();
	if (sceKernelCheckDipsw(DIPSW_DEVELOPMENT_MODE) || sceKernelCheckDipsw(DIPSW_DMAC6) ||
	    sceKernelCheckDipsw(DIPSW_SYSTEM_DECI) || sceKernelCheckDipsw(DIPSW_DECI)) {
		DebugBusGateEnable();
		DebugBusResetDisable();
	}
	if (sceKernelCheckDipsw(DIPSW_DEVELOPMENT_MODE)) {
		DebugDeviceGateEnable();
		DebugDeviceResetDisable();
		DebugPaGateEnable();
		DebugPaResetDisable();
	}
	if (sceKernelCheckDipsw(DIPSW_DMAC6) || sceKernelCheckDipsw(DIPSW_SYSTEM_DECI)) {
		ScePervasiveForDriver_D34E74C5(0);
		ScePervasiveForDriver_EDCA3961(0);
	}
	if (sceKernelCheckDipsw(DIPSW_HARDWARE_BREAKPOINT)) {
		ArmGateEnable(0xc00000);
		ArmDebuggerGateEnable();
		ArmDebuggerResetDisable();
	}
	if (sceKernelCheckDipsw(DIPSW_DEVELOPMENT_MODE))
		EnableLpddr2Sub();
	if (sceKernelCheckDipsw(DIPSW_DEVELOPMENT_MODE))
		GRAB_NS->developmentMode = 1;
	PERVASIVE_MISC->pervasiveAccessMode = 0x1f3da;
	PERVASIVE_MISC->tasAccessMode = 0xff;
	PERVASIVE2->accessMode = 0x7ffff;
	TIMER_LT5->control = 0x2f345008;
	TIMER_LT5->countLo = 0;
	TIMER_LT5->countHi = 0;
	TIMER_LT5->compareLo = 0xffffffff;
	TIMER_LT5->compareHi = 0xffffffff;
	TIMER_LT5->control = 0x2f34500d;
	sceKernelSetGPO(0x84);
	if (bootSaveArea.state == 0)
		memcpy((void *)(SCE_KBL_PARAM_ADDRESS + 0x40200000), &s_kblParam, sizeof(s_kblParam));
	return aslr;
}

void boot(void)
{
	void *lr = __builtin_return_address(0);
	SceKernelBootParam *param;
	SceUIntPtr pa;
	uint32_t aslr;
	unsigned int i;
	uint32_t *l2;
	int cpu;

	SceCpuForKernel_2FF54320(&bootSaveArea.corelock, 0);
	if (sceKernelCpuId() != 0)
		aslr = 0;
	else
		aslr = BootCpu0Init(lr);

	writeCsselr(0);
	writeTeecr(1);
	writeTeehbr(8);
	writeJoscr(1);
	writeVbar((uintptr_t)_start);
	writeTpidrurw(0);
	writeTpidruro(0);
	writeTpidrprw(0);
	SceCpuForKernel_FE2B77A3(&bootSaveArea.corelock);
	if (BootTestEarly) {
		SceCpuForKernel_2FF54320(&bootSaveArea.kblCorelock, 0);
		if (sceKernelCpuId() == 0)
			BootTestEarly();
		SceCpuForKernel_FE2B77A3(&bootSaveArea.kblCorelock);
	}

	sceKernelSetGPO(0x85);
	param = s_bootParam.current;
	cpu = sceKernelCpuId();
	param->cpu[cpu].ttbcr = 2;
	param->cpu[cpu].dacr = DACR_ALL_CLIENT;
	param->cpu[cpu].contextId = 0xf0;
	writeDacr(DACR_ALL_CLIENT);
	SceCpuForKernel_2FF54320(&bootSaveArea.corelock, 0);
	if (cpu == 0) {
		uint32_t seed = aslr;
		param->ttbr0MaxAddress = 0x3fffffff;
		for (i = 0; i < sizeof(s_aslrUsed); i++)
			s_aslrUsed[i] = 0;
		ReserveASLR(0, 0x4000);

		param->reset.name = "SceKernelReset";
		param->reset.pbase = param->memory[0].pbase + 0x100000;
		param->reset.vbase = 0;
		param->reset.psize = 0x1000;
		param->reset.vsize = 0x4000;
		param->reset.extraHigh = 0x3000;

		seed = ror4(seed);
		param->l2PageTable000.name = "SceKernelL2PageTable000";
		param->l2PageTable000.pbase = param->memory[0].pbase + 0x101000;
		param->l2PageTable000.vbase = MapASLR(0x1000, seed);
		param->l2PageTable000.psize = 0x1000;
		param->l2PageTable000.vsize = 0x2000;
		param->l2PageTable000.extraHigh = 0x1000;
		param->l2PageTable000MapBase = 0;

		seed = ror4(seed);
		param->sysroot.name = "SceSysroot";
		param->sysroot.pbase = param->memory[0].pbase + 0x104000;
		param->sysroot.vbase = MapASLR(0x4000, seed);
		param->sysroot.psize = 0x4000;
		param->sysroot.vsize = 0x4000;
		param->sysroot.extraHigh = 0;

		seed = ror4(seed);
		param->ttbr0.name = "SceKernelTTBR0";
		param->ttbr0.pbase = param->memory[0].pbase + 0x108000;
		param->ttbr0.vbase = MapASLR(0x4000, seed);
		param->ttbr0.psize = 0x4000;
		param->ttbr0.vsize = 0x4000;
		param->sizeTTBR0Address = 0x40000000;

		seed = ror4(seed);
		param->ttbr1.name = "SceKernelTTBR1";
		param->ttbr1.pbase = param->memory[0].pbase + 0x10c000;
		param->ttbr1.vbase = MapASLR(0x4000, seed);
		param->ttbr1.psize = 0x4000;
		param->ttbr1.vsize = 0x4000;
		param->sizeTTBR1Address = 0xc0000000;

		seed = ror4(seed);
		param->fixedHeap32B.name = "SceKernelFixedHeap32B";
		param->fixedHeap32B.vbase = MapASLR(0x2000, seed);
		param->fixedHeap32B.psize = 0x1000;
		param->fixedHeap32B.vsize = 0x2000;
		param->fixedHeap32B.extraHigh = 0x1000;

		seed = ror4(seed);
		param->fixedHeap48B.name = "SceKernelFixedHeap48B";
		param->fixedHeap48B.vbase = MapASLR(0x2000, seed);
		param->fixedHeap48B.psize = 0x1000;
		param->fixedHeap48B.vsize = 0x2000;
		param->fixedHeap48B.extraHigh = 0x1000;

		seed = ror4(seed);
		param->fixedHeap64B.name = "SceKernelFixedHeap64B";
		param->fixedHeap64B.vbase = MapASLR(0x2000, seed);
		param->fixedHeap64B.psize = 0x1000;
		param->fixedHeap64B.vsize = 0x2000;
		param->fixedHeap64B.extraHigh = 0x1000;

		seed = ror4(seed);
		param->fixedHeapUIDEntry.name = "SceKernelFixedHeapUIDEntry";
		param->fixedHeapUIDEntry.vbase = MapASLR(0x2000, seed);
		param->fixedHeapUIDEntry.psize = 0x1000;
		param->fixedHeapUIDEntry.vsize = 0x2000;
		param->fixedHeapUIDEntry.extraHigh = 0x1000;

		seed = ror4(seed);
		param->exceptionEntry.name = "SceKernelExceptionEntry";
		param->exceptionEntry.pbase = param->memory[0].pbase;
		param->exceptionEntry.vbase = MapASLR(0x2000, seed);
		param->exceptionEntry.psize = 0x1000;
		param->exceptionEntry.vsize = 0x2000;
		param->exceptionEntry.extraHigh = 0x1000;

		seed = ror4(seed);
		param->fixedHeapForL2Object.name = "SceKernelFixedHeapForL2Object";
		param->fixedHeapForL2Object.vbase = MapASLR(0x2000, seed);
		param->fixedHeapForL2Object.psize = 0x1000;
		param->fixedHeapForL2Object.vsize = 0x2000;
		param->fixedHeapForL2Object.extraHigh = 0x1000;

		seed = ror4(seed);
		param->l2Vector.name = "SceKernelL2Vector";
		param->l2Vector.pbase = param->memory[0].pbase + 0x110000;
		param->l2Vector.vbase = MapASLR(0x8000, seed);
		param->l2Vector.psize = 0x4000;
		param->l2Vector.vsize = 0x8000;
		param->l2Vector.extraHigh = 0x4000;

		seed = ror4(seed);
		param->phypage.name = "SceKernelPhyPageTable";
		param->phypage.pbase = param->memory[0].pbase + 0x118000;
		param->phypage.vbase = MapASLR(0x8000, seed);
		param->phypage.psize = 0x8000;
		param->phypage.vsize = 0x8000;
		param->phypage.extraHigh = 0;

		param->bootKernelImage.name = "SceBootKernelImage";
		param->bootKernelImage.pbase = 0x50000000;
		param->bootKernelImage.vbase = 0x50000000;
		param->bootKernelImage.psize = 0x1000000;
		param->bootKernelImage.vsize = 0x2000000;
		param->bootKernelImage.extraHigh = 0x1000000;

		param->hardwareRegisters.name = NULL;
		param->hardwareRegisters.pbase = 0xe0000000;
		param->hardwareRegisters.vbase = 0xe0000000;
		param->hardwareRegisters.psize = 0x8400000;

		sceKernelMMUL1Init((uint32_t *)param->ttbr0.pbase);
		sceKernelMMUL1Init((uint32_t *)param->ttbr1.pbase);
		MapSections(param, BOOT_MAP_CODE, param->bootKernelImage.vbase,
			    param->bootKernelImage.psize, param->bootKernelImage.pbase);
		MapSections(param, BOOT_MAP_CODE, param->memory[0].pbase, 0x100000,
			    param->memory[0].pbase);
		if (bootSaveArea.state == 0)
			MapSections(param, BOOT_MAP_CODE, KBL_IMAGE_ADDRESS, KBL_IMAGE_SIZE,
				    KBL_IMAGE_ADDRESS);
		MapRegisters(param);
		SsxInit();
		MapSections(param, BOOT_MAP_REGISTERS, param->hardwareRegisters.vbase,
			    param->hardwareRegisters.psize, param->hardwareRegisters.pbase);
		MapSections(param, BOOT_MAP_REGISTERS, CPU_PRIVATE_BASE, 0x100000, CPU_PRIVATE_BASE);

		sceKernelMMUSetL2PageTable(param->l2PageTable000MapBase <= param->ttbr0MaxAddress ?
					   (uint32_t *)param->ttbr0.pbase :
					   (uint32_t *)param->ttbr1.pbase,
					   BOOT_DOMAIN_KERNEL, param->l2PageTable000MapBase,
					   param->l2PageTable000.pbase);
		sceKernelMMUL2Init((uint32_t *)param->l2PageTable000.pbase);
		l2 = (uint32_t *)param->l2PageTable000.pbase;
		sceKernelMMUMapSmallPage(l2, BOOT_MAP_DATA, param->l2PageTable000.vbase,
					 param->l2PageTable000.vbase - param->l2PageTable000MapBase,
					 param->l2PageTable000.pbase);
		sceKernelMMUMapSmallPages((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					  param->sysroot.vbase,
					  param->sysroot.vbase - param->l2PageTable000MapBase, 0x4000,
					  param->sysroot.pbase);
		sceKernelMMUMapSmallPages((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					  param->ttbr0.vbase,
					  param->ttbr0.vbase - param->l2PageTable000MapBase, 0x4000,
					  param->ttbr0.pbase);
		sceKernelMMUMapSmallPages((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					  param->ttbr1.vbase,
					  param->ttbr1.vbase - param->l2PageTable000MapBase, 0x4000,
					  param->ttbr1.pbase);
		sceKernelMMUMapSmallPage((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_EXCEPTION,
					 param->exceptionEntry.vbase,
					 param->exceptionEntry.vbase - param->l2PageTable000MapBase,
					 param->exceptionEntry.pbase);
		sceKernelMMUMapSmallPages((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					  param->l2Vector.vbase,
					  param->l2Vector.vbase - param->l2PageTable000MapBase, 0x4000,
					  param->l2Vector.pbase);
		sceKernelMMUMapSmallPages((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					  param->phypage.vbase,
					  param->phypage.vbase - param->l2PageTable000MapBase,
					  param->phypage.psize, param->phypage.pbase);
	}
	SceCpuForKernel_FE2B77A3(&bootSaveArea.corelock);
	writeTtbr0(SceCpuForKernel_9818A20B(param->ttbr0.pbase));
	writeTtbr1(SceCpuForKernel_9818A20B(param->ttbr1.pbase));
	writeTtbcr(param->cpu[cpu].ttbcr);
	writeDacr(DACR_ALL_CLIENT);
	writeContextId(param->cpu[cpu].contextId);

	/* the kernel's vector page: two copies of this one, the monitor vectors in the first */
	SceCpuForKernel_2FF54320(&bootSaveArea.corelock, 0);
	if (sceKernelCpuId() == 0) {
		void *page = memcpy((void *)param->memory[0].pbase, _start, 0x100);

		memcpy((char *)page + 0x100, _start, 0x100);
		param->resetVector = param->exceptionEntry.vbase + 0x100;
		memcpy((void *)ALIGN32(param->memory[0].pbase + MONITOR_VECTORS_OFFSET),
		       monitorVectors, (char *)setMonitorStack - (char *)monitorVectors);
		((BootVectorPage *)s_bootParam.current->memory[0].pbase)->magic =
			BOOT_VECTOR_PAGE_MAGIC;
	}
	SceCpuForKernel_FE2B77A3(&bootSaveArea.corelock);
	if (sceKernelCpuId() == 0)
		CpuL2CacheInit(CPU_L2CC);
	SceCpuForKernel_2FF54320(&bootSaveArea.corelock, 0);
	SceCpuForKernel_FE2B77A3(&bootSaveArea.corelock);
	if (sceKernelCpuId() == 0) {
		CpuL2CacheInit(CPU_L2CC);
		if (sceKernelCheckDipsw(DIPSW_DEVELOPMENT_MODE))
			CpuL2CacheSetEventMonitorBus(1);
		CpuL2InvalidateAll();
	}
	SceCpuForKernel_F9B0B171();
	SceCpuForKernel_15A48AB4();
	if (sceKernelCpuId() == 0)
		CpuL2CacheEnable();
	CpuSctlrInit();
	CpuActlrInit();
	SceCpuForKernel_2FF54320(&bootSaveArea.corelock, 0);
	SceCpuForKernel_FE2B77A3(&bootSaveArea.corelock);
	SceCpuForKernel_2FF54320(&bootSaveArea.kblCorelock, 0);
	SceCpuForKernel_FE2B77A3(&bootSaveArea.kblCorelock);
	writeVbar(param->resetVector);
	writeMvbar(ALIGN32(param->resetVector + MONITOR_VECTORS_OFFSET));
	MMUSetReady();
	SceCpuForKernel_2FF54320(&bootSaveArea.kblCorelock, 0);
	sceKernelCpuId();
	SceCpuForKernel_FE2B77A3(&bootSaveArea.kblCorelock);

	sceKernelSetGPO(0x86);
	if (BootTest) {
		SceCpuForKernel_2FF54320(&bootSaveArea.kblCorelock, 0);
		if (sceKernelCpuId() == 0) {
			SCE_DBG_PRINTF(0xb, 2196, "Please add test code\n");
			BootTest();
		}
		SceCpuForKernel_FE2B77A3(&bootSaveArea.kblCorelock);
		for (;;)
			;
	}
	sceKernelSetGPO(0x87);
	if (sceKblMain) {
		if (sceKernelCpuId() == 0) {
			SceCpuForKernel_14E0B0C4(0x8000, &pa);
			SceCpuForKernel_14E0B0C4(0x40100000, &pa);
			SceCpuForKernel_14E0B0C4(0x47000000, &pa);
			SceCpuForKernel_14E0B0C4(0xe0000000, &pa);
		}
		sceKblMain(s_bootParam.current);
	}
	SceCpuForKernel_2FF54320(&bootSaveArea.kblCorelock, 0);
	sceKernelCpuId();
	SceCpuForKernel_FE2B77A3(&bootSaveArea.kblCorelock);
	for (;;)
		;
}

#else

/* marks a megabyte of the first 16 MiB used, unless it already is */
static inline int ReserveMegaASLR(unsigned int mega)
{
	if (s_megaAslrUsed[mega % 0x10] == 1)
		return SKELETON_ERROR;
	s_megaAslrUsed[mega % 0x10] = 1;
	return 0;
}

/* a free megabyte of the first 16 MiB, chosen by the seed */
static inline uintptr_t MapMegaASLR(uint32_t seed)
{
	unsigned int mega = seed % 0x10;
	unsigned int end = mega + 0x10;

	for (; mega < end; mega++) {
		if (ReserveMegaASLR(mega) >= 0)
			return mega << 20;
	}
	SCE_DBG_PRINTF(0xb, 617, "Failed\n");
	for (;;)
		;
}

/* CPU 0: the KBL Param, the console and the boot parameters */
static inline uint32_t BootCpu0Init(void *lr)
{
	SceKernelBootParam *param = &s_bootParam.param;
	SceKernelBootParam *current;
	uint32_t *p;
	uint32_t kermit;
	uint32_t aslr = 0;
	unsigned int i;
	unsigned int n;

	LedModuleStart();
	sceKernelSetGPO(BOOT_GPO(1));
	UnmapKbl();
	for (p = _fbss; p < _end; p++)
		*p = 0;
	param->socRevision = PERVASIVE_MISC->revision;
	param->socCacheStatus = PERVASIVE_MISC->cacheStatus;
	memcpy(&s_kblParam, (void *)(SCE_KBL_PARAM_ADDRESS + 0x40200000), sizeof(s_kblParam));
	param->kblParam = &s_kblParam;
	DipswSetState(&s_kblParam.dipsw);
	InitializeDebugPrintLevel();
	InitializeDebugBuffer();
	if (sceKernelCheckDipsw(DIPSW_UART1_CONSOLE) || sceKernelCheckDipsw(DIPSW_UART0_CONSOLE) ||
	    sceKernelCheckDipsw(DIPSW_CONSOLE_LOGGING)) {
		SerialInit(sceKernelCheckDipsw(DIPSW_UART0_CONSOLE));
		sceKernelSetGPO(BOOT_GPO(2));
		SceDebugForKernel_22546577((uintptr_t)SerialDebugPutchar, 0);
	}
	SCE_PRINTF_WITH_INFO_FUNC(0, 1868, "boot", "===================================================\n");
	sceKernelPrintfLevel(QA_LEVEL(0, 2),
			      "Starting PSP2 Kernel Boot Loader (Non-secure) [0x%08x]: %d\n",
			      FW_VERSION, KblGetBuildNumber());
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1897, "boot", "\n");

	kermit = param->socRevision & 0x1ffff;
	s_bootParam.current = param;
	param->kermitRevision = kermit;
	param->magic = SCE_KERNEL_BOOT_PARAM_MAGIC;
	param->corelock = &s_kblCorelock;
	param->size = sizeof(*param);
	param->secure = 0;
	if (kermit == 0)
		SCE_DBG_ASSERT_CHECK_LR_FUNC(0, 1932, "boot", lr);

	/* the CP's boot switches */
	sceKernelPrintfLevel(QA_LEVEL(1, 2), "\tBOOTSW\n");
	for (i = 0; i < 8; i++) {
		sceKernelPrintfLevel(QA_LEVEL(1, 2), "\t\t%d: 0x%08x", i, s_kblParam.dipswWords[i]);
		switch (i) {
		case 1:
			sceKernelPrintfLevel(QA_LEVEL(1, 2), ": CP bid & version\n");
			break;
		case 2:
			sceKernelPrintfLevel(QA_LEVEL(1, 2), ": CP time\n");
			break;
		case 3:
			sceKernelPrintfLevel(QA_LEVEL(1, 2), ": ASLR\n");
			aslr = s_kblParam.dipswWords[i];
			break;
		case 4:
			sceKernelPrintfLevel(QA_LEVEL(1, 2), ": SDK(SCE)\n");
			break;
		case 5:
			sceKernelPrintfLevel(QA_LEVEL(1, 2), ": SHELL\n");
			break;
		case 6:
			sceKernelPrintfLevel(QA_LEVEL(1, 2), ": debug control\n");
			break;
		case 7:
			sceKernelPrintfLevel(QA_LEVEL(1, 2), ": system control\n");
			break;
		default:
			sceKernelPrintfLevel(QA_LEVEL(1, 2), ": CP time\n");
			break;
		}
	}

	/* the kernel's DRAM: the main LPDDR2 above the boot loaders, CDRAM, the TOOL bank */
	current = s_bootParam.current;
	current->numMemory = 3;
	current->memory[0].pbase = 0x40200000;
	if (sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART))
		current->memory[0].psize = 0x3fe00000;
	else
		current->memory[0].psize = 0x1fe00000;
	current->memory[1].pbase = 0x20000000;
	current->memory[1].psize = 0x8000000;
	current->memory[2].pbase = 0x80000000;
	current->memory[2].psize = 0x40000000;
	for (n = 0; n < s_bootParam.current->numMemory; n++)
		sceKernelPrintfLevel(QA_LEVEL(1, 2), "\tmemory[%d] base: 0x%08x-0x%08x [0x%08x]\n",
				      n, s_bootParam.current->memory[n].pbase,
				      s_bootParam.current->memory[n].pbase +
				      s_bootParam.current->memory[n].psize - 1,
				      s_bootParam.current->memory[n].psize);
	sceKernelPrintfLevel(QA_LEVEL(1, 2), "\twakeupFactor: 0x%08x\n",
			      s_kblParam.wakeup_factor);
	sceKernelPrintfLevel(QA_LEVEL(1, 2), "\thardwareInfo: 0x%08x\n",
			      s_kblParam.hardware_info);
	SetHardwareDependent(param);

	sceKernelSetGPO(BOOT_GPO(3));
	PervasiveInit();
	sceKernelSetGPO(BOOT_GPO(4));
	return aslr;
}

void boot(void)
{
	void *lr = __builtin_return_address(0);
	SceKernelBootParam *param;
	SceUIntPtr pa;
	uint32_t aslr;
	uintptr_t mega;
	unsigned int i;
	int cpu;

	SceCpuForKernel_2FF54320(&s_bootCorelock, 0);
	if (sceKernelCpuId() != 0)
		aslr = 0;
	else
		aslr = BootCpu0Init(lr);

	writeCsselr(0);
	writeTeecr(1);
	writeTeehbr(8);
	writeJoscr(1);
	writeVbar((uintptr_t)_start);
	writeTpidrurw(0);
	writeTpidruro(0);
	writeTpidrprw(0);
	SceCpuForKernel_FE2B77A3(&s_bootCorelock);
	if (BootTestEarly) {
		SceCpuForKernel_2FF54320(&s_kblCorelock, 0);
		if (sceKernelCpuId() == 0)
			BootTestEarly();
		SceCpuForKernel_FE2B77A3(&s_kblCorelock);
	}

	sceKernelSetGPO(BOOT_GPO(5));
	param = s_bootParam.current;
	cpu = sceKernelCpuId();
	param->cpu[cpu].ttbcr = 2;
	param->cpu[cpu].dacr = DACR_ALL_CLIENT;
	param->cpu[cpu].contextId = 0xf7;
	writeDacr(DACR_ALL_CLIENT);
	SceCpuForKernel_2FF54320(&s_bootCorelock, 0);
	if (cpu == 0) {
		uint32_t seed = aslr;
		param->ttbr0MaxAddress = 0x3fffffff;
		for (i = 0; i < sizeof(s_aslrUsed); i++)
			s_aslrUsed[i] = 0;
		for (i = 0; i < sizeof(s_megaAslrUsed); i++)
			s_megaAslrUsed[i] = 0;
		ReserveASLR(0, 0x4000);
		ReserveMegaASLR(0);

		param->reset.name = "SceKernelReset";
		param->reset.pbase = param->memory[0].pbase;
		param->reset.vbase = 0;
		param->reset.psize = 0x1000;
		param->reset.vsize = 0x4000;
		param->reset.extraHigh = 0x3000;

		seed = ror5(seed);
		param->l2PageTable000.name = "SceKernelL2PageTable000";
		param->l2PageTable000.pbase = param->memory[0].pbase + 0x1000;
		param->l2PageTable000.vbase = MapASLR(0x1000, seed);
		param->l2PageTable000.psize = 0x1000;
		param->l2PageTable000.vsize = 0x2000;
		param->l2PageTable000.extraHigh = 0x1000;
		param->l2PageTable000MapBase = 0;

		seed = ror5(seed);
		param->sysroot.name = "SceSysroot";
		param->sysroot.pbase = param->memory[0].pbase + 0x4000;
		param->sysroot.vbase = MapASLR(0x4000, seed);
		param->sysroot.psize = 0x4000;
		param->sysroot.vsize = 0x4000;
		param->sysroot.extraHigh = 0;

		seed = ror5(seed);
		param->ttbr0.name = "SceKernelTTBR0";
		param->ttbr0.pbase = param->memory[0].pbase + 0x8000;
		param->ttbr0.vbase = MapASLR(0x4000, seed);
		param->ttbr0.psize = 0x4000;
		param->ttbr0.vsize = 0x4000;
		param->sizeTTBR0Address = 0x40000000;

		seed = ror5(seed);
		param->ttbr1.name = "SceKernelTTBR1";
		param->ttbr1.pbase = param->memory[0].pbase + 0xc000;
		param->ttbr1.vbase = MapASLR(0x4000, seed);
		param->ttbr1.psize = 0x4000;
		param->ttbr1.vsize = 0x4000;
		param->sizeTTBR1Address = 0xc0000000;

		seed = ror5(seed);
		param->fixedHeap32B.name = "SceKernelFixedHeap32B";
		param->fixedHeap32B.vbase = MapASLR(0x10000, seed);
		param->fixedHeap32B.psize = 0x10000;
		param->fixedHeap32B.vsize = 0x10000;
		param->fixedHeap32B.extraHigh = 0;

		seed = ror5(seed);
		param->fixedHeap48B.name = "SceKernelFixedHeap48B";
		param->fixedHeap48B.vbase = MapASLR(0x10000, seed);
		param->fixedHeap48B.psize = 0x10000;
		param->fixedHeap48B.vsize = 0x10000;
		param->fixedHeap48B.extraHigh = 0;

		seed = ror5(seed);
		param->fixedHeap64B.name = "SceKernelFixedHeap64B";
		param->fixedHeap64B.vbase = MapASLR(0x10000, seed);
		param->fixedHeap64B.psize = 0x10000;
		param->fixedHeap64B.vsize = 0x10000;
		param->fixedHeap64B.extraHigh = 0;

		seed = ror5(seed);
		param->fixedHeapUIDEntry.name = "SceKernelFixedHeapUIDEntry";
		param->fixedHeapUIDEntry.vbase = MapASLR(0x10000, seed);
		param->fixedHeapUIDEntry.psize = 0x10000;
		param->fixedHeapUIDEntry.vsize = 0x10000;
		param->fixedHeapUIDEntry.extraHigh = 0;

		seed = ror5(seed);
		param->exceptionEntry.name = "SceKernelExceptionEntry";
		param->exceptionEntry.pbase = param->memory[0].pbase;
		param->exceptionEntry.vbase = MapASLR(0x2000, seed);
		param->exceptionEntry.psize = 0x1000;
		param->exceptionEntry.vsize = 0x2000;
		param->exceptionEntry.extraHigh = 0x1000;

		seed = ror5(seed);
		param->fixedHeapForL2Object.name = "SceKernelFixedHeapForL2Object";
		param->fixedHeapForL2Object.vbase = MapASLR(0x2000, seed);
		param->fixedHeapForL2Object.psize = 0x1000;
		param->fixedHeapForL2Object.vsize = 0x2000;
		param->fixedHeapForL2Object.extraHigh = 0x1000;

		seed = ror5(seed);
		param->l2Vector.name = "SceKernelL2Vector";
		param->l2Vector.pbase = param->memory[0].pbase + 0x10000;
		param->l2Vector.vbase = MapASLR(0x8000, seed);
		param->l2Vector.psize = 0x4000;
		param->l2Vector.vsize = 0x8000;
		param->l2Vector.extraHigh = 0x4000;

		seed = ror5(seed);
		mega = MapMegaASLR(seed);
		param->phypage.name = "SceKernelPhyPageTable";
		param->phypage.pbase = param->memory[0].pbase + 0x80000;
		param->phypage.vbase = mega;
		param->phypage.psize = 0x80000;
		param->phypage.vsize = 0x80000;
		param->phypage.extraHigh = 0;

		param->phypageHigh.name = "SceKernelPhyPageTableHigh";
		param->phypageHigh.pbase = 0x77f00000;
		param->phypageHigh.vbase = mega + 0x80000;
		param->phypageHigh.psize = 0x80000;
		param->phypageHigh.vsize = 0x80000;
		param->phypageHigh.extraHigh = 0;

		param->bootKernelImage.name = "SceBootKernelImage";
		param->bootKernelImage.pbase = KBL_IMAGE_ADDRESS;
		param->bootKernelImage.vbase = KBL_IMAGE_ADDRESS;
		param->bootKernelImage.psize = KBL_IMAGE_SIZE;
		param->bootKernelImage.vsize = 0x2000000;
		param->bootKernelImage.extraHigh = 0x1000000;

		param->hardwareRegisters.name = NULL;
		param->hardwareRegisters.pbase = 0xe0000000;
		param->hardwareRegisters.vbase = 0xe0000000;
		param->hardwareRegisters.psize = 0x8000000;

		sceKernelMMUL1Init((uint32_t *)param->ttbr0.pbase);
		sceKernelMMUL1Init((uint32_t *)param->ttbr1.pbase);
		MapSections(param, BOOT_MAP_CODE, param->bootKernelImage.vbase,
			    param->bootKernelImage.psize, param->bootKernelImage.pbase);
		MapSections(param, BOOT_MAP_REGISTERS, param->hardwareRegisters.vbase,
			    param->hardwareRegisters.psize, param->hardwareRegisters.pbase);
		MapSections(param, BOOT_MAP_REGISTERS, CPU_PRIVATE_BASE, 0x100000, CPU_PRIVATE_BASE);

		sceKernelMMUSetL2PageTable(param->l2PageTable000MapBase <= param->ttbr0MaxAddress ?
					   (uint32_t *)param->ttbr0.pbase :
					   (uint32_t *)param->ttbr1.pbase,
					   BOOT_DOMAIN_KERNEL, param->l2PageTable000MapBase,
					   param->l2PageTable000.pbase);
		sceKernelMMUL2Init((uint32_t *)param->l2PageTable000.pbase);
		sceKernelMMUMapSmallPage((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					 param->l2PageTable000.vbase,
					 param->l2PageTable000.vbase - param->l2PageTable000MapBase,
					 param->l2PageTable000.pbase);
		sceKernelMMUMapSmallPages((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					  param->sysroot.vbase,
					  param->sysroot.vbase - param->l2PageTable000MapBase, 0x4000,
					  param->sysroot.pbase);
		sceKernelMMUMapSmallPages((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					  param->ttbr0.vbase,
					  param->ttbr0.vbase - param->l2PageTable000MapBase, 0x4000,
					  param->ttbr0.pbase);
		sceKernelMMUMapSmallPages((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					  param->ttbr1.vbase,
					  param->ttbr1.vbase - param->l2PageTable000MapBase, 0x4000,
					  param->ttbr1.pbase);
		sceKernelMMUMapSmallPage((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_EXCEPTION,
					 param->exceptionEntry.vbase,
					 param->exceptionEntry.vbase - param->l2PageTable000MapBase,
					 param->exceptionEntry.pbase);
		sceKernelMMUMapSmallPages((uint32_t *)param->l2PageTable000.pbase, BOOT_MAP_DATA,
					  param->l2Vector.vbase,
					  param->l2Vector.vbase - param->l2PageTable000MapBase, 0x4000,
					  param->l2Vector.pbase);

		/* the physical page tables: large pages in the next L2 table */
		sceKernelMMUSetL2PageTable(param->phypage.vbase <= param->ttbr0MaxAddress ?
					   (uint32_t *)param->ttbr0.pbase :
					   (uint32_t *)param->ttbr1.pbase,
					   BOOT_DOMAIN_KERNEL, param->phypage.vbase,
					   param->l2PageTable000.pbase + 0x400);
		sceKernelMMUL2Init((uint32_t *)(param->l2PageTable000.pbase + 0x400));
		sceKernelMMUMapLargePages((uint32_t *)(param->l2PageTable000.pbase + 0x400),
					  BOOT_MAP_DATA, param->phypage.vbase, 0,
					  param->phypage.psize, param->phypage.pbase);
		if (sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART))
			sceKernelMMUMapLargePages((uint32_t *)(param->l2PageTable000.pbase + 0x400),
						  BOOT_MAP_DATA, param->phypageHigh.vbase,
						  0x80000, param->phypageHigh.psize,
						  param->phypageHigh.pbase);
	}
	SceCpuForKernel_FE2B77A3(&s_bootCorelock);
	writeTtbr0(SceCpuForKernel_9818A20B(param->ttbr0.pbase));
	writeTtbr1(SceCpuForKernel_9818A20B(param->ttbr1.pbase));
	writeTtbcr(param->cpu[cpu].ttbcr);
	writeDacr(DACR_ALL_CLIENT);
	writeContextId(param->cpu[cpu].contextId);

	/* the kernel's vector page: two copies of this one */
	SceCpuForKernel_2FF54320(&s_bootCorelock, 0);
	if (sceKernelCpuId() == 0) {
		void *page = memcpy((void *)param->memory[0].pbase, _start, 0x100);

		memcpy((char *)page + 0x100, _start, 0x100);
		param->resetVector = param->exceptionEntry.vbase + 0x100;
	}
	SceCpuForKernel_FE2B77A3(&s_bootCorelock);
	SceCpuForKernel_2FF54320(&s_bootCorelock, 0);
	SceCpuForKernel_FE2B77A3(&s_bootCorelock);
	CpuSctlrInit();
	SceCpuForKernel_2FF54320(&s_bootCorelock, 0);
	SceCpuForKernel_FE2B77A3(&s_bootCorelock);
	SceCpuForKernel_2FF54320(&s_kblCorelock, 0);
	SceCpuForKernel_FE2B77A3(&s_kblCorelock);
	writeVbar(param->resetVector);
	MMUSetReady();
	SceCpuForKernel_2FF54320(&s_kblCorelock, 0);
	sceKernelCpuId();
	SceCpuForKernel_FE2B77A3(&s_kblCorelock);

	sceKernelSetGPO(BOOT_GPO(6));
	if (BootTest) {
		SceCpuForKernel_2FF54320(&s_kblCorelock, 0);
		if (sceKernelCpuId() == 0) {
			SCE_DBG_PRINTF(0xb, 2196, "Please add test code\n");
			BootTest();
		}
		SceCpuForKernel_FE2B77A3(&s_kblCorelock);
		for (;;)
			;
	}
	sceKernelSetGPO(BOOT_GPO(7));
	if (sceKblMain) {
		if (sceKernelCpuId() == 0) {
			SceCpuForKernel_14E0B0C4(0x8000, &pa);
			SceCpuForKernel_14E0B0C4(0x40100000, &pa);
			SceCpuForKernel_14E0B0C4(0x47000000, &pa);
			SceCpuForKernel_14E0B0C4(0xe0000000, &pa);
		}
		sceKblMain(s_bootParam.current);
	}
	SceCpuForKernel_2FF54320(&s_kblCorelock, 0);
	sceKernelCpuId();
	SceCpuForKernel_FE2B77A3(&s_kblCorelock);
	for (;;)
		;
}

#endif /* BOOT_NSKBL */

/*
 * The boot exception handlers: each reports itself on the GPO LEDs, prints
 * what the debug firmware can tell about the exception and stops. The
 * register reads stay in the release firmwares, whose prints are compiled
 * out; lr is the exception's return address.
 */
void boot_und_handler(uint32_t lr)
{
	sceKernelSetGPO(BOOT_GPO(0xa));
	SCE_DBG_PRINTF(0xb, 2252, "(***** Undefined Instruction Handler *****\n");
	SCE_DBG_PRINTF(0xb, 2253, "LR   0x%08x\n", lr);
	SCE_DBG_PRINTF(0, 2254, "STOPPED\n");
	for (;;)
		;
}

void boot_svc_handler(uint32_t lr)
{
	uint32_t cpsr, spsr;

	sceKernelSetGPO(BOOT_GPO(0xb));
	SCE_DBG_PRINTF(0xb, 2265, "(***** Susperviser Call Handler *****\n");
	SCE_DBG_PRINTF(0xb, 2266, "LR   0x%08x\n", lr);
	cpsr = readCpsr();
	SCE_DBG_PRINTF(0, 2269, "CPSR:0x%08x - ", cpsr);
	spsr = readSpsr();
	SCE_DBG_PRINTF(0, 2272, "SPSR:0x%08x", spsr);
	SCE_DBG_PRINTF(0, 2273, "\n");
	SCE_DBG_PRINTF(0, 2274, "STOPPED\n");
	for (;;)
		;
}

void boot_prefetch_abort_handler(uint32_t lr)
{
	uint32_t ifsr, dfar, ifar;

	sceKernelSetGPO(BOOT_GPO(0xc));
	SCE_DBG_PRINTF(0xb, 2285, "(***** Prefetch Abort Handler *****\n");
	SCE_DBG_PRINTF(0xb, 2286, "LR   0x%08x\n", lr);
	ifsr = readIfsr();
	SCE_DBG_PRINTF(0xb, 2288, "IFSR 0x%08x\n", ifsr);
	dfar = readDfar();
	SCE_DBG_PRINTF(0xb, 2290, "DFAR 0x%08x\n", dfar);
	ifar = readIfar();
	SCE_DBG_PRINTF(0xb, 2292, "IFAR 0x%08x\n", ifar);
	SCE_DBG_PRINTF(0, 2293, "STOPPED\n");
	for (;;)
		;
}

void boot_data_abort_handler(uint32_t lr)
{
	uint32_t dfsr;

	sceKernelSetGPO(BOOT_GPO(0xd));
	SCE_DBG_PRINTF(0xb, 2304, "***** Data Abort Handler *****\n");
	SCE_DBG_PRINTF(0xb, 2305, "LR   0x%08x\n", lr);
	SCE_DBG_PRINTF(0xb, 2306, "()\n");
	dfsr = readDfsr();
	SCE_DBG_PRINTF(0xb, 2308, "DFSR:        0x%08x\n", dfsr);
	SCE_DBG_PRINTF(0xb, 2310, "DFSR DOMAIN: 0x%x\n", (dfsr & 0xf0) >> 4);
	SCE_DBG_PRINTF(0xb, 2312, "DFAR:        0x%08x\n", readDfar());
	SCE_DBG_PRINTF(0xb, 2313, "IFAR:        0x%08x\n", readIfar());
	SCE_DBG_PRINTF(0, 2314, "STOPPED\n");
	for (;;)
		;
}

void boot_irq_handler(void)
{
	sceKernelSetGPO(BOOT_GPO(0xe));
	SCE_DBG_PRINTF(0xb, 2322, "()\n");
	SCE_DBG_PRINTF(0, 2323, "STOPPED\n");
	for (;;)
		;
}

void boot_fiq_handler(void)
{
	sceKernelSetGPO(BOOT_GPO(0xf));
	SCE_DBG_PRINTF(0xb, 2331, "()\n");
	SCE_DBG_PRINTF(0, 2332, "STOPPED\n");
	for (;;)
		;
}

#ifndef BOOT_NSKBL

/* the monitor's SMC entry: reports the call and returns 0 */
int monitor_call_handler(uint32_t code)
{
	uint32_t cpsr, spsr;

	SCE_DBG_PRINTF(0xb, 2345, "(0x%08x) -", code);
	cpsr = readCpsr();
	SCE_DBG_PRINTF(0, 2347, "CPSR:0x%08x - ", cpsr);
	spsr = readSpsr();
	SCE_DBG_PRINTF(0, 2349, "SPSR:0x%08x", spsr);
	SCE_DBG_PRINTF(0, 2350, "\n");
	SCE_DBG_PRINTF(0xb, 2351, "SCR => 0x%08x\n", readScr());
	return 0;
}

/*
 * names unknown: monitor.S's exception entries, which report the exception
 * as the boot handlers do
 */
void monitor_prefetch_abort_handler(uint32_t lr)
{
	boot_prefetch_abort_handler(lr);
}

void monitor_data_abort_handler(uint32_t lr)
{
	boot_data_abort_handler(lr);
}

void monitor_irq_handler(void)
{
	boot_irq_handler();
}

void monitor_fiq_handler(void)
{
	boot_fiq_handler();
}

#else

/* name unknown: the build number boot() prints */
__attribute__((noinline)) int KblGetBuildNumber(void)	/* boot() calls it whole */
{
	return 300;
}

#endif
