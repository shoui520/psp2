/*
 * import_savedata_util.cpp: CommonGuiDialog wrappers, AppMgr/LSDB/game_plugin
 * helpers, file stream copy, and SFO reader wrappers (0x813937F4..0x8139548F).
 */
#include "import_savedata_internal.h"

#include <appmgr/appmgr.h>
#include <kernel/iofilemgr.h>
#include <buffer/buffer.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <graphics/surface/surface.h>
#include <std/stdio.h>
#include <std/string.h>
#include "../../plugins.h"
#include "../../../game/game_plugin/game_plugin.h"
#include "../../../game/game_plugin/game_appinfo.h"

extern "C" {
int sceAppMgrGetAppIdByAppId(int appId);
int sceAppMgrDestroyAppByAppId(SceInt32 appId);
int sceAppMgrGetUserDirPath(int partition, char *outPath, SceSize maxLen);
const char *sceLsdbGetOriginalPath(const LsdbAppInfoPtr *app);
}

extern void shellSizeTextKb(paf::wstring *text, SceUInt32 sizeKb);

/* 0x81548D60, 0x81548D64 */
static paf::Plugin *s_plugin = NULL;
static bool s_gameDialogHoldActive = false;

/* 0x813937F4 */
void importSavedataSetPlugin(paf::Plugin *plugin)
{
	s_plugin = plugin;
}

/* 0x81393800 */
void importSavedataClearPlugin(void)
{
	s_plugin = NULL;
}

/* 0x8139380E */
paf::Plugin *importSavedataGetPlugin(void)
{
	return s_plugin;
}

/* 0x8139381A */
SceInt32 importSavedataShowLayoutDialog(const paf::wstring &text, SceUInt32 soundId,
					const CommonGuiDialogLayout *layout,
					CommonGuiDialogHandler *handler)
{
	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = s_plugin;
	param.text = text;
	param.layout = *layout;
	param.priority = 45;
	param.soundId = soundId;
	param.handler = handler;
	return SceCommonGuiDialog_CCFCD9A6(&param);
}

/* 0x8139391C */
SceUInt32 importSavedataResolveSoundId(SceInt32 soundType)
{
	if (soundType == IMPORT_SAVEDATA_SOUND_ALERT)
		return 0xD74A54E5u;
	if (soundType == IMPORT_SAVEDATA_SOUND_COMPLETE)
		return 0xE0DA2718u;
	return 0;
}

/* 0x81393946 */
const wchar_t *importSavedataGetString(const paf::IDParam &id)
{
	return s_plugin->GetString(id);
}

/* 0x8139395C */
SceInt32 importSavedataShowOkDialog(const paf::IDParam &textId, SceInt32 soundType,
				    CommonGuiDialogHandler *handler)
{
	const wchar_t *raw = s_plugin->GetString(textId);
	paf::wstring text;
	if (raw != NULL)
		text.assign(raw, sce_paf_wcslen(raw));

	SceUInt32 soundId = importSavedataResolveSoundId(soundType);
	return importSavedataShowLayoutDialog(text, soundId, &SceCommonGuiDialog_AF75CA6D, handler);
}

/* 0x81393A02 */
SceInt32 importSavedataShowOkSizeDialog(const paf::IDParam &textId, SceInt32 soundType,
					SceInt64 sizeBytes, CommonGuiDialogHandler *handler)
{
	paf::wstring sizeText;
	shellSizeTextKb(&sizeText, static_cast<SceUInt32>(sizeBytes >> 10));

	const wchar_t *fmt = s_plugin->GetString(textId);
	paf::wstring formatted = shellFormatText(fmt, sizeText.c_str());

	SceUInt32 soundId = importSavedataResolveSoundId(soundType);
	return importSavedataShowLayoutDialog(formatted, soundId, &SceCommonGuiDialog_AF75CA6D,
					      handler);
}

/* 0x81393B02 */
SceInt32 importSavedataShowCancelOkDialog(const paf::IDParam &textId, SceInt32 soundType,
					  CommonGuiDialogHandler *handler)
{
	const wchar_t *raw = s_plugin->GetString(textId);
	paf::wstring text;
	if (raw != NULL)
		text.assign(raw, sce_paf_wcslen(raw));

	SceUInt32 soundId = importSavedataResolveSoundId(soundType);
	return importSavedataShowLayoutDialog(text, soundId, &SceCommonGuiDialog_92752DE2, handler);
}

