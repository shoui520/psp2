/*
 * UpdateGuiView (sys/internal/usermode/src/psp2swu/update_gui_view.h):
 * PAF-based GUI system updater view hosting common_resource.rco and
 * updater_plugin.rco.
 */
#ifndef PSP2SWU_UPDATE_GUI_VIEW_H
#define PSP2SWU_UPDATE_GUI_VIEW_H

#include <scetypes.h>
#include <framework.h>
#include "update_view_base.h"

class UpdateGuiView : public UpdateViewBase {
public:
	UpdateGuiView();
	virtual ~UpdateGuiView();

	virtual SceInt32 Initialize();
	virtual SceInt32 Operate();
	virtual SceInt32 Finalize();
	virtual SceInt32 OnVerifyFinished();
	virtual SceInt32 OnUpdateFinished();
	virtual void OnReleaseCheckError(SceInt32 errorCode);

	SceInt32 RunUpdate();
	SceInt32 GetProgress(SceInt32 *outRate, SceUInt64 *outRemainingMs);
	SceInt32 GetEstimateUpdateTime(SceUInt64 *outUpdateMs);
	SceInt32 GetRetryCount(SceInt32 *outRetryCount);
	SceInt32 IsDiffUpdate();
	SceInt32 HasUpdateFlagError();
	bool IsFactoryMode();

	static UpdateGuiView *GetInstance();

private:
	static UpdateGuiView *s_instance;

	paf::Framework *m_framework;
	SceInt32 m_updateResult;
};

#endif
