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

#endif /* ROBIN_GPIO_H */
