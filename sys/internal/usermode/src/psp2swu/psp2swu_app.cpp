/*
 * SwuApplication and entry point main() for pup:psp2swu.self
 * (sys/internal/usermode/src/psp2swu/psp2swu_app.cpp).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>
#include <std/new>
#include <std/stdlib.h>
#include <std/string.h>
#include <update_mgr/update_mgr.h>
#include <updater_core/updater_core.h>
#include <vshbridge/vshbridge.h>
#include "../cui_setupper/cui_view.h"
#include "index_dat.h"
#include "psp2swu_app.h"
#include "update_cui_view.h"
#include "update_gui_view.h"
#include "update_view_base.h"

#define SWU_ERROR_NO_MEMORY			((SceInt32)0x80180001)
#define SWU_ERROR_NOT_FOUND			((SceInt32)0x80180004)
#define SWU_ERROR_UNSET				((SceInt32)0x80180008)
#define SWU_ERROR_DIFF_BASE_BUILD_MISMATCH	((SceInt32)0x80182414)
#define SWU_ERROR_DIFF_BASE_VERSION_MISMATCH	((SceInt32)0x80182415)
#define SWU_ERROR_RELEASE_CHECK_MODE		((SceInt32)0x80182802)

#define SWU_VS0_MOUNT_ID			0x300
#define SWU_NVS_DIFF_UPDATE_AREA		4
#define SWU_NVS_DIFF_UPDATE_ACTIVE		0x10
#define SWU_NVS_DIFF_UPDATE_CLEARED		0xFF
#define SWU_CUI_NAME_BUFFER_SIZE		64
#define SWU_MAX_RETRY_COUNT			2

#define SWU_SWINFO_KIND_INT			0
#define SWU_SWINFO_KIND_STR			1
#define SWU_SWINFO_MAPPING_COUNT		6

#define SWU_SPKG_TYPE_SYSTEM			1
#define SWU_SPKG_TYPE_SYSCON			21
#define SWU_SPKG_INFO_SYSCON			8

/* PAF global heap size for pup:psp2swu.self (10 MiB) */
SceUInt32 sce_paf_global_heap_size = 0x00A00000;

/* Process and main-thread parameters exported through crt0's sceProcessParam */
extern "C" const unsigned int sce_process_preload_disabled = 0x01FB0000;
extern "C" const char sceUserMainThreadName[] = "ScePsp2Swu";
extern "C" const char sceProcessName[] = "ScePsp2Swu";
extern "C" const int sceUserMainThreadPriority = 0x100000F2;

extern "C" char *cuiNameBuffer;

extern "C" void __cxa_pure_virtual(void)
{
	__builtin_breakpoint(0);
}

static const SwInfoIndexMapping s_swInfoMappings[SWU_SWINFO_MAPPING_COUNT] = {
	{ "sys-repository-path", "SysRepository", SWU_SWINFO_KIND_STR },
	{ "sys-revision",        "SysRevision",   SWU_SWINFO_KIND_INT },
	{ "sys-build-number",    "SysBuildNumber", SWU_SWINFO_KIND_INT },
	{ "vsh-repository-path", "VshRepository", SWU_SWINFO_KIND_STR },
	{ "vsh-revision",        "VshRevision",   SWU_SWINFO_KIND_INT },
	{ "vsh-build-number",    "VshBuildNumber", SWU_SWINFO_KIND_INT },
};

SwuApplication *SwuApplication::s_instance = NULL;
UpdateViewBase *SwuApplication::s_view = NULL;

static bool IsGuiUpdateMode(SceUInt8 mode)
{
	return (mode >= 0x10 && mode <= 0x13) ||
	       (mode >= 0x20 && mode <= 0x23) ||
	       (mode >= 0x40 && mode <= 0x43);
}

SwuApplication::SwuApplication()
	: m_firstError(SWU_ERROR_UNSET),
	  m_updateFlagError(0),
	  m_retryCount(-1),
	  m_clearUpdateMode(1)
{
	s_instance = this;
}

SwuApplication::~SwuApplication()
{
}

void SwuApplication::RecordFirstError(SceInt32 errorCode)
{
	if (m_firstError == SWU_ERROR_UNSET)
		m_firstError = errorCode;
}

