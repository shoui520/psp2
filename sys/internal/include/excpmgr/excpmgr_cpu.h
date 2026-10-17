/*
 * Coprocessor register access for the exception manager (os0:kd/excpmgr.skprx
 * and the secure-world slb2:kbl/SceExcpmgr): the CP15/CP14 accessors, and the
 * VFP system registers.
 */
#pragma once

#include <sysmem/cp15.h>

#include <sysmem/barrier.h>

/* the current program status register; I: IRQs masked */
#define PSR_I_BIT	0x00000080

static inline unsigned int readCpsr(void)
{
	unsigned int value;

	__asm__ volatile("mrs %0, cpsr" : "=r" (value));
	return value;
}

/* the saved program status register of the current mode */
static inline unsigned int readSpsr(void)
{
	unsigned int value;

	__asm__ volatile("mrs %0, spsr" : "=r" (value));
	return value;
}

/* CPSR's control field (mode and interrupt masks), as a saved CPSR has it */
static inline void writeCpsrControl(unsigned int value)
{
	__asm__ volatile("msr cpsr_c, %0" :: "r" (value) : "memory");
}

/* VFP system registers (no VFP data registers are touched by the C code) */
static inline unsigned int readFpexc(void)
{
	unsigned int value;

	__asm__ volatile(".fpu neon\n\tvmrs %0, fpexc" : "=r" (value) :: "memory");
	return value;
}

/* VFP identification */
static inline unsigned int readFpsid(void)
{
	unsigned int value;

	__asm__ volatile(".fpu neon\n\tvmrs %0, fpsid" : "=r" (value) :: "memory");
	return value;
}

static inline unsigned int readMvfr0(void)
{
	unsigned int value;

	__asm__ volatile(".fpu neon\n\tvmrs %0, mvfr0" : "=r" (value) :: "memory");
	return value;
}

static inline unsigned int readMvfr1(void)
{
	unsigned int value;

	__asm__ volatile(".fpu neon\n\tvmrs %0, mvfr1" : "=r" (value) :: "memory");
	return value;
}

static inline void writeFpexc(unsigned int value)
{
	__asm__ volatile(".fpu neon\n\tvmsr fpexc, %0" :: "r" (value) : "memory");
}

/* FPSCR: flush-to-zero and default-NaN modes */
#define FPSCR_FZ	0x01000000
#define FPSCR_DN	0x02000000

static inline unsigned int readFpscr(void)
{
	unsigned int value;

	__asm__ volatile(".fpu neon\n\tvmrs %0, fpscr" : "=r" (value) :: "memory");
	return value;
}

static inline void writeFpscr(unsigned int value)
{
	__asm__ volatile(".fpu neon\n\tvmsr fpscr, %0" :: "r" (value) : "memory");
}
