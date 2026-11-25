/*
 * SceShell livearea_plugin: LiveAreaRequestUpdateJob & update/promote/activity/clean tasks
 * 3.65-CEX: 0x811B307C - 0x811B6624 (sti 0x811B65B4)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/ctype.h>
#include <std/string>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/file.h>
#include <file/httpfile.h>
#include <file/localfile.h>
#include <job/job.h>
#include <plugin.h>
#include <system/system.h>
#include <thread/thread.h>

#include "../../livespace_db/lsdb_keys.h"

struct SceRtcTickLocal {
	SceUInt64 tick;
};

extern "C" {
extern int sceRtcParseDateTime(SceRtcTickLocal *out, const char *str);
extern int sceRtcFormatRFC2822(char *out, const SceRtcTickLocal *utc, int tz);
extern int sceRegMgrGetKeyStrForUser(const char *cat, const char *name, char *buf, SceSize size);
extern int sceKernelGetSystemSwVersion(void *param);
extern int sceCompatGetPspSystemSoftwareVersion(int arg);
extern int vshSblSsIsDevelopmentMode(void);
extern int vshSblUtMgrHasComTestFlag(void);
extern const char *SceActivityDb_575DA6A6(const char *titleId);
extern const char *SceActivityDb_E6C23731(void);
extern void SceActivityDb_B83F0A77(void *ctx);
extern int SceActivityDb_41E31BEF(void *ctx, void *req);
extern int SceActivityDb_5B44F019(void *ctx);
extern void SceLsdb_C2B7D81B(paf::string *outEnv);
extern int SceLsdb_D22CFA8C(void *ctx, paf::string *titleId, SceRtcTickLocal *outUtc);
extern void SceLsdb_D322A870(void *ctx);
extern void SceLsdb_56D8DCF0(void *ctx);
extern int SceLsdb_87EE0526(void *ctx, paf::string *titleId, paf::common::SharedPtr<int> *row);
extern SceInt64 SceLsdb_8BC2DF72(paf::common::SharedPtr<int> *row);
extern int SceLsdb_105428BF(void *ctx, paf::string *titleId, SceRtcTickLocal *utc);
extern int SceLsdb_315B9FD6(void *notice, int flag);
extern int SceLsdb_95EF7C51(void *ctx, paf::string *titleId, int a3);
extern const char *sceLsdbGetAppVer(paf::common::SharedPtr<int> *row);
extern const char *SceLsdb_8FDA59EA(paf::common::SharedPtr<int> *row);
extern const char *sceLsdbGetGameDataId(paf::common::SharedPtr<int> *row);
extern int SceLsdb_76EC65E3(void *ctx, paf::string *titleId);
extern int SceLiveAreaUtil_B0571F88(const char *srcDir, const char *titleId, SceRtcTickLocal *utc);
}

extern void liveareaPowerLockInc(void);				/* 0x8120EE1E */
extern void liveareaPowerLockDec(void);				/* 0x8120EE3C */
extern int liveareaUpdatorCancelById(void *updator, const SceUInt64 *id); /* 0x811B2FA8 */
extern int liveareaPkgOpen(const char *dir, int flags, int mode, void *opt); /* 0x8136D6A2 */
extern int liveareaPkgWrite(int handle, const void *buf, SceSize size); /* 0x8136D8F4 */
extern int liveareaPkgClose(int handle);			/* 0x8136D828 */
extern int liveareaHmacSha256(const void *key, SceSize keyLen, const void *data, SceSize dataLen, void *out); /* 0x8137E058 */
extern int liveareaPatchCheckInit(void *ctx, int a2, void *param); /* 0x812A99AE */
extern int liveareaPatchCheckRun(void *ctx, void *res, void *buf); /* 0x812A9BB0 */
extern void liveareaPatchCheckTerm(void *ctx);			/* 0x812A9708 */
extern int liveareaCleanCacheEntry(void);			/* 0x810A36EE */

