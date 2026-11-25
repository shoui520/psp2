/*
 * SceCamImportDialogPlugin: CameraImportView (viewfinder, overlay, shutter, confirm UI)
 * and ShutterSoundJobItem ("SceCameraImportDialogShutterSoundJobItem").
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <cameraimport_dialog.h>
#include <kernel/modulemgr.h>
#include <std/string.h>
#include <std/string>
#include <std/new>
#include <plugin.h>
#include <timer/timer.h>
#include <math/ngp/v4.h>
#include <std/memory>
#include <job/job.h>
#include <common/job_slots.h>
#include <common/main_thread_call_list.h>
#include <common/shared_ptr.h>
#include <common/transition.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/pad/pad.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_button_base.h>
#include <widget/w_plane.h>
#include <widget/w_scene.h>
#include "cameraimport_dialog_plugin.h"
#include "camera_core.h"
#include "camera_view_widget.h"
#include "motion_listener.h"
#include "xui_shell_util.h"

namespace paf {
namespace gutil {
graph::SurfacePool *GetDefaultSurfacePool();
}
}

extern "C" {
int sceShutterSoundPlay(int mode, int option);
void ScePafWidget_730AA895(paf::ui::Widget *w, uint32_t flags, bool recursive);
int sceKernelGetSystemSwVersion(void *info);
}

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::job;
using namespace paf::ui;

/* Resource widget ID hashes (from cameraimport_dialog_plugin.rco) */
#define WIDGET_ID_CAMERA_PAGE		0xDDC04196U
#define WIDGET_ID_VIEWFINDER_PLANE	0xFDE92FD8U
#define WIDGET_ID_FLASH_PLANE		0xC7519CE6U
#define WIDGET_ID_CONTROLS_PLANE	0x683D5B1CU
#define WIDGET_ID_CONTROLS_TEMPLATE	0x78FA2B9DU
#define WIDGET_ID_CANCEL_BTN		0xF0A5BF8AU
#define WIDGET_ID_CAMERA_SWITCH_BTN	0xCF5B7B74U
#define WIDGET_ID_RES_SWITCH_BTN	0x0E603A36U
#define WIDGET_ID_SHUTTER_BTN		0xFF106454U
#define WIDGET_ID_CONFIRM_BAR		0xBB89B6B6U
#define WIDGET_ID_CONFIRM_TEMPLATE	0x39AE2D2DU
#define WIDGET_ID_CONFIRM_OK_BTN	0xF78AE0C1U
#define WIDGET_ID_CONFIRM_RETAKE_BTN	0x757D2C51U

/* Shutter button icon texture ID hashes */
#define TEXTURE_ID_SHUTTER_NORMAL	0x92C039B8U
#define TEXTURE_ID_SHUTTER_PRESSED	0x8FD6EB86U

/* UI state bitmask (m_uiFlags) */
#define VIEW_FLAG_FLASH_DONE		0x00000001U
#define VIEW_FLAG_ENCODE_DONE		0x00000002U
#define VIEW_FLAG_CAPTURING		0x00000004U
#define VIEW_FLAG_CAPTURE_LOCKED	0x00000008U
#define VIEW_FLAG_PREVIEW_SWITCHING	0x00000010U
#define VIEW_FLAG_CONTROLS_HIDDEN	0x00000020U
#define VIEW_FLAG_PAGE_HIDDEN		0x00000040U
#define VIEW_FLAG_DIALOG_BLOCKED	0x00000080U
#define VIEW_FLAG_CONFIRM_MODE		0x01000000U

/* Job slot indices */
#define JOB_SLOT_SHUTTER_SOUND		0U
#define JOB_SLOT_ENCODE			2U
#define JOB_SLOT_COUNT			5U

/* Event IDs */
#define VIEW_EVENT_TOUCH_TAP		0x00020004
#define VIEW_EVENT_TRANSITION_OUT_END	0x00100002
#define VIEW_PAD_CROSS_MASK		0x00000800U
#define VIEW_PAD_IGNORE_MASK		0xFFFFDFFFU

static const IDParam s_shutterNormalTexId("", TEXTURE_ID_SHUTTER_NORMAL);
static const IDParam s_shutterPressedTexId("", TEXTURE_ID_SHUTTER_PRESSED);

static char s_systemSwVersion[32];
static bool s_systemSwVersionLoaded = false;

static const char *GetSystemSoftwareVersionString(void)
{
	if (!s_systemSwVersionLoaded) {
		struct SwVersionParam {
			SceSize size;
			char versionString[28];
			uint32_t versionInt;
			uint32_t flags;
		} fwInfo;

		sce_paf_memset(&fwInfo, 0, sizeof(fwInfo));
		fwInfo.size = sizeof(fwInfo);
		if (sceKernelGetSystemSwVersion(&fwInfo) != 0) {
			s_systemSwVersion[0] = '\0';
		} else if (sce_paf_strlen(fwInfo.versionString) != 0) {
			sce_paf_strncpy(s_systemSwVersion, fwInfo.versionString, 28);
			s_systemSwVersion[27] = '\0';
		}
		s_systemSwVersionLoaded = true;
	}
	return s_systemSwVersion;
}

