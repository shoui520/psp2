/*
 * PageWizard and its page classes (vsh/app/common/src/page_wizard/page_wizard.h):
 * a page stack with 3D turn/slide/zoom transitions over PAF scenes, plus an
 * observer interface and deferred job dispatch on transition completion.
 * Sony's binaries name "page_wizard_bg" and "page_wizard_fg"; class and method
 * names are derived from their roles and the sibling DialogPage in photoreview_dialog.
 */
#ifndef VSH_APP_COMMON_PAGE_WIZARD_H
#define VSH_APP_COMMON_PAGE_WIZARD_H

#include <stdint.h>
#include <std/list>
#include <std/map>
#include <std/vector>
#include <common/job_slots.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <plugin.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>

class PageObservable;

class PageObserver {
public:
	virtual ~PageObserver();
	virtual void OnPageNotify(PageObservable *sender, int32_t eventCode,
				  const paf::IDParam &pageId) = 0;
};

struct PageObserverPendingChange {
	int32_t operation;
	PageObserver *observer;
	int32_t priority;
};

#ifdef PAF_BUILD_2014
PAF_LIST_INLINE_DTOR(PageObserver *)
PAF_LIST_INLINE_DTOR(PageObserverPendingChange)
#endif

class PageObservable {
public:
	PageObservable();
	virtual ~PageObservable();

	void NotifyObservers(int32_t eventCode, const paf::IDParam &pageId);

	paf::list<PageObserver *> m_observers;
	paf::list<PageObserverPendingChange> m_pendingChanges;
	bool m_notifying;
};

class PageBase {
public:
	PageBase(paf::Plugin *plugin, const paf::IDParam &pageId,
		 const paf::Plugin::PageOpenParam &openParam);

	virtual ~PageBase();
	virtual int OnCreate();
	virtual int OnDestroy();
	virtual int Open();
	virtual int Close();
	virtual int Activate();
	virtual int Inactivate();
	virtual void FadeIn(float durationMs, float delayMs);
	virtual void FadeOut(float durationMs, float delayMs);
	virtual void Hide(float durationMs);
	virtual bool IsOpen();

	virtual void OnInit(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnActivate(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnInactivate(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnCancel(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnDecide(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnButton(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnContextMenu(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnSystemEvent(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnKeyEvent(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
	virtual void OnFadeEnd(paf::Timer *timer, paf::ui::AnimListener *listener,
			       void *targetWidget, void *userContext, void *userContextExtra);
	virtual void OnHidden();

	paf::ui::Scene *GetRoot();
	paf::IDParam GetID() const;
	paf::ui::Widget *FindChild(const paf::IDParam &childId, int32_t searchOption);

	static void InitCallback(int32_t eventCode, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userContext);
	static void ActivateCallback(int32_t eventCode, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userContext);
	static void InactivateCallback(int32_t eventCode, paf::ui::Handler *handler,
				       paf::ui::Event *event, void *userContext);
	static void FadeEndCallback(paf::Timer *timer, paf::ui::AnimListener *listener,
				    void *targetWidget, void *userContext, void *userContextExtra);
	static void HiddenCallback(paf::Timer *timer, paf::ui::AnimListener *listener,
				   void *targetWidget, void *userContext, void *userContextExtra);

	paf::Plugin *m_plugin;
	paf::IDParam m_pageId;
	paf::Plugin::PageOpenParam m_openParam;
};

class WizardPage : public PageBase, public PageObservable {
public:
	WizardPage(paf::Plugin *plugin, const paf::IDParam &pageId,
		   const paf::Plugin::PageOpenParam &openParam);
	virtual ~WizardPage();

	virtual void OnTransitionFinished();

	int DispatchDeferredJob();

	paf::common::SharedPtr<paf::job::JobItem> m_deferredJob;
	paf::job::JobQueue *m_jobQueue;
	paf::common::JobSlots *m_jobSlots;
	int32_t m_jobSlotIndex;
};

class PageWizard {
public:
	enum TransitionKind {
		TRANSITION_INSTANT = 0,
		TRANSITION_FADE = 1,
		TRANSITION_TURN_FORWARD = 2,
		TRANSITION_TURN_BACKWARD = 3,
		TRANSITION_SLIDE_LEFT = 4,
		TRANSITION_SLIDE_RIGHT = 5,
		TRANSITION_ZOOM_IN = 6,
		TRANSITION_ZOOM_OUT = 7
	};

	struct InitParam {
		InitParam();

		paf::IDParam foregroundPageId;
		paf::IDParam backgroundPageId;
		int32_t foregroundPriority;
		int32_t backgroundPriority;
	};

	struct TransitionRequest {
		WizardPage *incomingPage;
		WizardPage *outgoingPage;
		int32_t transitionKind;
	};

	static PageWizard *GetInstance();
	static void DestroyInstance();

	PageWizard();
	virtual ~PageWizard();

	int Initialize(paf::Plugin *plugin, const InitParam &param);
	int CloseAllPages();
	int Finalize();
	int RegisterPage(WizardPage *page);
	int ClearPages();
	WizardPage *FindPage(const paf::IDParam &pageId);
	WizardPage *GetCurrentPage();
	int PushPage(const paf::IDParam &pageId, int32_t transitionKind);

private:
	int StartTransition(const TransitionRequest &request);
	void CompleteTransition();
	static void TransitionEndCallback(paf::Timer *timer, paf::ui::AnimListener *listener,
					  void *targetWidget, void *userContext,
					  void *userContextExtra);

	static PageWizard *s_instance;

	InitParam m_initParam;
	paf::Plugin *m_plugin;
	paf::map<paf::IDParam, WizardPage *> m_pages;
	paf::vector<WizardPage *> m_pageStack;
	paf::ui::Scene *m_backgroundScene;
	paf::ui::Scene *m_foregroundScene;
	TransitionRequest m_activeTransition;
	bool m_transitioning;
};

#endif
