/*
 * SceShell livearea_plugin: LiveAreaRequestUpdateJob & update/patch/activity/clean tasks
 * 3.65-CEX: 0x811B307C - 0x811B6624 (sti 0x811B65B4)
 */
#include <std/stdio.h>
#include <std/string.h>
#include <std/ctype.h>
#include <buffer/buffer.h>
#include <dir/dir.h>
#include <file/file.h>
#include <file/httpfile.h>
#include <file/localfile.h>
#include <system/system.h>
#include <rtc.h>
#include <modulemgr/module_info.h>
#include "livearea_internal.h"

struct LiveAreaPatchCheckParam {
	void *(*allocFn)(SceSize);
	void (*freeFn)(void *);
	int mode;
	char titleId[16];
	char appVer[8];
	char contentId[48];
	int sysVer;
	int pspSysVer;
	char gameDataId[20];
};

extern "C" {
extern int sceRegMgrGetKeyStr(const char *category, const char *name, char *buf, SceSize size);
extern int sceCompatGetPspSystemSoftwareVersion(void);
extern int vshSblSsIsDevelopmentMode(void);
extern int vshSblUtMgrHasComTestFlag(void);
extern const char *SceActivityDb_575DA6A6(const char *titleId);
extern const char *SceActivityDb_E6C23731(void);
extern void SceLsdb_C2B7D81B(paf::string *outEnv);
extern int SceLsdb_D22CFA8C(void *ctx, paf::string *titleId, SceRtcTick *outUtc);
extern int SceLsdb_87EE0526(AppInfoSClient *client, paf::string *titleId, LsdbAppInfoPtr *row);
extern SceInt64 SceLsdb_8BC2DF72(LsdbAppInfoPtr *row);
extern int SceLsdb_105428BF(void *ctx, paf::string *titleId, SceRtcTick *utc);
extern int SceLsdb_95EF7C51(void *ctx, paf::string *titleId, int flags);
extern const char *sceLsdbGetAppVer(LsdbAppInfoPtr *row);
extern const char *SceLsdb_8FDA59EA(LsdbAppInfoPtr *row);
extern const char *sceLsdbGetGameDataId(LsdbAppInfoPtr *row);
extern int SceLsdb_76EC65E3(AppInfoSClient *client, paf::string *titleId);
extern int SceLiveAreaUtil_B0571F88(const char *srcDir, const char *titleId, SceRtcTick *utc);
}

extern "C" {
int sceNpDrmInstallOpen(const char *dirPath, SceUInt32 flags, SceMode mode, void *workCtx);
int sceNpDrmInstallWrite(int handle, const void *buf, SceSize size);
int sceNpDrmInstallClose(int handle);
}

static inline int liveareaPkgOpen(const char *dir, int flags, int mode, void *option)
{
	return sceNpDrmInstallOpen(dir, static_cast<SceUInt32>(flags), static_cast<SceMode>(mode), option);
}
static inline int liveareaPkgWrite(int handle, const void *buf, SceSize size)
{
	return sceNpDrmInstallWrite(handle, buf, size);
}
static inline int liveareaPkgClose(int handle)
{
	return sceNpDrmInstallClose(handle);
}
extern "C" SceInt32 sceHmacSha256Digest(const void *secretkey, SceUInt32 keylen,
					const void *plain, SceUInt32 plainlen, void *digest);

/* 0x8137E058 */
int liveareaHmacSha256(const void *key, SceSize keyLen, const void *msg,
		       SceSize msgLen, void *outDigest)
{
	return sceHmacSha256Digest(key, (SceUInt32)keyLen, msg, (SceUInt32)msgLen, outDigest);
}
struct SceGameUpdateContext;
struct SceGameUpdateInitParam;
struct SceGameUpdateResult;
struct SceGameUpdateInfo;
extern int sceGameUpdateInit(SceGameUpdateContext *context, int option, const SceGameUpdateInitParam *param); /* 0x812A99AE */
extern int sceGameUpdateRun(SceGameUpdateContext *context, SceGameUpdateResult *result, SceGameUpdateInfo *info); /* 0x812A9BB0 */
extern int sceGameUpdateTerm(SceGameUpdateContext *context); /* 0x812A9708 */
extern int gameWhiteListDownload(void);				/* 0x810A36EE */

static inline int liveareaCleanCacheEntry(void)
{
	return gameWhiteListDownload();
}

