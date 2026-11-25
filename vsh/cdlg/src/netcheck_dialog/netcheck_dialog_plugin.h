/*
 * SceNetCheckDialogPlugin (vs0:sys/external/libcdlg_netcheck.suprx):
 * network connectivity, PSN sign-in, parental age check, ad-hoc, and PS3 connect dialog.
 */
#ifndef NETCHECK_DIALOG_PLUGIN_H
#define NETCHECK_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <netcheck_dialog.h>
#include <plugin.h>
#include <job/job.h>
#include <math/ngp/v4.h>
#include <thread/thread.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"

struct NetCheckDialogInitArg {
	SceUInt16 enabled[2];
	SceUInt16 useAgeRestriction;
	SceUInt16 cleared[2];
	SceUInt8 reserved[122];
};

struct CommonDialogNetCheckParam {
	SceNetCheckDialogParam param;
	NetCheckDialogInitArg initArg;
	SceNetCheckDialogPS3ConnectParam ps3ConnectParam;
	SceNetCheckDialogResult result;
	SceNetCheckDialogPS3ConnectInfo ps3ConnectInfo;
	SceUInt8 pluginState[44];
	SceCommonDialogColor bgColor;
	char apiName[64];
	SceNetAdhocctlGroupName groupName;
	SceNetCheckDialogAgeRestriction ageRestriction[SCE_NETCHECK_DIALOG_AGE_RESTRICTION_COUNT_MAX];
};

class SceNetCheckDialogCtrl;

class SceNetCheckJob : public paf::job::JobItem {
public:
	SceNetCheckJob(SceNetCheckDialogCtrl *controller, SceInt32 mode);
	virtual ~SceNetCheckJob();

	virtual SceInt32 Run();
	virtual void Finish(SceInt32 result);

private:
	SceNetCheckDialogCtrl *m_controller;
	SceInt32 m_mode;
};

class SceNetCheckDialogCtrl {
public:
	explicit SceNetCheckDialogCtrl(paf::Plugin *plugin);
	~SceNetCheckDialogCtrl();

	SceInt32 Start(const CommonDialogNetCheckParam *work);
	void Stop();
	void RequestAbort();

	void OnJobFinished(SceInt32 result);
	void LaunchSignInPlugin();
	void LaunchPatchCheckPlugin();
	void ApplyBackground(const CommonDialogNetCheckParam *work, bool useAgeRestriction);

	bool IsFinished() const;
	SceInt32 GetDialogResult() const;
	SceInt32 GetResultCode() const;
	const SceNetCheckDialogPS3ConnectInfo &GetPs3ConnectInfo() const;

	static const char *ModeName(SceInt32 mode);

private:
	paf::Plugin *m_plugin;
	paf::job::JobQueue *m_jobQueue;
	paf::thread::RMutex m_impsMutex;
	SceNetCheckDialogParam m_param;
	SceNetCheckDialogPS3ConnectInfo m_ps3Info;
	SceInt32 m_dialogResult;
	SceInt32 m_resultCode;
	bool m_finished;
	bool m_aborted;
	bool m_signInLoaded;
	bool m_patchChkLoaded;
};

#endif /* NETCHECK_DIALOG_PLUGIN_H */
