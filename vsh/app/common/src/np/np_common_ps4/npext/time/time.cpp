/*
 * SceNpCommonPs4: ticks of the real time clock.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

/* the time left of a limit that began at start: 0 and 1 when it has passed, else what is left and 0 */
static int timeLeft(const SceInt64 *pNow, const SceInt64 *pStart, SceUInt64 limit,
	SceUInt64 *pLeft)
{
	SceInt64 now;
	SceInt64 start;

	if ((SceInt64)limit < 0 || (now = *pNow, start = *pStart, now < start) || (SceUInt64)(now - start) >= limit) {
		if (pLeft != NULL)
			*pLeft = 0;
		return 1;
	}
	if (pLeft != NULL)
		*pLeft = limit - (now - start);
	return 0;
}

int SceNpCommonPs4_477BD360(SceRtcTick *pTick)
{
	int result;

	result = sceRtcGetCurrentTick(pTick);
	if (result >= 0)
		return 0;
	pTick->tick = 0;
	return result;
}

int SceNpCommonPs4_5B904477(SceRtcTick *pTick)
{
	int result;

	result = sceRtcGetCurrentNetworkTick(pTick);
	if (result >= 0)
		return 0;
	pTick->tick = 0;
	return result;
}

SceRtcTick *SceNpCommonPs4_06A16F9C(SceRtcTick *pTick, const SceInt64 *pTicks)
{
	static const char functionName[] = "operator+";
	int ret;

	ret = sceRtcTickAddTicks(pTick, pTick, *pTicks);
	NP_COMMON_PS4_ASSERT(ret >= 0, functionName, "npext\\time\\time.cpp", 169, "ret == 0");
	return pTick;
}

SceRtcTick *SceNpCommonPs4_80BBB277(SceRtcTick *pTick, const SceInt64 *pTicks)
{
	static const char functionName[] = "operator+";
	int ret;

	ret = sceRtcTickAddTicks(pTick, pTick, *pTicks);
	NP_COMMON_PS4_ASSERT(ret >= 0, functionName, "npext\\time\\time.cpp", 180, "ret == 0");
	return pTick;
}

SceRtcTick *SceNpCommonPs4_D4FB440C(SceRtcTick *pTick, int reserved, SceInt64 microseconds)
{
	static const char functionName[] = "AddMicroSeconds";
	int ret;

	ret = sceRtcTickAddMicroseconds(pTick, pTick, microseconds);
	NP_COMMON_PS4_ASSERT(ret >= 0, functionName, "npext\\time\\time.cpp", 73, "ret == 0");
	return pTick;
}

SceRtcTick *SceNpCommonPs4_3AA2F9B2(SceRtcTick *pTick, int reserved, SceInt64 seconds)
{
	static const char functionName[] = "AddSeconds";
	int ret;

	ret = sceRtcTickAddSeconds(pTick, pTick, seconds);
	NP_COMMON_PS4_ASSERT(ret >= 0, functionName, "npext\\time\\time.cpp", 84, "ret == 0");
	return pTick;
}

SceRtcTick *SceNpCommonPs4_1D6608F1(SceRtcTick *pTick, int reserved, SceInt64 minutes)
{
	static const char functionName[] = "AddMinutes";
	int ret;

	ret = sceRtcTickAddMinutes(pTick, pTick, minutes);
	NP_COMMON_PS4_ASSERT(ret >= 0, functionName, "npext\\time\\time.cpp", 95, "ret == 0");
	return pTick;
}

SceRtcTick *SceNpCommonPs4_A5D9D337(SceRtcTick *pTick, int reserved, int hours)
{
	static const char functionName[] = "AddHours";
	int ret;

	ret = sceRtcTickAddHours(pTick, pTick, hours);
	NP_COMMON_PS4_ASSERT(ret >= 0, functionName, "npext\\time\\time.cpp", 106, "ret == 0");
	return pTick;
}

SceRtcTick *SceNpCommonPs4_A08F4BF9(SceRtcTick *pTick, int reserved, int days)
{
	static const char functionName[] = "AddDays";
	int ret;

	ret = sceRtcTickAddDays(pTick, pTick, days);
	NP_COMMON_PS4_ASSERT(ret >= 0, functionName, "npext\\time\\time.cpp", 117, "ret == 0");
	return pTick;
}

int SceNpCommonPs4_7DABC125(const SceInt64 *pNow, const SceInt64 *pStart, SceUInt64 limit, SceUInt64 *pLeft)
{
	return timeLeft(pNow, pStart, limit, pLeft);
}

int SceNpCommonPs4_71D62E39(const SceInt64 *pNow, const SceInt64 *pStart, SceUInt32 limit, SceUInt32 *pLeft)
{
	SceUInt64 left[1];
	int result;

	result = timeLeft(pNow, pStart, limit, left);
	if (pLeft != NULL)
		*pLeft = (SceUInt32)left[0];
	return result;
}

int SceNpCommonPs4_6B9F019D(const SceInt64 *pNow, const SceInt64 *pStart, SceUInt64 seconds, SceUInt64 *pLeftSeconds)
{
	SceUInt64 left[1];
	int result;

	result = timeLeft(pNow, pStart, seconds * 1000000, left);
	if (pLeftSeconds != NULL)
		*pLeftSeconds = left[0] / 1000000;
	return result;
}

int SceNpCommonPs4_F3FCB2EF(const SceUInt64 *pLeft, const SceUInt64 *pRight)
{
	return *pLeft >= *pRight;
}

}
