/*
 * paf::ui::NumSpin (DEBUGGER 0x81100d0c..0x81101170, its destructors and its parameters' at
 * 0x8122d3dc..0x8122d77c): a spin widget of numbers. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <versatileparam/resourceparam.h>
#include <widget/factory.h>
#include <widget/w_num_spin.h>
#include "../paf_types.h"

namespace paf {
namespace ui {

NumSpin::NumSpin(Widget *parent, CreateParam *param) : SpinBase(parent, param), m_digit(-1)
{
}

const char NumSpin::m_widget_type[] = "num_spin";

bool NumSpin::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return SpinBase::IsInherit(type);
}

char *NumSpin::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *NumSpin::TypeName()
{
	return m_widget_type;
}

int32_t NumSpin::Create(int32_t min, int32_t max, int32_t value, bool loop, int32_t digit)
{
	m_digit = digit;
	if (digit == 0) {
		int32_t count = 1;

		for (int32_t rest = sce_paf_abs(max) / 10; rest > 0; rest /= 10)
			count++;
		m_digit = count;
	}
	return InitSpin(min, max, value, loop);
}

const char NumSpinParam::m_param_name[] = "num_spin";

wstring NumSpin::GetValueString(int32_t value)
{
	wchar_t format[8];
	wchar_t str[64];
	const wchar_t *fmt = L"%d";

	if (m_digit >= 0) {
		fmt = format;
		sce_paf_swprintf(format, 8, L"%%0%dd", m_digit);
	}
	sce_paf_swprintf(str, 64, fmt, value);
	return wstring(str);
}

const char *NumSpinParam::GetParamName() const
{
	return m_param_name;
}

int32_t NumSpinParam::Create(int32_t mode)
{
	int32_t ret = SpinBaseParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("min", &m_min, ResourceParam::SEMANTIC_INT);
		if (m_serial != NULL) {
			m_serial->AddParam("max", &m_max, ResourceParam::SEMANTIC_INT);
			if (m_serial != NULL) {
				m_serial->AddParam("value", &m_value, ResourceParam::SEMANTIC_INT);
				if (m_serial != NULL) {
					m_serial->AddParam("loop", &m_loop,
							   ResourceParam::SEMANTIC_BOOL);
					if (m_serial != NULL)
						m_serial->AddParam("digit", &m_digit,
								   ResourceParam::SEMANTIC_INT);
				}
			}
		}
	}
	return 0;
}

int32_t NumSpinParam::Set()
{
	int32_t ret = SpinBaseParam::Set();

	if (ret < 0)
		return ret;
	((NumSpin *)m_widget)->Create(m_min, m_max, m_value, m_loop, m_digit);
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewNumSpin(Widget *parent)
{
	return new NumSpin(parent, NULL);
}

static WidgetParam *NewNumSpinParam()
{
	return new NumSpinParam;
}

static VersatileParam *NewNumSpinLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewNumSpinFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewNumSpinStyleParam()
{
	return new WidgetStyleParam;
}

static VersatileParam *NewNumSpinHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewNumSpinSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryNumSpin = {
	NumSpin::TypeName(),
	NewNumSpin,
	NewNumSpinParam,
	NewNumSpinLayoutParam,
	NewNumSpinFocusParam,
	NewNumSpinStyleParam,
	NewNumSpinHandlerParam,
	NewNumSpinSoundParam,
};
