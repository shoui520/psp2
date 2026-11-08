/*
 * paf::ui::NumSpin: a spin widget of numbers, each shown with at least m_digit digits. The class,
 * TypeName, Create and the parameter names are Sony's (export NIDs hash as them; "num_spin", "min",
 * "max", "value", "loop", "digit" are its strings); the members' names are ours, after the
 * parameters.
 */
#ifndef PAF_WIDGET_W_NUM_SPIN_H
#define PAF_WIDGET_W_NUM_SPIN_H

#include <stdint.h>
#include <std/string>
#include <widget/w_spin_base.h>

namespace paf {
namespace ui {

class NumSpin : public SpinBase {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	NumSpin(Widget *parent, CreateParam *param);

	virtual ~NumSpin()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* the value with at least m_digit digits, leading zeros added */
	virtual wstring GetValueString(int32_t value);

	/* the range, value, loop and digits it starts with; digit 0 for as many as max has */
	int32_t Create(int32_t min, int32_t max, int32_t value, bool loop, int32_t digit);

	/* the fewest digits a value is shown with, -1 for no leading zeros */
	int32_t m_digit;
};

/* a NumSpin's parameters, by their resource names */
class NumSpinParam : public SpinBaseParam {
public:
	static const char m_param_name[];

	NumSpinParam() : m_min(0), m_max(0), m_value(0), m_loop(false), m_digit(0)
	{
	}

	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Create(int32_t mode);

	int32_t m_min;
	int32_t m_max;
	int32_t m_value;
	bool m_loop;
	int32_t m_digit;
};

}
}

#endif
