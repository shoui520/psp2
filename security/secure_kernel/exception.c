/*
 * exception.c: MeP hardware exception handlers for unexpected reset vector
 * execution, reserved instruction (RI), and division by zero (ZDIV)
 * (0x800260..0x8002bf).
 *
 * Function names are ours; panic semantics recovered from 0.995.070-TOOL
 * ("Secure Kernel Panic: Executing reset vector...", "Secure Block Panic:
 * RI exception at %p", "ZDIV exception at %p").
 */

#include "secure_kernel.h"

/* 0x800260 */
void excResetVectorHandler(void)
{
	uint32_t psw;
	uint32_t epc;
	uint32_t lp;

	__asm__ volatile ("ldc %0, $psw" : "=r"(psw));
	__asm__ volatile ("ldc %0, $epc" : "=r"(epc));
	__asm__ volatile ("ldc %0, $lp" : "=r"(lp));

	skPanic("%08x %08x %08x %08x\n", 20u,
		lp ^ SK_PC_OBFUSCATION_MASK,
		epc ^ SK_PC_OBFUSCATION_MASK,
		psw ^ SK_PC_OBFUSCATION_MASK);
}

/* 0x80028c */
void excReservedInsnHandler(void)
{
	uint32_t epc;

	__asm__ volatile ("ldc %0, $epc" : "=r"(epc));
	skPanic("%08x %08x\n", 21u, epc ^ SK_PC_OBFUSCATION_MASK);
}

/* 0x8002a6 */
void excZeroDivHandler(void)
{
	uint32_t epc;

	__asm__ volatile ("ldc %0, $epc" : "=r"(epc));
	skPanic("%08x %08x\n", 22u, epc ^ SK_PC_OBFUSCATION_MASK);
}
