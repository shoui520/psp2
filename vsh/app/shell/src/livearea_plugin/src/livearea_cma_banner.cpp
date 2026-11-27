/*
 * SceShell livearea_plugin: Content Manager (NPXS10026) LiveArea banner
 * 3.65-CEX: 0x811AD166 - 0x811AE858 (sti 0x811AE7E8)
 */
#include <std/stdio.h>
#include <std/string.h>
#include <appmgr.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/math.h>
#include <timer/timer.h>
#include <widget/core/handler.h>
#include <widget/w_button.h>
#include <widget/w_image_button.h>
#include "livearea_internal.h"

extern "C" {
extern int sceRegMgrSrvGetRegion(void);
extern int sceAppMgrLaunchAppByNameForShell(int flags, const char *titleId, const char *param);
}

typedef void (*LiveAreaShellEventCallback)(int code, const void *payload, SceSize size, void *arg);
extern int shellAppEventServerAddListener(int event, LiveAreaShellEventCallback callback, void *arg);
extern int shellAppEventServerRemoveListener(int event, LiveAreaShellEventCallback callback, void *arg);

static float s_cmaBarMaxWidth = 0.0f;				/* 0x81544D94 */
static SceUInt32 s_cmaBarCurWidth = 0;				/* 0x81544D98 */
static SceUInt8 s_cmaStorageHooked = 0;				/* 0x81544D9C */

static const int s_cmaTexObjIds[2] = { 0, 0x44 };		/* 0x81511818 */

class LiveAreaCmaGetSizeJob : public paf::job::JobItem {
public:
	typedef void (*Callback)(int res, char showBar, SceInt64 total, SceInt64 freeSize);

	LiveAreaCmaGetSizeJob(Callback callback, char showBar)
		: paf::job::JobItem("Job_GetSize", NULL),
		  m_callback(callback), m_showBar(showBar), m_total(0), m_free(0)
	{}

	virtual ~LiveAreaCmaGetSizeJob() {}
	virtual int Run() { return 0; }
	virtual void Finish(int) {}

	Callback m_callback;
	char m_showBar;
	SceInt64 m_total;
	SceInt64 m_free;
};

static int liveareaCmaOnFocusCb(void);
static int liveareaCmaOnUnfocusCb(int eventType, int pageKey);
static int liveareaCmaOnStorageEvent(int eventType, const char *titleId);
static void liveareaCmaResetGlobals(paf::ui::Widget *widget, void *userdata);
static void liveareaCmaOnButtonPress(SceInt32 ev, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata);
static void liveareaCmaOnTextLayout(SceInt32 ev, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userdata);

static paf::IDParam makeHashId(SceUInt32 hash)
{
	return paf::IDParam(hash);
}

static paf::ui::Widget *findTypedImageButton(paf::ui::Widget *parent, SceUInt32 hash)
{
	paf::ui::Widget *widget = parent->FindChild(makeHashId(hash), 0);
	if (widget != NULL && widget->IsInherit(paf::ui::ImageButton::TypeName()))
		return widget;
	return NULL;
}

static paf::ui::Widget *findTypedButton(paf::ui::Widget *parent, SceUInt32 hash)
{
	paf::ui::Widget *widget = parent->FindChild(makeHashId(hash), 0);
	if (widget != NULL && widget->IsInherit(paf::ui::Button::TypeName()))
		return widget;
	return NULL;
}

