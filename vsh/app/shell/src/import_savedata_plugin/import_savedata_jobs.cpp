/*
 * import_savedata_jobs.cpp: background job items for import_savedata_plugin:
 *   - ImportSaveDataExecuteJobItem      (0x813991B0..0x81399A51)
 *   - ImportSaveDataLoadIconJobItem     (0x81399AC4..0x81399E87)
 *   - ImportSaveDataLoadInfoJobItem     (0x81399EF8..0x8139A327)
 *   - ImportSaveDataSearchTargetJobItem (0x8139A398..0x8139AAF9)
 */
#include "import_savedata_internal.h"

#include <appmgr/appmgr.h>
#include <kernel/iofilemgr.h>
#include <regmgr/regmgr.h>
#include <rtc/rtc.h>
#include <file/localfile.h>
#include <graphics/surface/surface.h>
#include <std/stdio.h>
#include <std/string.h>
#include "../../../game/game_plugin/game_plugin.h"
#include "../../../game/game_plugin/game_appinfo.h"

extern "C" {
int sceAppMgrInstallDirMount(int mountId, const char *path, const char *key, char *mountPoint);
int sceAppMgrUmount(const char *mountPoint);
int sceAppMgrGetUserDirPath(int partition, char *outPath, SceSize maxLen);
int sceRtcGetCurrentTick(SceRtcTick *tick);
const char *sceLsdbGetTitle(const LsdbAppInfoPtr *app);
}

/* FNV-1a hashes of INSTALL_DIR_SAVEDATA_ADD_1..7 (from Sony binary strings) */
static const SceUInt32 kLsdbInstallDirSavedataAddKeys[7] = {
	0xC79BFEECu,	/* INSTALL_DIR_SAVEDATA_ADD_1 */
	0xCA9C03A5u,	/* INSTALL_DIR_SAVEDATA_ADD_2 */
	0xC99C0212u,	/* INSTALL_DIR_SAVEDATA_ADD_3 */
	0xC49BFA33u,	/* INSTALL_DIR_SAVEDATA_ADD_4 */
	0xC39BF8A0u,	/* INSTALL_DIR_SAVEDATA_ADD_5 */
	0xC69BFD59u,	/* INSTALL_DIR_SAVEDATA_ADD_6 */
	0xC59BFBC6u	/* INSTALL_DIR_SAVEDATA_ADD_7 */
};

static const char *lookupLsdbStringValue(LsdbAppInfoPtr &appInfo, SceUInt32 keyHash)
{
	LsdbAppInfo::iterator it = appInfo->find(keyHash);
	if (it == appInfo->end())
		return NULL;
	if (it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL)
		return it->second.text->c_str();
	return "";
}

/* ============================================================================
 * ImportSaveDataExecuteJobItem (0x813991B0..0x81399A51)
 * ============================================================================ */

/* 0x813991B0 */
ImportSaveDataExecuteJobItem::ImportSaveDataExecuteJobItem(ImportSavedataJobResultCallback finishCb,
							   ProgressCallback progressCb,
							   SceUInt64 totalBytes, void *userData)
	: paf::job::JobItem("ImportSaveDataExecuteJobItem", NULL),
	  m_finishCallback(finishCb),
	  m_progressCallback(progressCb),
	  m_callbackUserData(userData),
	  m_suppressed(false),
	  m_totalBytes(totalBytes),
	  m_copiedBytes(0),
	  m_progressPercent(0)
{
	sce_paf_memset(m_destTitleId, 0, sizeof(m_destTitleId));
}

/* 0x8139921C / 0x81399284 */
ImportSaveDataExecuteJobItem::~ImportSaveDataExecuteJobItem()
{
}

/* 0x813992F0 */
void ImportSaveDataExecuteJobItem::Cancel()
{
	paf::job::JobItem::Cancel();
}

/* 0x813992F8 */
SceInt32 ImportSaveDataExecuteJobItem::SetDestinationTitleId(const char *titleId)
{
	sce_paf_memcpy(m_destTitleId, titleId, 13);
	m_destTitleId[12] = '\0';
	return 0;
}

/* 0x8139930E */
void ImportSaveDataExecuteJobItem::SetTargets(ImportSavedataTargetMap *targets)
{
	for (ImportSavedataTargetMap::iterator it = targets->begin();
	     it != targets->end(); ++it) {
		if (it->second.selected)
			m_sourceTitleIds.push_back(it->first);
	}
}

