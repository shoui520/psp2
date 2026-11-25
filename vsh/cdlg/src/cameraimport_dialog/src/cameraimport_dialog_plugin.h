/*
 * SceCamImportDialogPlugin: CameraImportDialog and CameraImportView declarations.
 */
#ifndef CAMERAIMPORT_DIALOG_PLUGIN_H
#define CAMERAIMPORT_DIALOG_PLUGIN_H

#include <stdint.h>
#include <scetypes.h>
#include <common_dialog.h>
#include <cameraimport_dialog.h>
#include <plugin.h>
#include <std/memory>
#include <common/job_slots.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include "../../photoreview_dialog/src/dialog_page.h"
#include "../../photoreview_dialog/src/event_dispatcher.h"
#include "../../photoreview_dialog/src/gui_dialog.h"
#include "../../photoreview_dialog/src/input_listener.h"
#include "../../photoreview_dialog/src/photoreview_dialog_plugin.h"
#include "camera_core.h"
#include "camera_view_widget.h"
#include "motion_listener.h"

/* Camera import dialog error codes */
#define CAMERAIMPORT_PLUGIN_ERROR_NO_WORK_MEMORY	((int)0x80104722)
#define CAMERAIMPORT_PLUGIN_ERROR_API_NAME		((int)0x80104721)
#define CAMERAIMPORT_PLUGIN_ERROR_GENERIC		((int)0x800201FF)
#define CAMERAIMPORT_PLUGIN_ERROR_DEVICE_UNAVAILABLE	((int)0x80104704)
#define CAMERAIMPORT_PLUGIN_ERROR_CAMERA_IN_USE		((int)0x80104701)

/* CameraImportView events dispatched to CameraImportDialog */
enum {
	CAMERAIMPORT_EVENT_READY = 0,
	CAMERAIMPORT_EVENT_SHOWN = 1,
	CAMERAIMPORT_EVENT_CLOSED = 2,
	CAMERAIMPORT_EVENT_CAPTURE_FAILED = 3,
	CAMERAIMPORT_EVENT_ENCODE_DONE = 4,
	CAMERAIMPORT_EVENT_RESOLUTION_SWITCHED = 5,
	CAMERAIMPORT_EVENT_CAMERA_SWITCHED = 6,
	CAMERAIMPORT_EVENT_MOTION_ERROR = 7,
	CAMERAIMPORT_EVENT_ACCEPT = 8,
	CAMERAIMPORT_EVENT_CANCEL = 9
};

/* Callback invoked when CameraImportDialog completes */
typedef void (*CameraImportFinishCallback)(const SceCameraImportDialogResult *result,
					   void *userData);

/* Encode result passed with CAMERAIMPORT_EVENT_ENCODE_DONE */
struct CameraImportEncodeResult {
	uint64_t photoId;
	int result;
};

