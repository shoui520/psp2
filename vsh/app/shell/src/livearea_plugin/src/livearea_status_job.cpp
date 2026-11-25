/*
 * livearea_status_job.cpp: livearea_plugin GetStatusInfoJob (activity status viewer launch;
 * 0x8119FB10-0x811A078C, sti 0x811A071C). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <job/job.h>
#include <common/ref_obj.h>
#include <common/shared_ptr.h>
#include <widget/core/widget.h>
#include "../../livespace_db/lsdb_keys.h"

struct LiveAreaMgr;
extern LiveAreaMgr *g_liveareaMgr;				/* 0x815670A8 */

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
long long sce_paf_strtoll(const char *nptr, char **endptr, int base);
void SceActivityDb_6C98B745(void *ctx);
int SceActivityDb_F65C0C21(void *ctx, void *req);
int SceActivityDb_74F96707(void *ctx, void *uids, void *outItems);
int SceActivityDb_7ED9033C(void *ctx, void *a2, SceUInt64 *outTime, void *a4, void *a5);
void SceActivityDb_6E198FFA(void *ctx);
void SceCommonGuiDialog_696DB9C9(void *param);
}

extern int liveareaCheckDialogActive(void);			/* 0x8135B002 */
extern void liveareaCloseDialogFast(void);			/* 0x81353E16 */
extern void liveareaStatusDlgCreateCtx(void *outA, void *outB, void *req); /* 0x8135B0BA */
extern void *liveareaStatusDlgCtor(void *mem, int a2, paf::ui::Widget *w, paf::common::RefObj **item,
				   int type, void *ctxA, void *tsSp, void *nullRef); /* 0x811A078C */
extern void liveareaStatusDlgSub31DA(void *dlg, int arg);	/* 0x813531DA */
extern void liveareaStatusDlgShow(void *dlg);			/* 0x811A0B5C */
extern void liveareaStatusDlgBind(void *dlg, void *ctxB, const char *title); /* 0x811A0E5A */

static bool s_statusJobActive;					/* 0x81567130 */
static int s_statusDlgId = -1;					/* 0x81544D90 */

struct LiveAreaStatusItemRaw {
	unsigned char data[168];
};

class GetStatusInfoJob : public paf::job::JobItem {
public:
	GetStatusInfoJob(const char *title, const char *param, void *waitWidget); /* 0x8119FB10 */
	virtual ~GetStatusInfoJob();				/* 0x8119FC54 / 0x8119FCB8 */
	virtual int Run();					/* 0x8119FD20 */
	virtual void Finish(int result);			/* 0x8119FF8E */

	static paf::common::SharedPtr<paf::job::JobItem> Enqueue(const char *title, const char *param, void *waitWidget); /* 0x8119FBB8 */

	paf::string m_title;					/* +0x1C */
	paf::string m_param;					/* +0x28 */
	void *m_waitWidget;					/* +0x34 */
	paf::vector<LiveAreaStatusItemRaw> m_items;		/* +0x38 */
	SceUInt64 m_lastReadTime;				/* +0x48 */
	SceInt64 m_statusUid;					/* +0x50 */
};

/* 0x8119FB10 */
GetStatusInfoJob::GetStatusInfoJob(const char *title, const char *param, void *waitWidget)
	: paf::job::JobItem("GetStatusInfoJob", NULL),
	  m_title(title),
	  m_param(param),
	  m_waitWidget(waitWidget),
	  m_items(),
	  m_lastReadTime(0),
	  m_statusUid(-1)
{
	s_statusJobActive = true;
}

/* 0x8119FBB8 */
paf::common::SharedPtr<paf::job::JobItem> GetStatusInfoJob::Enqueue(const char *title, const char *param, void *waitWidget)
{
	paf::job::JobItem *job = new GetStatusInfoJob(title, param, waitWidget);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	typedef paf::job::JobQueue *(*GetQueueFn)(void);
	void *topmenu = *((void **)g_liveareaMgr + 1);
	paf::job::JobQueue *q = (*(GetQueueFn *)((char *)topmenu + 52))();
	q->Enqueue(sp);
	return sp;
}

/* 0x8119FC54 / 0x8119FCB8 */
GetStatusInfoJob::~GetStatusInfoJob()
{
	s_statusJobActive = false;
}

