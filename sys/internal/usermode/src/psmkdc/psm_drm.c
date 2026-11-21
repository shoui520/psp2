/*
 * ScePsmDrm's calls as the PSM applications link them (the shell, NPXS10015 and NPXS10104 carry
 * the same code): the system calls take the times' pointers in a structure. The names are ours.
 */

#include <npdrm/psmdrm.h>
#include "psmkdc_internal.h"

int KdcRemoveActData(SceUInt64 *pAccountId)
{
	return scePsmDrmRemoveActData(pAccountId);
}

int KdcGetActInfo(int *pActType, int *pVersion, SceUInt64 *pAccountId, SceUInt64 *pStartTime,
		  SceUInt64 *pEndTime)
{
	struct NpDrmCheckActDataArgs times;

	times.startTime = pStartTime;
	times.expirationTime = pEndTime;
	return scePsmDrmGetActInfo(pActType, pVersion, pAccountId, &times);
}

int KdcGetRifInfo(const ScePsmDrmLicense *pLicense, char *pContentId, SceUInt64 *pAccountId,
		  SceUInt64 *pStartTime, SceUInt64 *pEndTime)
{
	struct NpDrmCheckActDataArgs times;

	times.startTime = pStartTime;
	times.expirationTime = pEndTime;
	return scePsmDrmGetRifInfo(pLicense, pContentId, pAccountId, &times);
}

int KdcGetRifName(char *pName, SceUInt64 accountId)
{
	return scePsmDrmGetRifName(pName, accountId);
}

int KdcGetRifNameForInstall(char *pName, const ScePsmDrmLicense *pLicense)
{
	return scePsmDrmGetRifNameForInstall(pName, pLicense);
}

int KdcGetRifKey(const ScePsmDrmLicense *pLicense, ScePsmDrmKeySet *pKeySet, int *pFlags,
		 SceRtcTick *pStartTime, SceRtcTick *pEndTime)
{
	ScePsmDrmExpireTime times;

	times.start_date = pStartTime;
	times.end_date = pEndTime;
	return scePsmDrmGetRifKey(pLicense, pKeySet, pFlags, &times);
}

int KdcGetCurrentSecureTickVsh(SceRtcTick *pTick)
{
	return _vshRtcGetCurrentSecureTick(pTick);
}
