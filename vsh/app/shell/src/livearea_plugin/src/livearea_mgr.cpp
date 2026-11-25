/*
 * livearea_mgr.cpp: livearea_plugin main manager & interface callbacks
 * (0x8119A698-0x8119B594, sti 0x8119B522). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <plugin.h>
#include <widget/core/widget.h>
#include <graphics/surface/surface.h>
#include <common/main_thread_call_list.h>
#include <common/shared_ptr.h>
#include <system/system.h>
#include <appmgr.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../livespace_db/livespace_db.h"

extern void *g_liveareaInterface;				/* 0x81566F90 */
extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */
extern void *shellPluginGetInterface(const char *name, int id); /* 0x810C9D14 */

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
int SceShell_7FE4E732(int id, void *cb, void *arg);
int SceShell_D4C7D2B7(int id, void *cb, void *arg);
int SceActivityDb_F138D4E5(void);
int SceLsdb_B51740A6(void *ctx, int a1, int a2, unsigned char a3);
int SceLsdb_DA5DC77C(void *ctx, void *vec, paf::string *outName);
int SceLsdb_C87397C0(paf::string *name);
const char *sceLsdbGetName(void *item);
int SceLsdb_C070E42E(void *item);
int SceLsdb_C875C57A(paf::string *name, int val);
}

extern void liveareaUpdaterCtor(void *u);			/* 0x811A2910 */
extern void liveareaUpdaterDtor(void *u);			/* 0x811A292C */
extern void liveareaUpdaterStart(void *u);			/* 0x811A2940 */
extern void liveareaUpdaterStop(void *u);			/* 0x811A294A */
extern int liveareaUpdaterSub954(void *u, void *out, const char *title, SceInt64 *ts, int mode, void *cb, int appId); /* 0x811A2954 */
extern int liveareaUpdaterSub972(void *u, void *out, int a2, int a3, int a4, int a5); /* 0x811A2972 */
extern int liveareaUpdaterSub9C0(void *u, void *out, const char *title, int mode, void *arg, void *cb, int appId); /* 0x811A29C0 */
extern int liveareaUpdaterSub9F0(void *u, void *out, const char *title, int mode, void *cb, int appId); /* 0x811A29F0 */
extern int liveareaNetRegisterCb(void (*cb)(int), void *arg);	/* 0x8132781C */
extern void liveareaNetUnregisterCb(int id);			/* 0x81327896 */
extern int liveareaNetGetState(int *out);			/* 0x81326F8A */
extern int liveareaNetGetOnlineId(char *out);			/* 0x81326D40 */
extern void liveareaCb20EF44(void);				/* 0x8120EF44 */
extern void liveareaSub34A716(void);				/* 0x8134A716 */
extern int liveareaLaunchWithArg(const char *titleId, int arg); /* 0x811A064C */

extern void *liveareaPageCtor(void *mem, int appId, const char *titleId, int a3, int a4); /* 0x8119B594 */
extern const char *liveareaPageGetTitleId(void *page);		/* 0x8119B990 */
extern int liveareaPageScrollToContent(void *page);		/* 0x8119BD0E */
extern void liveareaPageResetScroll(void *page, bool anim);	/* 0x8119BD32 */
extern bool liveareaPageIsAtTop(void *page);			/* 0x8119BE16 */
extern void liveareaPageSubC064(void *page, int arg);		/* 0x8119C064 */
extern int liveareaPageSubC06E(void *page);			/* 0x8119C06E */
extern void *liveareaPageFindByAppId(int appId);		/* 0x8119C0C6 */
extern void *liveareaPageFindByTitleId(const char *titleId);	/* 0x8119C12E */
extern void liveareaPageHideAllFeeds(void);			/* 0x8119C17C */
extern void liveareaPageOnEvent4(void);				/* 0x8119C1BC */
extern void liveareaPageOnEvent5(int appId);			/* 0x8119C234 */
extern void liveareaPageTickAll(bool changed);			/* 0x8119C312 */
extern int liveareaPageIfEntry5(const char *titleId, int a2, int a3); /* 0x8119C37E */
extern void liveareaPageIfEntry7(void);				/* 0x8119C406 */
extern void liveareaPageIfEntry8(void);				/* 0x8119C4F2 */
extern int liveareaFeedRequestUpdate(void *feed, int mode);	/* 0x8119C964 */

