/*
 * topmenu_dialog_mgr.cpp: tracks CommonGuiDialog instances opened by topmenu_plugin,
 * binds power/indicator holds while open, and closes them cleanly on suspend/return
 * (0x81292AF4-0x81292EF7).
 */
#include "topmenu_internal.h"

class TopmenuDialogMgr {
public:
	TopmenuDialogMgr() : m_deferredDialogId(-1) {}
	~TopmenuDialogMgr() { m_activeDialogIds.clear(); }

	void RegisterDialog(int dialogId, bool deferUntilCloseAll);
	void CloseAllDialogs();
	void RemoveDialogId(int dialogId);

private:
	paf::list<int> m_activeDialogIds;
	int m_deferredDialogId;
};

namespace {

class TopmenuErrorDialogHandler : public CommonGuiDialogHandler {
public:
	virtual ~TopmenuErrorDialogHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button != 0) {
			SceCommonGuiDialog_BC311DC0(dialogId);
			return;
		}
		if (g_topWindow->m_powerManageIf != NULL)
			g_topWindow->m_powerManageIf->popKeyHandler(
				topmenuDialogPowerKeyCallback, (void *)dialogId);
		if (g_topWindow->m_indicatorIf != NULL)
			g_topWindow->m_indicatorIf->UnlockButton();
	}
};

/* 0x81292C48 */
void onTopmenuDialogWidgetDeleted(paf::ui::Widget *widget, void *userData)
{
	(void)widget;
	int dialogId = (int)(intptr_t)userData;
	paf::thread::RMutex::main_thread_mutex.Lock();
	g_topmenuDialogMgr->RemoveDialogId(dialogId);
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

} /* namespace */

TopmenuDialogMgr *g_topmenuDialogMgr;

/* 0x81292B28 */
void TopmenuDialogMgr::RegisterDialog(int dialogId, bool deferUntilCloseAll)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	paf::ui::Widget *dialogWidget = SceCommonGuiDialog_8947FC23(dialogId, 4);
	if (dialogWidget != NULL) {
		if (deferUntilCloseAll) {
			m_deferredDialogId = dialogId;
		} else {
			m_activeDialogIds.push_back(dialogId);
			dialogWidget->AddDeleteCB(onTopmenuDialogWidgetDeleted,
						  (void *)(intptr_t)dialogId);
		}
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x81292BEC */
void TopmenuDialogMgr::CloseAllDialogs()
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	while (m_activeDialogIds.size() != 0) {
		int dialogId = m_activeDialogIds.front();
		m_activeDialogIds.pop_front();
		SceCommonGuiDialog_BC311DC0(dialogId);
	}
	if (m_deferredDialogId >= 0) {
		RegisterDialog(m_deferredDialogId, false);
		m_deferredDialogId = -1;
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

void TopmenuDialogMgr::RemoveDialogId(int dialogId)
{
	m_activeDialogIds.remove(dialogId);
}

/* 0x81292AF4 */
void topmenuDialogOnPowerRelease(int dialogId)
{
	if (g_topWindow->m_powerManageIf != NULL)
		g_topWindow->m_powerManageIf->popKeyHandler(topmenuDialogPowerKeyCallback,
							    (void *)(intptr_t)dialogId);
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->UnlockButton();
}

/* 0x81292C94 */
void topmenuDialogMgrInit(void)
{
	g_topmenuDialogMgr = new TopmenuDialogMgr();
	g_topmenuInterface->registerDialogId = topmenuDialogRegisterAndHold;
	g_topmenuInterface->unregisterDialogId = topmenuDialogOnPowerRelease;
}

/* 0x81292CF2 */
void topmenuDialogMgrTerm(void)
{
	delete g_topmenuDialogMgr;
	g_topmenuDialogMgr = NULL;
}

/* 0x81292D3C */
void topmenuDialogRegister(int dialogId)
{
	g_topmenuDialogMgr->RegisterDialog(dialogId, false);
}

/* 0x81292D54 */
void topmenuDialogRegisterDeferred(int dialogId)
{
	g_topmenuDialogMgr->RegisterDialog(dialogId, true);
}

/* 0x81292D6C */
void topmenuDialogCloseAll(void)
{
	g_topmenuDialogMgr->CloseAllDialogs();
}

/* 0x81292D7E */
void topmenuDialogOnPowerHold(int dialogId)
{
	if (g_topWindow->m_powerManageIf != NULL)
		g_topWindow->m_powerManageIf->pushKeyHandler(topmenuDialogPowerKeyCallback,
							     (void *)(intptr_t)dialogId);
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->LockButton();
}

/* 0x81292DB2 */
int topmenuOpenErrorDialog(CommonGuiMessageDialogParam *param)
{
	param->handler = new TopmenuErrorDialogHandler();
	param->notifyClose = true;
	param->priority = 33;

	int dialogId = SceCommonGuiDialog_20B141F1(param);
	if (dialogId >= 0) {
		g_topmenuDialogMgr->RegisterDialog(dialogId, false);
		if (g_topWindow->m_powerManageIf != NULL)
			g_topWindow->m_powerManageIf->pushKeyHandler(
				topmenuDialogPowerKeyCallback, (void *)(intptr_t)dialogId);
		if (g_topWindow->m_indicatorIf != NULL)
			g_topWindow->m_indicatorIf->LockButton();
	}
	return dialogId;
}

/* 0x81292E2E */
void topmenuDialogPowerKeyCallback(void *userData)
{
	(void)userData;
	g_topWindow->m_imposeIf->zoomRefuse();
}

/* 0x81292E42 */
void topmenuDialogRegisterAndHold(int dialogId)
{
	g_topmenuDialogMgr->RegisterDialog(dialogId, false);
	if (g_topWindow->m_powerManageIf != NULL)
		g_topWindow->m_powerManageIf->pushKeyHandler(topmenuDialogPowerKeyCallback,
							     (void *)(intptr_t)dialogId);
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->LockButton();
}