SceInt32 SwuApplication::GetRetryCount(SceInt32 *outRetryCount)
{
	*outRetryCount = m_retryCount;
	return 0;
}

SceInt32 SwuApplication::Finalize()
{
	SceInt32 ret = sceUpdaterPowerUnlock();

	if (ret != 0) {
		RecordFirstError(ret);
		CuiInformUpdateFinished(ret, "finalize power lock failed.", 27);
	}
	if (m_firstError == SWU_ERROR_UNSET)
		m_firstError = 0;

	sceUpdaterUpdateModeSetLastError(m_firstError);
	sceUpdaterLogEnd();

	bool shutdown = false;

	if (m_firstError != 0) {
		SceUInt8 mode = 0;
		SceInt32 modeRet = sceUpdaterGetUpdateMode(&mode);

		if (modeRet != 0) {
			RecordFirstError(modeRet);
			shutdown = (m_retryCount >= SWU_MAX_RETRY_COUNT);
		} else if (!IsGuiUpdateMode(mode)) {
			shutdown = (m_retryCount >= SWU_MAX_RETRY_COUNT);
		}
	}

	ret = sceUpdaterFinalize(shutdown ? 1 : 0);
	if (ret != 0) {
		RecordFirstError(ret);
		CuiInformUpdateFinished(ret, "finalize core failed.", 21);
	}
	return ret;
}

SceInt32 SwuApplication::WriteVersionSwInfo()
{
	SceUpdaterSpkgHeader *header = static_cast<SceUpdaterSpkgHeader *>(
		sce_paf_memalign(8, sizeof(SceUpdaterSpkgHeader)));

	if (header == NULL) {
		sceClibPrintf("[ERROR]%s\n", "no memory.");
		return SWU_ERROR_NO_MEMORY;
	}

	header->version = 0;

	SceInt32 ret = sceUpdaterGetSpackageHeader(SWU_SPKG_TYPE_SYSTEM, header);

	if (ret == SWU_ERROR_NOT_FOUND) {
		SceSblUsSpkgInfo spkgInfo;

		if (sceSblUsGetSpkgInfo(SWU_SPKG_TYPE_SYSTEM, &spkgInfo) == 0)
			header->version = spkgInfo.version;
	} else if (ret < 0) {
		sceClibPrintf("[WARNING]%s: %x\n", "get sys spkg header failed.", ret);
		sce_paf_free(header);
		return ret;
	}

	SceInt32 swInfoRet = CuiSetSwInfoInt("SdkVersion", 10, header->version);

	if (swInfoRet < 0)
		sceClibPrintf("[WARNING]%s: %x\n", "set sys ver. failed.", swInfoRet);

	header->version = 0;
	ret = sceUpdaterGetSpackageHeader(SWU_SPKG_TYPE_SYSCON, header);
	if (ret == SWU_ERROR_NOT_FOUND) {
		SceSblUsSpkgInfo spkgInfo;

		if (sceSblUsGetSpkgInfo(SWU_SPKG_INFO_SYSCON, &spkgInfo) == 0)
			header->version = spkgInfo.version;
	} else if (ret < 0) {
		sceClibPrintf("[WARNING]%s: %x\n", "get sc spkg header failed.", ret);
		sce_paf_free(header);
		return ret;
	}

	if (header->version != 0) {
		swInfoRet = CuiSetSwInfoInt("ScVersion", 9, header->version);
		if (swInfoRet < 0)
			sceClibPrintf("[WARNING]%s: %x\n", "set sc ver. failed.", swInfoRet);
	} else {
		sceClibPrintf("[INFO]%s\n", "sc ver is not avilable.");
	}

	sce_paf_free(header);
	return 0;
}

SceInt32 SwuApplication::WriteIndexSwInfoEntry(const SwInfoIndexMapping *mapping, const char *value)
{
	switch (mapping->valueKind) {
	case SWU_SWINFO_KIND_INT: {
		SceUInt32 numValue = static_cast<SceUInt32>(sce_paf_atoi(value));
		SceSize keyLen = sce_paf_strlen(mapping->swInfoKey);

		return CuiSetSwInfoInt(mapping->swInfoKey, keyLen, numValue);
	}
	case SWU_SWINFO_KIND_STR: {
		SceSize keyLen = sce_paf_strlen(mapping->swInfoKey);
		SceSize valLen = sce_paf_strlen(value);

		return CuiSetSwInfoStr(mapping->swInfoKey, keyLen, value, valLen);
	}
	default:
		return mapping->valueKind;
	}
}