static const SceUInt8 s_liveareaHmacKeyObf[32] = {		/* 0x81544DE4 */
	0xA7, 0xA4, 0x92, 0x09, 0x11, 0xD4, 0xB2, 0x17,
	0xAB, 0x02, 0xA4, 0x92, 0xE2, 0x43, 0xD7, 0x5A,
	0xFC, 0x02, 0x97, 0xD5, 0x07, 0xE4, 0x00, 0x86,
	0x52, 0xE5, 0x6A, 0xEB, 0x83, 0xA7, 0x95, 0xBB
};

/* 0x811B307C */
void LiveAreaUpdateTaskBase::OnFinish(int result)
{
	if (m_callback != NULL)
		m_callback(result, m_userdata);
}

/* 0x811B308E */
void LiveAreaUpdateTaskBase::Cancel()
{
	m_canceled = 1;
}

class LiveAreaRequestUpdateJob : public paf::job::JobItem {
public:
	LiveAreaRequestUpdateJob(SceUInt64 id, LiveAreaUpdatorImpl *updator, SceUInt8 requireNetwork,
				 const paf::common::SharedPtr<LiveAreaUpdateTaskBase> &task);
	virtual ~LiveAreaRequestUpdateJob() {}
	virtual int Run();
	virtual void Finish(int result);
	void CancelJob();

private:
	SceUInt64 m_id;						/* +0x20 */
	LiveAreaUpdatorImpl *m_updator;				/* +0x28 */
	SceUInt8 m_requireNetwork;				/* +0x2C */
	paf::common::SharedPtr<LiveAreaUpdateTaskBase> m_task;	/* +0x30 */
};

/* 0x811B3094 */
LiveAreaRequestUpdateJob::LiveAreaRequestUpdateJob(SceUInt64 id, LiveAreaUpdatorImpl *updator,
						   SceUInt8 requireNetwork,
						   const paf::common::SharedPtr<LiveAreaUpdateTaskBase> &task)
	: paf::job::JobItem("LiveAreaRequestUpdateJob", NULL),
	  m_id(id),
	  m_updator(updator),
	  m_requireNetwork(requireNetwork),
	  m_task(task)
{}

/* 0x811B30FC */
int LiveAreaRequestUpdateJob::Run()
{
	if (m_requireNetwork != 0)
		netCtlIntAcquire();

	int res = m_task->Run();
	if (res != 0) {
		if (res == (int)0x80431007 ||
		    res == (int)0x80AF0822 ||
		    res == (int)0x80AF0824 ||
		    res == (int)0x80AF0833 ||
		    res == (int)0x80AF0834 ||
		    res == (int)0x80107C08 ||
		    res == (int)0x80107C0A ||
		    res == (int)0x80AF0863) {
			res = 0;
		}
	}

	if (m_requireNetwork != 0)
		netCtlIntRelease();
	return res;
}

/* 0x811B318A */
void LiveAreaRequestUpdateJob::Finish(int result)
{
	if (m_task.get() != NULL && !IsCanceled())
		m_task->OnFinish(result);
	if (m_updator != NULL)
		liveareaUpdatorCancelById(m_updator, &m_id);
}

/* 0x811B31BE */
void LiveAreaRequestUpdateJob::CancelJob()
{
	m_task->Cancel();
	Cancel();
}

/* ------------------------------------------------------------------ */
/* LiveAreaContentUpdateTask (0x811B31D2 - 0x811B5474)                */
/* ------------------------------------------------------------------ */

class LiveAreaContentUpdateTask : public LiveAreaUpdateTaskBase {
public:
	LiveAreaContentUpdateTask(const char *titleId, const SceUInt64 *timestamp,
				  SceUInt8 checkDevDir, SceUInt8 postNotice,
				  CompleteCallback callback, void *userdata, const char *localPkg);
	virtual ~LiveAreaContentUpdateTask() {}
	virtual int Run();

private:
	int StreamPkg(paf::File *file, int pkgHandle, SceUInt8 *outEmpty);
	int GetLastModified(paf::File *file, SceRtcTick *outUtc);
	int OpenHttp(paf::File *file);
	int OpenSource(paf::common::SharedPtr<paf::File> *outFile);

	SceUInt8 m_checkDevDir;					/* +0x19 */
	SceUInt8 m_postNotice;					/* +0x1A */
	paf::string m_localPkg;					/* +0x1C */
};

/* 0x811B31D2 */
LiveAreaContentUpdateTask::LiveAreaContentUpdateTask(const char *titleId,
						     const SceUInt64 *timestamp,
						     SceUInt8 checkDevDir, SceUInt8 postNotice,
						     CompleteCallback callback, void *userdata,
						     const char *localPkg)
	: LiveAreaUpdateTaskBase(titleId, callback, userdata),
	  m_checkDevDir(checkDevDir),
	  m_postNotice(postNotice),
	  m_localPkg(localPkg ? localPkg : "")
{
	(void)timestamp;
}

