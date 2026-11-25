/*
 * livearea_page_inner.cpp: livearea_plugin content zone, gate & frame renderer
 * (0x811A364C-0x811ACF10, sti 0x811ACC24). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/list>
#include <plugin.h>
#include <framework.h>
#include <common/ref_obj.h>
#include <common/shared_ptr.h>
#include <widget/core/widget.h>
#include <widget/w_list_view.h>
#include <widget/w_file_list.h>
#include <graphics/surface/surface.h>
#include <timer/timer.h>
#include <appmgr.h>
#include "../../livespace_db/lsdb_keys.h"

extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */
struct LiveAreaMgr;
extern LiveAreaMgr *g_liveareaMgr;				/* 0x815670A8 */

extern "C" {
int sceAppMgrLaunchAppByNameForShell(unsigned int flags, const char *name, const char *param);
int sce_paf_rand(void);
int SceLsdb_628CFBE1(void *item, int a2, unsigned char flag);
void *SceLsdb_92D3A2EA(void *frame, int idx);
unsigned char SceLsdb_69B32F6C(int arg);
void SceCommonGuiDialog_696DB9C9(void *param);
void SceCommonGuiDialog_20B141F1(void *param);
}

extern void liveareaMgrUpdatePage(void *inner, int mode);	/* 0x8119A9BA */
extern void liveareaReplaceOnlineId(paf::string &out, const paf::string &in); /* 0x8119AB34 */
extern void liveareaPageUpdateLastLaunch(void *page);		/* 0x8119B9B2 */
extern int liveareaPageCreateImgCache(void *page, paf::string *path, void **outRef); /* 0x8119BE9A */
extern void liveareaPageEnsureFeed(void *page);			/* 0x8119BF26 */
extern void liveareaPageOnFocusComplete(void *page);		/* 0x8119C042 */
extern int liveareaLoadRco(const paf::string *rcoName, const paf::string *pluginName, int arg); /* 0x811ACF10 */
extern int liveareaUnloadRco(const paf::string *pluginName);	/* 0x811AD0C2 */
extern void liveareaListSrcSetSeg(paf::common::RefObj *src, int idx, paf::common::RefObj **cell, void *param); /* 0x8134A8D2 */
extern void *liveareaXmlVisCtor(void);				/* 0x811B7470 */
extern void *liveareaXmlCellCtor(void);				/* 0x8135D814 */
extern void liveareaXmlLoadJobEnqueue(paf::common::SharedPtr<int> *out, void *inner,
				      const char *xmlPath, const char *baseDir,
				      int a5, int a6, int flags, int a8); /* 0x811B67AA */
extern void shellRecordMetric(int id, int val);			/* 0x81376D36 */

static paf::IDParam s_destroyContentZoneId("__destroy_content_zone__"); /* 0x815672A4 */
static paf::math::v4 s_gateDimColor(0.0f, 0.0f, 0.0f, 0.75f);	/* 0x815672B8 */
static paf::string s_appContentManager("content_manager");	/* 0x815672C8 */
static paf::string s_appMusic("music");				/* 0x815672D4 */
static paf::string s_appKids("kids");				/* 0x815672E0 */

static paf::IDParam s_topIconIds[9] = {
	paf::IDParam("PATCH"),
	paf::IDParam("UPGRADE"),
	paf::IDParam("SEARCH"),
	paf::IDParam("VOUCHERCODE"),
	paf::IDParam("ONLINE_HELP"),
	paf::IDParam("HELP"),
	paf::IDParam("RELOAD"),
	paf::IDParam("IMPORT_SAVEDATA"),
	paf::IDParam("ONLINE_STORAGE")
};

static paf::IDParam s_tmplContentZoneLayout("template_content_zone_layout"); /* 0x8156737C */
static paf::IDParam s_gateId("gate");				/* 0x8156738C */

static paf::IDParam s_frameIds[10] = {
	paf::IDParam("frame1"), paf::IDParam("frame2"), paf::IDParam("frame3"),
	paf::IDParam("frame4"), paf::IDParam("frame5"), paf::IDParam("frame6"),
	paf::IDParam("frame7"), paf::IDParam("frame8"), paf::IDParam("frame9"),
	paf::IDParam("frame10")
};

struct LiveAreaFrameState {
	int curIdx;						/* +0x00 */
	float timer;						/* +0x04 */
	int pendingLoads;					/* +0x08 */
	bool switching;						/* +0x0C */
	unsigned char pad0D[3];
	paf::ui::Widget *frameWidget;				/* +0x10 */
	int slot20;						/* +0x14 */
	int slot24;						/* +0x18 */
	int textObjIdx;						/* +0x1C */
	int slot32;						/* +0x20 */
	int slot36;						/* +0x24 */
	int slot40;						/* +0x28 */
	int slot44;						/* +0x2C */
	int scrollMode;						/* +0x30 */
	int pad34;
	float scrollDeltaX;					/* +0x38 */
	float scrollDeltaY;					/* +0x3C */
	bool scrollActive;					/* +0x40 */
	unsigned char pad41[3];
	int scrollPhase;					/* +0x44 */
	float scrollElapsed;					/* +0x48 */
	float scrollDuration;					/* +0x4C */
};

