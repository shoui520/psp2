/* SceLowio internal hardware mappings. Private helper names are inferred. */
#pragma once

#include <stdint.h>
#include <psp2kern/kernel/cpu.h>
#include <sce/pervasive_regs.h>

extern uint8_t *s_misc, *s_video, *s_mailbox, *s_pervasive2;
extern ScePervasiveResetRegisters *s_reset;
extern ScePervasiveGateRegisters *s_gate;
extern uint8_t *s_uartClock, *s_baseClock;
extern uint8_t *s_tas[8];
extern SceKernelSpinlock s_pervasiveLock;

static inline void LowioDmb(void)
{
	__asm__ volatile("dmb sy" : : : "memory");
}

static inline void LowioDsb(void)
{
	__asm__ volatile("dsb sy" : : : "memory");
}

uint32_t Smc110(uint32_t, uint32_t, uint32_t, uint32_t);
int Smc111(volatile uint32_t *reg, int value);
int Smc113(volatile uint32_t *reg, int value);
uint32_t Smc114(uint32_t, uint32_t, uint32_t, uint32_t);

int ScePervasiveForDriver_4AF7A01E(int port, unsigned int mask);
