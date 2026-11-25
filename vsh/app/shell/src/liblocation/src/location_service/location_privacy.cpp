/*
 * location_privacy.cpp: per-application privacy/permission table ("SceLocationPrivacySettings")
 * (0x8118E9F4-0x8118F32C, sti 0x8118F2BA). The file and class names are ours (PLACEHOLDER).
 */
#include "location_service.h"
#include <std/string.h>
#include <system/system.h>

extern void *locationPosMgrGetInstance(void);
extern class LocationPrivacySettings *locationPosMgrGetPrivacy(void *posMgr);

extern int locationSettingGetAppTitleId(void *setting, int appId, char *titleId);
extern int locationSettingIsDebugApp(void *setting, const char *titleId);
extern int locationSettingGetAppPermission(void *setting, const char *titleId, int *perm,
					   unsigned char *flag);
extern int locationSettingSetAppPermission(void *setting, const char *titleId, int perm);
extern int locationSettingUpdateCounter(void *setting, const char *titleId, int counter);
extern int locationSettingGetLastCounter(void *setting, const char *titleId,
					 SceUInt32 *lastCounter);
extern int locationSettingGetParental(void *setting);
extern int locationSettingIsServiceEnabled(void *setting);
extern int locationSettingGetUpdateCounter(void *setting);

struct LocationProviderCtrl {
	virtual void vfn0();
	virtual void vfn1();
	virtual int Start();		/* +8 */
	virtual int Stop();		/* +12 */
	virtual void vfn4();
	virtual int Resume();		/* +20 */
	virtual int Suspend();		/* +24 */
};

struct LocationPrivacyEntry {
	char titleId[32];		/* +0 */
	int permission;			/* +32 */
	unsigned char dirty;		/* +36 */
	unsigned char pad[3];
	int lastCounter;		/* +40 */
	unsigned char refCount;		/* +44 */
	unsigned char pad2[3];
};

class LocationPrivacySettings {
public:
	LocationPrivacySettings(void *setting);
	virtual ~LocationPrivacySettings();

	int Init();
	signed char AddOrIncrement(const LocationPrivacyEntry *entry);
	int OpenApp(int appId, signed char *outSlot, const SceUInt32 *lmethod);
	int CloseApp(signed char slot);
	signed char FindSlot(const char *titleId);
	int GetPermission(signed char slot, int *outPerm);
	int SetPermission(const char *titleId, int perm, signed char slot);
	int CheckPermission(signed char slot);
	bool IsGloballyEnabled();
	bool HasAnyAllowedApp();
	int ProviderStart(LocationProviderCtrl *provider);
	int ProviderStop(LocationProviderCtrl *provider);
	int ProviderResume(LocationProviderCtrl *provider);
	int ProviderSuspend(LocationProviderCtrl *provider);
	int ConfirmApp(signed char slot);
	int NeedsConfirm(signed char slot, bool *outNeeds);
	int SetAllPermissions(int perm);

	LocationPrivacyEntry m_entries[8];	/* +4 (384 bytes -> +388) */
	unsigned char m_count;			/* +388 */
	int m_mutexResult;			/* +392 */
	int m_pad396;				/* +396 */
	SceKernelLwMutexWork m_mutex;		/* +400 */
	void *m_setting;			/* +432 */
	unsigned char m_providerActive;		/* +436 */
};

/* 0x8118E9F4 */
LocationPrivacySettings::LocationPrivacySettings(void *setting)
{
	m_setting = setting;
	sce_paf_memset(m_entries, 0, sizeof(m_entries));
	m_providerActive = 0;
	m_count = 0;
	m_mutexResult = -1;
}

/* 0x8118EA2C / 0x8118EA4E */
LocationPrivacySettings::~LocationPrivacySettings()
{
	if (m_mutexResult == 0)
		sceKernelDeleteLwMutex(&m_mutex);
}

/* 0x8118EA76 */
int LocationPrivacySettings::Init()
{
	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, "SceLocationPrivacySettings", 2, 0, NULL);
	return m_mutexResult;
}

/* 0x8118EA9E */
signed char LocationPrivacySettings::AddOrIncrement(const LocationPrivacyEntry *entry)
{
	signed char result = -1;
	if (m_count < 8) {
		signed char idx = 0;
		int seen = 0;
		int freeIdx = -1;
		int found = 0;
		if (m_count != 0) {
			while (!found) {
				if (m_entries[idx].refCount != 0) {
					if (sce_paf_strncmp(m_entries[idx].titleId,
							    entry->titleId, 32) == 0) {
						result = idx;
						m_entries[idx].refCount++;
						found = 1;
					}
					seen++;
				} else {
					freeIdx = idx;
				}
				idx++;
				if (seen >= m_count) {
					if (found)
						return result;
					if (freeIdx != -1)
						goto store_entry;
					goto use_idx;
				}
			}
			return result;
		}
use_idx:
		freeIdx = idx;
store_entry:
		m_entries[freeIdx] = *entry;
		result = (signed char)freeIdx;
		m_count++;
	}
	return result;
}

