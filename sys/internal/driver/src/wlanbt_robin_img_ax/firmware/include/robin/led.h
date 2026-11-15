/*
 * The LEDs the host drives through GPIO pins, set by HostCmd 0x004e
 * (CMD_802_11_LED_GPIO_CTRL in Marvell's host drivers). Each of the three
 * LEDs has a GPIO pin and, for each firmware state, a behaviour: what the
 * pin does (gpio.h's GPIO_CONFIG_*) and its argument. The TLV names and the
 * fields follow Marvell's libertas driver (TLV_TYPE_LED_GPIO,
 * TLV_TYPE_LEDBEHAVIOR, struct mrvl_ie_ledbhv); the rest is inferred.
 */
#ifndef ROBIN_LED_H
#define ROBIN_LED_H

#include <stdint.h>

#define TLV_LED_GPIO		0x108
#define TLV_LED_BEHAVIOR	0x109

#define LED_COUNT		3
#define LED_FIRMWARE_STATES	9
#define LED_GPIO_NONE		18	/* the LED has no pin */

/* The firmware states that are the link's state, not a passing event. */
#define LED_STATE_DISCONNECTED	0
#define LED_STATE_LINK_6	6
#define LED_STATE_LINK_7	7

struct LedBehavior {
	uint8_t led;		/* 1..3 */
	uint8_t gpio;		/* the LED's pin, LED_GPIO_NONE without one */
	uint8_t state;		/* what the pin does, GPIO_CONFIG_* */
	uint8_t argument;
};

struct LedConfig {
	struct LedBehavior behavior[LED_FIRMWARE_STATES];
};

_Static_assert(sizeof(struct LedConfig) == 36, "LED configuration");

extern struct LedConfig ledConfig[LED_COUNT];	/* 0x550b0 */
extern int ledActive;				/* 0x5511c */
extern unsigned int ledFirmwareState;		/* 0x55120 */
extern unsigned int ledLinkState;		/* 0x55124 */

/* wlan/hostcmdcfg.c: the TLVs of HostCmd 0x004e. */
unsigned int ledGpioTlvParse(const uint8_t *tlv, uint16_t length, struct LedConfig *config,
			     void (*release)(unsigned int pin));			/* 0x3ec72 */
unsigned int ledBehaviorTlvParse(const uint8_t *tlv, int length, struct LedConfig *config);	/* 0x3ec32 */
unsigned int ledControlTlvsPut(uint8_t *out, const struct LedConfig *config);		/* 0x3ecf6 */

/* sqram/ledpin.c */
void ledInit(void);						/* SQRAM 0xc0009b34 */
void ledIdle(void);						/* SQRAM 0xc0009b36 */
void ledGpioRelease(unsigned int pin, unsigned int mask);	/* SQRAM 0xc0009b38 */
void ledStateRefresh(void);					/* SQRAM 0xc0009b6a */

/* sqram/led.c */
void ledFirmwareStateSet(unsigned int state);			/* SQRAM 0xc0009ba0 */
unsigned int ledLinkStateGet(void);				/* SQRAM 0xc0009be4 */
void hostCmd004eSet(uint8_t *body, unsigned int length);	/* SQRAM 0xc0009bea */
unsigned int hostCmd004eGet(uint8_t *body);			/* SQRAM 0xc0009c50 */

#endif /* ROBIN_LED_H */
