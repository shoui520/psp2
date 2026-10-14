/*
 * Mask ROM RAM setup, init table entry (0xc00075a6 .. 0xc0007614).
 *
 * The ROM keeps its own variables in fixed DTCM and SQRAM areas outside the
 * firmware's scatter regions. This entry clears the ROM's zero-initialised
 * areas, copies its initialised data out of the ROM image, and installs
 * three firmware hooks.
 */
#include <stdint.h>
#include "robin/sys.h"

#define ROM_ZI_DTCM		((uint32_t *)0x0400a700)
#define ROM_ZI_DTCM_SIZE	0x4300
#define ROM_ZI_SQRAM		((uint32_t *)0xc0033000)
#define ROM_ZI_SQRAM_SIZE	0x1000
#define ROM_RW_A		((uint32_t *)0x0400f400)
#define ROM_RW_A_INIT		((const uint32_t *)0x03f46e00)
#define ROM_RW_A_SIZE		0x800
#define ROM_RW_B		((uint32_t *)0x0400ea00)
#define ROM_RW_B_INIT		((const uint32_t *)0x03f47600)
#define ROM_RW_B_SIZE		0xa00

void osaTimerListInit(void);	/* 0x00027e6a, not converted */
void rfHookInit(void);		/* 0x00026fc6, not converted */

/* Word copy (0xc00075e8). */
static void romRamCopy(uint32_t *dst, const uint32_t *src, uint32_t size)
{
	uint32_t i;

	for (i = 0; i < size >> 2; i++)
		dst[i] = src[i];
}

/* Word fill (0xc00075fe). */
static void romRamFill(uint32_t *dst, uint32_t value, uint32_t size)
{
	uint32_t i;

	for (i = 0; i < size >> 2; i++)
		dst[i] = value;
}

int romRamInit(void)
{
	romRamFill(ROM_ZI_DTCM, 0, ROM_ZI_DTCM_SIZE);
	romRamFill(ROM_ZI_SQRAM, 0, ROM_ZI_SQRAM_SIZE);
	romRamCopy(ROM_RW_A, ROM_RW_A_INIT, ROM_RW_A_SIZE);
	romRamCopy(ROM_RW_B, ROM_RW_B_INIT, ROM_RW_B_SIZE);
	osaTimerListInit();
	socEventHookInit();
	rfHookInit();
	return 0;
}
