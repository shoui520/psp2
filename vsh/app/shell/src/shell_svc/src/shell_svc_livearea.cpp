/*
 * SceShell shell_svc: LiveAreaUtil IPMI async handlers & job execution
 * 3.65-CEX: 0x811B84F2 - 0x811B918C (sti 0x811B90AC)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/list>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <ipmi/session.h>
#include <job/job.h>
#include <plugin.h>
#include <thread/thread.h>

#include "../../livespace_db/lsdb_keys.h"

extern "C" {
extern void SceLsdb_A6D8772C(void *ctx);
extern int SceLsdb_E90B81CD(void *ctx);
extern void SceLsdb_E2101B03(void *ctx);
extern int SceLsdb_D239031A(void *ctx, const paf::string *titleId, paf::common::SharedPtr<int> *row);
extern int sceLsdbGetSystemVersion(paf::common::SharedPtr<int> *row);
extern int sceLsdbGetType(paf::common::SharedPtr<int> *row);
extern int SceLsdb_BE72A404(const char *titleId);
extern int *SceLsdb_D3EF2C98(void *mem);
extern int SceLsdb_60B535FD(int sysVer, int a2);
extern void SceLsdb_88FDC55C(paf::string *out, int sysVer);
extern int SceLsdb_1673AD48(void);
extern int SceLsdb_5BDC6351(void **outNode, const char *xml, SceSize len, int *a4,
	const char *schema, int a6, SceUInt8 allowAny, int type);
extern int SceLsdb_BA1D0DB8(void *ctx, paf::string *titleId, paf::string *frameId,
	paf::string *outContent, SceUID pid);
}

extern int shellSvcLiveAreaEnqueueUpdate(IPMI::Session *session, SceUInt32 reqId,
	const char *path, const char *titleId, const SceUInt64 *ts,
	SceUInt8 allowAny, SceUInt8 flag);			/* 0x811B7B54 */
extern int shellSvcLiveAreaSetContentSingle(IPMI::Session *session, SceUInt32 reqId,
	int a3, const void *xmlData, SceUInt32 xmlSize,
	const char *path, int flags, const char *titleId,
	SceUInt8 allowAny, SceUInt8 flag);			/* 0x811B7E32 */
extern int shellSvcLiveAreaSetContentMulti(IPMI::Session *session, SceUInt32 reqId,
	int a3, const char **xmlList, SceUInt32 count,
	const char *path, int flags, const char *titleId, SceUInt8 allowAny); /* 0x811B7F1C */
extern int shellSvcLiveAreaEnqueueGetTs(IPMI::Session *session, SceUInt32 reqId,
	const char *frameId, SceUInt8 flag);			/* 0x811B804C */
extern int shellSvcLiveAreaEnqueueGetContent(IPMI::Session *session, SceUInt32 reqId,
	const char *frameId, SceUInt32 maxSize, SceUInt8 flag);	/* 0x811B8250 */

extern void shellSvcResolveClientTitleId(paf::string *out, SceUID appId, SceUInt8 flag, int mode); /* 0x811C1300 */
extern int shellSvcLiveAreaUpdateFromPath(SceUID pid, const char *path, paf::string *titleId,
	SceUInt64 *ts, SceUInt8 allowAny, paf::string *outDir, int *outFlag); /* 0x811B9710 */
extern int shellSvcLiveAreaSetFromPath(SceUID pid, const char *path, paf::common::SharedPtr<int> *parsed,
	int flags, paf::string *titleId, SceUInt8 allowAny, paf::string *outDir, int *outFlag); /* 0x811B9836 */

class LiveAreaUtilJobBase : public paf::job::JobItem {
public:
	LiveAreaUtilJobBase(IPMI::Session *session, SceUInt32 reqId,
		const char *path, const char *titleId, SceUInt8 flag);
	virtual ~LiveAreaUtilJobBase() {}
	virtual int Run() { return 0; }
	virtual void Finish(int) {}

	IPMI::Session *m_session;				/* +28 */
	SceUInt32 m_reqId;					/* +32 */
	paf::string m_path;					/* +36 */
	paf::string m_titleId;					/* +48 */
	SceUInt8 m_flag;					/* +60 */
};

static paf::list<LiveAreaUtilJobBase *> s_liveareaUtilJobs;	/* 0x8156B650 */
static paf::thread::RMutex s_liveareaUtilMutex("SceLiveAreaUtilJobMutex", 0); /* 0x8156B660 */

/* 0x811B84F2 */
int shellSvcLiveAreaSendUpdateResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x110001, reqId, err, NULL, 0);
}