/* 0x811B3270 */
int LiveAreaContentUpdateTask::StreamPkg(paf::File *file, int pkgHandle, SceUInt8 *outEmpty)
{
	paf::common::SharedPtr<paf::Buffer> readBuf = paf::MallocBuffer::Allocate(0x8000);
	paf::common::SharedPtr<paf::Buffer> writeBuf = paf::MallocBuffer::Allocate(0x8000);
	if (readBuf.get() == NULL || writeBuf.get() == NULL)
		return (int)0x80107C04;

	void *activeReadPtr = readBuf->GetAddr();
	void *activeWritePtr = writeBuf->GetAddr();
	SceInt64 bytesRead = 0;

	int ret = file->ReadAsync(activeReadPtr, 0x8000);
	if (ret != 0)
		return ret;

	while (m_canceled == 0) {
		ret = file->WaitAsync(&bytesRead);
		if (ret != 0)
			return ret;
		if (bytesRead < 0)
			return (int)bytesRead;
		if (bytesRead == 0)
			break;

		void *completedBuf = activeReadPtr;
		activeReadPtr = activeWritePtr;
		activeWritePtr = completedBuf;
		*outEmpty = 0;

		ret = file->ReadAsync(activeReadPtr, 0x8000);
		if (ret != 0)
			return ret;

		int written = liveareaPkgWrite(pkgHandle, activeWritePtr, (SceSize)bytesRead);
		if (written < 0)
			return written;
		if (bytesRead != 0x8000)
			break;
	}
	return m_canceled ? (int)0x80107C05 : 0;
}

/* 0x811B362E */
int LiveAreaContentUpdateTask::GetLastModified(paf::File *file, SceRtcTick *outUtc)
{
	if (file->GetFileType() != paf::File::Type_Http)
		return 0;
	paf::string headerValue;
	int ret = ((paf::HttpFile *)file)->GetResponseHeader("Last-Modified", headerValue);
	if (ret == 0)
		ret = sceRtcParseDateTime(outUtc, headerValue.c_str());
	return ret;
}

/* 0x811B36C8 */
static void buildLiveAreaCdnId(paf::string *out, const char *titleId,
			       const paf::string &modelTag, const paf::string &country)
{
	if (sce_paf_strcmp("NPXS10002", titleId) == 0)
		*out = paf::string("LA-PN.") + modelTag + country + "-STORE";
	else if (sce_paf_strcmp("NPXS10010", titleId) == 0)
		*out = paf::string("LA-PN.") + modelTag + country + "-VDPL";
	else
		*out = titleId ? titleId : "";
}

/* 0x811B3BD2 */
static int computeLiveAreaUrlHash(const char *env, const char *cdnId,
				  char *outHex, SceSize outSize)
{
	SceUInt8 key[32];
	sce_paf_memcpy(key, s_liveareaHmacKeyObf, sizeof(key));
	for (int i = 0; i < 32; ++i)
		key[i] ^= 0x7Fu;

	paf::string message = paf::string(env ? env : "") + "_" + paf::string(cdnId ? cdnId : "");
	SceUInt8 digest[32];
	int ret = liveareaHmacSha256(key, sizeof(key), message.c_str(), message.size(), digest);
	if (ret != 0)
		return ret;

	if (outHex == NULL || outSize < 65)
		return (int)0x80107C02;

	static const char hexDigits[] = "0123456789abcdef";
	for (SceSize i = 0; i < 32 && (2 * i + 1) < outSize; ++i) {
		outHex[2 * i] = hexDigits[(digest[i] >> 4) & 0xF];
		outHex[2 * i + 1] = hexDigits[digest[i] & 0xF];
	}
	outHex[64] = '\0';
	return 0;
}

