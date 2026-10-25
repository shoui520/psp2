/*
 * The power and battery requests (app/powerreq.c): request 8 takes the power commands
 * 0x0880-0x08c4 (the Elmo and Cookie PMICs, the devices' power, the LEDs, the charger),
 * request 9 the battery's 0x0980-0x09b4, and the host commands 0x0800-0x0804 and
 * 0x0900-0x0902 read the power and battery status at once.
 */
#ifndef ERNIE_POWERREQ_H
#define ERNIE_POWERREQ_H

#include "host.h"
#include "input.h"

/* the power status (host command 0x0800) */
union PowerStatus {
	unsigned char	byte[2];
	struct {
		unsigned char	flags;
		unsigned char	usb:2;		/* the USB status's bits 2-3 (0x089a) */
		unsigned char	reserved1:2;
		unsigned char	cookieLock0:1;	/* either set: the Cookie's register 3 keeps */
		unsigned char	cookieLock1:1;	/* its bits (powerCookieClear, sub_2A379) */
		unsigned char	reserved2:2;
	} bits;
};

extern __sreg union PowerStatus saddr_FFE54;
extern struct HostRequest powerRequest;
extern struct HostRequest batteryRequest;
extern struct InputHandler powerInputHandler;

extern void powerRequestInit(void);
extern void powerRequestEvent(void);
extern void batteryRequestEvent(void);
extern unsigned char hostCommandPowerStatus(unsigned char *out, unsigned char *length);
extern unsigned char hostCommand0801(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandBatteryRemainCapOld(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandBatteryVoltOld(unsigned char *out, unsigned char *length);
extern unsigned char hostCommand0804(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandBatteryRemainCap(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandBatteryVolt(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandBatteryLifePercent(unsigned char *out, unsigned char *length);
extern void powerRequestLedDone(void);

#endif