/* 0x8118EB84 */
int LocationPrivacySettings::OpenApp(int appId, signed char *outSlot, const SceUInt32 *lmethod)
{
	LocationPrivacyEntry entry;
	sce_paf_memset(&entry, 0, sizeof(entry));

	int ret = locationSettingGetAppTitleId(m_setting, appId, entry.titleId);
	if (ret == 0) {
		if (locationSettingIsDebugApp(m_setting, entry.titleId) != 0)
			*((unsigned char *)m_setting + 57) = 1;

		ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
		if (ret == 0) {
			unsigned char flag = 0;
			ret = locationSettingGetAppPermission(m_setting, entry.titleId,
							      &entry.permission, &flag);
			if (ret == 0) {
				entry.dirty = flag;
				if ((*lmethod & 0xF) != 0 && entry.permission == 0) {
					ret = SCE_LOCATION_ERROR_DISABLE_APPLICATION;
				} else {
					entry.refCount = 1;
					SceUInt32 lastCounter = 0;
					ret = locationSettingGetLastCounter(m_setting,
									    entry.titleId,
									    &lastCounter);
					if (ret == 0) {
						entry.lastCounter = (int)lastCounter;
						signed char slot = AddOrIncrement(&entry);
						*outSlot = slot;
						if (slot < 0)
							ret = SCE_LOCATION_ERROR_FATAL_ERROR;
					}
				}
			}
			sceKernelUnlockLwMutex(&m_mutex, 1);
		}
	}
	return ret;
}

/* 0x8118EC74 */
int LocationPrivacySettings::CloseApp(signed char slot)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	if ((unsigned int)slot < 8) {
		ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
		if (ret == 0) {
			if (m_count != 0) {
				unsigned char rc = m_entries[slot].refCount;
				if (rc != 0) {
					m_entries[slot].refCount = rc - 1;
					if (rc == 1) {
						if (locationSettingIsDebugApp(m_setting,
									      m_entries[slot].titleId) != 0)
							*((unsigned char *)m_setting + 57) = 0;
						m_entries[slot].titleId[0] = '\0';
						m_count--;
					}
					ret = 0;
				}
			}
			sceKernelUnlockLwMutex(&m_mutex, 1);
		}
	}
	return ret;
}

/* 0x8118ECF8 */
signed char LocationPrivacySettings::FindSlot(const char *titleId)
{
	signed char result = -1;
	if (sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
		if (m_count != 0) {
			int i = 0;
			int found = 0;
			while (!found) {
				if (m_entries[i].refCount == 0 ||
				    sce_paf_strncmp(m_entries[i].titleId, titleId, 32) != 0) {
					i++;
					if (i >= 8)
						break;
				} else {
					found = 1;
					result = (signed char)i;
				}
			}
		}
		sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return result;
}

/* 0x8118ED6A */
int LocationPrivacySettings::GetPermission(signed char slot, int *outPerm)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	if ((unsigned int)slot < 8) {
		ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
		if (ret == 0) {
			if (m_count != 0) {
				if (m_entries[slot].refCount != 0) {
					ret = 0;
					*outPerm = m_entries[slot].permission;
				}
			}
			sceKernelUnlockLwMutex(&m_mutex, 1);
		}
	}
	return ret;
}

/* 0x8118EDC6 */
int LocationPrivacySettings::SetPermission(const char *titleId, int perm, signed char slot)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		ret = locationSettingSetAppPermission(m_setting, titleId, perm);
		if (ret == 0) {
			if (slot == -1)
				slot = FindSlot(titleId);
			if ((unsigned int)slot < 8) {
				ret = SCE_LOCATION_ERROR_FATAL_ERROR;
				if (m_count != 0 && m_entries[slot].refCount != 0) {
					m_entries[slot].permission = perm;
					ret = 0;
					m_entries[slot].dirty = 1;
				}
			}
		}
		sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return ret;
}

/* 0x8118EEB6 */
int LocationPrivacySettings::CheckPermission(signed char slot)
{
	if ((unsigned int)slot >= 8)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	int perm = m_entries[slot].permission;
	if (perm == 0)
		return SCE_LOCATION_ERROR_DISABLE_APPLICATION;
	if (paf::system::Model::GetType() == 1)
		return SCE_LOCATION_INFO_DENIED_BY_USER;
	if (*(int *)((char *)m_setting + 64) == 0)
		return SCE_LOCATION_INFO_DENIED_BY_USER;
	if (locationSettingGetParental(m_setting) == 0)
		return SCE_LOCATION_INFO_DENIED_BY_USER;
	if (perm == 1)
		return SCE_LOCATION_INFO_DENIED_BY_USER;
	if (locationSettingIsServiceEnabled(m_setting) == 0)
		return SCE_LOCATION_INFO_DENIED_BY_USER;
	if (perm == 2)
		return SCE_LOCATION_INFO_DENIED_BY_USER;
	return 0;
}