static inline void ShowWidgetWithTimer(Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xFF;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

static inline void HideWidgetWithTimer(Widget *widget, Timer *timer)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xFF;
	widget->DoTransitionReverse_core(timer != NULL ? timer->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* Async shutter sound playback job ("SceCameraImportDialogShutterSoundJobItem") */
class ShutterSoundJobItem : public SlotJobItem {
public:
	ShutterSoundJobItem() : SlotJobItem("SceCameraImportDialogShutterSoundJobItem", NULL) {}
	virtual ~ShutterSoundJobItem() {}

	virtual int Run()
	{
		return sceShutterSoundPlay(0, 0);
	}

	virtual void Finish(int result)
	{
		(void)result;
	}
};

/* CameraImportView */
CameraImportView::CameraImportView(Plugin *plugin)
	: DialogPage(plugin, IDParam("", WIDGET_ID_CAMERA_PAGE), Plugin::PageOpenParam()),
	  m_uiFlags(0), m_powerTickFrame(0), m_powerTickInterval(29), m_keepAliveFrame(0),
	  m_keepAliveInterval(299), m_currentResolution(0), m_currentDevice(0),
	  m_allowedResolutionCount(0), m_allowedDeviceCount(0), m_waitDialog(), m_jobSlots(),
	  m_overlaySurface(), m_dialogParam(NULL), m_rotationEnabled(false)
{
	for (int i = 0; i < 8; ++i)
		m_allowedResolutions[i] = false;
	for (int i = 0; i < 3; ++i)
		m_allowedDevices[i] = false;
	m_encodeResult.photoId = (uint64_t)-1;
	m_encodeResult.result = 0;
	m_jobSlots.Grow(JOB_SLOT_COUNT);
	m_jobSlots.SetRemovedFunction(JobSlotRemovedCB, this);
	m_uiFlags |= VIEW_FLAG_PAGE_HIDDEN;
	sce_paf_memset(&m_captureResult, 0, sizeof(m_captureResult));
}

CameraImportView::~CameraImportView()
{
	m_overlaySurface = NULL;
}

int CameraImportView::OnCreate()
{
	AddMotionHandler(this, 0);
	AddInputHandler(this);
	return DialogPage::OnCreate();
}

int CameraImportView::OnDestroy()
{
	RemoveMotionHandler(this);
	RemoveInputHandler(this);
	m_jobSlots.CancelAll();
	JobQueue *shutterQueue = CameraImportJobQueueGet(1);

	if (shutterQueue != NULL)
		shutterQueue->WaitEmpty();
	JobQueue *encodeQueue = CameraImportJobQueueGet(3);

	if (encodeQueue != NULL)
		encodeQueue->WaitEmpty();
	return DialogPage::OnDestroy();
}

void CameraImportView::OnTick()
{
	if (!IsOpen())
		return;
	++m_powerTickFrame;
	if (m_powerTickFrame >= m_powerTickInterval)
		m_powerTickFrame = 0;
	++m_keepAliveFrame;
	if (m_keepAliveFrame >= m_keepAliveInterval) {
		m_keepAliveFrame = 0;
		if ((m_uiFlags & (VIEW_FLAG_CONFIRM_MODE | VIEW_FLAG_DIALOG_BLOCKED |
				  VIEW_FLAG_PAGE_HIDDEN)) == 0)
			XuiPowerTick(0);
	}
}

void CameraImportView::TickCB(void *userData)
{
	if (userData != NULL)
		((CameraImportView *)userData)->OnTick();
}

int CameraImportView::Open()
{
	if (!IsOpen())
		return DialogPage::Open();
	if (IsOpen()) {
		m_powerTickFrame = m_powerTickInterval;
		OnTick();
	}
	Notify(CAMERAIMPORT_EVENT_READY, NULL);
	return 0;
}

CameraViewWidget *CameraImportView::FindCameraViewWidget()
{
	if (!IsOpen())
		return NULL;
	Widget *w = FindChild(IDParam("CameraViewWidget"), 0);

	if (w != NULL && w->IsInherit(CameraViewWidget::WidgetType()))
		return (CameraViewWidget *)w;
	return NULL;
}

int CameraImportView::Close()
{
	m_uiFlags |= VIEW_FLAG_PAGE_HIDDEN;
	if (IsOpen()) {
		MainThreadCallList::Unregister(TickCB, this);
		CameraViewWidget *camView = FindCameraViewWidget();

		if (camView != NULL)
			camView->SetCameraDevice(CAMERA_CORE_DEVICE_NONE);
		DisableInputHandler(this);
		DisableMotionHandler(this);
		for (uint32_t i = 0; i < JOB_SLOT_COUNT; ++i)
			m_jobSlots.Cancel(i);
		if ((m_uiFlags & VIEW_FLAG_CAPTURING) != 0) {
			m_uiFlags |= (VIEW_FLAG_FLASH_DONE | VIEW_FLAG_ENCODE_DONE);
			if ((m_uiFlags & 7U) == 7U) {
				m_uiFlags &= ~7U;
				Notify(CAMERAIMPORT_EVENT_ENCODE_DONE, &m_encodeResult);
			}
		}
	}
	return DialogPage::Close();
}

void CameraImportView::UpdateUiVisibility()
{
	Widget *controlsPlane = FindChild(IDParam("", WIDGET_ID_CONTROLS_PLANE), 0);

	if (controlsPlane != NULL) {
		bool hideControls = (m_uiFlags & (VIEW_FLAG_CONFIRM_MODE |
						  VIEW_FLAG_DIALOG_BLOCKED |
						  VIEW_FLAG_CONTROLS_HIDDEN |
						  VIEW_FLAG_CAPTURING)) != 0;

		if (hideControls)
			HideWidgetWithTimer(controlsPlane, new paf::Timer(200.0f));
		else
			ShowWidgetWithTimer(controlsPlane, new paf::Timer(200.0f));
	}

	struct ButtonGate {
		uint32_t widgetId;
		uint32_t disableMask;
	};
	static const ButtonGate gates[3] = {
		{ WIDGET_ID_CAMERA_SWITCH_BTN, VIEW_FLAG_PAGE_HIDDEN },
		{ WIDGET_ID_RES_SWITCH_BTN, VIEW_FLAG_PAGE_HIDDEN },
		{ WIDGET_ID_SHUTTER_BTN,
		  VIEW_FLAG_PAGE_HIDDEN | VIEW_FLAG_PREVIEW_SWITCHING | VIEW_FLAG_CAPTURE_LOCKED }
	};

	for (int i = 0; i < 3; ++i) {
		Widget *w = FindChild(IDParam("", gates[i].widgetId), 0);

		if (w != NULL && w->IsInherit(ButtonBase::TypeName())) {
			ButtonBase *btn = (ButtonBase *)w;

			if (gates[i].disableMask != 0) {
				if ((m_uiFlags & gates[i].disableMask) != 0)
					btn->Disable(false);
				else
					btn->Enable(false);
			}
		}
	}
}

void CameraImportView::ShowPage()
{
	Scene *root = GetRoot();

	if (root != NULL) {
		ShowWidgetWithTimer(root, NULL);
		ScePafCommon_6CF3A3C8(0.0f, root, transition::Type_SlideFromBottom1, true, false);
	}
	m_uiFlags &= ~VIEW_FLAG_PAGE_HIDDEN;
	UpdateUiVisibility();
	Notify(CAMERAIMPORT_EVENT_SHOWN, NULL);
}

void CameraImportView::HidePage()
{
	m_uiFlags |= VIEW_FLAG_PAGE_HIDDEN;
	UpdateUiVisibility();
	Scene *root = GetRoot();

	if (root != NULL)
		ScePafCommon_B48C9BDC(0.0f, root, transition::Type_SlideFromBottom1, false, false);
	Notify(CAMERAIMPORT_EVENT_CLOSED, NULL);
}

void CameraImportView::ParseAvailableOptions(const SceCameraImportDialogParam *param)
{
	if (param == NULL)
		return;
	uint32_t resMask = param->availableResolution;
	uint32_t devMask = param->availableDevice;

	m_allowedResolutions[CAMERA_CORE_RES_640X480] = (resMask & 1U) != 0;
	m_allowedResolutions[CAMERA_CORE_RES_640X360] = (resMask & 2U) != 0;
	m_allowedResolutions[CAMERA_CORE_RES_480X480] = (resMask & 4U) != 0;
	m_allowedDevices[CAMERA_CORE_DEVICE_FRONT] = (devMask & 1U) != 0;
	m_allowedDevices[CAMERA_CORE_DEVICE_REAR] = (devMask & 2U) != 0;

	m_allowedResolutionCount = 0;
	for (int i = 0; i < 8; ++i) {
		if (m_allowedResolutions[i])
			++m_allowedResolutionCount;
	}
	m_allowedDeviceCount = 0;
	for (int i = 0; i < 3; ++i) {
		if (m_allowedDevices[i])
			++m_allowedDeviceCount;
	}
}

void CameraImportView::ParseInitialOptions(const SceCameraImportDialogParam *param)
{
	if (param == NULL)
		return;
	switch (param->initialResolution) {
	case 1:
		m_currentResolution = CAMERA_CORE_RES_640X480;
		break;
	case 2:
		m_currentResolution = CAMERA_CORE_RES_640X360;
		break;
	case 4:
		m_currentResolution = CAMERA_CORE_RES_480X480;
		break;
	default:
		break;
	}
	switch (param->initialDevice) {
	case 1:
		m_currentDevice = CAMERA_CORE_DEVICE_FRONT;
		break;
	case 2:
		m_currentDevice = CAMERA_CORE_DEVICE_REAR;
		break;
	default:
		break;
	}
}

intrusive_ptr<Surface>
CameraImportView::CreateOverlaySurface(const SceCameraImportDialogOverlayImage *overlay)
{
	intrusive_ptr<Surface> surface;

	if (overlay == NULL || overlay->buffer == NULL || overlay->bufferSize == 0)
		return surface;
	SurfacePool *pool = paf::gutil::GetDefaultSurfacePool();

	if (pool == NULL)
		return surface;
	surface = intrusive_ptr<Surface>(new Surface(pool, overlay->width, overlay->height,
						     (ImageMode)4, (ImageOrder)1, 1, 1, false));
	if (surface.get() == NULL || surface->GetMemory() == NULL)
		return surface;

	uint8_t *dst = (uint8_t *)surface->Lock(0);

	if (dst != NULL) {
		uint32_t dstPitch = (uint32_t)surface->GetStride();
		const uint8_t *src = (const uint8_t *)overlay->buffer;
		uint32_t height = overlay->height;

		if (overlay->texType == 0x60000000U || overlay->stride == dstPitch) {
			sce_paf_memcpy(dst, src, dstPitch * height);
		} else {
			uint32_t rowBytes = (overlay->stride < dstPitch) ? overlay->stride
									 : dstPitch;

			for (uint32_t y = 0; y < height; ++y) {
				sce_paf_memcpy(dst, src, rowBytes);
				dst += dstPitch;
				src += overlay->stride;
			}
		}
		surface->Unlock();
	} else {
		surface->Unlock();
		surface = NULL;
	}
	return surface;
}

void CameraImportView::SetDialogParam(const SceCameraImportDialogParam *param)
{
	if (param == NULL)
		return;
	m_dialogParam = param;
	if (param->overlayMode != 0 && param->overlayImage != NULL)
		m_overlaySurface = CreateOverlaySurface(param->overlayImage);
	ParseInitialOptions(m_dialogParam);
	ParseAvailableOptions(m_dialogParam);
	m_rotationEnabled = (m_dialogParam->rotationMode == 1);
}

void CameraImportView::RotateControls(int orientation)
{
	float targetAngle = 0.0f;

	switch (orientation) {
	case CAMERA_ORIENTATION_NORMAL:
		targetAngle = 0.0f;
		break;
	case CAMERA_ORIENTATION_180:
		targetAngle = 3.1415927f;
		break;
	case CAMERA_ORIENTATION_90:
		targetAngle = 1.5707964f;
		break;
	case CAMERA_ORIENTATION_270:
		targetAngle = 4.712389f;
		break;
	default:
		return;
	}

	Widget *shutterBtn = FindChild(IDParam("", WIDGET_ID_SHUTTER_BTN), 0);

	if (shutterBtn != NULL) {
		math::v4 curRot = shutterBtn->GetRot(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0);
		float delta = curRot.extract_z() - targetAngle;
		float absDelta = (delta < 0.0f) ? -delta : delta;
		float animAngle = targetAngle;

		if (absDelta > 3.1415927f)
			animAngle = (delta < 0.0f) ? (targetAngle - 6.2831855f)
						   : (targetAngle + 6.2831855f);
		shutterBtn->SetRot(math::v4(0.0f, 0.0f, animAngle, 0.0f), 2, new paf::Timer(200.0f),
				   Widget::ANIM_ROT, 2, 0.0f, NULL, NULL);
	}
}

int CameraImportView::UpdateOverlayGeometry()
{
	Widget *overlayWidget = FindChild(IDParam("CameraOverlayImageWidget"), 0);

	if (overlayWidget == NULL)
		return -1;
	float texW = 0.0f;
	float texH = 0.0f;

	if (m_overlaySurface.get() != NULL) {
		texW = (float)m_overlaySurface->GetWidth();
		texH = (float)m_overlaySurface->GetHeight();
	}

	int resPreset = 0;
	CameraViewWidget *camView = FindCameraViewWidget();

	if (camView != NULL) {
		int dev = camView->GetCameraDevice();

		if (dev == CAMERA_CORE_DEVICE_NONE)
			dev = CAMERA_CORE_DEVICE_REAR;
		CameraCoreGetConfig(dev, &resPreset, NULL, NULL);
	}

	float camW = 0.0f;
	float camH = 0.0f;

	switch (resPreset) {
	case CAMERA_CORE_RES_640X480:
		camW = 640.0f;
		camH = 480.0f;
		break;
	case CAMERA_CORE_RES_320X240:
		camW = 320.0f;
		camH = 240.0f;
		break;
	case CAMERA_CORE_RES_640X360:
		camW = 640.0f;
		camH = 360.0f;
		break;
	case CAMERA_CORE_RES_320X180:
		camW = 320.0f;
		camH = 180.0f;
		break;
	case CAMERA_CORE_RES_480X272:
		camW = 480.0f;
		camH = 272.0f;
		break;
	case CAMERA_CORE_RES_480X270:
		camW = 480.0f;
		camH = 270.0f;
		break;
	case CAMERA_CORE_RES_480X480:
		camW = 480.0f;
		camH = 480.0f;
		break;
	default:
		break;
	}

	math::v4 parentSize(0.0f, 0.0f, 0.0f, 0.0f);

	if (overlayWidget->m_parent != NULL)
		parentSize = *overlayWidget->m_parent->GetSize(0);
	float parentW = parentSize.extract_x();
	float parentH = parentSize.extract_y();

	if (texW <= 0.0f || texH <= 0.0f || camW <= 0.0f || camH <= 0.0f ||
	    parentW <= 0.0f || parentH <= 0.0f)
		return -1;

	DrawObj *drawObj = overlayWidget->GetDrawObj(0);

	if (drawObj == NULL || !drawObj->IsInherit(PlaneObj::DrawObjType()))
		return -1;
	PlaneObj *planeObj = (PlaneObj *)drawObj;
	float camAspect = camW / camH;
	float fittedW = (parentW / parentH < camAspect) ? parentW : (parentH * camAspect);
	float scale = fittedW / camW;
	float visW = parentW / scale;
	float visH = parentH / scale;

	if (visW > texW)
		visW = texW;
	if (visH > texH)
		visH = texH;
	math::v4 uvRegion((texW - visW) * 0.5f / texW, (texH - visH) * 0.5f / texH, visW / texW,
			  visH / texH);

	planeObj->SetTexRegion(uvRegion);
	planeObj->SetScaleMode(PlaneObj::SCALE_VALUE, PlaneObj::SCALE_VALUE);
	planeObj->SetScaleValue(math::v3(scale * visW, scale * visH, 0.0f));
	return 0;
}

void CameraImportView::ApplyCameraResolution(int resolutionPreset)
{
	if (resolutionPreset < CAMERA_CORE_RES_640X480 ||
	    resolutionPreset > CAMERA_CORE_RES_480X480)
		return;
	CameraViewWidget *camView = FindCameraViewWidget();

	if (camView == NULL)
		return;
	int fps = 0;
	int fmt = 0;
	int dev = camView->GetCameraDevice();

	if (dev == CAMERA_CORE_DEVICE_NONE)
		dev = CAMERA_CORE_DEVICE_REAR;
	if (CameraCoreGetConfig(dev, NULL, &fps, &fmt) >= 0) {
		CameraCoreSetConfig(CAMERA_CORE_DEVICE_FRONT, resolutionPreset, fps, fmt);
		CameraCoreSetConfig(CAMERA_CORE_DEVICE_REAR, resolutionPreset, fps, fmt);
	}
}

void CameraImportView::StartPreview()
{
	MainThreadCallList::Register(TickCB, this);
	ApplyCameraResolution(m_currentResolution);
	CameraViewWidget *camView = FindCameraViewWidget();

	if (camView != NULL)
		camView->SetCameraDevice(m_currentDevice);
	Widget *overlayWidget = FindChild(IDParam("CameraOverlayImageWidget"), 0);

	if (overlayWidget != NULL) {
		UpdateOverlayGeometry();
		overlayWidget->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), NULL,
					Widget::ANIM_COLOR, 2, 0.0f, NULL, NULL);
	}
	if (m_rotationEnabled) {
		EnableMotionHandler(this);
		if (GetMotionSensorError() == 1)
			Notify(CAMERAIMPORT_EVENT_MOTION_ERROR, NULL);
		RotateControls(GetMotionOrientation());
	} else {
		RotateControls(CAMERA_ORIENTATION_NORMAL);
	}
	EnableInputHandler(this);
	UpdateUiVisibility();
}

