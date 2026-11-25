/*
 * client_manager.cpp: ClientManager ("SceLocationClientList", 8 client slots of 288 bytes each;
 * 0x811912F0-0x81193078, sti 0x81193006). Sony's class name
 * ("ClientManager::NotifyIfPositionConditionMet() ConfirmStart") comes from its log string.
 */
#include "location_service.h"
#include <appmgr/appmgr.h>
#include <std/string.h>

extern "C" {
SceUInt64 sceRtcGetAccumulativeTimeForUser(void);
int SceShell_6B8772A2(int pid);
int SceShell_794BF5BE(int handle);
}

extern void *g_locationLogger;				/* 0x815496B8 */
extern void locationLoggerPrintf(void *logger, int level, const char *fmt, ...);

extern void *locationConfirmGetInstance(void);		/* 0x81187F14 */
extern int locationConfirmShow(void *mgr, int appId, SceLocationHandle handle, SceUInt32 arg,
			       int dataArg);		/* 0x81188902 */

extern void *locationPosMgrGetPermMgr(void *pm);	/* 0x8118BF8C */
extern void *locationPosMgrGetBridge(void *pm);		/* 0x8118BF90 */
extern void *locationPosMgrGetHeadingMgr(void *pm);	/* 0x81409E68 */
extern int locationPosMgrComparePriority(void *pm, SceUInt32 *a, const SceUInt32 *b); /* 0x8118CE9E */
extern int locationPosMgrIsAppActive(void *pm, int appId, bool *outActive); /* 0x8118CEDC */
extern int locationPosMgrNotifyEvent(void *pm, SceLocationHandle handle, int result, int type); /* 0x8118DAEE */
extern int locationPosMgrNotifyHeading(void *pm, SceLocationHandle handle, int result); /* 0x8118DB20 */
extern double locationPosMgrCalcDistance(void *pm, const SceLocationLocationInfo *a,
					 const SceLocationLocationInfo *b); /* 0x8118DB2E */

extern int locationPermOpenApp(void *perm, int appId, signed char *outSlot,
			       const SceUInt32 *lmethod); /* 0x8118EB84 */
extern int locationPermCloseApp(void *perm, signed char slot); /* 0x8118EC74 */
extern signed char locationPermFindSlot(void *perm, const char *titleId); /* 0x8118ECF8 */
extern int locationPermGetPermission(void *perm, signed char slot, int *outPerm); /* 0x8118ED6A */
extern int locationPermSetPermission(void *permMgr, const char *titleId, int perm,
				     signed char slot); /* 0x8118EDC6 */
extern int locationPermCheckPermission(void *perm, signed char slot); /* 0x8118EEB6 */
extern int locationPermConfirmApp(void *perm, signed char slot); /* 0x8118F006 */
extern int locationPermNeedsConfirm(void *perm, signed char slot, bool *outNeeds); /* 0x8118F1F2 */

extern int locationDecodeInterval(const SceUInt32 *lmethod); /* 0x8118F32E */
extern int locationDecodeDistance(const SceUInt32 *lmethod); /* 0x8118F39C */
extern int locationInitLocationInfo(SceLocationLocationInfo *info); /* 0x8118F4BC */
extern int locationCompareTick(const SceRtcTick *a, const SceRtcTick *b); /* 0x8118F54A */
extern SceLocationHandle locationMakeHandle(int idx);	/* 0x8118F562 */
extern unsigned int locationHandleToIndex(SceLocationHandle handle); /* 0x8118F568 */

extern int locationSettingGetAppTitleId(void *setting, int appId, char *outTitleId); /* 0x8118FB08 */
extern int locationHeadingMgrGetHeading(void *hm, const SceUInt32 *hmethod,
					SceLocationHeadingInfo *out); /* 0x811964BE */
extern int locationBridgeGetDefaultHeadInterval(void *bridge); /* 0x8142AE1C */

struct ClientSlot {
	int appId;					/* +0 */
	SceUInt32 mode;					/* +4 */
	SceUInt32 stateFlags;				/* +8 */
	int shellLockHandle;				/* +12 */
	signed char privacySlot;			/* +16 */
	unsigned char pendingConfirm;			/* +17 */
	unsigned char pad18[6];				/* +18..+23 */
	int pendingCmd;					/* +24 */
	int pendingArg;					/* +28 */
	SceRtcTick pendingTimestamp;			/* +32 */
	int pendingError;				/* +40 */
	int pad44;					/* +44 */
	SceUInt32 lmethod;				/* +48 */
	SceUInt32 distance;				/* +52 */
	int lastLocResult;				/* +56 */
	int pad60;					/* +60 */
	SceUInt64 lastFailTime;				/* +64 */
	SceUInt32 locInterval;				/* +72 */
	SceUInt32 locElapsed;				/* +76 */
	SceLocationLocationInfo lastLocInfo;		/* +80..+127 (48B) */
	SceUInt64 getStartTime;				/* +128 */
	SceUInt32 getTimeout;				/* +136 */
	SceUInt32 getTypeFlags;				/* +140 */
	SceRtcTick queryTimestamp;			/* +144 */
	SceUInt64 queryStartTime;			/* +152 */
	SceUInt32 queryTimeout;				/* +160 */
	int pad164;					/* +164 */
	SceLocationLocationInfo queryResultInfo;	/* +168..+215 (48B) */
	int queryResultCode;				/* +216 */
	int pad220;					/* +220 */
	SceUInt32 hmethod;				/* +224 */
	float headAngle;				/* +228 */
	int lastHeadResult;				/* +232 */
	SceUInt32 headInterval;				/* +236 */
	SceUInt32 headElapsed;				/* +240 */
	float cachedHeading;				/* +244 */
	float lastTrueHeading;				/* +248 */
	int pad252;					/* +252 */
	SceLocationHeadingInfo lastHeadInfo;		/* +256..+287 (32B) */
};

