/*
 * SceShell livearea_plugin: Content Manager (NPXS10026) LiveArea banner
 * 3.65-CEX: 0x811AD166 - 0x811AE858 (sti 0x811AE7E8)
 */

#include <stddef.h>
#include <scetypes.h>
#include <appmgr.h>
#include <std/stdio.h>
#include <std/string.h>
#include <common/ref_obj.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/surface/surface.h>
#include <job/job.h>
#include <math/math.h>
#include <plugin.h>
#include <timer/timer.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_button.h>
#include <widget/w_image_button.h>

#include "../../livespace_db/lsdb_keys.h"

extern void *g_liveareaMgr;					/* 0x815670A8 */
extern void *g_pafFramework;					/* 0x81488474 */

extern "C" {
extern int sceRegMgrSrvGetRegionForUser(void);
extern int sceAppMgrLaunchAppByNameForShell(int flags, const char *titleId, const char *param);
}

extern int liveareaStorageHookAdd(void);			/* 0x810E17DE */
extern int liveareaStorageHookRegister(int type, int (*cb)(int, const char *), void *arg); /* 0x810E17E6 */
extern void liveareaFormatUriParam(paf::string *out, const paf::string *in); /* 0x8119AB34 */

static float s_cmaBarMaxWidth = 0.0f;				/* 0x81544D94 */
static SceUInt32 s_cmaBarCurWidth = 0;				/* 0x81544D98 */
static SceUInt8 s_cmaStorageHooked = 0;				/* 0x81544D9C */

static const int s_cmaTexObjIds[2] = { 0, 0x44 };		/* 0x81511818 */

struct CmaCallbackEntry {
	void (*fn)(void);
	void *arg;
};

struct CmaCallbackList {
	void *alloc;
	CmaCallbackEntry *data;
	int count;
	int capacity;
};

struct CmaPageMgrVtbl {
	void *pad0[4];
	paf::ui::Widget *(*getWidget)(void);			/* +16 */
	void *pad1[7];
	int (*findAppByTitle)(const char *titleId);		/* +48 */
	void *pad2[5];
	void (*regUnfocusCb)(int pageId, int (*cb)(int, int), void *arg); /* +72 */
	void (*unregUnfocusCb)(int pageId, int (*cb)(int, int), void *arg); /* +76 */
	void *pad3[5];
	void (*setFocusCb)(int pageId, int (*cb)(void), void *arg); /* +100 */
};

class LiveAreaCmaGetSizeJob : public paf::job::JobItem {
public:
	typedef void (*Callback)(int res, char showBar, SceInt64 total, SceInt64 freeSize);

	LiveAreaCmaGetSizeJob(Callback cb, char showBar)
		: paf::job::JobItem("Job_GetSize", NULL),
		  m_cb(cb), m_showBar(showBar), m_total(0), m_free(0)
	{}

	virtual ~LiveAreaCmaGetSizeJob() {}
	virtual int Run() { return 0; }
	virtual void Finish(int) {}

	Callback m_cb;
	char m_showBar;
	SceInt64 m_total;
	SceInt64 m_free;
};

static int liveareaCmaOnFocusCb(void);
static int liveareaCmaOnUnfocusCb(int a1, int pageId);
static int liveareaCmaOnStorageEvent(int a1, const char *titleId);
static void liveareaCmaResetGlobals(void);
static void liveareaCmaOnButtonPress(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);
static void liveareaCmaOnTextLayout(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);

static void cmaWidgetShow(paf::ui::Widget *w, paf::Timer *t)
{
	SceUInt8 flags = *((SceUInt8 *)w + 406);
	if ((flags & 0x80) == 0) {
		*((SceUInt8 *)w + 388) = 0xFF;
		*((SceUInt8 *)w + 406) = flags | 0x80;
		typedef void (*Fn)(paf::ui::Widget *, int);
		(*(Fn *)(*(char **)w + 144))(w, 1);
	}
	ScePafWidget_FF5BC957(w, t, 2, 0.0f, NULL);
}

