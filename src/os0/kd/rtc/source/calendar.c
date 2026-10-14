/*
 * SceRtc calendar arithmetic (sce/rtc_calendar.h, shared with the user
 * library). The kernel exports the functions by NID; the user library's
 * names map onto them here.
 */

#include "rtc_internal.h"

#define sceRtcGetTickResolution		SceRtcForDriver_6965E5DF
#define sceRtcIsLeapYear		SceRtcForDriver_6F96FAA5
#define sceRtcGetDaysInMonth		SceRtcForDriver_C2DC38D9
#define sceRtcGetDayOfWeek		SceRtcForDriver_0F800878
#define sceRtcGetDayOfYear		SceRtcForDriver_9097EF0D
#define sceRtcCheckValid		SceRtcForDriver_ADF63D7C
#define sceRtcSetDosTime		SceRtcForDriver_EB27F94E
#define sceRtcGetDosTime		SceRtcForDriver_09AF419B
#define sceRtcSetWin32FileTime		SceRtcForDriver_BA14600F
#define sceRtcGetWin32FileTime		SceRtcForDriver_1DD4C5B6
#define sceRtcSetTime64_t		SceRtcForDriver_F4E23829
#define sceRtcSetTime_t			SceRtcForDriver_DE9F4515
#define sceRtcGetTime64_t		ksceRtcConvertDateTimeToUnixTime
#define sceRtcGetTime_t			SceRtcForDriver_FFD7A04F
#define sceRtcSetTick			ksceRtcConvertTickToDateTime
#define sceRtcGetTick			SceRtcForDriver_F139E359
#define sceRtcTickAddTicks		SceRtcForDriver_A82AC5B3
#define sceRtcTickAddMicroseconds	SceRtcForDriver_CE51C2A1
#define sceRtcTickAddSeconds		SceRtcForDriver_9C8AF264
#define sceRtcTickAddMinutes		SceRtcForDriver_0F9E02EF
#define sceRtcTickAddHours		SceRtcForDriver_32E50C36
#define sceRtcTickAddDays		SceRtcForDriver_0CB72FAF
#define sceRtcTickAddWeeks		SceRtcForDriver_24209FBF
#define sceRtcTickAddMonths		SceRtcForDriver_A8CDB6BD
#define sceRtcTickAddYears		SceRtcForDriver_85F3026C
#define sceRtcCompareTick		SceRtcForDriver_0EBB3F12

#include "sce/rtc_calendar.h"
