/*
 * SceRtc calendar arithmetic (rtc/rtc_calendar.h, shared with the user
 * library). The kernel exports the functions by NID; the user library's
 * names map onto them here.
 */

#include "rtc_internal.h"

#define sceRtcGetTickResolution		sceRtcGetTickResolution
#define sceRtcIsLeapYear		sceRtcIsLeapYear
#define sceRtcGetDaysInMonth		sceRtcGetDaysInMonth
#define sceRtcGetDayOfWeek		sceRtcGetDayOfWeek
#define sceRtcGetDayOfYear		sceRtcGetDayOfYear
#define sceRtcSetDosTime		sceRtcSetDosTime
#define sceRtcGetDosTime		sceRtcGetDosTime
#define sceRtcSetWin32FileTime		sceRtcSetWin32FileTime
#define sceRtcGetWin32FileTime		sceRtcGetWin32FileTime
#define sceRtcSetTime64_t		sceRtcSetTime64_t
#define sceRtcSetTime_t			sceRtcSetTime_t
#define sceRtcGetTime64_t		sceRtcGetTime64_t
#define sceRtcGetTime_t			sceRtcGetTime_t
#define sceRtcSetTick			sceRtcSetTick
#define sceRtcGetTick			sceRtcGetTick
#define sceRtcTickAddTicks		sceRtcTickAddMicroseconds
#define sceRtcTickAddMicroseconds	sceRtcTickAddTicks
#define sceRtcTickAddSeconds		sceRtcTickAddSeconds
#define sceRtcTickAddHours		sceRtcTickAddHours
#define sceRtcTickAddDays		sceRtcTickAddDays
#define sceRtcTickAddWeeks		sceRtcTickAddWeeks
#define sceRtcTickAddMonths		sceRtcTickAddMonths
#define sceRtcTickAddYears		sceRtcTickAddYears
#define sceRtcCompareTick		sceRtcCompareTick

/* the kernel build inlines these helpers into their callers */
#define RTC_CALENDAR_INLINE inline
#include <rtc/rtc_calendar.h>