struct LiveAreaPageInitInfo {
	int style;						/* +0x00 */
	int field4;						/* +0x04 */
	int field8;						/* +0x08 */
	int field12;						/* +0x0C */
	paf::wstring title;					/* +0x10 */
	paf::string searchKey;					/* +0x1C */
	paf::string url;					/* +0x28 */
	int appAttr;						/* +0x34 */
	int kind;						/* +0x38 */
	bool hasInfo;						/* +0x3C */
	bool hasExt;						/* +0x3D */
	bool hasFeed;						/* +0x3E */
	bool flag3F;						/* +0x3F */
};

class LiveAreaPageInner {
public:
	LiveAreaPageInner(void *page, const char *dir, const LiveAreaPageInitInfo *info); /* 0x811A3C72 */
	~LiveAreaPageInner();					/* 0x811A3F4E */

	void Init();						/* 0x811A417E */
	int TryReloadXml();					/* 0x811A44CA */
	void AnimatePopIn(paf::ui::Widget *w, void *cb);	/* 0x811A4684 */
	void OnInitialImagesLoaded();				/* 0x811A58D4 */
	void BuildLayout(int flags);				/* 0x811A985E */
	void OnRcoLoaded();					/* 0x811A9CCC */
	void SetGateSurface(paf::graph::Surface **surf);	/* 0x811AA854 */
	void ClearGateSurface();				/* 0x811AA9F2 */
	void LoadDefaultXml();					/* 0x811AAB9E */
	int GetFrameTimestamp(const char *frameId, SceUInt64 *outTs); /* 0x811AACC2 */
	int QueueReload(int flags, const char *dir);		/* 0x811AAD60 */
	int QueueReloadFromReq(const void *req);		/* 0x811AAE02 */
	int GetGateNormPos(float *outX, float *outY);		/* 0x811AAE14 */
	int SwitchFrame(void *frameDesc, LiveAreaFrameState *st); /* 0x811AAE7C */
	void ResetGateFocus();					/* 0x811AAFB8 */
	void Tick();						/* 0x811AB056 */
	void OnScroll();					/* 0x811AB2DE */
	void StartReloadSpin();					/* 0x811AB38A */
	void StopReloadSpin(int err);				/* 0x811AB5DA */
	int GetReloadCount();					/* 0x811AB8F2 */
	int IsIntroTimerDone();					/* 0x811AB8F8 */
	void ClearGateInitFlag();				/* 0x811AB944 */
	void SetDirtyFlag();					/* 0x811AB94C */
	void UpdateInitInfo(const LiveAreaPageInitInfo *info);	/* 0x811AB954 */
	int GetResetMask();					/* 0x811AB9B8 */
	int HasPatchIcon();					/* 0x811ABA06 */
	int LayoutFrameChild(paf::ui::Widget *w, bool anim);	/* 0x811ABAB4 */
	LiveAreaFrameState *FindFrameState(paf::ui::Widget *w, void **outDesc); /* 0x811ABC98 */

	void *m_page;						/* +0x00 */
	int m_style;						/* +0x04 */
	int m_field08;						/* +0x08 */
	int m_field0C;						/* +0x0C */
	int m_field10;						/* +0x10 */
	paf::wstring m_title;					/* +0x14 */
	paf::string m_searchKey;				/* +0x20 */
	paf::string m_url;					/* +0x2C */
	int m_appAttr;						/* +0x38 */
	int m_kind;						/* +0x3C */
	bool m_hasInfo;						/* +0x40 */
	bool m_hasExt;						/* +0x41 */
	bool m_hasFeed;						/* +0x42 */
	bool m_flag43;						/* +0x43 */
	int m_rcoRefCount;					/* +0x44 */
	int m_reloadFlags;					/* +0x48 */
	paf::string m_reloadDir;				/* +0x4C */
	paf::string m_baseDir;					/* +0x58 */
	paf::string m_xmlPath;					/* +0x64 */
	paf::string m_helpUri;					/* +0x70 */
	int m_initPhase;					/* +0x7C */
	paf::common::SharedPtr<int> m_xmlDoc;			/* +0x80 */
	void *m_gateCacheRef;					/* +0x88 */
	paf::graph::Surface *m_gateSurface;			/* +0x8C */
	paf::ui::Widget *m_contentZone;				/* +0x90 */
	paf::ui::Widget *m_gateWidget;				/* +0x94 */
	paf::ui::Widget *m_gateBtn;				/* +0x98 */
	bool m_gateHasBg;					/* +0x9C */
	unsigned char m_pad9D[3];
	int m_reloadCount;					/* +0xA0 */
	int m_reloadError;					/* +0xA4 */
	paf::common::SharedPtr<int> m_xmlJob;			/* +0xA8 */
	int m_pendingInitialLoads;				/* +0xB0 */
	bool m_shown;						/* +0xB4 */
	bool m_introDone;					/* +0xB5 */
	bool m_scrolledDown;					/* +0xB6 */
	bool m_hasLiveSurface;					/* +0xB7 */
	bool m_flagB8;						/* +0xB8 */
	bool m_flagB9;						/* +0xB9 */
	bool m_gateResetDone;					/* +0xBA */
	bool m_dirty;						/* +0xBB */
	paf::list<LiveAreaFrameState *> m_frames;		/* +0xBC */
};