/* 0x81393BA8 */
SceInt32 importSavedataShowYesNoDialog(const paf::IDParam &textId, SceInt32 soundType,
				       CommonGuiDialogHandler *handler)
{
	const wchar_t *raw = s_plugin->GetString(textId);
	paf::wstring text;
	if (raw != NULL)
		text.assign(raw, sce_paf_wcslen(raw));

	SceUInt32 soundId = importSavedataResolveSoundId(soundType);
	return importSavedataShowLayoutDialog(text, soundId, &SceCommonGuiDialog_6CD802FC, handler);
}

/* 0x81393C4E */
SceInt32 importSavedataShowWaitingDialog(const paf::IDParam &textId,
					 CommonGuiDialogHandler *handler)
{
	const wchar_t *raw = s_plugin->GetString(textId);
	paf::wstring text;
	if (raw != NULL)
		text.assign(raw, sce_paf_wcslen(raw));

	return importSavedataShowLayoutDialog(text, 0, &SceCommonGuiDialog_F8B2D7E1, handler);
}

/* 0x81393CD0 */
SceInt32 importSavedataShowErrorDialog(const paf::IDParam &messageId, SceInt32 errorCode,
				       CommonGuiDialogHandler *handler,
				       CommonGuiMessageDialogOpenHandler *openHandler)
{
	CommonGuiMessageDialogParam param;
	param.resourcePlugin = s_plugin;
	param.type = errorCode;
	param.priority = 45;
	param.buttonCount = 0;
	param.defaultButton = 2;
	param.cancelButton = 2;
	if (messageId.GetIDHash() != 0xC51B20E4u)
		param.messageId = messageId.GetIDHash();
	param.handler = handler;
	param.openHandler = openHandler;
	return SceCommonGuiDialog_20B141F1(&param);
}

/* 0x81393DFE */
SceInt32 importSavedataCloseDialog(SceInt32 dialogId)
{
	if (dialogId >= 0)
		SceCommonGuiDialog_BC311DC0(dialogId);
	return dialogId;
}

/* 0x81393E0A */
SceInt32 importSavedataOpenTemplate(paf::ui::Widget *parent, const paf::IDParam &templateId)
{
	paf::Plugin::TemplateOpenParam openParam;
	return s_plugin->TemplateOpen(parent, templateId, openParam);
}

/* 0x81393E4C */
paf::wstring importSavedataFormatDateTime(const SceDateTime *dt)
{
	wchar_t buf[18];
	sce_paf_memset(buf, 0, sizeof(buf));
	sce_paf_swprintf(buf, 34, L"%04d/%d/%d %d:%02d",
			 dt->year, dt->month, dt->day, dt->hour, dt->minute);
	return paf::wstring(buf);
}

/* 0x81393EC8 */
SceInt32 importSavedataDestroyApp(SceInt32 appId)
{
	return sceAppMgrDestroyAppByAppId(appId);
}

/* 0x81393ED0 */
SceInt32 importSavedataQueryLsdbAppInfo(const char *titleId, LsdbAppInfoPtr *outInfo)
{
	AppInfoQClient *client = static_cast<AppInfoQClient *>(operator new(sizeof(AppInfoQClient)));
	if (client == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	SceLsdb_A6D8772C(client);
	SceInt32 ret = SceLsdb_E90B81CD(client);
	if (ret == 0) {
		paf::string titleStr(titleId != NULL ? titleId : "");
		ret = SceLsdb_D239031A(client, &titleStr, outInfo);
	}
	SceLsdb_E2101B03(client);
	operator delete(client);
	return ret;
}

/* 0x81393F7C */
SceInt32 importSavedataGetForegroundAppId(void)
{
	return sceAppMgrGetAppIdByAppId(-3);
}

/* 0x81393F88 */
SceInt32 importSavedataGetAppTitleId(SceInt32 appId, char *outTitleId, SceSize maxLen)
{
	SceAppMgrAppStatus status;
	SceInt32 ret = sceAppMgrGetStatusByAppId(appId, &status);
	if (ret >= 0)
		sce_paf_memcpy(outTitleId, status.appName, maxLen);
	return ret;
}

/* 0x81393FC6 */
SceInt32 importSavedataAcquireGameDialogHold(void)
{
	if (!s_gameDialogHoldActive) {
		paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
		if (gamePlugin == NULL)
			return IMPORT_SAVEDATA_ERR_NO_MEMORY;

		GamePluginInterface *iface =
			static_cast<GamePluginInterface *>(gamePlugin->GetInterface(1));
		if (iface == NULL)
			return IMPORT_SAVEDATA_ERR_NO_MEMORY;

		void (*setDialogHold)(bool) = reinterpret_cast<void (*)(bool)>(
			iface->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD]);
		setDialogHold(true);
		s_gameDialogHoldActive = true;
	}
	return 0;
}

