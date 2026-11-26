/*
 * UpdateCuiView (sys/internal/usermode/src/psp2swu/update_cui_view.h):
 * text-mode system updater view rendered to CDRAM framebuffers via libdbgfont
 * when psp2swu runs in non-GUI mode.
 */
#ifndef PSP2SWU_UPDATE_CUI_VIEW_H
#define PSP2SWU_UPDATE_CUI_VIEW_H

#include <scetypes.h>
#include "update_view_base.h"

class UpdateCuiView : public UpdateViewBase {
public:
	UpdateCuiView();
	virtual ~UpdateCuiView();

	virtual SceInt32 Initialize();
	virtual SceInt32 Operate();
	virtual SceInt32 Finalize();
	virtual SceInt32 OnVerifyFinished();
	virtual SceInt32 OnUpdateFinished();
	virtual void OnReleaseCheckError(SceInt32 errorCode);

private:
	static void ProgressTimerCallback(int phase, int step, SceUpdaterProgress progress);
	static SceInt32 InitDisplay();
	static SceInt32 ShutdownDisplay();
	static SceInt32 DrawProgressScreen();
	static SceInt32 DrawErrorScreen();
	static SceInt32 OnPhaseStarted(SceUInt32 phase, SceInt32 step, const char *message);
	static SceInt32 OnPhaseProgress(SceUInt32 phase, SceInt32 step, SceInt32 progress);
	static SceInt32 OnPhaseError(SceInt32 errorCode, const char *message);
};

#endif
