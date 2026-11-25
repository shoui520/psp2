/*
 * ScePocsDialogPlugin: PocketStation menu UI controller, jobs, and page stack.
 */
#ifndef POCKETSTATION_MENU_H
#define POCKETSTATION_MENU_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/memory>
#include <job/job.h>
#include <plugin.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../companion_dialog/src/furl_stack.h"
#include "../friendlist2_dialog/src/popup_list_view.h"
#include "pocketstation_dialog_plugin.h"

typedef void (*PocketStationWaitCallback)(void *userData);

class WaitJob : public paf::job::JobItem {
public:
	WaitJob(SceUInt32 waitMs, PocketStationWaitCallback callback, void *userData);
	virtual ~WaitJob();

	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	SceUInt32 m_waitMs;
	PocketStationWaitCallback m_callback;
	void *m_userData;
};

class MargeFontAndBgJob : public paf::job::JobItem {
public:
	MargeFontAndBgJob(paf::ui::Widget *targetWidget,
		const paf::intrusive_ptr<paf::graph::Surface> &bgSurface,
		const paf::intrusive_ptr<paf::graph::Surface> &fontSurface);
	virtual ~MargeFontAndBgJob();

	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	paf::ui::Widget *m_targetWidget;
	paf::intrusive_ptr<paf::graph::Surface> m_bgSurface;
	paf::intrusive_ptr<paf::graph::Surface> m_fontSurface;
};

struct PocketStationMenuPageEntry {
	paf::ui::Widget *rootWidget;
	paf::string pageName;
	paf::ui::Widget *headerWidget;
	paf::ui::Widget *footerWidget;
};

class PocketStationMenu {
public:
	static SceInt32 Initialize(void);
	static SceInt32 Stop(void);
	static SceInt32 Finalize(void);
	static SceInt32 Open(const PocketStationDialogParam *param,
		PocketStationCloseCallback closeCb);
	static SceInt32 Close(bool isAborted);
	static SceInt32 Resume(void);
	static SceInt32 CopyResultData(void *outBuffer);

	PocketStationMenu();
	~PocketStationMenu();

	SceInt32 OpenTopMenu(const PocketStationDialogParam *param,
		PocketStationCloseCallback closeCb);
	SceInt32 OpenCorrespondentList(bool directOpen, bool withShortcut);
	SceInt32 OpenReadManual(void);
	SceInt32 OpenBackgroundSelector(void);
	void CloseTopPage(void);
	void CloseAllPages(SceInt32 result);
	void ShowScreenshotBusy(bool visible);
	void ShowDisconnectBusy(bool visible);
	void ShowErrorDialog(SceInt32 errorCode);
	void ShowConfirmDialog(const paf::wstring &message, bool yesNo);
	void CheckSystemNotifications(void);
	void HandleGuiDialogButton(SceInt32 dialogValue, SceInt32 button);
	void ScheduleWait(SceUInt32 waitMs, PocketStationWaitCallback cb, void *userData);

private:
	paf::ui::Widget *OpenPageTemplate(const char *pageId, const char *templateId);
	void PopulateTopMenuItems(paf::ui::Widget *listWidget);
	void PopulateCorrespondentItems(paf::ui::Widget *listWidget);
	void CloseGuiDialog(void);

	PocketStationCloseCallback m_closeCb;
	PocketStationDialogParam m_param;
	paf::vector<PocketStationMenuPageEntry> m_pageStack;
	FurlStack m_furlStack;
	SceInt32 m_guiDialogId;
	SceUInt32 m_selectedAction;
	SceUInt32 m_selectedIndex;
	bool m_isOpen;
	bool m_screenshotSavedNotice;
	bool m_disconnectedNotice;
};

#endif /* POCKETSTATION_MENU_H */
