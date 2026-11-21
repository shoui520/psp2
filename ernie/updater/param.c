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

#if SOURCE_0103
/* the PS TV's and the slim's: their applications' revision, without the dispatch locks */
int paramInit(void)
{
	memset(&param, 0, sizeof(param));
	param.adjust = 0;
	param.alarmLevel = 0;
	param.alarmLevel3G = 0;
	param.presets[0] = 0x19;
	param.presets[1] = 0x19;
	param.presets[2] = 0x09;
	param.presets[3] = 0x09;
	paramLoad();
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
#elif FW_SINCE(0x0100060D)
/* 0x0100060D's: the application's 0x0100010A revision, the settings block's part loaded by
   paramLoad */
int paramInit(void)
{
	memset(&param, 0, sizeof(param));
	param.levels[0] = 0x23;
	param.levels[1] = 0x23;
	param.levels[2] = 0x23;
	param.levels[3] = 0x23;
	param.base = 7;
	param.adjust = 0;
	paramLoad();
	return 0;
}

void paramLoad(void)
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
}
#else
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
#endif

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

#if SOURCE_0103
void paramGetAdjust(unsigned char *adjust)
{
	*adjust = param.adjust & 0xf0;
}

void paramGetPattern1(unsigned char *pattern)
{
	memcpy(pattern, param.pattern1, PARAM_PATTERN_SIZE);
}

void paramGetPattern2(unsigned char *pattern)
{
	memcpy(pattern, param.pattern2, PARAM_PATTERN_SIZE);
}

/* the charge alarms' adjust: 0 when the host set none of 0-2 */
void paramGetAlarmLevel3G(unsigned char *level)
{
	if (param.alarmLevel3G < 3)
		*level = param.alarmLevel3G;
	else
		*level = 0;
}

void paramGetAlarmLevel(unsigned char *level)
{
	if (param.alarmLevel < 3)
		*level = param.alarmLevel;
	else
		*level = 0;
}
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

void paramGetAdjust(unsigned char *adjust)
{
	*adjust = param.adjust & 0xf0;
}

void paramGetPattern1(unsigned char *pattern)
{
	memcpy(pattern, param.pattern1, PARAM_PATTERN_SIZE);
}
#endif
