/*
 * PageWizard implementation (vsh/app/common/src/page_wizard/page_wizard.cpp):
 * page lifecycle, observer dispatch, 3D scene transition animations, and
 * deferred job scheduling on transition completion.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <math/ngp/quaternion.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include "page_wizard.h"

using namespace paf;

#ifdef PAF_BUILD_2014
#define PAGE_WIZARD_ERROR_INVALID_ARGUMENT	((int)0x80104101)
#define PAGE_WIZARD_ERROR_ALREADY_EXISTS	((int)0x80104103)
#define PAGE_WIZARD_ERROR_BUSY			((int)0x80104105)
#else
#define PAGE_WIZARD_ERROR_INVALID_ARGUMENT	((int)0x80104001)
#define PAGE_WIZARD_ERROR_ALREADY_EXISTS	((int)0x80104003)
#define PAGE_WIZARD_ERROR_BUSY			((int)0x80104005)
#endif

#define PAGE_WIZARD_DEFAULT_FG_PRIORITY		140
#define PAGE_WIZARD_DEFAULT_BG_PRIORITY		150
#define PAGE_WIZARD_TRANSITION_TIME_MS		400.0f
#define PAGE_WIZARD_TRANSITION_EASING		21
#define PAGE_WIZARD_EULER_SMALL_Y		0.157079637f	/* PI / 20 */
#define PAGE_WIZARD_EULER_LARGE_Y		0.785398185f	/* PI / 4 */

struct PageTransitionPreset {
	math::v4 incomingOffset;
	math::v4 outgoingOffset;
	math::quaternion incomingRotation;
	math::quaternion outgoingRotation;
};

#ifdef PAF_BUILD_2014
static const PageTransitionPreset s_fadePreset = {
	math::v4(0.0f, 0.0f, 0.0f, 0.0f),
	math::v4(0.0f, 0.0f, 0.0f, 0.0f),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f)),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f))
};
#endif

static const PageTransitionPreset s_turnForwardPreset = {
	math::v4(440.0f, 0.0f, 200.0f, 0.0f),
	math::v4(-60.0f, 0.0f, 0.0f, 0.0f),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, PAGE_WIZARD_EULER_SMALL_Y, 0.0f)),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, PAGE_WIZARD_EULER_LARGE_Y, 0.0f))
};

static const PageTransitionPreset s_turnBackwardPreset = {
	math::v4(-60.0f, 0.0f, 0.0f, 0.0f),
	math::v4(440.0f, 0.0f, 200.0f, 0.0f),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, PAGE_WIZARD_EULER_LARGE_Y, 0.0f)),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, PAGE_WIZARD_EULER_SMALL_Y, 0.0f))
};

static const PageTransitionPreset s_slideLeftPreset = {
	math::v4(-960.0f, 0.0f, 0.0f, 0.0f),
	math::v4(960.0f, 0.0f, 0.0f, 0.0f),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f)),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f))
};

static const PageTransitionPreset s_slideRightPreset = {
	math::v4(960.0f, 0.0f, 0.0f, 0.0f),
	math::v4(-960.0f, 0.0f, 0.0f, 0.0f),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f)),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f))
};

static const PageTransitionPreset s_zoomInPreset = {
	math::v4(0.0f, 0.0f, 400.0f, 0.0f),
	math::v4(0.0f, 0.0f, -400.0f, 0.0f),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f)),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f))
};

static const PageTransitionPreset s_zoomOutPreset = {
	math::v4(0.0f, 0.0f, -400.0f, 0.0f),
	math::v4(0.0f, 0.0f, 400.0f, 0.0f),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f)),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f))
};

#ifndef PAF_BUILD_2014
static const PageTransitionPreset s_fadePreset = {
	math::v4(0.0f, 0.0f, 0.0f, 0.0f),
	math::v4(0.0f, 0.0f, 0.0f, 0.0f),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f)),
	math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f, 0.0f))
};
#endif

PageWizard *PageWizard::s_instance = NULL;