/* 0x8119FD20 */
int GetStatusInfoJob::Run()
{
	unsigned char ctx[4];
	SceActivityDb_6C98B745(ctx);

	struct {
		int type;
		paf::string filter;
		paf::string title;
		int limit;
	} req;
	req.type = 0;
	req.title = m_title;
	req.limit = 0;

	int ret = SceActivityDb_F65C0C21(ctx, &req);
	if (ret != 0) {
		SceActivityDb_6E198FFA(ctx);
		return ret;
	}

	paf::vector<SceInt64> uids;
	paf::string paramStr(m_param);
	size_t prefixLen = sce_paf_strlen("status_uid=");
	if (paramStr.find("status_uid=") != paf::string::npos)
		paramStr.erase(0, prefixLen);

	char *endp = NULL;
	SceInt64 uid = sce_paf_strtoll(paramStr.c_str(), &endp, 10);
	if (endp != NULL && *endp == '\0') {
		m_statusUid = uid;
		uids.push_back(uid);
	}

	ret = SceActivityDb_74F96707(ctx, &uids, &m_items);
	if (ret != 0) {
		SceActivityDb_6E198FFA(ctx);
		return ret;
	}

	unsigned char b1[8], b2[8], b3[8];
	ret = SceActivityDb_7ED9033C(ctx, b1, &m_lastReadTime, b2, b3);
	SceActivityDb_6E198FFA(ctx);
	return ret;
}

/* 0x8119FF8E */
void GetStatusInfoJob::Finish(int result)
{
	paf::ui::Widget *waitW = (paf::ui::Widget *)m_waitWidget;
	paf::ui::Widget *parent = *((paf::ui::Widget **)waitW + 90);
	typedef void (*CloseFn)(paf::ui::Widget *);
	(*(CloseFn **)(*(void **)waitW))[108 / 4](waitW);

	if (result != 0) {
		unsigned char dlg[200];
		SceCommonGuiDialog_696DB9C9(dlg);
		*(int *)(dlg + 68) = result;
		if (result == (int)0x80100F05)
			*(int *)(dlg + 92) = 482989490;
		typedef void (*ShowErrFn)(void *);
		void *topmenu = *((void **)g_liveareaMgr + 1);
		(*(ShowErrFn *)((char *)topmenu + 268))(dlg);
		return;
	}

	if (m_items.size() == 1) {
		sceAppMgrLaunchAppByName(0, m_title.c_str(), NULL);
		paf::common::SharedPtr<SceUInt64> tsSp(new SceUInt64(m_lastReadTime));
		paf::ui::Widget *overlay = new paf::ui::Widget(parent, NULL);
		paf::math::v4 pos(0.0f, -22.0f, 0.0f, 0.0f);
		paf::math::v4 sz(960.0f, 512.0f, 0.0f, 0.0f);
		overlay->SetPos(pos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
		overlay->SetSize(sz, NULL, 65540, 0, 0.0f, NULL, NULL);

		struct {
			int type;
			paf::string filter;
			paf::string title;
			int flags;
		} req;
		req.type = 0;
		req.title = m_title;
		req.flags = 0;

		paf::common::SharedPtr<int> ctxB;
		paf::common::SharedPtr<int> ctxA;
		liveareaStatusDlgCreateCtx(&ctxA, &ctxB, &req);

		void *mem = operator new(240);
		if (mem != NULL) {
			paf::common::RefObj *itemObj = NULL;
			void *nullRef = NULL;
			void *dlg = liveareaStatusDlgCtor(mem, 0, overlay, &itemObj, 3, &ctxA, &tsSp, &nullRef);
			if (dlg != NULL) {
				liveareaStatusDlgSub31DA(dlg, 0);
				liveareaStatusDlgShow(dlg);
				s_statusDlgId = *((int *)dlg + 1);
				liveareaStatusDlgBind(dlg, &ctxB, m_title.c_str());
				return;
			}
		}
		(*(CloseFn **)(*(void **)overlay))[108 / 4](overlay);
	}
}

/* 0x811A064C */
void liveareaLaunchWithArg(const char *title, const char *param)
{
	if (s_statusJobActive)
		return;
	if (s_statusDlgId >= 0 && liveareaCheckDialogActive() != 0) {
		liveareaCloseDialogFast();
		s_statusDlgId = -1;
	}
	typedef void *(*CreateWaitFn)(int);
	void *topmenu = *((void **)g_liveareaMgr + 1);
	void *waitW = (*(CreateWaitFn *)((char *)topmenu + 204))(0);
	if (waitW != NULL) {
		paf::common::SharedPtr<paf::job::JobItem> sp = GetStatusInfoJob::Enqueue(title, param, waitW);
	}
}
