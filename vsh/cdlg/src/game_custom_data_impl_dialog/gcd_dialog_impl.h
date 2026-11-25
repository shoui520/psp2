/*
 * SceNpGameCustomDataDlgImpl (vs0:sys/external/libcdlg_game_custom_data_impl.suprx):
 * plugin lifecycle and SceGameCustomDataDialog controller.
 */
#ifndef GCD_IMPL_DIALOG_H
#define GCD_IMPL_DIALOG_H

#include <scetypes.h>
#include <common_dialog.h>
#include <game_custom_data_dialog.h>
#include <plugin.h>
#include <std/string.h>
#include <thread/thread.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include "gcd_core.h"

class SceGameCustomDataDialog {
public:
	explicit SceGameCustomDataDialog(paf::Plugin *plugin);
	~SceGameCustomDataDialog();

	SceInt32 Start(const CommonDialogGameCustomDataParam *work, const char *npCommId);
	void Stop();
	void WriteResults(CommonDialogGameCustomDataParam *work);
	void WriteSendData(CommonDialogGameCustomDataParam *work);
	const paf::string &GetLaunchUri() const;

private:
	paf::Plugin *m_plugin;
	SceGcdCoreImpl m_core;
	paf::thread::RMutex m_useMapLock;
	paf::thread::RMutex m_itemInfoLock;
	paf::thread::RMutex m_iconLock;
	paf::thread::RMutex m_reqLock;
	paf::string m_npCommId;
	paf::string m_launchUri;
	SceGameCustomDataDialogResult m_result;
	SceInt32 m_dialogStatus;
	bool m_running;
};

#endif /* GCD_IMPL_DIALOG_H */
