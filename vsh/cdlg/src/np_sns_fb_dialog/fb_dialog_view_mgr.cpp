/*
 * SceNpSnsFacebookDialogPlugin: FbViewBase defaults and FbViewManager stack management.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/new>
#include <std/string>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include "fb_dialog_plugin.h"
#include "fb_dialog_job.h"
#include "fb_dialog_view_mgr.h"
#include "fb_dialog_controller.h"

using namespace paf;

static const char s_fbEmptyName[] = "";

/* FbViewBase */
FbViewBase::~FbViewBase()
{
}

IDParam FbViewBase::GetId() const
{
	return m_id;
}

int FbViewBase::Open()
{
	return 0;
}

int FbViewBase::Suspend()
{
	return 0;
}

int FbViewBase::Resume()
{
	return 0;
}

int FbViewBase::Close()
{
	return 0;
}

int FbViewBase::IsOpen() const
{
	return m_open;
}

int FbViewBase::IsDismissed() const
{
	return 0;
}

int FbViewBase::IsOverlay() const
{
	return 0;
}

int FbViewBase::SetBgColor(const math::v4 &color)
{
	(void)color;
	return 0;
}

int FbViewBase::OnBgAction()
{
	return 0;
}

void FbViewBase::SetParamInt1(int value)
{
	(void)value;
}

void FbViewBase::SetParamInt2(int value)
{
	(void)value;
}

void FbViewBase::SetParamPtr1(const void *ptr)
{
	(void)ptr;
}

void FbViewBase::SetParamPtr2(const void *ptr)
{
	(void)ptr;
}

void FbViewBase::SetParamPtr3(const void *ptr)
{
	(void)ptr;
}

void FbViewBase::SetParamPtr4(const void *ptr)
{
	(void)ptr;
}

/* FbViewManager */
FbViewManager::FbViewManager(FbEventBus *bus, Plugin *plugin)
	: m_aborting(0), m_bus(bus), m_plugin(plugin), m_useLongInput(0), m_stackDepth(0),
	  m_views(), m_reserved(0), m_layer1Id(s_fbEmptyName, 0), m_layer2Id(s_fbEmptyName, 0)
{
	m_bus->AddListener(this);
}

FbViewManager::~FbViewManager()
{
	m_bus->RemoveListener(this);
	for (size_t i = 0; i < m_views.size(); ++i) {
		if (m_views[i] != NULL)
			delete m_views[i];
	}
	m_views.clear();
}

void FbViewManager::OnAttach(FbDialogBase *owner)
{
	m_owner = owner;
	if (owner == NULL)
		return;
	for (size_t i = 0; i < m_views.size(); ++i)
		m_owner->AddChild(m_views[i]);
}

void FbViewManager::OnDetach()
{
	if (m_owner != NULL) {
		for (size_t i = 0; i < m_views.size(); ++i)
			m_owner->RemoveChild(m_views[i]);
	}
	m_owner = NULL;
}

int FbViewManager::OnEvent(int event, void *arg)
{
	(void)event;
	(void)arg;
	return 0;
}

int FbViewManager::AddView(FbViewBase *view)
{
	if (view == NULL)
		return FB_DIALOG_ERROR_INVALID_STATE;
	m_views.push_back(view);
	if (m_owner != NULL)
		m_owner->AddChild(view);
	return 0;
}

void FbViewManager::SetAborting()
{
	m_aborting = 1;
}

FbViewBase *FbViewManager::FindView(uint32_t idHash) const
{
	for (size_t i = 0; i < m_views.size(); ++i) {
		if (m_views[i]->GetId().GetIDHash() == idHash)
			return m_views[i];
	}
	return NULL;
}

int FbViewManager::SetBgColor(const math::v4 &color)
{
	IDParam id(s_fbEmptyName, FB_VIEW_BG_ID);
	FbViewBase *v = FindView(id.GetIDHash());

	if (v == NULL)
		return FB_DIALOG_ERROR_INVALID_STATE;
	return v->SetBgColor(color);
}

