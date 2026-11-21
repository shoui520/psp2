/*
 * The PS4s found: the search list the state machine reads, filled by the UDP search job from a
 * list of its own. One mutex guards every list ("SceCompanionUtil::CSearchListMutex").
 */
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

static SceKernelLwMutexWork s_searchListMutex;
static bool s_searchListMutexCreated;

int CSearchList::CreateMutex(void)
{
	int ret;

	if (s_searchListMutexCreated)
		return -1;
	ret = sceKernelCreateLwMutex(&s_searchListMutex, "SceCompanionUtil::CSearchListMutex",
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret >= 0) {
		s_searchListMutexCreated = true;
		return 0;
	}
	return ret;
}

int CSearchList::DeleteMutex(void)
{
	int ret;

	if (!s_searchListMutexCreated)
		return -1;
	ret = sceKernelDeleteLwMutex(&s_searchListMutex);
	if (ret >= 0) {
		s_searchListMutexCreated = false;
		return 0;
	}
	return ret;
}

void CSearchList::Init(SceUInt32 capacity)
{
	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	m_count = 0;
	m_capacity = 0;
	m_hosts = NULL;
	m_hosts = (CompanionUtilHost *)Allocate(capacity * sizeof(CompanionUtilHost));
	if (m_hosts != NULL) {
		sceClibMemset(m_hosts, 0, capacity * sizeof(CompanionUtilHost));
		m_capacity = capacity;
		m_count = 0;
	}
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
}

void CSearchList::Term(void)
{
	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	if (m_hosts != NULL) {
		Free(m_hosts);
		m_hosts = NULL;
	}
	m_count = 0;
	m_capacity = 0;
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
}

SceUInt32 CSearchList::GetCapacity(void)
{
	SceUInt32 capacity;

	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	capacity = m_capacity;
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
	return capacity;
}

void CSearchList::Release(void)
{
	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	if (m_hosts != NULL) {
		Free(m_hosts);
		m_hosts = NULL;
	}
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
}

void CSearchList::Clear(void)
{
	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	sceClibMemset(m_hosts, 0, m_capacity * sizeof(CompanionUtilHost));
	m_count = 0;
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
}

void CSearchList::GetResult(CompanionUtilSearchResult *result)
{
	CompanionUtilHostInfo *info;
	CompanionUtilHost *host;
	SceUInt32 i;

	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	result->count = m_count;
	info = result->hosts;
	for (i = 0; i < m_count; i++) {
		host = At(i);
		info->address = host->address;
		info->port = host->port;
		info->status = host->status;
		sceClibStrncpy(info->hostName, host->hostName, sizeof(info->hostName));
		sceClibStrncpy(info->hostId, host->hostId, sizeof(info->hostId));
		info++;
	}
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
}

void CSearchList::CopyTo(CSearchList *list)
{
	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	if (list->m_capacity >= m_capacity) {
		list->m_count = m_count;
		sceClibMemcpy_safe(list->m_hosts, m_hosts, m_capacity * sizeof(CompanionUtilHost));
	}
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
}

void CSearchList::Update(CSearchList *list)
{
	SceUInt32 count;
	SceUInt32 i;

	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	sceClibMemset(m_hosts, 0, m_capacity * sizeof(CompanionUtilHost));
	count = 0;
	m_count = count;
	for (i = 0; i < list->m_count; i++) {
		if (list->At(i)->lastSeen > 0 && count < m_capacity) {
			sceClibMemcpy_safe(&m_hosts[count], list->At(i), sizeof(CompanionUtilHost));
			count = m_count + 1;
			m_count = count;
		}
	}
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
}

SceUInt16 CSearchList::GetStatus(const char *hostId)
{
	SceUInt16 status = 0;
	CompanionUtilHost *host;
	SceUInt32 i;

	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	for (i = 0; i < m_count; i++) {
		host = At(i);
		if (sceClibStrncmp(host->hostId, hostId, sizeof(host->hostId)) == 0) {
			status = host->status;
			break;
		}
	}
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
	return status;
}

SceUInt32 CSearchList::GetAddress(const char *hostId)
{
	SceUInt32 address = 0;
	CompanionUtilHost *host;
	SceUInt32 i;

	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	for (i = 0; i < m_count; i++) {
		host = At(i);
		if (sceClibStrncmp(host->hostId, hostId, sizeof(host->hostId)) == 0) {
			address = host->address;
			break;
		}
	}
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
	return address;
}

bool CSearchList::IsSeenAfter(const char *hostId, SceUInt64 time)
{
	bool seen = false;
	CompanionUtilHost *host;
	SceUInt32 i;

	sceKernelLockLwMutex(&s_searchListMutex, 1, NULL);
	for (i = 0; i < m_count; i++) {
		host = At(i);
		if (sceClibStrncmp(host->hostId, hostId, sizeof(host->hostId)) == 0) {
			if (time < host->lastSeen)
				seen = true;
			break;
		}
	}
	sceKernelUnlockLwMutex(&s_searchListMutex, 1);
	return seen;
}

} /* namespace SceCompanionUtil */
