/*
 * SceShell livearea_plugin: ContentParserJob
 * 3.65-CEX: 0x811B6624 - 0x811B71E8 (sti 0x811B7176)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/map>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <job/job.h>
#include <plugin.h>
#include <thread/thread.h>

#include "../../livespace_db/lsdb_keys.h"

extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */
extern void *g_liveareaMgr;					/* 0x815670A8 */
extern paf::thread::RMutex g_liveareaDialogMutex;		/* 0x81467AAC */

extern "C" {
extern void *SceLsdb_800809E3(void *mem, void *a2, int a3, int a4);
extern void SceLsdb_29B275BA(paf::string *titleId, void *a2);
extern void *SceLsdb_4B153968(void *parser);
extern int SceLsdb_60B535FD(void *a1, int a2);
extern void SceLsdb_88FDC55C(paf::string *out, void *a2);
extern int SceLsdb_1673AD48(void);
extern void SceLsdb_43495B68(void *parser, int *a2, const char *s, int a4);
extern int SceLsdb_BE72A404(const char *titleId);
extern SceUInt8 SceLsdb_69B32F6C(void *a1);
extern int SceLsdb_632ACD0F(void *parser);
extern void SceCommonGuiDialog_696DB9C9(void *param);
extern int SceCommonGuiDialog_CCFCD9A6(void *param, int a2);
extern void SceCommonGuiDialog_BC311DC0(int dlgId);
}

extern const char *liveareaPageGetTitleId(void *page);		/* 0x8119B990 */
extern int liveareaCacheGetTemplate(paf::common::SharedPtr<int> *out, paf::string *titleId, SceUInt8 isDefault); /* 0x8119A99C */
extern int liveareaMgrDownloadContent(paf::common::SharedPtr<int> *row, int a2, int a3); /* 0x8119B222 */
extern void liveareaPageInnerOnParseComplete(void *inner, void *parsed, int reqId, paf::string *dir); /* 0x811A9D0C */
extern void liveareaPageInnerOnParseFail(void *inner);		/* 0x811A9CE4 */
extern void liveareaPageInnerLoadDefaultXml(void *inner);	/* 0x811AAB9E */

struct ContentParserDlgCb {
	virtual ~ContentParserDlgCb() {}
	virtual void OnClose() {}
	void *m_inner;
};

class LiveAreaContentParserJob : public paf::job::JobItem {
public:
	LiveAreaContentParserJob(void **inner, void *lsdbCtx, const char *dir,
		SceUInt8 useLsdb, SceUInt8 isDefault, int reqId, SceUInt8 allowDialog);
	virtual ~LiveAreaContentParserJob();
	virtual int Run();
	virtual void Finish(int result);

	void CancelAndDetach();

private:
	paf::string m_titleId;					/* +28 */
	paf::string m_dir;					/* +40 */
	SceUInt8 m_flag52;					/* +52 */
	SceUInt8 m_useLsdb;					/* +53 */
	SceUInt8 m_allowDialog;					/* +54 */
	SceUInt8 m_isDefault;					/* +55 */
	int m_reqId;						/* +56 */
	paf::common::SharedPtr<int> m_parsed;			/* +60 */
	void *m_lsdbParser;					/* +68 */
	void **m_inner;						/* +72 */
	int m_pageVal24;					/* +76 */
	paf::string m_pageStr28;				/* +80 */
	void *m_innerField14;					/* +92 */
	int m_innerField1;					/* +96 */
};

