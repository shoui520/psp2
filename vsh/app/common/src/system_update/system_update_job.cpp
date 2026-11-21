/*
 * system_update_job.cpp: the system update's job base (0x811329B0-0x81132B02): its name, its
 * result and cancellation, and the installed packages' versions.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <update_mgr/update_mgr.h>

#include "system_update_job.h"

#define SYSTEM_UPDATE_ERROR_INVALID	((int)0x80182005)	/* PLACEHOLDER name */
#define SYSTEM_UPDATE_SPKG_VERSION	0x01000010		/* PLACEHOLDER name */

SystemUpdateJob::SystemUpdateJob(SystemUpdateListener *listener)
	: paf::common::SlotJobItem("SceSystemUpdateJob", NULL), m_listener(listener), m_cancelled(false)
{
	m_result.error = 0;
	m_result.reserved = 0;
}

void SystemUpdateJob::SetResult(SystemUpdateResult result)
{
	m_result = result;
}

SystemUpdateResult SystemUpdateJob::GetResult(void)
{
	return m_result;
}

void SystemUpdateJob::Cancel()
{
	m_cancelled = true;
}

/* an installed package's version, by the package's kind (PLACEHOLDER name) */
int Bgdl_81132A2C(int type, SceUInt32 *version)
{
	int packageType;
	SceSblUsSpkgInfo info;

	if (version == NULL)
		return SYSTEM_UPDATE_ERROR_INVALID;
	switch (type) {
	case 0:
		packageType = 1;
		break;
	case 1:
		packageType = 10;
		break;
	case 2:
		packageType = 23;
		break;
	case 3:
		packageType = 13;
		break;
	default:
		return SYSTEM_UPDATE_ERROR_INVALID;
	}

	int ret = sceSblUsGetSpkgInfo(packageType, &info);

	if (ret < 0)
		return ret;
	*version = info.version;
	return 0;
}

/* the manufacturing package is the one updates expect (PLACEHOLDER name) */
bool Bgdl_81132AA2(int *status)
{
	SceUInt32 version = 0;
	int reserved;

	if (status == NULL)
		status = &reserved;
	if (Bgdl_81132A2C(2, &version) < 0) {
		sce_paf_printf_dummy();
		*status = 2;
		return false;
	}
	if (version != SYSTEM_UPDATE_SPKG_VERSION) {
		sce_paf_printf_dummy();
		return false;
	}
	return true;
}
