/*
 * paf::ui::FocusInfo (TOOL 0x811416a8..0x81144516): the focus cursor of one panel and the
 * widget it is on; as far as converted, the moves it remembers. No record or string names the
 * file.
 */
#include <stddef.h>
#include <stdint.h>
#include <widget/core/environment.h>
#include <widget/core/widget.h>
#include <widget/w_button_base.h>
#include <widget/w_list_item.h>
#include <widget/w_text.h>
#include <timer/timer.h>

#if !defined(FW_TYPE_CEX)
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...);
#endif

namespace paf {
namespace ui {

/* the child of the list item its text auto scroll target names, if it is of the type */
template<class T>
static inline __attribute__((always_inline)) T *FindAutoScrollChild(ListItem *item)
{
	T *found = NULL;
	IDParam id(item->m_text_auto_scroll_target);

	if (item->FindChild(id, 0) != NULL) {
		IDParam id2(item->m_text_auto_scroll_target);
		Widget *child = item->FindChild(id2, 0);

		if (child->IsInherit(T::TypeName())) {
			IDParam id3(item->m_text_auto_scroll_target);

			found = (T *)item->FindChild(id3, 0);
		}
	}
	return found;
}

void FocusInfo::UpdateHighlight()
{
	if (!m_shown)
		return;

	ListItem *item = ListItem::GetListItem(m_target);

	if (item != NULL) {
		ButtonBase *button = FindAutoScrollChild<ButtonBase>(item);

		if (button != NULL && button->IsLongPressTextAutoScrollEnabled()) {
			Event e;

			button->BeginLongPressTextAutoScroll(&e);
			button->EndLongPressTextAutoScroll(&e);
		}

		Text *text = FindAutoScrollChild<Text>(item);

		if (text != NULL) {
			bool on = false;

			ScePafWidget_222D55C7(text, &on);
			if (on) {
				Event e;

				text->HandleLongPress(&e);
				text->m_touch_held = false;
			}
		}
	} else {
		ButtonBase *button = (ButtonBase *)m_target;

		if (button != NULL && button->IsInherit(ButtonBase::TypeName())) {
			button = (ButtonBase *)m_target;
			if (button != NULL && button->IsLongPressTextAutoScrollEnabled()) {
				Event e;

				button->BeginLongPressTextAutoScroll(&e);
				button->EndLongPressTextAutoScroll(&e);
			}
		}
	}

	ListItem *previous_item = ListItem::GetListItem(m_previous);
	ButtonBase *previous = (ButtonBase *)m_previous;

	if (previous_item == NULL || previous_item == item) {
		if (m_target != (Widget *)previous && previous != NULL &&
		    previous->IsInherit(ButtonBase::TypeName())) {
			ButtonBase *button = (ButtonBase *)m_previous;

			if (button != NULL && button->IsLongPressTextAutoScrollEnabled())
				button->RequestTextAutoScrollStop();
		}
	} else {
		ButtonBase *button = FindAutoScrollChild<ButtonBase>(previous_item);

		if (button != NULL && button->IsLongPressTextAutoScrollEnabled())
			button->RequestTextAutoScrollStop();

		Text *text = FindAutoScrollChild<Text>(previous_item);

		if (text != NULL) {
			bool on = false;

			ScePafWidget_222D55C7(text, &on);
			if (on)
				text->StopAutoScroll();
		}
	}
}

#if !defined(FW_TYPE_CEX)
void FocusInfo::PrintTarget()
{
	Widget *target = m_target;

	if (target != NULL) {
		if (m_shown) {
			string name = target->m_name.m_id;

			if (name == "")
				name = "NO_NAME";
			ScePafAutoTestTty_9E10E873(2, "%s", name.c_str());
		} else {
			ScePafAutoTestTty_9E10E873(2, "");
		}
	}
}
#endif

void FocusInfo::Show()
{
	if (m_enabled && !m_shown) {
		m_was_shown = m_shown;
		m_shown = true;
		if (m_cursor != NULL) {
			intrusive_ptr<graph::Surface> texture;

			m_cursor->GetTexture(texture);
			if (texture.get() == NULL) {
				{
					intrusive_ptr<graph::Surface> fallback;

					ScePafWidget_B6C64486(fallback, 0);
				}
				m_default_texture = true;
			}
		}
		UpdateHighlight();
#if !defined(FW_TYPE_CEX)
		PrintTarget();
#endif
	}
}

void FocusInfo::Hide()
{
	if (m_enabled && m_shown) {
		m_was_shown = m_shown;
		m_shown = false;

		Widget *cursor = m_cursor;

		if (cursor != NULL) {
			uint32_t mode = m_move_mode;
			Widget *target = m_target;
			float delay = 250.0f;

			if (mode == 1 || mode == 6 || mode == 7 || target == NULL)
				delay = 0.0f;
			if (target != NULL && !target->IsInputTarget())
				delay = 0.0f;
			if (delay > 0.0f) {
				Timer *timer = new Timer(delay);
				float total = timer != NULL ? timer->GetTotalTime() : 0.0f;

				cursor->m_transition_complete = false;
				cursor->m_transition = 0xff;
				cursor->DoTransitionReverse_core(total, true);
				ScePafWidget_4588699E(cursor, timer, 2, 0.0f, NULL);
			} else {
				cursor->m_transition_complete = false;
				cursor->m_transition = 0xff;
				cursor->DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
			}
			cursor = m_cursor;
			cursor->SetMetaAlpha(1.0f, new Timer(delay), 0x10003, 2, 0.0f, NULL);
#if !defined(FW_TYPE_CEX)
			PrintTarget();
#endif
		}
	}
}

void FocusInfo::AddFocusHistory(Widget *widget, Widget *next, uint32_t dir)
{
	if (widget != next) {
		uint32_t count = m_history_count;

		if (count == 1)
			count = m_history_count - 1;
		m_history_count = count + 1;
		m_history[count].m_widget = widget;
		m_history[count].m_next = next;
		m_history[count].m_dir = dir;
	}
}

void FocusInfo::RemoveFocusHistory(Widget *widget)
{
	uint32_t count = m_history_count;

	if (count != 0) {
		if (widget != NULL) {
			History *end = &m_history[0] + count;
			History *begin = &m_history[0];
			History *found = end;

			if (end != begin) {
				while (found[-1].m_widget != widget && found[-1].m_next != widget) {
					found--;
					if (found == begin)
						return;
				}

				History *from = found;
				History *to = begin;

				if (found != end) {
					do {
						*to = *from;
						from++;
						to++;
					} while (from != end);
					count = m_history_count;
				}
				m_history_count = count - (found - begin);
			}
		} else {
			m_history_count = 0;
		}
	}
}

Widget *FocusInfo::FindFocusHistory(Widget *widget, uint32_t dir)
{
	/* the direction asked for and the one it was made in */
	const uint32_t opposite[4][2] = { { 1, 2 }, { 2, 1 }, { 3, 4 }, { 4, 3 } };
	Widget *result = NULL;

	if (m_history_count != 0) {
		int32_t i = 0;

		while (opposite[i][0] != dir) {
			i++;
			if (i >= 4)
				return result;
		}

		uint32_t want = opposite[i][1];

		if (want != 0) {
			History *begin = &m_history[0];
			History *entry = begin + m_history_count;

			if (entry != begin) {
				while (entry[-1].m_next != widget || want != entry[-1].m_dir) {
					entry--;
					if (entry == begin)
						return result;
				}
				result = entry[-1].m_widget;
				m_history_count = m_history_count - (begin + m_history_count - entry);
			}
		}
	}
	return result;
}

}
}