/* 0x811AD166 */
int liveareaCmaSetActive(int active, int pageKey)
{
	LiveAreaTopmenuInterface *topmenuIf = g_liveareaMgr->m_topmenuIf;
	SceUInt8 want = (SceUInt8)active;
	int res = want;
	if (topmenuIf == NULL)
		return res;

	int targetKey = pageKey;
	if (targetKey == -1) {
		res = topmenuIf->findPageKeyByTitleId("NPXS10026");
		targetKey = res;
	}
	if (targetKey >= 0) {
		res = s_cmaStorageHooked;
		if (s_cmaStorageHooked != want) {
			int hookRes;
			if (want != 0) {
				topmenuIf->addPageShowListener(targetKey, (void *)liveareaCmaOnFocusCb, NULL);
				hookRes = shellAppEventServerAddListener(
					2, reinterpret_cast<LiveAreaShellEventCallback>(liveareaCmaOnStorageEvent), NULL);
			} else {
				topmenuIf->addPageShowListener(targetKey, NULL, NULL);
				hookRes = shellAppEventServerRemoveListener(
					2, reinterpret_cast<LiveAreaShellEventCallback>(liveareaCmaOnStorageEvent), NULL);
			}
			if (hookRes >= 0) {
				if (want != 0)
					topmenuIf->addPageEventListener(targetKey, (void *)liveareaCmaOnUnfocusCb, NULL);
				else
					topmenuIf->removePageEventListener(targetKey, (void *)liveareaCmaOnUnfocusCb, NULL);
			}
			s_cmaStorageHooked = want;
		}
	}
	return res;
}

/* 0x811AD216 */
void liveareaCmaShowBar(void)
{
	LiveAreaTopmenuInterface *topmenuIf = g_liveareaMgr->m_topmenuIf;
	if (topmenuIf == NULL)
		return;
	int pageKey = topmenuIf->findPageKeyByTitleId("NPXS10026");
	if (pageKey < 0)
		return;
	paf::ui::Widget *root = topmenuIf->getPageWidget(pageKey);
	if (root == NULL)
		return;

	paf::ui::Widget *storageBtn = findTypedImageButton(root, 1170604782U);
	if (storageBtn == NULL)
		return;

	paf::ui::Widget *bar = storageBtn->FindChild(makeHashId(651034106U), 0);
	if (bar != NULL)
		bar->Show(0xff, NULL, NULL, 0.0f);

	paf::graph::DrawObj *barBg = storageBtn->GetDrawObj(3);
	if (barBg != NULL && !barBg->m_flags.m_visible)
		barBg->m_flags.m_visible = true;
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
		SceUInt32 barWidth = (SceUInt32)(s_cmaBarMaxWidth * ratio);
		if (barWidth < 5 && total != freeSize)
			barWidth = 5;
		if (s_cmaBarCurWidth != barWidth) {
			paf::IDParam barId = makeHashId(651034106U);
			LiveAreaTopmenuInterface *topmenuIf = g_liveareaMgr->m_topmenuIf;
			paf::ui::Widget *bar = NULL;
			if (topmenuIf != NULL) {
				int pageKey = topmenuIf->findPageKeyByTitleId("NPXS10026");
				if (pageKey >= 0) {
					paf::ui::Widget *root = topmenuIf->getPageWidget(pageKey);
					if (root != NULL)
						bar = root->FindChild(barId, 0);
				}
			}
			if (bar != NULL) {
				const paf::math::v4 *curSize = bar->GetSize(0);
				paf::math::v4 nextSize((float)barWidth, curSize->extract_y(),
						       curSize->extract_z(), curSize->extract_w());
				bar->SetSize(nextSize, NULL, 65540, 0, 0.0f, NULL, NULL);
				s_cmaBarCurWidth = barWidth;
			}
		}
	}
}

/* 0x811AD6A2 */
int liveareaCmaRequestSize(char showBar)
{
	paf::job::JobQueue *queue = paf::job::JobQueue::default_queue;
	if (queue == NULL)
		return -2;
	LiveAreaCmaGetSizeJob *job = new LiveAreaCmaGetSizeJob(liveareaCmaOnGetSize, showBar);
	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);
	if (jobRef.get() == NULL)
		return -2;
	return queue->Enqueue(jobRef);
}

