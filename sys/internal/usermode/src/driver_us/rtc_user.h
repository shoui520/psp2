/*
 * SceDriverUser: SceRtcUser internals shared by the calendar and the parser.
 */

#ifndef _RTC_USER_H
#define _RTC_USER_H

#include <rtc.h>

/*
 * The run-time division helper, called by name where a newer compiler
 * would otherwise fold the division into different operands.
 */
SceUInt64 __aeabi_uldivmod(SceUInt64 n, SceUInt64 d);

int sceRtcCheckValid(const SceDateTime *time);
int rtcGetTickUnchecked(const SceDateTime *time, SceRtcTick *tick);
int sceRtcTickAddMinutes(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n);

#endif /* _RTC_USER_H */