static void cmaWidgetHide(paf::ui::Widget *w)
{
	*((SceUInt8 *)w + 406) &= ~0x80u;
	*((SceUInt8 *)w + 388) = 0xFF;
	typedef void (*Fn)(paf::ui::Widget *, int);
	(*(Fn *)(*(char **)w + 148))(w, 1);
	ScePafWidget_4588699E(w, NULL, 2, 0.0f, NULL);
}

static paf::IDParam makeHashId(SceUInt32 hash)
{
	return paf::IDParam(hash);
}

static paf::ui::Widget *findTypedImageButton(paf::ui::Widget *parent, SceUInt32 hash)
{
	paf::IDParam id = makeHashId(hash);
	if (!parent->FindChild(id, 0))
		return NULL;
	paf::ui::Widget *w = parent->FindChild(makeHashId(hash), 0);
	typedef int (*IsTypeFn)(paf::ui::Widget *, const char *);
	const char *tn = paf::ui::ImageButton::TypeName();
	if ((*(IsTypeFn *)(*(char **)w + 100))(w, tn))
		return parent->FindChild(makeHashId(hash), 0);
	return NULL;
}

static paf::ui::Widget *findTypedButton(paf::ui::Widget *parent, SceUInt32 hash)
{
	paf::IDParam id = makeHashId(hash);
	if (!parent->FindChild(id, 0))
		return NULL;
	paf::ui::Widget *w = parent->FindChild(makeHashId(hash), 0);
	typedef int (*IsTypeFn)(paf::ui::Widget *, const char *);
	const char *tn = paf::ui::Button::TypeName();
	if ((*(IsTypeFn *)(*(char **)w + 100))(w, tn))
		return parent->FindChild(makeHashId(hash), 0);
	return NULL;
}

/* 0x811AD166 */
int liveareaCmaSetActive(int active, int pageId)
{
	CmaPageMgrVtbl *mgr = *(CmaPageMgrVtbl **)((char *)g_liveareaMgr + 4);
	SceUInt8 want = (SceUInt8)active;
	int res = want;
	if (!mgr)
		return res;

	int id = pageId;
	if (id == -1) {
		res = mgr->findAppByTitle("NPXS10026");
		id = res;
	}
	if (id >= 0) {
		res = s_cmaStorageHooked;
		if (s_cmaStorageHooked != want) {
			int r;
			if (want != 0) {
				mgr->setFocusCb(id, liveareaCmaOnFocusCb, NULL);
				r = liveareaStorageHookAdd();
			} else {
				mgr->setFocusCb(id, NULL, NULL);
				r = liveareaStorageHookRegister(2, liveareaCmaOnStorageEvent, NULL);
			}
			if (r >= 0) {
				if (want != 0)
					mgr->regUnfocusCb(id, liveareaCmaOnUnfocusCb, NULL);
				else
					mgr->unregUnfocusCb(id, liveareaCmaOnUnfocusCb, NULL);
			}
			s_cmaStorageHooked = want;
		}
	}
	return res;
}

/* 0x811AD216 */
void liveareaCmaShowBar(void)
{
	CmaPageMgrVtbl *mgr = *(CmaPageMgrVtbl **)((char *)g_liveareaMgr + 4);
	if (!mgr || mgr->findAppByTitle("NPXS10026") < 0)
		return;
	paf::ui::Widget *root = mgr->getWidget();
	if (!root)
		return;

	paf::ui::Widget *btn = findTypedImageButton(root, 1170604782u);
	if (!btn)
		return;

	paf::ui::Widget *bar = btn->FindChild(makeHashId(651034106u), 0);
	if (bar)
		cmaWidgetShow(bar, NULL);

	typedef void *(*GetObjFn)(paf::ui::Widget *, int);
	void *obj = (*(GetObjFn *)(*(char **)btn + 248))(btn, 3);
	if (obj && (*((SceUInt8 *)obj + 52) & 0x20) == 0)
		*((SceUInt8 *)obj + 52) |= 0x20u;
}