extern int liveareaPageInnerSubAA854(void *inner, int a2);	/* 0x811AA854 */
extern int liveareaPageInnerSubAA9F2(void *inner);		/* 0x811AA9F2 */
extern int liveareaPageInnerSubAAE02(void *inner, void *a2);	/* 0x811AAE02 */
extern int liveareaPageInnerSubAAE14(void *inner, int *a2, int *a3); /* 0x811AAE14 */
extern void liveareaPageInnerSubAB38A(void *inner);		/* 0x811AB38A */
extern void liveareaPageInnerSubAB5DA(void *inner, int a1);	/* 0x811AB5DA */
extern int liveareaPageInnerSubAB8F2(void *inner);		/* 0x811AB8F2 */
extern void liveareaPageInnerSubAB944(void *inner);		/* 0x811AB944 */
extern void liveareaPageInnerSubAB94C(void *inner);		/* 0x811AB94C */
extern void *liveareaPageInnerSubAB9B8(void *inner);		/* 0x811AB9B8 */
extern paf::common::SharedPtr<int> liveareaSubA3426(void *inner, void *arg); /* 0x811A3426 */

/* Forward declarations for interface table callbacks */
int liveareaIfCreatePage(int appId, const char *titleId, int a3, int a4); /* 0x8119AD92 */
int liveareaIfEntry1(int appId, int a2);			/* 0x8119ADBA */
int liveareaIfEntry2(int appId);				/* 0x8119ADD0 */
int liveareaIfEntry3(int appId, void *a2);			/* 0x8119ADE2 */
void liveareaIfEntry15(int appId);				/* 0x8119ADFC */
void liveareaIfEntry16(int appId);				/* 0x8119AE90 */
int liveareaIfEntry17(int appId);				/* 0x8119AEA2 */
int liveareaIfEntry4(int appId);				/* 0x8119AEB6 */
int liveareaIfEntry6(int appId, int *a2, int *a3);		/* 0x8119AECA */
paf::ui::Widget *liveareaIfEntry9(paf::ui::Widget *widget);	/* 0x8119AEF4 */
int liveareaIfEntry10(int a1, int a2, int a3, int a4);		/* 0x8119B0B0 */
int liveareaIfEntry11(const char *titleId, int a2);		/* 0x8119B110 */
void liveareaIfEntry12(int appId);				/* 0x8119B18E */
void liveareaIfEntry13(int appId, int a2);			/* 0x8119B1E0 */
void liveareaIfEntry14(int appId);				/* 0x8119B1F4 */
int liveareaIfEntry18(int appId);				/* 0x8119B20E */
int liveareaIfEntry19(void *item, bool force, bool flag);	/* 0x8119B222 */
int liveareaIfEntry20(paf::string *name);			/* 0x8119B4C8 */
void liveareaMgrOnUpdaterDone(int a1, int a2);			/* 0x8119B4EC */
void liveareaMgrOnNetEvent(int event);				/* 0x8119B506 */
void liveareaMgrOnShellEvent4(void);				/* 0x8119B510 */
void liveareaMgrOnShellEvent5(int a1, int a2, int a3);		/* 0x8119B518 */
void liveareaMgrMainThreadTick(void *userdata);			/* 0x8119A8EA */

struct LiveAreaMgr {
	LiveAreaMgr(paf::Plugin *plugin);			/* 0x8119A698 */

	paf::Plugin *m_plugin;					/* +0x00 */
	void *m_topmenuIf;					/* +0x04 */
	void *m_gameIf;						/* +0x08 */
	void *m_applauncherIf;					/* +0x0C */
	int m_netState;						/* +0x10 */
	int m_signedInState;					/* +0x14 */
	float m_pollTimer;					/* +0x18 */
	paf::string m_pendingLaunchTitle;			/* +0x1C */
	int m_currentAppId;					/* +0x28 */
	int m_lsdbCookie;					/* +0x2C */
	int m_updater;						/* +0x30 */
	int m_netCbId;						/* +0x34 */
};

