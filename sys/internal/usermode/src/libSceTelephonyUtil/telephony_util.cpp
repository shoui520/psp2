/*
 * SceTelephonyUtil's exports: each asks the shell (SceShellSvc's IPMI service) about a telephony
 * service. No Sony record names its files; the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include "../../../../../vsh/lowlayer/src/shell_svc/shell_svc_client.h"
#include "telephony_util.h"
#include "telephony_util_ipc_client.h"

extern "C" SceInt32 sceTelephonyIsAvailable(SceUInt32 serviceType)
{
	TelephonyUtilReserved reserved;

	sceClibMemset(&reserved, 0, sizeof(reserved));
	return telephonyUtilIsAvailable(SceShellSvc_B31E7F1C(), &reserved, serviceType);
}

extern "C" SceInt32 SceTelephonyUtil_68D0E38D(SceUInt32 serviceType, SceUInt64 *pValue)
{
	TelephonyUtilReserved reserved;
	SceUInt64 value;
	int ret;

	if (pValue == NULL)
		return SCE_ERROR_ERRNO_EFAULT;
	sceClibMemset(&reserved, 0, sizeof(reserved));
	value = 0;
	ret = telephonyUtilRequest50052(SceShellSvc_B31E7F1C(), &reserved, serviceType, &value);
	*pValue = value;
	return ret;
}

extern "C" SceInt32 SceTelephonyUtil_D7362313(SceUInt32 serviceType)
{
	TelephonyUtilReserved reserved;

	sceClibMemset(&reserved, 0, sizeof(reserved));
	return telephonyUtilRequest50053(SceShellSvc_B31E7F1C(), &reserved, serviceType);
}