static void *getTopmenuIf(void)
{
	return *((void **)g_liveareaMgr + 1);
}

/* 0x811A364C */
static void liveareaSelectNextItem(int *curIdx, void *frameDesc, unsigned char flag)
{
	if (frameDesc == NULL) {
		*curIdx = -1;
		return;
	}
	int count = *((int *)frameDesc + 15);
	if (count == 0) {
		*curIdx = -1;
		return;
	}
	void **items = *((void ***)frameDesc + 14);
	int valid[5];
	int nValid = 0;
	for (int i = 0; i < count; ++i) {
		if (SceLsdb_628CFBE1(items[i], 0, flag) != 0) {
			valid[nValid++] = i;
			if (nValid >= 5)
				break;
		}
	}
	if (nValid <= 0) {
		*curIdx = -1;
		return;
	}
	int mode = *((int *)frameDesc + 5);
	if (mode == 0) {
		int picked = valid[0];
		for (int i = 0; i < nValid; ++i) {
			if (*curIdx < valid[i]) {
				picked = valid[i];
				break;
			}
		}
		*curIdx = picked;
	} else if (mode == 1) {
		if (nValid <= 1) {
			*curIdx = valid[0];
		} else {
			int cand[5];
			int nCand = 0;
			for (int i = 0; i < nValid; ++i) {
				if (valid[i] != *curIdx)
					cand[nCand++] = valid[i];
			}
			int r = (int)((float)nCand * (float)sce_paf_rand() / 32767.0f);
			if (r >= nCand)
				r = nCand - 1;
			*curIdx = (r < 0) ? cand[0] : cand[r];
		}
	}
	if (*curIdx >= count)
		*curIdx = count - 1;
	if (*curIdx < -1)
		*curIdx = -1;
}

/* 0x811A37F2 */
static bool liveareaTickFrameSwitch(LiveAreaFrameState *st, void *frameDesc, unsigned char flag)
{
	if (st->pendingLoads > 0 || st->switching)
		return false;
	void *curItem = SceLsdb_92D3A2EA(frameDesc, st->curIdx);
	bool invalid = (curItem == NULL) || (SceLsdb_628CFBE1(curItem, 0, flag) == 0);
	unsigned int interval = *((unsigned int *)frameDesc + 6);
	if (interval != 0) {
		st->timer += (1.0f / 60.0f);
		if (st->timer >= (float)interval) {
			st->timer = 0.0f;
			invalid = true;
		}
	}
	if (!invalid)
		return false;
	liveareaSelectNextItem(&st->curIdx, frameDesc, flag);
	return curItem != SceLsdb_92D3A2EA(frameDesc, st->curIdx);
}

/* 0x811A38A6 */
static void liveareaTickTextScroll(LiveAreaFrameState *st)
{
	if (!st->scrollActive)
		return;
	st->scrollElapsed += (1000.0f / 60.0f);
	if (st->frameWidget == NULL)
		return;
	paf::IDParam id((uint32_t)-635233871);
	paf::ui::Widget *child = st->frameWidget->FindChild(id, 0);
	if (child == NULL)
		return;
	switch (st->scrollPhase) {
	case 0:
		if (st->scrollElapsed > 3000.0f) {
			st->scrollElapsed = 0.0f;
			st->scrollPhase = 2;
		}
		break;
	case 1:
		if (st->scrollElapsed > 1000.0f) {
			st->scrollElapsed = 0.0f;
			st->scrollPhase = 2;
		}
		break;
	case 2:
		if (st->scrollElapsed >= st->scrollDuration) {
			st->scrollElapsed = 0.0f;
			st->scrollPhase = 3;
		}
		break;
	case 3:
		if (st->scrollElapsed > 1000.0f) {
			st->scrollElapsed = 0.0f;
			st->scrollPhase = 4;
		}
		break;
	case 4:
		if (st->scrollElapsed >= 1000.0f) {
			st->scrollElapsed = 0.0f;
			st->scrollPhase = 1;
		}
		break;
	}
}

