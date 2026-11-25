/*
 * SceCompanionDialogPlugin: FurlStack page-furling widget stack controller.
 */
#ifndef COMPANION_FURL_STACK_H
#define COMPANION_FURL_STACK_H

#include <scetypes.h>
#include <std/vector>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>

enum FurlLayerState {
	FURL_STATE_TOP = 0,
	FURL_STATE_SECOND = 1,
	FURL_STATE_BACKGROUND = 2
};

struct FurlStackEntry {
	paf::ui::Widget *widget;
	SceUInt32 pad;
	paf::math::v4 initialPos;
	paf::math::v4 initialRot;
	float initialAlpha;
	SceInt32 savedAnchorX;
	paf::math::v4 furlPos;
	SceInt32 layerState;
	SceUInt32 reserved;
};

typedef void (*FurlStackNotifyCallback)(class FurlStack *stack, void *userData);

class FurlStack {
public:
	explicit FurlStack(bool useOverlay);
	virtual ~FurlStack();

	virtual void OnOverlayClicked();
	virtual void Layout(bool immediate);

	void Push(paf::ui::Widget *widget);
	void Pop();
	void Clear();
	void SetNotifyCallback(FurlStackNotifyCallback cb, void *userData);

private:
	void RemoveEntry(SceSize index, bool destroying);
	static void AnimateWidget(paf::ui::Widget *widget, const paf::math::v4 &pos,
		const paf::math::v4 &rot, float alpha, float durationMs);
	static void OnWidgetDestroyed(paf::ui::Widget *widget, void *userData);
	static void OnOverlayButtonEvent(SceInt32 eventId, paf::ui::Handler *handler,
		paf::ui::Event *event, void *userData);

	paf::vector<FurlStackEntry> m_entries;
	SceUInt32 m_pad;
	float m_enterOffsetX;
	float m_enterOffsetY;
	float m_enterRotY;
	float m_enterRotZ;
	float m_secondOffsetX;
	float m_secondOffsetY;
	float m_secondRotY;
	float m_secondAlpha;
	float m_bgOffsetX;
	float m_bgOffsetY;
	float m_bgRotY;
	float m_bgAlpha;
	FurlStackNotifyCallback m_notifyCb;
	void *m_notifyUserData;
	float m_overlayFadeMs;
	bool m_useOverlay;
};

#endif /* COMPANION_FURL_STACK_H */
