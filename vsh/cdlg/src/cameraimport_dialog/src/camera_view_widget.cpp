/*
 * SceCamImportDialogPlugin: CameraViewWidget ("camera_view") and CameraImportJobQueue.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <job/job.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>
#include <thread/thread.h>
#include <widget/w_plane.h>
#include "camera_view_widget.h"

using namespace paf;
using namespace paf::graph;
using namespace paf::ui;

/* 4-slot job queue pool ("SceCameraImportDialogJobQueue%u") */
struct CameraImportJobQueuePool {
	CameraImportJobQueuePool();
	void WaitAllIdle();

	job::JobQueue *queues[4];
	void *threads[4];
};

static CameraImportJobQueuePool *s_jobQueuePool = NULL;

CameraImportJobQueuePool::CameraImportJobQueuePool()
{
	job::JobQueue::Option opt;

	opt.priority = 0x10000100;
	for (uint32_t i = 0; i < 4; ++i) {
		char name[32];

		sce_paf_memset(name, 0, sizeof(name));
		sce_paf_snprintf(name, sizeof(name), "SceCameraImportDialogJobQueue%u", i);
		queues[i] = new job::JobQueue(name, &opt);
		threads[i] = NULL;
		int nextPrio = opt.priority + 1;

		if (nextPrio < 0x10000100)
			nextPrio = 0x10000100;
		if (nextPrio > 0x10000104)
			nextPrio = 0x10000104;
		opt.priority = nextPrio;
	}
}

void CameraImportJobQueuePool::WaitAllIdle()
{
	for (;;) {
		bool allEmpty = true;

		for (uint32_t i = 0; i < 4; ++i) {
			if (queues[i] != NULL && queues[i]->NumItems() != 0) {
				allEmpty = false;
				break;
			}
		}
		if (allEmpty)
			break;
		paf::thread::Sleep(10);
	}
}

void CameraImportJobQueueInit(void)
{
	if (s_jobQueuePool == NULL)
		s_jobQueuePool = new CameraImportJobQueuePool();
}

void CameraImportJobQueueTerm(void)
{
	if (s_jobQueuePool != NULL) {
		s_jobQueuePool->WaitAllIdle();
		for (uint32_t i = 0; i < 4; ++i) {
			if (s_jobQueuePool->queues[i] != NULL) {
				delete s_jobQueuePool->queues[i];
				s_jobQueuePool->queues[i] = NULL;
			}
		}
		delete s_jobQueuePool;
		s_jobQueuePool = NULL;
	}
}

job::JobQueue *CameraImportJobQueueGet(int index)
{
	if (s_jobQueuePool == NULL || index < 0 || index >= 4)
		return NULL;
	return s_jobQueuePool->queues[index];
}

job::JobQueue *CameraImportJobQueueGetEncode(void)
{
	return CameraImportJobQueueGet(3);
}

/* CameraViewWidget */
const char *CameraViewWidget::WidgetType()
{
	return "camera_view";
}

bool CameraViewWidget::IsInherit(const char *name) const
{
	if (name == WidgetType())
		return true;
	return Plane::IsInherit(name);
}

char *CameraViewWidget::GetTypeName() const
{
	return (char *)WidgetType();
}

CameraViewWidget::CameraViewWidget(Widget *parent, Widget::CreateParam *param)
	: Plane(parent, param), m_controller(NULL), m_surface(), m_frameInfo(), m_frozen(false),
	  m_statusCb(NULL), m_statusCbArg(NULL), m_lastStatus(0)
{
	m_controller = new DeviceController(NULL);
}

CameraViewWidget::~CameraViewWidget()
{
	m_surface = NULL;
}

void CameraViewWidget::DestroyWidget()
{
	if (m_controller != NULL) {
		m_controller->SetDevice(CAMERA_CORE_DEVICE_NONE);
		delete m_controller;
		m_controller = NULL;
	}
	m_surface = NULL;
	m_statusCb = NULL;
	m_statusCbArg = NULL;
	Widget::DestroyWidget();
}

int CameraViewWidget::SetCameraDevice(int deviceId)
{
	if (m_controller == NULL)
		return CAMERA_CORE_ERROR_GENERIC;
	return m_controller->SetDevice(deviceId);
}

int CameraViewWidget::SetStatusCallback(CameraViewStatusCallback cb, void *userData)
{
	m_statusCb = cb;
	m_statusCbArg = userData;
	return 0;
}

int CameraViewWidget::GetCameraDevice() const
{
	if (m_controller == NULL)
		return CAMERA_CORE_DEVICE_NONE;
	return m_controller->GetDeviceId();
}

int CameraViewWidget::FreezeFrame()
{
	m_frozen = true;
	return 0;
}

int CameraViewWidget::UnfreezeFrame()
{
	m_frozen = false;
	return 0;
}

int CameraViewWidget::GetCapturedFrame(intrusive_ptr<Surface> *outSurface, CameraFrameInfo *outInfo)
{
	if (outSurface != NULL)
		*outSurface = m_surface;
	if (outInfo != NULL)
		*outInfo = m_frameInfo;
	return 0;
}

void CameraViewWidget::UpdateTextureRegion(const intrusive_ptr<Surface> &surface, uint32_t cropY,
					   uint32_t cropX, uint32_t cropW, uint32_t cropH)
{
	if (surface.get() == NULL)
		return;
	double texW = (double)surface->GetWidth();
	double texH = (double)surface->GetHeight();
	DrawObj *obj = GetDrawObj(0);

	if (obj != NULL && obj->IsInherit(PlaneObj::DrawObjType())) {
		PlaneObj *plane = (PlaneObj *)obj;

		plane->SetScaleMode((PlaneObj::ScaleMode)6, (PlaneObj::ScaleMode)6);
		math::v4 region((float)((double)cropX / texW), (float)((double)cropY / texH),
				(float)((double)cropW / texW), (float)((double)cropH / texH));

		plane->SetTexRegion(region);
	}
}

void CameraViewWidget::Update()
{
	int status = 0;

	if (m_controller != NULL) {
		intrusive_ptr<Surface> frameSurf;
		CameraFrameInfo frameInfo;
		int ret = m_controller->GetCurrentFrame(&frameSurf, &frameInfo);

		if (ret != 0 || frameSurf.get() == NULL) {
			if (!m_frozen || ret != 0) {
				intrusive_ptr<Surface> emptySurf;

				status = 2;
				SetTexture(emptySurf, 0, 0);
				m_surface = NULL;
				m_frameInfo = CameraFrameInfo();
			}
		} else if (m_frameInfo.timestamp != frameInfo.timestamp && !m_frozen) {
			status = 1;
			SetTexture(frameSurf, 0, 0);
			if (m_frameInfo.cropY != frameInfo.cropY ||
			    m_frameInfo.cropX != frameInfo.cropX ||
			    m_frameInfo.cropWidth != frameInfo.cropWidth ||
			    m_frameInfo.cropHeight != frameInfo.cropHeight) {
				UpdateTextureRegion(frameSurf, (uint32_t)frameInfo.cropY,
						    (uint32_t)frameInfo.cropX,
						    (uint32_t)frameInfo.cropWidth,
						    (uint32_t)frameInfo.cropHeight);
			}
			m_surface = frameSurf;
			m_frameInfo = frameInfo;
		}
		if (status != 0 && m_lastStatus != status) {
			m_lastStatus = status;
			if (m_statusCb != NULL)
				m_statusCb(m_statusCbArg, status > 1 ? 1 : 0, 0);
		}
	}
	Plane::Update();
}
