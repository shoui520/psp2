/*
 * SceInvitationDlgImplPlugin (vs0:sys/external/libcdlg_invitation_impl.suprx):
 * plugin lifecycle and SceInvitationDialog controller.
 */
#ifndef INVITATION_IMPL_DIALOG_H
#define INVITATION_IMPL_DIALOG_H

#include <scetypes.h>
#include <common_dialog.h>
#include <invitation_dialog.h>
#include <plugin.h>
#include <std/string.h>
#include <thread/thread.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include "si_core.h"

class SceInvitationDialog {
public:
	explicit SceInvitationDialog(paf::Plugin *plugin);
	~SceInvitationDialog();

	SceInt32 Start(const CommonDialogInvitationParam *work, const char *npCommId);
	void Stop();
	void WriteResults(CommonDialogInvitationParam *work);
	void WriteSendData(CommonDialogInvitationParam *work);
	const paf::string &GetLaunchUri() const;

private:
	paf::Plugin *m_plugin;
	SceSiCoreImpl m_core;
	paf::thread::RMutex m_useMapLock;
	paf::thread::RMutex m_invitationListLock;
	paf::thread::RMutex m_iconLock;
	paf::thread::RMutex m_requestLock;
	paf::string m_npCommId;
	paf::string m_launchUri;
	SceInvitationDialogResult m_result;
	bool m_running;
};

#endif /* INVITATION_IMPL_DIALOG_H */