LiveAreaMgr *g_liveareaMgr;					/* 0x815670A8 */

/* 0x8119A698 */
LiveAreaMgr::LiveAreaMgr(paf::Plugin *plugin)
	: m_plugin(plugin),
	  m_topmenuIf(shellPluginGetInterface("topmenu_plugin", 1)),
	  m_gameIf(shellPluginGetInterface("game_plugin", 1)),
	  m_applauncherIf(shellPluginGetInterface("applauncher_plugin", 1)),
	  m_netState(0),
	  m_signedInState(0),
	  m_pollTimer(0.0f),
	  m_pendingLaunchTitle(),
	  m_currentAppId(-1),
	  m_lsdbCookie(0),
	  m_updater(0),
	  m_netCbId(-1)
{
	liveareaUpdaterCtor(&m_updater);
	g_liveareaMgr = this;

	void **iface = (void **)g_liveareaInterface;
	iface[0] = (void *)liveareaIfCreatePage;
	iface[1] = (void *)liveareaIfEntry1;
	iface[2] = (void *)liveareaIfEntry2;
	iface[3] = (void *)liveareaIfEntry3;
	iface[16] = (void *)liveareaIfEntry16;
	iface[19] = (void *)liveareaIfEntry19;
	iface[20] = (void *)liveareaIfEntry20;
	iface[17] = (void *)liveareaIfEntry17;
	iface[18] = (void *)liveareaIfEntry18;
	iface[15] = (void *)liveareaIfEntry15;
	iface[4] = (void *)liveareaIfEntry4;
	iface[6] = (void *)liveareaIfEntry6;
	iface[5] = (void *)liveareaPageIfEntry5;
	iface[7] = (void *)liveareaPageIfEntry7;
	iface[8] = (void *)liveareaPageIfEntry8;
	iface[9] = (void *)liveareaIfEntry9;
	iface[10] = (void *)liveareaIfEntry10;
	iface[11] = (void *)liveareaIfEntry11;
	iface[12] = (void *)liveareaIfEntry12;
	iface[13] = (void *)liveareaIfEntry13;
	iface[14] = (void *)liveareaIfEntry14;

	liveareaUpdaterStart(&m_updater);
	m_netCbId = liveareaNetRegisterCb(liveareaMgrOnNetEvent, NULL);
	SceShell_7FE4E732(4, (void *)liveareaMgrOnShellEvent4, NULL);
	SceShell_7FE4E732(5, (void *)liveareaMgrOnShellEvent5, NULL);
}

/* 0x8119A83C */
LiveAreaMgr *liveareaMgrCreate(paf::Plugin *plugin)
{
	return new LiveAreaMgr(plugin);
}

/* 0x8119A852 */
void liveareaMgrDestroy(void)
{
	LiveAreaMgr *mgr = g_liveareaMgr;
	if (mgr != NULL) {
		SceShell_D4C7D2B7(4, (void *)liveareaMgrOnShellEvent5, NULL);
		SceShell_D4C7D2B7(4, (void *)liveareaMgrOnShellEvent4, NULL);
		liveareaNetUnregisterCb(mgr->m_netCbId);
		liveareaUpdaterStop(&mgr->m_updater);
		g_liveareaMgr = NULL;
		liveareaUpdaterDtor(&mgr->m_updater);
		delete mgr;
	}
}

/* 0x8119A8C6 */
void liveareaMgrStart(void)
{
	paf::common::MainThreadCallList::Register(liveareaMgrMainThreadTick, NULL);
}

/* 0x8119A8D8 */
void liveareaMgrStop(void)
{
	paf::common::MainThreadCallList::Unregister(liveareaMgrMainThreadTick, NULL);
}