void CameraImportView::AttachViewfinderTapHandler(Widget *widget)
{
	if (widget == NULL)
		return;
	widget->m_event_enable |= 0xC0U;
	widget->SetGestureMask(widget->m_touch_gesture_mask | 1U, true);
	widget->AddEventCallback(VIEW_EVENT_TOUCH_TAP, ViewfinderTapCB, this);
}

void CameraImportView::OnInit(int32_t type, Handler *self, Event *e)
{
	(void)type;
	(void)self;
	(void)e;
	m_uiFlags |= VIEW_FLAG_PAGE_HIDDEN;

	Widget *vfWidget = FindChild(IDParam("", WIDGET_ID_VIEWFINDER_PLANE), 0);
	Plane *viewfinderPlane =
		(vfWidget != NULL && vfWidget->IsInherit(Plane::TypeName())) ? (Plane *)vfWidget
									     : NULL;
	CameraViewWidget *camView = NULL;

	if (viewfinderPlane != NULL) {
		camView = new CameraViewWidget(viewfinderPlane, NULL);
		if (camView != NULL) {
			camView->m_name = IDParam("CameraViewWidget");
			camView->SetSize(*viewfinderPlane->GetSize(0), NULL, Widget::ANIM_SIZE, 0,
					 0.0f, NULL, NULL);
			camView->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL,
					  Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
			camView->SetStatusCallback(CameraViewStatusCB, this);
		}
		if (m_dialogParam != NULL &&
		    (m_dialogParam->overlayMode == 1 || m_dialogParam->overlayMode == 3) &&
		    m_overlaySurface.get() != NULL) {
			Plane *overlayPlane = new Plane(viewfinderPlane, NULL);

			if (overlayPlane != NULL) {
				overlayPlane->m_name = IDParam("CameraOverlayImageWidget");
				overlayPlane->SetSize(*viewfinderPlane->GetSize(0), NULL,
						      Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
				overlayPlane->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f), NULL,
						       Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
				overlayPlane->SetTexture(m_overlaySurface, 0, 0);
				overlayPlane->SetParent(overlayPlane->m_parent, NULL, 6);
				overlayPlane->m_align_x = 2;
				overlayPlane->m_align_y = 2;
			}
		}
		if (m_dialogParam != NULL && m_dialogParam->commonParam.bgColor != NULL) {
			const SceCommonDialogColor *bg = m_dialogParam->commonParam.bgColor;
			math::v4 color((float)bg->r / 255.0f, (float)bg->g / 255.0f,
				       (float)bg->b / 255.0f, (float)bg->a / 255.0f);

			viewfinderPlane->SetColor(color, NULL, Widget::ANIM_COLOR, 0, 0.0f, NULL,
						  NULL);
		}
	}

	Widget *flashWidget = FindChild(IDParam("", WIDGET_ID_FLASH_PLANE), 0);
	Plane *flashPlane =
		(flashWidget != NULL && flashWidget->IsInherit(Plane::TypeName()))
			? (Plane *)flashWidget
			: NULL;

	if (flashPlane != NULL)
		flashPlane->SetParent(flashPlane->m_parent, NULL, 6);

	Widget *ctrlWidget = FindChild(IDParam("", WIDGET_ID_CONTROLS_PLANE), 0);
	Plane *controlsPlane =
		(ctrlWidget != NULL && ctrlWidget->IsInherit(Plane::TypeName()))
			? (Plane *)ctrlWidget
			: NULL;

	if (controlsPlane != NULL) {
		controlsPlane->SetParent(controlsPlane->m_parent, NULL, 6);
		if (m_plugin != NULL)
			m_plugin->TemplateOpen(controlsPlane,
					       IDParam("", WIDGET_ID_CONTROLS_TEMPLATE),
					       Plugin::TemplateOpenParam());
		ListenDecide(IDParam("", WIDGET_ID_CANCEL_BTN));
	}

	ListenDecide(IDParam("", WIDGET_ID_CAMERA_SWITCH_BTN));
	ListenDecide(IDParam("", WIDGET_ID_RES_SWITCH_BTN));
	ListenDecide(IDParam("", WIDGET_ID_SHUTTER_BTN));

	Widget *shutterBtn = FindChild(IDParam("", WIDGET_ID_SHUTTER_BTN), 0);

	if (shutterBtn != NULL) {
		static const int32_t btnEvents[9] = {
			ButtonBase::CB_BTN_NORMAL,
			ButtonBase::CB_BTN_DISABLE,
			ButtonBase::CB_BTN_PRESS,
			ButtonBase::CB_BTN_PRESS_OUT,
			ButtonBase::CB_BTN_PRESS_REPEAT,
			ButtonBase::CB_BTN_PRESS_REPEAT_END,
			ButtonBase::CB_BTN_LONG_PRESS,
			ButtonBase::CB_BTN_MULTI_TAP,
			ButtonBase::CB_BTN_DECIDE
		};

		for (int i = 0; i < 9; ++i)
			shutterBtn->AddEventCallback(btnEvents[i], ShutterButtonCallback, this);
	}

	AttachViewfinderTapHandler(viewfinderPlane);
	if (camView != NULL) {
		camView->m_align_x = 2;
		camView->m_align_y = 2;
	}
	if (flashPlane != NULL) {
		flashPlane->m_align_x = 2;
		flashPlane->m_align_y = 2;
	}
	if (controlsPlane != NULL) {
		controlsPlane->m_align_x = 2;
		controlsPlane->m_align_y = 2;
	}

	m_uiFlags &= ~VIEW_FLAG_CONTROLS_HIDDEN;
	if (m_allowedResolutionCount <= 1) {
		Widget *resBtn = FindChild(IDParam("", WIDGET_ID_RES_SWITCH_BTN), 0);

		if (resBtn != NULL)
			HideWidgetWithTimer(resBtn, NULL);
	}
	if (m_allowedDeviceCount <= 1) {
		Widget *camBtn = FindChild(IDParam("", WIDGET_ID_CAMERA_SWITCH_BTN), 0);

		if (camBtn != NULL)
			HideWidgetWithTimer(camBtn, NULL);
	}
	if (shutterBtn != NULL)
		shutterBtn->SetFocus(0);

	StartPreview();
	Scene *root = GetRoot();

	if (root != NULL)
		HideWidgetWithTimer(root, NULL);
	Notify(CAMERAIMPORT_EVENT_READY, NULL);
}