/* 0x811B8512 */
int shellSvcLiveAreaSendSetResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x110003, reqId, err, NULL, 0);
}

/* 0x811B8532 */
int shellSvcLiveAreaHandleSetMulti(IPMI::Session *session, SceUInt32 reqId, const IPMI::DataInfo *inBufs)
{
	const SceUInt32 *hdr = (const SceUInt32 *)inBufs[0].pData;
	SceUInt32 xmlCount = hdr[0];
	const char *xmlList[8];
	SceUInt32 idx = 2;
	for (SceUInt32 i = 0; i < xmlCount; ++i) {
		xmlList[i] = (const char *)inBufs[idx].pData;
		idx++;
	}
	return shellSvcLiveAreaSetContentMulti(session, reqId, (int)inBufs[1].pData,
		xmlList, hdr[1], (const char *)inBufs[idx].pData, (int)hdr[2],
		(const char *)inBufs[idx + 1].pData, *((const SceUInt8 *)hdr + 12));
}

/* 0x811B85F2 */
int shellSvcLiveAreaSendSetMultiResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x110005, reqId, err, NULL, 0);
}

/* 0x811B8614 */
int shellSvcLiveAreaSendGetTsResp(IPMI::Session *session, SceUInt32 reqId, int err, SceUInt64 *ts)
{
	IPMI::DataInfo out[1];
	out[0].pData = ts;
	out[0].dataSize = 8;
	return session->respondToAsyncMethodRequest(0x110007, reqId, err, out, 1);
}

/* 0x811B8658 */
int shellSvcLiveAreaSendGetContentResp(IPMI::Session *session, SceUInt32 reqId, int err,
	const void *buf, SceUInt32 *size)
{
	IPMI::DataInfo out[2];
	out[0].pData = buf;
	out[0].dataSize = *size;
	out[1].pData = size;
	out[1].dataSize = 4;
	return session->respondToAsyncMethodRequest(0x110009, reqId, err, out, 2);
}

/* 0x811B86A4 */
int shellSvcLiveAreaDispatchAsync(IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 methodId, const IPMI::DataInfo *inBufs, SceUInt32)
{
	switch (methodId) {
	case 0x110000: {
		const SceUInt8 *flags = (const SceUInt8 *)inBufs[0].pData;
		return shellSvcLiveAreaEnqueueUpdate(session, reqId,
			(const char *)inBufs[1].pData,
			(const char *)inBufs[2].pData,
			(const SceUInt64 *)inBufs[3].pData,
			flags[0], flags[1]);
	}
	case 0x110002: {
		const SceUInt32 *hdr = (const SceUInt32 *)inBufs[0].pData;
		const SceUInt8 *b = (const SceUInt8 *)hdr;
		return shellSvcLiveAreaSetContentSingle(session, reqId,
			(int)inBufs[1].pData, inBufs[2].pData, hdr[0],
			(const char *)inBufs[3].pData, (int)hdr[1],
			(const char *)inBufs[4].pData, b[8], b[9]);
	}
	case 0x110004:
		return shellSvcLiveAreaHandleSetMulti(session, reqId, inBufs);
	case 0x110006:
		return shellSvcLiveAreaEnqueueGetTs(session, reqId,
			(const char *)inBufs[1].pData, *(const SceUInt8 *)inBufs[0].pData);
	case 0x110008: {
		const SceUInt32 *hdr = (const SceUInt32 *)inBufs[0].pData;
		return shellSvcLiveAreaEnqueueGetContent(session, reqId,
			(const char *)inBufs[1].pData, hdr[0], *((const SceUInt8 *)hdr + 4));
	}
	default:
		return (int)0x800205C1;
	}
}

/* 0x811B8740 (wrapper 0x811B8810) */
LiveAreaUtilJobBase::LiveAreaUtilJobBase(IPMI::Session *session, SceUInt32 reqId,
	const char *path, const char *titleId, SceUInt8 flag)
	: paf::job::JobItem("SceLiveAreaUtilJob", NULL),
	  m_session(session), m_reqId(reqId),
	  m_path(path ? path : ""), m_titleId(titleId ? titleId : ""), m_flag(flag)
{
	s_liveareaUtilMutex.Lock();
	s_liveareaUtilJobs.push_back(this);
	s_liveareaUtilMutex.Unlock();
}

/* 0x811B8810 */
void *shellSvcLiveAreaJobBaseCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	const char *path, const char *titleId, SceUInt8 flag)
{
	return new (mem) LiveAreaUtilJobBase(session, reqId, path, titleId, flag);
}