/* 0x811A3C72 */
LiveAreaPageInner::LiveAreaPageInner(void *page, const char *dir, const LiveAreaPageInitInfo *info)
	: m_page(page),
	  m_style(info->style),
	  m_field08(info->field4),
	  m_field0C(info->field8),
	  m_field10(info->field12),
	  m_title(info->title),
	  m_searchKey(info->searchKey),
	  m_url(info->url),
	  m_appAttr(info->appAttr),
	  m_kind(info->kind),
	  m_hasInfo(info->hasInfo),
	  m_hasExt(info->hasExt),
	  m_hasFeed(info->hasFeed),
	  m_flag43(info->flag3F),
	  m_rcoRefCount(0),
	  m_reloadFlags(0),
	  m_reloadDir(),
	  m_baseDir(dir),
	  m_xmlPath(),
	  m_helpUri(),
	  m_initPhase(2),
	  m_xmlDoc(),
	  m_gateCacheRef(NULL),
	  m_gateSurface(NULL),
	  m_contentZone(NULL),
	  m_gateWidget(NULL),
	  m_gateBtn(NULL),
	  m_gateHasBg(false),
	  m_reloadCount(0),
	  m_reloadError(0),
	  m_xmlJob(),
	  m_pendingInitialLoads(0),
	  m_shown(false),
	  m_introDone(false),
	  m_scrolledDown(false),
	  m_hasLiveSurface(false),
	  m_flagB8(false),
	  m_flagB9(false),
	  m_gateResetDone(false),
	  m_dirty(false),
	  m_frames()
{
	m_baseDir += "/";
	m_xmlPath = paf::string(dir) + paf::string("/template.xml");
}

/* 0x811A3F4E */
LiveAreaPageInner::~LiveAreaPageInner()
{
	if (m_xmlJob.get() != NULL) {
		typedef void (*CancelFn)(void *);
		(*(CancelFn **)(*(void **)m_xmlJob.get()))[2](m_xmlJob.get());
	}
	while (m_rcoRefCount > 0) {
		paf::string tmplName(*(const char **)((char *)m_page + 28));
		liveareaUnloadRco(&tmplName);
		--m_rcoRefCount;
	}
	while (m_frames.size() != 0) {
		LiveAreaFrameState *st = m_frames.front();
		m_frames.pop_front();
		delete st;
	}
	if (m_gateSurface != NULL)
		m_gateSurface->Release();
	if (m_gateCacheRef != NULL) {
		typedef void (*ReleaseFn)(void *);
		(*(ReleaseFn **)(*(void **)m_gateCacheRef))[7](m_gateCacheRef);
	}
}

/* 0x811A417E */
void LiveAreaPageInner::Init()
{
	paf::ui::FileList *fl = *(paf::ui::FileList **)((char *)m_page + 40);
	paf::common::RefObj *segSrc = new paf::common::RefObj();
	segSrc->AddRef();
	paf::common::SharedPtr<int> vis((int *)liveareaXmlVisCtor());
	paf::common::RefObj *cellSrc = (paf::common::RefObj *)liveareaXmlCellCtor();
	if (cellSrc != NULL)
		cellSrc->AddRef();

	unsigned char param[44];
	sce_paf_memset(param, 0, sizeof(param));
	param[25] = 1;
	liveareaListSrcSetSeg(segSrc, 0, &cellSrc, param);
	fl->SetSegmentLayoutType(0, paf::ui::ListView::LAYOUT_TYPE_LIST);

	if (cellSrc != NULL)
		cellSrc->Release();
	segSrc->Release();

	paf::common::SharedPtr<int> job;
	liveareaXmlLoadJobEnqueue(&job, this, m_xmlPath.c_str(), NULL, 0, 0, -1, 1);
	m_xmlJob = job;
}

/* 0x811A44CA */
int LiveAreaPageInner::TryReloadXml()
{
	if (m_reloadFlags == 0)
		return 1;
	if (m_xmlJob.get() != NULL || m_initPhase != 0 || !m_shown)
		return 0;
	if (!m_baseDir.empty() && !m_reloadDir.empty()) {
		if (sce_paf_strncmp(m_baseDir.c_str(), "vs0:", 4) == 0 &&
		    sce_paf_strncmp(m_reloadDir.c_str(), "ur0:", 4) == 0)
			m_reloadFlags = -1;
	}
	paf::common::SharedPtr<int> job;
	liveareaXmlLoadJobEnqueue(&job, this, m_xmlPath.c_str(), m_reloadDir.c_str(), 0, 1, m_reloadFlags, 0);
	m_xmlJob = job;
	if (m_xmlJob.get() == NULL)
		return 0;
	m_reloadFlags = 0;
	m_reloadDir = "";
	return 1;
}

