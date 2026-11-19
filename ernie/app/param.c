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

#if FW_SINCE(0x00090804) && !BOARD_DOL1001	/* DOL-1001 keeps no default patterns */
static TABLE_CONST unsigned char paramPattern1Default[PARAM_PATTERN_SIZE] = {
	0x00, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	0x01, 0x01, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
#if FW_SINCE(0x01000104)
	0x00, 0x0c, 0x12,
#else
	0x00, 0x06, 0x0a,
#endif
};
#endif

#if FW_SINCE(0x01000306) && !BOARD_DOL1001
static const unsigned char paramPattern2Default[PARAM_PATTERN_SIZE] = {
	0x00, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	0x01, 0x01, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	0x00, 0x0c, 0x12,
};
#endif

#if !FW_SINCE(0x00090206)
/* 0x00090009's battery levels (remainCap, volt), without a settings block */
static const struct ChargeLevel paramChargeLevelsDefault[9] = {
	{ 0x00d6, 0x0dac },
	{ 0x0047, 0x0ce4 },
	{ 0x00d6, 0x0dac },
	{ 0x00d6, 0x0dac },
	{ 0x008f, 0x0d48 },
	{ 0x0047, 0x0ce4 },
	{ 0x8024, 0x0c1c },
	{ 0x00d6, 0x0dac },
	{ 0x00d6, 0x0dac },
};
#endif

struct Param param;

/* serial1Command0950's base step at a reset: 0x00411000's 0x0b (not 0x00315200's) */
#if BOARD_IS(0x00411000) && !HW_IS(0x00315200)
#define PARAM_BASE_DEFAULT	0x0b
#else
#define PARAM_BASE_DEFAULT	7
#endif

#if FW_SINCE(0x01000104)
int paramInit(void)
{
	dis_dsp();
	memset(&param, 0, sizeof(param));
#if !FW_SINCE(0x0100060B)
	/* the 0x0100060x builds have no regulator levels (commands 0x0950 and 0x0951) */
	param.levels[0] = 0x23;
	param.levels[1] = 0x23;
	param.levels[2] = 0x23;
	param.levels[3] = 0x23;
	param.base = PARAM_BASE_DEFAULT;
#endif
	param.adjust = 0;
#if !BOARD_DOL1001
	memcpy(param.pattern1, paramPattern1Default, PARAM_PATTERN_SIZE);
#if FW_SINCE(0x01000306)
	memcpy(param.pattern2, paramPattern2Default, PARAM_PATTERN_SIZE);
#endif
#endif
#if FW_SINCE(0x0100060B)
	param.alarmLevel = 0;
	param.alarmLevel3G = 0;
#endif
#if SOURCE_0103
	param.presets[0] = 0x19;
	param.presets[1] = 0x19;
	param.presets[2] = 0x09;
	param.presets[3] = 0x09;
#endif
	paramLoad();
	ena_dsp();
	return 0;
}
#elif !FW_SINCE(0x00090206)
/* 0x00090009 has no settings block: nine levels from its own table, the tenth unset */
int paramInit(void)
{
	dis_dsp();
	memset(&param, 0, sizeof(param));
	memcpy(param.chargeLevels, paramChargeLevelsDefault, sizeof(paramChargeLevelsDefault));
	param.chargeLevelsEnd[0] = 0xff;
	param.chargeLevelsEnd[1] = 0xff;
	param.chargeLevelsEnd[2] = 0xff;
	param.chargeLevelsEnd[3] = 0xff;
	param.levels[0] = 0x24;
	param.levels[1] = 0x24;
	param.levels[2] = 0x24;
	param.levels[3] = 0x24;
	param.base = PARAM_BASE_DEFAULT;
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
	param.adjust = 0;
#endif
	ena_dsp();
	return 0;
}
#else
/* before 0x01000104 the block has no press times, and the settings come before pattern 1 */
int paramInit(void)
{
	dis_dsp();
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
	param.base = PARAM_BASE_DEFAULT;
	param.adjust = 0;
#if FW_SINCE(0x00090804)
	param.setting1 = configGet()->paramSetting1;
	param.setting2 = configGet()->paramSetting2;
	memcpy(param.pattern1, paramPattern1Default, PARAM_PATTERN_SIZE);
#else
	/* 0x00090206 takes the settings from the bytes the press times have later, and has no
	   pattern 1 */
	param.setting1 = configGet()->pressTime1;
	param.setting2 = configGet()->pressTime2;
#endif
	ena_dsp();
	return 0;
}
#endif

#if FW_SINCE(0x01000104)
#if FW_SINCE(0x0100010B)
int paramLoad(void)
#else
void paramLoad(void)
#endif
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
#if FW_SINCE(0x0100010B)
	return 0;
#endif
}
#endif

/* host command 0x0092: 1 when the range is past the block or longer than PARAM_ACCESS_MAX */
unsigned char paramRead(unsigned int offset, unsigned int size, unsigned char *buf)
{
	if (offset + size > sizeof(param))
		return 1;
	if (size > PARAM_ACCESS_MAX)
		return 1;
#if FW_SINCE(0x00090206)
	dis_dsp();
	memcpy(buf, (unsigned char *)&param + offset, size);
	ena_dsp();
#else
	/* 0x00090009 reads with dispatching on */
	memcpy(buf, (unsigned char *)&param + offset, size);
#endif
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

#if FW_SINCE(0x00090206)
	dis_dsp();
	for (i = 0; i < 10; i++) {
		levels->remainCap = param.chargeLevels[i].remainCap;
		levels->volt = param.chargeLevels[i].volt;
		levels++;
	}
	ena_dsp();
#else
	/* 0x00090009: its nine levels, dispatching on */
	for (i = 0; i < 9; i++) {
		levels->remainCap = param.chargeLevels[i].remainCap;
		levels->volt = param.chargeLevels[i].volt;
		levels++;
	}
#endif
}

#if FW_SINCE(0x01000306)
#if !FW_SINCE(0x0100060B)
void paramGetBase(unsigned char *base)
{
	dis_dsp();
	*base = param.base & 0x3f;
	ena_dsp();
}
#endif

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

#if FW_SINCE(0x0100060B)
/* the charge alarms' adjust (config chargeAlarmAdjust's row): 0 when the host set none of
   0-2 */
void paramGetAlarmLevel3G(unsigned char *level)
{
	dis_dsp();
	if (param.alarmLevel3G < 3)
		*level = param.alarmLevel3G;
	else
		*level = 0;
	ena_dsp();
}

void paramGetAlarmLevel(unsigned char *level)
{
	dis_dsp();
	if (param.alarmLevel < 3)
		*level = param.alarmLevel;
	else
		*level = 0;
	ena_dsp();
}
#endif
#else
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

#if FW_SINCE(0x00090009)
void paramGetAdjust(unsigned char *adjust)
{
	*adjust = param.adjust & 0xf0;
}
#endif

#if FW_SINCE(0x00090804)
void paramGetPattern1(unsigned char *pattern)
{
	memcpy(pattern, param.pattern1, PARAM_PATTERN_SIZE);
}
#endif
#endif
