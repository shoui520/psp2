/* SceLowio PWM services (ScePwmForDriver) not declared by vitasdk. */
#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int ScePwmForDriver_13161CCC(unsigned int channel, uint32_t flags, uint32_t period, uint32_t duty);

int ScePwmForDriver_0374213C(unsigned int channel, uint16_t *periodOut, uint16_t *dutyOut, uint32_t *levelOut);
int ScePwmForDriver_22C17D24(unsigned int channel);
int ScePwmForDriver_514773B1(unsigned int channel, uint32_t period, uint32_t duty);
int ScePwmForDriver_99911920(unsigned int channel);
