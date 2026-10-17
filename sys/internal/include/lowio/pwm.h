/* SceLowio PWM services (ScePwmForDriver) not declared by vitasdk. */
#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int ScePwmForDriver_13161CCC(unsigned int channel, uint32_t flags, uint32_t period, uint32_t duty);