/* 0x811AD7B8 */
void liveareaCmaAnimatePopIn(paf::ui::Widget *widget)
{
	if (widget == NULL)
		return;
	widget->Hide(0xff, NULL, NULL, 0.0f);
	paf::Timer *fadeTimer = new paf::Timer();
	if (fadeTimer != NULL)
		fadeTimer->InitTimer(200.0f, 21, 0, -1, NULL);
	widget->Show(0xff, fadeTimer, NULL, 0.0f);

	paf::math::v4 zeroScale(0.0f, 0.0f, 0.0f, 0.0f);
	widget->SetScale(zeroScale, NULL, 65541, 0, 0.0f, NULL, NULL);

	paf::math::v4 fullScale(1.0f, 1.0f, 0.0f, 0.0f);
	paf::Timer *scaleTimer = new paf::Timer();
	if (scaleTimer != NULL)
		scaleTimer->InitTimer(200.0f, 32, 0, -1, NULL);
	widget->SetScale(fullScale, scaleTimer, 65541, 2, 0.0f, NULL, NULL);
}

/* 0x811AD966 */
void liveareaCmaSetup(paf::Plugin *plugin, paf::ui::Widget *root)
{
	if (root == NULL)
		return;

	root->AddDeleteCB(liveareaCmaResetGlobals, NULL);

	paf::intrusive_ptr<paf::graph::Surface> regionTexture;
	if (sceRegMgrSrvGetRegion() == 13 && plugin != NULL) {
		paf::IDParam texId = makeHashId(379628764U);
		regionTexture = plugin->GetTexture(texId);
	}

	paf::ui::Widget *storageFrame = root->FindChild(makeHashId(260879598U), 0);
	if (storageFrame != NULL) {
		paf::ui::Widget *storageBtn = findTypedImageButton(storageFrame, 1170604782U);
		if (storageBtn != NULL) {
			paf::ui::EventCBListener *pressListener =
				new paf::ui::EventCBListener(liveareaCmaOnButtonPress, (void *)1);
			storageBtn->AddEventListener(268435464, pressListener, 0);

			paf::ui::EventCBListener *layoutListener =
				new paf::ui::EventCBListener(liveareaCmaOnTextLayout, (void *)2);
			storageBtn->AddEventListener(1048580, layoutListener, 0);

			paf::graph::DrawObj *barBg = storageBtn->GetDrawObj(3);
			if (barBg != NULL) {
				s_cmaBarMaxWidth = barBg->m_size.extract_x();
				if (barBg->m_flags.m_visible)
					barBg->m_flags.m_visible = false;
			}

			paf::ui::Widget *bar = storageBtn->FindChild(makeHashId(651034106U), 0);
			if (bar != NULL) {
				bar->Hide(0xff, NULL, NULL, 0.0f);
				liveareaCmaRequestSize(1);
			}
			liveareaCmaAnimatePopIn(storageBtn);
		}
	}

	paf::ui::Widget *connectFrame = root->FindChild(makeHashId(71033373U), 0);
	if (connectFrame != NULL) {
		paf::ui::Widget *connectBtn = findTypedButton(connectFrame, (SceUInt32)-1669094780);
		if (connectBtn != NULL) {
			connectBtn->Hide(0xff, NULL, NULL, 0.0f);
			if (regionTexture.get() != NULL) {
				for (SceUInt32 i = 0; i < 2; ++i) {
					paf::graph::DrawObj *drawObj =
						connectBtn->GetDrawObj(s_cmaTexObjIds[i]);
					if (drawObj != NULL)
						drawObj->SetTexture(regionTexture, 0);
				}
			}

			paf::ui::EventCBListener *pressListener =
				new paf::ui::EventCBListener(liveareaCmaOnButtonPress, NULL);
			connectBtn->AddEventListener(268435464, pressListener, 0);

			paf::ui::EventCBListener *layoutListener =
				new paf::ui::EventCBListener(liveareaCmaOnTextLayout, (void *)1);
			connectBtn->AddEventListener(1048580, layoutListener, 0);

			liveareaCmaAnimatePopIn(connectBtn);
		}
	}
}