/* 0x811B6624 */
LiveAreaContentParserJob::LiveAreaContentParserJob(void **inner, void *lsdbCtx, const char *dir,
	SceUInt8 useLsdb, SceUInt8 isDefault, int reqId, SceUInt8 allowDialog)
	: paf::job::JobItem("ContentParserJob", NULL),
	  m_titleId(liveareaPageGetTitleId(*inner) ? liveareaPageGetTitleId(*inner) : ""),
	  m_dir(dir ? dir : ""),
	  m_flag52(inner[15] != NULL),
	  m_useLsdb(useLsdb),
	  m_allowDialog(allowDialog),
	  m_isDefault(isDefault),
	  m_reqId(reqId),
	  m_lsdbParser(NULL),
	  m_inner(inner),
	  m_pageVal24(*(int *)((char *)(*inner) + 24)),
	  m_pageStr28(*(const char **)((char *)(*inner) + 28) ? *(const char **)((char *)(*inner) + 28) : ""),
	  m_innerField14(inner[14]),
	  m_innerField1((int)inner[1])
{
	if (sce_paf_strncmp(m_titleId.c_str(), "NPXS10998", sce_paf_strlen("NPXS10998")) == 0 ||
	    sce_paf_strncmp(m_titleId.c_str(), "NPXS10999", sce_paf_strlen("NPXS10999")) == 0) {
		m_useLsdb = 1;
	}
	if (isDefault != 0) {
		m_useLsdb = 0;
	} else if (m_useLsdb != 0) {
		void *mem = operator new(60);
		m_lsdbParser = mem ? SceLsdb_800809E3(mem, lsdbCtx, 1, 1) : NULL;
		SceLsdb_29B275BA(&m_titleId, lsdbCtx);
	}
}

/* 0x811B67AA */
int liveareaEnqueueContentParserJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	void **inner, void *lsdbCtx, const char *dir,
	SceUInt8 useLsdb, SceUInt8 isDefault, int reqId, SceUInt8 allowDialog)
{
	LiveAreaContentParserJob *job = new LiveAreaContentParserJob(
		inner, lsdbCtx, dir, useLsdb, isDefault, reqId, allowDialog);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	*out = sp;

	typedef paf::job::JobQueue *(*GetQueueFn)(void);
	void *mgr = *(void **)((char *)g_liveareaMgr + 4);
	paf::job::JobQueue *q = (*(GetQueueFn *)((char *)mgr + 52))();
	return q->Enqueue(*out);
}

/* 0x811B687A (deleting dtor 0x811B6942) */
LiveAreaContentParserJob::~LiveAreaContentParserJob()
{
	if (m_lsdbParser) {
		void *p = SceLsdb_4B153968(m_lsdbParser);
		operator delete(p);
	}
}

/* 0x811B6952 */
int LiveAreaContentParserJob::Run()
{
	if (IsCanceled())
		return -1;

	if (m_useLsdb == 0) {
		int res = liveareaCacheGetTemplate(&m_parsed, &m_titleId, m_isDefault);
		if (res == (int)0x80101005 && m_allowDialog != 0) {
			int needWait = 0;
			paf::common::SharedPtr<int> row(new int[3]);
			typedef int (*QueryRowFn)(paf::common::SharedPtr<int> *, const char *);
			void *mgr4 = *(void **)((char *)g_liveareaMgr + 4);
			if ((*(QueryRowFn *)((char *)mgr4 + 276))(&row, m_titleId.c_str()) == 0)
				return (int)0x80108E00;

			typedef void (*CheckWaitFn)(paf::common::SharedPtr<int> *, int *);
			void *mgr8 = *(void **)((char *)g_liveareaMgr + 8);
			(*(CheckWaitFn *)((char *)mgr8 + 4))(&row, &needWait);
			if (needWait != 0) {
				g_liveareaDialogMutex.Lock();
				char dlgParam[192];
				SceCommonGuiDialog_696DB9C9(dlgParam);
				*(paf::Plugin **)dlgParam = g_liveareaPlugin;
				*(int *)(dlgParam + 48) = 40;
				const wchar_t *msg = g_liveareaPlugin->GetString(paf::IDParam(1663788851u));
				paf::wstring wmsg(msg ? msg : L"");
				int dlgId = SceCommonGuiDialog_CCFCD9A6(dlgParam, 1);
				typedef void (*SetBusyFn)(int);
				typedef void (*VoidFn)(void);
				(*(SetBusyFn *)((char *)mgr8 + 40))(1);
				(*(VoidFn *)((char *)mgr4 + 124))();
				g_liveareaDialogMutex.Unlock();

				int dlRes = liveareaMgrDownloadContent(&row, 0, 0);

				g_liveareaDialogMutex.Lock();
				SceCommonGuiDialog_BC311DC0(dlgId);
				(*(SetBusyFn *)((char *)mgr8 + 40))(0);
				(*(VoidFn *)((char *)mgr4 + 128))();
				g_liveareaDialogMutex.Unlock();

				if (dlRes != 0)
					return dlRes;
				res = liveareaCacheGetTemplate(&m_parsed, &m_titleId, m_isDefault);
			}
		}
		return res;
	}

	int val = SceLsdb_60B535FD(m_innerField14, -1);
	paf::string xmlPath;
	SceLsdb_88FDC55C(&xmlPath, m_innerField14);
	if (xmlPath.size() != 0) {
		int a4 = SceLsdb_1673AD48();
		SceLsdb_43495B68(m_lsdbParser, &val, xmlPath.c_str(), a4);
		if (m_innerField1 == 3)
			*(int *)((char *)m_lsdbParser + 40) = SceLsdb_BE72A404(m_titleId.c_str());
		if (m_flag52 != 0)
			*((SceUInt8 *)m_lsdbParser + 38) = 1;
		*((SceUInt8 *)m_lsdbParser + 39) = SceLsdb_69B32F6C(m_innerField14);
		return SceLsdb_632ACD0F(m_lsdbParser);
	}

	paf::string special("NPXS10015");
	if (special == m_titleId)
		return SceLsdb_632ACD0F(m_lsdbParser);
	return -5;
}

