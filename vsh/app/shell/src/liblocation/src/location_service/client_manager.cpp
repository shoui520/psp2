/*
 * client_manager.cpp: ClientManager ("SceLocationClientList", 8 client slots of 288 bytes each;
 * 0x811912F0-0x81193078, sti 0x81193006). Sony's class name
 * ("ClientManager::NotifyIfPositionConditionMet() ConfirmStart") comes from its log string.
 */
#include "location_internal.h"
#include <appmgr/appmgr.h>
#include <std/string.h>

extern "C" {
SceUInt64 sceRtcGetAccumulativeTime(void);
int SceShell_6B8772A2(int pid);
int SceShell_794BF5BE(int handle);
}

/* 0x811912F0 */
ClientManager::ClientManager(PositionManager *posMgr)
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
			return m_posMgr->GetPermMgr()->CloseApp(privSlot);
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
	int ret = m_posMgr->GetPermMgr()->OpenApp(appId, &privSlot, lmethod);
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
			slot->location.lmethod = *lmethod;
			slot->heading.hmethod = *hmethod;
			slot->location.locInterval = (SceUInt32)locationDecodeInterval(lmethod);
			SceUInt32 headInt = (SceUInt32)locationDecodeDistance(hmethod);
			slot->heading.headInterval = headInt;
			if (headInt == 0)
				slot->heading.headInterval =
					(SceUInt32)m_posMgr->GetBridge()->m_interval;
			slot->heading.cachedHeading = 0.0f;
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
		slot->location.lmethod = *lmethod;
		slot->heading.hmethod = *hmethod;
		slot->location.locInterval = (SceUInt32)locationDecodeInterval(lmethod);
		SceUInt32 headInt = (SceUInt32)locationDecodeDistance(hmethod);
		slot->heading.headInterval = headInt;
		if (headInt == 0)
			slot->heading.headInterval =
				(SceUInt32)m_posMgr->GetBridge()->m_interval;
		slot->heading.cachedHeading = 0.0f;
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
			*lmethod = slot->location.lmethod;
		if (hmethod != NULL)
			*hmethod = slot->heading.hmethod;
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
	m_clients[locationHandleToIndex(handle)].heading.cachedHeading = val;
	return 0;
}