/* 0x8118EF34 */
bool LocationPrivacySettings::IsGloballyEnabled()
{
	if (locationSettingGetParental(m_setting) != 0 &&
	    locationSettingIsServiceEnabled(m_setting) != 0)
		return *(int *)((char *)m_setting + 64) != 0;
	return false;
}

/* 0x8118EF60 */
bool LocationPrivacySettings::HasAnyAllowedApp()
{
	bool allowed = false;
	if (sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
		int count = m_count;
		if (count != 0) {
			int seen = 0;
			int i = 0;
			do {
				if (allowed)
					break;
				if (m_entries[i].refCount != 0) {
					allowed = (m_entries[i].permission == 3);
					seen++;
				}
				i++;
			} while (seen < count);
		}
		sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return allowed;
}

/* 0x8118EFAA */
int LocationPrivacySettings::ProviderStart(LocationProviderCtrl *provider)
{
	return provider->Start();
}

/* 0x8118EFB6 */
int LocationPrivacySettings::ProviderStop(LocationProviderCtrl *provider)
{
	return provider->Stop();
}

/* 0x8118EFC2 */
int LocationPrivacySettings::ProviderResume(LocationProviderCtrl *provider)
{
	if (m_providerActive != 0)
		return 0;
	int ret = provider->Resume();
	if (ret == 0)
		m_providerActive = 1;
	return ret;
}

/* 0x8118EFE4 */
int LocationPrivacySettings::ProviderSuspend(LocationProviderCtrl *provider)
{
	if (m_providerActive == 0)
		return 0;
	int ret = provider->Suspend();
	if (ret == 0)
		m_providerActive = 0;
	return ret;
}

/* 0x8118F006 */
int LocationPrivacySettings::ConfirmApp(signed char slot)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	if ((unsigned int)slot < 8) {
		ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
		if (ret == 0) {
			if (m_entries[slot].refCount != 0) {
				int counter = locationSettingGetUpdateCounter(m_setting);
				ret = locationSettingUpdateCounter(m_setting,
								   m_entries[slot].titleId,
								   counter);
				if (ret == 0) {
					m_entries[slot].dirty = 0;
					m_entries[slot].lastCounter = counter;
					signed char pairedSlot = -1;
					if (sce_paf_strncmp(m_entries[slot].titleId,
							    "NPXS10004", 32) == 0) {
						void *mgr = locationPosMgrGetInstance();
						if (mgr != NULL) {
							LocationPrivacySettings *priv =
								locationPosMgrGetPrivacy(mgr);
							pairedSlot = priv->FindSlot("NPXS10095");
						}
					} else if (sce_paf_strncmp(m_entries[slot].titleId,
								   "NPXS10095", 32) == 0) {
						void *mgr = locationPosMgrGetInstance();
						if (mgr != NULL) {
							LocationPrivacySettings *priv =
								locationPosMgrGetPrivacy(mgr);
							pairedSlot = priv->FindSlot("NPXS10004");
						}
					}
					if ((unsigned int)pairedSlot < 8 &&
					    m_entries[pairedSlot].refCount != 0) {
						ret = locationSettingUpdateCounter(
							m_setting,
							m_entries[pairedSlot].titleId,
							counter);
						if (ret == 0) {
							m_entries[pairedSlot].dirty = 0;
							m_entries[pairedSlot].lastCounter = counter;
						}
					}
				}
			} else {
				ret = SCE_LOCATION_ERROR_FATAL_ERROR;
			}
			sceKernelUnlockLwMutex(&m_mutex, 1);
		}
	}
	return ret;
}

/* 0x8118F1F2 */
int LocationPrivacySettings::NeedsConfirm(signed char slot, bool *outNeeds)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	if ((unsigned int)slot < 8) {
		ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
		if (ret == 0) {
			if (m_entries[slot].refCount != 0) {
				*outNeeds = (m_entries[slot].dirty != 0) ||
					    (m_entries[slot].lastCounter !=
					     locationSettingGetUpdateCounter(m_setting));
				ret = 0;
			} else {
				ret = SCE_LOCATION_ERROR_FATAL_ERROR;
			}
			sceKernelUnlockLwMutex(&m_mutex, 1);
		}
	}
	return ret;
}

/* 0x8118F26C */
int LocationPrivacySettings::SetAllPermissions(int perm)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		int count = m_count;
		if (count != 0) {
			int seen = 0;
			int i = 0;
			do {
				if (m_entries[i].refCount != 0) {
					m_entries[i].permission = perm;
					seen++;
					m_entries[i].dirty = 1;
				}
				i++;
			} while (seen < count);
		}
		sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return ret;
}