/* 0x813993C8 */
SceInt32 ImportSaveDataExecuteJobItem::CopyDirectoryRecursive(const char *srcDir,
							      const char *dstDir)
{
	SceInt32 ret = sceIoMkdir(dstDir, 6);
	if (ret < 0)
		return ret;

	SceUID dfd = sceIoDopen(srcDir);
	if (dfd < 0) {
		if (dfd == IMPORT_SAVEDATA_KERNEL_ENODEV || dfd == IMPORT_SAVEDATA_PFS_NO_MEDIA)
			return IMPORT_SAVEDATA_ERR_NO_MEDIA;
		return dfd;
	}

	SceInt32 status = 0;
	for (;;) {
		SceIoDirent dirent;
		sce_paf_memset(&dirent, 0, sizeof(dirent));
		SceInt32 dreadRet = sceIoDread(dfd, &dirent);
		if (dreadRet <= 0) {
			status = dreadRet;
			if (dreadRet < 0 &&
			    (dreadRet == IMPORT_SAVEDATA_KERNEL_ENODEV ||
			     dreadRet == IMPORT_SAVEDATA_PFS_NO_MEDIA))
				status = IMPORT_SAVEDATA_ERR_NO_MEDIA;
			break;
		}

		paf::string srcChild(srcDir);
		srcChild.append("/");
		srcChild.append(dirent.d_name);

		paf::string dstChild(dstDir);
		dstChild.append("/");
		dstChild.append(dirent.d_name);

		SceInt32 errorStage = 0;
		SceInt32 copyRet = 0;
		if ((dirent.d_stat.st_mode & 0x1000) != 0)
			copyRet = CopyDirectoryRecursive(srcChild.c_str(), dstChild.c_str());
		else
			copyRet = importSavedataCopyFileStream(srcChild.c_str(), dstChild.c_str(),
							       1, &errorStage);

		if (copyRet < 0) {
			status = copyRet;
			if (errorStage == 0 &&
			    (copyRet == IMPORT_SAVEDATA_KERNEL_ENODEV ||
			     copyRet == IMPORT_SAVEDATA_PFS_NO_MEDIA))
				status = IMPORT_SAVEDATA_ERR_NO_MEDIA;
			break;
		}

		m_copiedBytes += static_cast<SceUInt64>(dirent.d_stat.st_size);
		if (m_totalBytes != 0) {
			SceInt32 pct = static_cast<SceInt32>((m_copiedBytes / m_totalBytes) * 100ULL);
			if (m_progressPercent < pct)
				m_progressPercent = pct;
		}
		status = copyRet;
	}

	sceIoDclose(dfd);
	return status;
}