/* 0x8119A8EA */
void liveareaMgrMainThreadTick(void *userdata)
{
	(void)userdata;
	LiveAreaMgr *mgr = g_liveareaMgr;
	int prevNet = mgr->m_netState;
	typedef int (*GetNetStateFn)(void);
	int curNet = (*(GetNetStateFn *)((char *)mgr->m_topmenuIf + 336))();
	mgr->m_netState = curNet;

	bool changed = (prevNet != curNet);
	bool poll = changed;
	mgr->m_pollTimer += (1.0f / 60.0f);
	if (mgr->m_pollTimer > 60.0f)
		poll = true;

	if (poll) {
		mgr->m_pollTimer = 0.0f;
		int signedIn = 0;
		if (liveareaNetGetState(&signedIn) == 0) {
			if (mgr->m_signedInState != signedIn) {
				changed = true;
				mgr->m_signedInState = signedIn;
			}
		}
	}
	liveareaPageTickAll(changed);
}

/* 0x8119A99C */
int liveareaMgrLsdbCall(int a1, int a2, unsigned char a3)
{
	return SceLsdb_B51740A6(&g_liveareaMgr->m_lsdbCookie, a1, a2, a3);
}

/* 0x8119A9BA */
void liveareaMgrUpdatePage(void **pageInner, int mode)
{
	if (pageInner[32] == NULL || liveareaPageInnerSubAB8F2(pageInner) > 0)
		return;

	const char *title = liveareaPageGetTitleId(pageInner[0]);
	SceInt64 ts = *((SceInt64 *)pageInner[32] + 6);
	void *cb = NULL;
	int appId = 0;
	bool checkType = false;
	bool notified = false;
	char outBuf[8];

	if (mode == 1) {
		cb = (void *)liveareaMgrOnUpdaterDone;
		appId = *(int *)pageInner[0];
	}

	if (*((unsigned char *)pageInner + 64) != 0) {
		if (mode != 0) {
			if (paf::system::Model::GetType() == 1)
				checkType = true;
		} else if (pageInner[15] != NULL) {
			goto check_flag65;
		}
		if (mode == 1) {
			notified = true;
			liveareaCb20EF44();
		}
		int r = liveareaUpdaterSub954(&g_liveareaMgr->m_updater, outBuf, title, &ts, mode, cb, appId);
		if (r == 0 && mode == 1)
			liveareaPageInnerSubAB38A(pageInner);
		if (checkType) {
			if (!notified) {
				notified = true;
				liveareaCb20EF44();
			}
			if (liveareaUpdaterSub9F0(&g_liveareaMgr->m_updater, outBuf, title, 1, cb, appId) == 0)
				liveareaPageInnerSubAB38A(pageInner);
		}
	}

check_flag65:
	if (*((unsigned char *)pageInner + 65) != 0) {
		if (!notified && mode == 1) {
			notified = true;
			liveareaCb20EF44();
		}
		if (liveareaUpdaterSub9C0(&g_liveareaMgr->m_updater, outBuf, title, mode, pageInner[3], cb, appId) == 0
		    && mode == 1)
			liveareaPageInnerSubAB38A(pageInner);
	}

	if (*((unsigned char *)pageInner + 66) != 0 && mode == 1) {
		if (!notified)
			liveareaCb20EF44();
		void *feed = *((void **)pageInner[0] + 62);
		if (feed != NULL && liveareaFeedRequestUpdate(feed, 1) == 0)
			liveareaPageInnerSubAB38A(pageInner);
	}
}

/* 0x8119AB34 */
void liveareaReplaceOnlineId(paf::string &out, const paf::string &in)
{
	static paf::string s_tag("<SCE_PSN_ONLINEID>");
	out = in;
	size_t pos = out.find(s_tag);
	if (pos != paf::string::npos) {
		char onlineId[20];
		if (liveareaNetGetOnlineId(onlineId) < 0)
			out.erase(pos, s_tag.size());
		else
			out.replace(pos, s_tag.size(), onlineId);
	}
}

/* 0x8119AC7A */
void liveareaMgrForcePoll(void)
{
	g_liveareaMgr->m_pollTimer += 60.0f;
}

/* 0x8119AC9C */
void liveareaMgrNotifyPage(const char *titleId, int arg)
{
	void *page = liveareaPageFindByTitleId(titleId);
	if (page != NULL) {
		struct {
			const char *title;
			int zero;
			int arg;
		} req;
		req.title = titleId;
		req.zero = 0;
		req.arg = arg;
		void *inner = (char *)page + 48;
		liveareaPageInnerSubAB94C(inner);
		liveareaPageInnerSubAAE02(inner, &req);
	}
}