/* 0x811A4684 */
void LiveAreaPageInner::AnimatePopIn(paf::ui::Widget *w, void *cb)
{
	ScePafWidget_4588699E(w, NULL, 2, 0.0f, NULL);
	paf::Timer *t1 = new paf::Timer();
	t1->InitTimer(21.0f, 0, -1, 0, NULL);
	ScePafWidget_FF5BC957(w, t1, 2, 0.0f, NULL);

	paf::math::v4 s0(0.0f, 0.0f, 0.0f, 0.0f);
	w->SetScale(s0, NULL, 65541, 0, 0.0f, NULL, NULL);

	paf::math::v4 s1(1.0f, 1.0f, 0.0f, 0.0f);
	paf::Timer *t2 = new paf::Timer();
	t2->InitTimer(32.0f, 0, -1, 0, NULL);
	typedef void (*ScaleCb)(paf::Timer *, paf::ui::AnimListener *, void *, void *, void *);
	w->SetScale(s1, t2, 65541, 2, 0.0f, (ScaleCb)cb, this);
}

/* 0x811A58D4 */
void LiveAreaPageInner::OnInitialImagesLoaded()
{
	m_shown = true;
	liveareaPageUpdateLastLaunch(m_page);
	liveareaPageEnsureFeed(m_page);
	liveareaMgrUpdatePage(this, 0);
}

/* 0x811A985E */
void LiveAreaPageInner::BuildLayout(int flags)
{
	(void)flags;
	TryReloadXml();
}

/* 0x811A9CCC */
void LiveAreaPageInner::OnRcoLoaded()
{
	BuildLayout(-1);
}

/* 0x811AA854 */
void LiveAreaPageInner::SetGateSurface(paf::graph::Surface **surf)
{
	m_hasLiveSurface = true;
	paf::graph::Surface *ns = *surf;
	if (ns != m_gateSurface) {
		if (m_gateSurface != NULL)
			m_gateSurface->Release();
		m_gateSurface = ns;
		if (ns != NULL)
			ns->AddRef();
	}
	if (m_gateBtn != NULL) {
		paf::intrusive_ptr<paf::graph::Surface> sp(ns);
		m_gateBtn->SetTexture(sp, 0, 0);
	}
}

/* 0x811AA9F2 */
void LiveAreaPageInner::ClearGateSurface()
{
	m_hasLiveSurface = false;
	if (m_gateSurface != NULL) {
		m_gateSurface->Release();
		m_gateSurface = NULL;
	}
}

/* 0x811AAB9E */
void LiveAreaPageInner::LoadDefaultXml()
{
	paf::common::SharedPtr<int> job;
	liveareaXmlLoadJobEnqueue(&job, this,
		"vs0:data/internal/livearea/default/sce_sys/livearea/contents/template.xml",
		"vs0:data/internal/livearea/default/sce_sys/livearea/contents/",
		1, 0, -1, 0);
	m_xmlJob = job;
}

/* 0x811AACC2 */
int LiveAreaPageInner::GetFrameTimestamp(const char *frameId, SceUInt64 *outTs)
{
	void *doc = m_xmlDoc.get();
	if (doc == NULL || m_reloadFlags != 0 || m_xmlJob.get() != NULL)
		return (int)0x80000002;
	if (frameId == NULL) {
		*outTs = *(SceUInt64 *)((char *)doc + 24);
		return 0;
	}
	return (int)0x80000001;
}

/* 0x811AAD60 */
int LiveAreaPageInner::QueueReload(int flags, const char *dir)
{
	if ((flags & 0x110000) != 0)
		flags = -1;
	m_reloadFlags |= flags;
	if (dir != NULL) {
		m_reloadDir = dir;
		m_reloadDir += "/";
	}
	TryReloadXml();
	return 1;
}

/* 0x811AAE02 */
int LiveAreaPageInner::QueueReloadFromReq(const void *req)
{
	const char *dir = *((const char *const *)req + 1);
	int flags = *((const int *)req + 2);
	return QueueReload(flags, dir);
}

/* 0x811AAE14 */
int LiveAreaPageInner::GetGateNormPos(float *outX, float *outY)
{
	if (m_gateBtn == NULL) {
		*outX = 0.0f;
		*outY = 0.0f;
		return 0;
	}
	if ((*((unsigned char *)m_gateBtn + 387) & 7) != 0)
		m_gateBtn->UpdateMatrix();
	*outX = *(const float *)((const char *)m_gateBtn + 232) / 480.0f;
	*outY = *(const float *)((const char *)m_gateBtn + 236) / 272.0f;
	return 1;
}