void CameraImportView::HideConfirmButtons()
{
	static const uint32_t confirmBtns[2] = {
		WIDGET_ID_CONFIRM_OK_BTN,
		WIDGET_ID_CONFIRM_RETAKE_BTN
	};

	for (int i = 0; i < 2; ++i) {
		Widget *w = FindChild(IDParam("", confirmBtns[i]), 0);

		if (w != NULL && w->IsInherit(ButtonBase::TypeName()))
			((ButtonBase *)w)->Disable(false);
	}
}

void CameraImportView::CloseConfirmOverlay()
{
	Widget *confirmBar = FindChild(IDParam("", WIDGET_ID_CONFIRM_BAR), 0);

	if (confirmBar != NULL) {
		confirmBar->AddEventCallback(VIEW_EVENT_TRANSITION_OUT_END, ConfirmFadeOutEndCB,
					     this);
		ScePafCommon_B48C9BDC(0.0f, confirmBar, transition::Type_SlideFromBottom3, true,
				      false);
	}
}

int CameraImportView::FillCaptureResult(const CameraFrameInfo *frameInfo,
					const CameraLocationInfo *location)
{
	m_captureResult.output.width = (SceUInt32)frameInfo->cropWidth;
	m_captureResult.output.height = (SceUInt32)frameInfo->cropHeight;
	bool invalid = (frameInfo->cropWidth * frameInfo->cropHeight == 0);

	if (frameInfo->deviceId == CAMERA_CORE_DEVICE_FRONT)
		m_captureResult.output.device = 1;
	else if (frameInfo->deviceId == CAMERA_CORE_DEVICE_REAR)
		m_captureResult.output.device = 2;
	else
		invalid = true;

	switch (location->exifOrientation) {
	case 1:
		m_captureResult.output.orientation = 0;
		break;
	case 6:
		m_captureResult.output.orientation = 1;
		break;
	case 3:
		m_captureResult.output.orientation = 2;
		break;
	case 8:
		m_captureResult.output.orientation = 3;
		break;
	default:
		invalid = true;
		break;
	}
	return invalid ? -1 : 0;
}

