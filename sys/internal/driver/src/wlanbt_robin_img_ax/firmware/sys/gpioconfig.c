/*
 * Pin configuration the host asks for (0x4b3d8..0x4b538): a pin of the first
 * sixteen set to drive a level, or handed to one of the chip's own signals
 * with the two four-bit settings of its group; the two settings of the
 * coexistence signals in register 0x24c. The SQRAM host command code calls
 * fmCoexScheduleEntry() and keeps fmCoexSyncEntry() in a table.
 *
 * The registers are read as words and written as halfwords. Marvell
 * published no source; names are inferred from use.
 */
#include "robin/gpio.h"
#include "robin/soc.h"

#define GPIO_FUNCTION_0		0x200	/* pins 0-7, two bits each */
#define GPIO_FUNCTION_1		0x204	/* pins 8-15 */
#define GPIO_RELEASE_LOW	0x208	/* a bit per pin 0-15: the pin is released */
#define GPIO_LEVEL_LOW		0x20c	/* the level of pins 0-15 */
#define GPIO_SIGNAL_LOW(group)	(0x228 + 4 * (group))	/* four pins a group, four bits each */
#define GPIO_SIGNAL_HIGH(group)	(0x238 + 4 * (group))
#define GPIO_COEX_SIGNAL	0x24c	/* bits 3..0 and 7..4 */

#define GPIO_FUNCTION_DRIVE	1	/* the pin drives its level register bit */
#define GPIO_FUNCTION_SIGNAL	3	/* the pin carries a signal of the chip */

#define GPIO_READ(offset)		PERIPH_REG(offset)
#define GPIO_WRITE(offset, value)	(REG16(0x90000000u + (offset)) = (uint16_t)(value))

__attribute__((noinline)) void fmCoexQueryEntry(int pin, unsigned int level)
{
	unsigned int function;
	unsigned int shift;
	uint32_t bit;

	if (pin > 7) {
		function = GPIO_FUNCTION_1;
		shift = 2 * (pin - 8);
	} else {
		function = GPIO_FUNCTION_0;
		shift = 2 * pin;
	}
	GPIO_WRITE(function, (GPIO_READ(function) & ~(3u << shift)) | (GPIO_FUNCTION_DRIVE << shift));
	bit = 1u << pin;
	GPIO_WRITE(GPIO_RELEASE_LOW, GPIO_READ(GPIO_RELEASE_LOW) & ~bit);
	GPIO_WRITE(GPIO_LEVEL_LOW, (GPIO_READ(GPIO_LEVEL_LOW) & ~bit) | (level << pin));
}

__attribute__((noinline)) void fmCoexResetEntry(int pin, unsigned int high, unsigned int low)
{
	unsigned int function;
	unsigned int group;
	unsigned int base;
	unsigned int first;
	unsigned int shift;
	uint32_t field;
	uint32_t mask;

	if (pin <= 3) {
		function = GPIO_FUNCTION_0;
		group = 0;
		base = 0;
		first = 0;
	} else if ((unsigned int)(pin - 4) <= 3) {
		function = GPIO_FUNCTION_0;
		group = 1;
		base = 0;
		first = 4;
	} else if ((unsigned int)(pin - 8) <= 3) {
		function = GPIO_FUNCTION_1;
		group = 2;
		base = 8;
		first = 8;
	} else {
		function = GPIO_FUNCTION_1;
		group = 3;
		base = 8;
		first = 12;
	}
	field = GPIO_FUNCTION_SIGNAL << (2 * (pin - base));
	GPIO_WRITE(function, (GPIO_READ(function) & ~field) | field);
	GPIO_WRITE(GPIO_RELEASE_LOW, GPIO_READ(GPIO_RELEASE_LOW) & ~(1u << pin));
	shift = 4 * (pin - first);
	mask = 0xfu << shift;
	GPIO_WRITE(GPIO_SIGNAL_LOW(group), (GPIO_READ(GPIO_SIGNAL_LOW(group)) & ~mask) | (low << shift));
	GPIO_WRITE(GPIO_SIGNAL_HIGH(group), (GPIO_READ(GPIO_SIGNAL_HIGH(group)) & ~mask) | (high << shift));
}

__attribute__((noinline)) void fmCoexSyncEntry(unsigned int pin)
{
	GPIO_WRITE(GPIO_RELEASE_LOW, GPIO_READ(GPIO_RELEASE_LOW) | (1u << pin));
}

__attribute__((noinline)) void fmCoexApplyEntry(unsigned int setting)
{
	GPIO_WRITE(GPIO_COEX_SIGNAL, (GPIO_READ(GPIO_COEX_SIGNAL) & ~0xf0u) | ((setting & 0xf) << 4));
}

__attribute__((noinline)) void fmCoexResolveEntry(unsigned int setting)
{
	GPIO_WRITE(GPIO_COEX_SIGNAL, (GPIO_READ(GPIO_COEX_SIGNAL) & ~0xfu) | (setting & 0xf));
}

void fmCoexScheduleEntry(int active, int pin, unsigned int kind, unsigned int settings)
{
	unsigned int level;

	switch (kind) {
	case GPIO_CONFIG_DRIVE_ACTIVE:
		level = active != 0;
		goto drive;
	case GPIO_CONFIG_DRIVE_INACTIVE:
		level = active == 0;
	drive:
		fmCoexQueryEntry(pin, level);
		break;
	case GPIO_CONFIG_SIGNAL:
		fmCoexResetEntry(pin, settings >> 4, settings & 0xf);
		break;
	case GPIO_CONFIG_COEX_LOW:
		fmCoexResolveEntry(settings & 0xf);
		break;
	case GPIO_CONFIG_COEX_HIGH:
		fmCoexApplyEntry(settings >> 4);
		break;
	case GPIO_CONFIG_COEX_BOTH:
		fmCoexApplyEntry(settings >> 4);
		fmCoexResolveEntry(settings & 0xf);
		break;
	default:
		break;
	}
}
