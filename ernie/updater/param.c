/*
 * The parameter block, as the updater has it (an older revision of the application's): loaded
 * at the start from the settings block (the battery's levels, the button times), read by the
 * host and field by field by the firmware.
 */
#include <string.h>

#include "ernie.h"
#include "config.h"
#include "param.h"

struct Param param;

int paramInit(void)
{
	memset(&param, 0, sizeof(param));
	if (configGet()->board.bits.bbmcDetect)
		memcpy(param.chargeLevels, configGet()->chargeLevels, sizeof(param.chargeLevels));
	else
		memcpy(param.chargeLevels, configGet()->chargeLevelsNoDetect,
		       sizeof(param.chargeLevels));
	param.levels[0] = 0x23;
	param.levels[1] = 0x23;
	param.levels[2] = 0x23;
	param.levels[3] = 0x23;
	param.base = 7;
	param.adjust = 0;
	param.pressTime1 = configGet()->pressTime1;
	param.pressTime2 = configGet()->pressTime2;
	return 0;
}

/* host command 0x0092: 1 when the range is past the block or longer than PARAM_ACCESS_MAX */
unsigned char paramRead(unsigned int offset, unsigned int size, unsigned char *buf)
{
	if (offset + size > sizeof(param))
		return 1;
	if (size > PARAM_ACCESS_MAX)
		return 1;
	memcpy(buf, (unsigned char *)&param + offset, size);
	return 0;
}

void paramGetChargeLevels(struct ChargeLevel *levels)
{
	unsigned int i;

	for (i = 0; i < 10; i++) {
		levels->remainCap = param.chargeLevels[i].remainCap;
		levels->volt = param.chargeLevels[i].volt;
		levels++;
	}
}

/* the Elmo's regulators 0x81-0x84's levels */
void paramGetLevel0(unsigned char *level)
{
	*level = param.levels[0] & 0x3f;
}

void paramGetLevel1(unsigned char *level)
{
	*level = param.levels[1] & 0x3f;
}

void paramGetLevel2(unsigned char *level)
{
	*level = param.levels[2] & 0x3f;
}

void paramGetLevel3(unsigned char *level)
{
	*level = param.levels[3] & 0x3f;
}

void paramGetBase(unsigned char *base)
{
	*base = param.base & 0x3f;
}

void paramGetAdjust(unsigned char *adjust)
{
	*adjust = param.adjust & 0xf0;
}

void paramGetPattern1(unsigned char *pattern)
{
	memcpy(pattern, param.pattern1, PARAM_PATTERN_SIZE);
}