PageObserver::~PageObserver()
{
}

PageObservable::PageObservable() : m_notifying(false)
{
}

#ifdef PAF_BUILD_2014
__attribute__((noinline))
#endif
PageObservable::~PageObservable()
{
}

void PageObservable::NotifyObservers(int32_t eventCode, const IDParam &pageId)
{
	list<PageObserver *> snapshot(m_observers);

	for (list<PageObserver *>::iterator it = snapshot.begin(); it != snapshot.end(); ++it)
		(*it)->OnPageNotify(this, eventCode, pageId);
}

PageBase::PageBase(Plugin *plugin, const IDParam &pageId, const Plugin::PageOpenParam &openParam)
	: m_plugin(plugin), m_pageId(pageId), m_openParam(openParam)
{
}

#ifdef PAF_BUILD_2014
__attribute__((noinline))
#endif
PageBase::~PageBase()
{
}

int PageBase::OnCreate()
{
	return 0;
}

int PageBase::OnDestroy()
{
	return 0;
}

int PageBase::Open()
{
	ui::Scene *scene = m_plugin->PageOpen(m_pageId, m_openParam);

	if (scene != NULL) {
		scene->AddEventCallback(ui::Handler::CB_STATE_INIT, InitCallback, this);
		scene->AddEventCallback(ui::Scene::CB_SCENE_ACTIVATE, ActivateCallback, this);
		scene->AddEventCallback(ui::Scene::CB_SCENE_INACTIVATE, InactivateCallback, this);
	}
	return 0;
}

int PageBase::Close()
{
	m_plugin->PageClose(m_pageId, Plugin::PageCloseParam());
	return 0;
}

int PageBase::Activate()
{
	m_plugin->PageActivate(m_pageId);
	return 0;
}

int PageBase::Inactivate()
{
	m_plugin->PageInactivate(m_pageId);
	return 0;
}

#ifdef PAF_BUILD_2014
__attribute__((noinline))
#endif
ui::Scene *PageBase::GetRoot()
{
	if (m_plugin != NULL)
		return m_plugin->PageRoot(m_pageId);
	return NULL;
}

void PageBase::FadeIn(float durationMs, float delayMs)
{
	ui::Scene *scene;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_pageId)) != NULL)
		scene->SetMetaAlpha(1.0f, new Timer(durationMs), ui::Widget::ANIM_META_ALPHA, 2,
				    delayMs, FadeEndCallback, this);
}

void PageBase::FadeOut(float durationMs, float delayMs)
{
	ui::Scene *scene;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_pageId)) != NULL)
		scene->SetMetaAlpha(0.0f, new Timer(durationMs), ui::Widget::ANIM_META_ALPHA, 2,
				    delayMs, FadeEndCallback, this);
}

void PageBase::Hide(float durationMs)
{
	ui::Scene *scene;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_pageId)) != NULL)
		scene->SetMetaAlpha(0.0f, new Timer(durationMs), ui::Widget::ANIM_META_ALPHA, 2,
				    0.0f, HiddenCallback, this);
}

bool PageBase::IsOpen()
{
	return m_plugin != NULL && m_plugin->PageRoot(m_pageId) != NULL;
}

IDParam PageBase::GetID() const
{
	return m_pageId;
}

ui::Widget *PageBase::FindChild(const IDParam &childId, int32_t searchOption)
{
	ui::Scene *scene;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_pageId)) != NULL)
		return scene->FindChild(childId, searchOption);
	return NULL;
}

void PageBase::OnInit(int32_t eventCode, ui::Handler *handler, ui::Event *event)
{
}

void PageBase::OnActivate(int32_t eventCode, ui::Handler *handler, ui::Event *event)
{
}

void PageBase::OnInactivate(int32_t eventCode, ui::Handler *handler, ui::Event *event)
{
}

void PageBase::OnCancel(int32_t eventCode, ui::Handler *handler, ui::Event *event)
{
}

