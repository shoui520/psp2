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
	SwuApplication *app = SwuApplication::GetInstance();
	const char *pupPath = app->GetPupPath();
	SceInt32 ret = app->VerifyData(pupPath);

	if (ret != 0) {
		app->RecordFirstError(ret);
		app->UpdateModeFlag();
		return ret;
	}

	ret = OnVerifyFinished();
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "verify notification failed.", 27);
		app->RecordFirstError(ret);
		app->UpdateModeFlag();
		return ret;
	}

	ret = app->UpdateFirmware(pupPath);
	if (ret != 0) {
		app->RecordFirstError(ret);
		app->UpdateModeFlag();
		return ret;
	}

	ret = app->UpdateModeFlag();
	if (ret == 0) {
		ret = OnUpdateFinished();
		if (ret != 0) {
			app->RecordFirstError(ret);
			CuiInformUpdateFinished(ret, "update notification failed.", 27);
			return ret;
		}
	}
	return ret;
}
