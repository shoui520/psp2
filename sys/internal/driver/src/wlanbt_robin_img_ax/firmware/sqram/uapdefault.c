/*
 * The defaults a uAP BSS starts with (SQRAM 0xc000e5a8..0xc000e628): its
 * rates, its capability and its HT capability element. The filename and the
 * names are inferred.
 */
#include <stdint.h>
#include "robin/rom.h"
#include "robin/uapbss.h"

/* The twelve 802.11g rates in 500 kb/s units; 1, 2, 5.5 and 11 Mb/s are basic. */
void uapDefaultRatesSet(uint8_t *rates)
{
	rates[0] = 0x82;
	rates[1] = 0x84;
	rates[2] = 0x8b;
	rates[3] = 0x96;
	rates[4] = 12;
	rates[5] = 18;
	rates[6] = 24;
	rates[7] = 36;
	rates[8] = 48;
	rates[9] = 72;
	rates[10] = 96;
	rates[11] = 108;
}

/* ESS and short preamble; short slot time. */
void uapDefaultCapabilitySet(uint8_t *capability)
{
	capability[0] = 0x21;
	capability[1] = 0x04;
}

/*
 * The HT capability element: 20 MHz only, SM power save off, greenfield, no
 * short guard interval, one RX STBC stream, DSSS/CCK in 40 MHz, A-MPDUs of
 * up to 64 KiB, and MCS 0-7 and 32.
 */
void uapDefaultHtCapabilitySet(uint8_t *htCapability)
{
	__aeabi_memclr(htCapability, 28);
	htCapability[0] = 45;
	htCapability[1] = 26;
	htCapability[2] = (((htCapability[2] & ~0x02) | 0x0c) | 0x10) & 0x9f;
	htCapability[3] = ((htCapability[3] & ~0x03) + 1) | 0x10;
	htCapability[4] |= 0x03;
	htCapability[5] = 0xff;
	htCapability[9] = 0x01;
}