/* 0x811B3EF8 */
static int buildLiveAreaPkgUrl(const paf::string *titleId, paf::string *outUrl)
{
	const char *mappedTitle = SceActivityDb_575DA6A6(titleId->c_str());
	paf::string env;
	paf::string country;
	paf::string cdnId;
	paf::string modelTag(paf::system::Model::GetType() == 1 ? "DC." : "VT.");

	if (sce_paf_strcmp("NPXS10010", mappedTitle) != 0 &&
	    sce_paf_strcmp("NPXS10002", mappedTitle) != 0 &&
	    vshSblUtMgrHasComTestFlag() != 0) {
		env = "e1-np";
	} else {
		SceLsdb_C2B7D81B(&env);
	}

	char countryBuf[4];
	int ret = sceRegMgrGetKeyStr("/CONFIG/NP", "country", countryBuf, 3);
	if (ret != 0)
		return ret;

	for (int i = 0; i < 3; ++i)
		countryBuf[i] = (char)sce_paf_toupper(countryBuf[i]);
	country = countryBuf;

	buildLiveAreaCdnId(&cdnId, mappedTitle, modelTag, country);

	char hashHex[68];
	ret = computeLiveAreaUrlHash(env.c_str(), cdnId.c_str(), hashHex, 65);
	if (ret != 0)
		return ret;

	paf::common::SharedPtr<paf::Buffer> urlBuf = paf::MallocBuffer::Allocate(0x400);
	if (urlBuf.get() == NULL)
		return (int)0x80107C04;

	char *dst = (char *)urlBuf->GetAddr();
	if (sce_paf_strcmp("NPXS10010", mappedTitle) == 0) {
		sce_paf_snprintf(dst, 0x400,
			"http://livearea.%s.dl.playstation.net/livearea/n/info/%s/LA-PN.%s%s-VDPL/%s/LA-PN.%s%s-VDPL-0.pkg?country=%s",
			env.c_str(), env.c_str(), modelTag.c_str(), country.c_str(),
			hashHex, modelTag.c_str(), country.c_str(), country.c_str());
	} else if (sce_paf_strcmp("NPXS10002", mappedTitle) == 0) {
		sce_paf_snprintf(dst, 0x400,
			"http://livearea.%s.dl.playstation.net/livearea/n/info/%s/LA-PN.%s%s-STORE/%s/LA-PN.%s%s-STORE-0.pkg?country=%s",
			env.c_str(), env.c_str(), modelTag.c_str(), country.c_str(),
			hashHex, modelTag.c_str(), country.c_str(), country.c_str());
	} else {
		sce_paf_snprintf(dst, 0x400,
			"http://livearea.%s.dl.playstation.net/livearea/e/info/%s/%s/%s/%s-0.pkg?country=%s",
			env.c_str(), env.c_str(), mappedTitle, hashHex, mappedTitle, country.c_str());
	}
	*outUrl = dst;
	return 0;
}

/* 0x811B4328 */
int LiveAreaContentUpdateTask::OpenHttp(paf::File *file)
{
	paf::string url;
	int ret = buildLiveAreaPkgUrl(&m_titleId, &url);
	if (ret != 0)
		return ret;

	SceRtcTick utc;
	utc.tick = 0;
	char lsdbHandle[8];
	SceLsdb_D22CFA8C(lsdbHandle, &m_titleId, &utc);

	paf::HttpFile::OpenArg openArg;
	ret = openArg.ParseUrl(url.c_str());
	if (ret != 0)
		return ret;

	if (utc.tick != 0) {
		char rfcDate[64];
		sce_paf_memset(rfcDate, 0, sizeof(rfcDate));
		if (sceRtcFormatRFC2822(rfcDate, &utc, 0) == 0)
			openArg.AddRequestHeader("If-Modified-Since", rfcDate);
	}
	return file->Open(&openArg);
}

/* 0x811B465E */
int LiveAreaContentUpdateTask::OpenSource(paf::common::SharedPtr<paf::File> *outFile)
{
	if (m_localPkg.size() == 0) {
		paf::common::SharedPtr<paf::File> httpFile(new paf::HttpFile());
		*outFile = httpFile;
		return OpenHttp(outFile->get());
	}
	paf::common::SharedPtr<paf::File> localFile(new paf::LocalFile());
	*outFile = localFile;
	paf::LocalFile::OpenArg openArg;
	openArg.filename = m_localPkg.c_str();
	openArg.flag = 1;
	openArg.mode = 6;
	return (*outFile)->Open(&openArg);
}

/* 0x811B48D8 */
static int clearDirectoryContents(const char *dirPath)
{
	paf::Dir dir;
	paf::DirEnt entry;
	if (dir.Open(dirPath) == 0) {
		while (dir.Read(&entry) == 0) {
			if (entry.name.c_str()[0] != '.') {
				paf::string fullPath =
					paf::string(dirPath ? dirPath : "") + "/" + entry.name;
				if (entry.type != paf::DirEnt::Type_Dir)
					paf::LocalFile::RemoveFile(fullPath.c_str());
				else
					paf::Dir::RemoveRecursive(fullPath.c_str());
			}
		}
	}
	dir.Close();
	return 0;
}

