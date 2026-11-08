/*
 * paf::ui::FocusInfo (TOOL 0x811416a8..0x81144516): the focus cursor of one panel and the
 * widget it is on; as far as converted, the moves it remembers. No record or string names the
 * file.
 */
#include <stddef.h>
#include <stdint.h>
#include <widget/core/environment.h>

namespace paf {
namespace ui {

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
