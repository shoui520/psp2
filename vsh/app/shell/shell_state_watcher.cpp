/*
 * shell_state_watcher.cpp: SceShell's strong dialog manager & input/plugin lock watcher
 * (0x8124D61C-0x8124DFFF).
 *
 * Tracks up to 5 standard shell dialogs plus 1 high-priority dialog (dimmer == 2), acquires
 * input ownership (0x6F), locks topmenu_plugin and power_manage_plugin key handling while any
 * dialog is active (retrying via MainThreadCallList if those plugins are not yet loaded), and
 * releases locks when the last tracked dialog closes.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <plugin.h>
#include <common/main_thread_call_list.h>
#include <inputdevice/inputdevice.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <common_gui_dialog/common_gui_dialog.h>

#include "shell.h"
#include "plugins.h"
#include "shell_util_server.h"
#include "src/power_manage_plugin/src/power_manage_internal.h"

using namespace paf;

namespace {

enum {
	SHELL_WATCHER_MAX_STANDARD_DIALOGS	= 5,
	SHELL_WATCHER_INPUT_OWNERSHIP_MASK	= 0x6F,
	SHELL_WATCHER_DIMMER_HIGH_PRIORITY	= 2,
	SHELL_WATCHER_DIMMER_STANDARD		= 1,
	SHELL_WATCHER_PRIORITY_HIGH		= 5,
	SHELL_WATCHER_PRIORITY_STANDARD		= 23,
	SHELL_WATCHER_SOUND_HIGH_PRIORITY	= -682994459,	/* 0xD74A3625 */
	SHELL_WATCHER_SOUND_STANDARD		= -522585832,	/* 0xE0D9F518 */
	SHELL_WATCHER_TOPMENU_LAYER_LOCK	= 23,
	SHELL_WATCHER_TOPMENU_LAYER_UNLOCK	= 230,
	SHELL_WATCHER_TOPMENU_SLOT_LOCK		= 52,		/* +208 */
	SHELL_WATCHER_TOPMENU_SLOT_UNLOCK	= 53,		/* +212 */
	SHELL_WATCHER_DIALOG_PART_DIMMER	= 3,
	SHELL_WATCHER_EVENT_CLOSE		= 0x100002,	/* 1048578 */
};

class ShellStateWatcher;
static ShellStateWatcher *s_stateWatcher = NULL;

static void noopKeyHandler(void *userData)
{
	(void)userData;
}

static void retryTopmenuLock(void *userData);
static void retryPowerManageLock(void *userData);
static void onDialogCloseEvent(int32_t eventType, ui::Handler *self, ui::Event *event,
	void *userData);

class ShellStateWatcher {
public:
	ShellStateWatcher()
		: m_highPriorityDialogId(-1),
		  m_highPriorityLevel(-1),
		  m_topmenuLocked(false),
		  m_powerManageLocked(false) {}
	virtual ~ShellStateWatcher() {}

	int OpenLayoutDialog(const CommonGuiLayoutDialogParam *param, bool playSound, int level);
	int OpenMessageDialog(const CommonGuiMessageDialogParam *param, bool playSound, int level);
	int OpenListDialog(const CommonGuiListDialogParam *param, bool playSound, int level);
	int CloseDialog(int dialogId);

	int RemoveDialog(int dialogId);
	void TryAcquireTopmenuLock();
	void TryAcquirePowerManageLock();
	bool IsTopmenuLocked() const { return m_topmenuLocked; }
	bool IsPowerManageLocked() const { return m_powerManageLocked; }

private:
	void AcquireFocusAndLocks();
	void ReleaseFocusAndLocks();
	void RegisterOpenedDialog(const CommonGuiDialogParam *param, int level, int dialogId);
	int PrepareDialogSlot(CommonGuiDialogParam *param, bool playSound, int level);

	list<int> m_standardDialogIds;
	int m_highPriorityDialogId;
	int m_highPriorityLevel;
	bool m_topmenuLocked;
	bool m_powerManageLocked;
};

/* 0x8124DF24 */
static void retryTopmenuLock(void *userData)
{
	(void)userData;
	if (s_stateWatcher == NULL)
		return;

	s_stateWatcher->TryAcquireTopmenuLock();
	if (s_stateWatcher->IsTopmenuLocked())
		common::MainThreadCallList::Unregister(retryTopmenuLock, NULL);
}

/* 0x8124DF82 */
static void retryPowerManageLock(void *userData)
{
	(void)userData;
	if (s_stateWatcher == NULL)
		return;

	s_stateWatcher->TryAcquirePowerManageLock();
	if (s_stateWatcher->IsPowerManageLocked())
		common::MainThreadCallList::Unregister(retryPowerManageLock, NULL);
}

/* 0x8124DFDA */
static void onDialogCloseEvent(int32_t eventType, ui::Handler *self, ui::Event *event,
	void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	int dialogId = (int)(intptr_t)userData;
	if (s_stateWatcher != NULL && dialogId >= 0) {
		if (s_stateWatcher->RemoveDialog(dialogId) == 0)
			SceCommonGuiDialog_BC311DC0(dialogId);
	}
}