/* 0x811AD4A0 */
void liveareaCmaOnGetSize(int result, char showBar, SceInt64 total, SceInt64 freeSize)
{
	if (result < 0)
		return;
	if (showBar != 0) {
		liveareaCmaShowBar();
		liveareaCmaSetActive(1, -1);
	}
	if (total != 0) {
		SceUInt64 used = (SceUInt64)(total - freeSize);
		float ratio = (float)((double)used / (double)(SceUInt64)total);
		SceUInt32 w = (SceUInt32)(s_cmaBarMaxWidth * ratio);
		if (w < 5 && total != freeSize)
			w = 5;
		if (s_cmaBarCurWidth != w) {
			paf::IDParam id = makeHashId(651034106u);
			CmaPageMgrVtbl *mgr = *(CmaPageMgrVtbl **)((char *)g_liveareaMgr + 4);
			paf::ui::Widget *bar = NULL;
			if (mgr && mgr->findAppByTitle("NPXS10026") >= 0) {
				paf::ui::Widget *root = mgr->getWidget();
				if (root)
					bar = root->FindChild(id, 0);
			}
			if (bar) {
				const float *cur = (const float *)bar->GetSize(0);
				paf::math::v4 sz((float)w, cur[1], cur[2], cur[3]);
				bar->SetSize(sz, NULL, 65540, 0, 0.0f, NULL, NULL);
				s_cmaBarCurWidth = w;
			}
		}
	}
}

/* 0x811AD6A2 */
int liveareaCmaRequestSize(char showBar)
{
	paf::job::JobQueue *q = (paf::job::JobQueue *)((char *)g_pafFramework + 584);
	if (!q)
		return -2;
	LiveAreaCmaGetSizeJob *job = new LiveAreaCmaGetSizeJob(liveareaCmaOnGetSize, showBar);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (!sp.get())
		return -2;
	return q->Enqueue(sp);
}

/* 0x811AD7B8 */
void liveareaCmaAnimatePopIn(paf::ui::Widget *w)
{
	if (!w)
		return;
	cmaWidgetHide(w);
	paf::Timer *t1 = new paf::Timer();
	if (t1)
		t1->InitTimer(200.0f, 21, 0, -1, NULL);
	cmaWidgetShow(w, t1);

	paf::math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
	w->SetScale(zero, NULL, 65541, 0, 0.0f, NULL, NULL);

	paf::math::v4 one(1.0f, 1.0f, 0.0f, 0.0f);
	paf::Timer *t2 = new paf::Timer();
	if (t2)
		t2->InitTimer(200.0f, 32, 0, -1, NULL);
	w->SetScale(one, t2, 65541, 2, 0.0f, NULL, NULL);
}