/* 0x8119ACE4 */
void liveareaMgrClearPendingLaunch(void)
{
	g_liveareaMgr->m_pendingLaunchTitle.clear();
}

/* 0x8119ACFC */
bool liveareaMgrIsPendingLaunch(const char *titleId)
{
	if (g_liveareaMgr->m_pendingLaunchTitle.empty())
		return false;
	paf::string tmp(titleId);
	return tmp == g_liveareaMgr->m_pendingLaunchTitle;
}

/* 0x8119AD92 */
int liveareaIfCreatePage(int appId, const char *titleId, int a3, int a4)
{
	void *mem = operator new(252);
	if (mem != NULL)
		liveareaPageCtor(mem, appId, titleId, a3, a4);
	return 1;
}

/* 0x8119ADBA */
int liveareaIfEntry1(int appId, int a2)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL)
		return liveareaPageInnerSubAA854((char *)page + 48, a2);
	return 0;
}

/* 0x8119ADD0 */
int liveareaIfEntry2(int appId)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL)
		return liveareaPageInnerSubAA9F2((char *)page + 48);
	return 0;
}

/* 0x8119ADE2 */
int liveareaIfEntry3(int appId, void *a2)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL)
		return liveareaPageInnerSubAAE02((char *)page + 48, a2);
	return 0;
}

/* 0x8119ADFC */
void liveareaIfEntry15(int appId)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL) {
		void *inner = (char *)page + 48;
		void *arg = liveareaPageInnerSubAB9B8(inner);
		if (arg != NULL)
			paf::common::SharedPtr<int> sp = liveareaSubA3426(inner, arg);
	}
}

/* 0x8119AE90 */
void liveareaIfEntry16(int appId)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL)
		liveareaPageResetScroll(page, true);
}

/* 0x8119AEA2 */
int liveareaIfEntry17(int appId)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL)
		return liveareaPageIsAtTop(page) ? 1 : 0;
	return 1;
}

/* 0x8119AEB6 */
int liveareaIfEntry4(int appId)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL)
		return *((unsigned char *)page + 229);
	return 1;
}

/* 0x8119AECA */
int liveareaIfEntry6(int appId, int *a2, int *a3)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL)
		return liveareaPageInnerSubAAE14((char *)page + 48, a2, a3);
	*a2 = 0;
	*a3 = 1037226707;
	return 0;
}

/* 0x8119AEF4 */
paf::ui::Widget *liveareaIfEntry9(paf::ui::Widget *widget)
{
	paf::IDParam tmplId(0xF88DEE04U);
	paf::Plugin::TemplateOpenParam openParam;
	g_liveareaPlugin->TemplateOpen(widget, tmplId, openParam);

	paf::IDParam childId(0x5721D77BU);
	paf::ui::Widget *child = widget->FindChild(childId, 0);
	if (child != NULL) {
		paf::IDParam texId(0xE766F27FU);
		paf::intrusive_ptr<paf::graph::Surface> surf = g_liveareaPlugin->GetTexture(texId);
		typedef void (*SetTexFn)(paf::ui::Widget *, paf::intrusive_ptr<paf::graph::Surface> *);
		SetTexFn fn = (*(SetTexFn **)(*(void **)child))[0x100 / 4];
		fn(child, &surf);

		paf::math::v4 pos(0.0f, 384.0f, 0.0f, 0.0f);
		child->SetPos(pos, NULL, 0, 0x10000, 0.0f, NULL, NULL);
	}
	return child;
}

/* 0x8119B0B0 */
int liveareaIfEntry10(int a1, int a2, int a3, int a4)
{
	(void)a1;
	if (g_liveareaMgr == NULL)
		return -1;
	char outBuf[8];
	int act = SceActivityDb_F138D4E5();
	return liveareaUpdaterSub972(&g_liveareaMgr->m_updater, outBuf, act, a2, a3, a4);
}