/* 0x811B4A7E */
int LiveAreaContentUpdateTask::Run()
{
	SceRtcTick utc;
	utc.tick = 0;
	paf::string srcDir;

	if (m_checkDevDir != 0 && vshSblSsIsDevelopmentMode() != 0) {
		if (paf::Dir::IsExist("host0:livearea_update/contents"))
			srcDir = "host0:livearea_update/contents";
		else if (paf::Dir::IsExist("ux0:data/livearea_update/contents"))
			srcDir = "ux0:data/livearea_update/contents";
	}

	int res = 0;
	if (srcDir.size() == 0) {
		paf::common::SharedPtr<paf::File> file;
		res = OpenSource(&file);
		if (res != 0)
			return res;

		if (paf::Dir::IsExist("ur0:temp/livearea"))
			res = clearDirectoryContents("ur0:temp/livearea");
		else
			res = paf::Dir::CreateRecursive("ur0:temp/livearea", 6);
		if (res != 0)
			return res;

		SceInt64 waitRes = 0;
		res = file->WaitAsync(&waitRes);
		if (res != 0)
			return res;
		if ((int)waitRes != 0)
			return (int)waitRes;

		res = GetLastModified(file.get(), &utc);
		if (res != 0)
			return res;

		SceUInt32 pkgOption[512];
		sce_paf_memset(pkgOption, 0, sizeof(pkgOption));
		pkgOption[0] = 4;
		int pkgHandle = liveareaPkgOpen("ur0:temp/livearea/", 67074, 6, pkgOption);
		res = pkgHandle;
		if (pkgHandle >= 0) {
			SceUInt8 isEmpty = 1;
			res = StreamPkg(file.get(), pkgHandle, &isEmpty);
			liveareaPkgClose(pkgHandle);
			if (res == 0 && isEmpty != 0)
				res = (int)0x80107C08;
		}
		if (res != 0)
			return res;

		LsdbAppInfoPtr appInfo(new LsdbAppInfo());
		AppInfoSClient lsdbClient;
		SceLsdb_D322A870(&lsdbClient);
		res = SceLsdb_87EE0526(&lsdbClient, &m_titleId, &appInfo);
		if (res == 0) {
			if (SceLsdb_8BC2DF72(&appInfo) == 3)
				srcDir = "ur0:temp/livearea/sce_sys/retail/livearea/contents";
			else
				srcDir = "ur0:temp/livearea/sce_sys/livearea/contents";
		}
		SceLsdb_56D8DCF0(&lsdbClient);
		if (res != 0)
			return res;
	}

	res = SceLiveAreaUtil_B0571F88(srcDir.c_str(), m_titleId.c_str(), &utc);
	if (res != 0) {
		char lsdbStamp;
		SceLsdb_105428BF(&lsdbStamp, &m_titleId, &utc);
		paf::Plugin *topmenu = paf::Plugin::Find("topmenu_plugin");
		if (topmenu != NULL) {
			LiveAreaTopmenuInterface *topmenuIf =
				(LiveAreaTopmenuInterface *)topmenu->GetInterface(1);
			if (topmenuIf != NULL) {
				const char *notifyArgs[3] = { m_titleId.c_str(), NULL, NULL };
				topmenuIf->notifyContentUpdated(notifyArgs);
			}
		}
	}

	clearDirectoryContents("ur0:temp/livearea");

	if (res == 0 && m_postNotice != 0) {
		LsdbNewEvent notice;
		notice.titleId = "LAUPDATE";
		notice.itemId = m_titleId;
		notice.msgType = 2304;
		notice.actType = 2;
		notice.newFlag = true;
		notice.popupNo = true;
		notice.execTitleId = m_titleId;

		const char *curApp = SceActivityDb_E6C23731();
		if (curApp != NULL && sce_paf_strcmp(m_titleId.c_str(), curApp) == 0)
			notice.iconPath = paf::string("vs0:app/") + curApp + "/sce_sys/icon0.png";

		res = SceLsdb_315B9FD6(&notice, true);
	}
	return res;
}

/* ------------------------------------------------------------------ */
/* LiveAreaPatchCheckTask (0x811B5474 - 0x811B577A)                   */
/* ------------------------------------------------------------------ */

class LiveAreaPatchCheckTask : public LiveAreaUpdateTaskBase {
public:
	LiveAreaPatchCheckTask(const char *titleId, int appType,
			       CompleteCallback callback, void *userdata);
	virtual ~LiveAreaPatchCheckTask() {}
	virtual int Run();

private:
	int m_appType;						/* +0x1C */
};

