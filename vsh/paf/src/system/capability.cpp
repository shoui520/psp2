/*
 * paf::system::Capability: what the console has. Only system software may
 * ask; a game or an application is stopped.
 */
#include <system/system.h>
#include <common/common_internal.h>

using namespace paf;
using namespace paf::system;

static Capability s_capability;

static inline void CheckSystemProgram()
{
	if (sceAppMgrIsGameProgram() != 0 || sceAppMgrIsNonGameProgram() != 0)
		common_internal::on_error();
}

bool Capability::HasWirelessController()
{
	CheckSystemProgram();
	return (s_capability.flags & Flag_WirelessController) != 0;
}

bool Capability::HasCamera()
{
	CheckSystemProgram();
	return (s_capability.flags & Flag_Camera) != 0;
}

bool ScePafMisc_B7CFFF5C(void)
{
	CheckSystemProgram();
	return (s_capability.flags & Capability::Flag_B7CFFF5C) != 0;
}

bool Capability::HasOLED()
{
	CheckSystemProgram();
	return (s_capability.flags & Flag_OLED) != 0;
}

bool Capability::HasLCD()
{
	CheckSystemProgram();
	return (s_capability.flags & Flag_LCD) != 0;
}

bool Capability::HasHID()
{
	CheckSystemProgram();
	return (s_capability.flags & Flag_HID) != 0;
}

bool ScePafMisc_7ED8F03C(void)
{
	CheckSystemProgram();
	return (s_capability.flags & Capability::Flag_7ED8F03C) != 0;
}

bool Capability::HasFrontTouch()
{
	CheckSystemProgram();
	return Model::GetType() == Model::Type_Vita;
}

bool Capability::HasBackTouch()
{
	CheckSystemProgram();
	return Model::GetType() == Model::Type_Vita;
}

bool ScePafMisc_183E674B(void)
{
	CheckSystemProgram();
	return Model::GetType() == Model::Type_Vita;
}

bool Capability::HasHDMI()
{
	CheckSystemProgram();
	return Model::GetType() == Model::Type_Dolce;
}
