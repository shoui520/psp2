/*
 * location_util.cpp: liblocation method decoders, RTC tick helpers, struct initializers, and
 * handle utilities (0x8118F32E-0x8118F574). The file and function names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <liblocation.h>
#include <rtc.h>
#include <std/string.h>

extern "C" {
int sceRtcGetCurrentRetainedNetworkTick(SceRtcTick *tick);
int sceRtcGetCurrentGpsTick(SceRtcTick *tick);
}

/* 0x8118F32E */
int locationDecodeInterval(const SceUInt32 *lmethod)
{
	switch (*lmethod & 0x70) {
	case 0x10:
		return 5000;
	case 0x20:
		return 10000;
	case 0x30:
		return 30000;
	case 0x40:
		return 60000;
	case 0x50:
		return 600000;
	default:
		return 0;
	}
}

/* 0x8118F370 */
int locationDecodePriority(const SceUInt32 *lmethod)
{
	switch (*lmethod & 0x7) {
	case 0:
		return 127;
	case 1:
		return 0;
	case 2:
		return 1;
	case 3:
		return 2;
	case 4:
		return 3;
	default:
		return 127;
	}
}

/* 0x8118F39C */
int locationDecodeDistance(const SceUInt32 *lmethod)
{
	switch (*lmethod & 0x70) {
	case 0x10:
		return 64;
	case 0x20:
		return 100;
	case 0x30:
		return 500;
	case 0x40:
		return 1000;
	default:
		return 0;
	}
}

/* 0x8118F3CC */
float locationDecodeHeadingAngle(const SceUInt32 *hmethod)
{
	switch (*hmethod & 0x700) {
	case 0x100:
		return 1.0f;
	case 0x200:
		return 5.0f;
	case 0x300:
		return 11.25f;
	case 0x400:
		return 22.5f;
	case 0x500:
		return 45.0f;
	case 0x600:
		return 90.0f;
	default:
		return 0.0f;
	}
}

/* 0x8118F43C */
const char *locationStrNStr(const char *haystack, const char *needle, SceSize len)
{
	if (needle == NULL || haystack == NULL)
		return NULL;

	SceSize needleLen = sce_paf_strlen(needle);
	char first = *needle;
	if (first == '\0')
		return haystack;

	const char *rest = needle + 1;
	const char *p = haystack;
	do {
		const char *start;
		char c;
		for (;;) {
			start = p;
			c = *p;
			len--;
			p++;
			if (c == '\0' || c == first)
				break;
			if (len < needleLen)
				return NULL;
		}
		if (c == '\0')
			return NULL;
		if (sce_paf_strncmp(p, rest, needleLen - 1) == 0)
			return start;
	} while (1);
}

/* 0x8118F49E */
int locationGetCurrentTick(SceRtcTick *tick)
{
	int ret = sceRtcGetCurrentRetainedNetworkTick(tick);
	if (ret != 0) {
		ret = sceRtcGetCurrentGpsTick(tick);
		if (ret != 0)
			ret = sceRtcGetCurrentTick(tick);
	}
	return ret;
}

/* 0x8118F4BC */
int locationInitLocationInfo(SceLocationLocationInfo *info)
{
	info->latitude = -9999.0;
	info->longitude = -9999.0;
	info->altitude = -9999.0;
	info->accuracy = -9999.0f;
	info->reserve = -9999.0f;
	info->direction = -9999.0f;
	info->speed = -9999.0f;
	return locationGetCurrentTick(&info->timestamp);
}

/* 0x8118F50E */
int locationInitHeadingInfo(SceLocationHeadingInfo *info)
{
	info->heading = -9999.0f;
	info->nedQuat.x = -9999.0f;
	info->nedQuat.y = -9999.0f;
	info->nedQuat.z = -9999.0f;
	info->nedQuat.w = -9999.0f;
	info->stability = 0;
	return locationGetCurrentTick(&info->timestamp);
}

/* 0x8118F54A */
int locationCompareTick(const SceRtcTick *a, const SceRtcTick *b)
{
	if (a->tick == b->tick)
		return 1;
	return 0;
}

/* 0x8118F562 */
SceLocationHandle locationMakeHandle(int index)
{
	return (SceLocationHandle)((unsigned int)index | 0x80000000U);
}

/* 0x8118F568 */
unsigned int locationHandleToIndex(SceLocationHandle handle)
{
	unsigned int idx = (unsigned int)handle & 0x7FFFFFFFU;
	if (idx >= 8)
		return 0;
	return idx;
}
