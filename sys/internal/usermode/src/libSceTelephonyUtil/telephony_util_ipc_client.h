/* the shell's telephony methods (telephony_util_ipc_client.cpp; the names are ours) */
#ifndef LIBSCETELEPHONYUTIL_TELEPHONY_UTIL_IPC_CLIENT_H
#define LIBSCETELEPHONYUTIL_TELEPHONY_UTIL_IPC_CLIENT_H

#include <scetypes.h>
#include "../../../../../vsh/lowlayer/src/ipmi/client.h"
#include "telephony_util.h"

int telephonyUtilIsAvailable(IPMI::Client *client, const TelephonyUtilReserved *pReserved,
			     SceUInt32 serviceType);
int telephonyUtilRequest50052(IPMI::Client *client, const TelephonyUtilReserved *pReserved,
			      SceUInt32 serviceType, SceUInt64 *pValue);
int telephonyUtilRequest50053(IPMI::Client *client, const TelephonyUtilReserved *pReserved,
			      SceUInt32 serviceType);

#endif