static const SceUInt8 s_liveareaHmacKeyObf[32] = {		/* 0x81544DE4 */
	0xA7, 0xA4, 0x92, 0x09, 0x11, 0xD4, 0xB2, 0x17,
	0xAB, 0x02, 0xA4, 0x92, 0xE2, 0x43, 0xD7, 0x5A,
	0xFC, 0x02, 0x97, 0xD5, 0x07, 0xE4, 0x00, 0x86,
	0x52, 0xE5, 0x6A, 0xEB, 0x83, 0xA7, 0x95, 0xBB
};

class LiveAreaUpdateTaskBase {
public:
	typedef void (*CompleteCb)(int result, void *userdata);

	LiveAreaUpdateTaskBase(const char *titleId, CompleteCb cb, void *userdata)
		: m_titleId(titleId ? titleId : ""), m_cb(cb), m_userdata(userdata), m_canceled(0)
	{}

	virtual ~LiveAreaUpdateTaskBase() {}
	virtual int Run() = 0;
	virtual void OnFinish(int result);			/* 0x811B307C */
	virtual void Cancel();					/* 0x811B308E */

	paf::string m_titleId;					/* +4 */
	CompleteCb m_cb;					/* +16 */
	void *m_userdata;					/* +20 */
	SceUInt8 m_canceled;					/* +24 */
};

/* 0x811B307C */
void LiveAreaUpdateTaskBase::OnFinish(int result)
{
	if (m_cb)
		m_cb(result, m_userdata);
}

/* 0x811B308E */
void LiveAreaUpdateTaskBase::Cancel()
{
	m_canceled = 1;
}

class LiveAreaRequestUpdateJob : public paf::job::JobItem {
public:
	LiveAreaRequestUpdateJob(SceUInt64 id, void *updator, SceUInt8 lockPower,
		const paf::common::SharedPtr<LiveAreaUpdateTaskBase> &task);
	virtual ~LiveAreaRequestUpdateJob() {}
	virtual int Run();
	virtual void Finish(int result);
	void CancelJob();

private:
	SceUInt64 m_id;						/* +32 */
	void *m_updator;					/* +40 */
	SceUInt8 m_lockPower;					/* +44 */
	paf::common::SharedPtr<LiveAreaUpdateTaskBase> m_task;	/* +48 */
};

/* 0x811B3094 */
LiveAreaRequestUpdateJob::LiveAreaRequestUpdateJob(SceUInt64 id, void *updator, SceUInt8 lockPower,
	const paf::common::SharedPtr<LiveAreaUpdateTaskBase> &task)
	: paf::job::JobItem("LiveAreaRequestUpdateJob", NULL),
	  m_id(id), m_updator(updator), m_lockPower(lockPower), m_task(task)
{}

/* 0x811B30FC */
int LiveAreaRequestUpdateJob::Run()
{
	if (m_lockPower != 0)
		liveareaPowerLockInc();

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

	if (m_lockPower != 0)
		liveareaPowerLockDec();
	return res;
}

/* 0x811B318A */
void LiveAreaRequestUpdateJob::Finish(int result)
{
	if (m_task.get() && !IsCanceled())
		m_task->OnFinish(result);
	if (m_updator)
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
	LiveAreaContentUpdateTask(const char *titleId, int a3, SceUInt8 checkDevDir,
		SceUInt8 postNotice, CompleteCb cb, void *userdata, const char *localPkg);
	virtual ~LiveAreaContentUpdateTask() {}
	virtual int Run();

private:
	int StreamPkg(paf::File *file, int pkgHandle, SceUInt8 *outEmpty);
	int GetLastModified(paf::File *file, SceRtcTickLocal *outUtc);
	int OpenHttp(paf::File *file);
	int OpenSource(paf::common::SharedPtr<paf::File> *outFile);

	SceUInt8 m_checkDevDir;					/* +25 */
	SceUInt8 m_postNotice;					/* +26 */
	paf::string m_localPkg;					/* +28 */
};

/* 0x811B31D2 */
LiveAreaContentUpdateTask::LiveAreaContentUpdateTask(const char *titleId, int, SceUInt8 checkDevDir,
	SceUInt8 postNotice, CompleteCb cb, void *userdata, const char *localPkg)
	: LiveAreaUpdateTaskBase(titleId, cb, userdata),
	  m_checkDevDir(checkDevDir), m_postNotice(postNotice),
	  m_localPkg(localPkg ? localPkg : "")
{}