void CameraImportView::FillLocationInfo(int deviceId, CameraLocationInfo *location)
{
	(void)deviceId;
	location->latitude = CAMERA_CORE_UNSET_COORD;
	location->longitude = CAMERA_CORE_UNSET_COORD;
	location->altitude = CAMERA_CORE_UNSET_COORD;
	location->horizontalAccuracy = CAMERA_CORE_UNSET_FLOAT;
	location->verticalAccuracy = CAMERA_CORE_UNSET_FLOAT;
	location->headingAccuracy = CAMERA_CORE_UNSET_FLOAT;
	location->utcTimestamp = 0;
	location->heading = CAMERA_CORE_UNSET_FLOAT;

	int exifOrient = 1;

	if (m_rotationEnabled) {
		switch (GetMotionOrientation()) {
		case CAMERA_ORIENTATION_NORMAL:
			exifOrient = 1;
			break;
		case CAMERA_ORIENTATION_180:
			exifOrient = 3;
			break;
		case CAMERA_ORIENTATION_90:
			exifOrient = 6;
			break;
		case CAMERA_ORIENTATION_270:
			exifOrient = 8;
			break;
		default:
			exifOrient = 1;
			break;
		}
	}
	location->exifOrientation = exifOrient;
}

void CameraImportView::PlayShutterSound()
{
	if (m_plugin == NULL)
		return;
	m_jobSlots.Cancel(JOB_SLOT_SHUTTER_SOUND);
	SharedPtr<JobItem> item(new ShutterSoundJobItem());
	JobQueue *queue = CameraImportJobQueueGet(1);

	if (item.get() != NULL && queue != NULL)
		m_jobSlots.Enqueue(JOB_SLOT_SHUTTER_SOUND, queue, item);
}

