/*
 * SceCamImportDialogPlugin: CameraViewWidget ("camera_view") and CameraImportJobQueue.
 */
#ifndef CAMERAIMPORT_DIALOG_CAMERA_VIEW_WIDGET_H
#define CAMERAIMPORT_DIALOG_CAMERA_VIEW_WIDGET_H

#include <stdint.h>
#include <scetypes.h>
#include <std/memory>
#include <job/job.h>
#include <graphics/surface/surface.h>
#include <widget/w_plane.h>
#include "camera_core.h"

/* CameraViewWidget status callback */
typedef void (*CameraViewStatusCallback)(void *userData, int status, int arg);

/* Global 4-queue job manager ("SceCameraImportDialogJobQueue%u") */
void CameraImportJobQueueInit(void);
void CameraImportJobQueueTerm(void);
paf::job::JobQueue *CameraImportJobQueueGet(int index);
paf::job::JobQueue *CameraImportJobQueueGetEncode(void);

/* Custom PAF Plane widget displaying live camera frames ("camera_view") */
class CameraViewWidget : public paf::ui::Plane {
public:
	CameraViewWidget(paf::ui::Widget *parent, paf::ui::Widget::CreateParam *param);
	virtual ~CameraViewWidget();
	virtual bool IsInherit(const char *name) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual void Update();

	static const char *WidgetType();

	int SetCameraDevice(int deviceId);
	int SetStatusCallback(CameraViewStatusCallback cb, void *userData);
	int GetCameraDevice() const;
	int FreezeFrame();
	int UnfreezeFrame();
	int GetCapturedFrame(paf::intrusive_ptr<paf::graph::Surface> *outSurface,
			     CameraFrameInfo *outInfo);

private:
	void UpdateTextureRegion(const paf::intrusive_ptr<paf::graph::Surface> &surface,
				 uint32_t cropY, uint32_t cropX, uint32_t cropW, uint32_t cropH);

	DeviceController *m_controller;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
	CameraFrameInfo m_frameInfo;
	bool m_frozen;
	CameraViewStatusCallback m_statusCb;
	void *m_statusCbArg;
	int m_lastStatus;
};

#endif /* CAMERAIMPORT_DIALOG_CAMERA_VIEW_WIDGET_H */
