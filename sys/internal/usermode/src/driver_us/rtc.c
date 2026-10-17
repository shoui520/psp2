/*
 * SceDriverUser: SceRtcUser calendar arithmetic, the user-side copy of the
 * SceRtc library (rtc/rtc_calendar.h, shared with the kernel). The clocks
 * themselves are system calls.
 */

#include <psp2common/kernel/rtc.h>
#include "driver_user.h"
#include "rtc_user.h"
#include <rtc/rtc_calendar.h>