int CameraImportView::TakePicture()
{
	if ((m_uiFlags & (VIEW_FLAG_CAPTURING | VIEW_FLAG_CAPTURE_LOCKED)) != 0)
		return 1;
	if (m_dialogParam == NULL)
		return -1;

	void *outBuf = NULL;
	uint32_t outBufSize = 0;
	int encodeFmt = 0;
	bool bakeOverlay = (m_dialogParam->overlayMode == 2 || m_dialogParam->overlayMode == 3);

	if (m_dialogParam->outputMode == 0) {
		if (m_dialogParam->memRawParam != NULL) {
			outBuf = m_dialogParam->memRawParam->buffer;
			outBufSize = m_dialogParam->memRawParam->bufferSize;
			encodeFmt = 2;
		}
	} else if (m_dialogParam->outputMode == 1) {
		if (m_dialogParam->memJpegParam != NULL) {
			outBuf = m_dialogParam->memJpegParam->buffer;
			outBufSize = m_dialogParam->memJpegParam->bufferSize;
			encodeFmt = 0;
		}
	}
	if (outBuf == NULL)
		return -1;

	PlayShutterSound();
	CameraViewWidget *camView = FindCameraViewWidget();

	if (camView == NULL) {
		m_uiFlags &= ~7U;
		UpdateUiVisibility();
		return -1;
	}

	CameraFrameInfo frameInfo;
	CameraLocationInfo location;
	CameraPhotoMeta photoMeta;
	intrusive_ptr<Surface> capturedSurface;

	if (camView->GetCapturedFrame(&capturedSurface, &frameInfo) < 0 ||
	    capturedSurface.get() == NULL)
		return 1;

	FillLocationInfo(frameInfo.deviceId, &location);
	photoMeta.make = "Sony Computer Entertainment Inc.";
	photoMeta.model = "PlayStation(R)Vita";
	photoMeta.software = GetSystemSoftwareVersionString();
	FillCaptureResult(&frameInfo, &location);

	CameraEncodeInput encodeInput;

	encodeInput.surface = capturedSurface;
	encodeInput.frameInfo = &frameInfo;
	encodeInput.locationInfo = &location;
	encodeInput.photoMeta = &photoMeta;

	CameraEncodeOverlay encodeOverlay;

	if (bakeOverlay)
		encodeOverlay.surface = m_overlaySurface;

	CameraEncodeOption encodeOption;

	encodeOption.format = encodeFmt;
	encodeOption.destination = 1;
	encodeOption.outputBuffer = outBuf;
	encodeOption.outputBufferSize = outBufSize;
	encodeOption.jpegCompressionRatio = 25;
	encodeOption.overlayAlpha = 0xFF;

	SharedPtr<JobItem> encodeJob(
		CameraCoreCreateEncodeJob(encodeInput, encodeOverlay, encodeOption));
	JobQueue *encodeQueue = CameraImportJobQueueGetEncode();
	bool queued = (encodeJob.get() != NULL && encodeQueue != NULL &&
		       m_jobSlots.Enqueue(JOB_SLOT_ENCODE, encodeQueue, encodeJob) == 0);

	if (!queued) {
		m_uiFlags &= ~7U;
		UpdateUiVisibility();
		return -1;
	}

	m_uiFlags = (m_uiFlags & ~7U) | VIEW_FLAG_CAPTURING;
	Widget *flashWidget = FindChild(IDParam("", WIDGET_ID_FLASH_PLANE), 0);
	Plane *flashPlane =
		(flashWidget != NULL && flashWidget->IsInherit(Plane::TypeName()))
			? (Plane *)flashWidget
			: NULL;

	if (flashPlane != NULL) {
		flashPlane->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), NULL, Widget::ANIM_COLOR, 2,
				     0.0f, NULL, NULL);
		flashPlane->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new paf::Timer(150.0f),
				     Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
		if (flashPlane->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new paf::Timer(500.0f),
					 Widget::ANIM_COLOR, 0, 0.0f, ShutterFlashEndCB,
					 this) != 0) {
			m_uiFlags |= VIEW_FLAG_FLASH_DONE;
		} else {
			CameraViewWidget *view = FindCameraViewWidget();

			if (view != NULL)
				view->FreezeFrame();
		}
	} else {
		m_uiFlags |= VIEW_FLAG_FLASH_DONE;
	}
	UpdateUiVisibility();
	return 0;
}