void ShellStateWatcher::TryAcquireTopmenuLock()
{
	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
		if (iface != NULL) {
			void (*lockLayerFn)(void *, int, int, int, int) =
				(void (*)(void *, int, int, int, int))iface->func[SHELL_WATCHER_TOPMENU_SLOT_LOCK];
			lockLayerFn(this, SHELL_WATCHER_TOPMENU_LAYER_LOCK, 1, 0, 0);
			m_topmenuLocked = true;
		}
	}
}

void ShellStateWatcher::TryAcquirePowerManageLock()
{
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL) {
			iface->pushKeyHandler(noopKeyHandler, NULL);
			m_powerManageLocked = true;
		}
	}
}

/* 0x8124D7BE */
void ShellStateWatcher::AcquireFocusAndLocks()
{
	if (m_standardDialogIds.size() != 0 || m_highPriorityDialogId >= 0)
		return;

	inputdevice::GetOwnership(SHELL_WATCHER_INPUT_OWNERSHIP_MASK);

	TryAcquireTopmenuLock();
	if (!m_topmenuLocked)
		common::MainThreadCallList::Register(retryTopmenuLock, NULL);

	TryAcquirePowerManageLock();
	if (!m_powerManageLocked)
		common::MainThreadCallList::Register(retryPowerManageLock, NULL);
}

/* 0x8124D85C */
void ShellStateWatcher::RegisterOpenedDialog(const CommonGuiDialogParam *param, int level,
	int dialogId)
{
	AcquireFocusAndLocks();

	if (param->dimmer == SHELL_WATCHER_DIMMER_HIGH_PRIORITY) {
		m_highPriorityLevel = level;
		m_highPriorityDialogId = dialogId;
	} else {
		m_standardDialogIds.push_back(dialogId);
	}

	ui::Widget *dimmerWidget = SceCommonGuiDialog_8947FC23(dialogId,
		SHELL_WATCHER_DIALOG_PART_DIMMER);
	if (dimmerWidget != NULL) {
		ScePafWidget_0253DC9C(dimmerWidget, true);
		dimmerWidget->AddEventListener(SHELL_WATCHER_EVENT_CLOSE,
			new ui::EventCBListener(onDialogCloseEvent, (void *)(intptr_t)dialogId),
			false);
	}
}

/* 0x8124D908 */
void ShellStateWatcher::ReleaseFocusAndLocks()
{
	if (m_standardDialogIds.size() != 0 || m_highPriorityDialogId >= 0)
		return;

	if (m_powerManageLocked) {
		Plugin *pm = Plugin::Find("power_manage_plugin");
		if (pm != NULL) {
			power_manage::PowerManageInterface *iface =
				(power_manage::PowerManageInterface *)pm->GetInterface(1);
			if (iface != NULL) {
				iface->popKeyHandler(noopKeyHandler, NULL);
				m_powerManageLocked = false;
			}
		}
	} else {
		common::MainThreadCallList::Unregister(retryPowerManageLock, NULL);
	}

	if (m_topmenuLocked) {
		Plugin *topmenu = Plugin::Find("topmenu_plugin");
		if (topmenu != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
			if (iface != NULL) {
				void (*unlockLayerFn)(void *, int) =
					(void (*)(void *, int))iface->func[SHELL_WATCHER_TOPMENU_SLOT_UNLOCK];
				unlockLayerFn(this, SHELL_WATCHER_TOPMENU_LAYER_UNLOCK);
				m_topmenuLocked = false;
			}
		}
	} else {
		common::MainThreadCallList::Unregister(retryTopmenuLock, NULL);
	}

	inputdevice::ReleaseOwnership(SHELL_WATCHER_INPUT_OWNERSHIP_MASK);
}

/* 0x8124D99E */
int ShellStateWatcher::RemoveDialog(int dialogId)
{
	for (list<int>::iterator it = m_standardDialogIds.begin();
	     it != m_standardDialogIds.end(); ++it) {
		if (*it == dialogId) {
			m_standardDialogIds.erase(it);
			ReleaseFocusAndLocks();
			return 0;
		}
	}

	if (m_highPriorityDialogId == dialogId) {
		m_highPriorityDialogId = -1;
		m_highPriorityLevel = -1;
		ReleaseFocusAndLocks();
		return 0;
	}

	return SHELL_UTIL_ERR_NOT_FOUND;
}

/* 0x8124D9F4 */
int ShellStateWatcher::CloseDialog(int dialogId)
{
	if (dialogId < 0)
		return SHELL_UTIL_ERR_INVALID_ID;

	int ret = RemoveDialog(dialogId);
	if (ret == 0)
		SceCommonGuiDialog_BC311DC0(dialogId);
	return ret;
}

