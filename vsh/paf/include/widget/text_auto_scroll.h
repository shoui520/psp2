/*
 * The auto scroll of a text widget (Text::StartAutoScroll): a text too long for its box scrolls
 * through it, horizontally or vertically, fades out at the end and starts again, or (the long
 * press scroll types) scrolls through once while the press lasts. Its friends, other texts of the
 * same parent ("auto_scroll_target_friend"), scroll as far as the longest of them. The class,
 * method and member names are ours (after the parameters "auto_scroll_speed",
 * "text_auto_scroll_target" and "auto_scroll_target_friend").
 */
#ifndef PAF_WIDGET_TEXT_AUTO_SCROLL_H
#define PAF_WIDGET_TEXT_AUTO_SCROLL_H

#include <stdint.h>
#include <widget/w_text_box.h>
#include <widget/core/animlistener.h>
#include <graphics/drawobj/text_obj.h>
#include <versatileparam/idparam.h>
#include <math/ngp/v2.h>
#include <std/vector>

namespace paf {
namespace ui {

/* 0x38 bytes */
class TextAutoScroll {
public:
	/* the scroll that loops: a wait, the scroll, a wait and the fade back (name ours) */
	enum {
		ANIM_AUTO_SCROLL_LOOP = 0x10000000 | Text::ANIM_AUTO_SCROLL
	};

	/* what waits for the texts to be laid out (SetIntervalEvent's ID, name ours) */
	enum {
		TIMER_WAIT_LAYOUT = 0x8000
	};

	/* 0x811ad35c (TOOL) */
	TextAutoScroll(Text *text, graph::TextObj *text_obj);
	/* 0x811ad3b4 */
	virtual ~TextAutoScroll();

	/* 0x811ad486: the text's color with the alpha */
	int32_t SetAlpha(float alpha);
	/* 0x811ad4ee: the layout as it was before the scroll, and the scroll stopped */
	int32_t Restore();
	/* 0x811ad6cc */
	int32_t Stop();
	/* 0x811ad6f8: the bounds of the text, or of the longest of its friends */
	void GetFriendBounds(math::v2 &bounds);
	/* 0x811ad864: no friend still being laid out */
	bool IsFriendReady();
	/* 0x811ad946 */
	bool IsRunning();
	/* 0x811ad956: the scroll's animations made for the text as it is laid out now */
	int32_t Update();
	/* 0x811ae1a4 */
	int32_t Start(Text::ScrollType type);
	/* 0x811ae1ce: the speed for a press at the position, from where the text was when the
	   press began */
	int32_t GetSpeedByPos(const math::v2 &pos, float &speed, bool set_origin, bool reverse);
	/* 0x811ae32c: mode 1 restores the layout before the scroll starts again */
	int32_t SetSpeed(float speed, int32_t mode);
	/* 0x811ae382 */
	int32_t GetSpeed(float &speed);
	/* 0x811ae38e */
	static int32_t GetDefaultSpeed(float &speed, Text::ScrollType type);
	/* 0x811ae3d0 */
	int32_t AddFriend(const IDParam &id);
	/* 0x811ae466 */
	int32_t RemoveFriend(const IDParam &id);

	/* 0x811ae4cc: the texts laid out, the scroll starts */
	static void OnLayoutReady(int32_t type, Handler *self, Event *e, void *data);
	/* 0x811ae500: the text fades out, and a scroll that runs once ends */
	static void OnFadeTick(Timer *t, AnimListener *listener, void *data1, void *data2,
			       void *data3);
	/* 0x811ae53a */
	static void OnScrollTick(Timer *t, AnimListener *listener, void *data1, void *data2,
				 void *data3);
	/* 0x811ae5a8: the text fades in, and the scroll ends */
	static void OnFadeBackTick(Timer *t, AnimListener *listener, void *data1, void *data2,
				   void *data3);

	/* the text back at its start (inlined in Sony's code, name ours) */
	void ResetPos()
	{
		m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos,
					       math::v2(0.0f, 0.0f));
	}

	/* the scroll types that run once */
	bool IsOnce()
	{
		return m_type == Text::SCROLL_TYPE_HORIZONTAL_ONCE ||
		       m_type == Text::SCROLL_TYPE_VERTICAL_ONCE;
	}

	/* the text's animation of the ID still running */
	bool IsAnimRunning(int32_t id)
	{
		if (m_text != NULL) {
			AnimQueue *queue = m_text->FindAnim(id);

			if (queue != NULL && !queue->IsFinished())
				return true;
		}
		return false;
	}

	Text *m_text;
	graph::TextObj *m_text_obj;
	vector<IDParam> m_friends;
	/* the ellipsis and the word wrap the scroll turned off */
	bool m_ellipsis : 1;
	bool m_word_wrap : 1;
	/* m_length measured by the last Update */
	bool m_length_new : 1;
	/* a Text::ScrollType, 0 while it does not scroll */
	uint32_t m_type : 4;
	/* points per second */
	float m_speed;
	/* how far it scrolls */
	math::v2 m_length;
	/* where the text was when the press began */
	math::v2 m_origin;
};

}
}

#endif