/* 0x8139400E */
SceInt32 importSavedataReleaseGameDialogHold(void)
{
	if (s_gameDialogHoldActive) {
		paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
		if (gamePlugin == NULL)
			return IMPORT_SAVEDATA_ERR_NO_MEMORY;

		GamePluginInterface *iface =
			static_cast<GamePluginInterface *>(gamePlugin->GetInterface(1));
		if (iface == NULL)
			return IMPORT_SAVEDATA_ERR_NO_MEMORY;

		void (*setDialogHold)(bool) = reinterpret_cast<void (*)(bool)>(
			iface->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD]);
		setDialogHold(false);
		s_gameDialogHoldActive = false;
	}
	return 0;
}

/* 0x81394058 */
SceInt32 importSavedataEnqueueGameJob(paf::job::JobItem *job)
{
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	GamePluginInterface *iface =
		static_cast<GamePluginInterface *>(gamePlugin->GetInterface(1));
	if (iface == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	paf::job::JobQueue *(*getJobQueue)(void) =
		reinterpret_cast<paf::job::JobQueue *(*)(void)>(
			iface->func[GAME_PLUGIN_FUNC_GET_JOB_QUEUE]);
	paf::job::JobQueue *queue = getJobQueue();
	if (queue == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	paf::common::SharedPtr<paf::job::JobItem> jobPtr(job);
	return queue->Enqueue(jobPtr);
}

/* 0x81394146 */
SceInt32 importSavedataLoadSurfaceFromFile(const char *path,
					   paf::intrusive_ptr<paf::graph::Surface> *outSurface)
{
	SceInt32 err = 0;
	paf::common::SharedPtr<paf::File> file = paf::LocalFile::Open(path, 1, 0, &err);
	if (file.get() == NULL)
		return err;

	paf::graph::Surface::LoadOption loadOption;
	paf::intrusive_ptr<paf::graph::Surface> loaded =
		paf::graph::Surface::Load(s_plugin->m_surface_pool, file, &loadOption);
	*outSurface = loaded;
	file->Close();

	if (outSurface->get() == NULL)
		err = loadOption.error;
	return err;
}

/* 0x813942F8 */
SceInt32 importSavedataLoadLsdbIcon(paf::intrusive_ptr<paf::graph::Surface> *outSurface,
				    const LsdbAppInfoPtr *appInfo)
{
	paf::string iconPath;
	const char *origPath = sceLsdbGetOriginalPath(appInfo);
	if (origPath != NULL) {
		char buf[512];
		sce_paf_snprintf(buf, sizeof(buf), "%s/%s", origPath, "sce_sys/icon0.png");
		iconPath = buf;
	}
	return importSavedataLoadSurfaceFromFile(iconPath.c_str(), outSurface);
}

/* 0x81394396 */
SceInt32 importSavedataGetDeviceFreeSpace(const char *device, SceUInt64 *outFreeBytes)
{
	struct SceIoDevInfo {
		SceUInt64 maxSize;
		SceUInt64 freeSize;
		SceUInt32 clusterSize;
		SceUInt32 reservedWord;
	};
	SceIoDevInfo devInfo;
	SceInt32 ret = sceIoDevctl(device, 0x3001u, NULL, 0, &devInfo, sizeof(devInfo));
	if (ret == 0)
		*outFreeBytes = devInfo.freeSize;
	return ret;
}

/* 0x813943DA */
paf::string importSavedataBuildCardPath(const char *titleId)
{
	char buf[512];
	sce_paf_snprintf(buf, sizeof(buf), "%s/savedata/%s", "grw0:", titleId);
	return paf::string(buf);
}

/* 0x8139443E */
SceInt32 importSavedataGetCardTreeSize(const char *titleId, void *cancelUserData,
				       SceUInt64 *outSize)
{
	paf::string path = importSavedataBuildCardPath(titleId);
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	GamePluginInterface *iface =
		static_cast<GamePluginInterface *>(gamePlugin->GetInterface(1));
	if (iface == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	typedef SceInt32 (*DirGetSizeFn)(const char *path, GameCancelFunc cancel,
					 void *arg, SceUInt64 *size);
	DirGetSizeFn dirGetSize = reinterpret_cast<DirGetSizeFn>(iface->func[21]);
	return dirGetSize(path.c_str(), NULL, cancelUserData, outSize);
}

/* 0x813944F4 */
paf::string importSavedataBuildUserPath(const char *titleId, bool temporary)
{
	char userDir[32];
	if (sceAppMgrGetUserDirPath(GAME_USER_DIR_PARTITION, userDir, sizeof(userDir)) != 0)
		return paf::string();

	char path[512];
	sce_paf_memset(path, 0, sizeof(path));
	const char *fmt = temporary ? "%s/_temp_savedata_vc/%s" : "%s/savedata/%s";
	sce_paf_snprintf(path, sizeof(path), fmt, userDir, titleId);
	return paf::string(path);
}

/* 0x81394594 */
bool importSavedataIsMemoryCardMounted(void)
{
	paf::LocalFileStat st;
	return paf::LocalFile::Getstat("ux0:", &st) == 0;
}

/* 0x8139461C */
SceInt32 importSavedataIsGameCardInserted(bool *outInserted)
{
	SceIoStat st;
	SceInt32 ret = sceIoGetstat("gro0:", &st);
	if (outInserted != NULL)
		*outInserted = (ret >= 0);
	return ret;
}

/* 0x81394660 */
SceInt32 importSavedataCopyFileStream(const char *srcPath, const char *dstPath,
				      SceInt32 copyOption, SceInt32 *outErrorStage)
{
	(void)copyOption;
	SceInt64 copied = 0;
	SceInt32 err = IMPORT_SAVEDATA_ERR_GENERAL;

	if (srcPath == NULL || dstPath == NULL) {
		if (outErrorStage != NULL)
			*outErrorStage = 2;
		return IMPORT_SAVEDATA_ERR_INVALID_ARG;
	}

	SceInt64 fileSize = paf::LocalFile::GetFileSize(srcPath);
	if (fileSize <= 0) {
		if (outErrorStage != NULL)
			*outErrorStage = 0;
		return static_cast<SceInt32>(fileSize);
	}

	paf::common::SharedPtr<paf::File> srcFile =
		paf::LocalFile::Open(srcPath, 0x10001u, 6u, &err);
	if (err != 0) {
		if (outErrorStage != NULL)
			*outErrorStage = 0;
		return err;
	}

	paf::common::SharedPtr<paf::File> dstFile =
		paf::LocalFile::Open(dstPath, 0x10602u, 6u, &err);
	if (err != 0) {
		if (outErrorStage != NULL)
			*outErrorStage = 1;
		if (srcFile.get() != NULL)
			srcFile->Close();
		return err;
	}

	paf::common::SharedPtr<paf::MallocBuffer> ioBuffer = paf::MallocBuffer::Allocate(0x8000u);
	if (ioBuffer.get() == NULL || ioBuffer->GetSize() == 0) {
		err = IMPORT_SAVEDATA_ERR_NO_MEMORY;
		if (outErrorStage != NULL)
			*outErrorStage = 2;
	} else {
		while (copied < fileSize) {
			SceInt32 bytesRead = srcFile->Read(ioBuffer->GetAddr(), ioBuffer->GetSize());
			if (bytesRead <= 0) {
				err = (bytesRead < 0) ? bytesRead : 0;
				if (outErrorStage != NULL)
					*outErrorStage = 0;
				break;
			}
			SceInt32 bytesWritten = dstFile->Write(ioBuffer->GetAddr(), bytesRead);
			if (bytesWritten <= 0) {
				err = bytesWritten;
				if (outErrorStage != NULL)
					*outErrorStage = 1;
				break;
			}
			copied += bytesWritten;
		}
		if (err >= 0) {
			err = (copied >= fileSize) ? 0 : IMPORT_SAVEDATA_ERR_INCOMPLETE_COPY;
			if (err == IMPORT_SAVEDATA_ERR_INCOMPLETE_COPY && outErrorStage != NULL)
				*outErrorStage = 2;
		}
	}

	if (dstFile.get() != NULL)
		dstFile->Close();
	if (srcFile.get() != NULL)
		srcFile->Close();

	if (err != 0 && copied != 0)
		paf::LocalFile::RemoveFile(dstPath);
	return err;
}

/* 0x81395080 */
SceInt32 importSavedataRemoveDirectoryRecursive(const char *path)
{
	return paf::Dir::RemoveRecursive(path);
}

/* 0x81395088 */
SceInt32 importSavedataRemoveUserSubdir(const char *subDirName)
{
	char userDir[33];
	sce_paf_memset(userDir, 0, sizeof(userDir));
	SceInt32 ret = sceAppMgrGetUserDirPath(GAME_USER_DIR_PARTITION, userDir, sizeof(userDir));
	if (ret != 0)
		return ret;

	char fullPath[512];
	sce_paf_memset(fullPath, 0, sizeof(fullPath));
	sce_paf_snprintf(fullPath, sizeof(fullPath), "%s/%s", userDir, subDirName);
	return paf::Dir::RemoveRecursive(fullPath);
}

/* 0x81395106 */
SceInt32 importSavedataOpenSfoFile(const char *path, ImportSavedataSfoFile *outSfo)
{
	SceIoStat st;
	sce_paf_memset(&st, 0, sizeof(st));
	SceInt32 ret = sceIoGetstat(path, &st);
	if (ret < 0)
		return ret;

	SceUID fd = sceIoOpen(path, 1, 0);
	if (fd < 0)
		return fd;

	void *rawBuf = sce_paf_malloc(static_cast<SceSize>(st.st_size));
	outSfo->buffer = rawBuf;
	SceSSize bytesRead = sceIoRead(fd, rawBuf, static_cast<SceSize>(st.st_size));
	if (bytesRead < 0) {
		sceIoClose(fd);
		return bytesRead;
	}

	outSfo->size = static_cast<SceSize>(st.st_size);
	ret = sceIoClose(fd);
	if (ret < 0)
		return ret;

	return sfoOpen(outSfo->buffer, outSfo->size, &outSfo->handle);
}

/* 0x813951A0 */
SceInt32 importSavedataCloseSfoFile(ImportSavedataSfoFile *sfo)
{
	SceInt32 ret = sfoClose(sfo->handle);
	sce_paf_free(sfo->buffer);
	return ret;
}

/* 0x813951B8 */
SceInt32 importSavedataGetSfoParam(ImportSavedataSfoFile *sfo, const char *key,
				   const void **outValue, SceSize *outLen, SceSize *outMaxLen)
{
	void *param = NULL;
	SceInt32 ret = sfoFindParam(sfo->handle, key, &param);
	if (ret != 0)
		return ret;

	ret = sfoGetParamValue(sfo->handle, param, outValue);
	if (ret != 0)
		return ret;

	const char *foundKey = NULL;
	return sfoGetParamInfo(sfo->handle, param, &foundKey, outLen, outMaxLen);
}

/* 0x8139521A */
const char *importSavedataGetInfoKeyName(SceInt32 languageId)
{
	switch (languageId) {
	case 0: return "IMPORT_SAVEDATA_INFO_00";
	case 1: return "IMPORT_SAVEDATA_INFO_01";
	case 2: return "IMPORT_SAVEDATA_INFO_02";
	case 3: return "IMPORT_SAVEDATA_INFO_03";
	case 4: return "IMPORT_SAVEDATA_INFO_04";
	case 5: return "IMPORT_SAVEDATA_INFO_05";
	case 6: return "IMPORT_SAVEDATA_INFO_06";
	case 7: return "IMPORT_SAVEDATA_INFO_07";
	case 8: return "IMPORT_SAVEDATA_INFO_08";
	case 9: return "IMPORT_SAVEDATA_INFO_09";
	case 10: return "IMPORT_SAVEDATA_INFO_10";
	case 11: return "IMPORT_SAVEDATA_INFO_11";
	case 12: return "IMPORT_SAVEDATA_INFO_12";
	case 13: return "IMPORT_SAVEDATA_INFO_13";
	case 14: return "IMPORT_SAVEDATA_INFO_14";
	case 15: return "IMPORT_SAVEDATA_INFO_15";
	case 16: return "IMPORT_SAVEDATA_INFO_16";
	case 17: return "IMPORT_SAVEDATA_INFO_17";
	case 18: return "IMPORT_SAVEDATA_INFO_18";
	case 19: return "IMPORT_SAVEDATA_INFO_19";
	default: return "IMPORT_SAVEDATA_INFO";
	}
}

/* 0x8139531C */
const char *importSavedataGetTitleKeyName(SceInt32 languageId)
{
	switch (languageId) {
	case 0: return "TITLE_00";
	case 1: return "TITLE_01";
	case 2: return "TITLE_02";
	case 3: return "TITLE_03";
	case 4: return "TITLE_04";
	case 5: return "TITLE_05";
	case 6: return "TITLE_06";
	case 7: return "TITLE_07";
	case 8: return "TITLE_08";
	case 9: return "TITLE_09";
	case 10: return "TITLE_10";
	case 11: return "TITLE_11";
	case 12: return "TITLE_12";
	case 13: return "TITLE_13";
	case 14: return "TITLE_14";
	case 15: return "TITLE_15";
	case 16: return "TITLE_16";
	case 17: return "TITLE_17";
	case 18: return "TITLE_18";
	case 19: return "TITLE_19";
	default: return "TITLE";
	}
}
