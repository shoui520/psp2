/*
 * The power and battery requests (app/powerreq.c): request 8 takes the power commands
 * 0x0880-0x08c4 (the Elmo and Cookie PMICs, the devices' power, the LEDs, the charger),
 * request 9 the battery's 0x0980-0x09b4, and the host commands 0x0800-0x0804 and
 * 0x0900-0x0902 read the power and battery status at once.
 */
#ifndef ERNIE_POWERREQ_H
#define ERNIE_POWERREQ_H

#include "charge.h"
#include "host.h"
#include "input.h"

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
