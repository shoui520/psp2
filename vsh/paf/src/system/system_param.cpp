/*
 * paf::system::SystemParam: the text style the Framework asks for, and three
 * system parameters read as flags, read again when ScePafToplevel asks.
 */
#include <system/system.h>

using namespace paf::system;

static SystemParam s_system_param;

void SystemParam::SetTextStyle(char style)
{
	s_system_param.text_style = style;
}

char SystemParam::GetTextStyle()
{
	return s_system_param.text_style;
}

void SystemParam::SetParamFlags(int32_t flags)
{
	s_system_param.param_flags = flags;
}

int32_t ScePafMisc_C51EF81D(void)
{
	return s_system_param.param_flags;
}

int32_t SystemParam::ReadParamFlags()
{
	int32_t flags = 0;
	int value;

	if (sceRegMgrSystemParamGetInt(ID_23FC2, &value) == 0)
		flags = value == 1;
	if (sceRegMgrSystemParamGetInt(ID_611DC9, &value) == 0 && value == 1)
		flags |= 2;
	if (sceRegMgrSystemParamGetInt(ID_8A2AD7, &value) == 0 && value == 1)
		flags |= 4;
	return flags;
}
