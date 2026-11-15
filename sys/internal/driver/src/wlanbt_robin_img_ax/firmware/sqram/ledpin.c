/*
 * The LEDs' hooks into the rest of the firmware (SQRAM 0xc0009b34..
 * 0xc0009ba0): a pin taken for another use is taken from every LED, and the
 * LEDs follow the link when the MAC goes back to sleep. The filename and the
 * names are inferred.
 */
#include "robin/conn.h"
#include "robin/coex.h"
#include "robin/led.h"

/* Nothing to set up; the hook is kept for the start of the MAC. */
void ledInit(void)
{
}

/* Nothing to do when the transmit path goes idle. */
void ledIdle(void)
{
}

/* Takes the pin from every LED that uses it. */
void ledGpioRelease(unsigned int pin, unsigned int mask)
{
	unsigned int led;
	unsigned int state;

	(void)mask;
	for (led = 0; led < LED_COUNT; led = (uint8_t)(led + 1))
		for (state = 0; state < LED_FIRMWARE_STATES; state = (uint8_t)(state + 1))
			if (ledConfig[led].behavior[state].gpio == pin)
				ledConfig[led].behavior[state].gpio = LED_GPIO_NONE;
}

/* Shows the link's state again: connected, connected and asleep (2), or not connected (5, 2 in power save). */
void ledStateRefresh(void)
{
	unsigned int state;

	if (wlanConnectionExists(0)) {
		if (wlanAnyConnectionFlags2Bit4())
			state = 2;
		else
			state = ledLinkStateGet();
	} else if (wlanPsFilterState == 0) {
		state = 5;
	} else {
		state = 2;
	}
	ledFirmwareStateSet(state);
}
