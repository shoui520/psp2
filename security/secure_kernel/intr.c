/*
 * intr.c: MeP interrupt controller (INTC) handler registration, lookup, and
 * enable-mask save/restore (0x802852..0x802925).
 *
 * Also defines the 64-byte aligned IRQ handler table and CPU context save
 * frames in .bss (0x807f40..0x80815f).
 *
 * Function and variable names are ours.
 */

#include "secure_kernel.h"

/* .bss (0x807f40..0x80815f) */
static uint32_t s_irqHandlerTable[12] __attribute__((aligned(64)));
SceSkCpuContext g_irqCpuContext __attribute__((aligned(64)));
SceSkCpuContext g_cmdIrqCpuContext __attribute__((aligned(64)));
SceSkCpuContext g_kernelCpuContext __attribute__((aligned(64)));
SceSkCpuContext g_swiCpuContext __attribute__((aligned(64)));

/* 0x802852 */
void skSetIrqMaskRaw(uint32_t mask)
{
	__asm__ volatile (
		"stcb %0, 0x2\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop"
		:
		: "r"(mask)
	);
}

/* 0x80285e */
void skRegisterIrqHandler(uint32_t irqNum, uint32_t handlerAddr)
{
	uint32_t psw;
	uint32_t ier;

	__asm__ volatile ("ldc %0, $psw" : "=r"(psw));
	if ((psw & 1u) != 0u) {
		setPanicArmInterruptCode(0x00200000u);
		skPanic("%08x\n", 14u);
	}

	if (irqNum >= 12u) {
		setPanicArmInterruptCode(0x00200000u);
		skPanic("%08x\n", 34u);
	}

	s_irqHandlerTable[irqNum] = handlerAddr;
	__asm__ volatile ("ldcb %0, 0x2" : "=r"(ier));
	if (handlerAddr == 0u)
		ier &= ~(1u << (irqNum & 0x1fu));
	else
		ier |= (1u << (irqNum & 0x1fu));

	skSetIrqMaskRaw(ier);
}

/* 0x8028d4 */
uint32_t skGetIrqHandler(uint32_t irqNum)
{
	if (irqNum >= 12u) {
		setPanicArmInterruptCode(0x00200000u);
		skPanic("%08x\n", 35u);
	}
	return s_irqHandlerTable[irqNum];
}

/* 0x802908 */
uint32_t skSaveIrqMask(void)
{
	uint32_t ier;

	__asm__ volatile ("ldcb %0, 0x2" : "=r"(ier));
	return ier & 0x0e7cu;
}

/* 0x802912 */
void skRestoreIrqMask(uint32_t savedMask)
{
	uint32_t ier;

	__asm__ volatile ("ldcb %0, 0x2" : "=r"(ier));
	ier = (ier & ~0x0e7cu) | (savedMask & 0x0e7cu);
	skSetIrqMaskRaw(ier);
}
