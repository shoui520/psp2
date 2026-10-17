/*
 * Secure monitor calls: SMC #0 with the service number in r12, its
 * arguments in r0-r3 (henkaku wiki, SceExcpmgr "SMC calls").
 */
#pragma once

#define SCE_MONCALL_SET_SHARED_MEMORY	0x101	/* Sony's name */
#define MONCALL_SYSCON_SET_POWER_MODE	0x11a	/* sceSysconSetPowerMode(type, mode) */
#define MONCALL_CRASH_DUMP		0x122	/* sceCrashDumpMonitorCall(type, TTBR0, VBAR, sysroot) */

#include <stdint.h>

/* a monitor call with two arguments; the monitor may change r1-r3 */
static inline __attribute__((always_inline))
uint32_t moncall2(uint32_t service, uint32_t a0, uint32_t a1)
{
	register uint32_t r0 __asm__("r0") = a0;
	register uint32_t r1 __asm__("r1") = a1;

	__asm__ volatile(".arch_extension sec\n\tmovw ip, %2\n\tsmc #0"
			 : "+r" (r0), "+r" (r1) : "i" (service) : "r2", "r3", "ip", "memory");
	return r0;
}

/* a monitor call with four arguments */
static inline uint32_t moncall4(uint32_t service, uint32_t a0, uint32_t a1,
				uint32_t a2, uint32_t a3)
{
	register uint32_t r0 __asm__("r0") = a0;
	register uint32_t r1 __asm__("r1") = a1;
	register uint32_t r2 __asm__("r2") = a2;
	register uint32_t r3 __asm__("r3") = a3;
	register uint32_t ip __asm__("ip") = service;

	__asm__ volatile(".arch_extension sec\n\tsmc #0"
			 : "+r" (r0), "+r" (r1), "+r" (r2), "+r" (r3), "+r" (ip)
			 :: "memory", "cc");
	return r0;
}