struct MethodBitSummary {
	int pad0;
	unsigned char hasGps;				/* +4 */
	unsigned char hasWifi;				/* +5 */
	unsigned char has3G;				/* +6 */
	unsigned char hasEmu;				/* +7 */
	unsigned char pad8;
};

struct LocationSettingVtbl {
	virtual void vfn0();
	virtual void vfn1();
	virtual void vfn2();
	virtual void vfn3();
	virtual int HasGps();
	virtual int IsEmulationMode();
	virtual int IsDeviceEnabled();			/* +24 */
};

class ClientManager {
public:
	ClientManager(void *posMgr);			/* 0x811912F0 */
	virtual ~ClientManager();			/* 0x81191348 / 0x81191372 */

	int Init();					/* 0x811913A2 */
	int GetCount();					/* 0x811913C8 */
	bool IsValidHandle(int appId, SceLocationHandle handle); /* 0x811913CE */
	int Close(int appId, SceLocationHandle handle);	/* 0x811913F2 */
	int Open(int appId, SceLocationHandle *handle, SceUInt32 *lmethod, SceUInt32 *hmethod,
		 SceUInt32 *mode);			/* 0x81191482 */
	int Reopen(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
		   SceUInt32 *hmethod);			/* 0x8119159A */
	int GetMethod(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
		      SceUInt32 *hmethod);		/* 0x81191638 */
	int SetCachedHeading(SceLocationHandle handle, float val); /* 0x811916B2 */
	int GetCachedHeading(SceLocationHandle handle, float *out); /* 0x811916D8 */
	static void AccumulateLMethod(MethodBitSummary *sum, const SceUInt32 *lmethod); /* 0x81191706 */
	int GetActiveMethods(SceUInt32 *outLMethod, SceUInt32 *outHMethod); /* 0x81191748 */
	int UpdateShellLock(ClientSlot *slot);		/* 0x811918E4 */
	int StartLocationCallback(int appId, SceLocationHandle handle, SceUInt32 distance); /* 0x8119195E */
	int HasActiveWifiHistory(unsigned char *out);	/* 0x811919FA */
	int StartGetLocation(SceLocationHandle handle, SceUInt32 timeout, SceUInt32 typeFlags,
			     const SceRtcTick *queryTs); /* 0x81191A70 */
	int GetQueryResult(SceLocationHandle handle, SceLocationLocationInfo *outInfo); /* 0x81191B42 */
	int StartHeadingCallback(int appId, SceLocationHandle handle, float angle); /* 0x81191BCE */
	int StopLocationCallback(int appId, SceLocationHandle handle); /* 0x81191C60 */
	int CancelGetLocation(SceLocationHandle handle, SceUInt32 typeFlags); /* 0x81191CF8 */
	int StopHeadingCallback(int appId, SceLocationHandle handle); /* 0x81191DFA */
	int CheckPositionCondition(ClientSlot *slot, const SceLocationLocationInfo *info,
				   int result, SceUInt32 elapsed); /* 0x81191E8A */
	int SetPendingRequest(SceLocationHandle handle, int arg, int cmd, const SceRtcTick *ts); /* 0x81191F7E */
	int SetPendingConfirm(SceLocationHandle handle, unsigned char flag); /* 0x81191FDC */
	int GetPendingConfirm(SceLocationHandle handle, unsigned char *outFlag); /* 0x8119201E */
	int NeedsConfirm(SceLocationHandle handle, bool *outNeeds); /* 0x81192062 */
	int GetMode(int appId, SceLocationHandle handle, SceUInt32 *outMode); /* 0x8119208A */
	int NotifyIfPositionConditionMet(const SceLocationLocationInfo *info, int result,
					 SceUInt32 elapsed, const SceRtcTick *queryTs); /* 0x811920CC */
	int CheckHeadingCondition(ClientSlot *slot, const SceLocationHeadingInfo *info, int result); /* 0x811925EA */
	int NotifyHeadingUpdate(int result, SceUInt32 elapsed); /* 0x81192678 */
	int GetStateFlags(int appId, SceLocationHandle handle); /* 0x811927DA */
	int GetTypeFlags(int appId, SceLocationHandle handle); /* 0x81192812 */
	int CheckPermission(SceLocationHandle handle);	/* 0x8119284C */
	int SetHandlePermission(SceLocationHandle handle, int perm); /* 0x81192874 */
	int GetHandlePermission(SceLocationHandle handle, int *outPerm); /* 0x81192AD0 */
	int GetPendingRequest(int *outAppId, SceLocationHandle handle, int *outArg, int *outCmd,
			      SceRtcTick *outTs);	/* 0x81192B24 */
	int GetPendingCmd(SceLocationHandle handle, int *outCmd); /* 0x81192BBA */
	int SetPendingError(SceLocationHandle handle, int err); /* 0x81192BFE */
	int GetActiveCount(int *outCount);		/* 0x81192C40 */
	int OnPrivacySlotChanged(signed char privSlot);	/* 0x81192CFC */
	int ClearPermissionErrors();			/* 0x81192DD2 */
	int OnGlobalPermissionChanged();		/* 0x81192E32 */
	int ConfirmHandle(int appId, SceLocationHandle handle); /* 0x81192F04 */
	int AreAllAppsInactive();			/* 0x81192F72 */