SceInt32 SwuApplication::WriteIndexSwInfo()
{
	vshIoMount(SWU_VS0_MOUNT_ID, NULL, 0, NULL, 0, 0);

	IndexDatEntry entries[INDEX_DAT_MAX_ENTRIES];
	SceInt32 ret = ReadIndexDat(entries);

	if (ret != 0)
		return ret;

	for (SceInt32 idx = 0; idx < INDEX_DAT_MAX_ENTRIES; idx++) {
		if (entries[idx].key == "")
			break;

		const char *key = entries[idx].key.c_str();
		const char *val = entries[idx].value.c_str();

		if (key != NULL && val != NULL && sce_paf_strncmp(val, "N/A", 4) != 0) {
			for (SceUInt32 mapIdx = 0; mapIdx < SWU_SWINFO_MAPPING_COUNT; mapIdx++) {
				SceSize expectedLen = sce_paf_strlen(s_swInfoMappings[mapIdx].indexKey);

				if (sce_paf_strncmp(s_swInfoMappings[mapIdx].indexKey, key,
						    expectedLen + 1) == 0) {
					WriteIndexSwInfoEntry(&s_swInfoMappings[mapIdx], val);
					break;
				}
			}
		}
	}
	return 0;
}

SceInt32 SwuApplication::Initialize()
{
	SceUpdaterMemoryInfo memoryInfo;

	memoryInfo.updaterCoreAlloc = UpdaterAlloc;
	memoryInfo.updaterCoreFree = UpdaterFree;
	memoryInfo.updaterCoreMemalign = UpdaterMemalign;

	sceUpdaterLogStart();
	sceUpdaterUpdateModeSetLastError(m_firstError);

	SceInt32 ret = sceUpdaterInitialize(&memoryInfo);

	if (ret != 0) {
		RecordFirstError(ret);
		CuiInformUpdateFinished(ret, "initialize core failed.", 23);
		return ret;
	}

	ret = sceUpdaterPowerLock();
	if (ret != 0) {
		RecordFirstError(ret);
		CuiInformUpdateFinished(ret, "initialize power lock failed.", 29);
		return ret;
	}

	SceUInt8 mode = 0;
	bool useGui = false;
	SceInt32 modeRet = sceUpdaterGetUpdateMode(&mode);

	if (modeRet != 0)
		RecordFirstError(modeRet);
	else if (IsGuiUpdateMode(mode))
		useGui = true;

	if (useGui) {
		sceClibPrintf("[INFO]%s\n", "GUI mode.");
		s_view = new UpdateGuiView();
	} else {
		sceClibPrintf("[INFO]%s\n", "CUI mode.");
		s_view = new UpdateCuiView();
	}

	if (s_view != NULL) {
		ret = s_view->Initialize();
		if (ret != 0) {
			RecordFirstError(SWU_ERROR_RELEASE_CHECK_MODE);
			CuiInformUpdateFinished(ret, "initialize view failed.", 23);
		}
	} else {
		ret = SWU_ERROR_NO_MEMORY;
		CuiInformUpdateFinished(ret, "initialize view failed.", 23);
	}
	return ret;
}

const char *SwuApplication::GetPupPath()
{
	return sceUpdaterGetPupPath();
}