/* 0x811AAE7C */
int LiveAreaPageInner::SwitchFrame(void *frameDesc, LiveAreaFrameState *st)
{
	paf::IDParam id(*(const paf::string *)((const char *)frameDesc + 8));
	paf::ui::Widget *child = m_contentZone->FindChild(id, 0);
	if (child == NULL)
		return 0;
	st->switching = true;
	return 1;
}

/* 0x811AAFB8 */
void LiveAreaPageInner::ResetGateFocus()
{
	if (m_gateBtn != NULL) {
		paf::IDParam id((uint32_t)-147058763);
		paf::ui::Widget *child = m_gateBtn->FindChild(id, 0);
		if (child != NULL) {
			typedef void (*Vfn77)(paf::ui::Widget *, int);
			(*(Vfn77 **)(*(void **)child))[77](child, 0);
		}
	}
}

/* 0x811AB056 */
void LiveAreaPageInner::Tick()
{
	if (m_xmlDoc.get() == NULL || m_initPhase > 0 || !m_shown)
		return;
	if (m_gateBtn != NULL && !m_gateResetDone &&
	    (*((unsigned char *)m_gateBtn + 405) & 0x80) == 0 &&
	    (*((unsigned char *)m_gateBtn + 406) & 1) == 0) {
		m_gateResetDone = true;
		ResetGateFocus();
	}
	unsigned char flag = SceLsdb_69B32F6C(m_appAttr);
	(void)flag;
	for (paf::list<LiveAreaFrameState *>::iterator it = m_frames.begin();
	     it != m_frames.end(); ++it) {
		liveareaTickTextScroll(*it);
	}
}

/* 0x811AB2DE */
void LiveAreaPageInner::OnScroll()
{
	paf::ui::ListView *lv = *(paf::ui::ListView **)((char *)m_page + 40);
	typedef paf::ui::Widget *(*GetBodyFn)(paf::ui::ListView *);
	paf::ui::Widget *body = (*(GetBodyFn **)(*(void **)lv))[140 / 4](lv);
	paf::math::v4 pos = body->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	typedef void (*TopmenuFn172)(int, int);
	TopmenuFn172 fn = *(TopmenuFn172 *)((char *)getTopmenuIf() + 172);
	if (((const float *)&pos)[1] <= 264.0f) {
		if (m_scrolledDown) {
			m_scrolledDown = false;
			fn(*(int *)m_page, 0);
		}
	} else if (!m_scrolledDown) {
		m_scrolledDown = true;
		fn(*(int *)m_page, 1);
	}
}

/* 0x811AB38A */
void LiveAreaPageInner::StartReloadSpin()
{
	if (m_reloadCount == 0 && m_contentZone != NULL) {
		paf::IDParam barId(1728383896U);
		paf::ui::Widget *bar = m_contentZone->FindChild(barId, 0);
		if (bar != NULL) {
			paf::ui::Widget *btn = bar->FindChild(s_topIconIds[6], 0);
			if (btn != NULL) {
				paf::IDParam iconId((uint32_t)-1039764635);
				paf::ui::Widget *icon = btn->FindChild(iconId, 0);
				if (icon != NULL) {
					icon->KillAnimType(65542);
					paf::math::v4 rot(0.0f, 0.0f, -6.2831855f, 0.0f);
					paf::Timer *t1 = new paf::Timer();
					t1->InitTimer(22.0f, 0, -1, 0, NULL);
					icon->SetRot(rot, 0, t1, 65542, 0, 0.0f, NULL, NULL);
					paf::Timer *t2 = new paf::Timer();
					t2->InitTimer(0.0f, 0, -1, 0, NULL);
					icon->SetRot(rot, 0, t2, 268500998, 0, 0.0f, NULL, NULL);
				}
			}
		}
	}
	++m_reloadCount;
}

