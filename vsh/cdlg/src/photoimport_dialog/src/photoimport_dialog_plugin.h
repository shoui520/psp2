/*
 * ScePhotoImportDialogPlugin (vs0:sys/external/libcdlg_photoimport.suprx):
 * plugin entry, work-memory controller, and ScePhotoImportPhotoGetResultJob.
 */
#ifndef PHOTOIMPORT_DIALOG_PLUGIN_H
#define PHOTOIMPORT_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <photoimport_dialog.h>
#include <plugin.h>
#include <common/job_slots.h>
#include <math/ngp/v4.h>
#include "../../cdlg_mgr/common_dialog_mgr.h"
#include "../../cdlg_main/common_dialog_main.h"
#include "photo_db.h"
#include "photo_list_view.h"

#define PHOTOIMPORT_DIALOG_ERROR_NO_WORK_MEMORY		(-2146428000)	/* 0x80101BA0 */
#define PHOTOIMPORT_DIALOG_ERROR_API_NAME		(-2146427999)	/* 0x80101BA1 */
#define PHOTOIMPORT_DIALOG_ERROR_NO_PHOTO		(-2146427998)	/* 0x80101BA2 */
#define PHOTOIMPORT_DIALOG_ERROR_DB_QUERY		(-2146427995)	/* 0x80101BA5 */

typedef void (*PhotoImportResultCallback)(const ScePhotoImportDialogResult *result, void *userData);

class ScePhotoImportDialogCtrl;

class ScePhotoImportPhotoGetResultJob : public paf::common::SlotJobItem {
public:
	ScePhotoImportPhotoGetResultJob(PhotoDBManager *dbManager,
					const ScePhotoImportDialogParam *param,
					ScePhotoImportDialogCtrl *controller,
					SceInt64 selectedMrid);
	virtual ~ScePhotoImportPhotoGetResultJob();

	virtual SceInt32 Run();
	virtual void Finish(SceInt32 result);

private:
	PhotoDBManager *m_dbManager;
	const ScePhotoImportDialogParam *m_param;
	ScePhotoImportDialogCtrl *m_controller;
	SceInt64 m_selectedMrid;
	ScePhotoImportDialogResult m_result;
};

class ScePhotoImportDialogCtrl {
public:
	explicit ScePhotoImportDialogCtrl(paf::Plugin *plugin);
	virtual ~ScePhotoImportDialogCtrl();

	virtual SceInt32 OnInit(paf::Plugin *plugin);
	virtual SceInt32 OnStart(paf::Plugin *plugin);
	virtual SceInt32 OnStop(paf::Plugin *plugin);
	virtual SceInt32 OnExit(paf::Plugin *plugin);

	SceInt32 SetCommonParam(SceInt32 drawPriority, const SceCommonDialogColor *bgColor);
	SceInt32 SetImportParam(const ScePhotoImportDialogParam *param,
				PhotoImportResultCallback callback, void *userData);
	SceInt32 StartGetResultJob(SceInt64 mrid);
	void NotifyResult(const ScePhotoImportDialogResult *result);
	void ShowErrorDialog(SceInt32 errorCode);

private:
	SceInt32 MountPhotoDb(paf::string &outDbPath);
	SceInt32 InitStandaloneParam(paf::Plugin *plugin);

	paf::Plugin *m_plugin;
	PhotoDBManager m_dbManager;
	PhotoListView m_listView;
	paf::common::JobSlots m_resultSlots;
	ScePhotoImportDialogParam m_param;
	ScePhotoImportDialogResult m_result;
	PhotoImportResultCallback m_callback;
	void *m_callbackUserData;
	char m_mountPoint[16];
	bool m_standalone;
	bool m_mounted;
};

#endif /* PHOTOIMPORT_DIALOG_PLUGIN_H */