void PageBase::OnDecide(int32_t eventCode, ui::Handler *handler, ui::Event *event)
{
}

void PageBase::OnButton(int32_t eventCode, ui::Handler *handler, ui::Event *event)
{
}

void PageBase::OnContextMenu(int32_t eventCode, ui::Handler *handler, ui::Event *event)
{
}

void PageBase::OnSystemEvent(int32_t eventCode, ui::Handler *handler, ui::Event *event)
{
}

void PageBase::OnKeyEvent(int32_t eventCode, ui::Handler *handler, ui::Event *event)
{
}

void PageBase::OnFadeEnd(Timer *timer, ui::AnimListener *listener, void *targetWidget,
			 void *userContext, void *userContextExtra)
{
}

void PageBase::OnHidden()
{
}

void PageBase::InitCallback(int32_t eventCode, ui::Handler *handler, ui::Event *event,
			    void *userContext)
{
	if (userContext != NULL)
		static_cast<PageBase *>(userContext)->OnInit(eventCode, handler, event);
}

void PageBase::ActivateCallback(int32_t eventCode, ui::Handler *handler, ui::Event *event,
				void *userContext)
{
	if (userContext != NULL)
		static_cast<PageBase *>(userContext)->OnActivate(eventCode, handler, event);
}

void PageBase::InactivateCallback(int32_t eventCode, ui::Handler *handler, ui::Event *event,
				  void *userContext)
{
	if (userContext != NULL)
		static_cast<PageBase *>(userContext)->OnInactivate(eventCode, handler, event);
}

void PageBase::FadeEndCallback(Timer *timer, ui::AnimListener *listener, void *targetWidget,
			       void *userContext, void *userContextExtra)
{
	if (userContext != NULL)
		static_cast<PageBase *>(userContext)->OnFadeEnd(timer, listener, targetWidget,
								userContext, userContextExtra);
}

void PageBase::HiddenCallback(Timer *timer, ui::AnimListener *listener, void *targetWidget,
			      void *userContext, void *userContextExtra)
{
	if (userContext != NULL)
		static_cast<PageBase *>(userContext)->OnHidden();
}

PageWizard::InitParam::InitParam()
	: foregroundPageId(""), backgroundPageId(""),
	  foregroundPriority(PAGE_WIZARD_DEFAULT_FG_PRIORITY),
	  backgroundPriority(PAGE_WIZARD_DEFAULT_BG_PRIORITY)
{
}

PageWizard::PageWizard()
	: m_plugin(NULL), m_backgroundScene(NULL), m_foregroundScene(NULL), m_transitioning(false)
{
}

PageWizard *PageWizard::GetInstance()
{
	if (s_instance == NULL)
		s_instance = new PageWizard();
	return s_instance;
}

void PageWizard::DestroyInstance()
{
	if (s_instance != NULL) {
		delete s_instance;
		s_instance = NULL;
	}
}

PageWizard::~PageWizard()
{
}

int PageWizard::Initialize(Plugin *plugin, const InitParam &param)
{
	if (m_pageStack.size() != 0)
		return PAGE_WIZARD_ERROR_ALREADY_EXISTS;

	m_plugin = plugin;
	m_initParam = param;
	m_transitioning = false;

	if (!(m_initParam.backgroundPageId != IDParam("")))
		m_initParam.backgroundPageId = IDParam("page_wizard_bg");
	if (!(m_initParam.foregroundPageId != IDParam("")))
		m_initParam.foregroundPageId = IDParam("page_wizard_fg");

	if (m_plugin != NULL) {
		Plugin::PageOpenParam bgParam;
		Plugin::PageOpenParam fgParam;

		bgParam.overwrite_draw_priority = m_initParam.backgroundPriority;
		fgParam.overwrite_draw_priority = m_initParam.foregroundPriority;
		m_backgroundScene = m_plugin->PageOpen(m_initParam.backgroundPageId, bgParam);
		m_foregroundScene = m_plugin->PageOpen(m_initParam.foregroundPageId, fgParam);
	}
	return 0;
}

