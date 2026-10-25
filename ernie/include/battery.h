/*
 * The battery (app/battery.c): its gauge on the I2C bus at 0xaa (0xa4 where the settings move
 * the Cookie's registers), and its bootloader's commands (0x09b2-0x09b4).
 */
#ifndef ERNIE_BATTERY_H
#define ERNIE_BATTERY_H


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

#endif