int FbViewManager::OnBgAction()
{
	IDParam id(s_fbEmptyName, FB_VIEW_BG_ID);
	FbViewBase *v = FindView(id.GetIDHash());

	if (v == NULL)
		return 0;
	return v->OnBgAction();
}

int FbViewManager::CloseTop()
{
	if (m_stackDepth == 2) {
		FbViewBase *v = FindView(m_layer1Id.GetIDHash());

		if (v == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		int ret = v->Close();

		if (ret >= 0)
			m_stackDepth = 1;
		return ret;
	}
	if (m_stackDepth == 3) {
		FbViewBase *v = FindView(m_layer2Id.GetIDHash());

		if (v == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		int ret = v->Close();

		if (ret >= 0)
			m_stackDepth = 2;
		return ret;
	}
	if (m_stackDepth >= 0 && m_stackDepth <= 1)
		return 0;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbViewManager::CloseAllLayers()
{
	if (m_stackDepth == 2)
		return CloseTop();
	if (m_stackDepth == 3) {
		int ret = CloseTop();

		if (ret >= 0)
			ret = CloseTop();
		return ret;
	}
	if (m_stackDepth >= 0 && m_stackDepth <= 1)
		return 0;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbViewManager::CheckTopDismissed()
{
	if (m_stackDepth == 2) {
		FbViewBase *v = FindView(m_layer1Id.GetIDHash());
		int ret = 0;

		if (v == NULL) {
			m_stackDepth = 1;
			return FB_DIALOG_ERROR_INVALID_STATE;
		}
		if (v->IsDismissed() == 0)
			ret = FB_DIALOG_ERROR_INVALID_STATE;
		m_stackDepth = 1;
		return ret;
	}
	if (m_stackDepth == 3) {
		FbViewBase *v = FindView(m_layer2Id.GetIDHash());
		int ret = 0;

		if (v == NULL) {
			m_stackDepth = 2;
			return FB_DIALOG_ERROR_INVALID_STATE;
		}
		if (v->IsDismissed() == 0)
			ret = FB_DIALOG_ERROR_INVALID_STATE;
		m_stackDepth = 2;
		return ret;
	}
	if (m_stackDepth >= 0 && m_stackDepth <= 1)
		return 0;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbViewManager::CloseBg()
{
	IDParam id(s_fbEmptyName, FB_VIEW_BG_ID);
	FbViewBase *v = FindView(id.GetIDHash());

	if (v == NULL)
		return FB_DIALOG_ERROR_INVALID_STATE;
	if (m_stackDepth == 0)
		return 0;
	if (m_stackDepth == 1) {
		int ret = v->Close();

		m_stackDepth = 0;
		return ret;
	}
	if (m_stackDepth >= 2 && m_stackDepth <= 3) {
		CloseAllLayers();
		int ret = v->Close();

		m_stackDepth = 0;
		return ret;
	}
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbViewManager::CloseAll()
{
	int ret = CloseTop();

	if (m_stackDepth == 1) {
		ret = CloseBg();
		m_stackDepth = 0;
		return ret;
	}
	if (m_stackDepth >= 2 && m_stackDepth <= 3) {
		CloseAllLayers();
		ret = CloseBg();
		m_stackDepth = 0;
		return ret;
	}
	if (m_stackDepth == 0)
		return ret;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbViewManager::OpenBg()
{
	IDParam id(s_fbEmptyName, FB_VIEW_BG_ID);
	FbViewBase *v = FindView(id.GetIDHash());

	if (v == NULL)
		return FB_DIALOG_ERROR_INVALID_STATE;
	if (m_stackDepth == 0) {
		int ret = v->Open();

		m_stackDepth = 1;
		return ret;
	}
	if (m_stackDepth >= 1 && m_stackDepth <= 3)
		return 0;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbViewManager::ShowView(const IDParam &id, int p1, int p2, const void *p3, const void *p4,
			    const void *p5, const void *p6)
{
	FbViewBase *v = FindView(id.GetIDHash());

	if (v == NULL)
		return FB_DIALOG_ERROR_INVALID_STATE;
	v->SetParamInt1(p1);
	v->SetParamInt2(p2);
	v->SetParamPtr1(p3);
	v->SetParamPtr2(p4);
	v->SetParamPtr3(p5);
	v->SetParamPtr4(p6);
	int isOverlay = v->IsOverlay();

	if (m_stackDepth == 0 || m_stackDepth == 1) {
		if (m_stackDepth == 0)
			OpenBg();
		int ret = v->Open();

		m_layer1Id = id;
		m_stackDepth = 2;
		return ret;
	}
	if (m_stackDepth == 2) {
		FbViewBase *cur = FindView(m_layer1Id.GetIDHash());

		if (cur == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		if (isOverlay != 0 && cur->IsOverlay() == 0) {
			cur->Suspend();
			int ret = v->Open();

			m_layer2Id = id;
			m_stackDepth = 3;
			return ret;
		}
		int ret;

		if (m_layer1Id.GetIDHash() == id.GetIDHash()) {
			ret = v->Resume();
		} else {
			ret = CloseTop();
			if (ret < 0)
				return ret;
			ret = v->Open();
			m_layer1Id = id;
		}
		m_stackDepth = 2;
		return ret;
	}
	if (m_stackDepth == 3) {
		if (isOverlay != 0) {
			int ret = CloseTop();

			if (ret >= 0) {
				ret = v->Open();
				m_layer2Id = id;
				m_stackDepth = 3;
			}
			return ret;
		}
		if (m_layer1Id.GetIDHash() == id.GetIDHash()) {
			int ret = CloseTop();

			if (ret >= 0) {
				FbViewBase *base = FindView(id.GetIDHash());

				if (base == NULL)
					return FB_DIALOG_ERROR_INVALID_STATE;
				ret = base->Resume();
				m_stackDepth = 2;
			}
			return ret;
		}
		int ret = CloseAllLayers();

		if (ret >= 0) {
			ret = v->Open();
			m_layer1Id = id;
			m_stackDepth = 2;
		}
		return ret;
	}
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbViewManager::ShowLogin(const void *titleName, const void *account, const void *password)
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_LOGIN_ID), 0, 0, NULL, titleName, account,
			password);
}

int FbViewManager::ShowConfirmBind(const void *titleName)
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_CONFIRM_BIND_ID), 0, 0, NULL, titleName,
			NULL, NULL);
}

int FbViewManager::ShowInput(const void *account, const void *password)
{
	if (m_useLongInput != 0)
		return ShowView(IDParam(s_fbEmptyName, FB_VIEW_INPUT_LONG_ID), 0, 0, NULL, NULL,
				account, password);
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_INPUT_SHORT_ID), 0, 0, NULL, NULL, account,
			password);
}

int FbViewManager::ShowPermission(const void *permList, const void *titleName)
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_PERMISSION_ID), 0, 0, permList, titleName,
			NULL, NULL);
}

