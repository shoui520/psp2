/*
 * SceSysmem: the debug LEDs and the general purpose inputs/outputs. The GPO
 * value goes to the LED register block through a mask; debug tools register
 * the LED handlers.
 */

#include <psp2kern/kernel/sysclib.h>
#include "sysmem_internal.h"
#include "sce/suspend.h"

#define LED_REG_BASE			0xe20a0000
#define LED_GPO_SHIFT			16
#define LED_GPO_BITS			0xff0000
#define LED_SUSPEND_SLOT		5

typedef struct LedRegs {
	SceUInt32 reserved[2];
	SceUInt32 set;				/* 0x08 */
	SceUInt32 clear;			/* 0x0c */
} LedRegs;

int ksceKernelCpuId(void);
SceInt32 ksceKernelAtomicCompareAndSet32(SceInt32 *store, SceInt32 value, SceInt32 new_value);

/* the registers' physical address until LedModuleStart maps them */
static volatile LedRegs *s_ledRegs = (volatile LedRegs *)0xe20a0000;
static volatile SceUInt32 s_gpo = 0xff;
/* the GPO bits that reach the LEDs */
static SceInt32 s_gpoMask = 0xff;
static SceUInt32 s_gpi;
static void (*s_ledHandle0)(int a1, int a2, int a3, int a4);
static void (*s_ledHandle1)(int a1, int a2, int a3, int a4);

static inline void LedWriteGPO(SceUInt32 gpo)
{
	volatile LedRegs *regs = s_ledRegs;
	SceUInt32 bits = (s_gpoMask & gpo) << LED_GPO_SHIFT;

	regs->clear = ~bits & LED_GPO_BITS;
	regs->set = bits & LED_GPO_BITS;
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

int ksceKernelSetGPOMask(SceUInt32 clear, SceUInt32 set)
{
	SceInt32 mask;

	do {
		mask = (s_gpoMask & ~clear) | set;
		ksceKernelAtomicCompareAndSet32(&s_gpoMask, s_gpoMask, mask);
	} while (mask != s_gpoMask);
	return 0;
}

SceUInt32 ksceKernelGetGPO(void)
{
	return s_gpo;
}

void ksceKernelSetGPI(SceUInt32 gpi)
{
	s_gpi = gpi;
}

SceUInt32 sceKernelGetGPI(void)
{
	return s_gpi;
}

void ksceDebugLedRegisterHandle1(void (*handle)(int a1, int a2, int a3, int a4))
{
	s_ledHandle1 = handle;
}

void ksceDebugLedRegisterHandle0(void (*handle)(int a1, int a2, int a3, int a4))
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

struct SysmemBootContext;

/* the LED register block, mapped once (CPU 0) */
int LedModuleStart(const struct SysmemBootContext *context)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	SceUID uid;

	if (ksceKernelCpuId() != 0)
		return 0;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.paddr = LED_REG_BASE;
	opt.attr = 2;
	uid = ksceKernelAllocMemBlock("SceLedReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW, 0x10000, &opt);
	if (uid < 0)
		SCE_PANIC(218);
	ksceKernelGetMemBlockBase(uid, &base);
	s_ledRegs = base;
	SceKernelSuspendForDriver_CE7A2207(LED_SUSPEND_SLOT, LedSuspend, NULL);
	SceKernelSuspendForDriver_105C5752(LED_SUSPEND_SLOT, LedResume, NULL);
	return 0;
}