/* 0x811B8826 */
void shellSvcLiveAreaCancelSession(IPMI::Session *session)
{
	s_liveareaUtilMutex.Lock();
	for (paf::list<LiveAreaUtilJobBase *>::iterator it = s_liveareaUtilJobs.begin();
	     it != s_liveareaUtilJobs.end(); ++it) {
		if ((*it)->m_session == session) {
			(*it)->Cancel();
			break;
		}
	}
	s_liveareaUtilMutex.Unlock();
}

/* 0x811B886C */
int shellSvcLiveAreaUpdateJobRun(void *rawJob)
{
	struct UpdateJob : public LiveAreaUtilJobBase {
		SceUInt64 m_ts;
		SceUInt8 m_allowAny;
	} *job = (UpdateJob *)rawJob;

	int res = 0;
	int outFlag = 0;
	paf::string outDir;

	SceUID appId = job->m_session->getAppId();
	if (job->m_titleId.size() == 0) {
		paf::string resolved;
		shellSvcResolveClientTitleId(&resolved, appId, job->m_flag, 0);
		job->m_titleId = resolved;
		if (job->m_titleId.size() == 0)
			res = (int)0x801040FF;
	}
	if (res == 0) {
		SceUID pid = job->m_session->getProcessId();
		res = shellSvcLiveAreaUpdateFromPath(pid, job->m_path.c_str(), &job->m_titleId,
			&job->m_ts, job->m_allowAny, &outDir, &outFlag);
	}

	s_liveareaUtilMutex.Lock();
	if (!job->IsCanceled())
		shellSvcLiveAreaSendUpdateResp(job->m_session, job->m_reqId, res);
	s_liveareaUtilMutex.Unlock();

	if (res == 0) {
		paf::Plugin *topmenu = paf::Plugin::Find("topmenu_plugin");
		if (topmenu) {
			void *iface = topmenu->GetInterface(1);
			if (iface) {
				const void *args[3] = { job->m_titleId.c_str(), outDir.c_str(), (const void *)outFlag };
				typedef void (*NotifyFn)(const void **);
				(*(NotifyFn *)((char *)iface + 180))(args);
			}
		}
	}
	return res;
}

/* 0x811B898E */
static int liveareaGetAppSysVerAndType(const paf::string *titleId, int *outType)
{
	int sysVer = 0;
	*outType = 0;
	if (sce_paf_strcmp(titleId->c_str(), "NPXS10999") == 0 ||
	    sce_paf_strcmp(titleId->c_str(), "NPXS10998") == 0) {
		return 0;
	}

	paf::common::SharedPtr<int> row(new int[3]);
	char lsdbCtx[4];
	SceLsdb_A6D8772C(lsdbCtx);
	if (SceLsdb_E90B81CD(lsdbCtx) == 0) {
		if (SceLsdb_D239031A(lsdbCtx, titleId, &row) == 0) {
			sysVer = sceLsdbGetSystemVersion(&row);
			if (sceLsdbGetType(&row) == 3)
				*outType = SceLsdb_BE72A404(titleId->c_str());
		}
	}
	SceLsdb_E2101B03(lsdbCtx);
	return sysVer;
}

/* 0x811B8B4A */
int shellSvcLiveAreaSetContentJobRun(void *rawJob)
{
	struct SetJob : public LiveAreaUtilJobBase {
		int m_flags;
		SceUInt8 m_allowAny;
		SceUInt8 m_isMulti;
		paf::vector<paf::string> m_xmls;
	} *job = (SetJob *)rawJob;

	void *mem = operator new(56);
	int *parsed = mem ? SceLsdb_D3EF2C98(mem) : NULL;
	paf::common::SharedPtr<int> spParsed(parsed);

	paf::string outDir;
	int outFlag = 0;
	int res = 0;

	SceUID appId = job->m_session->getAppId();
	if (job->m_titleId.size() == 0) {
		paf::string resolved;
		shellSvcResolveClientTitleId(&resolved, appId, job->m_flag, 0);
		job->m_titleId = resolved;
		if (job->m_titleId.size() == 0)
			res = (int)0x801040FF;
	}

	if (res == 0) {
		int typeVal = 0;
		int sysVer = liveareaGetAppSysVerAndType(&job->m_titleId, &typeVal);
		int verInfo = SceLsdb_60B535FD(sysVer, -1);
		paf::string schema;
		SceLsdb_88FDC55C(&schema, sysVer);
		int schemaArg = SceLsdb_1673AD48();

		while (job->m_xmls.size() != 0) {
			paf::string &xml = job->m_xmls[job->m_xmls.size() - 1];
			void *node = NULL;
			if (SceLsdb_5BDC6351(&node, xml.c_str(), xml.size(), &verInfo,
			                     schema.c_str(), schemaArg, job->m_allowAny, typeVal) == 0) {
				res = (int)0x80104004;
				break;
			}
			((paf::vector<void *> *)((char *)parsed + 36))->push_back(node);
			job->m_xmls.pop_back();
		}

		if (res == 0) {
			SceUID pid = job->m_session->getProcessId();
			res = shellSvcLiveAreaSetFromPath(pid, job->m_path.c_str(), &spParsed,
				job->m_flags, &job->m_titleId, job->m_allowAny, &outDir, &outFlag);
		}
	}

	s_liveareaUtilMutex.Lock();
	if (!job->IsCanceled()) {
		if (job->m_isMulti != 0)
			shellSvcLiveAreaSendSetMultiResp(job->m_session, job->m_reqId, res);
		else
			shellSvcLiveAreaSendSetResp(job->m_session, job->m_reqId, res);
	}
	s_liveareaUtilMutex.Unlock();

	if (res == 0) {
		paf::Plugin *topmenu = paf::Plugin::Find("topmenu_plugin");
		if (topmenu) {
			void *iface = topmenu->GetInterface(1);
			if (iface) {
				const void *args[3] = { job->m_titleId.c_str(), outDir.c_str(), (const void *)outFlag };
				typedef void (*NotifyFn)(const void **);
				(*(NotifyFn *)((char *)iface + 180))(args);
			}
		}
	}
	return res;
}