/* 0x811B5474 */
LiveAreaPatchCheckTask::LiveAreaPatchCheckTask(const char *titleId, int appType,
					       CompleteCallback callback, void *userdata)
	: LiveAreaUpdateTaskBase(titleId, callback, userdata),
	  m_appType(appType)
{}

/* 0x811B54E4 */
int LiveAreaPatchCheckTask::Run()
{
	char lsdbHandle[4];
	if (SceLsdb_95EF7C51(lsdbHandle, &m_titleId, 0) != 0)
		return 0;

	LiveAreaPatchCheckParam param;
	sce_paf_memset(&param, 0, sizeof(param));
	param.allocFn = sce_paf_malloc;
	param.freeFn = sce_paf_free;
	if (m_appType == 2)
		param.mode = 1;
	else if (m_appType == 1)
		param.mode = 2;
	else
		param.mode = 0;

	sce_paf_strncpy(param.titleId, m_titleId.c_str(), 12);
	if (param.mode == 1)
		param.pspSysVer = sceCompatGetPspSystemSoftwareVersion();

	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	AppInfoSClient lsdbClient;
	SceLsdb_D322A870(&lsdbClient);
	int res = SceLsdb_87EE0526(&lsdbClient, &m_titleId, &appInfo);
	if (res == 0) {
		const char *appVer = sceLsdbGetAppVer(&appInfo);
		const char *contentId = SceLsdb_8FDA59EA(&appInfo);
		const char *gameDataId = sceLsdbGetGameDataId(&appInfo);
		if (appVer != NULL)
			sce_paf_strncpy(param.appVer, appVer, 5);
		if (contentId != NULL)
			sce_paf_strncpy(param.contentId, contentId, 47);
		if (gameDataId != NULL)
			sce_paf_strncpy(param.gameDataId, gameDataId, 12);
	}
	SceLsdb_56D8DCF0(&lsdbClient);

	if (res == 0) {
		SceKernelSystemSwVersion swInfo;
		sce_paf_memset(&swInfo, 0, sizeof(swInfo));
		swInfo.size = sizeof(swInfo);
		res = sceKernelGetSystemSwVersion(&swInfo);
		if (res == 0)
			param.sysVer = (int)swInfo.version;
	}
	if (res == 0) {
		SceUInt32 statusBuf[4];
		SceUInt8 patchBuf[412];
		SceUInt32 checkerCtx[3];
		sce_paf_memset(statusBuf, 0, sizeof(statusBuf));
		sce_paf_memset(patchBuf, 0, sizeof(patchBuf));
		sce_paf_memset(checkerCtx, 0, sizeof(checkerCtx));
		statusBuf[0] = 16;

		res = sceGameUpdateInit((SceGameUpdateContext *)checkerCtx, 0, (const SceGameUpdateInitParam *)&param);
		if (res == 0) {
			res = sceGameUpdateRun((SceGameUpdateContext *)checkerCtx, (SceGameUpdateResult *)statusBuf, (SceGameUpdateInfo *)patchBuf);
			sceGameUpdateTerm((SceGameUpdateContext *)checkerCtx);
			if (res == 0) {
				if (statusBuf[1] == 1)
					return 2;
				if (statusBuf[1] == 3)
					return (int)0x80107C0A;
			}
		}
	}
	return res;
}

/* ------------------------------------------------------------------ */
/* LiveAreaActivityUpdateTask (0x811B577A - 0x811B6554)               */
/* ------------------------------------------------------------------ */

class LiveAreaActivityUpdateTask : public LiveAreaUpdateTaskBase {
public:
	typedef void (*ActivityCallback)(int result, SceInt64 *timestamp, void *userdata);

	LiveAreaActivityUpdateTask(const char *titleId, int forceFlag,
				   ActivityCallback callback, void *userdata);
	virtual ~LiveAreaActivityUpdateTask() {}
	virtual int Run();
	virtual void OnFinish(int result);
	virtual void Cancel();

private:
	int FetchActivity(ActivityDbSyncSummary *outSummary);
	static void OnSyncComplete(int result, bool changed, const paf::string *titleId,
				   const SceInt64 *syncedTime,
				   const ActivityDbSyncSummary *summary, void *userdata);

	int m_activityHandle;					/* +0x1C */
	int m_forceFlag;					/* +0x20 */
	ActivityCallback m_activityCallback;			/* +0x24 */
	SceInt64 m_lastSyncTime;				/* +0x28 */
	paf::thread::Semaphore m_sem;				/* +0x30 */
};

struct ActivitySyncWaitContext {
	LiveAreaActivityUpdateTask *task;
	ActivityDbSyncSummary *outSummary;
	int result;
};