SceInt32 SwuApplication::VerifyData(const char *pupPath)
{
	SceUInt8 mode = 0;

	m_clearUpdateMode = 1;

	SceInt32 ret = sceUpdaterGetUpdateMode(&mode);

	m_retryCount = (ret == 0) ? static_cast<SceInt32>(mode & 0xF) : -1;
	if (ret != 0)
		return ret;

	bool isDiff = false;
	SceUInt32 baseBuildNo = 0;
	SceUInt32 baseVersion = 0;

	ret = GetPupDiffBaseInfo(sceUpdaterGetPupPath(), &isDiff, &baseBuildNo, &baseVersion);
	if (ret == 0 && isDiff) {
		SceUInt32 deviceVersion = 0;
		SceUInt32 vshBuildNumber = 0;

		ret = ReadDeviceAndVshBuildNumbers(&deviceVersion, &vshBuildNumber);
		if (ret == 0) {
			if (baseVersion != deviceVersion)
				ret = SWU_ERROR_DIFF_BASE_VERSION_MISMATCH;
			else if (baseBuildNo != vshBuildNumber)
				ret = SWU_ERROR_DIFF_BASE_BUILD_MISMATCH;
		}
	}
	if (ret != 0)
		return ret;

	CuiInformUpdateStarted(4, 0, 300, "verifying data phase 1...", 25);
	ret = sceUpdaterVerifyPup(pupPath);
	if (ret == SWU_ERROR_RELEASE_CHECK_MODE) {
		CuiInformUpdateFinished(SWU_ERROR_RELEASE_CHECK_MODE,
					"release mode check mode is enabled.", 35);
		sceClibPrintf("[ERROR]%s\n", "stop updating.");
		sceClibPrintf("[ERROR]%s\n",
			      "retry update after setting release check mode to development mode.\n\n");
		s_view->OnReleaseCheckError(SWU_ERROR_RELEASE_CHECK_MODE);
		return SWU_ERROR_RELEASE_CHECK_MODE;
	}
	if (ret != 0) {
		if (m_retryCount > 0)
			m_clearUpdateMode = 0;
		CuiInformUpdateFinished(ret, "verifying data phase 1 failed.", 30);
		return ret;
	}

	CuiInformUpdateOngoing(4, 0, 100, "verifying data phase 1 done.", 29);
	CuiInformUpdateStarted(0x80000005, 0, 300, "verifying data phase 2...", 25);
	ret = sceUpdaterInspectAllSpackage(pupPath);
	if (ret != 0) {
		if (m_retryCount > 0)
			m_clearUpdateMode = 0;
		CuiInformUpdateFinished(ret, "verifying data phase 2 failed.", 30);
		return ret;
	}

	CuiInformUpdateOngoing(5, 0, 100, "verifying data phase 2 done.", 29);
	return 0;
}

SceInt32 SwuApplication::IsDiffUpdate()
{
	return sceUpdaterIsDiffUpdate();
}

SceInt32 SwuApplication::UpdateFirmware(const char *pupPath)
{
	SceUInt8 diffFlag;

	if (sceUpdaterIsDiffUpdate() != 0) {
		diffFlag = SWU_NVS_DIFF_UPDATE_ACTIVE;
		vshSblSsSetNvsData(SWU_NVS_DIFF_UPDATE_AREA, &diffFlag, 1);
	}

	CuiInformUpdateStarted(0x80000006, 0, 300, "updating system firmware...", 27);

	SceInt32 ret = sceUpdaterUpdateAllSpackage(pupPath);

	if (ret != 0) {
		m_clearUpdateMode = 0;
		CuiInformUpdateFinished(ret, "updating system firmware failed.", 32);
		return ret;
	}

	CuiInformUpdateOngoing(6, 0, 100, "updating system firmware done.", 31);
	if (sceUpdaterIsDiffUpdate() != 0) {
		diffFlag = SWU_NVS_DIFF_UPDATE_CLEARED;
		return static_cast<SceInt32>(
			vshSblSsSetNvsData(SWU_NVS_DIFF_UPDATE_AREA, &diffFlag, 1));
	}
	return 0;
}

SceInt32 SwuApplication::GetProgress(SceInt32 *outRate, SceUInt64 *outRemainingMs)
{
	SceUpdaterProgress progress;
	SceInt32 ret = sceUpdaterGetProgress(&progress);

	if (ret != 0) {
		RecordFirstError(ret);
	} else {
		*outRate = progress.rate;
		*outRemainingMs = progress.updateRemainingMs;
	}
	return ret;
}

SceInt32 SwuApplication::GetEstimateUpdateTime(const char *pupPath, SceUInt64 *outUpdateMs)
{
	SceUpdaterEstimateTime estimate;
	SceInt32 ret = sceUpdaterGetEstimateProcessTime(pupPath, &estimate);

	if (ret != 0)
		RecordFirstError(ret);
	else
		*outUpdateMs = estimate.updateMs;
	return ret;
}

SceInt32 SwuApplication::HasUpdateFlagError()
{
	return m_updateFlagError;
}

SwuApplication *SwuApplication::GetInstance()
{
	return s_instance;
}

