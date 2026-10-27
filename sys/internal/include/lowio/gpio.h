/*
 * SceLowio (os0:kd/lowio.skprx): SceGpioForDriver, the GPIO ports' exports, each declared once
 * with its definition as the prototype.
 */
#pragma once

#include <scetypes.h>
#include <stdint.h>

int SceGpioForDriver_010DC295(unsigned bank, unsigned port);	/* vitasdk: ksceGpioQueryIntr */
int SceGpioForDriver_129DF5AC(unsigned bank, unsigned port);	/* vitasdk: ksceGpioPortRead */
int SceGpioForDriver_35AAD77A(unsigned bank, unsigned port);	/* vitasdk: ksceGpioAcquireIntr */
/* vitasdk: ksceGpioSetPortMode */
int SceGpioForDriver_372022A4(unsigned bank, unsigned port, unsigned mode);
int SceGpioForDriver_A1B5A462(unsigned bank, unsigned port);	/* vitasdk: ksceGpioPortReset */
/* vitasdk: ksceGpioSetIntrMode */
int SceGpioForDriver_BBEA1DDC(unsigned bank, unsigned port, unsigned mode);
int SceGpioForDriver_C4CB78E2(unsigned bank, unsigned port);	/* vitasdk: ksceGpioGetPortMode */
int sceGpioPortSet(unsigned bank, unsigned port);
int SceGpioForDriver_E97A3B31(unsigned bank, unsigned port);	/* vitasdk: ksceGpioGetIntrMode */
int sceGpioPortClear(unsigned bank, unsigned port);
