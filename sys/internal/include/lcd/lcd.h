/* SceLcdForDriver: LCD panel and backlight driver interface. */
#pragma once

#include <stdint.h>

int SceLcdForDriver_0C7E03D8(void);	/* vitasdk: ksceLcdWaitReady */
int SceLcdForDriver_E03E120B(uint16_t *id, uint16_t *revision);	/* vitasdk: ksceLcdGetDDB */
int SceLcdForDriver_5F4124AB(void);	/* vitasdk: ksceLcdDisplayOn */
int SceLcdForDriver_1A0A7519(void);	/* vitasdk: ksceLcdDisplayOff */
int SceLcdForDriver_581D3A87(unsigned value);	/* vitasdk: ksceLcdSetBrightness */
unsigned SceLcdForDriver_3A6D6AC3(void);	/* vitasdk: ksceLcdGetBrightness */
int SceLcdForDriver_D40968FB(unsigned mode);	/* vitasdk: ksceLcdSetDisplayColorSpaceMode */
unsigned SceLcdForDriver_17F66722(void);	/* vitasdk: ksceLcdGetDisplayColorSpaceMode */

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
