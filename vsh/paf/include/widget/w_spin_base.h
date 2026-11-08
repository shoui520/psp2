/*
 * paf::ui::SpinBase: the base of the spin widgets (NumSpin, ItemSpin), a value in a range shown
 * as text (draw object 1, on its plane, draw object 0) that the derived widget turns into a
 * string, and while it is open a drum of the ten values around it (SpinDrum) the user turns by
 * touch, keys or focus. The class, TypeName, the parameter name and the exports are Sony's
 * (export NIDs hash as them); the members' names and SpinDrum are ours.
 */
#ifndef PAF_WIDGET_W_SPIN_BASE_H
#define PAF_WIDGET_W_SPIN_BASE_H

#include <stdint.h>
#include <math/ngp/matrix.h>
#include <std/string>
#include <std/vector>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class SpinDrum;

class SpinBase : public Widget {
public:
	/* its draw objects; the drum's ten digits follow as 2 to 11 */
	enum {
		OBJ_BASE = 0,
		OBJ_TEXT = 1,
		OBJ_DRUM = 2,
		OBJ_DRUM_END = 11
	};

	/* m_state: the drum turning, or at rest (names ours) */
	enum {
		STATE_TURNING = 0,
		STATE_IDLE = 1
	};

	/* the events it sends (names ours) */
	enum {
		CB_VALUE_CHANGED = 0x10000001,
		CB_TURN_START = 0x10000002,
		CB_TURN_END = 0x10000003
	};

	static const char m_widget_type[];
	static const char *TypeName();

	SpinBase(Widget *parent, CreateParam *param);

	virtual ~SpinBase()
	{
	}

	/* keys are not handled (-1) */
	virtual int32_t HandleKeycodeEvent(Event *e);
	/* a touch opens the drum where it is opened by a click */
	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* 2 to 11 are the drum's digits */
	virtual graph::DrawObj *GetDrawObj(int32_t obj);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	using Widget::SetTexture;
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	using Widget::GetTexture;
	/* the string shown in place of the value */
	virtual int32_t SetString(const wstring &str);
	using Widget::SetString;
	/* the drum's, else Widget's */
	virtual int32_t SetFocus(SetFocusInfo *info);
	/* Widget's (ScePafWidget 566E18A6), and the drum's */
	virtual void SetFocusDirection(int32_t dir, IDParam id);
	/* Widget's (ScePafWidget C71B79B2), and the drum's */
	virtual void SetFocusDirectionWidget(int32_t dir, Widget *widget);

	/* the slot after Widget's, at 0x170: the string of a value (name ours) */
	virtual wstring GetValueString(int32_t value) = 0;

	int32_t SetValue(int32_t value);
	int32_t SetFloatValue(float value);
	float GetFloatValue() const;
	int32_t SetRange(int32_t min, int32_t max);

	/* the text shown again, and the drum opened or closed (name ours) */
	void Refresh();
	/* the drum's value taken, unless the drum was only let go of where it commits by itself
	   (reason 0 let go, 15 stopped turning; name ours) */
	void OnDrumCommit(int32_t value, int32_t reason);
	/* the drum turning or at rest (STATE_*), the change sent as an event (name ours) */
	void SetSpinState(int32_t state);
	/* the range, value and loop the derived widget starts with (name ours) */
	int32_t InitSpin(int32_t min, int32_t max, int32_t value, bool loop);

	int32_t m_min;
	int32_t m_max;
	/* -1 for none */
	int32_t m_value;
	bool m_loop;
	/* shown in place of the value where it is set */
	wstring m_string;
	/* the active_on_click parameter: the drum opens at a click, else it is always open */
	int32_t m_active_on_click;
	/* set by InitSpin */
	bool m_initialized;
	/* nonzero while the drum is open */
	int32_t m_drum_open;
	/* the value was taken from the drum */
	bool m_value_changed;
	SpinDrum *m_drum;
	/* STATE_* */
	int32_t m_state;
};

/* a spin widget's parameters: active_on_click */
class SpinBaseParam : public WidgetParam {
public:
	/* the derived widget's; none is defined for SpinBaseParam, which is never made itself, so
	   no vtable is */
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Create(int32_t mode);

	bool m_active_on_click;
};

/* a value of the drum, as it is dragged: how far, and in how long (microseconds) (names
   ours) */
class SpinDrumSample {
public:
	float m_distance;
	uint64_t m_time;
};

/* the drum of a spin widget: ten digits, each a TextObj placed on a cylinder as its offset
   turns, 60 pixels to a value (class and names ours) */
class SpinDrum : public Widget {
public:
	static const char m_widget_type[];

	/* one digit's place on the cylinder */
	class Digit {
	public:
		math::matrix m_matrix;
		float m_y;
		float m_scale_y;
	};

	SpinDrum(Widget *parent, CreateParam *param);

	virtual ~SpinDrum()
	{
	}

	/* up and down turn it; the turn ends at the next value where the key is let go */
	virtual int32_t HandleKeycodeEvent(Event *e);
	/* touch, drag, flick and release */
	virtual int32_t HandleGestureEvent(Event *e);
	/* the turn ends at the next value where the focus leaves */
	virtual int32_t HandleFocusEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* the turn moved on, its end taken */
	virtual void UpdateAfterChildren();
	/* each digit drawn on its place of the cylinder */
	virtual int32_t OnDraw(graph::DrawInfo &info);
	/* the string shown for the blank value before the first */
	virtual int32_t SetString(const wstring &str);
	using Widget::SetString;

	/* up and down only, while its focus directions are off */
	virtual int32_t GetFocusDirectionMask()
	{
		return m_focus_direction_off ? 2 : 7;
	}

	/* the value at the offset, m_pos */
	void UpdatePos(float offset);
	/* the digits placed for the offset */
	void UpdateDigits(float offset);
	/* the turn ends at the value nearest pos, at the acceleration */
	void SnapTo(float pos, float accel);
	void Flick();
	void Drag();
	void TouchDown(uint32_t touch_id);
	/* the value at the offset taken by the spin widget */
	void Commit(int32_t reason);
	void Release();
	/* the turn moved on by one frame; true where it ended */
	bool Animate();

	int32_t m_min;
	/* m_min, one less where the blank value comes before it */
	int32_t m_first;
	int32_t m_max;
	/* the value at offset 0, -1 for m_first */
	int32_t m_value;
	bool m_loop;
	/* the value is taken when it is let go of */
	bool m_commit_on_release;
	/* the string of the blank value, where there is one */
	wstring m_blank;
	/* the value at its offset, -1.0 for the blank one */
	float m_pos;
	/* where the touch went down */
	float m_touch_y;
	/* the touch dragging it, -1 for none */
	int32_t m_touch_id;
	/* how far it is dragged */
	float m_drag;
	/* how far it is turned, 60 to a value */
	float m_offset;
	float m_velocity;
	uint32_t reserved;
	/* the drag's last moves */
	vector<SpinDrumSample> m_samples;
	uint64_t m_last_time;
	/* turning to m_target, at m_accel */
	bool m_snapping;
	float m_accel;
	float m_target;
	/* the direction a key turns it, 0 for none */
	float m_key_dir;
	Digit m_digits[10];
};

}
}

/* Exports with no name found for their NIDs. */
extern "C" {
/* the drum opened at a click, or always open (the active_on_click parameter) */
void ScePafWidget_176F3B21(paf::ui::SpinBase *spin, int32_t active_on_click);
}

#endif
