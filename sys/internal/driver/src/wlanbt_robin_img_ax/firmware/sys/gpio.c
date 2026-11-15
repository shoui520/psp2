/*
 * The general-purpose pins: function and level of the pins the host
 * commands and the SDIO wake line use. Marvell published no source for it;
 * the names say what the code does.
 */
#include "robin/gpio.h"
#include "robin/soc.h"

#define GPIO_FUNCTION_0			PERIPH_REG(0x200)	/* pins 0-7, two bits each */
#define GPIO_FUNCTION_1			PERIPH_REG(0x204)	/* pins 8-15 */
#define GPIO_RELEASE_LOW		PERIPH_REG(0x208)	/* a bit per pin 0-15: the pin is released */
#define GPIO_LEVEL_LOW			PERIPH_REG(0x20c)	/* the level of pins 0-15 */
#define GPIO_FUNCTION_2			PERIPH_REG(0x25c)	/* pins 16-23 */
#define GPIO_FUNCTION_3			PERIPH_REG(0x260)	/* pins 24-31 */
#define GPIO_RELEASE_HIGH		PERIPH_REG(0x264)	/* a bit per pin 16-31 */
#define GPIO_LEVEL_HIGH			PERIPH_REG(0x268)	/* the level of pins 16-31 */

#define GPIO_FUNCTION_DRIVE		1	/* the function that drives the level register's bit */

int gpioPinWriteLow(unsigned int pin, int level)
{
	GPIO_LEVEL_LOW = (GPIO_LEVEL_LOW & ~(1u << pin)) | ((uint32_t)level << pin);
	return 1;
}

int gpioPinSelect(unsigned int pin, int enable)
{
	uint32_t bit = 1u << pin;
	uint32_t highBit = 1u << (pin - 16);

	if (enable == 0) {
		if (pin < 16)
			GPIO_RELEASE_LOW |= bit;
		else
			GPIO_RELEASE_HIGH |= highBit;
		return 1;
	}
	if (pin >= 8) {
		if (pin < 16) {
			GPIO_FUNCTION_1 = (GPIO_FUNCTION_1 & ~(3u << (2 * (pin - 8)))) |
					  (GPIO_FUNCTION_DRIVE << (2 * (pin - 8)));
			GPIO_RELEASE_LOW &= ~bit;
			return 1;
		}
		if (pin >= 24) {
			if (pin < 32)
				GPIO_FUNCTION_3 = (GPIO_FUNCTION_3 & ~(3u << (2 * (pin - 24)))) |
						  (GPIO_FUNCTION_DRIVE << (2 * (pin - 24)));
		} else {
			GPIO_FUNCTION_2 = (GPIO_FUNCTION_2 & ~(3u << (2 * (pin - 16)))) |
					  (GPIO_FUNCTION_DRIVE << (2 * (pin - 16)));
		}
		GPIO_RELEASE_HIGH &= ~highBit;
		return 1;
	}
	GPIO_FUNCTION_0 = (GPIO_FUNCTION_0 & ~(3u << (2 * pin))) | (GPIO_FUNCTION_DRIVE << (2 * pin));
	GPIO_RELEASE_LOW &= ~bit;
	return 1;
}

unsigned int gpioPinWrite(unsigned int pin, int level)
{
	uint32_t value;

	if ((int)pin >= 16) {
		pin -= 16;
		value = GPIO_LEVEL_HIGH & ~(1u << pin);
		if (level != 0)
			value |= 1u << pin;
		GPIO_LEVEL_HIGH = value;
	} else {
		value = GPIO_LEVEL_LOW & ~(1u << pin);
		if (level != 0)
			value |= 1u << pin;
		GPIO_LEVEL_LOW = value;
	}
	return pin;
}