/* 0x8119B110 */
int liveareaIfEntry11(const char *titleId, int a2)
{
	if (g_liveareaMgr == NULL)
		return (int)titleId;
	liveareaPageHideAllFeeds();
	if (a2 != 0)
		return liveareaLaunchWithArg(titleId, a2);

	void *page = liveareaPageFindByTitleId(titleId);
	if (page != NULL) {
		if (*((unsigned char *)page + 45) != 0 && *((void **)page + 62) != NULL)
			return liveareaPageScrollToContent(page);
		*((unsigned char *)page + 46) = 1;
	} else if (g_liveareaMgr != NULL) {
		g_liveareaMgr->m_pendingLaunchTitle = titleId;
	}
	return sceAppMgrLaunchAppByName(0, titleId, NULL);
}

/* 0x8119B18E */
void liveareaIfEntry12(int appId)
{
	LiveAreaMgr *mgr = g_liveareaMgr;
	if (mgr != NULL && mgr->m_currentAppId != appId) {
		void *page = liveareaPageFindByAppId(appId);
		if (page != NULL) {
			liveareaPageInnerSubAB944((char *)page + 48);
			if (liveareaMgrIsPendingLaunch(*(const char **)((char *)page + 4)))
				*((unsigned char *)page + 46) = 1;
			mgr->m_pendingLaunchTitle.clear();
		}
		mgr->m_currentAppId = appId;
	}
}

/* 0x8119B1E0 */
void liveareaIfEntry13(int appId, int a2)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL)
		liveareaPageSubC064(page, a2);
}

/* 0x8119B1F4 */
void liveareaIfEntry14(int appId)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL) {
		if (*((int *)page + 44) != 0)
			liveareaMgrUpdatePage((void **)((char *)page + 48), 0);
	}
}

/* 0x8119B20E */
int liveareaIfEntry18(int appId)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL)
		return liveareaPageSubC06E(page);
	return 0;
}

/* 0x8119B222 */
int liveareaIfEntry19(void *item, bool force, bool flag)
{
	LiveAreaMgr *mgr = g_liveareaMgr;
	/* Check attribute 0x5142196B bit 0x1000 on item */
	typedef int (*GameFn204)(void *, int);
	typedef int (*GameFn208)(paf::string *, int);
	typedef void (*TopmenuFn284)(paf::vector<paf::string> *);

	if (!force) {
		paf::vector<paf::string> titles;
		paf::string outName;
		(*(TopmenuFn284 *)((char *)mgr->m_topmenuIf + 284))(&titles);
		SceLsdb_DA5DC77C(&mgr->m_lsdbCookie, &titles, &outName);
		if (!outName.empty()) {
			int r = (*(GameFn208 *)((char *)mgr->m_gameIf + 208))(&outName, 1);
			if (r != 0)
				return r;
			r = SceLsdb_C87397C0(&outName);
			if (r != 0)
				return r;
		}
	}

	int mode = 0;
	if (force)
		mode = flag ? 2 : 1;
	int r = (*(GameFn204 *)((char *)mgr->m_gameIf + 204))(item, mode);
	if (r == 0 && !force) {
		const char *nm = sceLsdbGetName(item);
		paf::string nameStr(nm);
		int val = SceLsdb_C070E42E(item);
		r = SceLsdb_C875C57A(&nameStr, val);
	}
	return r;
}

/* 0x8119B4C8 */
int liveareaIfEntry20(paf::string *name)
{
	typedef void (*GameFn208)(paf::string *, int);
	(*(GameFn208 *)((char *)g_liveareaMgr->m_gameIf + 208))(name, 1);
	SceLsdb_C87397C0(name);
	return 0;
}

/* 0x8119B4EC */
void liveareaMgrOnUpdaterDone(int a1, int a2)
{
	void *page = liveareaPageFindByAppId(a2);
	if (page != NULL)
		liveareaPageInnerSubAB5DA((char *)page + 48, a1);
}

/* 0x8119B506 */
void liveareaMgrOnNetEvent(int event)
{
	if (event == 0)
		liveareaSub34A716();
}

/* 0x8119B510 */
void liveareaMgrOnShellEvent4(void)
{
	liveareaPageOnEvent4();
}

/* 0x8119B518 */
void liveareaMgrOnShellEvent5(int a1, int a2, int a3)
{
	(void)a1;
	(void)a2;
	liveareaPageOnEvent5(a3);
}