/* 0x813996CA */
int ImportSaveDataExecuteJobItem::Run()
{
	if (m_sourceTitleIds.empty())
		return 0;

	char srcPath[512];
	char dstPath[512];
	char mountPoint[16];
	sce_paf_memset(srcPath, 0, sizeof(srcPath));
	sce_paf_memset(dstPath, 0, sizeof(dstPath));
	sce_paf_memset(mountPoint, 0, sizeof(mountPoint));

	for (paf::vector<paf::string>::iterator it = m_sourceTitleIds.begin();
	     it != m_sourceTitleIds.end(); ++it) {
		SceInt32 ret = sceAppMgrInstallDirMount(505, NULL, NULL, mountPoint);
		if (ret != 0)
			return ret;

		sce_paf_snprintf(srcPath, sizeof(srcPath), "ur0:appmeta/%s/%s",
				 it->c_str(), "param.sfo");
		sce_paf_snprintf(dstPath, sizeof(dstPath), "%s/%s", mountPoint, "param.sfo");
		SceInt32 stepRet = importSavedataCopyFileStream(srcPath, dstPath, 0, NULL);
		if (stepRet != 0) {
			sceAppMgrUmount(mountPoint);
			return stepRet;
		}

		sce_paf_snprintf(srcPath, sizeof(srcPath), "ur0:appmeta/%s/%s",
				 it->c_str(), "icon0.png");
		sce_paf_snprintf(dstPath, sizeof(dstPath), "%s/%s", mountPoint, "icon0.png");
		stepRet = importSavedataCopyFileStream(srcPath, dstPath, 0, NULL);
		if (stepRet != 0) {
			sceAppMgrUmount(mountPoint);
			return stepRet;
		}

		ImportSavedataMetaHeader meta;
		sce_paf_memset(&meta, 0, sizeof(meta));
		meta.version = 0;
		sceRtcGetCurrentTick(&meta.timestamp);

		sce_paf_snprintf(dstPath, sizeof(dstPath), "%s/%s", mountPoint, "meta.dat");
		SceUID fd = sceIoOpen(dstPath, 0x603, 6);
		if (fd < 0) {
			sceAppMgrUmount(mountPoint);
			return fd;
		}
		SceSSize written = sceIoWrite(fd, &meta, sizeof(meta));
		SceInt32 closeRet = sceIoClose(fd);
		if (written < 0) {
			sceAppMgrUmount(mountPoint);
			return written;
		}
		if (written != static_cast<SceSSize>(sizeof(meta))) {
			sceAppMgrUmount(mountPoint);
			return IMPORT_SAVEDATA_ERR_GENERAL;
		}
		if (closeRet != 0) {
			sceAppMgrUmount(mountPoint);
			return closeRet;
		}

		ret = sceAppMgrUmount(mountPoint);
		if (ret != 0)
			return ret;

		paf::string cardSavedata = importSavedataBuildCardPath(it->c_str());
		paf::string tempSavedata = importSavedataBuildUserPath(it->c_str(), true);
		ret = CopyDirectoryRecursive(cardSavedata.c_str(), tempSavedata.c_str());
		if (ret != 0)
			return ret;
	}

	char userDir[33];
	sce_paf_memset(userDir, 0, sizeof(userDir));
	SceInt32 ret = sceAppMgrGetUserDirPath(GAME_USER_DIR_PARTITION, userDir, sizeof(userDir));
	if (ret != 0)
		return ret;

	SceIoStat st;
	sce_paf_memset(&st, 0, sizeof(st));
	sce_paf_snprintf(srcPath, sizeof(srcPath), "%s/%s", userDir, "savedata_vc");
	if (sceIoGetstat(srcPath, &st) == 0 && (st.st_mode & 0x1000) != 0) {
		sce_paf_snprintf(dstPath, sizeof(dstPath), "%s/%s", userDir, "_temp2_savedata_vc");
		ret = sceIoRename(srcPath, dstPath);
		if (ret != 0)
			return ret;
	}

	sce_paf_snprintf(srcPath, sizeof(srcPath), "%s/%s", userDir, "_temp_savedata_vc");
	sce_paf_snprintf(dstPath, sizeof(dstPath), "%s/%s", userDir, "savedata_vc");
	ret = sceIoRename(srcPath, dstPath);
	if (ret != 0)
		return ret;

	sce_paf_snprintf(srcPath, sizeof(srcPath), "%s/%s", userDir, "_temp2_savedata_vc");
	return importSavedataRemoveDirectoryRecursive(srcPath);
}

/* 0x81399A3A */
void ImportSaveDataExecuteJobItem::Finish(int result)
{
	if (!m_suppressed && m_finishCallback != NULL)
		m_finishCallback(result, m_callbackUserData);
}

/* ============================================================================
 * ImportSaveDataLoadIconJobItem (0x81399AC4..0x81399E87)
 * ============================================================================ */

/* 0x81399AC4 */
ImportSaveDataLoadIconJobItem::ImportSaveDataLoadIconJobItem(
	ImportSavedataIconLoadedCallback doneCb, paf::ui::Widget *targetWidget,
	paf::Plugin *plugin, void *userData)
	: paf::job::JobItem("ImportSaveDataLoadIconJobItem", NULL),
	  m_doneCallback(doneCb),
	  m_targetWidget(targetWidget),
	  m_plugin(plugin),
	  m_callbackUserData(userData),
	  m_cellIndex(-1),
	  m_storageType(0)
{
}

/* 0x81399B20 / 0x81399B5C */
ImportSaveDataLoadIconJobItem::~ImportSaveDataLoadIconJobItem()
{
}

void ImportSaveDataLoadIconJobItem::SetTarget(SceInt32 cellIndex, SceInt32 storageType,
					      const paf::string &titleId)
{
	m_cellIndex = cellIndex;
	m_storageType = storageType;
	m_titleId = titleId;
}

