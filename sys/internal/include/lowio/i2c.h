/*
 * SceLowio (os0:kd/lowio.skprx): SceI2cForDriver, the I2C buses' exports, each declared once with
 * its definition as the prototype.
 */
#pragma once

#include <scetypes.h>
#include <stdint.h>

int sceI2cMasterTransmitReceive(unsigned bus, unsigned writeAddress, const uint8_t *writeBuffer, unsigned writeSize, unsigned readAddress, uint8_t *readBuffer, unsigned readSize);
int SceI2cForDriver_30CF9469(unsigned bus, uint32_t first, uint32_t second);
int SceI2cForDriver_76D277AB(int enable);	/* vitasdk: ksceI2cReset */
int SceI2cForDriver_9CF8F3D6(unsigned bus);	/* vitasdk: ksceI2cInit */
int sceI2cMasterTransmit(unsigned bus, unsigned address, const uint8_t *buffer, unsigned size);
/* vitasdk: ksceI2cTransferRead */
int SceI2cForDriver_D1D0A9A4(unsigned bus, unsigned address, uint8_t *buffer, unsigned size);
int SceI2cForDriver_E449AC6E(unsigned bus, uint32_t noLock);
