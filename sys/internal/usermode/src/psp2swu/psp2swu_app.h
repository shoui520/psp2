/*
 * SwuApplication (sys/internal/usermode/src/psp2swu/psp2swu_app.h):
 * main controller for pup:psp2swu.self. Manages updater_core initialization,
 * view selection (GUI vs CUI), PUP verification and flashing, update mode flag
 * transitions, and SwInfo reporting.
 */
#ifndef PSP2SWU_PSP2SWU_APP_H
#define PSP2SWU_PSP2SWU_APP_H

#include <scetypes.h>

class UpdateViewBase;

struct SwInfoIndexMapping {
	const char *indexKey;
	const char *swInfoKey;
	SceInt32 valueKind;
};

class SwuApplication {
public:
	SwuApplication();
	virtual ~SwuApplication();

	void RecordFirstError(SceInt32 errorCode);
	SceInt32 GetRetryCount(SceInt32 *outRetryCount);
	SceInt32 Initialize();
	SceInt32 Finalize();
	SceInt32 WriteVersionSwInfo();
	SceInt32 WriteIndexSwInfoEntry(const SwInfoIndexMapping *mapping, const char *value);
	SceInt32 WriteIndexSwInfo();
	const char *GetPupPath();
	SceInt32 VerifyData(const char *pupPath);
	SceInt32 IsDiffUpdate();
	SceInt32 UpdateFirmware(const char *pupPath);
	SceInt32 GetProgress(SceInt32 *outRate, SceUInt64 *outRemainingMs);
	SceInt32 GetEstimateUpdateTime(const char *pupPath, SceUInt64 *outUpdateMs);
	SceInt32 HasUpdateFlagError();
	SceInt32 UpdateModeFlag();
	bool IsFactoryMode();

	static SwuApplication *GetInstance();
	static UpdateViewBase *GetView();

	static void *UpdaterAlloc(SceSize size);
	static void UpdaterFree(void *ptr);
	static void *UpdaterMemalign(SceSize align, SceSize size);

private:
	static SwuApplication *s_instance;
	static UpdateViewBase *s_view;

	SceInt32 m_firstError;
	SceInt32 m_updateFlagError;
	SceInt32 m_retryCount;
	SceInt32 m_clearUpdateMode;
};

#endif