/* 0x811B3270 */
int LiveAreaContentUpdateTask::StreamPkg(paf::File *file, int pkgHandle, SceUInt8 *outEmpty)
{
	paf::common::SharedPtr<paf::Buffer> bufA = paf::MallocBuffer::Allocate(0x8000);
	paf::common::SharedPtr<paf::Buffer> bufB = paf::MallocBuffer::Allocate(0x8000);
	if (!bufA.get() || !bufB.get())
		return (int)0x80107C04;

	void *pA = bufA->GetAddr();
	void *pB = bufB->GetAddr();
	SceInt64 nread = 0;

	int r = file->ReadAsync(pA, 0x8000);
	if (r != 0)
		return r;

	while (m_canceled == 0) {
		r = file->WaitAsync(&nread);
		if (r != 0)
			return r;
		if (nread < 0)
			return (int)nread;
		if (nread == 0)
			break;

		void *prev = pA;
		pA = pB;
		pB = prev;
		*outEmpty = 0;

		r = file->ReadAsync(pA, 0x8000);
		if (r != 0)
			return r;

		int wr = liveareaPkgWrite(pkgHandle, pB, (SceSize)nread);
		if (wr < 0)
			return wr;
		if (nread != 0x8000)
			break;
	}
	return m_canceled ? (int)0x80107C05 : 0;
}

/* 0x811B362E */
int LiveAreaContentUpdateTask::GetLastModified(paf::File *file, SceRtcTickLocal *outUtc)
{
	if (file->GetFileType() != paf::File::Type_Http)
		return 0;
	paf::string hdr;
	int r = ((paf::HttpFile *)file)->GetResponseHeader("Last-Modified", hdr);
	if (r == 0)
		r = sceRtcParseDateTime(outUtc, hdr.c_str());
	return r;
}

/* 0x811B36C8 */
static void buildLiveAreaCdnId(paf::string *out, const char *titleId,
	const paf::string &modelTag, const paf::string &country)
{
	if (sce_paf_strcmp("NPXS10002", titleId) == 0) {
		*out = paf::string("LA-PN.") + modelTag + country + "-STORE";
	} else if (sce_paf_strcmp("NPXS10010", titleId) == 0) {
		*out = paf::string("LA-PN.") + modelTag + country + "-VDPL";
	} else {
		*out = titleId ? titleId : "";
	}
}

/* 0x811B3BD2 */
static int computeLiveAreaUrlHash(const char *env, const char *cdnId, char *outHex, SceSize outSize)
{
	SceUInt8 key[32];
	sce_paf_memcpy(key, s_liveareaHmacKeyObf, sizeof(key));
	for (int i = 0; i < 32; ++i)
		key[i] ^= 0x7Fu;

	paf::string msg = paf::string(env ? env : "") + "_" + paf::string(cdnId ? cdnId : "");
	SceUInt8 digest[32];
	int r = liveareaHmacSha256(key, sizeof(key), msg.c_str(), msg.size(), digest);
	if (r != 0)
		return r;

	if (!outHex || outSize < 65)
		return (int)0x80107C02;

	static const char hex[] = "0123456789abcdef";
	for (SceSize i = 0; i < 32 && (2 * i + 1) < outSize; ++i) {
		outHex[2 * i] = hex[(digest[i] >> 4) & 0xF];
		outHex[2 * i + 1] = hex[digest[i] & 0xF];
	}
	outHex[64] = '\0';
	return 0;
}

