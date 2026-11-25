/*
 * SceCompanionDialogPlugin: FurlStack implementation.
 */
#include "furl_stack.h"
#include <versatileparam/idparam.h>
#include <widget/core/event.h>

static const SceUInt32 kFurlAnimMove = 0x10000;
static const SceUInt32 kFurlAnimRot = 0x10007;
static const SceUInt32 kFurlAnimAlpha = 0x10003;
static const SceUInt32 kFurlAnimPivot = 0x10008;
static const SceUInt32 kOverlayFadeTimerId = 0x0100A4F5;

FurlStack::FurlStack(bool useOverlay)
	: m_pad(0)
	, m_enterOffsetX(960.0f)
	, m_enterOffsetY(330.0f)
	, m_enterRotY(-0.78539819f)
	, m_enterRotZ(0.0f)
	, m_secondOffsetX(-270.0f)
	, m_secondOffsetY(-330.0f)
	, m_secondRotY(1.57079637f)
	, m_secondAlpha(0.15f)
	, m_bgOffsetX(-270.0f)
	, m_bgOffsetY(-330.0f)
	, m_bgRotY(1.85004902f)
	, m_bgAlpha(0.0f)
	, m_notifyCb(0)
	, m_notifyUserData(0)
	, m_overlayFadeMs(500.0f)
	, m_useOverlay(useOverlay)
{
}

FurlStack::~FurlStack()
{
	Clear();
}

void FurlStack::OnOverlayClicked()
{
	if (m_notifyCb != 0)
		m_notifyCb(this, m_notifyUserData);
}

void FurlStack::SetNotifyCallback(FurlStackNotifyCallback cb, void *userData)
{
	m_notifyCb = cb;
	m_notifyUserData = userData;
}

void FurlStack::AnimateWidget(paf::ui::Widget *widget, const paf::math::v4 &pos,
	const paf::math::v4 &rot, float alpha, float durationMs)
{
	(void)rot;
	(void)alpha;
	(void)durationMs;
	if (widget == 0)
		return;
	widget->SetPos(pos);
}

void FurlStack::OnWidgetDestroyed(paf::ui::Widget *widget, void *userData)
{
	FurlStack *self = static_cast<FurlStack *>(userData);
	if (self == 0)
		return;
	for (SceSize idx = 0; idx < self->m_entries.size(); ++idx) {
		if (self->m_entries[idx].widget == widget) {
			self->RemoveEntry(idx, true);
			break;
		}
	}
}

void FurlStack::OnOverlayButtonEvent(SceInt32 eventId, paf::ui::Handler *handler,
	paf::ui::Event *event, void *userData)
{
	(void)eventId;
	(void)handler;
	(void)event;
	FurlStack *self = static_cast<FurlStack *>(userData);
	if (self != 0)
		self->OnOverlayClicked();
}

void FurlStack::Push(paf::ui::Widget *widget)
{
	if (widget == 0)
		return;

	FurlStackEntry entry;
	entry.widget = widget;
	entry.pad = 0;
	entry.initialPos = paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	entry.initialRot = paf::math::v4(0.0f, 0.0f, 0.0f, 1.0f);
	entry.initialAlpha = 1.0f;
	entry.savedAnchorX = 0;
	entry.furlPos = entry.initialPos;
	entry.layerState = FURL_STATE_TOP;
	entry.reserved = 0;

	m_entries.push_back(entry);

	if (m_entries.size() >= 2) {
		FurlStackEntry &top = m_entries[m_entries.size() - 1];
		paf::math::v4 enterPos(m_enterOffsetX, top.furlPos.extract_y(), m_enterOffsetY, 0.0f);
		paf::math::v4 enterRot(0.0f, m_enterRotY, 0.0f, 1.0f);
		AnimateWidget(top.widget, enterPos, enterRot, 1.0f, 0.0f);
		AnimateWidget(top.widget, top.furlPos, top.initialRot, top.initialAlpha, 300.0f);
	}
	Layout(false);
}

void FurlStack::RemoveEntry(SceSize index, bool destroying)
{
	if (index >= m_entries.size())
		return;
	FurlStackEntry &entry = m_entries[index];
	if (!destroying && entry.widget != 0) {
		paf::IDParam overlayId("furl_stack_overlay_button");
		paf::ui::Widget *overlay = entry.widget->FindChild(overlayId, 2);
		(void)overlay;
	}
	for (SceSize idx = index; idx + 1 < m_entries.size(); ++idx)
		m_entries[idx] = m_entries[idx + 1];
	m_entries.pop_back();
}

void FurlStack::Pop()
{
	if (m_entries.size() == 0)
		return;
	RemoveEntry(m_entries.size() - 1, false);
	Layout(false);
}

void FurlStack::Clear()
{
	while (m_entries.size() > 0) {
		RemoveEntry(m_entries.size() - 1, false);
		Layout(false);
	}
}

void FurlStack::Layout(bool immediate)
{
	SceSize count = m_entries.size();
	float dur = immediate ? 0.0f : 300.0f;

	if (count >= 3) {
		for (SceSize idx = 0; idx + 2 < count; ++idx) {
			FurlStackEntry &entry = m_entries[idx];
			if (immediate || entry.layerState != FURL_STATE_BACKGROUND) {
				entry.layerState = FURL_STATE_BACKGROUND;
				paf::math::v4 pos(m_bgOffsetX, entry.furlPos.extract_y(), m_bgOffsetY, 0.0f);
				paf::math::v4 rot(0.0f, m_bgRotY, 0.0f, 1.0f);
				AnimateWidget(entry.widget, pos, rot, m_bgAlpha, dur);
			}
		}
	}

	if (count >= 2) {
		FurlStackEntry &second = m_entries[count - 2];
		if (immediate || second.layerState != FURL_STATE_SECOND) {
			second.layerState = FURL_STATE_SECOND;
			paf::math::v4 pos(m_secondOffsetX, second.furlPos.extract_y(), m_secondOffsetY, 0.0f);
			paf::math::v4 rot(0.0f, m_secondRotY, 0.0f, 1.0f);
			AnimateWidget(second.widget, pos, rot, m_secondAlpha, dur);
		}
	}

	if (count >= 1) {
		FurlStackEntry &top = m_entries[count - 1];
		if (immediate || top.layerState != FURL_STATE_TOP) {
			top.layerState = FURL_STATE_TOP;
			AnimateWidget(top.widget, top.furlPos, top.initialRot, top.initialAlpha, dur);
		}
	}
}