/* 0x811AE0A4 */
void liveareaCmaCleanup(paf::ui::Widget *root)
{
	if (root == NULL)
		return;

	root->RemoveDeleteCB(liveareaCmaResetGlobals, NULL);

	paf::ui::Widget *storageFrame = root->FindChild(makeHashId(260879598U), 0);
	if (storageFrame != NULL) {
		s_cmaBarMaxWidth = 0.0f;
		s_cmaBarCurWidth = 0;
		paf::ui::Widget *storageBtn = findTypedImageButton(storageFrame, 1170604782U);
		if (storageBtn != NULL) {
			storageBtn->DeleteEventCallback(268435464, NULL, NULL);
			storageBtn->DeleteEventCallback(1048580, NULL, NULL);
		}
		storageFrame->m_name = paf::IDParam("");
		storageFrame->DestroyWidget();
	}

	paf::ui::Widget *connectFrame = root->FindChild(makeHashId(71033373U), 0);
	if (connectFrame != NULL) {
		paf::ui::Widget *connectBtn = findTypedButton(connectFrame, (SceUInt32)-1669094780);
		if (connectBtn != NULL) {
			connectBtn->DeleteEventCallback(268435464, NULL, NULL);
			connectBtn->DeleteEventCallback(1048580, NULL, (void *)1);
		}
		connectFrame->m_name = paf::IDParam("");
		connectFrame->DestroyWidget();
	}
}

/* 0x811AE5A0 */
static void liveareaCmaResetGlobals(paf::ui::Widget *widget, void *userdata)
{
	(void)widget;
	(void)userdata;
	s_cmaBarMaxWidth = 0.0f;
	s_cmaBarCurWidth = 0;
}

/* 0x811AE5B8 */
static void liveareaCmaOnButtonPress(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *userdata)
{
	int actionKind = (int)userdata;
	paf::string launchParam;
	paf::string uri;
	if (actionKind == 1)
		uri = "psla:mc_mgmt";
	else if (actionKind == 0)
		uri = "psla:hostconnect";
	liveareaReplaceOnlineId(launchParam, uri);
	sceAppMgrLaunchAppByNameForShell(0x20000, "NPXS10026", launchParam.c_str());
}

/* 0x811AE680 */
static void liveareaCmaOnTextLayout(SceInt32, paf::ui::Handler *handler,
				    paf::ui::Event *, void *userdata)
{
	if (handler == NULL)
		return;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	paf::graph::DrawObj *drawObj = widget->GetDrawObj(1);
	if (drawObj == NULL || !drawObj->IsInherit(paf::graph::TextObj::DrawObjType()))
		return;

	paf::graph::TextObj *textObj = (paf::graph::TextObj *)drawObj;
	if (textObj->m_flags.m_visible)
		textObj->m_flags.m_visible = false;

	uint32_t lines = 0;
	textObj->GetLineCount(lines);
	if (lines > (uint32_t)userdata) {
		paf::wstring str;
		textObj->GetString(str);
		textObj->SetStyleAttribute((paf::graph::TextStyleAttribute)1, 0, str.size(), false);
		paf::math::v3 expandedSize(textObj->m_size.extract_x() + 4.0f,
					   textObj->m_size.extract_y(),
					   textObj->m_size.extract_z());
		textObj->SetSize(expandedSize);
		textObj->Update();
	}
	if (!textObj->m_flags.m_visible)
		textObj->m_flags.m_visible = true;
}

/* 0x811AE7B4 */
static int liveareaCmaOnFocusCb(void)
{
	return liveareaCmaRequestSize(0);
}

/* 0x811AE7BE */
static int liveareaCmaOnUnfocusCb(int, int pageKey)
{
	return liveareaCmaSetActive(0, pageKey);
}

/* 0x811AE7C8 */
static int liveareaCmaOnStorageEvent(int, const char *titleId)
{
	int cmp = sce_paf_strncmp(titleId, "NPXS10026", 10);
	if (cmp == 0)
		return liveareaCmaRequestSize(0);
	return cmp;
}
