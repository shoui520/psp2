/*
 * livearea_interface.h: public cross-plugin interface of SceShell's livearea_plugin.
 */
#ifndef LIVEAREA_INTERFACE_H
#define LIVEAREA_INTERFACE_H

#include <scetypes.h>
#include <std/string>
#include <cache/cache_image.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include "../../livespace_db/livespace_db_internal.h"

struct LiveAreaPageInitInfo {
	int style;						/* +0x00 */
	int manualMode;						/* +0x04 */
	int upgradeMode;					/* +0x08 */
	paf::cache::CacheImageInterface *gateIconCache;		/* +0x0C */
	paf::wstring title;					/* +0x10 */
	paf::string searchKey;					/* +0x1C */
	paf::string url;					/* +0x28 */
	int appAttr;						/* +0x34 */
	int kind;						/* +0x38 */
	bool hasInfo;						/* +0x3C */
	bool hasExt;						/* +0x3D */
	bool hasFeed;						/* +0x3E */
	bool hasOnlineManual;					/* +0x3F */
};

struct LiveAreaReloadRequest {
	const char *titleId;					/* +0x00 */
	const char *contentDir;					/* +0x04 */
	int reloadMask;						/* +0x08 */
};

struct LiveAreaPluginInterface {
	int (*createPage)(int pageKey, const char *titleId, const char *contentDir,
			  const LiveAreaPageInitInfo *info);			/* +0x00 */
	void (*setGateSurface)(int pageKey, paf::graph::Surface **surface);	/* +0x04 */
	void (*clearGateSurface)(int pageKey);					/* +0x08 */
	int (*requestReload)(int pageKey, const LiveAreaReloadRequest *req);	/* +0x0C */
	int (*isIntroDone)(int pageKey);					/* +0x10 */
	int (*getFrameTimestamp)(const char *titleId, const char *frameId,
				 SceUInt64 *outTimestamp);			/* +0x14 */
	int (*getGateNormPos)(int pageKey, float *outX, float *outY);		/* +0x18 */
	void (*reloadAllPages)(void);						/* +0x1C */
	void (*reloadPocketStationPages)(void);					/* +0x20 */
	paf::ui::Widget *(*openPageOverlay)(paf::ui::Widget *parent);		/* +0x24 */
	int (*enqueueActivityRevert)(int unusedAppId, int flags, int mode, int option); /* +0x28 */
	int (*openOrLaunchApp)(const char *titleId, const char *statusParam);	/* +0x2C */
	void (*onActivePageChanged)(int pageKey);				/* +0x30 */
	void (*updatePageInfo)(int pageKey, const LiveAreaPageInitInfo *info);	/* +0x34 */
	void (*refreshPage)(int pageKey);					/* +0x38 */
	void (*resetPageFrames)(int pageKey);					/* +0x3C */
	void (*resetPageScroll)(int pageKey);					/* +0x40 */
	int (*isPageAtTop)(int pageKey);					/* +0x44 */
	int (*hasPatchIcon)(int pageKey);					/* +0x48 */
	int (*installLiveAreaContent)(LsdbAppInfoPtr *appInfo, bool force, bool isUpdate); /* +0x4C */
	int (*onAppDeleted)(paf::string *titleId);				/* +0x50 */
};

#endif /* LIVEAREA_INTERFACE_H */