/* 0x811916D8 */
int ClientManager::GetCachedHeading(SceLocationHandle handle, float *out)
{
	if (out == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	*out = m_clients[locationHandleToIndex(handle)].heading.cachedHeading;
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
					AccumulateLMethod(&sum, &m_clients[i].location.lmethod);
					SceUInt32 curInt =
						(SceUInt32)locationDecodeInterval(&lmethod);
					SceUInt32 slotInt = (SceUInt32)locationDecodeInterval(
						&m_clients[i].location.lmethod);
					if (curInt > slotInt)
						lmethod = (lmethod & ~0x70U) |
							  (m_clients[i].location.lmethod & 0x70U);
					if ((m_clients[i].location.lmethod & 0x100) == 0)
						lmethod &= ~0x100U;
					if (m_posMgr->ComparePriority(
						    &hmethod, &m_clients[i].heading.hmethod) == -1)
						hmethod = (hmethod & ~7U) |
							  (m_clients[i].heading.hmethod & 7U);
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
	    m_posMgr->GetPermMgr()->CheckPermission(slot->privacySlot) == 0 &&
	    (slot->stateFlags & 2) == 0) {
		if (slot->shellLockHandle == 0) {
			SceAppMgrAppStatus status;
			ret = sceAppMgrGetStatusByAppId(slot->appId, &status);
			if (ret == 0)
				slot->shellLockHandle = SceShell_6B8772A2(status.processId);
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
			slot->location.distance = distance;
			slot->location.lastLocResult = 0;
			slot->stateFlags |= 8;
			slot->location.lastFailTime = 0;
			slot->location.locElapsed = 0;
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
				active = (m_clients[i].location.getTypeFlags & 2) != 0;
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
				slot->location.queryTimestamp = *queryTs;
			slot->location.queryStartTime = sceRtcGetAccumulativeTime();
			slot->location.queryTimeout = timeout;
		} else {
			slot->stateFlags |= 4;
			slot->location.getStartTime = sceRtcGetAccumulativeTime();
			slot->location.getTimeout = timeout;
			m_activeGetCount++;
		}
		slot->location.getTypeFlags |= typeFlags;
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
		if (slot->pending.error != 0) {
			locationInitLocationInfo(outInfo);
			ret = slot->pending.error;
			outInfo->timestamp = slot->pending.timestamp;
		} else {
			*outInfo = slot->location.queryResultInfo;
			ret = slot->location.queryResultCode;
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
			slot->heading.headAngle = angle;
			slot->heading.lastHeadResult = 0;
			slot->stateFlags |= 0x10;
			slot->heading.headElapsed = 0;
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
			slot->location.distance = 0;
			sce_paf_memset(&slot->location.lastLocInfo, 0,
				       sizeof(slot->location.lastLocInfo));
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
			slot->location.queryTimestamp.tick = 0;
			slot->location.getTypeFlags &= ~4U;
			locationInitLocationInfo(&slot->location.queryResultInfo);
			slot->location.queryResultInfo.timestamp = slot->location.queryTimestamp;
			slot->location.queryResultCode = SCE_LOCATION_INFO_UNDETERMINED_LOCATION;
			slot->location.queryStartTime = 0;
			slot->location.queryTimeout = 0;
			slot->stateFlags &= ~0x40U;
		} else {
			slot->location.getTimeout = 0;
			slot->location.getTypeFlags &= ~3U;
			slot->location.getStartTime = 0;
			slot->stateFlags &= ~0x24U;
			m_activeGetCount--;
		}
		slot->pending.cmd = 0;
		slot->pending.arg = 0;
		slot->pending.timestamp.tick = 0;
		slot->pending.error = 0;
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
			slot->heading.headAngle = 0.0f;
			sce_paf_memset(&slot->heading.lastHeadInfo, 0,
				       sizeof(slot->heading.lastHeadInfo));
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
	if (slot->location.lastLocInfo.timestamp.tick != 0 &&
	    result == slot->location.lastLocResult) {
		if (result == 0) {
			if (slot->location.distance != 0) {
				double dist = m_posMgr->CalcDistance(&slot->location.lastLocInfo,
								     info);
				return (dist >= (double)slot->location.distance) ? 1 : 0;
			}
			return ((slot->location.locElapsed + elapsed) >=
				slot->location.locInterval)
				       ? 1
				       : 0;
		}
		return 0;
	}

	SceUInt64 now = sceRtcGetAccumulativeTime();
	if ((slot->mode & 0xFF) == 0 && result == SCE_LOCATION_INFO_UNDETERMINED_LOCATION) {
		if (slot->location.lastFailTime != 0) {
			(void)((now - slot->location.lastFailTime) / 1000U);
		}
		slot->location.lastFailTime = now;
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
	slot->pending.cmd = cmd;
	slot->pending.arg = arg;
	if (cmd == 8)
		slot->pending.timestamp = *ts;

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
	return m_posMgr->GetPermMgr()->NeedsConfirm(privSlot, outNeeds);
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

	SceUInt64 now = sceRtcGetAccumulativeTime();
	LocationPrivacySettings *permMgr = m_posMgr->GetPermMgr();
	int errOut = 0;
	unsigned char seen = 0;

	for (unsigned char idx = 0; seen < m_count && idx < 8 && errOut == 0; idx++) {
		ClientSlot *slot = &m_clients[idx];
		if ((slot->stateFlags & 1) == 0)
			continue;

		int curRes = result;
		if (curRes != 0) {
			if (m_posMgr->m_config->IsDeviceEnabled() == 0) {
				if ((slot->mode & 0xFF) != 0 || (slot->mode & 0xF0000) == 0x10000)
					curRes = SCE_LOCATION_INFO_DISABLE_DEVICE;
			}
		}

		if ((slot->stateFlags & 4) != 0 || (slot->stateFlags & 0x40) != 0) {
			bool fire = false;
			if (queryTs != NULL) {
				if ((slot->location.getTypeFlags & 4) != 0 &&
				    locationCompareTick(&slot->location.queryTimestamp, queryTs) !=
					    0) {
					if (curRes != SCE_LOCATION_INFO_UNDETERMINED_LOCATION) {
						fire = true;
					} else if (slot->location.queryTimeout != 0 &&
						   (slot->location.queryStartTime > now ||
						    ((SceUInt32)(now -
								 slot->location.queryStartTime) /
						     1000000U) >= slot->location.queryTimeout)) {
						fire = true;
					}
				}
			} else if ((slot->location.getTypeFlags & 4) == 0) {
				if (curRes == 0 || curRes == SCE_LOCATION_INFO_DISABLE_DEVICE ||
				    (slot->location.getTimeout != 0 &&
				     (slot->location.getStartTime > now ||
				      ((SceUInt32)(now - slot->location.getStartTime) / 1000000U) >=
					      slot->location.getTimeout))) {
					fire = true;
				}
			}

			if (fire) {
				if ((slot->location.getTypeFlags & 4) != 0) {
					slot->location.queryResultInfo = *info;
					if (slot->location.queryTimeout != 0 &&
					    (slot->location.queryStartTime > now ||
					     ((SceUInt32)(now - slot->location.queryStartTime) /
					      1000000U) >= slot->location.queryTimeout))
						slot->location.queryResultCode =
							SCE_LOCATION_INFO_TIMEOUT;
					else
						slot->location.queryResultCode = curRes;
					slot->location.queryResultInfo.timestamp = *queryTs;
					m_posMgr->NotifyEvent(locationMakeHandle(idx),
							      slot->location.queryResultCode, 3);
				} else {
					slot->stateFlags |= 0x20;
					m_posMgr->NotifyEvent(locationMakeHandle(idx), curRes, 0);
				}
			}
		}

		if (queryTs == NULL && (slot->stateFlags & 8) != 0) {
			slot->location.locElapsed += elapsed;
			int permErr = permMgr->CheckPermission(slot->privacySlot);
			if (permErr != 0)
				curRes = permErr;

			if (curRes == SCE_LOCATION_INFO_DENIED_BY_USER) {
				SceUInt32 mode = 0;
				GetMode(slot->appId, locationMakeHandle(idx), &mode);
				if ((mode & 0xFFFFFF00U) == 0x100) {
					bool needsConfirm = false;
					bool appActive = false;
					int activeRet =
						m_posMgr->IsAppActive(slot->appId, &appActive);
					if (NeedsConfirm(locationMakeHandle(idx), &needsConfirm) ==
						    0 &&
					    needsConfirm && (slot->stateFlags & 2) == 0 &&
					    activeRet == 0 && appActive) {
						LocationConfirmMgr *confirmMgr =
							locationConfirmGetInstance();
						if (confirmMgr != NULL) {
							unsigned char pend = 0;
							if (GetPendingConfirm(
								    locationMakeHandle(idx),
								    &pend) != 0 ||
							    pend == 0) {
								if (g_locationLogger != NULL)
									g_locationLogger->Log(
										8,
										"ClientManager::NotifyIfPositionConditionMet() ConfirmStart\n");
								confirmMgr->Show(
									slot->appId,
									locationMakeHandle(idx), 0,
									0, NULL, false);
								SetPendingConfirm(
									locationMakeHandle(idx), 1);
								SetPendingRequest(
									locationMakeHandle(idx), 0,
									64, NULL);
								m_posMgr->NotifyEvent(
									locationMakeHandle(idx),
									curRes, 6);
							}
						}
					}
				}
			}

			if (CheckPositionCondition(slot, info, curRes, elapsed) != 0) {
				int nret = m_posMgr->NotifyEvent(locationMakeHandle(idx), curRes,
								 1);
				if (nret != 0)
					errOut = nret;
				slot->location.lastLocInfo = *info;
				slot->location.lastLocResult = curRes;
			}
			if ((slot->location.locElapsed + elapsed) >= slot->location.locInterval)
				slot->location.locElapsed = 0;
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
	if (result != slot->heading.lastHeadResult)
		return 1;
	if (result == 0) {
		if (slot->heading.headAngle == 0.0f)
			return 1;
		float diff = slot->heading.lastHeadInfo.heading - info->heading;
		if (diff < 0.0f)
			diff = -diff;
		double wrap = 360.0 - (double)diff;
		if ((double)diff < wrap)
			wrap = (double)diff;
		if ((float)wrap >= slot->heading.headAngle)
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
			slot->heading.headElapsed += elapsed;
			if (slot->heading.headAngle != 0.0f ||
			    (slot->heading.headElapsed + elapsed) >= slot->heading.headInterval) {
				SceLocationHeadingInfo info;
				m_posMgr->m_headingMgr->GetHeading(&slot->heading.hmethod, &info);
				slot->heading.lastTrueHeading = info.heading;
				if (slot->heading.lastHeadInfo.timestamp.tick == 0 ||
				    CheckHeadingCondition(slot, &info, result) != 0) {
					int nret = m_posMgr->NotifyHeading(locationMakeHandle(idx),
									   result);
					if (nret != 0)
						errOut = nret;
					slot->heading.lastHeadInfo = info;
					slot->heading.lastHeadResult = result;
				}
			}
			if ((slot->heading.headElapsed + elapsed) >= slot->heading.headInterval)
				slot->heading.headElapsed = 0;
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
		return (int)m_clients[locationHandleToIndex(handle)].location.getTypeFlags;
	return 0;
}

/* 0x8119284C */
int ClientManager::CheckPermission(SceLocationHandle handle)
{
	signed char privSlot = m_clients[locationHandleToIndex(handle)].privacySlot;
	return m_posMgr->GetPermMgr()->CheckPermission(privSlot);
}

/* 0x81192874 */
int ClientManager::SetHandlePermission(SceLocationHandle handle, int perm)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		char titleId[32];
		int appId = m_clients[locationHandleToIndex(handle)].appId;
		ret = m_posMgr->m_config->GetAppTitleId(appId, titleId);
		if (ret >= 0) {
			signed char privSlot = m_clients[locationHandleToIndex(handle)].privacySlot;
			ret = m_posMgr->GetPermMgr()->SetPermission(titleId, perm, privSlot);
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
						signed char pairedSlot =
							m_posMgr->GetPermMgr()->FindSlot(
								pairedTitle);
						sce_paf_strncpy(titleId, pairedTitle, 32);
						ret = m_posMgr->GetPermMgr()->SetPermission(
							titleId, perm, pairedSlot);
						if (ret == 0 &&
						    sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
							int seen = 0;
							for (int i = 0; seen < m_count; i++) {
								if ((m_clients[i].stateFlags & 1) !=
								    0) {
									if (m_clients[i].privacySlot ==
									    pairedSlot)
										ret = UpdateShellLock(
											&m_clients[i]);
									seen++;
								}
							}
							int uret = sceKernelUnlockLwMutex(&m_mutex,
											  1);
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
		ret = m_posMgr->GetPermMgr()->GetPermission(privSlot, outPerm);
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
		*outArg = slot->pending.arg;
		*outCmd = slot->pending.cmd;
		if (slot->pending.cmd == 8)
			*outTs = slot->pending.timestamp;
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
		*outCmd = m_clients[locationHandleToIndex(handle)].pending.cmd;
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
		m_clients[locationHandleToIndex(handle)].pending.error = err;
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
					if (status.processRunRequested != 0) {
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

	int clearRet = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (clearRet == 0) {
		int seen = 0;
		for (int i = 0; seen < m_count; i++) {
			if ((m_clients[i].stateFlags & 1) != 0) {
				if (m_clients[i].privacySlot == privSlot &&
				    m_clients[i].location.lastLocResult ==
					    SCE_LOCATION_INFO_DENIED_BY_USER)
					m_clients[i].location.lastLocResult = 0;
				seen++;
			}
		}
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret < 0)
			clearRet = uret;
	}
	return (clearRet != 0) ? clearRet : ret;
}

/* 0x81192DD2 */
int ClientManager::ClearPermissionErrors()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		int seen = 0;
		for (int i = 0; seen < m_count; i++) {
			if ((m_clients[i].stateFlags & 1) != 0) {
				if (m_clients[i].location.lastLocResult ==
				    SCE_LOCATION_INFO_DENIED_BY_USER)
					m_clients[i].location.lastLocResult = 0;
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
		int clearRet = ClearPermissionErrors();
		if (clearRet != 0)
			ret = clearRet;
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
			ret = m_posMgr->GetPermMgr()->ConfirmApp(privSlot);
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
				m_posMgr->IsAppActive(m_clients[i].appId, &active);
				if (active)
					allInactive = 0;
				seen++;
			}
		}
		sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return allInactive;
}