	void *m_posMgr;					/* +4 */
	ClientSlot m_clients[8];			/* +8..+2311 (0x900) */
	signed char m_count;				/* +2312 */
	int m_mutexResult;				/* +2316 */
	SceKernelLwMutexWork m_mutex;			/* +2320..+2351 */
	int m_activeGetCount;				/* +2352 */
};

/* 0x811912F0 */
ClientManager::ClientManager(void *posMgr)
	: m_posMgr(posMgr)
	, m_count(0)
	, m_mutexResult(-1)
	, m_activeGetCount(0)
{
	sce_paf_memset(m_clients, 0, sizeof(m_clients));
}

/* 0x81191348 / 0x81191372 */
ClientManager::~ClientManager()
{
	if (m_mutexResult == 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}
}

/* 0x811913A2 */
int ClientManager::Init()
{
	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, "SceLocationClientList", 2, 0, NULL);
	return m_mutexResult;
}

/* 0x811913C8 */
int ClientManager::GetCount()
{
	return m_count;
}

/* 0x811913CE */
bool ClientManager::IsValidHandle(int appId, SceLocationHandle handle)
{
	return m_clients[locationHandleToIndex(handle)].appId == appId;
}

/* 0x811913F2 */
int ClientManager::Close(int appId, SceLocationHandle handle)
{
	if (m_clients[locationHandleToIndex(handle)].appId != appId)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;

	signed char privSlot = m_clients[locationHandleToIndex(handle)].privacySlot;
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		sce_paf_memset(&m_clients[locationHandleToIndex(handle)], 0, sizeof(ClientSlot));
		m_count--;
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (ret == 0)
			return locationPermCloseApp(locationPosMgrGetPermMgr(m_posMgr), privSlot);
	}
	return ret;
}

/* 0x81191482 */
int ClientManager::Open(int appId, SceLocationHandle *handle, SceUInt32 *lmethod,
			SceUInt32 *hmethod, SceUInt32 *mode)
{
	if (m_count >= 8)
		return SCE_LOCATION_ERROR_TOO_MANY_HANDLES;

	signed char privSlot = -1;
	int ret = locationPermOpenApp(locationPosMgrGetPermMgr(m_posMgr), appId, &privSlot, lmethod);
	if (ret == 0) {
		signed char idx = 0;
		for (; idx < 8; idx++) {
			if (m_clients[idx].stateFlags == 0)
				break;
		}
		if (idx >= 8)
			return ret;

		ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
		if (ret == 0) {
			ClientSlot *slot = &m_clients[idx];
			sce_paf_memset(slot, 0, sizeof(ClientSlot));
			slot->appId = appId;
			slot->mode = *mode;
			slot->stateFlags = 1;
			slot->privacySlot = privSlot;
			slot->lmethod = *lmethod;
			slot->hmethod = *hmethod;
			slot->locInterval = (SceUInt32)locationDecodeInterval(lmethod);
			SceUInt32 headInt = (SceUInt32)locationDecodeDistance(hmethod);
			slot->headInterval = headInt;
			if (headInt == 0)
				slot->headInterval = (SceUInt32)locationBridgeGetDefaultHeadInterval(
					locationPosMgrGetBridge(m_posMgr));
			slot->cachedHeading = 0.0f;
			m_count++;
			ret = sceKernelUnlockLwMutex(&m_mutex, 1);
			if (ret == 0) {
				*handle = locationMakeHandle(idx);
				return 0;
			}
		}
	}
	return ret;
}

/* 0x8119159A */
int ClientManager::Reopen(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
			  SceUInt32 *hmethod)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	if (m_clients[locationHandleToIndex(handle)].appId == appId) {
		ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
		slot->lmethod = *lmethod;
		slot->hmethod = *hmethod;
		slot->locInterval = (SceUInt32)locationDecodeInterval(lmethod);
		SceUInt32 headInt = (SceUInt32)locationDecodeDistance(hmethod);
		slot->headInterval = headInt;
		if (headInt == 0)
			slot->headInterval = (SceUInt32)locationBridgeGetDefaultHeadInterval(
				locationPosMgrGetBridge(m_posMgr));
		slot->cachedHeading = 0.0f;
		ret = 0;
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return ret;
}

/* 0x81191638 */
int ClientManager::GetMethod(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
			     SceUInt32 *hmethod)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	if (m_clients[locationHandleToIndex(handle)].appId == appId) {
		ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
		if (lmethod != NULL)
			*lmethod = slot->lmethod;
		if (hmethod != NULL)
			*hmethod = slot->hmethod;
		ret = 0;
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return ret;
}

/* 0x811916B2 */
int ClientManager::SetCachedHeading(SceLocationHandle handle, float val)
{
	m_clients[locationHandleToIndex(handle)].cachedHeading = val;
	return 0;
}