int FbViewManager::ShowWait()
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_WAIT_ID), 0, 0, NULL, NULL, NULL, NULL);
}

int FbViewManager::ShowError(int error)
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_ERROR_ID), error, 0, NULL, NULL, NULL, NULL);
}

int FbViewManager::ShowChatRestrict(int error)
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_CHAT_RESTRICT_ID), error, 0, NULL, NULL, NULL,
			NULL);
}

int FbViewManager::ShowFeedConfirm(int postType, int feedData)
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_FEED_CONFIRM_ID), postType, feedData, NULL,
			NULL, NULL, NULL);
}

int FbViewManager::ShowRetryAuth(int error)
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_RETRY_AUTH_ID), error, 0, NULL, NULL, NULL,
			NULL);
}

int FbViewManager::ShowRetryServer(int error)
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_RETRY_SERVER_ID), error, 0, NULL, NULL, NULL,
			NULL);
}

int FbViewManager::ShowNotice(int error)
{
	return ShowView(IDParam(s_fbEmptyName, FB_VIEW_NOTICE_ID), error, 0, NULL, NULL, NULL,
			NULL);
}

int FbViewManager::IsWaitOpen()
{
	IDParam id(s_fbEmptyName, FB_VIEW_WAIT_ID);
	FbViewBase *v = FindView(id.GetIDHash());

	if (v == NULL)
		return 0;
	return v->IsOpen();
}
