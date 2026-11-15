/*
 * The general-purpose pins (sys/gpio.c). 32 pins in the second block of
 * peripheral registers: two bits per pin select its function, one bit per pin
 * releases it and one bit per pin gives the level it drives. The names are
 * inferred from use; no Marvell source names this code.
 */
#ifndef ROBIN_GPIO_H
#define ROBIN_GPIO_H

#include <stdint.h>

#define GPIO_PINS	32

/* Sets the level of a pin of the first sixteen; the pin must be selected to drive it. */
int gpioPinWriteLow(unsigned int pin, int level);	/* 0x2812c */
/* Selects the pin's function 1 (enable) or releases it (enable 0). */
int gpioPinSelect(unsigned int pin, int enable);	/* 0x28142 */
/* Sets the level of any pin. */
unsigned int gpioPinWrite(unsigned int pin, int level);	/* 0x281d0 */

/* What fmCoexScheduleEntry() does with a pin (sys/gpioconfig.c). */
enum {
	GPIO_CONFIG_DRIVE_ACTIVE = 0,	/* drive the pin high while active */
	GPIO_CONFIG_DRIVE_INACTIVE = 1,	/* drive it low while active */
	GPIO_CONFIG_SIGNAL = 2,		/* hand it to a signal of the chip */
	GPIO_CONFIG_COEX_LOW = 3,	/* set the low coexistence setting */
	GPIO_CONFIG_COEX_HIGH = 4,	/* set the high one */
	GPIO_CONFIG_COEX_BOTH = 5,
	GPIO_CONFIG_NONE = 6,
};

void fmCoexQueryEntry(int pin, unsigned int level);			/* 0x4b3d8 */
void fmCoexResetEntry(int pin, unsigned int high, unsigned int low);	/* 0x4b416 */
void fmCoexSyncEntry(unsigned int pin);					/* 0x4b4aa */
void fmCoexApplyEntry(unsigned int setting);				/* 0x4b4b8 */
void fmCoexResolveEntry(unsigned int setting);				/* 0x4b4cc */
void fmCoexScheduleEntry(int active, int pin, unsigned int kind, unsigned int settings); /* 0x4b4e0 */

#endif /* ROBIN_GPIO_H */