int PageWizard::CloseAllPages()
{
	if (m_plugin == NULL)
		return PAGE_WIZARD_ERROR_INVALID_ARGUMENT;

	while (m_pageStack.size() != 0) {
		WizardPage *page = m_pageStack.back();

		m_pageStack.pop_back();
		if (m_plugin != NULL && page != NULL)
			page->Close();
	}
	return 0;
}

int PageWizard::Finalize()
{
	if (m_plugin == NULL)
		return 0;

	CloseAllPages();
	if (m_plugin != NULL) {
		if (m_foregroundScene != NULL) {
			m_plugin->PageClose(m_initParam.foregroundPageId, Plugin::PageCloseParam());
			m_foregroundScene = NULL;
		}
		if (m_backgroundScene != NULL) {
			m_plugin->PageClose(m_initParam.backgroundPageId, Plugin::PageCloseParam());
			m_backgroundScene = NULL;
		}
	}
	m_plugin = NULL;
	m_transitioning = false;
	return 0;
}

int PageWizard::RegisterPage(WizardPage *page)
{
	if (page == NULL)
		return PAGE_WIZARD_ERROR_INVALID_ARGUMENT;

	IDParam pageId = page->GetID();

	if (m_pages.find(pageId) != m_pages.end())
		return PAGE_WIZARD_ERROR_ALREADY_EXISTS;

	m_pages.insert(pair<IDParam, WizardPage *>(pageId, page));
	return 0;
}

int PageWizard::ClearPages()
{
	m_pages.clear();
	return 0;
}

WizardPage *PageWizard::FindPage(const IDParam &pageId)
{
	map<IDParam, WizardPage *>::iterator it = m_pages.find(pageId);

	if (it == m_pages.end())
		return NULL;
	return it->second;
}

WizardPage *PageWizard::GetCurrentPage()
{
	if (m_pageStack.size() == 0)
		return NULL;
	return m_pageStack.back();
}