/* 0x811B6F18 */
void LiveAreaContentParserJob::Finish(int result)
{
	if (IsCanceled() || !m_inner)
		return;

	if (m_useLsdb == 0) {
		if (result == 0) {
			liveareaPageInnerOnParseComplete(m_inner, &m_parsed, m_reqId, &m_dir);
			return;
		}
		if (m_allowDialog != 0 && result == (int)0x8010111E) {
			char dlgParam[192];
			SceCommonGuiDialog_696DB9C9(dlgParam);
			*(paf::Plugin **)dlgParam = g_liveareaPlugin;
			*(int *)(dlgParam + 40) = -682994459;
			*(int *)(dlgParam + 48) = 40;
			const wchar_t *msg = g_liveareaPlugin->GetString(paf::IDParam((SceUInt32)-198752243));
			paf::wstring wmsg(msg ? msg : L"");
			ContentParserDlgCb *cb = new ContentParserDlgCb();
			if (cb)
				cb->m_inner = m_inner;
			int dlgId = SceCommonGuiDialog_CCFCD9A6(dlgParam, 1);
			void *mgr4 = *(void **)((char *)g_liveareaMgr + 4);
			void *mgr12 = *(void **)((char *)g_liveareaMgr + 12);
			typedef void (*RegDlgFn)(int);
			typedef void (*VoidFn)(void);
			(*(RegDlgFn *)((char *)mgr4 + 288))(dlgId);
			liveareaPageInnerOnParseFail(m_inner);
			(*(VoidFn *)((char *)mgr12 + 32))();
			return;
		}
		liveareaPageInnerLoadDefaultXml(m_inner);
		return;
	}

	if (result != 0) {
		if (result != -3) {
			liveareaPageInnerOnParseFail(m_inner);
			return;
		}
		liveareaPageInnerLoadDefaultXml(m_inner);
		return;
	}

	if (*((SceUInt8 *)m_lsdbParser + 14) != 0)
		m_dir = "vs0:data/internal/livearea/default/sce_sys/livearea/contents/";
	liveareaPageInnerOnParseComplete(m_inner, (char *)m_lsdbParser + 16, m_reqId, &m_dir);
}

/* 0x811B7168 */
void LiveAreaContentParserJob::CancelAndDetach()
{
	m_inner = NULL;
	Cancel();
}