/* 0x811B577A */
LiveAreaActivityUpdateTask::LiveAreaActivityUpdateTask(const char *titleId, int forceFlag,
						       ActivityCallback callback, void *userdata)
	: LiveAreaUpdateTaskBase(titleId, NULL, userdata),
	  m_forceFlag(forceFlag),
	  m_activityCallback(callback),
	  m_lastSyncTime(0),
	  m_sem("SceLiveAreaUpdateActivity", 0, 2, NULL)
{
	new (&m_activityHandle) ActivityDbSyncHandle();
}

/* 0x811B5816 */
int LiveAreaActivityUpdateTask::FetchActivity(ActivityDbSyncSummary *outSummary)
{
	ActivitySyncWaitContext waitCtx;
	waitCtx.task = this;
	waitCtx.outSummary = outSummary;
	waitCtx.result = 0;

	ActivityDbSyncParam param;
	param.condition.type = (sce_paf_strcmp(m_titleId.c_str(), "NPXS10006") == 0)
		? ACTIVITY_DB_QUERY_ALL
		: ACTIVITY_DB_QUERY_TITLE;
	param.condition.titleId = m_titleId;
	param.condition.limit = 0;
	param.since = 0;
	param.keepNewComments = (m_forceFlag != 0) ? 1 : 0;
	param.callback = OnSyncComplete;
	param.arg = &waitCtx;
	param.reload = false;

	int ret = reinterpret_cast<ActivityDbSyncHandle *>(&m_activityHandle)->SyncActivity(&param);
	if (ret == 0) {
		m_sem.Acquire();
		ret = waitCtx.result;
	}
	return ret;
}

/* 0x811B58F6 */
static int postFriendsActivityNotice(const ActivityDbSyncSummary *summary)
{
	LsdbNewEvent notice;
	char numBuf[64];

	if (summary->addedComments != 0) {
		if (summary->addedVotes != 0) {
			notice.msgType = 2311;
			sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld",
					 (SceInt64)summary->addedComments);
			notice.msgArg[0] = numBuf;
			sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld",
					 (SceInt64)summary->addedVotes);
			notice.msgArg[1] = numBuf;
		} else {
			notice.msgType = 2309;
			sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld",
					 (SceInt64)summary->addedComments);
			notice.msgArg[0] = numBuf;
			if (summary->addedComments == 1 && summary->rowid != 0) {
				sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", summary->rowid);
				notice.execArg = paf::string("status_uid=") + numBuf;
			}
		}
	} else if (summary->addedVotes != 0) {
		notice.msgType = 2310;
		sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", (SceInt64)summary->addedVotes);
		notice.msgArg[0] = numBuf;
		if (summary->addedVotes == 1 && summary->rowid != 0) {
			sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", summary->rowid);
			notice.execArg = paf::string("status_uid=") + numBuf;
		}
	} else {
		return 0;
	}

	notice.titleId = "LAUPDATE";
	notice.itemId = "NPXS10006";
	notice.execTitleId = "NPXS10006";
	notice.newFlag = true;
	notice.actType = 8;
	return (SceLsdb_315B9FD6(&notice, true) == 0) ? 1 : 0;
}

/* 0x811B5F40 */
static int postTitleActivityNotice(const ActivityDbSyncSummary *summary,
				   const paf::string *fallbackTitleId)
{
	int msgType;
	if (summary->ownerType == ACTIVITY_DB_OWNER_SELF) {
		if (summary->hasNewComments)
			msgType = summary->hasNewVotes ? 2308 : 2306;
		else if (summary->hasNewVotes)
			msgType = 2307;
		else
			return 0;
	} else if (summary->ownerType == ACTIVITY_DB_OWNER_USER && summary->hasNewComments) {
		msgType = 2305;
	} else {
		return 0;
	}

	AppInfoSClient lsdbClient;
	SceLsdb_D322A870(&lsdbClient);
	int exists = SceLsdb_76EC65E3(&lsdbClient, (paf::string *)&summary->titleId);
	SceLsdb_56D8DCF0(&lsdbClient);

	const paf::string *targetTitle = exists ? &summary->titleId : fallbackTitleId;

	LsdbNewEvent notice;
	notice.titleId = "LAUPDATE";
	notice.itemId = *targetTitle;
	notice.msgType = msgType;
	notice.actType = 8;
	notice.newFlag = true;
	notice.popupNo = true;
	notice.execTitleId = *targetTitle;

	if (summary->rowid != 0) {
		char numBuf[64];
		sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", summary->rowid);
		notice.execArg = paf::string("status_uid=") + numBuf;
	}

	if (msgType == 2305) {
		const char *sender = summary->ownerId.c_str();
		const char *atSign = sce_paf_strchr(sender, '@');
		if (atSign != NULL)
			notice.msgArg[0] = paf::string(sender, (SceSize)(atSign - sender));
		else
			notice.msgArg[0] = summary->ownerId;
	}

	return SceLsdb_315B9FD6(&notice, true);
}

