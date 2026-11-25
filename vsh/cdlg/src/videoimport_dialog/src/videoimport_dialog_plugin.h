/*
 * SceVideoImportDialogPlugin (vs0:sys/external/libcdlg_videoimport.suprx):
 * plugin entry, work-memory controller, and SceVideoImportVideoGetResultJob.
 */
#ifndef VIDEOIMPORT_DIALOG_PLUGIN_H
#define VIDEOIMPORT_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <videoimport_dialog.h>
#include <plugin.h>
#include <common/job_slots.h>
#include <math/ngp/v4.h>
#include "../../cdlg_mgr/common_dialog_mgr.h"
#include "../../cdlg_main/common_dialog_main.h"
#include "video_db.h"
#include "video_list_view.h"

#define VIDEOIMPORT_DIALOG_ERROR_NO_WORK_MEMORY		(-2146396512)	/* 0x801096A0 */
#define VIDEOIMPORT_DIALOG_ERROR_API_NAME		(-2146396511)	/* 0x801096A1 */
#define VIDEOIMPORT_DIALOG_ERROR_NO_VIDEO		(-2146396510)	/* 0x801096A2 */
#define VIDEOIMPORT_DIALOG_ERROR_DB_QUERY		(-2146396507)	/* 0x801096A5 */

typedef void (*VideoImportResultCallback)(const SceVideoImportDialogResult *result, void *userData);

class SceVideoImportDialogCtrl;

class SceVideoImportVideoGetResultJob : public paf::common::SlotJobItem {
public:
	SceVideoImportVideoGetResultJob(VideoDBManager *dbManager,
					const SceVideoImportDialogParam *param,
					SceVideoImportDialogCtrl *controller,
					SceInt64 selectedMrid);
	virtual ~SceVideoImportVideoGetResultJob();

	virtual SceInt32 Run();
	virtual void Finish(SceInt32 result);

private:
	VideoDBManager *m_dbManager;
	const SceVideoImportDialogParam *m_param;
	SceVideoImportDialogCtrl *m_controller;
	SceInt64 m_selectedMrid;
	SceVideoImportDialogResult m_result;
};

class SceVideoImportDialogCtrl {
public:
	explicit SceVideoImportDialogCtrl(paf::Plugin *plugin);
	virtual ~SceVideoImportDialogCtrl();

	virtual SceInt32 OnInit(paf::Plugin *plugin);
	virtual SceInt32 OnStart(paf::Plugin *plugin);
	virtual SceInt32 OnStop(paf::Plugin *plugin);
	virtual SceInt32 OnExit(paf::Plugin *plugin);

	SceInt32 SetCommonParam(SceInt32 drawPriority, const SceCommonDialogColor *bgColor);
	SceInt32 SetImportParam(const SceVideoImportDialogParam *param,
				VideoImportResultCallback callback, void *userData);
	SceInt32 StartGetResultJob(SceInt64 mrid);
	void NotifyResult(const SceVideoImportDialogResult *result);
	void ShowErrorDialog(SceInt32 errorCode);

private:
	SceInt32 MountVideoDb(paf::string &outDbPath);
	SceInt32 InitStandaloneParam(paf::Plugin *plugin);

	paf::Plugin *m_plugin;
	VideoDBManager m_dbManager;
	VideoListView m_listView;
	paf::common::JobSlots m_resultSlots;
	SceVideoImportDialogParam m_param;
	SceVideoImportDialogResult m_result;
	VideoImportResultCallback m_callback;
	void *m_callbackUserData;
	char m_mountPoint[16];
	bool m_standalone;
	bool m_mounted;
};

#endif /* VIDEOIMPORT_DIALOG_PLUGIN_H */