/* 0x81399B9C */
int ImportSaveDataLoadIconJobItem::Run()
{
	if (m_plugin == NULL || m_titleId.empty() || m_storageType == 0 || m_cellIndex < 0)
		return IMPORT_SAVEDATA_ERR_INVALID_ARG;
	if (IsCanceled())
		return 0;

	char mountPoint[16];
	char iconPath[512];
	sce_paf_memset(mountPoint, 0, sizeof(mountPoint));
	sce_paf_memset(iconPath, 0, sizeof(iconPath));

	bool mounted = false;
	if (m_storageType == 2 || m_storageType == 3) {
		SceInt32 mountRet = sceAppMgrAppDataMountById(
			110, m_titleId.c_str(),
			reinterpret_cast<ScePfsRndDriveId *>(mountPoint));
		if (mountRet != 0)
			return mountRet;
		sce_paf_snprintf(iconPath, sizeof(iconPath), "%s/icon0.png", mountPoint);
		mounted = true;
	} else if (m_storageType == 1) {
		sce_paf_snprintf(iconPath, sizeof(iconPath), "ur0:appmeta/%s/icon0.png",
				 m_titleId.c_str());
	} else {
		return IMPORT_SAVEDATA_ERR_INVALID_ARG;
	}

	if (IsCanceled()) {
		if (mounted)
			sceAppMgrUmount(mountPoint);
		return 0;
	}

	SceInt32 err = 0;
	if (iconPath[0] != '\0') {
		paf::common::SharedPtr<paf::File> file =
			paf::LocalFile::Open(iconPath, 1, 0, &err);
		if (file.get() != NULL) {
			paf::graph::Surface::LoadOption loadOption;
			m_surface = paf::graph::Surface::Load(
				m_plugin->m_surface_pool, file, &loadOption);
			file->Close();
			if (m_surface.get() == NULL)
				err = loadOption.error;
		} else {
			err = IMPORT_SAVEDATA_ERR_GENERAL;
		}
	}

	if (mounted)
		sceAppMgrUmount(mountPoint);
	return err;
}

/* 0x81399E18 */
void ImportSaveDataLoadIconJobItem::Finish(int result)
{
	if (IsCanceled())
		return;

	if (result == 0 && m_targetWidget != NULL) {
		m_targetWidget->SetTexture(m_surface);
		m_targetWidget->Show(0xFF, NULL, NULL, 0.0f);
	}
	if (m_doneCallback != NULL)
		m_doneCallback(m_cellIndex, result, m_callbackUserData);
}

/* ============================================================================
 * ImportSaveDataLoadInfoJobItem (0x81399EF8..0x8139A327)
 * ============================================================================ */

/* 0x81399EF8 */
ImportSaveDataLoadInfoJobItem::ImportSaveDataLoadInfoJobItem(
	ImportSavedataJobResultCallback doneCb, const char *titleId, void *userData)
	: paf::job::JobItem("ImportSaveDataLoadInfoJobItem", NULL),
	  m_doneCallback(doneCb),
	  m_titleId(titleId != NULL ? titleId : ""),
	  m_callbackUserData(userData)
{
}

/* 0x81399F54 / 0x81399FA2 */
ImportSaveDataLoadInfoJobItem::~ImportSaveDataLoadInfoJobItem()
{
}

/* 0x81399FF6 */
SceInt32 ImportSaveDataLoadInfoJobItem::ReadInfoFromSfo(const char *sfoPath)
{
	ImportSavedataSfoFile sfoFile;
	SceInt32 ret = importSavedataOpenSfoFile(sfoPath, &sfoFile);
	if (ret != 0)
		return ret;

	SceInt32 languageId = -1;
	if (sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "language", &languageId) != 0)
		languageId = -1;

	const void *infoVal = NULL;
	SceSize infoLen = 0;
	SceSize infoMaxLen = 0;
	SceInt32 lookupRet = importSavedataGetSfoParam(
		&sfoFile, importSavedataGetInfoKeyName(languageId),
		&infoVal, &infoLen, &infoMaxLen);
	if (lookupRet != 0 && languageId != -1) {
		lookupRet = importSavedataGetSfoParam(
			&sfoFile, importSavedataGetInfoKeyName(-1),
			&infoVal, &infoLen, &infoMaxLen);
	}
	if (lookupRet == 0 && infoVal != NULL)
		m_infoText = static_cast<const char *>(infoVal);

	return importSavedataCloseSfoFile(&sfoFile);
}

