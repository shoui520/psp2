/* SceLowio DSI services (SceDsiForDriver) not declared by vitasdk. */
#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int SceDsiForDriver_6F8029A1(unsigned head);
int SceDsiForDriver_F2921E29(unsigned head, unsigned output);
int SceDsiForDriver_C2E85919(unsigned head, unsigned start);

int SceDsiForDriver_114D1413(unsigned head);
int SceDsiForDriver_3FB0DF1F(unsigned head, int command, void *data, unsigned size);
int SceDsiForDriver_4DF9E924(unsigned head);
int SceDsiForDriver_5BE5AA9B(unsigned head);
int SceDsiForDriver_7640F607(unsigned head);
int SceDsiForDriver_78E6E3CF(unsigned head, unsigned lanes, unsigned pixelSize);
int SceDsiForDriver_8610B795(unsigned head, uint32_t numerator, uint32_t denominator);
int SceDsiForDriver_89C00D2F(unsigned head, int parameter0, int parameter1);
int SceDsiForDriver_97BFEA76(unsigned head, unsigned vic);
int SceDsiForDriver_98120684(unsigned head, int parameter, void *data, unsigned size);
int SceDsiForDriver_B3A70C05(unsigned vic, int *width, int *height);
int SceDsiForDriver_BA6BC89F(unsigned head, int command, int parameter);
