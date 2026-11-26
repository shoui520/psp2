/*
 * UpdateViewBase (sys/internal/usermode/src/psp2swu/update_view_base.cpp).
 */
#include <scetypes.h>
#include "../cui_setupper/cui_view.h"
#include "psp2swu_app.h"
#include "update_view_base.h"

UpdateViewBase::UpdateViewBase()
{
}

UpdateViewBase::~UpdateViewBase()
{
}

SceInt32 UpdateViewBase::ExecuteUpdate()
{
	const char *pupPath = SwuApplication::GetInstance()->GetPupPath();
	SceInt32 ret = SwuApplication::GetInstance()->VerifyData(pupPath);

	if (ret != 0) {
		SwuApplication::GetInstance()->RecordFirstError(ret);
		SwuApplication::GetInstance()->UpdateModeFlag();
		return ret;
	}

	ret = OnVerifyFinished();
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "verify notification failed.", 27);
		SwuApplication::GetInstance()->RecordFirstError(ret);
		SwuApplication::GetInstance()->UpdateModeFlag();
		return ret;
	}

	ret = SwuApplication::GetInstance()->UpdateFirmware(pupPath);
	if (ret != 0) {
		SwuApplication::GetInstance()->RecordFirstError(ret);
		SwuApplication::GetInstance()->UpdateModeFlag();
		return ret;
	}

	ret = SwuApplication::GetInstance()->UpdateModeFlag();
	if (ret == 0) {
		ret = OnUpdateFinished();
		if (ret != 0) {
			SwuApplication::GetInstance()->RecordFirstError(ret);
			CuiInformUpdateFinished(ret, "update notification failed.", 27);
			return ret;
		}
	}
	return ret;
}