/* 0x8139A0B6 */
SceInt32 ImportSaveDataLoadInfoJobItem::ResolveSfoPath(char *outSfoPath, SceSize pathCapacity,
						       char *outMountPoint, bool *outMounted)
{
	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	if (appInfo.get() == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	SceInt32 ret = importSavedataQueryLsdbAppInfo(m_titleId.c_str(), &appInfo);
	if (ret != 0)
		return ret;

	SceInt32 storageType = SceLsdb_298134FE(&appInfo);
	if (storageType == 2 || storageType == 3) {
		ret = sceAppMgrAppDataMountById(
			110, m_titleId.c_str(),
			reinterpret_cast<ScePfsRndDriveId *>(outMountPoint));
		if (ret == 0) {
			sce_paf_snprintf(outSfoPath, pathCapacity, "%s/param.sfo", outMountPoint);
			*outMounted = true;
		}
		return ret;
	}
	if (storageType == 1) {
		sce_paf_snprintf(outSfoPath, pathCapacity, "ur0:appmeta/%s/param.sfo",
				 m_titleId.c_str());
		return 0;
	}
	return IMPORT_SAVEDATA_ERR_GENERAL;
}

/* 0x8139A278 */
int ImportSaveDataLoadInfoJobItem::Run()
{
	if (m_titleId.empty())
		return IMPORT_SAVEDATA_ERR_GENERAL;

	char sfoPath[512];
	char mountPoint[16];
	bool mounted = false;
	sce_paf_memset(sfoPath, 0, sizeof(sfoPath));
	sce_paf_memset(mountPoint, 0, sizeof(mountPoint));

	SceInt32 ret = ResolveSfoPath(sfoPath, sizeof(sfoPath), mountPoint, &mounted);
	if (ret != 0)
		return ret;

	ret = ReadInfoFromSfo(sfoPath);
	if (ret != 0) {
		if (mounted)
			sceAppMgrUmount(mountPoint);
		return ret;
	}

	if (mounted)
		return sceAppMgrUmount(mountPoint);
	return 0;
}

/* 0x8139A316 */
void ImportSaveDataLoadInfoJobItem::Finish(int result)
{
	if (m_doneCallback != NULL)
		m_doneCallback(result, m_callbackUserData);
}

/* ============================================================================
 * ImportSaveDataSearchTargetJobItem (0x8139A398..0x8139AAF9)
 * ============================================================================ */

/* 0x8139A398 */
ImportSaveDataSearchTargetJobItem::ImportSaveDataSearchTargetJobItem(
	ImportSavedataJobResultCallback doneCb, const char *titleId, void *userData)
	: paf::job::JobItem("ImportSaveDataSearchTargetJobItem", NULL),
	  m_doneCallback(doneCb),
	  m_callbackUserData(userData),
	  m_titleId(titleId != NULL ? titleId : "")
{
}

/* 0x8139A416 / 0x8139A464 */
ImportSaveDataSearchTargetJobItem::~ImportSaveDataSearchTargetJobItem()
{
}

/* 0x8139A474 */
int ImportSaveDataSearchTargetJobItem::Run()
{
	LsdbAppInfoPtr sourceAppInfo(new LsdbAppInfo());
	if (sourceAppInfo.get() == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	SceInt32 ret = importSavedataQueryLsdbAppInfo(m_titleId.c_str(), &sourceAppInfo);
	if (ret != 0)
		return ret;

	AppInfoQClient client;
	SceLsdb_A6D8772C(&client);
	ret = SceLsdb_E90B81CD(&client);
	if (ret != 0) {
		SceLsdb_E2101B03(&client);
		return ret;
	}

	LsdbAppInfoPtr candidateInfo(new LsdbAppInfo());
	if (candidateInfo.get() == NULL) {
		SceLsdb_E2101B03(&client);
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;
	}

	paf::string ownSavedataDir;
	const char *ownDirStr = lookupLsdbStringValue(sourceAppInfo, LSDB_KEY_INSTALL_DIR_SAVEDATA);
	if (ownDirStr != NULL)
		ownSavedataDir = ownDirStr;

	for (SceUInt32 idx = 0; idx < 7; ++idx) {
		const char *candidateStr =
			lookupLsdbStringValue(sourceAppInfo, kLsdbInstallDirSavedataAddKeys[idx]);
		if (candidateStr == NULL)
			break;

		paf::string candidateId(candidateStr);
		if (candidateId.empty())
			break;

		if (!ownSavedataDir.empty() && ownSavedataDir == candidateId)
			continue;

		paf::string cardPath = importSavedataBuildCardPath(candidateId.c_str());
		SceIoStat st;
		if (sceIoGetstat(cardPath.c_str(), &st) != 0 || (st.st_mode & 0x1000) == 0)
			continue;

		if (importSavedataQueryLsdbAppInfo(candidateId.c_str(), &candidateInfo) != 0)
			continue;

		ImportSavedataTargetEntry entry;
		entry.selected = true;
		entry.storageType = SceLsdb_298134FE(&candidateInfo);
		const char *titleUtf8 = sceLsdbGetTitle(&candidateInfo);
		entry.title = (titleUtf8 != NULL) ? titleUtf8 : "";
		entry.titleId = candidateId;

		m_targets.insert(paf::pair<paf::string, ImportSavedataTargetEntry>(
			candidateId, entry));
	}

	SceLsdb_E2101B03(&client);
	return static_cast<int>(m_targets.size());
}

/* 0x8139AAE8 */
void ImportSaveDataSearchTargetJobItem::Finish(int result)
{
	if (m_doneCallback != NULL)
		m_doneCallback(result, m_callbackUserData);
}
