/*
 * power_manage_passcode_screen.cpp: lock-screen passcode prompt and lockout controller.
 * 3.65-CEX: 0x81240848 - 0x81240B24
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <inputdevice/inputdevice.h>

#include "shell.h"
#include "plugins.h"
#include "exclusive_func.h"
#include "src/livespace_db/lsdb_keys.h"
#include "power_manage_internal.h"

extern "C" {
extern int sceAppMgrRestoreBgmSettingForShell(void);
}

namespace power_manage {

PasscodeLockScreen *g_passcodeLockScreen = NULL;

static const SceUInt32 kInputOwnershipMask = 0x6FU;
static const int kTopmenuSlotUnlockComplete = 75;	/* +0x12C (300) */

static void passcodeKeyNoop(void *userData)
{
	(void)userData;
}

PasscodeLockScreen::PasscodeLockScreen(paf::Plugin *plugin) : m_plugin(plugin)
{
	g_passcodeLockScreen = this;

	ExclusiveFuncMgr *exclMgr = ExclusiveFuncMgr::GetInstance();
	if (exclMgr != NULL)
		exclMgr->SetPriority(2, -2);

	KeyLockBlankPage::Close();
	if (authServiceStart(AUTH_STORE_SCREEN_LOCK_SCREEN, AUTH_ACTION_VERIFY,
			     OnAuthFinished, NULL, NULL) == AUTH_ERROR_LOCKED_OUT) {
		shellExclusiveFuncPasscodeLockoutStart(OnLockoutPluginLoaded, NULL, NULL);
	}

	powerManagePushKeyHandler(passcodeKeyNoop, NULL);
	powerManageLockPsButton();
	paf::inputdevice::GetOwnership(kInputOwnershipMask);
}

PasscodeLockScreen::~PasscodeLockScreen()
{
	paf::inputdevice::ReleaseOwnership(kInputOwnershipMask);
	powerManagePopKeyHandler(passcodeKeyNoop, NULL);
	powerManageUnlockPsButton();
	authServiceCancel();
	shellExclusiveFuncPasscodeLockoutStop();

	ExclusiveFuncMgr *exclMgr = ExclusiveFuncMgr::GetInstance();
	if (exclMgr != NULL)
		exclMgr->SetPriority(2, 0);

	g_passcodeLockScreen = NULL;
}

void PasscodeLockScreen::Unlock(bool skipPasscode)
{
	if (!skipPasscode)
		return;

	sceAppMgrRestoreBgmSettingForShell();
	PowerManage::SetPowerSaveMode(false, false);

	bool hasPendingApp = !g_powerManage->m_pendingLaunchApp.empty();
	paf::string launchApp = g_powerManage->m_pendingLaunchApp;
	PowerManage::SetScreenState(SCREEN_STATE_UNLOCKED);

	paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
	if (topmenuPlugin != NULL) {
		ShellPluginInterface *topmenuIface =
			(ShellPluginInterface *)topmenuPlugin->GetInterface(1);
		if (topmenuIface != NULL) {
			void (*onUnlock)(bool, const char *) =
				(void (*)(bool, const char *))
					topmenuIface->func[kTopmenuSlotUnlockComplete];
			onUnlock(hasPendingApp, launchApp.c_str());
		}
	}
}

void PasscodeLockScreen::OnLockoutPluginLoaded(paf::Plugin *plugin, void *userData)
{
	(void)userData;
	typedef void (*LockoutStartFunc)(void (*finishedCb)(int, void *), void *userData);
	LockoutStartFunc *lockoutIface = (LockoutStartFunc *)plugin->GetInterface(1);
	if (lockoutIface != NULL && lockoutIface[0] != NULL)
		lockoutIface[0](OnLockoutFinished, NULL);
}

void PasscodeLockScreen::OnAuthFinished(int result, void *userData)
{
	(void)userData;
	if (result == AUTH_OK) {
		g_passcodeLockScreen->Unlock(true);
	} else if (result != AUTH_VIEW_EVENT_ABORTED) {
		PowerManage::SetScreenState(SCREEN_STATE_KEYLOCK);
	}
}

void PasscodeLockScreen::OnLockoutFinished(int result, void *userData)
{
	(void)userData;
	if (result != 0)
		PowerManage::SetScreenState(SCREEN_STATE_KEYLOCK);
}

} /* namespace power_manage */