/* 0x811916D8 */
int ClientManager::GetCachedHeading(SceLocationHandle handle, float *out)
{
	if (out == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	*out = m_clients[locationHandleToIndex(handle)].cachedHeading;
	return 0;
}

/* 0x81191706 */
void ClientManager::AccumulateLMethod(MethodBitSummary *sum, const SceUInt32 *lmethod)
{
	switch (*lmethod & 0xF) {
	case 1:
		sum->hasGps = 1;
		sum->hasWifi = 1;
		sum->has3G = 1;
		break;
	case 2:
		sum->hasWifi = 1;
		sum->has3G = 1;
		break;
	case 3:
		sum->hasWifi = 1;
		break;
	case 4:
		sum->has3G = 1;
		break;
	case 5:
		sum->hasGps = 1;
		break;
	case 15:
		sum->hasEmu = 1;
		break;
	default:
		break;
	}
}

/* 0x81191748 */
int ClientManager::GetActiveMethods(SceUInt32 *outLMethod, SceUInt32 *outHMethod)
{
	SceUInt32 lmethod = 0x150;
	SceUInt32 hmethod = 0;

	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		if (m_count != 0) {
			MethodBitSummary sum;
			sce_paf_memset(&sum, 0, sizeof(sum));
			int seen = 0;
			for (int i = 0; seen < m_count; i++) {
				if ((m_clients[i].stateFlags & 1) != 0) {
					AccumulateLMethod(&sum, &m_clients[i].lmethod);
					SceUInt32 curInt = (SceUInt32)locationDecodeInterval(&lmethod);
					SceUInt32 slotInt = (SceUInt32)locationDecodeInterval(&m_clients[i].lmethod);
					if (curInt > slotInt)
						lmethod = (lmethod & ~0x70U) | (m_clients[i].lmethod & 0x70U);
					if ((m_clients[i].lmethod & 0x100) == 0)
						lmethod &= ~0x100U;
					if (locationPosMgrComparePriority(m_posMgr, &hmethod,
									  &m_clients[i].hmethod) == -1)
						hmethod = (hmethod & ~7U) | (m_clients[i].hmethod & 7U);
					seen++;
				}
			}
			SceUInt32 mode = 0;
			if (sum.hasEmu != 0)
				mode = 15;
			else if (sum.hasGps != 0 && (sum.hasWifi != 0 || sum.has3G != 0))
				mode = 1;
			else if (sum.hasWifi != 0 && sum.has3G != 0)
				mode = 2;
			else if (sum.hasGps != 0)
				mode = 5;
			else if (sum.hasWifi != 0)
				mode = 3;
			else if (sum.has3G != 0)
				mode = 4;
			lmethod = (lmethod & ~0xFU) | mode;
		} else {
			lmethod = (lmethod & ~0x17FU) | 1U;
		}
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	*outLMethod = lmethod;
	*outHMethod = hmethod;
	return ret;
}

/* 0x811918E4 */
int ClientManager::UpdateShellLock(ClientSlot *slot)
{
	int ret = 0;
	if (((slot->stateFlags & 8) != 0 || (slot->stateFlags & 4) != 0 ||
	     (slot->stateFlags & 0x40) != 0) &&
	    locationPermCheckPermission(locationPosMgrGetPermMgr(m_posMgr), slot->privacySlot) == 0 &&
	    (slot->stateFlags & 2) == 0) {
		if (slot->shellLockHandle == 0) {
			SceAppMgrAppStatus status;
			ret = sceAppMgrGetStatusByAppId(slot->appId, &status);
			if (ret == 0)
				slot->shellLockHandle = SceShell_6B8772A2(*(int *)((char *)&status + 52));
		}
	} else {
		if (slot->shellLockHandle != 0) {
			SceShell_794BF5BE(slot->shellLockHandle);
			slot->shellLockHandle = 0;
		}
	}
	return ret;
}

/* 0x8119195E */
int ClientManager::StartLocationCallback(int appId, SceLocationHandle handle, SceUInt32 distance)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	if (m_clients[locationHandleToIndex(handle)].appId == appId) {
		ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
		if ((slot->stateFlags & 8) != 0) {
			ret = SCE_LOCATION_ERROR_MULTIPLE_CALLBACK;
		} else {
			slot->distance = distance;
			slot->lastLocResult = 0;
			slot->stateFlags |= 8;
			slot->lastFailTime = 0;
			slot->locElapsed = 0;
			ret = UpdateShellLock(slot);
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return ret;
}

/* 0x811919FA */
int ClientManager::HasActiveWifiHistory(unsigned char *out)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		bool active = false;
		int seen = 0;
		for (int i = 0; seen < m_count && i < 8 && !active; i++) {
			if ((m_clients[i].stateFlags & 1) != 0) {
				active = (m_clients[i].getTypeFlags & 2) != 0;
				seen++;
			}
		}
		*out = active ? 1 : 0;
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81191A70 */
int ClientManager::StartGetLocation(SceLocationHandle handle, SceUInt32 timeout,
				    SceUInt32 typeFlags, const SceRtcTick *queryTs)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
	if ((typeFlags == 4 && (slot->stateFlags & 0x40) != 0) ||
	    (typeFlags != 4 && (slot->stateFlags & 4) != 0)) {
		ret = SCE_LOCATION_ERROR_MULTIPLE_GET_LOCATION;
	} else {
		if (typeFlags == 4) {
			slot->stateFlags |= 0x40;
			if (queryTs != NULL)
				slot->queryTimestamp = *queryTs;
			slot->queryStartTime = sceRtcGetAccumulativeTimeForUser();
			slot->queryTimeout = timeout;
		} else {
			slot->stateFlags |= 4;
			slot->getStartTime = sceRtcGetAccumulativeTimeForUser();
			slot->getTimeout = timeout;
			m_activeGetCount++;
		}
		slot->getTypeFlags |= typeFlags;
		ret = UpdateShellLock(slot);
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return ret;
}

/* 0x81191B42 */
int ClientManager::GetQueryResult(SceLocationHandle handle, SceLocationLocationInfo *outInfo)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
		if (slot->pendingError != 0) {
			locationInitLocationInfo(outInfo);
			ret = slot->pendingError;
			outInfo->timestamp = slot->pendingTimestamp;
		} else {
			*outInfo = slot->queryResultInfo;
			ret = slot->queryResultCode;
		}
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81191BCE */
int ClientManager::StartHeadingCallback(int appId, SceLocationHandle handle, float angle)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	if (m_clients[locationHandleToIndex(handle)].appId == appId) {
		ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
		if ((slot->stateFlags & 0x10) != 0) {
			ret = SCE_LOCATION_ERROR_MULTIPLE_CALLBACK;
		} else {
			slot->headAngle = angle;
			slot->lastHeadResult = 0;
			slot->stateFlags |= 0x10;
			slot->headElapsed = 0;
			ret = 0;
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return ret;
}

/* 0x81191C60 */
int ClientManager::StopLocationCallback(int appId, SceLocationHandle handle)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	if (m_clients[locationHandleToIndex(handle)].appId == appId) {
		ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
		if ((slot->stateFlags & 8) != 0) {
			slot->distance = 0;
			sce_paf_memset(&slot->lastLocInfo, 0, sizeof(slot->lastLocInfo));
			slot->stateFlags &= ~8U;
			ret = UpdateShellLock(slot);
		} else {
			ret = SCE_LOCATION_ERROR_NOT_RUNNING_CALLBACK;
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return ret;
}

/* 0x81191CF8 */
int ClientManager::CancelGetLocation(SceLocationHandle handle, SceUInt32 typeFlags)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
	if ((typeFlags != 4 && (slot->stateFlags & 4) != 0) ||
	    (typeFlags == 4 && (slot->stateFlags & 0x40) != 0)) {
		if (typeFlags == 4) {
			slot->queryTimestamp.tick = 0;
			slot->getTypeFlags &= ~4U;
			locationInitLocationInfo(&slot->queryResultInfo);
			slot->queryResultInfo.timestamp = slot->queryTimestamp;
			slot->queryResultCode = SCE_LOCATION_INFO_UNDETERMINED_LOCATION;
			slot->queryStartTime = 0;
			slot->queryTimeout = 0;
			slot->stateFlags &= ~0x40U;
		} else {
			slot->getTimeout = 0;
			slot->getTypeFlags &= ~3U;
			slot->getStartTime = 0;
			slot->stateFlags &= ~0x24U;
			m_activeGetCount--;
		}
		slot->pendingCmd = 0;
		slot->pendingArg = 0;
		slot->pendingTimestamp.tick = 0;
		slot->pendingError = 0;
		ret = UpdateShellLock(slot);
	} else {
		ret = SCE_LOCATION_INFO_GET_LOCATION_CANCELED;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return ret;
}

/* 0x81191DFA */
int ClientManager::StopHeadingCallback(int appId, SceLocationHandle handle)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	if (m_clients[locationHandleToIndex(handle)].appId == appId) {
		ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
		if ((slot->stateFlags & 0x10) != 0) {
			slot->headAngle = 0.0f;
			sce_paf_memset(&slot->lastHeadInfo, 0, sizeof(slot->lastHeadInfo));
			slot->stateFlags &= ~0x10U;
			ret = 0;
		} else {
			ret = SCE_LOCATION_ERROR_NOT_RUNNING_CALLBACK;
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return ret;
}

/* 0x81191E8A */
int ClientManager::CheckPositionCondition(ClientSlot *slot, const SceLocationLocationInfo *info,
					  int result, SceUInt32 elapsed)
{
	if (slot->lastLocInfo.timestamp.tick != 0 && result == slot->lastLocResult) {
		if (result == 0) {
			if (slot->distance != 0) {
				double dist = locationPosMgrCalcDistance(m_posMgr, &slot->lastLocInfo, info);
				return (dist >= (double)slot->distance) ? 1 : 0;
			}
			return ((slot->locElapsed + elapsed) >= slot->locInterval) ? 1 : 0;
		}
		return 0;
	}

	SceUInt64 now = sceRtcGetAccumulativeTimeForUser();
	if ((slot->mode & 0xFF) == 0 && result == SCE_LOCATION_INFO_UNDETERMINED_LOCATION) {
		if (slot->lastFailTime != 0) {
			(void)((now - slot->lastFailTime) / 1000U);
		}
		slot->lastFailTime = now;
		return 0;
	}
	if ((slot->mode & 0xFFFFFF00U) != 0x100 || slot->pendingConfirm == 0)
		return 1;
	return 0;
}

/* 0x81191F7E */
int ClientManager::SetPendingRequest(SceLocationHandle handle, int arg, int cmd,
				     const SceRtcTick *ts)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
	slot->pendingCmd = cmd;
	slot->pendingArg = arg;
	if (cmd == 8)
		slot->pendingTimestamp = *ts;

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return 0;
}

/* 0x81191FDC */
int ClientManager::SetPendingConfirm(SceLocationHandle handle, unsigned char flag)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		m_clients[locationHandleToIndex(handle)].pendingConfirm = flag;
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x8119201E */
int ClientManager::GetPendingConfirm(SceLocationHandle handle, unsigned char *outFlag)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		*outFlag = m_clients[locationHandleToIndex(handle)].pendingConfirm;
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81192062 */
int ClientManager::NeedsConfirm(SceLocationHandle handle, bool *outNeeds)
{
	signed char privSlot = m_clients[locationHandleToIndex(handle)].privacySlot;
	return locationPermNeedsConfirm(locationPosMgrGetPermMgr(m_posMgr), privSlot, outNeeds);
}

/* 0x8119208A */
int ClientManager::GetMode(int appId, SceLocationHandle handle, SceUInt32 *outMode)
{
	if (m_clients[locationHandleToIndex(handle)].appId == appId) {
		*outMode = m_clients[locationHandleToIndex(handle)].mode;
		return 0;
	}
	return SCE_LOCATION_ERROR_INVALID_HANDLE;
}

/* 0x811920CC */
int ClientManager::NotifyIfPositionConditionMet(const SceLocationLocationInfo *info, int result,
						SceUInt32 elapsed, const SceRtcTick *queryTs)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	SceUInt64 now = sceRtcGetAccumulativeTimeForUser();
	void *permMgr = locationPosMgrGetPermMgr(m_posMgr);
	int errOut = 0;
	unsigned char seen = 0;

	for (unsigned char idx = 0; seen < m_count && idx < 8 && errOut == 0; idx++) {
		ClientSlot *slot = &m_clients[idx];
		if ((slot->stateFlags & 1) == 0)
			continue;

		int curRes = result;
		if (curRes != 0) {
			LocationSettingVtbl *setting =
				*(LocationSettingVtbl **)((char *)m_posMgr + 24);
			if (setting->IsDeviceEnabled() == 0) {
				if ((slot->mode & 0xFF) != 0 || (slot->mode & 0xF0000) == 0x10000)
					curRes = SCE_LOCATION_INFO_DISABLE_DEVICE;
			}
		}

		if ((slot->stateFlags & 4) != 0 || (slot->stateFlags & 0x40) != 0) {
			bool fire = false;
			if (queryTs != NULL) {
				if ((slot->getTypeFlags & 4) != 0 &&
				    locationCompareTick(&slot->queryTimestamp, queryTs) != 0) {
					if (curRes != SCE_LOCATION_INFO_UNDETERMINED_LOCATION) {
						fire = true;
					} else if (slot->queryTimeout != 0 &&
						   (slot->queryStartTime > now ||
						    ((SceUInt32)(now - slot->queryStartTime) / 1000000U) >=
							    slot->queryTimeout)) {
						fire = true;
					}
				}
			} else if ((slot->getTypeFlags & 4) == 0) {
				if (curRes == 0 || curRes == SCE_LOCATION_INFO_DISABLE_DEVICE ||
				    (slot->getTimeout != 0 &&
				     (slot->getStartTime > now ||
				      ((SceUInt32)(now - slot->getStartTime) / 1000000U) >=
					      slot->getTimeout))) {
					fire = true;
				}
			}

			if (fire) {
				if ((slot->getTypeFlags & 4) != 0) {
					slot->queryResultInfo = *info;
					if (slot->queryTimeout != 0 &&
					    (slot->queryStartTime > now ||
					     ((SceUInt32)(now - slot->queryStartTime) / 1000000U) >=
						     slot->queryTimeout))
						slot->queryResultCode = SCE_LOCATION_INFO_TIMEOUT;
					else
						slot->queryResultCode = curRes;
					slot->queryResultInfo.timestamp = *queryTs;
					locationPosMgrNotifyEvent(m_posMgr, locationMakeHandle(idx),
								  slot->queryResultCode, 3);
				} else {
					slot->stateFlags |= 0x20;
					locationPosMgrNotifyEvent(m_posMgr, locationMakeHandle(idx),
								  curRes, 0);
				}
			}
		}

		if (queryTs == NULL && (slot->stateFlags & 8) != 0) {
			slot->locElapsed += elapsed;
			int permErr = locationPermCheckPermission(permMgr, slot->privacySlot);
			if (permErr != 0)
				curRes = permErr;

			if (curRes == SCE_LOCATION_INFO_DENIED_BY_USER) {
				SceUInt32 mode = 0;
				GetMode(slot->appId, locationMakeHandle(idx), &mode);
				if ((mode & 0xFFFFFF00U) == 0x100) {
					bool needsConfirm = false;
					bool appActive = false;
					int activeRet = locationPosMgrIsAppActive(m_posMgr, slot->appId,
										  &appActive);
					if (NeedsConfirm(locationMakeHandle(idx), &needsConfirm) == 0 &&
					    needsConfirm && (slot->stateFlags & 2) == 0 &&
					    activeRet == 0 && appActive) {
						void *confirmMgr = locationConfirmGetInstance();
						if (confirmMgr != NULL) {
							unsigned char pend = 0;
							if (GetPendingConfirm(locationMakeHandle(idx),
									      &pend) != 0 ||
							    pend == 0) {
								if (g_locationLogger != NULL)
									locationLoggerPrintf(
										g_locationLogger, 8,
										"ClientManager::NotifyIfPositionConditionMet() ConfirmStart\n");
								locationConfirmShow(confirmMgr,
										    slot->appId,
										    locationMakeHandle(idx),
										    0, 0);
								SetPendingConfirm(locationMakeHandle(idx), 1);
								SetPendingRequest(locationMakeHandle(idx), 0, 64, NULL);
								locationPosMgrNotifyEvent(
									m_posMgr,
									locationMakeHandle(idx),
									curRes, 6);
							}
						}
					}
				}
			}

			if (CheckPositionCondition(slot, info, curRes, elapsed) != 0) {
				int nret = locationPosMgrNotifyEvent(m_posMgr, locationMakeHandle(idx),
								     curRes, 1);
				if (nret != 0)
					errOut = nret;
				slot->lastLocInfo = *info;
				slot->lastLocResult = curRes;
			}
			if ((slot->locElapsed + elapsed) >= slot->locInterval)
				slot->locElapsed = 0;
		}
		seen++;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return errOut;
}

/* 0x811925EA */
int ClientManager::CheckHeadingCondition(ClientSlot *slot, const SceLocationHeadingInfo *info,
					 int result)
{
	if (result != slot->lastHeadResult)
		return 1;
	if (result == 0) {
		if (slot->headAngle == 0.0f)
			return 1;
		float diff = slot->lastHeadInfo.heading - info->heading;
		if (diff < 0.0f)
			diff = -diff;
		double wrap = 360.0 - (double)diff;
		if ((double)diff < wrap)
			wrap = (double)diff;
		if ((float)wrap >= slot->headAngle)
			return 1;
	}
	return 0;
}

/* 0x81192678 */
int ClientManager::NotifyHeadingUpdate(int result, SceUInt32 elapsed)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	int errOut = 0;
	int seen = 0;
	for (int idx = 0; seen < m_count && idx < 8 && errOut == 0; idx++) {
		ClientSlot *slot = &m_clients[idx];
		if ((slot->stateFlags & 1) == 0)
			continue;

		if ((slot->stateFlags & 0x10) != 0) {
			slot->headElapsed += elapsed;
			if (slot->headAngle != 0.0f ||
			    (slot->headElapsed + elapsed) >= slot->headInterval) {
				SceLocationHeadingInfo info;
				locationHeadingMgrGetHeading(locationPosMgrGetHeadingMgr(m_posMgr),
							     &slot->hmethod, &info);
				slot->lastTrueHeading = info.heading;
				if (slot->lastHeadInfo.timestamp.tick == 0 ||
				    CheckHeadingCondition(slot, &info, result) != 0) {
					int nret = locationPosMgrNotifyHeading(
						m_posMgr, locationMakeHandle(idx), result);
					if (nret != 0)
						errOut = nret;
					slot->lastHeadInfo = info;
					slot->lastHeadResult = result;
				}
			}
			if ((slot->headElapsed + elapsed) >= slot->headInterval)
				slot->headElapsed = 0;
		}
		seen++;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return errOut;
}

/* 0x811927DA */
int ClientManager::GetStateFlags(int appId, SceLocationHandle handle)
{
	if (m_clients[locationHandleToIndex(handle)].appId == appId)
		return (int)m_clients[locationHandleToIndex(handle)].stateFlags;
	return 0;
}

/* 0x81192812 */
int ClientManager::GetTypeFlags(int appId, SceLocationHandle handle)
{
	if (m_clients[locationHandleToIndex(handle)].appId == appId)
		return (int)m_clients[locationHandleToIndex(handle)].getTypeFlags;
	return 0;
}

/* 0x8119284C */
int ClientManager::CheckPermission(SceLocationHandle handle)
{
	signed char privSlot = m_clients[locationHandleToIndex(handle)].privacySlot;
	return locationPermCheckPermission(locationPosMgrGetPermMgr(m_posMgr), privSlot);
}

/* 0x81192874 */
int ClientManager::SetHandlePermission(SceLocationHandle handle, int perm)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		char titleId[32];
		void *setting = *(void **)((char *)m_posMgr + 24);
		int appId = m_clients[locationHandleToIndex(handle)].appId;
		ret = locationSettingGetAppTitleId(setting, appId, titleId);
		if (ret >= 0) {
			signed char privSlot = m_clients[locationHandleToIndex(handle)].privacySlot;
			ret = locationPermSetPermission(locationPosMgrGetPermMgr(m_posMgr),
							titleId, perm, privSlot);
			if (ret == 0) {
				if (sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
					int seen = 0;
					for (int i = 0; seen < m_count; i++) {
						if ((m_clients[i].stateFlags & 1) != 0) {
							if (m_clients[i].privacySlot == privSlot)
								ret = UpdateShellLock(&m_clients[i]);
							seen++;
						}
					}
					int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
					if (uret != 0)
						ret = uret;
				}
				if (ret == 0) {
					const char *pairedTitle = NULL;
					if (sce_paf_strncmp(titleId, "NPXS10004", 32) == 0)
						pairedTitle = "NPXS10095";
					else if (sce_paf_strncmp(titleId, "NPXS10095", 32) == 0)
						pairedTitle = "NPXS10004";
					if (pairedTitle != NULL) {
						signed char pairedSlot = locationPermFindSlot(
							locationPosMgrGetPermMgr(m_posMgr),
							pairedTitle);
						sce_paf_strncpy(titleId, pairedTitle, 32);
						ret = locationPermSetPermission(
							locationPosMgrGetPermMgr(m_posMgr),
							titleId, perm, pairedSlot);
						if (ret == 0 &&
						    sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
							int seen = 0;
							for (int i = 0; seen < m_count; i++) {
								if ((m_clients[i].stateFlags & 1) != 0) {
									if (m_clients[i].privacySlot == pairedSlot)
										ret = UpdateShellLock(&m_clients[i]);
									seen++;
								}
							}
							int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
							if (uret != 0)
								ret = uret;
						}
					}
				}
			}
		}
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81192AD0 */
int ClientManager::GetHandlePermission(SceLocationHandle handle, int *outPerm)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		signed char privSlot = m_clients[locationHandleToIndex(handle)].privacySlot;
		ret = locationPermGetPermission(locationPosMgrGetPermMgr(m_posMgr), privSlot, outPerm);
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81192B24 */
int ClientManager::GetPendingRequest(int *outAppId, SceLocationHandle handle, int *outArg,
				     int *outCmd, SceRtcTick *outTs)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		ClientSlot *slot = &m_clients[locationHandleToIndex(handle)];
		*outAppId = slot->appId;
		*outArg = slot->pendingArg;
		*outCmd = slot->pendingCmd;
		if (slot->pendingCmd == 8)
			*outTs = slot->pendingTimestamp;
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81192BBA */
int ClientManager::GetPendingCmd(SceLocationHandle handle, int *outCmd)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		*outCmd = m_clients[locationHandleToIndex(handle)].pendingCmd;
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81192BFE */
int ClientManager::SetPendingError(SceLocationHandle handle, int err)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		m_clients[locationHandleToIndex(handle)].pendingError = err;
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81192C40 */
int ClientManager::GetActiveCount(int *outCount)
{
	int count = 0;
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		int seen = 0;
		for (int i = 0; seen < m_count; i++) {
			if ((m_clients[i].stateFlags & 1) != 0) {
				SceAppMgrAppStatus status;
				int sret = sceAppMgrGetStatusByAppId(m_clients[i].appId, &status);
				if (sret != 0) {
					ret = sret;
				} else {
					if (*(int *)((char *)&status + 60) != 0) {
						count++;
						m_clients[i].stateFlags &= ~2U;
					} else {
						m_clients[i].stateFlags |= 2U;
					}
					UpdateShellLock(&m_clients[i]);
				}
				seen++;
			}
		}
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			ret = uret;
	}
	*outCount = count;
	return ret;
}

/* 0x81192CFC */
int ClientManager::OnPrivacySlotChanged(signed char privSlot)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		int seen = 0;
		for (int i = 0; seen < m_count; i++) {
			if ((m_clients[i].stateFlags & 1) != 0) {
				if (m_clients[i].privacySlot == privSlot)
					ret = UpdateShellLock(&m_clients[i]);
				seen++;
			}
		}
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret != 0)
			ret = uret;
	}

	int ret2 = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret2 == 0) {
		int seen = 0;
		for (int i = 0; seen < m_count; i++) {
			if ((m_clients[i].stateFlags & 1) != 0) {
				if (m_clients[i].privacySlot == privSlot &&
				    m_clients[i].lastLocResult == SCE_LOCATION_INFO_DENIED_BY_USER)
					m_clients[i].lastLocResult = 0;
				seen++;
			}
		}
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			ret2 = uret;
	}
	return (ret2 != 0) ? ret2 : ret;
}