/* CameraImportView: main viewfinder page, overlay, shutter, and confirm UI */
class CameraImportView : public EventDispatcher, public EventListener, public DialogPage,
			 public InputHandler, public MotionHandler {
public:
	CameraImportView(paf::Plugin *plugin);
	virtual ~CameraImportView();

	virtual int OnCreate();
	virtual int OnDestroy();
	virtual int Open();
	virtual int Close();
	virtual void OnInit(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnDecide(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnButton(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnEvent(EventDispatcher *sender, int32_t event, void *arg);
	virtual void OnPadInput(const paf::inputdevice::pad::Data *data,
				const paf::inputdevice::pad::Data *pre);
	virtual void OnOrientationChanged(int orientation);
	virtual void OnMotionSensorError(int error);

	virtual void OnConfirmFadeOutEnd(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnViewfinderTap(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnShutterButtonStateChanged(int32_t type, paf::ui::Handler *self,
						 paf::ui::Event *e);
	virtual void OnCameraViewStatus(int status);

	void SetDialogParam(const SceCameraImportDialogParam *param);
	void ShowPage();
	void HidePage();
	void EnterConfirmMode();
	void SetDialogBlocked();
	const SceCameraImportDialogResult *GetCaptureResult() const { return &m_captureResult; }

private:
	CameraViewWidget *FindCameraViewWidget();
	void UpdateUiVisibility();
	int UpdateOverlayGeometry();
	void ApplyCameraResolution(int resolutionPreset);
	void StartPreview();
	void AttachViewfinderTapHandler(paf::ui::Widget *widget);
	void PlayShutterSound();
	int TakePicture();
	int FillCaptureResult(const CameraFrameInfo *frameInfo, const CameraLocationInfo *location);
	void FillLocationInfo(int deviceId, CameraLocationInfo *location);
	void RotateControls(int orientation);
	void ParseAvailableOptions(const SceCameraImportDialogParam *param);
	void ParseInitialOptions(const SceCameraImportDialogParam *param);
	paf::intrusive_ptr<paf::graph::Surface> CreateOverlaySurface(
		const SceCameraImportDialogOverlayImage *overlay);
	void HideConfirmButtons();
	void CloseConfirmOverlay();
	void OpenConfirmOverlay();
	void OnJobSlotRemoved(uint32_t index, paf::common::SharedPtr<paf::job::JobItem> item);
	void OnTick();

	static void TickCB(void *userData);
	static void CameraViewStatusCB(void *userData, int status, int arg);
	static void JobSlotRemovedCB(uint32_t index, paf::common::SharedPtr<paf::job::JobItem> item,
				     void *userData);
	static void ConfirmFadeOutEndCB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
					void *userData);
	static void ViewfinderTapCB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				    void *userData);
	static void ShutterButtonCallback(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
					  void *userData);
	static void ShutterFlashEndCB(paf::Timer *t, paf::ui::AnimListener *listener, void *argA,
				      void *argB, void *argC);
	static void PreviewFadeEndCB(paf::Timer *t, paf::ui::AnimListener *listener, void *argA,
				     void *argB, void *argC);

	uint32_t m_uiFlags;
	uint32_t m_powerTickFrame;
	uint32_t m_powerTickInterval;
	uint32_t m_keepAliveFrame;
	uint32_t m_keepAliveInterval;
	int m_currentResolution;
	int m_currentDevice;
	bool m_allowedResolutions[8];
	bool m_allowedDevices[3];
	uint32_t m_allowedResolutionCount;
	uint32_t m_allowedDeviceCount;
	CameraImportEncodeResult m_encodeResult;
	GuiWaitDialog m_waitDialog;
	paf::common::JobSlots m_jobSlots;
	paf::intrusive_ptr<paf::graph::Surface> m_overlaySurface;
	const SceCameraImportDialogParam *m_dialogParam;
	SceCameraImportDialogResult m_captureResult;
	bool m_rotationEnabled;
};

/* CameraImportDialog: top-level plugin app coordinating view, error dialog, and camera core */
class CameraImportDialog : public EventListener, public PluginApp, public CameraCoreListener {
public:
	CameraImportDialog(paf::Plugin *plugin);
	virtual ~CameraImportDialog();

	virtual int Init(paf::Plugin *plugin);
	virtual int Start(paf::Plugin *plugin);
	virtual int Stop(paf::Plugin *plugin);
	virtual int Exit(paf::Plugin *plugin);
	virtual void OnEvent(EventDispatcher *sender, int32_t event, void *arg);
	virtual void OnCameraEvent(int event, int arg, int result);

	int SetParam(const SceCameraImportDialogParam *param, CameraImportFinishCallback cb,
		     void *userData);

private:
	int InitSurfacePool();
	void OpenErrorDialog(int errorCode);
	void CompleteDialog();
	void HandleViewEvent(int32_t event, void *arg);

	CameraImportView m_view;
	SceCameraImportDialogParam m_param;
	SceCameraImportDialogMemoryRawParam m_memRawParam;
	SceCameraImportDialogMemoryJpegParam m_memJpegParam;
	SceCameraImportDialogOverlayImage m_overlayImage;
	SceCommonDialogColor m_bgColor;
	SceCameraImportDialogResult m_result;
	CameraImportFinishCallback m_finishCb;
	void *m_finishCbArg;
	GuiMessageDialog m_errorDialog;
	int m_initError;
	int m_pendingError;
	bool m_stopping;
	paf::graph::SurfacePool *m_surfacePool;
	SurfaceInstancePool m_surfaceInstancePool;
};

#endif /* CAMERAIMPORT_DIALOG_PLUGIN_H */