UpdateViewBase *SwuApplication::GetView()
{
	return s_view;
}

SceInt32 SwuApplication::UpdateModeFlag()
{
	SceInt32 ret = sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);

	if (ret != 0)
		return ret;

	m_updateFlagError = 0;
	if (m_clearUpdateMode != 0) {
		ret = sceUpdaterSetUpdateMode(SCE_UPDATER_UPDATE_MODE_NONE);
		if (ret != 0) {
			RecordFirstError(ret);
			CuiInformUpdateFinished(ret, "clear update mode failed.", 25);
			m_clearUpdateMode = 0;
			m_updateFlagError = 1;
		} else {
			if (m_firstError == SWU_ERROR_RELEASE_CHECK_MODE)
				sceClibPrintf("[INFO]%s\n", "clear update mode flag.");
			ret = sceUpdaterCleanUp();
			if (ret != 0)
				m_clearUpdateMode = 0;
		}
	} else {
		SceUInt8 mode = 0;

		ret = sceUpdaterGetUpdateMode(&mode);
		if (ret != 0) {
			m_clearUpdateMode = 0;
			m_updateFlagError = 1;
		} else {
			switch (mode) {
			case 0x10:
				mode = 0x11;
				break;
			case 0x11:
			case 0x12:
				mode = 0x12;
				break;
			case 0x20:
				mode = 0x21;
				break;
			case 0x21:
			case 0x22:
				mode = 0x22;
				break;
			case 0x30:
				mode = 0x31;
				break;
			case 0x31:
			case 0x32:
				mode = 0x32;
				break;
			case 0x40:
			case 0x41:
			case 0x42:
				mode = 0xFF;
				break;
			default:
				break;
			}
			ret = sceUpdaterSetUpdateMode(mode);
			if (ret != 0) {
				m_clearUpdateMode = 0;
				m_updateFlagError = 1;
			}
		}
	}

	SceInt32 unlockRet = sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);

	if (unlockRet != 0 && ret == 0)
		return unlockRet;
	return ret;
}

bool SwuApplication::IsFactoryMode()
{
	SceUInt8 mode = 0;

	if (sceUpdaterGetUpdateMode(&mode) != 0)
		return false;
	return mode == 0x40 || mode == 0x41 || mode == 0x42 || mode == 0x43;
}

void *SwuApplication::UpdaterAlloc(SceSize size)
{
	return sce_paf_malloc(size);
}

void SwuApplication::UpdaterFree(void *ptr)
{
	sce_paf_free(ptr);
}

void *SwuApplication::UpdaterMemalign(SceSize align, SceSize size)
{
	return sce_paf_memalign(align, size);
}

int main(int argc, char *argv[])
{
	SwuApplication application;

	CuiInformUpdateStarted(3, 0, 30, "psp2swu.self: Build: Jun  9 2014 11:44:56", 41);
	sceClibPrintf("[INFO]%s\n", "system update start.");
	sceClibPrintf("[INFO]%s\n", "initialize...");

	SceInt32 ret = application.Initialize();

	if (ret != 0) {
		CuiInformUpdateFinished(ret, "initialize failed.", 18);
	} else {
		CuiInformUpdateOngoing(3, 0, 100, "initialize done.", 17);
		sceClibPrintf("[INFO]%s\n", "operating...");
		ret = SwuApplication::GetView()->Operate();
		if (ret != 0) {
			CuiInformUpdateFinished(ret, "operation failed.", 17);
		} else {
			CuiInformUpdateFinished(0, "operation done (SUCCESS).", 25);
			cuiNameBuffer = static_cast<char *>(
				sce_paf_memalign(8, SWU_CUI_NAME_BUFFER_SIZE));
			if (cuiNameBuffer != NULL) {
				application.WriteIndexSwInfo();
				application.WriteVersionSwInfo();
				sce_paf_free(cuiNameBuffer);
				cuiNameBuffer = NULL;
			} else {
				sceClibPrintf("[ERROR]%s\n", "no memory.");
			}
		}
	}

	sceClibPrintf("[INFO]%s\n", "reboot the device.\n\n");
	ret = application.Finalize();
	if (ret != 0)
		CuiInformUpdateFinished(ret, "finalize failed.", 16);
	return ret;
}
