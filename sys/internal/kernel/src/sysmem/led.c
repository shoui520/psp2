/*
 * SceSysmem: the debug LEDs and the general purpose inputs/outputs. The GPO
 * value goes to the LED register block through a mask; debug tools register
 * the LED handlers. The secure kernel's SceSysmem (SYSMEM_TZS) only maps the
 * registers; the boot loaders (SYSMEM_SKBL, SYSMEM_NSKBL) write them at their
 * physical address.
 */

#include <sysmem/sysclib.h>
#include "sysmem_internal.h"
#include <sysmem/suspend.h>
#include <lowio/gpio_regs.h>
#include <lowio/pervasive.h>

#define LED_GPO_SHIFT			16
#define LED_GPO_BITS			0xff0000
#define LED_SUSPEND_SLOT		5

int sceKernelCpuId(void);

/* the registers' physical address until LedModuleStart maps them */
static volatile SceGpioRegisters *s_ledRegs = (void *)GPIO0_REG_BASE;
#ifndef SYSMEM_TZS
static volatile SceUInt32 s_gpo = 0xff;
/* the GPO bits that reach the LEDs */
static volatile SceInt32 s_gpoMask = 0xff;
static SceUInt32 s_gpi;
static void (*s_ledHandle0)(int a1, int a2, int a3, int a4);
static void (*s_ledHandle1)(int a1, int a2, int a3, int a4);

static inline void LedWriteGPO(SceUInt32 gpo)
{
	volatile SceGpioRegisters *regs = s_ledRegs;
	SceUInt32 bits = (s_gpoMask & gpo) << LED_GPO_SHIFT;

	regs->outputClear = ~bits & LED_GPO_BITS;
	regs->outputSet = bits & LED_GPO_BITS;
}

/* name unknown: suspend and resume handlers */
static int LedSuspend(void *args)
{
	return 0;
}

int sceKernelSetGPO(SceUInt32 gpo)
{
	s_gpo = gpo;
	LedWriteGPO(gpo);
	return 0;
}

static int LedResume(void *args)
{
	return sceKernelSetGPO(s_gpo);
}

int SceDebugLedForDriver_098473B0(SceUInt32 clear, SceUInt32 set)
{
	SceInt32 old, mask;

	do {
		old = s_gpoMask;
		mask = (~clear & old) | set;
		SceCpuForDriver_CDA96E81(&s_gpoMask, old, mask);
	} while (mask != s_gpoMask);
	return 0;
}

SceUInt32 SceDebugLedForDriver_3BB289F7(void)
{
	return s_gpo;
}

void SceDebugLedForDriver_51C5325A(SceUInt32 gpi)
{
	s_gpi = gpi;
}

SceUInt32 sceKernelGetGPI(void)
{
	return s_gpi;
}

void SceDebugLedForDriver_F62154E7(void (*handle)(int a1, int a2, int a3, int a4))
{
	s_ledHandle1 = handle;
}

void SceDebugLedForDriver_24173819(void (*handle)(int a1, int a2, int a3, int a4))
{
	s_ledHandle0 = handle;
}

void sceDebugLedInvokeHandle1(int a1, int a2, int a3, int a4)
{
	if (s_ledHandle1 != NULL)
		s_ledHandle1(a1, a2, a3, a4);
}

void sceDebugLedInvokeHandle0(int a1, int a2, int a3, int a4)
{
	if (s_ledHandle0 != NULL)
		s_ledHandle0(a1, a2, a3, a4);
}
#endif

#if defined(SYSMEM_SKBL) || defined(SYSMEM_NSKBL)
/* the LED outputs on, before anything is mapped */
int LedModuleStart(void)
{
	ScePervasiveForDriver_2F195C97();
	s_ledRegs->portMode |= LED_GPO_BITS;
	s_ledRegs->enable = LED_GPO_BITS;
	s_ledHandle1 = NULL;
	s_ledHandle0 = NULL;
	return 0;
}
#else
struct SysmemBootContext;

/* the LED register block, mapped once (CPU 0) */
int LedModuleStart(const struct SysmemBootContext *context)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	SceUID uid;

	if (sceKernelCpuId() != 0)
		return 0;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.paddr = GPIO0_REG_BASE;
	opt.attr = 2;
	uid = SceSysmemForDriver_C94850C9("SceLedReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW, 0x10000, &opt);
	if (uid < 0)
		SCE_PANIC(218);
	SceSysmemForDriver_A841EDDA(uid, &base);
	s_ledRegs = base;
#ifndef SYSMEM_TZS
	SceKernelSuspendForDriver_CE7A2207(LED_SUSPEND_SLOT, LedSuspend, NULL);
	SceKernelSuspendForDriver_105C5752(LED_SUSPEND_SLOT, LedResume, NULL);
#endif
	return 0;
}
#endif
