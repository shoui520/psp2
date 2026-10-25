/*
 * The parameter block: loaded at the start from the settings block (the battery's levels,
 * the button times, two bytes for the host) and the defaults below, then read and written
 * whole by the host and read field by field by the firmware, each access with dispatching
 * off.
 */
#include <string.h>

#include "ernie.h"
#include "config.h"
#include "kernel.h"
#include "param.h"

static const unsigned char paramPattern1Default[PARAM_PATTERN_SIZE] = {
	0x00, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	0x01, 0x01, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	0x00, 0x0c, 0x12,
};

static const unsigned char paramPattern2Default[PARAM_PATTERN_SIZE] = {
	0x00, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	0x01, 0x01, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	0x00, 0x0c, 0x12,
};

struct Param param;

int paramInit(void)
{
	dis_dsp();
	memset(&param, 0, sizeof(param));
	param.levels[0] = 0x23;
	param.levels[1] = 0x23;
	param.levels[2] = 0x23;
	param.levels[3] = 0x23;
	param.base = 7;
	param.adjust = 0;
	memcpy(param.pattern1, paramPattern1Default, PARAM_PATTERN_SIZE);
	memcpy(param.pattern2, paramPattern2Default, PARAM_PATTERN_SIZE);
	paramLoad();
	ena_dsp();
	return 0;
}

int paramLoad(void)
{
	if (configGet()->board.bits.bbmcDetect)
		memcpy(param.chargeLevels, configGet()->chargeLevels, sizeof(param.chargeLevels));
	else
		memcpy(param.chargeLevels, configGet()->chargeLevelsNoDetect,
		       sizeof(param.chargeLevels));
	param.pressTime1 = configGet()->pressTime1;
	param.pressTime2 = configGet()->pressTime2;
	param.setting1 = configGet()->paramSetting1;
	param.setting2 = configGet()->paramSetting2;
	return 0;
}

/* host command 0x0092: 1 when the range is past the block or longer than PARAM_ACCESS_MAX */
unsigned char paramRead(unsigned int offset, unsigned int size, unsigned char *buf)
{
	if (offset + size > sizeof(param))
		return 1;
	if (size > PARAM_ACCESS_MAX)
		return 1;
	dis_dsp();
	memcpy(buf, (unsigned char *)&param + offset, size);
	ena_dsp();
	return 0;
}

/* host command 0x0093 */
unsigned char paramWrite(unsigned int offset, unsigned int size, unsigned char *buf)
{
	if (offset + size > sizeof(param))
		return 1;
	if (size > PARAM_ACCESS_MAX)
		return 1;
	dis_dsp();
	memcpy((unsigned char *)&param + offset, buf, size);
	ena_dsp();
	return 0;
}

void paramGetChargeLevels(struct ChargeLevel *levels)
{
	unsigned int i;

	dis_dsp();
	for (i = 0; i < 10; i++) {
		levels->remainCap = param.chargeLevels[i].remainCap;
		levels->volt = param.chargeLevels[i].volt;
		levels++;
	}
	ena_dsp();
}

void paramGetBase(unsigned char *base)
{
	dis_dsp();
	*base = param.base & 0x3f;
	ena_dsp();
}

void paramGetAdjust(unsigned char *adjust)
{
	dis_dsp();
	*adjust = param.adjust & 0xf0;
	ena_dsp();
}

void paramGetPattern1(unsigned char *pattern)
{
	dis_dsp();
	memcpy(pattern, param.pattern1, PARAM_PATTERN_SIZE);
	ena_dsp();
}

void paramGetPattern2(unsigned char *pattern)
{
	dis_dsp();
	memcpy(pattern, param.pattern2, PARAM_PATTERN_SIZE);
	ena_dsp();
}
