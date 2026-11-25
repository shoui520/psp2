/*
 * SceSaveDataDialogPlugin: SaveDataDialog main UI controller.
 */
#ifndef SAVEDATA_DIALOG_H
#define SAVEDATA_DIALOG_H

#include <scetypes.h>
#include <std/string>
#include <std/memory>
#include <plugin.h>
#include <widget/core/widget.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "savedata_dialog_plugin.h"
#include "savedata_job.h"
#include "savedata_list_view.h"

class SaveDataDialog {
public:
	SaveDataDialog(const SceSaveDataDialogParam *param,
		SaveDataDialogResultCallback callback, void *userData);
	SaveDataDialog(const ScePspSaveDataDialogParam *param,
		SaveDataDialogResultCallback callback, void *userData);
	~SaveDataDialog();

	SceInt32 UpdateVitaParam(const SceSaveDataDialogParam *param,
		SaveDataDialogResultCallback callback, void *userData);
	SceInt32 UpdatePspParam(const ScePspSaveDataDialogParam *param,
		SaveDataDialogResultCallback callback, void *userData);

	SceInt32 ShowUserMessage(const paf::wstring &message, SceUInt32 buttonType);
	SceInt32 ShowSystemMessage(SceSaveDataDialogSystemMessageType sysMsgType,
		SceUInt32 value);
	SceInt32 ShowErrorCode(SceInt32 errorCode);
	SceInt32 ShowProgressBar(const paf::wstring &message, SceUInt32 barType);
	void IncrementProgressBar(SceSaveDataDialogProgressBarTarget target, SceUInt32 delta);
	void SetProgressBarValue(SceSaveDataDialogProgressBarTarget target, SceUInt32 rate);
	void OnSlotUpdated(SceAppUtilSaveDataSlotId slotId);
	void BeginClose(bool isAborted);

	bool IsReadyForMode(SceSaveDataDialogMode mode) const;
	SceInt32 FormatSystemMessage(SceSaveDataDialogSystemMessageType sysMsgType,
		SceUInt32 value, paf::wstring *outText, SceInt32 *outSoundId);
	SceInt32 FormatProgressMessage(SceSaveDataDialogSystemMessageType sysMsgType,
		const SceChar8 *userMsg, paf::wstring *outText);

	void HandleGuiDialogButton(SceInt32 dialogValue, SceInt32 button);
	void HandleListSelection(SceSize selectedIndex);

private:
	void InitCommonState(SaveDataDialogResultCallback callback, void *userData);
	void OpenDialogPage(SceSaveDataDialogType dispType);
	void SetupHeaderTemplate(void);
	void UpdateViewState(SceInt32 viewState);
	void CloseGuiDialog(void);
	void SendSystemMessageNotification(SceSaveDataDialogSystemMessageType sysMsgType);
	void SendErrorCodeNotification(SceInt32 errorCode);
	void NotifyResult(SceInt32 result, SceInt32 buttonAction);

	static void OnVitaInfoLoaded(SceInt32 result,
		const SaveDataDialogSlotResultInfo &slotInfo, void *userData);
	static void OnVitaListLoaded(SceInt32 result,
		paf::common::SharedPtr<SaveDataSlotList> list, void *userData);
	static void OnPspInfoLoaded(SceInt32 result,
		const SaveDataDialogSlotResultInfo &slotInfo, void *userData);
	static void OnPspListLoaded(SceInt32 result,
		paf::common::SharedPtr<SaveDataSlotList> list, void *userData);
	static void OnListSelectStatic(SceSize selectedIndex, void *userData);

	SaveDataDialogResultCallback m_callback;
	void *m_callbackUserData;
	SceBool m_isPsp;
	SceSaveDataDialogMode m_mode;
	SceSaveDataDialogType m_dispType;
	SceAppUtilMountPoint m_mountPoint;
	char m_pspMountRoot[16];
	paf::ui::Widget *m_pageWidget;
	paf::ui::Widget *m_rootWidget;
	SaveDataListView *m_listView;
	paf::common::SharedPtr<SaveDataSlotList> m_slotList;
	SaveDataDialogSlotResultInfo m_selectedSlot;
	SceInt32 m_guiDialogId;
	SceUInt32 m_progressRate;
	bool m_headerInitialized;
	bool m_isClosing;
};

SceInt32 OpenVitaSaveDataDialog(const SceSaveDataDialogParam *param,
	SaveDataDialogResultCallback callback, void *userData);
SceInt32 OpenPspSaveDataDialog(const ScePspSaveDataDialogParam *param,
	SaveDataDialogResultCallback callback, void *userData);
SceInt32 CloseSaveDataDialog(bool isAborted);
SceInt32 DestroySaveDataDialog(void);
SaveDataDialog *GetSaveDataDialogInstance(void);

#endif /* SAVEDATA_DIALOG_H */
