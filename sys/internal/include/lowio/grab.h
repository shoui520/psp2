/*
 * SceLowio (os0:kd/lowio.skprx): SceGrabForDriver, the grabber's exports, each declared once with
 * its definition as the prototype.
 */
#pragma once

#include <scetypes.h>
#include <stdint.h>

uint32_t SceGrabForDriver_072B8D93(uint32_t mode);
uint32_t SceGrabForDriver_188BBCC8(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t SceGrabForDriver_1F292554(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t SceGrabForDriver_5F36ABC4(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t SceGrabForDriver_379130B2(void);
uint32_t SceGrabForDriver_B22EE8BC(uint32_t mask, uint32_t enable);
uint32_t SceGrabForDriver_E9C25A28(uint32_t bank, uint32_t paddr);
uint32_t SceGrabForDriver_F13C63DD(void);
