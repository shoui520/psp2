/*
 * ARMv7 barriers, events and the IRQ mask. Each is one instruction C has no
 * way to express; the "memory" clobber keeps the compiler from moving loads
 * and stores across.
 */
#pragma once

#define isb()	__asm__ volatile("isb sy" ::: "memory")
#define dsb()	__asm__ volatile("dsb sy" ::: "memory")
#define dmb()	__asm__ volatile("dmb sy" ::: "memory")
#define wfe()	__asm__ volatile("wfe" ::: "memory")
#define sev()	__asm__ volatile("sev" ::: "memory")
#define nop()	__asm__ volatile("nop")

#define enableIrq()	__asm__ volatile("cpsie i" ::: "memory")
#define disableIrq()	__asm__ volatile("cpsid i" ::: "memory")