/* 0x811B8E58 */
int shellSvcLiveAreaGetTsJobRun(void *rawJob)
{
	struct GetTsJob : public LiveAreaUtilJobBase {
		paf::string m_frameId;
	} *job = (GetTsJob *)rawJob;

	SceUInt64 ts = 0;
	int res = 0;
	SceUID appId = job->m_session->getAppId();
	if (job->m_titleId.size() == 0) {
		paf::string resolved;
		shellSvcResolveClientTitleId(&resolved, appId, job->m_flag, 1);
		job->m_titleId = resolved;
		if (job->m_titleId.size() == 0)
			res = (int)0x801040FF;
	}

	if (res == 0) {
		paf::Plugin *la = paf::Plugin::Find("livearea_plugin");
		void *iface = la ? la->GetInterface(1) : NULL;
		if (!iface) {
			res = (int)0x801040FF;
		} else {
			typedef int (*GetTsFn)(const char *, const char *, SceUInt64 *);
			GetTsFn fn = *(GetTsFn *)((char *)iface + 20);
			int tries = 625;
			do {
				const char *fid = job->m_frameId.size() ? job->m_frameId.c_str() : NULL;
				res = fn(job->m_titleId.c_str(), fid, &ts);
				if (res != (int)0x80000002)
					break;
				paf::thread::Sleep(16);
			} while (--tries > 0);
			if (res != 0)
				res = (int)0x8010400A;
		}
	}

	s_liveareaUtilMutex.Lock();
	if (!job->IsCanceled())
		res = shellSvcLiveAreaSendGetTsResp(job->m_session, job->m_reqId, res, &ts);
	s_liveareaUtilMutex.Unlock();
	return res;
}

/* 0x811B8F74 */
int shellSvcLiveAreaGetContentJobRun(void *rawJob)
{
	struct GetContentJob : public LiveAreaUtilJobBase {
		paf::string m_frameId;
		SceUInt32 m_maxSize;
	} *job = (GetContentJob *)rawJob;

	paf::string content;
	int res = 0;
	SceUID appId = job->m_session->getAppId();
	if (job->m_titleId.size() == 0) {
		paf::string resolved;
		shellSvcResolveClientTitleId(&resolved, appId, job->m_flag, 0);
		job->m_titleId = resolved;
		if (job->m_titleId.size() == 0)
			res = (int)0x801040FF;
	}

	if (res == 0) {
		char lsdbCtx[4];
		SceUID pid = job->m_session->getProcessId();
		int r = SceLsdb_BA1D0DB8(lsdbCtx, &job->m_titleId, &job->m_frameId, &content, pid);
		res = r;
		if (r != (int)0x8010400A && r != 0)
			res = (int)0x801040FF;
	}

	s_liveareaUtilMutex.Lock();
	if (!job->IsCanceled()) {
		if (job->m_maxSize <= content.size())
			content.resize(job->m_maxSize - 1);
		SceUInt32 outSize = content.size() + 1;
		res = shellSvcLiveAreaSendGetContentResp(job->m_session, job->m_reqId, res,
			content.c_str(), &outSize);
	}
	s_liveareaUtilMutex.Unlock();
	return res;
}
