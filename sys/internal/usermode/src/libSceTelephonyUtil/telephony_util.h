/*
 * SceTelephonyUtil (vs0:sys/external/libSceTelephonyUtil.suprx): the shell's telephony service
 * (SceShellSvc's IPMI methods 0x50051-0x50053). sceTelephonyIsAvailable is Sony's name (1.60's
 * messages); the other exports' names are not known.
 */
#ifndef LIBSCETELEPHONYUTIL_TELEPHONY_UTIL_H
#define LIBSCETELEPHONYUTIL_TELEPHONY_UTIL_H

#include <scetypes.h>

/* the second input of every method: eight bytes the library always sends zeroed */
typedef struct TelephonyUtilReserved {
	SceUInt32 reserved[2];
} TelephonyUtilReserved;

#ifdef __cplusplus
extern "C" {
#endif

SceInt32 sceTelephonyIsAvailable(SceUInt32 serviceType);
SceInt32 SceTelephonyUtil_68D0E38D(SceUInt32 serviceType, SceUInt64 *pValue);
SceInt32 SceTelephonyUtil_D7362313(SceUInt32 serviceType);

#ifdef __cplusplus
}
#endif

#endif