/* 0x811B3EF8 */
static int buildLiveAreaPkgUrl(const paf::string *titleId, paf::string *outUrl)
{
	const char *mapped = SceActivityDb_575DA6A6(titleId->c_str());
	paf::string env;
	paf::string country;
	paf::string cdnId;
	paf::string modelTag(paf::system::Model::GetType() == 1 ? "DC." : "VT.");

	if (sce_paf_strcmp("NPXS10010", mapped) != 0 &&
	    sce_paf_strcmp("NPXS10002", mapped) != 0 &&
	    vshSblUtMgrHasComTestFlag() != 0) {
		env = "e1-np";
	} else {
		SceLsdb_C2B7D81B(&env);
	}

	char countryBuf[4];
	int r = sceRegMgrGetKeyStrForUser("/CONFIG/NP", "country", countryBuf, 3);
	if (r != 0)
		return r;

	for (int i = 0; i < 3; ++i)
		countryBuf[i] = (char)sce_paf_toupper(countryBuf[i]);
	country = countryBuf;

	buildLiveAreaCdnId(&cdnId, mapped, modelTag, country);

	char hashHex[68];
	r = computeLiveAreaUrlHash(env.c_str(), cdnId.c_str(), hashHex, 65);
	if (r != 0)
		return r;

	paf::common::SharedPtr<paf::Buffer> buf = paf::MallocBuffer::Allocate(0x400);
	if (!buf.get())
		return (int)0x80107C04;

	char *dst = (char *)buf->GetAddr();
	if (sce_paf_strcmp("NPXS10010", mapped) == 0) {
		sce_paf_snprintf(dst, 0x400,
			"http://livearea.%s.dl.playstation.net/livearea/n/info/%s/LA-PN.%s%s-VDPL/%s/LA-PN.%s%s-VDPL-0.pkg?country=%s",
			env.c_str(), env.c_str(), modelTag.c_str(), country.c_str(),
			hashHex, modelTag.c_str(), country.c_str(), country.c_str());
	} else if (sce_paf_strcmp("NPXS10002", mapped) == 0) {
		sce_paf_snprintf(dst, 0x400,
			"http://livearea.%s.dl.playstation.net/livearea/n/info/%s/LA-PN.%s%s-STORE/%s/LA-PN.%s%s-STORE-0.pkg?country=%s",
			env.c_str(), env.c_str(), modelTag.c_str(), country.c_str(),
			hashHex, modelTag.c_str(), country.c_str(), country.c_str());
	} else {
		sce_paf_snprintf(dst, 0x400,
			"http://livearea.%s.dl.playstation.net/livearea/e/info/%s/%s/%s/%s-0.pkg?country=%s",
			env.c_str(), env.c_str(), mapped, hashHex, mapped, country.c_str());
	}
	*outUrl = dst;
	return 0;
}

/* 0x811B4328 */
int LiveAreaContentUpdateTask::OpenHttp(paf::File *file)
{
	paf::string url;
	int r = buildLiveAreaPkgUrl(&m_titleId, &url);
	if (r != 0)
		return r;

	SceRtcTickLocal utc;
	utc.tick = 0;
	char lsdbCtx[8];
	SceLsdb_D22CFA8C(lsdbCtx, &m_titleId, &utc);

	paf::HttpFile::OpenArg arg;
	r = arg.ParseUrl(url.c_str());
	if (r != 0)
		return r;

	if (utc.tick != 0) {
		char rfcDate[64];
		sce_paf_memset(rfcDate, 0, sizeof(rfcDate));
		if (sceRtcFormatRFC2822(rfcDate, &utc, 0) == 0)
			arg.AddRequestHeader("If-Modified-Since", rfcDate);
	}
	return file->Open(&arg);
}

/* 0x811B465E */
int LiveAreaContentUpdateTask::OpenSource(paf::common::SharedPtr<paf::File> *outFile)
{
	if (m_localPkg.size() == 0) {
		paf::common::SharedPtr<paf::File> sp(new paf::HttpFile());
		*outFile = sp;
		return OpenHttp(outFile->get());
	}
	paf::common::SharedPtr<paf::File> sp(new paf::LocalFile());
	*outFile = sp;
	paf::LocalFile::OpenArg arg;
	arg.filename = m_localPkg.c_str();
	arg.flag = 1;
	arg.mode = 6;
	return (*outFile)->Open(&arg);
}

/* 0x811B48D8 */
static int clearDirectoryContents(const char *dirPath)
{
	paf::Dir dir;
	paf::DirEnt ent;
	if (dir.Open(dirPath) == 0) {
		while (dir.Read(&ent) == 0) {
			if (ent.name.c_str()[0] != '.') {
				paf::string full = paf::string(dirPath ? dirPath : "") + "/" + ent.name;
				if (ent.type != paf::DirEnt::Type_Dir)
					paf::LocalFile::RemoveFile(full.c_str());
				else
					paf::Dir::RemoveRecursive(full.c_str());
			}
		}
	}
	dir.Close();
	return 0;
}