/* 0x811AB5DA */
void LiveAreaPageInner::StopReloadSpin(int err)
{
	--m_reloadCount;
	if (m_reloadError == 0 && err < 0)
		m_reloadError = err;
	if (m_reloadCount == 0) {
		if (m_contentZone != NULL) {
			paf::IDParam barId(1728383896U);
			paf::ui::Widget *bar = m_contentZone->FindChild(barId, 0);
			if (bar != NULL) {
				paf::ui::Widget *btn = bar->FindChild(s_topIconIds[6], 0);
				if (btn != NULL) {
					paf::IDParam iconId((uint32_t)-1039764635);
					paf::ui::Widget *icon = btn->FindChild(iconId, 0);
					if (icon != NULL) {
						icon->KillAnimType(65542);
						paf::math::v4 rot(0.0f, 0.0f, -6.2831855f, 0.0f);
						paf::Timer *t = new paf::Timer();
						t->InitTimer(0.0f, 0, -1, 0, NULL);
						icon->SetRot(rot, 0, t, 65542, 0, 0.0f, NULL, NULL);
					}
				}
			}
		}
		if (m_reloadError != 0) {
			typedef int (*IsActiveFn)(void);
			if ((*(IsActiveFn *)((char *)getTopmenuIf() + 196))() != 0) {
				unsigned char dlg[200];
				SceCommonGuiDialog_696DB9C9(dlg);
				*(int *)(dlg + 48) = 33;
				*(int *)(dlg + 68) = m_reloadError;
				SceCommonGuiDialog_20B141F1(dlg);
			}
		}
		m_reloadError = 0;
	}
}

/* 0x811AB8F2 */
int LiveAreaPageInner::GetReloadCount()
{
	return m_reloadCount;
}

/* 0x811AB8F8 */
int LiveAreaPageInner::IsIntroTimerDone()
{
	return m_introDone ? 1 : 0;
}

/* 0x811AB944 */
void LiveAreaPageInner::ClearGateInitFlag()
{
	m_gateResetDone = false;
}

/* 0x811AB94C */
void LiveAreaPageInner::SetDirtyFlag()
{
	m_dirty = true;
}

/* 0x811AB954 */
void LiveAreaPageInner::UpdateInitInfo(const LiveAreaPageInitInfo *info)
{
	m_style = info->style;
	m_field08 = info->field4;
	m_field0C = info->field8;
	m_field10 = info->field12;
	m_title = info->title;
	m_searchKey = info->searchKey;
	m_url = info->url;
	m_appAttr = info->appAttr;
	m_kind = info->kind;
	m_hasInfo = info->hasInfo;
	m_hasExt = info->hasExt;
	m_hasFeed = info->hasFeed;
	m_flag43 = info->flag3F;
}

/* 0x811AB9B8 */
int LiveAreaPageInner::GetResetMask()
{
	if (m_xmlDoc.get() == NULL)
		return 0;
	return 0;
}

/* 0x811ABA06 */
int LiveAreaPageInner::HasPatchIcon()
{
	if (m_contentZone == NULL)
		return 0;
	paf::IDParam barId(1728383896U);
	paf::ui::Widget *bar = m_contentZone->FindChild(barId, 0);
	if (bar != NULL && bar->FindChild(s_topIconIds[0], 0) != NULL)
		return 1;
	return 0;
}

/* 0x811ABE1C */
static void liveareaOnPatchClick(int a1, int a2, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a2; (void)a3;
	typedef void (*Fn)(int);
	(*(Fn *)((char *)getTopmenuIf() + 60))(*(int *)self->m_page);
}

/* 0x811ABE34 */
static void liveareaOnSearchClick(int a1, int a2, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a2; (void)a3;
	typedef int (*CheckFn)(int);
	if ((*(CheckFn *)((char *)getTopmenuIf() + 160))(*(int *)self->m_page) != 0) {
		char uri[1024];
		sce_paf_snprintf(uri, sizeof(uri), "%s%s", "search:", self->m_searchKey.c_str());
		sceAppMgrLaunchAppByUri(0x20000, uri);
	}
}

/* 0x811ABEA4 */
static void liveareaOnOnlineHelpClick(int a1, int a2, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a2; (void)a3;
	typedef int (*CheckFn)(int);
	if ((*(CheckFn *)((char *)getTopmenuIf() + 160))(*(int *)self->m_page) != 0)
		sceAppMgrLaunchAppByUri(0x20000, self->m_helpUri.c_str());
}

/* 0x811ABECE */
static void liveareaOnHelpClick(int a1, int a2, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a2; (void)a3;
	typedef void (*Fn)(int);
	(*(Fn *)((char *)getTopmenuIf() + 56))(*(int *)self->m_page);
}

/* 0x811ABEE6 */
static void liveareaOnVoucherClick(int a1, int a2, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a2; (void)a3;
	typedef int (*CheckFn)(int);
	if ((*(CheckFn *)((char *)getTopmenuIf() + 160))(*(int *)self->m_page) != 0)
		sceAppMgrLaunchAppByUri(0x20000, "psns:redeem");
}

/* 0x811ABF14 */
static void liveareaOnReloadClick(int a1, int a2, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a2; (void)a3;
	if (self->m_reloadCount <= 0 && self->m_xmlDoc.get() != NULL)
		liveareaMgrUpdatePage(self, 1);
}