/* 0x8124DA16 */
int ShellStateWatcher::PrepareDialogSlot(CommonGuiDialogParam *param, bool playSound, int level)
{
	if (param->dimmer == SHELL_WATCHER_DIMMER_HIGH_PRIORITY) {
		if (m_highPriorityLevel >= level)
			return SHELL_UTIL_ERR_DEFERRED;

		int existingId = m_highPriorityDialogId;
		if (existingId >= 0 && RemoveDialog(existingId) == 0)
			SceCommonGuiDialog_BC311DC0(existingId);

		if (param->soundId == 0 && playSound)
			param->soundId = (SceUInt32)SHELL_WATCHER_SOUND_HIGH_PRIORITY;
		param->priority = SHELL_WATCHER_PRIORITY_HIGH;
		return 0;
	}

	if (m_standardDialogIds.size() == SHELL_WATCHER_MAX_STANDARD_DIALOGS) {
		int oldestId = m_standardDialogIds.front();
		if (oldestId >= 0 && RemoveDialog(oldestId) == 0)
			SceCommonGuiDialog_BC311DC0(oldestId);
	}

	if (param->soundId == 0 && playSound)
		param->soundId = (SceUInt32)SHELL_WATCHER_SOUND_STANDARD;
	param->dimmer = SHELL_WATCHER_DIMMER_STANDARD;
	param->priority = SHELL_WATCHER_PRIORITY_STANDARD;
	return 0;
}

/* 0x8124DA9A */
int ShellStateWatcher::OpenLayoutDialog(const CommonGuiLayoutDialogParam *param,
	bool playSound, int level)
{
	CommonGuiLayoutDialogParam localParam(*param);
	int ret = PrepareDialogSlot(&localParam, playSound, level);
	if (ret != 0) {
		delete localParam.handler;
		delete localParam.openHandler;
		delete localParam.updateHandler;
		return ret;
	}

	int dialogId = SceCommonGuiDialog_CCFCD9A6(&localParam);
	if (dialogId >= 0)
		RegisterOpenedDialog(&localParam, level, dialogId);
	return dialogId;
}

/* 0x8124DC34 */
int ShellStateWatcher::OpenMessageDialog(const CommonGuiMessageDialogParam *param,
	bool playSound, int level)
{
	CommonGuiMessageDialogParam localParam(*param);
	int ret = PrepareDialogSlot(&localParam, playSound, level);
	if (ret != 0) {
		delete localParam.handler;
		delete localParam.openHandler;
		return ret;
	}

	int dialogId = SceCommonGuiDialog_20B141F1(&localParam);
	if (dialogId >= 0)
		RegisterOpenedDialog(&localParam, level, dialogId);
	return dialogId;
}

/* 0x8124DE08 */
int ShellStateWatcher::OpenListDialog(const CommonGuiListDialogParam *param,
	bool playSound, int level)
{
	CommonGuiListDialogParam localParam(*param);
	int ret = PrepareDialogSlot(&localParam, playSound, level);
	if (ret != 0) {
		delete localParam.handler;
		return ret;
	}

	int dialogId = SceCommonGuiDialog_F3D37CAE(&localParam);
	if (dialogId >= 0)
		RegisterOpenedDialog(&localParam, level, dialogId);
	return dialogId;
}

} /* anonymous namespace */

/* 0x8124D61C */
int shellStateWatcherOpenDialog(CommonGuiLayoutDialogParam *param, bool playSound, int level)
{
	if (s_stateWatcher == NULL)
		return SHELL_UTIL_ERR_NOT_INITIALIZED;
	return s_stateWatcher->OpenLayoutDialog(param, playSound, level);
}

/* 0x8124D646 */
int shellErrorDialogOpenParam(CommonGuiMessageDialogParam *param, bool playSound, int level)
{
	if (s_stateWatcher == NULL)
		return SHELL_UTIL_ERR_NOT_INITIALIZED;
	return s_stateWatcher->OpenMessageDialog(param, playSound, level);
}

/* 0x8124D670 */
int shellStateWatcherOpenListDialog(CommonGuiListDialogParam *param, bool playSound, int level)
{
	if (s_stateWatcher == NULL)
		return SHELL_UTIL_ERR_NOT_INITIALIZED;
	return s_stateWatcher->OpenListDialog(param, playSound, level);
}

/* 0x8124D69A */
int shellStateWatcherCloseDialog(int dialogId)
{
	if (s_stateWatcher == NULL)
		return SHELL_UTIL_ERR_NOT_INITIALIZED;
	return s_stateWatcher->CloseDialog(dialogId);
}

/* 0x8124D6BA */
int shellStateWatcherCreate(void)
{
	if (s_stateWatcher != NULL)
		return SHELL_ERROR_ALREADY_CREATED;

	s_stateWatcher = new ShellStateWatcher();
	if (s_stateWatcher == NULL)
		return SHELL_ERROR_NO_MEMORY;

	return 0;
}
