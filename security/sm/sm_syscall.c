/*
 * sm_syscall.c: Shared MeP software interrupt (swi 0) syscall wrappers,
 * watchdog timer control bus helpers, and __cxa_pure_virtual for os0:sm
 * modules.
 *
 * Internal function names (except __cxa_pure_virtual) are ours.
 */

#include "sm_common.h"

/* Syscall 1: unload module (name ours) */
int smSyscall1(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 1u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* Syscall 2 (name ours) */
int smSyscall2(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 2u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* Syscall 3 (name ours) */
int smSyscall3(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 3u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* Syscall 4: register/unregister interrupt handler (name ours) */
int smSyscall4(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 4u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* Syscall 5: secure NVS access (name ours) */
int smSyscall5(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 5u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* Syscall 6 (name ours) */
int smSyscall6(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 6u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* Syscall 7: panic / stack check failure (name ours) */
int smSyscall7(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 7u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* Syscall 8 (name ours) */
int smSyscall8(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 8u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* Start MeP control-bus timer / watchdog (name ours) */
void smWatchdogStart(void)
{
	smWatchdogStop();
	__asm__ volatile(
		"mov $12, 32\n\t"
		"stcb $12, 0x413\n\t"
		"mov $12, 5\n\t"
		"stcb $12, 0x415\n\t"
		"mov $12, 1\n\t"
		"stcb $12, 0x410\n\t"
		"mov $12, 0\n\t"
		"stcb $12, 0x411\n\t"
		"mov $12, 1\n\t"
		"stcb $12, 0x412"
		::: "$12", "memory");
}

/* Stop MeP control-bus timer / watchdog (name ours) */
void smWatchdogStop(void)
{
	uint32_t enabled;

	__asm__ volatile("ldcb %0, 0x412" : "=r"(enabled));
	if (enabled != 0u) {
		uint32_t zero = 0u;

		__asm__ volatile("stcb %0, 0x412" :: "r"(zero) : "memory");
	}
}

/* Read MeP control-bus timer control register 0x410 (name ours) */
uint32_t smWatchdogGetControl(void)
{
	uint32_t val;

	__asm__ volatile("ldcb %0, 0x410" : "=r"(val));
	return val;
}

/* Read MeP control-bus timer count register 0x415 (name ours) */
uint32_t smWatchdogGetCount(void)
{
	uint32_t val;

	__asm__ volatile("ldcb %0, 0x415" : "=r"(val));
	return val;
}

/* C++ pure virtual call trap */
void __cxa_pure_virtual(void)
{
	for (;;)
		;
}
