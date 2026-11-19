/*
 * The LEDs' state and HostCmd 0x004e (SQRAM 0xc0009ba0..0xc0009c78): each
 * firmware state sets every LED's pin to that state's behaviour, and the
 * host reads and writes the LED configuration as TLV_LED_GPIO and
 * TLV_LED_BEHAVIOR TLVs. The filename and the names are inferred.
 */
#include "robin/gpio.h"
#include "robin/led.h"

#define TLV_TYPE(tlv)		((uint16_t)((tlv)[0] | ((tlv)[1] << 8)))
#define TLV_HEADER_SIZE		4

/* The LEDs' configuration: LED 1 on pin 1, the others without a pin, until the host sets them. */
struct LedConfig ledConfig[LED_COUNT] __attribute__((section(".itcm.rw"))) = {
	{ {
		{ 1, 1, 0, 0 }, { 1, 1, 2, 4 }, { 1, 1, 1, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 },
		{ 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 2, 5 },
	} },
	{ {
		{ 2, 18, 0, 0 }, { 2, 18, 2, 4 }, { 2, 18, 1, 0 }, { 2, 18, 0, 0 }, { 2, 18, 0, 0 },
		{ 2, 18, 0, 0 }, { 2, 18, 0, 0 }, { 2, 18, 0, 0 }, { 2, 18, 2, 5 },
	} },
	{ {
		{ 3, 18, 0, 0 }, { 3, 18, 2, 4 }, { 3, 18, 1, 0 }, { 3, 18, 0, 0 }, { 3, 18, 0, 0 },
		{ 3, 18, 0, 0 }, { 3, 18, 0, 0 }, { 3, 18, 0, 0 }, { 3, 18, 2, 5 },
	} },
};
int ledActive __attribute__((section(".itcm.rw"))) = 1;
unsigned int ledFirmwareState __attribute__((section(".itcm.rw"))) = 0;
unsigned int ledLinkState __attribute__((section(".itcm.rw"))) = 0;

/* Sets every LED that has a pin to the state's behaviour; a link state is kept for ledLinkStateGet(). */
void ledFirmwareStateSet(unsigned int state)
{
	unsigned int led;

	for (led = 0; led < LED_COUNT; led = (uint8_t)(led + 1)) {
		const struct LedBehavior *behavior = &ledConfig[led].behavior[state];

		if (behavior->gpio < LED_GPIO_NONE) {
			unsigned int gpio = behavior->gpio;
			unsigned int pinState = behavior->state;
			unsigned int argument = behavior->argument;

			ledFirmwareState = state;
			fmCoexScheduleEntry(ledActive, gpio, pinState, argument);
		}
	}
	if (state == LED_STATE_DISCONNECTED || state == LED_STATE_LINK_6 || state == LED_STATE_LINK_7)
		ledLinkState = state;
}

unsigned int ledLinkStateGet(void)
{
	return ledLinkState;
}

/* Sets the LEDs from the command's TLVs, then shows the current state with them. */
void hostCmd004eSet(uint8_t *body, unsigned int length)
{
	uint8_t *tlv = body + 4;
	unsigned int remaining;

	body[2] = 3;
	body[3] = 0;
	remaining = (uint16_t)(length - 4);
	while (remaining != 0) {
		unsigned int step;

		if (remaining < TLV_HEADER_SIZE)
			return;
		switch (TLV_TYPE(tlv)) {
		case TLV_LED_GPIO:
			step = ledGpioTlvParse(tlv, remaining, ledConfig, fmCoexSyncEntry);
			break;
		case TLV_LED_BEHAVIOR:
			step = ledBehaviorTlvParse(tlv, (uint8_t)remaining, ledConfig);
			break;
		default:
			step = remaining;
			break;
		}
		if (remaining >= step)
			remaining = (uint16_t)(remaining - step);
		else
			remaining = 0;
		tlv += step;
	}
	ledFirmwareStateSet(ledFirmwareState);
}

/* Answers with the LEDs' configuration. */
unsigned int hostCmd004eGet(uint8_t *body)
{
	body[2] = 3;
	body[3] = 0;
	return ledControlTlvsPut(body + 4, ledConfig);
}
