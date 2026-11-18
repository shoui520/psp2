/*
 * The battery (app/battery.c): its gauge on the I2C bus at 0xaa (0xa4 where the settings move
 * the Cookie's registers), and its bootloader's commands (0x09b2-0x09b4).
 */
#ifndef ERNIE_BATTERY_H
#define ERNIE_BATTERY_H

#include "ernie.h"
#include "board.h"

/* a command to the battery's bootloader (0x09b2-0x09b4) */
struct BatteryBLCommand {
	unsigned int	command;
	unsigned char	length;
	unsigned char	*buffer;
};

#define BATTERY_BL_SIZE		0x63	/* the most the buffer holds */

extern struct BatteryBLCommand batteryBLCommand;

extern void batteryInit(void);
extern unsigned char batteryWrite(unsigned char *data, unsigned char length);
extern unsigned char batteryRead(unsigned char *reg, unsigned char *data, unsigned char length);
extern unsigned char batteryControl(unsigned int command, unsigned char *data,
				    unsigned char length);

extern unsigned char batteryReadInfo(unsigned int *info);
extern unsigned char batteryReadRemainCap(unsigned int *value);
extern unsigned char batteryReadVolt(unsigned int *value);
extern unsigned char batteryReadTemp(unsigned int *value);
extern unsigned char batteryReadBicTemp(unsigned int *value);
extern unsigned char batteryReadLifePercent(unsigned int *value);
extern unsigned char batteryReadAt(unsigned char *packet, unsigned char *data, unsigned char length);
extern unsigned char batteryWriteAt(unsigned char *packet, unsigned char length);
extern unsigned char batteryStartBLMode(void);
extern unsigned char batteryStopBLMode(void);
extern unsigned char batterySWReset(void);
extern void batteryBLLineOn(void);
extern void batteryBLLineOff(void);
extern unsigned char batteryHibernate(void);
extern unsigned char batteryControl0015(void);
#if BOARD_USS1001
/* the slim's faults record, logged as event 5, 0x10 (its first four bytes) at each change:
   byte 0 the power-on's (bits 0-3) and the charge's (4, 5), byte 1 the bootloader line (bit 0)
   and the hibernation (bit 1) */
union BatteryFaults {
	unsigned char	byte[8];
	unsigned int	word[4];
};
extern union BatteryFaults batteryFaults;
extern void batteryGetFaults(unsigned char *faults);
extern void batteryInfoRead(void);	/* after batteryReadInfo answered: nothing */
#endif

#endif