void CameraImportView::OnDecide(int32_t type, Handler *self, Event *e)
{
	(void)type;
	(void)e;
	if (self == NULL)
		return;
	uint32_t idHash = ((Widget *)self)->m_name.GetIDHash();

	if ((m_uiFlags & (VIEW_FLAG_CONFIRM_MODE | VIEW_FLAG_DIALOG_BLOCKED |
			  VIEW_FLAG_PAGE_HIDDEN | VIEW_FLAG_CAPTURING)) != 0) {
		if (idHash == WIDGET_ID_CONFIRM_OK_BTN) {
			HideConfirmButtons();
			Notify(CAMERAIMPORT_EVENT_ACCEPT, NULL);
		} else if (idHash == WIDGET_ID_CONFIRM_RETAKE_BTN) {
			HideConfirmButtons();
			CloseConfirmOverlay();
			Widget *shutterBtn = FindChild(IDParam("", WIDGET_ID_SHUTTER_BTN), 0);

			if (shutterBtn != NULL)
				shutterBtn->SetFocus(0);
		}
		return;
	}

	switch (idHash) {
	case WIDGET_ID_SHUTTER_BTN: {
		int capRet = TakePicture();

		if (capRet < 0) {
			CameraImportEncodeResult failRes;

			failRes.photoId = (uint64_t)-1;
			failRes.result = capRet;
			Notify(CAMERAIMPORT_EVENT_CAPTURE_FAILED, &failRes);
		}
		break;
	}
	case WIDGET_ID_CANCEL_BTN:
		Notify(CAMERAIMPORT_EVENT_CANCEL, NULL);
		break;
	case WIDGET_ID_CAMERA_SWITCH_BTN: {
		CameraViewWidget *camView = FindCameraViewWidget();

		if (camView != NULL) {
			int curDev = camView->GetCameraDevice();
			int nextDev = curDev;

			if (m_allowedDeviceCount > 1) {
				for (int step = 1; step < 3; ++step) {
					int cand = (curDev + step) % 3;

					if (m_allowedDevices[cand]) {
						nextDev = cand;
						break;
					}
				}
			}
			camView->SetCameraDevice(nextDev);
			m_currentDevice = nextDev;
		}
		Notify(CAMERAIMPORT_EVENT_CAMERA_SWITCHED, NULL);
		break;
	}
	case WIDGET_ID_RES_SWITCH_BTN: {
		int curRes = 0;
		CameraViewWidget *camView = FindCameraViewWidget();

		if (camView != NULL) {
			int dev = camView->GetCameraDevice();

			if (dev == CAMERA_CORE_DEVICE_NONE)
				dev = CAMERA_CORE_DEVICE_REAR;
			CameraCoreGetConfig(dev, &curRes, NULL, NULL);
		}
		int nextRes = curRes;

		if (m_allowedResolutionCount > 1) {
			for (int step = 1; step < 8; ++step) {
				int cand = (curRes + step) % 8;

				if (m_allowedResolutions[cand]) {
					nextRes = cand;
					break;
				}
			}
		}
		ApplyCameraResolution(nextRes);
		m_currentResolution = nextRes;
		Notify(CAMERAIMPORT_EVENT_RESOLUTION_SWITCHED, NULL);
		break;
	}
	default:
		break;
	}
}

void CameraImportView::OnButton(int32_t type, Handler *self, Event *e)
{
	(void)type;
	(void)self;
	(void)e;
}

void CameraImportView::ConfirmFadeOutEndCB(int32_t type, Handler *self, Event *e, void *userData)
{
	if (userData != NULL)
		((CameraImportView *)userData)->OnConfirmFadeOutEnd(type, self, e);
}

void CameraImportView::OnConfirmFadeOutEnd(int32_t type, Handler *self, Event *e)
{
	(void)type;
	(void)e;
	if (self != NULL && ((Widget *)self)->m_name.GetIDHash() == WIDGET_ID_CONFIRM_BAR) {
		m_uiFlags &= ~VIEW_FLAG_CONFIRM_MODE;
		CameraViewWidget *camView = FindCameraViewWidget();

		if (camView != NULL)
			camView->UnfreezeFrame();
		UpdateUiVisibility();
	}
}

void CameraImportView::ViewfinderTapCB(int32_t type, Handler *self, Event *e, void *userData)
{
	if (userData != NULL)
		((CameraImportView *)userData)->OnViewfinderTap(type, self, e);
}

void CameraImportView::OnViewfinderTap(int32_t type, Handler *self, Event *e)
{
	(void)type;
	(void)e;
	if (self == NULL || ((Widget *)self)->m_name.GetIDHash() != WIDGET_ID_VIEWFINDER_PLANE)
		return;
	if ((m_uiFlags & (VIEW_FLAG_CONFIRM_MODE | VIEW_FLAG_DIALOG_BLOCKED |
			  VIEW_FLAG_PAGE_HIDDEN | VIEW_FLAG_CAPTURING)) != 0)
		return;
	if ((m_uiFlags & VIEW_FLAG_CONTROLS_HIDDEN) != 0)
		m_uiFlags &= ~VIEW_FLAG_CONTROLS_HIDDEN;
	else
		m_uiFlags |= VIEW_FLAG_CONTROLS_HIDDEN;
	UpdateUiVisibility();
}

void CameraImportView::ShutterButtonCallback(int32_t type, Handler *self, Event *e, void *userData)
{
	if (userData != NULL)
		((CameraImportView *)userData)->OnShutterButtonStateChanged(type, self, e);
}

void CameraImportView::OnShutterButtonStateChanged(int32_t type, Handler *self, Event *e)
{
	(void)type;
	(void)self;
	(void)e;
	Widget *w = FindChild(IDParam("", WIDGET_ID_SHUTTER_BTN), 0);

	if (w == NULL || !w->IsInherit(ButtonBase::TypeName()))
		return;
	ButtonBase *shutterBtn = (ButtonBase *)w;
	IDParam texId("");

	switch (shutterBtn->m_btn_state) {
	case ButtonBase::ST_BTN_NORMAL:
	case ButtonBase::ST_BTN_DISABLE:
	case ButtonBase::ST_BTN_PRESS_OUT:
		texId = s_shutterNormalTexId;
		break;
	case ButtonBase::ST_BTN_PRESS:
	case ButtonBase::ST_BTN_PRESS_REPEAT:
		texId = s_shutterPressedTexId;
		break;
	default:
		break;
	}
	if (m_plugin != NULL) {
		intrusive_ptr<Surface> tex = m_plugin->GetTexture(texId);

		if (tex.get() != NULL)
			shutterBtn->SetTexture(tex, 0, 0);
	}
}

void CameraImportView::CameraViewStatusCB(void *userData, int status, int arg)
{
	(void)arg;
	if (userData != NULL)
		((CameraImportView *)userData)->OnCameraViewStatus(status);
}

void CameraImportView::OnCameraViewStatus(int status)
{
	if (status == 1) {
		CameraViewWidget *camView = FindCameraViewWidget();

		if (camView != NULL) {
			bool wasVisible = (camView->GetColor().extract_w() != 0.0f);

			camView->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL,
					  Widget::ANIM_COLOR, 2, 0.0f, NULL, NULL);
			if (wasVisible) {
				Widget *overlay = FindChild(IDParam("CameraOverlayImageWidget"), 0);

				if (overlay != NULL)
					overlay->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f), NULL,
							  Widget::ANIM_COLOR, 2, 0.0f, NULL, NULL);
			}
		}
		m_uiFlags |= VIEW_FLAG_PREVIEW_SWITCHING;
		UpdateUiVisibility();
	} else if (status == 0) {
		Widget *overlay = FindChild(IDParam("CameraOverlayImageWidget"), 0);

		if (overlay != NULL) {
			bool alreadyOpaque = (overlay->GetColor().extract_w() == 1.0f);

			UpdateOverlayGeometry();
			if (!alreadyOpaque)
				overlay->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
						  new paf::Timer(200.0f), Widget::ANIM_COLOR, 2, 0.0f,
						  NULL, NULL);
		}
		CameraViewWidget *camView = FindCameraViewWidget();

		if (camView != NULL)
			camView->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), new paf::Timer(200.0f),
					  Widget::ANIM_COLOR, 2, 0.0f, PreviewFadeEndCB, this);
	}
}