/* 0x811B4A7E */
int LiveAreaContentUpdateTask::Run()
{
	SceRtcTickLocal utc;
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

		SceUInt32 pkgOpt[512];
		sce_paf_memset(pkgOpt, 0, sizeof(pkgOpt));
		pkgOpt[0] = 4;
		int pkg = liveareaPkgOpen("ur0:temp/livearea/", 67074, 6, pkgOpt);
		res = pkg;
		if (pkg >= 0) {
			SceUInt8 empty = 1;
			res = StreamPkg(file.get(), pkg, &empty);
			liveareaPkgClose(pkg);
			if (res == 0 && empty != 0)
				res = (int)0x80107C08;
		}
		if (res != 0)
			return res;

		paf::common::SharedPtr<int> row(new int[3]);
		char lsdbCtx[4];
		SceLsdb_D322A870(lsdbCtx);
		res = SceLsdb_87EE0526(lsdbCtx, &m_titleId, &row);
		if (res == 0) {
			if (SceLsdb_8BC2DF72(&row) == 3)
				srcDir = "ur0:temp/livearea/sce_sys/retail/livearea/contents";
			else
				srcDir = "ur0:temp/livearea/sce_sys/livearea/contents";
		}
		SceLsdb_56D8DCF0(lsdbCtx);
		if (res != 0)
			return res;
	}

	res = SceLiveAreaUtil_B0571F88(srcDir.c_str(), m_titleId.c_str(), &utc);
	if (res != 0) {
		char lsdbFlag;
		SceLsdb_105428BF(&lsdbFlag, &m_titleId, &utc);
		paf::Plugin *topmenu = paf::Plugin::Find("topmenu_plugin");
		if (topmenu) {
			void *iface = topmenu->GetInterface(1);
			if (iface) {
				const char *args[3] = { m_titleId.c_str(), NULL, NULL };
				typedef void (*NotifyFn)(const char **);
				(*(NotifyFn *)((char *)iface + 180))(args);
			}
		}
	}

	clearDirectoryContents("ur0:temp/livearea");

	if (res == 0 && m_postNotice != 0) {
		struct NoticeParam {
			paf::string tag;
			paf::string key;
			int f24;
			int f28;
			int type;
			int f36;
			int f40;
			SceUInt8 f44;
			SceUInt8 f45;
			paf::string icon;
			int f60;
			int f64;
			paf::string extra[9];
			paf::string s1;
			paf::string s2;
			int f180;
			paf::string titleId;
			paf::string s3;
			paf::string s4;
			int f220[4];
		} notice;
		notice.tag = "LAUPDATE";
		notice.key = m_titleId;
		notice.f24 = 0;
		notice.f28 = 0;
		notice.type = 2304;
		notice.f36 = 0;
		notice.f40 = 2;
		notice.f44 = 1;
		notice.f45 = 1;
		notice.f60 = 0;
		notice.f64 = 0;
		notice.f180 = 0;
		notice.titleId = m_titleId;
		sce_paf_memset(notice.f220, 0, sizeof(notice.f220));

		const char *curApp = SceActivityDb_E6C23731();
		if (curApp && sce_paf_strcmp(m_titleId.c_str(), curApp) == 0)
			notice.icon = paf::string("vs0:app/") + curApp + "/sce_sys/icon0.png";

		res = SceLsdb_315B9FD6(&notice, 1);
	}
	return res;
}

/* ------------------------------------------------------------------ */
/* LiveAreaPatchCheckTask (0x811B5474 - 0x811B577A)                   */
/* ------------------------------------------------------------------ */

class LiveAreaPatchCheckTask : public LiveAreaUpdateTaskBase {
public:
	LiveAreaPatchCheckTask(const char *titleId, int appType, CompleteCb cb, void *userdata);
	virtual ~LiveAreaPatchCheckTask() {}
	virtual int Run();

private:
	int m_appType;						/* +28 */
};