/* 0x81192DD2 */
int ClientManager::ClearPermissionErrors()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		int seen = 0;
		for (int i = 0; seen < m_count; i++) {
			if ((m_clients[i].stateFlags & 1) != 0) {
				if (m_clients[i].lastLocResult == SCE_LOCATION_INFO_DENIED_BY_USER)
					m_clients[i].lastLocResult = 0;
				seen++;
			}
		}
		return sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return ret;
}

/* 0x81192E32 */
int ClientManager::OnGlobalPermissionChanged()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		if (sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
			int seen = 0;
			for (int i = 0; seen < m_count; i++) {
				if ((m_clients[i].stateFlags & 1) != 0) {
					ret = UpdateShellLock(&m_clients[i]);
					seen++;
				}
			}
			int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
			if (uret != 0)
				ret = uret;
		}
		int ret2 = ClearPermissionErrors();
		if (ret2 != 0)
			ret = ret2;
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81192F04 */
int ClientManager::ConfirmHandle(int appId, SceLocationHandle handle)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
		if (m_clients[locationHandleToIndex(handle)].appId == appId) {
			signed char privSlot = m_clients[locationHandleToIndex(handle)].privacySlot;
			ret = locationPermConfirmApp(locationPosMgrGetPermMgr(m_posMgr), privSlot);
		}
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81192F72 */
int ClientManager::AreAllAppsInactive()
{
	int allInactive = 1;
	if (sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
		int seen = 0;
		for (int i = 0; seen < m_count && allInactive != 0; i++) {
			if ((m_clients[i].stateFlags & 1) != 0) {
				bool active = false;
				locationPosMgrIsAppActive(m_posMgr, m_clients[i].appId, &active);
				if (active)
					allInactive = 0;
				seen++;
			}
		}
		sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return allInactive;
}