void CameraImportView::PreviewFadeEndCB(paf::Timer *t, AnimListener *listener, void *argA,
					void *argB, void *argC)
{
	(void)t;
	(void)listener;
	(void)argA;
	(void)argC;
	if (argB != NULL) {
		CameraImportView *self = (CameraImportView *)argB;

		self->m_uiFlags &= ~VIEW_FLAG_PREVIEW_SWITCHING;
		self->UpdateUiVisibility();
	}
}

void CameraImportView::SetDialogBlocked()
{
	m_uiFlags |= VIEW_FLAG_DIALOG_BLOCKED;
	UpdateUiVisibility();
}

void CameraImportView::OnJobSlotRemoved(uint32_t index, SharedPtr<JobItem> item)
{
	if (index != JOB_SLOT_ENCODE)
		return;
	if ((m_uiFlags & VIEW_FLAG_CAPTURING) == 0)
		return;
	m_uiFlags |= VIEW_FLAG_ENCODE_DONE;
	if (item.get() != NULL) {
		ImageEncodeJobItem *encJob = (ImageEncodeJobItem *)item.get();

		m_encodeResult.result = encJob->GetResult();
		m_encodeResult.photoId = encJob->GetPhotoId();
		m_captureResult.output.size = encJob->GetEncodedSize();
	} else {
		m_encodeResult.result = -1;
		m_encodeResult.photoId = (uint64_t)-1;
	}
	if ((m_uiFlags & 7U) == 7U) {
		m_uiFlags &= ~7U;
		Notify(CAMERAIMPORT_EVENT_ENCODE_DONE, &m_encodeResult);
	}
}

void CameraImportView::JobSlotRemovedCB(uint32_t index, SharedPtr<JobItem> item, void *userData)
{
	if (userData != NULL)
		((CameraImportView *)userData)->OnJobSlotRemoved(index, item);
}

void CameraImportView::OpenConfirmOverlay()
{
	Widget *flashPlane = FindChild(IDParam("", WIDGET_ID_FLASH_PLANE), 0);

	if (flashPlane != NULL) {
		if (flashPlane->FindChild(IDParam("", WIDGET_ID_CONFIRM_BAR), 2) == NULL) {
			if (m_plugin != NULL &&
			    m_plugin->TemplateOpen(flashPlane,
						   IDParam("", WIDGET_ID_CONFIRM_TEMPLATE),
						   Plugin::TemplateOpenParam()) == 0) {
				ListenDecide(IDParam("", WIDGET_ID_CONFIRM_OK_BTN));
				ListenDecide(IDParam("", WIDGET_ID_CONFIRM_RETAKE_BTN));
				Widget *confirmBar =
					flashPlane->FindChild(IDParam("", WIDGET_ID_CONFIRM_BAR), 0);

				if (confirmBar != NULL)
					ScePafCommon_6CF3A3C8(0.0f, confirmBar,
							      transition::Type_SlideFromBottom3,
							      true, false);
			}
		}
	}
	m_uiFlags |= VIEW_FLAG_CONFIRM_MODE;
	UpdateUiVisibility();
}

void CameraImportView::EnterConfirmMode()
{
	OpenConfirmOverlay();
	Widget *okBtn = FindChild(IDParam("", WIDGET_ID_CONFIRM_OK_BTN), 0);

	if (okBtn != NULL)
		okBtn->SetFocus(0);
}

void CameraImportView::OnPadInput(const inputdevice::pad::Data *data,
				  const inputdevice::pad::Data *pre)
{
	if (!IsOpen())
		return;
	if ((m_uiFlags & (VIEW_FLAG_CONFIRM_MODE | VIEW_FLAG_DIALOG_BLOCKED |
			  VIEW_FLAG_PAGE_HIDDEN | VIEW_FLAG_CAPTURING)) != 0)
		return;
	uint32_t curButtons = (data != NULL) ? (data->buttons & VIEW_PAD_IGNORE_MASK) : 0;
	uint32_t preButtons = (pre != NULL) ? (pre->buttons & VIEW_PAD_IGNORE_MASK) : 0;

	if (preButtons == 0 && curButtons == VIEW_PAD_CROSS_MASK) {
		Widget *w = FindChild(IDParam("", WIDGET_ID_SHUTTER_BTN), 0);

		if (w != NULL && w->IsInherit(ButtonBase::TypeName())) {
			ButtonBase *btn = (ButtonBase *)w;

			if (btn->m_enable && btn->m_active) {
				int capRet = TakePicture();

				if (capRet < 0) {
					CameraImportEncodeResult failRes;

					failRes.photoId = (uint64_t)-1;
					failRes.result = capRet;
					Notify(CAMERAIMPORT_EVENT_CAPTURE_FAILED, &failRes);
				}
			}
		}
	}
}

void CameraImportView::OnOrientationChanged(int orientation)
{
	RotateControls(orientation);
}

void CameraImportView::OnMotionSensorError(int error)
{
	if (m_rotationEnabled && error == 1)
		Notify(CAMERAIMPORT_EVENT_MOTION_ERROR, NULL);
}

void CameraImportView::OnEvent(EventDispatcher *sender, int32_t event, void *arg)
{
	(void)sender;
	(void)event;
	(void)arg;
}

void CameraImportView::ShutterFlashEndCB(paf::Timer *t, AnimListener *listener, void *argA,
					 void *argB, void *argC)
{
	(void)t;
	(void)listener;
	(void)argA;
	(void)argC;
	if (argB == NULL)
		return;
	CameraImportView *self = (CameraImportView *)argB;

	if ((self->m_uiFlags & VIEW_FLAG_CAPTURING) != 0) {
		self->m_uiFlags |= VIEW_FLAG_FLASH_DONE;
		if ((self->m_uiFlags & 7U) == 7U) {
			self->m_uiFlags &= ~7U;
			self->Notify(CAMERAIMPORT_EVENT_ENCODE_DONE,
					    &self->m_encodeResult);
		}
	}
}
