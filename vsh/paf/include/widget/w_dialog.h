/*
 * paf::ui::Dialog: a box that takes every point and touch event it is sent, and finds its parts
 * (the widgets it is made of) by index through the list its owner gives it. Names after the
 * exports (SetPartsList, GetPartsWidgetId, GetPartsWidget, SetPartsString, Dialog::PartsList);
 * the members are ours.
 */
#ifndef PAF_WIDGET_W_DIALOG_H
#define PAF_WIDGET_W_DIALOG_H

#include <stdint.h>
#include <std/map>
#include <std/string>
#include <versatileparam/idparam.h>
#include <widget/w_box.h>

namespace paf {
namespace ui {

class Dialog : public Box {
public:
	/* one part: its index and its widget's ID; a list ends at index 0 */
	struct PartsList {
		int32_t index;
		IDParam id;
	};

	Dialog(Widget *parent);

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	static const char m_widget_type[];
	static const char *TypeName();

	virtual ~Dialog();
	/* consumed, so nothing behind it gets them */
	virtual int32_t HandlePointEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);

	/* the parts of the list, each index to its ID */
	void SetPartsList(const PartsList *list);
	/* the ID of the part, or an empty one */
	const IDParam &GetPartsWidgetId(int32_t index);
	/* the widget of the part among its children, NULL when it has none */
	Widget *GetPartsWidget(int32_t index);
	/* the string of the part's widget; -1 without one */
	int32_t SetPartsString(int32_t index, const wstring &str);

	/* its parts' IDs by index */
	map<int32_t, IDParam> m_parts;
};

}
}

#endif