/* 0x811AD966 */
void liveareaCmaSetup(paf::Plugin *plugin, paf::ui::Widget *root)
{
	if (!root)
		return;

	CmaCallbackList *list = (CmaCallbackList *)((char *)root + 604);
	if (list->count == 0 && list->capacity == 0) {
		/* lazy alloc handled by widget callback vector */
	}
	if (list->data && list->count < list->capacity) {
		list->data[list->count].fn = liveareaCmaResetGlobals;
		list->data[list->count].arg = NULL;
		list->count++;
	}

	paf::graph::Surface *tex = NULL;
	if (sceRegMgrSrvGetRegionForUser() == 13 && plugin) {
		paf::IDParam tid = makeHashId(379628764u);
		paf::intrusive_ptr<paf::graph::Surface> sp = plugin->GetTexture(tid);
		if (sp.get()) {
			tex = sp.get();
			tex->AddRef();
		}
	}

	paf::ui::Widget *frame1 = root->FindChild(makeHashId(260879598u), 0);
	if (frame1) {
		paf::ui::Widget *btn1 = findTypedImageButton(frame1, 1170604782u);
		if (btn1) {
			paf::ui::EventCBListener *cb1 = new paf::ui::EventCBListener(liveareaCmaOnButtonPress, (void *)1);
			btn1->AddEventListener(268435464, cb1, 0);

			paf::ui::EventCBListener *cb2 = new paf::ui::EventCBListener(liveareaCmaOnTextLayout, (void *)2);
			btn1->AddEventListener(1048580, cb2, 0);

			typedef void *(*GetObjFn)(paf::ui::Widget *, int);
			void *obj = (*(GetObjFn *)(*(char **)btn1 + 248))(btn1, 3);
			if (obj) {
				s_cmaBarMaxWidth = *(float *)((char *)obj + 24);
				if (*((SceUInt8 *)obj + 52) & 0x20)
					*((SceUInt8 *)obj + 52) &= ~0x20u;
			}

			paf::ui::Widget *bar = btn1->FindChild(makeHashId(651034106u), 0);
			if (bar) {
				cmaWidgetHide(bar);
				liveareaCmaRequestSize(1);
			}
			liveareaCmaAnimatePopIn(btn1);
		}
	}

	paf::ui::Widget *frame2 = root->FindChild(makeHashId(71033373u), 0);
	if (frame2) {
		paf::ui::Widget *btn2 = findTypedButton(frame2, (SceUInt32)-1669094780);
		if (btn2) {
			cmaWidgetHide(btn2);
			if (tex) {
				for (SceUInt32 i = 0; i < 2; ++i) {
					typedef void *(*GetObjFn)(paf::ui::Widget *, int);
					void *obj = (*(GetObjFn *)(*(char **)btn2 + 248))(btn2, s_cmaTexObjIds[i]);
					if (obj) {
						typedef void (*SetTexFn)(void *, paf::graph::Surface **, int);
						(*(SetTexFn *)(*(char **)obj + 32))(obj, &tex, 0);
					}
				}
			}

			paf::ui::EventCBListener *cb3 = new paf::ui::EventCBListener(liveareaCmaOnButtonPress, NULL);
			btn2->AddEventListener(268435464, cb3, 0);

			paf::ui::EventCBListener *cb4 = new paf::ui::EventCBListener(liveareaCmaOnTextLayout, (void *)1);
			btn2->AddEventListener(1048580, cb4, 0);

			liveareaCmaAnimatePopIn(btn2);
		}
	}

	if (tex)
		tex->Release();
}

/* 0x811AE0A4 */
void liveareaCmaCleanup(paf::ui::Widget *root)
{
	if (!root)
		return;

	CmaCallbackList *list = (CmaCallbackList *)((char *)root + 604);
	if (list->data) {
		for (int i = 0; i < list->count; ++i) {
			if (list->data[i].fn == liveareaCmaResetGlobals && list->data[i].arg == NULL) {
				for (int j = i + 1; j < list->count; ++j)
					list->data[j - 1] = list->data[j];
				list->count--;
				break;
			}
		}
	}

	paf::ui::Widget *frame1 = root->FindChild(makeHashId(260879598u), 0);
	if (frame1) {
		s_cmaBarMaxWidth = 0.0f;
		s_cmaBarCurWidth = 0;
		paf::ui::Widget *btn1 = findTypedImageButton(frame1, 1170604782u);
		if (btn1) {
			btn1->DeleteEventCallback(268435464, NULL, NULL);
			btn1->DeleteEventCallback(1048580, NULL, NULL);
		}
		*(paf::IDParam *)((char *)frame1 + 320) = paf::IDParam("");
		typedef void (*DestroyFn)(paf::ui::Widget *);
		(*(DestroyFn *)(*(char **)frame1 + 108))(frame1);
	}

	paf::ui::Widget *frame2 = root->FindChild(makeHashId(71033373u), 0);
	if (frame2) {
		paf::ui::Widget *btn2 = findTypedButton(frame2, (SceUInt32)-1669094780);
		if (btn2) {
			btn2->DeleteEventCallback(268435464, NULL, NULL);
			btn2->DeleteEventCallback(1048580, NULL, (void *)1);
		}
		*(paf::IDParam *)((char *)frame2 + 320) = paf::IDParam("");
		typedef void (*DestroyFn)(paf::ui::Widget *);
		(*(DestroyFn *)(*(char **)frame2 + 108))(frame2);
	}
}

