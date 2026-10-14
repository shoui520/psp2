/* SceLcdForDriver: LCD panel and backlight driver interface. */
#pragma once

#include <stdint.h>

int ksceLcdWaitReady(void);
int ksceLcdGetDDB(uint16_t *id, uint16_t *revision);
int ksceLcdDisplayOn(void);
int ksceLcdDisplayOff(void);
int ksceLcdSetBrightness(unsigned value);
unsigned ksceLcdGetBrightness(void);
int ksceLcdSetDisplayColorSpaceMode(unsigned mode);
unsigned ksceLcdGetDisplayColorSpaceMode(void);

/* Unknown Sony names are preserved as library/NID identifiers. */
int SceLcdForDriver_04E9269A(const unsigned char *sequence);
int SceLcdForDriver_1D73D7F3(unsigned mode);
int SceLcdForDriver_223C8375(unsigned command, unsigned char *data,
			  unsigned length);
int SceLcdForDriver_3E7C2CCA(void);
int SceLcdForDriver_4735059C(unsigned poll);
int SceLcdForDriver_4B6B53C0(const unsigned char *sequence);
unsigned SceLcdForDriver_4C277FEC(void);
int SceLcdForDriver_4CC1E8C1(unsigned mode);
int SceLcdForDriver_5127FB5E(unsigned char command, const unsigned char *data,
			  unsigned length);
int SceLcdForDriver_59CCDC97(void);
int SceLcdForDriver_6A08623C(void);
int SceLcdForDriver_8C965153(void);
int SceLcdForDriver_949D45E4(void);
int SceLcdForDriver_9F1DB5CD(void);
int SceLcdForDriver_C0A2A6EA(void);
