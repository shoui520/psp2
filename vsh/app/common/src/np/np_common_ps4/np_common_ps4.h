/*
 * SceNpCommonPs4's internal exports (vs0:sys/external/np_common_ps4.suprx) other modules call. The
 * library is not converted yet, and no Sony name is known for them: what each takes is read from
 * its callers.
 */
#ifndef NP_COMMON_PS4_NP_COMMON_PS4_H
#define NP_COMMON_PS4_NP_COMMON_PS4_H

#include <scetypes.h>
#include <rtc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* a time the library keeps, UTC: *isSet is nonzero when it has one (SceMsgDialogPlugin shows it
   with its message for 0x80552A09) */
int SceNpCommonPs4_3C570D94(SceUInt8 *isSet, SceRtcTick *tick);

#ifdef __cplusplus
}
#endif

#endif
