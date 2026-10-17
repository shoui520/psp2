/* SceLowio DSI services (SceDsiForDriver) not declared by vitasdk. */
#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int SceDsiForDriver_6F8029A1(unsigned head);
int SceDsiForDriver_F2921E29(unsigned head, unsigned output);
int ksceDsiStartDisplay(unsigned head, unsigned start);
