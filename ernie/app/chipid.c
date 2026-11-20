/*
 * USS-1001's chip ID: four bytes of the eight the flash's extra area holds at 0x850-0x857
 * (flashReadExtra), read once at the start and kept with their checksum (USS-1002's 0x0105:
 * read at each call). Serial command 0x0917 reads them back.
 */
#include "board.h"

#if SOURCE_0103
#include "chipid.h"
#include <string.h>

#include "kernel.h"

#define CHIP_ID_VALID		0xaa
#define CHIP_ID_BAD		0x55	/* the checksum failed */

__near unsigned char chipIdExtra[8];		/* the extra area's bytes, near */

#if CHIP_ID_ON_DEMAND
/* the four bytes, read from the extra area at each call; the copy cleared after */
unsigned char chipIdGet(unsigned char *out)
{
	unsigned char result = 0;

	flashReadExtra(chipIdExtra);
	out[0] = chipIdExtra[0];
	out[1] = chipIdExtra[1];
	out[2] = chipIdExtra[2];
	out[3] = chipIdExtra[6];
	memset(chipIdExtra, 0, sizeof(chipIdExtra));
	return result;
}
#else

static struct {
	unsigned char	id[4];		/* the extra area's bytes 0-2 and 6 */
	unsigned char	check;		/* chipIdSum's, inverted */
	unsigned char	mark;		/* CHIP_ID_VALID, CHIP_ID_BAD, 0 before chipIdInit */
} chipId;

static unsigned char chipIdSum(void);

unsigned char chipIdInit(void)
{
	unsigned char result = 0;

	chipId.mark = 0;
	flashReadExtra(chipIdExtra);
	chipId.id[0] = chipIdExtra[0];
	chipId.id[1] = chipIdExtra[1];
	chipId.id[2] = chipIdExtra[2];
	chipId.id[3] = chipIdExtra[6];
	chipId.check = chipIdSum() ^ 0xff;
	chipId.mark = CHIP_ID_VALID;
	return result;
}

/* the four bytes: 0x80 before chipIdInit, 0x81 when the checksum fails (then always) */
unsigned char chipIdGet(unsigned char *out)
{
	unsigned char result = 0;

	dis_dsp();
	switch (chipId.mark) {
	case CHIP_ID_VALID:
		if ((unsigned char)(chipIdSum() + chipId.check) & 1) {
			out[0] = chipId.id[0];
			out[1] = chipId.id[1];
			out[2] = chipId.id[2];
			out[3] = chipId.id[3];
		} else {
			result = 0x81;
			chipId.mark = CHIP_ID_BAD;
		}
		break;
	case CHIP_ID_BAD:
		result = 0x81;
		break;
	default:
		result = 0x80;
		break;
	}
	ena_dsp();
	return result;
}

static unsigned char chipIdSum(void)
{
	unsigned char sum = 0;

	sum += chipId.id[0];
	sum += chipId.id[1];
	sum += chipId.id[2];
	sum += chipId.id[3];
	return sum;
}
#endif
#endif