int PageWizard::StartTransition(const TransitionRequest &request)
{
	WizardPage *incomingPage = request.incomingPage;
	WizardPage *outgoingPage = request.outgoingPage;
	math::v4 incomingOffset(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 outgoingOffset(0.0f, 0.0f, 0.0f, 0.0f);
	math::quaternion incomingRotation;
	math::quaternion outgoingRotation;
	uint32_t incomingOriginX = 0;
	uint32_t outgoingOriginX = 0;
	float durationMs = PAGE_WIZARD_TRANSITION_TIME_MS;

	switch (request.transitionKind) {
	case TRANSITION_INSTANT:
		incomingOffset = s_fadePreset.incomingOffset;
		outgoingOffset = s_fadePreset.outgoingOffset;
		incomingRotation = s_fadePreset.incomingRotation;
		outgoingRotation = s_fadePreset.outgoingRotation;
		durationMs = 0.0f;
		break;
	case TRANSITION_FADE:
		incomingOffset = s_fadePreset.incomingOffset;
		outgoingOffset = s_fadePreset.outgoingOffset;
		incomingRotation = s_fadePreset.incomingRotation;
		outgoingRotation = s_fadePreset.outgoingRotation;
		break;
	case TRANSITION_TURN_FORWARD:
		incomingOffset = s_turnForwardPreset.incomingOffset;
		outgoingOffset = s_turnForwardPreset.outgoingOffset;
		incomingRotation = s_turnForwardPreset.incomingRotation;
		outgoingRotation = s_turnForwardPreset.outgoingRotation;
		outgoingOriginX = 1;
		break;
	case TRANSITION_TURN_BACKWARD:
		incomingOffset = s_turnBackwardPreset.incomingOffset;
		outgoingOffset = s_turnBackwardPreset.outgoingOffset;
		incomingRotation = s_turnBackwardPreset.incomingRotation;
		outgoingRotation = s_turnBackwardPreset.outgoingRotation;
		incomingOriginX = 1;
		break;
	case TRANSITION_SLIDE_LEFT:
		incomingOffset = s_slideLeftPreset.incomingOffset;
		outgoingOffset = s_slideLeftPreset.outgoingOffset;
		incomingRotation = s_slideLeftPreset.incomingRotation;
		outgoingRotation = s_slideLeftPreset.outgoingRotation;
		break;
	case TRANSITION_SLIDE_RIGHT:
		incomingOffset = s_slideRightPreset.incomingOffset;
		outgoingOffset = s_slideRightPreset.outgoingOffset;
		incomingRotation = s_slideRightPreset.incomingRotation;
		outgoingRotation = s_slideRightPreset.outgoingRotation;
		break;
	case TRANSITION_ZOOM_IN:
		incomingOffset = s_zoomInPreset.incomingOffset;
		outgoingOffset = s_zoomInPreset.outgoingOffset;
		incomingRotation = s_zoomInPreset.incomingRotation;
		outgoingRotation = s_zoomInPreset.outgoingRotation;
		break;
	case TRANSITION_ZOOM_OUT:
		incomingOffset = s_zoomOutPreset.incomingOffset;
		outgoingOffset = s_zoomOutPreset.outgoingOffset;
		incomingRotation = s_zoomOutPreset.incomingRotation;
		outgoingRotation = s_zoomOutPreset.outgoingRotation;
		break;
	default:
		break;
	}

	if (outgoingPage != NULL) {
		ui::Scene *outgoingScene = outgoingPage->GetRoot();

		if (outgoingScene != NULL) {
			if (durationMs == 0.0f) {
				outgoingScene->SetMetaAlpha(0.0f, NULL,
							    ui::Widget::ANIM_META_ALPHA,
							    0, 0.0f, NULL, NULL);
			} else {
				outgoingScene->SetMetaAlpha(
					0.0f, new Timer(durationMs, PAGE_WIZARD_TRANSITION_EASING),
					ui::Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
			}

			math::v4 center = outgoingScene->GetPosCenter(
				1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			outgoingScene->SetOrigin(outgoingOriginX, outgoingScene->m_origin_y,
						 outgoingScene->m_origin_z, NULL);
			outgoingScene->SetPosCenter(center);

			math::v4 targetPos = outgoingScene->GetPos(
				1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) + outgoingOffset;

			outgoingScene->SetPos(
				targetPos, new Timer(durationMs, PAGE_WIZARD_TRANSITION_EASING),
				ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
			outgoingScene->SetQuat(
				outgoingRotation,
				new Timer(durationMs, PAGE_WIZARD_TRANSITION_EASING),
				ui::Widget::ANIM_QUAT, 0, 0.0f, NULL, NULL);
		}
	}

	if (incomingPage == NULL)
		return PAGE_WIZARD_ERROR_INVALID_ARGUMENT;

	ui::Scene *incomingScene = incomingPage->GetRoot();

	if (incomingScene == NULL)
		return PAGE_WIZARD_ERROR_INVALID_ARGUMENT;

	m_transitioning = true;
	if (durationMs == 0.0f) {
		incomingScene->SetMetaAlpha(1.0f, NULL, ui::Widget::ANIM_META_ALPHA,
					    0, 0.0f, NULL, NULL);
	} else {
		incomingScene->SetMetaAlpha(0.0f, NULL, ui::Widget::ANIM_META_ALPHA,
					    0, 0.0f, NULL, NULL);
		incomingScene->SetMetaAlpha(
			1.0f, new Timer(durationMs, PAGE_WIZARD_TRANSITION_EASING),
			ui::Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
	}

	math::v4 center = incomingScene->GetPosCenter(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	incomingScene->SetOrigin(incomingOriginX, incomingScene->m_origin_y,
				 incomingScene->m_origin_z, NULL);
	incomingScene->SetPosCenter(center);

	math::v4 startPos = incomingScene->GetPos(
		0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) + incomingOffset;

	incomingScene->SetQuat(incomingRotation, NULL, ui::Widget::ANIM_QUAT, 0, 0.0f, NULL, NULL);
	incomingScene->SetPos(startPos, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);

	math::v4 restPos(0.0f, 0.0f, 0.0f, 0.0f);

	if (incomingOriginX == 1) {
		math::v4 *sceneSize = incomingScene->GetSize(0);

		restPos = math::v4(sceneSize->extract_x() * -0.5f, 0.0f, 0.0f, 0.0f);
	}
	incomingScene->SetPos(restPos, new Timer(durationMs, PAGE_WIZARD_TRANSITION_EASING),
			      ui::Widget::ANIM_MOVE, 0, 0.0f, TransitionEndCallback, this);
	incomingScene->SetQuat(math::quaternion::Identity(),
			       new Timer(durationMs, PAGE_WIZARD_TRANSITION_EASING),
			       ui::Widget::ANIM_QUAT, 0, 0.0f, NULL, NULL);
	return 0;
}

int PageWizard::PushPage(const IDParam &pageId, int32_t transitionKind)
{
	if (m_plugin == NULL)
		return PAGE_WIZARD_ERROR_INVALID_ARGUMENT;
	if (m_transitioning)
		return PAGE_WIZARD_ERROR_BUSY;

	WizardPage *nextPage = FindPage(pageId);

	if (nextPage == NULL)
		return PAGE_WIZARD_ERROR_INVALID_ARGUMENT;

	WizardPage *prevPage = GetCurrentPage();

	if (nextPage == prevPage)
		return PAGE_WIZARD_ERROR_ALREADY_EXISTS;

	m_pageStack.push_back(nextPage);

	int ret = PAGE_WIZARD_ERROR_INVALID_ARGUMENT;

	if (m_plugin != NULL) {
		ret = nextPage->Open();
		if (ret == 0) {
			nextPage->FadeOut(0.0f, 0.0f);
			m_activeTransition.incomingPage = nextPage;
			m_activeTransition.outgoingPage = prevPage;
			m_activeTransition.transitionKind = transitionKind;
			StartTransition(m_activeTransition);
			return 0;
		}
	}
	m_pageStack.pop_back();
	return ret;
}

void PageWizard::CompleteTransition()
{
	WizardPage *outgoingPage = m_activeTransition.outgoingPage;
	WizardPage *incomingPage = m_activeTransition.incomingPage;

	m_transitioning = false;
	if (outgoingPage != NULL)
		outgoingPage->Close();
	if (incomingPage != NULL) {
		ui::Scene *scene = incomingPage->GetRoot();

		if (scene != NULL) {
			scene->SetOrigin(0, scene->m_origin_y, scene->m_origin_z, NULL);
			scene->SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL,
				      ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		}
		incomingPage->OnTransitionFinished();
	}
}

void PageWizard::TransitionEndCallback(Timer *timer, ui::AnimListener *listener,
				       void *targetWidget, void *userContext,
				       void *userContextExtra)
{
	static_cast<PageWizard *>(userContext)->CompleteTransition();
}

WizardPage::WizardPage(Plugin *plugin, const IDParam &pageId,
		       const Plugin::PageOpenParam &openParam)
	: PageBase(plugin, pageId, openParam), PageObservable(),
	  m_jobQueue(NULL), m_jobSlots(NULL), m_jobSlotIndex(-1)
{
	if (m_openParam.overwrite_draw_priority == -1)
		m_openParam.overwrite_draw_priority = PAGE_WIZARD_DEFAULT_BG_PRIORITY;
}

WizardPage::~WizardPage()
{
}

int WizardPage::DispatchDeferredJob()
{
	if (m_deferredJob.get() != NULL && m_jobSlots != NULL)
		m_jobSlots->Enqueue(m_jobSlotIndex, m_jobQueue, m_deferredJob);
	m_deferredJob.reset();
	return 0;
}

void WizardPage::OnTransitionFinished()
{
	NotifyObservers(0, IDParam(""));
	DispatchDeferredJob();
}