/* 0x811B5474 */
LiveAreaPatchCheckTask::LiveAreaPatchCheckTask(const char *titleId, int appType,
	CompleteCb cb, void *userdata)
	: LiveAreaUpdateTaskBase(titleId, cb, userdata), m_appType(appType)
{}

/* 0x811B54E4 */
int LiveAreaPatchCheckTask::Run()
{
	char lsdbCtx[4];
	if (SceLsdb_95EF7C51(lsdbCtx, &m_titleId, 0) != 0)
		return 0;

	struct PatchParam {
		void *(*allocFn)(SceSize);
		void (*freeFn)(void *);
		int mode;
		char titleId[16];
		char appVer[8];
		char contentId[48];
		int sysVer;
		int pspSysVer;
		char gameDataId[20];
	} param;
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
		param.pspSysVer = sceCompatGetPspSystemSoftwareVersion(1);

	paf::common::SharedPtr<int> row(new int[3]);
	SceLsdb_D322A870(lsdbCtx);
	int res = SceLsdb_87EE0526(lsdbCtx, &m_titleId, &row);
	if (res == 0) {
		const char *appVer = sceLsdbGetAppVer(&row);
		const char *cid = SceLsdb_8FDA59EA(&row);
		const char *gdid = sceLsdbGetGameDataId(&row);
		if (appVer)
			sce_paf_strncpy(param.appVer, appVer, 5);
		if (cid)
			sce_paf_strncpy(param.contentId, cid, 47);
		if (gdid)
			sce_paf_strncpy(param.gameDataId, gdid, 12);
	}
	SceLsdb_56D8DCF0(lsdbCtx);

	if (res == 0) {
		SceUInt32 swInfo[10];
		sce_paf_memset(swInfo, 0, sizeof(swInfo));
		swInfo[0] = 40;
		res = sceKernelGetSystemSwVersion(swInfo);
		if (res == 0)
			param.sysVer = (int)swInfo[8];
	}
	if (res == 0) {
		SceUInt32 statusBuf[4];
		SceUInt8 outBuf[412];
		SceUInt32 ctx[3];
		sce_paf_memset(statusBuf, 0, sizeof(statusBuf));
		sce_paf_memset(outBuf, 0, sizeof(outBuf));
		sce_paf_memset(ctx, 0, sizeof(ctx));
		statusBuf[0] = 16;

		res = liveareaPatchCheckInit(ctx, 0, &param);
		if (res == 0) {
			res = liveareaPatchCheckRun(ctx, statusBuf, outBuf);
			liveareaPatchCheckTerm(ctx);
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

struct ActivityUpdateResult {
	paf::string sender;					/* +0 */
	int kind;						/* +12 */
	SceInt64 statusUid;					/* +16 */
	paf::string targetTitleId;				/* +24 */
	SceUInt8 hasNew;					/* +36 */
	SceUInt8 hasComment;					/* +37 */
	int countA;						/* +40 */
	int countB;						/* +44 */
};

class LiveAreaActivityUpdateTask : public LiveAreaUpdateTaskBase {
public:
	typedef void (*ActivityCb)(int result, SceInt64 *ts, void *userdata);

	LiveAreaActivityUpdateTask(const char *titleId, int forceFlag, ActivityCb cb, void *userdata);
	virtual ~LiveAreaActivityUpdateTask() {}
	virtual int Run();
	virtual void OnFinish(int result);
	virtual void Cancel();

private:
	int FetchActivity(ActivityUpdateResult *outRes);
	static void OnActivityCb(int result, int a2, int a3, SceInt64 *ts,
		const ActivityUpdateResult *info, void *userdata);

	int m_actCtx;						/* +28 */
	int m_forceFlag;					/* +32 */
	ActivityCb m_actCb;					/* +36 */
	SceInt64 m_lastTs;					/* +40 */
	paf::thread::Semaphore m_sem;				/* +48 */
};

struct ActivityCbContext {
	LiveAreaActivityUpdateTask *task;
	ActivityUpdateResult *outRes;
	int result;
};

/* 0x811B577A */
LiveAreaActivityUpdateTask::LiveAreaActivityUpdateTask(const char *titleId, int forceFlag,
	ActivityCb cb, void *userdata)
	: LiveAreaUpdateTaskBase(titleId, NULL, userdata),
	  m_forceFlag(forceFlag), m_actCb(cb), m_lastTs(0),
	  m_sem("SceLiveAreaUpdateActivity", 0, 2, NULL)
{
	SceActivityDb_B83F0A77(&m_actCtx);
}

/* 0x811B5816 */
int LiveAreaActivityUpdateTask::FetchActivity(ActivityUpdateResult *outRes)
{
	ActivityCbContext cbCtx;
	cbCtx.task = this;
	cbCtx.outRes = outRes;
	cbCtx.result = 0;

	struct ActReq {
		int mode;
		paf::string s1;
		paf::string titleId;
		int f28;
		int f32;
		int f36;
		int forceFlag;
		void (*cb)(int, int, int, SceInt64 *, const ActivityUpdateResult *, void *);
		void *cbArg;
		SceUInt8 f52;
	} req;
	req.mode = (sce_paf_strcmp(m_titleId.c_str(), "NPXS10006") == 0) ? 1 : 0;
	req.titleId = m_titleId;
	req.f28 = 0;
	req.f32 = 0;
	req.f36 = 0;
	req.forceFlag = (m_forceFlag != 0) ? 1 : 0;
	req.cb = OnActivityCb;
	req.cbArg = &cbCtx;
	req.f52 = 0;

	int r = SceActivityDb_41E31BEF(&m_actCtx, &req);
	if (r == 0) {
		m_sem.Acquire();
		r = cbCtx.result;
	}
	return r;
}

/* 0x811B58F6 */
static int postFriendsActivityNotice(const ActivityUpdateResult *info)
{
	struct NoticeParam {
		paf::string tag;
		paf::string key;
		int f24;
		int f28;
		int type;
		int f36;
		int f40;
		SceUInt8 f44;
		SceUInt8 f45;
		paf::string icon;
		int f60;
		int f64;
		paf::string extra[9];
		paf::string s1;
		paf::string s2;
		int f180;
		paf::string titleId;
		paf::string paramStr;
		paf::string s4;
		int f220[4];
	} notice;

	notice.f24 = 0;
	notice.f28 = 0;
	notice.type = 256;
	notice.f36 = 0;
	notice.f40 = 0;
	notice.f44 = 0;
	notice.f45 = 1;
	notice.f60 = 0;
	notice.f64 = 0;
	notice.f180 = 0;
	sce_paf_memset(notice.f220, 0, sizeof(notice.f220));

	char numBuf[64];
	if (info->countA != 0) {
		if (info->countB != 0) {
			notice.type = 2311;
			sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", (SceInt64)info->countA);
			notice.extra[0] = numBuf;
			sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", (SceInt64)info->countB);
			notice.extra[1] = numBuf;
		} else {
			notice.type = 2309;
			sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", (SceInt64)info->countA);
			notice.extra[0] = numBuf;
			if (info->countA == 1 && info->statusUid != 0) {
				sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", info->statusUid);
				notice.paramStr = paf::string("status_uid=") + numBuf;
			}
		}
	} else if (info->countB != 0) {
		notice.type = 2310;
		sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", (SceInt64)info->countB);
		notice.extra[0] = numBuf;
		if (info->countB == 1 && info->statusUid != 0) {
			sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", info->statusUid);
			notice.paramStr = paf::string("status_uid=") + numBuf;
		}
	} else {
		return 0;
	}

	notice.tag = "LAUPDATE";
	notice.key = "NPXS10006";
	notice.titleId = "NPXS10006";
	notice.f44 = 1;
	notice.f40 = 8;
	return (SceLsdb_315B9FD6(&notice, 1) == 0) ? 1 : 0;
}

/* 0x811B5F40 */
static int postTitleActivityNotice(const ActivityUpdateResult *info, const paf::string *fallbackTitleId)
{
	int type;
	if (info->kind == 2) {
		if (info->hasNew)
			type = info->hasComment ? 2308 : 2306;
		else if (info->hasComment)
			type = 2307;
		else
			return 0;
	} else if (info->kind == 0 && info->hasNew) {
		type = 2305;
	} else {
		return 0;
	}

	char lsdbCtx[4];
	SceLsdb_D322A870(lsdbCtx);
	int exists = SceLsdb_76EC65E3(lsdbCtx, (paf::string *)&info->targetTitleId);
	SceLsdb_56D8DCF0(lsdbCtx);

	const paf::string *tid = exists ? &info->targetTitleId : fallbackTitleId;

	struct NoticeParam {
		paf::string tag;
		paf::string key;
		int f24;
		int f28;
		SceInt64 type;
		int f40;
		SceUInt8 f44;
		SceUInt8 f45;
		paf::string icon;
		int f60;
		int f64;
		paf::string extra[9];
		paf::string s1;
		paf::string s2;
		int f180;
		paf::string titleId;
		paf::string paramStr;
		paf::string s4;
		int f220[4];
	} notice;

	notice.tag = "LAUPDATE";
	notice.key = *tid;
	notice.f24 = 0;
	notice.f28 = 0;
	notice.type = type;
	notice.f40 = 8;
	notice.f44 = 1;
	notice.f45 = 1;
	notice.f60 = 0;
	notice.f64 = 0;
	notice.f180 = 0;
	notice.titleId = *tid;
	sce_paf_memset(notice.f220, 0, sizeof(notice.f220));

	if (info->statusUid != 0) {
		char numBuf[64];
		sce_paf_snprintf(numBuf, sizeof(numBuf), "%lld", info->statusUid);
		notice.paramStr = paf::string("status_uid=") + numBuf;
	}

	if (type == 2305) {
		const char *s = info->sender.c_str();
		const char *at = sce_paf_strchr(s, '@');
		if (at)
			notice.extra[0] = paf::string(s, (SceSize)(at - s));
		else
			notice.extra[0] = info->sender;
	}

	return SceLsdb_315B9FD6(&notice, 1);
}

/* 0x811B63F4 */
int LiveAreaActivityUpdateTask::Run()
{
	ActivityUpdateResult info;
	info.kind = 3;
	info.statusUid = 0;
	info.hasNew = 0;
	info.hasComment = 0;
	info.countA = 0;
	info.countB = 0;

	int r = FetchActivity(&info);
	if (r == 0) {
		if (sce_paf_strcmp(m_titleId.c_str(), "NPXS10006") == 0 &&
		    postFriendsActivityNotice(&info) != 0) {
			return 0;
		}
		r = postTitleActivityNotice(&info, &m_titleId);
	}
	return r;
}

/* 0x811B64B6 */
void LiveAreaActivityUpdateTask::OnFinish(int result)
{
	if (m_actCb)
		m_actCb(result, &m_lastTs, m_userdata);
}

/* 0x811B64CE */
void LiveAreaActivityUpdateTask::Cancel()
{
	m_canceled = 1;
	if (SceActivityDb_5B44F019(&m_actCtx) == 0)
		m_sem.Release();
}

/* 0x811B64EC */
void LiveAreaActivityUpdateTask::OnActivityCb(int result, int, int, SceInt64 *ts,
	const ActivityUpdateResult *info, void *userdata)
{
	ActivityCbContext *ctx = (ActivityCbContext *)userdata;
	if (ctx->task->m_canceled == 0) {
		ctx->result = result;
		ctx->task->m_lastTs = *ts;
		*ctx->outRes = *info;
		ctx->task->m_sem.Release();
	}
}

/* ------------------------------------------------------------------ */
/* LiveAreaCleanTask (0x811B6554 - 0x811B65B4)                        */
/* ------------------------------------------------------------------ */

class LiveAreaCleanTask : public LiveAreaUpdateTaskBase {
public:
	LiveAreaCleanTask(const char *titleId, CompleteCb cb, void *userdata);
	virtual ~LiveAreaCleanTask() {}
	virtual int Run();
};

/* 0x811B6554 */
LiveAreaCleanTask::LiveAreaCleanTask(const char *titleId, CompleteCb cb, void *userdata)
	: LiveAreaUpdateTaskBase(titleId, cb, userdata)
{}

/* 0x811B65AC */
int LiveAreaCleanTask::Run()
{
	return liveareaCleanCacheEntry();
}