/* 0x811B63F4 */
int LiveAreaActivityUpdateTask::Run()
{
	ActivityDbSyncSummary summary;
	int res = FetchActivity(&summary);
	if (res == 0 && m_forceFlag == 0) {
		if (sce_paf_strcmp(m_titleId.c_str(), "NPXS10006") == 0)
			postFriendsActivityNotice(&summary);
		else
			postTitleActivityNotice(&summary, &m_titleId);
	}
	return res;
}

/* 0x811B64D0 */
void LiveAreaActivityUpdateTask::OnFinish(int result)
{
	if (m_activityCallback != NULL)
		m_activityCallback(result, &m_lastSyncTime, m_userdata);
}

/* 0x811B64E8 */
void LiveAreaActivityUpdateTask::Cancel()
{
	reinterpret_cast<ActivityDbSyncHandle *>(&m_activityHandle)->Cancel();
	LiveAreaUpdateTaskBase::Cancel();
}

/* 0x811B64F8 */
void LiveAreaActivityUpdateTask::OnSyncComplete(int result, bool changed,
						const paf::string *titleId,
						const SceInt64 *syncedTime,
						const ActivityDbSyncSummary *summary,
						void *userdata)
{
	(void)changed;
	(void)titleId;
	ActivitySyncWaitContext *waitCtx = (ActivitySyncWaitContext *)userdata;
	waitCtx->result = result;
	if (syncedTime != NULL)
		waitCtx->task->m_lastSyncTime = *syncedTime;
	if (summary != NULL && waitCtx->outSummary != NULL)
		*waitCtx->outSummary = *summary;
	waitCtx->task->m_sem.Release();
}

/* ------------------------------------------------------------------ */
/* LiveAreaCleanTask (0x811B6554 - 0x811B6624)                        */
/* ------------------------------------------------------------------ */

class LiveAreaCleanTask : public LiveAreaUpdateTaskBase {
public:
	LiveAreaCleanTask(const char *titleId, CompleteCallback callback, void *userdata)
		: LiveAreaUpdateTaskBase(titleId, callback, userdata)
	{}
	virtual ~LiveAreaCleanTask() {}
	virtual int Run()
	{
		return liveareaCleanCacheEntry();
	}
};

paf::job::JobItem *liveareaRequestUpdateJobCtor(void *mem, SceUInt64 id,
						LiveAreaUpdatorImpl *updator,
						SceUInt8 requireNetwork,
						const paf::common::SharedPtr<LiveAreaUpdateTaskBase> &task)
{
	return new (mem) LiveAreaRequestUpdateJob(id, updator, requireNetwork, task);
}

LiveAreaUpdateTaskBase *liveareaContentUpdateTaskCtor(void *mem, const char *titleId,
						      const SceUInt64 *timestamp,
						      SceUInt8 checkDevDir, SceUInt8 postNotice,
						      void *callback, void *userdata,
						      const char *localPkg)
{
	return new (mem) LiveAreaContentUpdateTask(titleId, timestamp, checkDevDir, postNotice,
						   (LiveAreaUpdateTaskBase::CompleteCallback)callback,
						   userdata, localPkg);
}

LiveAreaUpdateTaskBase *liveareaPatchCheckTaskCtor(void *mem, const char *titleId,
						   int appType, void *callback, void *userdata)
{
	return new (mem) LiveAreaPatchCheckTask(titleId, appType,
						(LiveAreaUpdateTaskBase::CompleteCallback)callback,
						userdata);
}

LiveAreaUpdateTaskBase *liveareaActivityUpdateTaskCtor(void *mem, const char *titleId,
						       int forceFlag, void *callback,
						       void *userdata)
{
	return new (mem) LiveAreaActivityUpdateTask(titleId, forceFlag,
						    (LiveAreaActivityUpdateTask::ActivityCallback)callback,
						    userdata);
}

LiveAreaUpdateTaskBase *liveareaCleanTaskCtor(void *mem, const char *titleId,
					      void *callback, void *userdata)
{
	return new (mem) LiveAreaCleanTask(titleId,
					   (LiveAreaUpdateTaskBase::CompleteCallback)callback,
					   userdata);
}
