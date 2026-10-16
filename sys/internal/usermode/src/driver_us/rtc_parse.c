/*
 * SceDriverUser: SceRtcUser parsing of RFC 3339, RFC 2822 and asctime()
 * dates into ticks (shared with the kernel: sce/rtc_parse.h).
 */

#include "driver_user.h"
#include "rtc_user.h"

#include "sce/rtc_parse.h"
