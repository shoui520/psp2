/*
 * UpdateViewBase (sys/internal/usermode/src/psp2swu/update_view_base.h):
 * abstract interface shared by the CUI (dbgfont) and GUI (PAF) system updater
 * views, plus the common ExecuteUpdate sequence that verifies, updates, and
 * finalizes the update mode flag.
 */
#ifndef PSP2SWU_UPDATE_VIEW_BASE_H
#define PSP2SWU_UPDATE_VIEW_BASE_H

#include <scetypes.h>

class UpdateViewBase {
public:
	UpdateViewBase();
	virtual ~UpdateViewBase();

	virtual SceInt32 Initialize() = 0;
	virtual SceInt32 Operate() = 0;
	virtual SceInt32 Finalize() = 0;
	virtual SceInt32 OnVerifyFinished() = 0;
	virtual SceInt32 OnUpdateFinished() = 0;
	virtual void OnReleaseCheckError(SceInt32 errorCode) = 0;

	SceInt32 ExecuteUpdate();
};

#endif
