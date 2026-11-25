/*
 * SceCalendarDialogPlugin (vs0:sys/external/libcdlg_calendar_review.suprx):
 * calendar review common dialog plugin and iCalendar event controller.
 */
#ifndef CALENDAR_REVIEW_DIALOG_PLUGIN_H
#define CALENDAR_REVIEW_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <std/string.h>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <widget/w_scene.h>
#include "../../cdlg_mgr/common_dialog_mgr.h"
#include "../../cdlg_main/common_dialog_main.h"

#define CALENDAR_REVIEW_ERROR_NO_WORK_MEMORY	(-2146399840)	/* 0x801089A0 */
#define CALENDAR_REVIEW_ERROR_API_NAME		(-2146399839)	/* 0x801089A1 */

typedef void (*CalendarReviewResultCallback)(const CalendarReviewDialogResult *result,
					     void *userData);

struct CalendarEventSummary {
	paf::string summary;
	paf::string dtStamp;
	paf::string dtStart;
	paf::string dtEnd;
	paf::string description;
	paf::string location;
	paf::string rrule;
	paf::string titleId;
	paf::string contentId;
};

class SceCalendarReviewCtrl {
public:
	explicit SceCalendarReviewCtrl(paf::Plugin *plugin);
	~SceCalendarReviewCtrl();

	SceInt32 SetCommonParam(SceInt32 drawPriority, const SceCommonDialogColor *bgColor);
	SceInt32 SetReviewParam(const CalendarReviewDialogParam *param,
				CalendarReviewResultCallback callback, void *userData);

	SceInt32 OnInit(paf::Plugin *plugin);
	SceInt32 OnStart(paf::Plugin *plugin);
	SceInt32 OnStop(paf::Plugin *plugin);
	SceInt32 OnExit(paf::Plugin *plugin);

	void NotifyResult(const CalendarReviewDialogResult *result);

private:
	paf::Plugin *m_plugin;
	paf::ui::Scene *m_scene;
	CalendarReviewDialogParam m_param;
	CalendarReviewDialogResult m_result;
	CalendarEventSummary m_event;
	CalendarReviewResultCallback m_callback;
	void *m_callbackUserData;
	SceInt32 m_drawPriority;
	paf::math::v4 m_bgColor;
	bool m_opened;
};

#endif /* CALENDAR_REVIEW_DIALOG_PLUGIN_H */
