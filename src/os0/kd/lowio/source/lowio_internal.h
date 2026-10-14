/* SceLowio internal hardware mappings. Private helper names are inferred. */
#pragma once

#include <stdint.h>
#include <psp2kern/kernel/cpu.h>

extern uint8_t *s_misc, *s_video, *s_mailbox, *s_pervasive2;
extern uint8_t *s_reset, *s_gate, *s_uartClock, *s_baseClock;
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
uint32_t Smc111(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t Smc113(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t Smc114(uint32_t, uint32_t, uint32_t, uint32_t);

uint32_t ScePervasiveForDriver_4AF7A01E(unsigned head, unsigned mask);