/* 0x811ABF2E */
static void liveareaOnImportSaveClick(int a1, int a2, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a2; (void)a3;
	typedef void (*Fn)(int);
	(*(Fn *)((char *)getTopmenuIf() + 64))(*(int *)self->m_page);
}

/* 0x811ABF46 */
static void liveareaOnOnlineStorageClick(int a1, int a2, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a2; (void)a3;
	typedef void (*Fn)(int);
	(*(Fn *)((char *)getTopmenuIf() + 68))(*(int *)self->m_page);
}

/* 0x811ABF5E */
static void liveareaOnItemUriClick(int a1, int a2, int a3, void *item)
{
	(void)a1; (void)a2; (void)a3;
	if (item != NULL) {
		void *target = *((void **)item + 6);
		if (target != NULL && *((int *)target + 3) != 0) {
			paf::string uri;
			liveareaReplaceOnlineId(uri, *(const paf::string *)((const char *)target + 8));
			sceAppMgrLaunchAppByUri(0x20000, uri.c_str());
		}
	}
}

/* 0x811ABFB8 */
static void liveareaOnItemTrackedUriClick(int a1, int a2, int a3, void *item)
{
	(void)a1; (void)a2; (void)a3;
	if (item != NULL) {
		void *target = *((void **)item + 6);
		if (target != NULL && *((int *)target + 3) != 0) {
			paf::string uri;
			liveareaReplaceOnlineId(uri, *(const paf::string *)((const char *)target + 8));
			const char *u = uri.c_str();
			if (u != NULL) {
				if (sce_paf_strncmp("http:", u, 5) == 0 ||
				    sce_paf_strncmp("https:", u, 5) == 0 ||
				    sce_paf_strncmp("webmodal:", u, 9) == 0)
					shellRecordMetric(228, 1);
				else if (sce_paf_strncmp("psns:", u, 5) == 0 ||
					 sce_paf_strncmp("psts:", u, 5) == 0)
					shellRecordMetric(227, 1);
				else if (sce_paf_strncmp("psgm:", u, 5) == 0)
					shellRecordMetric(229, 1);
			}
			sceAppMgrLaunchAppByUri(0x20000, u);
		}
	}
}

/* 0x811AC0F4 */
static void liveareaOnItemAppClick(int a1, paf::ui::Widget *w, int a3, void *item)
{
	(void)a1; (void)a3;
	if (item != NULL) {
		void *target = *((void **)item + 6);
		if (target != NULL && *((int *)target + 3) != 0) {
			paf::string param;
			const char *appName = *(const char **)((const char *)w + 340);
			liveareaReplaceOnlineId(param, *(const paf::string *)((const char *)target + 8));
			sceAppMgrLaunchAppByNameForShell(0x20000, appName, param.c_str());
		}
	}
}

/* 0x811AC156 */
static void liveareaOnItemPslaClick(int a1, paf::ui::Widget *w, int a3, void *item)
{
	(void)a1; (void)a3;
	if (item != NULL) {
		void *target = *((void **)item + 6);
		if (target != NULL && *((int *)target + 3) != 0) {
			const char *appName = *(const char **)((const char *)w + 340);
			paf::string param(*(const paf::string *)((const char *)target + 8));
			size_t pos = param.find("psla:");
			if (pos != paf::string::npos)
				param.erase(pos, 5);
			sceAppMgrLaunchAppByNameForShell(0x20000, appName, param.c_str());
		}
	}
}

/* 0x811AC23E */
static void liveareaOnGateClick(int a1, int a2, int a3, int appId)
{
	(void)a1; (void)a2; (void)a3;
	typedef void (*Fn)(int);
	(*(Fn *)((char *)getTopmenuIf() + 156))(appId);
}

/* 0x811AC8B6 */
static void liveareaOnIntroTimer(int a1, paf::ui::Handler *h, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a3;
	self->m_introDone = true;
	h->KillTimer(16777507);
	typedef void (*Fn)(int);
	(*(Fn *)((char *)getTopmenuIf() + 176))(*(int *)self->m_page);
	self->ResetGateFocus();
}

/* 0x811AC8EC */
static void liveareaOnFocusTimer(int a1, paf::ui::Handler *h, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a3;
	liveareaPageOnFocusComplete(self->m_page);
	h->KillTimer(16777509);
}

/* 0x811AC906 */
static void liveareaOnBgClearTimer(int a1, paf::ui::Handler *h, int a3, LiveAreaPageInner *self)
{
	(void)a1; (void)a3;
	if (self->m_initPhase > 0) {
		--self->m_initPhase;
		if (self->m_initPhase == 0)
			self->BuildLayout(-1);
	}
	h->KillTimer(16777508);
}

/* 0x811ACBAE */
static bool liveareaCompareUint(const unsigned int *a, const unsigned int *b)
{
	return *a < *b;
}
