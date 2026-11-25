/*
 * SceTwLoginDialogPlugin / SceTwitterDialogPlugin: shared dialog framework (page manager,
 * error dialog, wait dialog, infobar control, and async job runner).
 */
#ifndef TW_DIALOG_COMMON_H
#define TW_DIALOG_COMMON_H

#include <stdint.h>
#include <scetypes.h>
#include <plugin.h>
#include <widget/core/widget.h>
#include <common/job_slots.h>
#include <std/string>
#include <std/vector>
#include "tw_lx.h"

/* Page lifecycle listener registered in TwPageManager */
class TwPageHandler {
public:
	TwPageHandler() : m_pageWidget(NULL), m_onPageInit(NULL), m_userData(NULL) {}
	virtual ~TwPageHandler() {}
	virtual void OnPageInit(paf::ui::Widget *pageWidget)
	{
		m_pageWidget = pageWidget;
		if (m_onPageInit != NULL)
			m_onPageInit(pageWidget, m_userData);
	}

	paf::ui::Widget *m_pageWidget;
	void (*m_onPageInit)(paf::ui::Widget *pageWidget, void *userData);
	void *m_userData;
};

/* Entry in static page handler table */
struct TwPageHandlerEntry {
	uint32_t pageIdHash;
	TwPageHandler *handler;
	uint32_t reserved;
};

/* Manages plugin page init handlers and PageOpen/PageClose calls */
class TwPageManager {
public:
	TwPageManager();
	~TwPageManager();

	void Init(paf::Plugin *plugin);
	void Term();
	void CloseAll();
	void RegisterHandler(uint32_t pageIdHash, TwPageHandler *handler);
	void UnregisterHandler(uint32_t pageIdHash);

	void OpenPage(uint32_t pageIdHash);
	void ClosePage(uint32_t pageIdHash);
	void OpenPageWithEffect(uint32_t pageIdHash, float duration, int effectType);
	void ClosePageWithEffect(uint32_t pageIdHash, float duration, int effectType);
	void OpenPageWithAnim(uint32_t pageIdHash, int animType);
	void ClosePageWithAnim(uint32_t pageIdHash, int animType);

	static void OnPluginPageInit(int32_t type, paf::ui::Handler *self, paf::ui::Event *ev,
				     void *userdata);

	paf::Plugin *m_plugin;
	TwPageHandlerEntry *m_entries;
	paf::InitEventFunctionEntry *m_initTable;
};

/* Error dialog helper backed by SceCommonGuiDialog */
class TwErrorDialog {
public:
	typedef void (*CloseCallback)(int buttonId, void *userData);

	TwErrorDialog();
	~TwErrorDialog() {}

	void Init(paf::Plugin *plugin);
	void SetError(SceInt32 errorCode, const LxString &detail);
	void Show(CloseCallback callback, void *userData, int16_t mode, int16_t flags);
	void Close();

	static int OnDialogResult(void *self, int event, int buttonId);

	paf::Plugin *m_plugin;
	SceInt32 m_dialogId;
	SceInt32 m_errorCode;
	LxString m_errorDetail;
	uint8_t m_useSystemMessage;
	uint8_t m_padding[3];
	paf::wstring m_message;
	CloseCallback m_callback;
	void *m_userData;
};

/* Infobar visibility helper */
class TwInfobar {
public:
	static int Show();
	static int Hide();
};

/* Async job callbacks and container */
class TwJobItem;

class TwJobCallback {
public:
	virtual ~TwJobCallback() {}
	virtual void OnJobStart(void *userData) = 0;
	virtual int OnJobRun(void *userData, TwJobItem *job) = 0;
	virtual void OnJobSuccess(void *userData, int result) = 0;
	virtual void OnJobCancel(void *userData, int result) = 0;
};

class TwJobObserver {
public:
	virtual ~TwJobObserver() {}
	virtual void OnJobAdded(TwJobItem *job) = 0;
	virtual void OnJobRemoved(TwJobItem *job) = 0;
	virtual int CancelAll(void (*doneCb)(void *), void *userData, bool force) = 0;
};

/* SlotJobItem subclass that dispatches to TwJobCallback and TwJobObserver */
class TwJobItem : public paf::common::SlotJobItem {
public:
	TwJobItem(void *userData, TwJobCallback *callback, TwJobObserver *observer,
		  const char *name);
	virtual ~TwJobItem();

	virtual int Run();
	virtual void Finish(int result);

	TwJobCallback *m_callback;
	TwJobObserver *m_observer;
	void *m_userData;
};

/* Concrete job manager implementing both TwJobCallback and TwJobObserver */
class TwJobController : public TwJobCallback, public TwJobObserver {
public:
	typedef void (*StartFn)(void *job, void *userData);
	typedef int (*RunFn)(void *userData, TwJobItem *job, void *ctx);
	typedef void (*CompleteFn)(int result, void *userData, void *ctx);
	typedef void (*CancelFn)(int result, void *userData, void *ctx, bool force);
	typedef void (*AllCanceledFn)(void *userData);

	TwJobController();
	virtual ~TwJobController();

	virtual void OnJobStart(void *userData);
	virtual int OnJobRun(void *userData, TwJobItem *job);
	virtual void OnJobSuccess(void *userData, int result);
	virtual void OnJobCancel(void *userData, int result);

	virtual void OnJobAdded(TwJobItem *job);
	virtual void OnJobRemoved(TwJobItem *job);
	virtual int CancelAll(AllCanceledFn doneCb, void *userData, bool force);

	paf::vector<TwJobItem *> m_jobs;
	int m_pendingCancelCount;
	AllCanceledFn m_cancelDoneCb;
	void *m_cancelDoneArg;
	StartFn m_startFn;
	void *m_startCtx;
	RunFn m_runFn;
	void *m_runCtx;
	CompleteFn m_completeFn;
	void *m_completeCtx;
	CancelFn m_cancelFn;
	void *m_cancelCtx;
	uint8_t m_forceCancel;
};

/* Wait spinner dialog helper backed by SceCommonGuiDialog */
class TwWaitDialog {
public:
	typedef void (*CloseCallback)(int result, void *userData);

	TwWaitDialog();
	~TwWaitDialog();

	int Open(paf::Plugin *plugin, const paf::wstring &message, int styleIndex,
		 CloseCallback callback, void *userData);
	int Close();

	static int OnDialogEvent(void *self, int event, int result);
	static int OnDialogClosed(void *self, int event, int result);

	SceInt32 m_dialogId;
	CloseCallback m_callback;
	void *m_userData;
};

void TwInitPluginContext(paf::Plugin *plugin);

#endif /* TW_DIALOG_COMMON_H */