/* 0x811AE5A0 */
static void liveareaCmaResetGlobals(void)
{
	s_cmaBarMaxWidth = 0.0f;
	s_cmaBarCurWidth = 0;
}

/* 0x811AE5B8 */
static void liveareaCmaOnButtonPress(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *userdata)
{
	int id = (int)userdata;
	paf::string out;
	paf::string uri;
	if (id == 1)
		uri = "psla:mc_mgmt";
	else if (id == 0)
		uri = "psla:hostconnect";
	liveareaFormatUriParam(&out, &uri);
	sceAppMgrLaunchAppByNameForShell(0x20000, "NPXS10026", out.c_str());
}

/* 0x811AE680 */
static void liveareaCmaOnTextLayout(SceInt32, paf::ui::Handler *h, paf::ui::Event *, void *userdata)
{
	if (!h)
		return;
	typedef void *(*GetObjFn)(paf::ui::Handler *, int);
	void *raw = (*(GetObjFn *)(*(char **)h + 248))(h, 1);
	if (!raw)
		return;

	paf::graph::TextObj *tobj = (paf::graph::TextObj *)raw;
	typedef int (*CheckTypeFn)(paf::graph::TextObj *, const char *);
	if (!(*(CheckTypeFn *)(*(char **)tobj))(tobj, paf::graph::TextObj::DrawObjType()))
		return;

	paf::graph::TextObj *obj = (paf::graph::TextObj *)(*(GetObjFn *)(*(char **)h + 248))(h, 1);
	if (!obj)
		return;

	if (*((SceUInt8 *)obj + 52) & 0x20)
		*((SceUInt8 *)obj + 52) &= ~0x20u;

	uint32_t lines = 0;
	obj->GetLineCount(lines);
	if (lines > (uint32_t)userdata) {
		paf::wstring str;
		obj->GetString(str);
		obj->SetStyleAttribute((paf::graph::TextStyleAttribute)1, 0, str.size(), (int32_t)0);
		paf::math::v4 pos(*(float *)((char *)obj + 24) + 4.0f,
			*(float *)((char *)obj + 28),
			*(float *)((char *)obj + 32), 0.0f);
		typedef void (*SetPosFn)(paf::graph::TextObj *, const paf::math::v4 &);
		typedef void (*UpdateFn)(paf::graph::TextObj *);
		(*(SetPosFn *)(*(char **)obj + 44))(obj, pos);
		(*(UpdateFn *)(*(char **)obj + 56))(obj);
	}
	if ((*((SceUInt8 *)obj + 52) & 0x20) == 0)
		*((SceUInt8 *)obj + 52) |= 0x20u;
}

/* 0x811AE7B4 */
static int liveareaCmaOnFocusCb(void)
{
	return liveareaCmaRequestSize(0);
}

/* 0x811AE7BE */
static int liveareaCmaOnUnfocusCb(int, int pageId)
{
	return liveareaCmaSetActive(0, pageId);
}

/* 0x811AE7C8 */
static int liveareaCmaOnStorageEvent(int, const char *titleId)
{
	int r = sce_paf_strncmp(titleId, "NPXS10026", 10);
	if (r == 0)
		return liveareaCmaRequestSize(0);
	return r;
}
