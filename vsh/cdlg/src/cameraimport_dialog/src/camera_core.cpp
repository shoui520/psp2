/*
 * SceCamImportDialogPlugin: SceCameraCore implementation.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <camera.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <common/main_thread_call_list.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <thread/thread.h>
#include "camera_core.h"
#include "picture_file_creator.h"

extern "C" {
SceInt32 sceKernelGetSystemSwVersion(void *param);
SceUInt64 sceKernelGetProcessTimeWide(void);
SceInt32 sceCameraSetImageQuality(int devnum, int quality);
}

using namespace paf;
using namespace paf::graph;
using namespace paf::thread;

/* Global camera open mutex shared by front and rear camera threads */
static Mutex s_cameraOpenMutex("SceCameraCoreCameraDeviceCameraOpenMutex", 0);

/* Global SceCameraCore manager */
class CameraCoreManager : public CameraCoreListener {
public:
	CameraCoreManager() : m_errorListener(NULL) {}
	virtual ~CameraCoreManager() {}

	virtual void OnCameraEvent(int event, int arg, int result)
	{
		if (m_errorListener != NULL)
			m_errorListener->OnCameraEvent(event, arg, result);
	}

	map<int, CameraDeviceThread *> m_devices;
	CameraCoreListener *m_errorListener;
};

static CameraCoreManager *s_coreManager = NULL;

/* SurfaceInstancePool */
SurfaceInstancePool::SurfaceInstancePool()
	: m_initialized(false), m_ownsPool(false), m_poolMemory(NULL), m_surfacePool(NULL),
	  m_surfaces(NULL), m_surfaceCount(0), m_format((ImageMode)-1), m_width(0), m_height(0),
	  m_mutex("SceCameraCoreSurfaceInstancePoolMutex", 0)
{
}

SurfaceInstancePool::~SurfaceInstancePool()
{
}

int SurfaceInstancePool::Initialize(SurfacePool *pool, uint32_t count, ImageMode format)
{
	m_mutex.Lock();
	if (m_initialized) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_STATE;
	}
	if (pool == NULL) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	}
	m_format = format;
	m_surfacePool = pool;
	m_ownsPool = false;
	m_width = 0;
	m_height = 0;
	m_surfaceCount = count;
	m_surfaces = new intrusive_ptr<Surface>[count];
	if (m_surfaces == NULL) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_OUT_OF_MEMORY;
	}
	m_initialized = true;
	m_mutex.Unlock();
	return 0;
}

int SurfaceInstancePool::Terminate()
{
	m_mutex.Lock();
	if (m_surfaces != NULL) {
		delete[] m_surfaces;
		m_surfaces = NULL;
	}
	if (m_ownsPool) {
		if (m_surfacePool != NULL) {
			delete m_surfacePool;
			m_surfacePool = NULL;
		}
		m_poolMemory = NULL;
	} else {
		m_poolMemory = NULL;
		m_surfacePool = NULL;
	}
	m_initialized = false;
	m_mutex.Unlock();
	return 0;
}

void SurfaceInstancePool::ReallocateSurfaces(int width, int height)
{
	for (uint32_t i = 0; i < m_surfaceCount; ++i)
		m_surfaces[i] = NULL;
	for (uint32_t i = 0; i < m_surfaceCount; ++i) {
		Surface *raw = new Surface(m_surfacePool, width, height, m_format,
					   (ImageOrder)1, 1, 256, 0, 1, false);
		intrusive_ptr<Surface> surf(raw);

		if (surf.get() != NULL) {
			if (surf->GetMemory() != NULL) {
				surf->SetMinFilter(LINEAR);
			} else {
				surf = NULL;
			}
		}
		m_surfaces[i] = surf;
	}
}

intrusive_ptr<Surface> SurfaceInstancePool::AcquireSurface(int width, int height)
{
	m_mutex.Lock();
	if (!m_initialized || width == 0 || height == 0 || m_surfaceCount == 0) {
		m_mutex.Unlock();
		return intrusive_ptr<Surface>();
	}
	if (m_width == width && m_height == height) {
		for (uint32_t i = 0; i < m_surfaceCount; ++i) {
			Surface *surf = m_surfaces[i].get();

			if (surf == NULL || surf->GetRefCount() <= 1) {
				intrusive_ptr<Surface> result(surf);

				m_mutex.Unlock();
				return result;
			}
		}
		m_mutex.Unlock();
		return intrusive_ptr<Surface>();
	}
	for (uint32_t i = 0; i < m_surfaceCount; ++i) {
		Surface *surf = m_surfaces[i].get();

		if (surf != NULL && surf->GetRefCount() > 1) {
			m_mutex.Unlock();
			return intrusive_ptr<Surface>();
		}
	}
	ReallocateSurfaces(width, height);
	m_width = width;
	m_height = height;
	intrusive_ptr<Surface> first(m_surfaces[0]);

	m_mutex.Unlock();
	return first;
}

/* CameraIf */
CameraIf::CameraIf()
	: m_state(0), m_mutex("SceCameraCoreCameraIfStatusFlagMutex", 0), m_framerateType(0),
	  m_deviceId(0), m_width(0), m_height(0), m_formatType(0), m_hwDevice(0)
{
	sce_paf_memset(m_cropRect, 0, sizeof(m_cropRect));
	sce_paf_memset(&m_cameraInfo, 0, sizeof(m_cameraInfo));
	for (int i = 0; i < CAMERA_CORE_PROP_COUNT; ++i)
		m_properties[i] = CAMERA_CORE_UNSET_VALUE;
}

CameraIf::~CameraIf()
{
}

int CameraIf::Initialize()
{
	m_mutex.Lock();
	if (m_state > 0) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_STATE;
	}
	m_state = 1;
	m_mutex.Unlock();
	return 0;
}

int CameraIf::Close()
{
	m_mutex.Lock();
	if (m_state != 2) {
		m_mutex.Unlock();
		return 1;
	}
	m_state = 1;
	int ret = sceCameraClose(m_hwDevice);

	m_mutex.Unlock();
	return ret != 0 ? ret : 0;
}

int CameraIf::Stop()
{
	m_mutex.Lock();
	if (m_state != 3) {
		m_mutex.Unlock();
		return 1;
	}
	m_state = 2;
	int ret = sceCameraStop(m_hwDevice);

	m_mutex.Unlock();
	return ret >= 0 ? 0 : ret;
}

int CameraIf::Terminate()
{
	m_mutex.Lock();
	if (m_state <= 0) {
		m_mutex.Unlock();
		return 1;
	}
	uint32_t stopRet = (uint32_t)Stop();
	uint32_t closeRet = (uint32_t)Close();
	uint32_t finalRet = stopRet > closeRet ? stopRet : closeRet;

	m_state = 0;
	m_mutex.Unlock();
	return (int)finalRet;
}

void CameraIf::FillCameraInfo(SceCameraInfo *info, int width, int height, int framerateType,
			      int formatType)
{
	if (info == NULL)
		return;
	sce_paf_memset(info, 0, sizeof(*info));
	info->sizeThis = sizeof(SceCameraInfo);
	info->wPriority = SCE_CAMERA_PRIORITY_SHARE;
	if (formatType == 1 || formatType == 2)
		info->wFormat = SCE_CAMERA_FORMAT_YUV420_PLANE;
	else if (formatType == 3)
		info->wFormat = SCE_CAMERA_FORMAT_YUV422_TO_ABGR;
	else
		info->wFormat = SCE_CAMERA_FORMAT_UNKNOWN;

	if (width == 640 && height == 480)
		info->wResolution = SCE_CAMERA_RESOLUTION_VGA;
	else if (width == 320 && height == 240)
		info->wResolution = SCE_CAMERA_RESOLUTION_QVGA;
	else if (width == 160 && height == 120)
		info->wResolution = SCE_CAMERA_RESOLUTION_QQVGA;
	else if (width == 352 && height == 288)
		info->wResolution = SCE_CAMERA_RESOLUTION_CIF;
	else if (width == 176 && height == 144)
		info->wResolution = SCE_CAMERA_RESOLUTION_QCIF;
	else if (width == 480 && height == 272)
		info->wResolution = SCE_CAMERA_RESOLUTION_PSP1;
	else if (width == 640 && height == 360)
		info->wResolution = SCE_CAMERA_RESOLUTION_NGP1;
	else
		info->wResolution = SCE_CAMERA_RESOLUTION_UNKNOWN;

	if (framerateType == 1)
		info->wFramerate = SCE_CAMERA_FRAMERATE_30;
	else if (framerateType == 2)
		info->wFramerate = SCE_CAMERA_FRAMERATE_60;
	else if (framerateType == 3)
		info->wFramerate = SCE_CAMERA_FRAMERATE_120;
	else
		info->wFramerate = SCE_CAMERA_FRAMERATE_UNKNOWN;

	if (formatType == 1 || formatType == 3)
		info->wRange = SCE_CAMERA_DATA_RANGE_FULL;
	else if (formatType == 2)
		info->wRange = SCE_CAMERA_DATA_RANGE_BT601;
	else
		info->wRange = (SceUInt16)-1;

	info->wBuffer = SCE_CAMERA_BUFFER_SETBYREAD;
}

int CameraIf::ResolvePreset(int preset, int *outWidth, int *outHeight, int outCropRect[4])
{
	if (outWidth == NULL || outHeight == NULL || outCropRect == NULL)
		return CAMERA_CORE_ERROR_GENERIC;
	switch (preset) {
	case CAMERA_CORE_RES_640X480:
		*outWidth = 640;
		*outHeight = 480;
		outCropRect[0] = 0;
		outCropRect[1] = 0;
		outCropRect[2] = 640;
		outCropRect[3] = 480;
		return 0;
	case CAMERA_CORE_RES_320X240:
		*outWidth = 320;
		*outHeight = 240;
		outCropRect[0] = 0;
		outCropRect[1] = 0;
		outCropRect[2] = 320;
		outCropRect[3] = 240;
		return 0;
	case CAMERA_CORE_RES_640X360:
		*outWidth = 640;
		*outHeight = 480;
		outCropRect[0] = 60;
		outCropRect[1] = 0;
		outCropRect[2] = 640;
		outCropRect[3] = 360;
		return 0;
	case CAMERA_CORE_RES_320X180:
		*outWidth = 320;
		*outHeight = 240;
		outCropRect[0] = 30;
		outCropRect[1] = 0;
		outCropRect[2] = 320;
		outCropRect[3] = 180;
		return 0;
	case CAMERA_CORE_RES_480X272:
		*outWidth = 480;
		*outHeight = 272;
		outCropRect[0] = 0;
		outCropRect[1] = 0;
		outCropRect[2] = 480;
		outCropRect[3] = 272;
		return 0;
	case CAMERA_CORE_RES_480X270:
		*outWidth = 480;
		*outHeight = 272;
		outCropRect[0] = 0;
		outCropRect[1] = 0;
		outCropRect[2] = 480;
		outCropRect[3] = 270;
		return 0;
	case CAMERA_CORE_RES_480X480:
		*outWidth = 640;
		*outHeight = 480;
		outCropRect[0] = 0;
		outCropRect[1] = 80;
		outCropRect[2] = 480;
		outCropRect[3] = 480;
		return 0;
	default:
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	}
}

int CameraIf::Open(int deviceId, int resolutionPreset, int framerateType, int formatType)
{
	m_mutex.Lock();
	if (m_state != 1) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_STATE;
	}
	m_deviceId = deviceId;
	if (deviceId == CAMERA_CORE_DEVICE_FRONT)
		m_hwDevice = SCE_CAMERA_DEVICE_FRONT;
	else if (deviceId == CAMERA_CORE_DEVICE_REAR)
		m_hwDevice = SCE_CAMERA_DEVICE_BACK;
	else
		m_hwDevice = -1;

	m_framerateType = framerateType;
	m_formatType = formatType;
	if (m_hwDevice < 0 ||
	    ResolvePreset(resolutionPreset, &m_width, &m_height, m_cropRect) < 0) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	}
	FillCameraInfo(&m_cameraInfo, m_width, m_height, m_framerateType, m_formatType);
	int ret = sceCameraOpen(m_hwDevice, &m_cameraInfo);

	if (ret != 0) {
		m_mutex.Unlock();
		return ret;
	}
	m_state = 2;
	m_mutex.Unlock();
	return 0;
}

int CameraIf::Start()
{
	m_mutex.Lock();
	if (m_state != 2) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_STATE;
	}
	int ret = sceCameraStart(m_hwDevice);

	if (ret != 0) {
		m_mutex.Unlock();
		return ret;
	}
	m_state = 3;
	if (m_formatType == 1)
		sceCameraSetImageQuality(m_hwDevice, 3);
	else if (m_formatType == 2)
		sceCameraSetImageQuality(m_hwDevice, 4);
	for (int i = 0; i < CAMERA_CORE_PROP_COUNT; ++i)
		QueryProperty(i, &m_properties[i]);
	m_mutex.Unlock();
	return 0;
}

int CameraIf::QueryProperty(int propId, int *outValue)
{
	if (outValue == NULL)
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	m_mutex.Lock();
	if (m_state < 3) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_STATE;
	}
	int ret;

	switch (propId) {
	case CAMERA_CORE_PROP_ISO:
		ret = sceCameraGetISO(m_hwDevice, outValue);
		break;
	case CAMERA_CORE_PROP_NIGHTMODE:
		ret = sceCameraGetNightmode(m_hwDevice, outValue);
		break;
	case CAMERA_CORE_PROP_EFFECT:
		ret = sceCameraGetEffect(m_hwDevice, outValue);
		break;
	case CAMERA_CORE_PROP_EV:
		ret = sceCameraGetEV(m_hwDevice, outValue);
		break;
	case CAMERA_CORE_PROP_WHITE_BALANCE:
		ret = sceCameraGetWhiteBalance(m_hwDevice, outValue);
		break;
	case CAMERA_CORE_PROP_SATURATION:
		ret = sceCameraGetSaturation(m_hwDevice, outValue);
		break;
	case CAMERA_CORE_PROP_BACKLIGHT:
		ret = sceCameraGetBacklight(m_hwDevice, outValue);
		break;
	default:
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	}
	if (ret != 0)
		*outValue = CAMERA_CORE_UNSET_VALUE;
	m_mutex.Unlock();
	return ret;
}

int CameraIf::GetPropertyCached(uint32_t propId, int *outValue)
{
	if (outValue == NULL)
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	m_mutex.Lock();
	if (m_state < 3) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_STATE;
	}
	if (propId >= CAMERA_CORE_PROP_COUNT) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	}
	*outValue = m_properties[propId];
	m_mutex.Unlock();
	return 0;
}

int CameraIf::SetProperty(int propId, int value)
{
	m_mutex.Lock();
	if (m_state < 3) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_STATE;
	}
	int ret;

	switch (propId) {
	case CAMERA_CORE_PROP_ISO:
		ret = sceCameraSetISO(m_hwDevice, value);
		break;
	case CAMERA_CORE_PROP_NIGHTMODE:
		ret = sceCameraSetNightmode(m_hwDevice, value);
		break;
	case CAMERA_CORE_PROP_EFFECT:
		ret = sceCameraSetEffect(m_hwDevice, value);
		break;
	case CAMERA_CORE_PROP_EV:
		ret = sceCameraSetEV(m_hwDevice, value);
		break;
	case CAMERA_CORE_PROP_WHITE_BALANCE:
		ret = sceCameraSetWhiteBalance(m_hwDevice, value);
		break;
	case CAMERA_CORE_PROP_SATURATION:
		ret = sceCameraSetSaturation(m_hwDevice, value);
		break;
	case CAMERA_CORE_PROP_BACKLIGHT:
		ret = sceCameraSetBacklight(m_hwDevice, value);
		break;
	default:
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	}
	if (ret == 0)
		m_properties[propId] = value;
	m_mutex.Unlock();
	return ret;
}

int CameraIf::GetResolution(int *outWidth, int *outHeight)
{
	m_mutex.Lock();
	if (m_state < 2) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_STATE;
	}
	if (outWidth == NULL || outHeight == NULL) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	}
	*outWidth = m_width;
	*outHeight = m_height;
	m_mutex.Unlock();
	return 0;
}

int CameraIf::ReadFrame(intrusive_ptr<Surface> *surface, CameraFrameInfo *outInfo)
{
	if (surface == NULL || surface->get() == NULL || outInfo == NULL)
		return CAMERA_CORE_ERROR_GENERIC;
	m_mutex.Lock();
	if (m_state != 3) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_INVALID_STATE;
	}
	Surface *surf = surface->get();

	if (surf->GetWidth() != m_width || surf->GetHeight() != m_height) {
		m_mutex.Unlock();
		return CAMERA_CORE_ERROR_GENERIC;
	}
	SceCameraRead readReq;

	sce_paf_memset(&readReq, 0, sizeof(readReq));
	readReq.sizeThis = sizeof(SceCameraRead);
	readReq.dwMode = SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_ON;

	int readRet = 0;
	uint8_t *pixels = (uint8_t *)surf->Lock(0);

	if (pixels != NULL) {
		if (m_formatType == 1 || m_formatType == 2) {
			uint32_t alignedWidth = ((uint32_t)m_width + 7U) & ~7U;
			uint32_t alignedHalfWidth = ((alignedWidth / 2U) + 7U) & ~7U;
			uint32_t halfHeight = (uint32_t)m_height / 2U;
			uint32_t ySize = alignedWidth * (uint32_t)m_height;
			uint32_t uvSize = alignedHalfWidth * halfHeight;

			readReq.sizeIBase = ySize;
			readReq.sizeUBase = uvSize;
			readReq.sizeVBase = uvSize;
			readReq.pvIBase = pixels;
			readReq.pvUBase = pixels + ySize;
			readReq.pvVBase = pixels + ySize + uvSize;
		} else {
			readReq.sizeIBase = (SceSize)surf->GetPageSize();
			readReq.sizeUBase = 0;
			readReq.sizeVBase = 0;
			readReq.pvIBase = pixels;
			readReq.pvUBase = NULL;
			readReq.pvVBase = NULL;
		}
		readRet = sceCameraRead(m_hwDevice, &readReq);
	}
	surf->Unlock();

	if (readRet != 0) {
		paf::thread::Sleep(4);
		m_mutex.Unlock();
		return readRet;
	}
	int statusRet = 0;

	if (readReq.dwStatus != SCE_CAMERA_STATUS_IS_ACTIVE) {
		if (readReq.dwStatus == SCE_CAMERA_STATUS_IS_NOT_STABLE) {
			statusRet = CAMERA_CORE_ERROR_NOT_STABLE;
		} else if (readReq.dwStatus == SCE_CAMERA_STATUS_IS_FORCED_STOP_POWER_CONFIG_CHANGE) {
			paf::thread::Sleep(4);
			statusRet = CAMERA_CORE_ERROR_FORCED_STOP;
		} else if (readReq.dwStatus >= 0) {
			paf::thread::Sleep(4);
			m_mutex.Unlock();
			return 1;
		} else {
			paf::thread::Sleep(4);
			statusRet = CAMERA_CORE_ERROR_READ_ERROR;
		}
	}

	outInfo->deviceId = m_deviceId;
	outInfo->timestamp = readReq.qwTimestamp;
	outInfo->framerate = m_framerateType;
	outInfo->format = m_formatType;
	outInfo->cropY = m_cropRect[0];
	outInfo->cropX = m_cropRect[1];
	outInfo->cropWidth = m_cropRect[2];
	outInfo->cropHeight = m_cropRect[3];
	outInfo->iso = m_properties[CAMERA_CORE_PROP_ISO];
	outInfo->nightmode = m_properties[CAMERA_CORE_PROP_NIGHTMODE];
	m_mutex.Unlock();
	return statusRet;
}

/* MainThreadCaller */
MainThreadCaller::MainThreadCaller()
	: m_mutex("SceCameraCoreMainThreadCallerMutex", 0),
	  m_cond(m_mutex, "SceCameraCoreMainThreadCallerCond", 0), m_listener(NULL), m_event(0),
	  m_arg(0), m_result(0)
{
}

MainThreadCaller::~MainThreadCaller()
{
}

void MainThreadCaller::Invoke(CameraCoreListener *listener, int event, int arg, int result)
{
	if (listener == NULL)
		return;
	m_mutex.Lock();
	m_listener = listener;
	m_event = event;
	m_arg = arg;
	m_result = result;
	common::MainThreadCallList::Register(CallbackEntry, this);
	m_cond.Wait(0);
	m_mutex.Unlock();
}

void MainThreadCaller::CallbackEntry(void *userData)
{
	MainThreadCaller *self = (MainThreadCaller *)userData;

	if (self == NULL)
		return;
	self->m_mutex.Lock();
	if (self->m_listener != NULL)
		self->m_listener->OnCameraEvent(self->m_event, self->m_arg, self->m_result);
	self->m_cond.NotifyAll();
	self->m_mutex.Unlock();
}

/* CameraDeviceThread */
CameraDeviceThread::CameraDeviceThread()
	: Thread(0x10000100, 0x4000, "SceCameraCoreCameraDeviceThread", NULL),
	  m_initialized(false), m_surfacePool(NULL), m_listeners(), m_deviceId(0),
	  m_resolutionPreset(0), m_framerateType(0), m_formatType(0), m_propsDirty(false),
	  m_cameraIf(NULL), m_propertyMutex("SceCameraCoreCameraDevicePropertyMutex", 0),
	  m_loopMutex("SceCameraCoreCameraDeviceLoopStatusMutex", 0),
	  m_loopCond(m_loopMutex, "SceCameraCoreCameraDeviceLoopStatusCond", 0),
	  m_needReopen(false), m_mainThreadCaller(), m_errorListener(NULL)
{
}

CameraDeviceThread::~CameraDeviceThread()
{
}

int CameraDeviceThread::Initialize(int deviceId, CameraCoreListener *errorListener)
{
	if (m_initialized)
		return CAMERA_CORE_ERROR_INVALID_STATE;
	if (deviceId != CAMERA_CORE_DEVICE_FRONT && deviceId != CAMERA_CORE_DEVICE_REAR)
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	m_deviceId = deviceId;
	m_initialized = true;
	m_errorListener = errorListener;

	m_propertyMutex.Lock();
	m_resolutionPreset = CAMERA_CORE_RES_640X480;
	m_framerateType = 1;
	m_formatType = 2;
	m_propertyMutex.Unlock();

	for (int i = 0; i < CAMERA_CORE_PROP_COUNT; ++i) {
		m_appliedProps[i] = CAMERA_CORE_UNSET_VALUE;
		m_requestedProps[i] = CAMERA_CORE_UNSET_VALUE;
	}
	m_propsDirty = false;
	m_cameraIf = new CameraIf();
	if (m_cameraIf == NULL)
		return CAMERA_CORE_ERROR_OUT_OF_MEMORY;
	int ret = m_cameraIf->Initialize();

	if (ret >= 0 && Start() != 0)
		return CAMERA_CORE_ERROR_GENERIC;
	return ret;
}

int CameraDeviceThread::Terminate()
{
	if (!m_initialized)
		return CAMERA_CORE_ERROR_INVALID_STATE;
	Cancel();
	m_loopCond.NotifyAll();
	Join();
	m_surfacePool = NULL;
	if (m_cameraIf != NULL) {
		m_cameraIf->Terminate();
		delete m_cameraIf;
		m_cameraIf = NULL;
	}
	return 0;
}

int CameraDeviceThread::GetConfig(int *outRes, int *outFps, int *outFmt)
{
	m_propertyMutex.Lock();
	if (outRes != NULL)
		*outRes = m_resolutionPreset;
	if (outFps != NULL)
		*outFps = m_framerateType;
	if (outFmt != NULL)
		*outFmt = m_formatType;
	m_propertyMutex.Unlock();
	return 0;
}

int CameraDeviceThread::SetConfig(int resolutionPreset, int framerateType, int formatType)
{
	m_propertyMutex.Lock();
	bool changed = (resolutionPreset != 0 && resolutionPreset != m_resolutionPreset) ||
		       (framerateType != 0 && framerateType != m_framerateType) ||
		       (formatType != 0 && formatType != m_formatType);

	if (!changed) {
		m_propertyMutex.Unlock();
		return 1;
	}
	if (resolutionPreset != 0)
		m_resolutionPreset = resolutionPreset;
	if (framerateType != 0)
		m_framerateType = framerateType;
	if (formatType != 0)
		m_formatType = formatType;
	m_propertyMutex.Unlock();

	DispatchEvent(CAMERA_CORE_EVENT_RESOLUTION_CHANGING, (void *)m_deviceId, 0);
	m_loopMutex.Lock();
	if (!m_listeners.size() || m_surfacePool == NULL) {
		DispatchEvent(CAMERA_CORE_EVENT_RESOLUTION_CHANGED, (void *)m_deviceId, 0);
	} else {
		m_needReopen = true;
		m_loopCond.NotifyAll();
	}
	m_loopMutex.Unlock();
	return 0;
}

int CameraDeviceThread::SetProperty(uint32_t propId, int value)
{
	if (propId >= CAMERA_CORE_PROP_COUNT)
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	m_propertyMutex.Lock();
	m_requestedProps[propId] = value;
	m_propsDirty = (value != m_appliedProps[propId]);
	m_propertyMutex.Unlock();

	DispatchEvent(CAMERA_CORE_EVENT_PROPERTY_CHANGING, (void *)m_deviceId, 0);
	m_loopMutex.Lock();
	if (!m_listeners.size() || m_surfacePool == NULL)
		DispatchEvent(CAMERA_CORE_EVENT_PROPERTY_CHANGED, (void *)m_deviceId, 0);
	m_loopMutex.Unlock();
	return 0;
}

int CameraDeviceThread::SetSurfacePool(SurfaceInstancePool *pool)
{
	m_loopMutex.Lock();
	m_propertyMutex.Lock();
	SurfaceInstancePool *prevPool = m_surfacePool;

	m_surfacePool = pool;
	m_propertyMutex.Unlock();

	if (prevPool != NULL) {
		CameraFramePacket emptyPacket;

		DispatchEvent(CAMERA_CORE_EVENT_FRAME_READY, &emptyPacket, 0);
	}
	if (!!m_listeners.size() && m_surfacePool != NULL)
		m_loopCond.NotifyAll();
	m_loopMutex.Unlock();
	return 0;
}

void CameraDeviceThread::AddListener(CameraDeviceListener *listener)
{
	if (listener == NULL)
		return;
	m_loopMutex.Lock();
	m_listeners.push_back(listener);
	if (!!m_listeners.size() && m_surfacePool != NULL)
		m_loopCond.NotifyAll();
	m_loopMutex.Unlock();
}

void CameraDeviceThread::RemoveListener(CameraDeviceListener *listener)
{
	if (listener == NULL)
		return;
	m_loopMutex.Lock();
	for (list<CameraDeviceListener *>::iterator it = m_listeners.begin();
	     it != m_listeners.end(); ++it) {
		if (*it == listener) {
			m_listeners.erase(it);
			break;
		}
	}
	m_loopMutex.Unlock();
}

void CameraDeviceThread::DispatchEvent(int event, void *arg, int result)
{
	for (list<CameraDeviceListener *>::iterator it = m_listeners.begin();
	     it != m_listeners.end(); ++it) {
		if (*it != NULL)
			(*it)->OnDeviceEvent(event, arg, result);
	}
}

int CameraDeviceThread::OpenAndStart(int resolutionPreset, int framerateType, int formatType)
{
	if (m_cameraIf == NULL)
		return CAMERA_CORE_ERROR_GENERIC;
	int ret = m_cameraIf->Open(m_deviceId, resolutionPreset, framerateType, formatType);

	if (ret >= 0)
		ret = m_cameraIf->Start();
	if (ret < 0 && m_cameraIf != NULL) {
		m_cameraIf->Stop();
		m_cameraIf->Close();
	}
	return ret;
}

int CameraDeviceThread::CaptureFrame(CameraFramePacket *packet)
{
	if (packet == NULL)
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	packet->surface = NULL;
	packet->info = CameraFrameInfo();
	if (m_cameraIf == NULL)
		return CAMERA_CORE_ERROR_GENERIC;
	int width = 0;
	int height = 0;
	int ret = m_cameraIf->GetResolution(&width, &height);

	if (ret < 0)
		return ret;
	if (m_surfacePool == NULL)
		return 1;
	packet->surface = m_surfacePool->AcquireSurface(width, height);
	if (packet->surface.get() == NULL)
		return CAMERA_CORE_ERROR_GENERIC;
	return m_cameraIf->ReadFrame(&packet->surface, &packet->info);
}

void CameraDeviceThread::EntryFunction()
{
	if (m_cameraIf == NULL)
		return;
	bool waitForWake = false;

	for (;;) {
		m_loopMutex.Lock();
		CameraFramePacket emptyPacket;

		DispatchEvent(CAMERA_CORE_EVENT_FRAME_READY, &emptyPacket, 0);
		if (((!m_listeners.size() || m_surfacePool == NULL) && !IsCanceled()) ||
		    waitForWake) {
			waitForWake = false;
			m_loopCond.Wait(0);
		}
		if (IsCanceled()) {
			m_loopMutex.Unlock();
			break;
		}
		m_needReopen = false;
		int res = m_resolutionPreset;
		int fps = m_framerateType;
		int fmt = m_formatType;

		m_loopMutex.Unlock();

		s_cameraOpenMutex.Lock();
		int openRet = OpenAndStart(res, fps, fmt);

		if (openRet < 0) {
			if (m_errorListener != NULL)
				m_mainThreadCaller.Invoke(m_errorListener, CAMERA_CORE_EVENT_DEVICE_ERROR,
							  m_deviceId, openRet);
			s_cameraOpenMutex.Unlock();
			waitForWake = true;
			continue;
		}
		DispatchEvent(CAMERA_CORE_EVENT_RESOLUTION_CHANGED, (void *)m_deviceId, 0);

		m_propertyMutex.Lock();
		for (int i = 0; i < CAMERA_CORE_PROP_COUNT; ++i)
			m_cameraIf->GetPropertyCached((uint32_t)i, &m_appliedProps[i]);
		m_propsDirty = true;
		m_propertyMutex.Unlock();

		for (;;) {
			m_loopMutex.Lock();
			bool shouldStop = !m_listeners.size() || m_surfacePool == NULL ||
					  IsCanceled() || m_needReopen;

			m_loopMutex.Unlock();
			if (shouldStop)
				break;

			bool updatedProps = false;
			uint32_t propErr = 0;

			m_propertyMutex.Lock();
			if (m_propsDirty) {
				for (int i = 0; i < CAMERA_CORE_PROP_COUNT; ++i) {
					if (m_requestedProps[i] != CAMERA_CORE_UNSET_VALUE &&
					    m_requestedProps[i] != m_appliedProps[i]) {
						uint32_t err = (uint32_t)m_cameraIf->SetProperty(
							i, m_requestedProps[i]);

						if (err > propErr)
							propErr = err;
						m_appliedProps[i] = m_requestedProps[i];
					}
				}
				updatedProps = true;
				m_propsDirty = false;
			}
			m_propertyMutex.Unlock();
			if (updatedProps)
				DispatchEvent(CAMERA_CORE_EVENT_PROPERTY_CHANGED, (void *)m_deviceId,
					      (int)propErr);

			CameraFramePacket packet;

			m_propertyMutex.Lock();
			int capRet = CaptureFrame(&packet);
			m_propertyMutex.Unlock();

			if (capRet != 1) {
				m_loopMutex.Lock();
				if (!!m_listeners.size() && m_surfacePool != NULL && !IsCanceled())
					DispatchEvent(CAMERA_CORE_EVENT_FRAME_READY, &packet, capRet);
				if (capRet == CAMERA_CORE_ERROR_FORCED_STOP) {
					if (m_errorListener != NULL)
						m_mainThreadCaller.Invoke(
							m_errorListener,
							CAMERA_CORE_EVENT_DEVICE_ERROR, m_deviceId,
							CAMERA_CORE_ERROR_FORCED_STOP);
					m_loopMutex.Unlock();
					waitForWake = true;
					break;
				}
				m_loopMutex.Unlock();
			}
			paf::thread::Sleep(2);
		}

		uint32_t closeErr = 0;

		if (m_cameraIf == NULL) {
			closeErr = (uint32_t)CAMERA_CORE_ERROR_GENERIC;
		} else {
			uint32_t stopErr = (uint32_t)m_cameraIf->Stop();
			uint32_t clErr = (uint32_t)m_cameraIf->Close();

			closeErr = stopErr > clErr ? stopErr : clErr;
		}
		if ((int)closeErr < 0 && m_errorListener != NULL)
			m_mainThreadCaller.Invoke(m_errorListener, CAMERA_CORE_EVENT_DEVICE_ERROR,
						  m_deviceId, (int)closeErr);
		s_cameraOpenMutex.Unlock();
	}
}

/* DeviceController */
DeviceController::DeviceController(CameraCoreListener *callback)
	: m_mutex("SceCameraCoreDeviceControllerMutex", 0), m_currentSurface(), m_frameInfo(),
	  m_device(NULL), m_paused(false), m_callback(callback)
{
}

DeviceController::~DeviceController()
{
	SetDevice(CAMERA_CORE_DEVICE_NONE);
}

void DeviceController::UpdateFrame(const intrusive_ptr<Surface> &surface,
				   const CameraFrameInfo &info)
{
	m_mutex.Lock();
	bool changed = (m_currentSurface.get() != surface.get());

	m_currentSurface = surface;
	m_frameInfo = info;
	if (changed && m_callback != NULL)
		m_callback->OnCameraEvent(CAMERA_CORE_EVENT_SURFACE_UPDATED,
					  (int)m_currentSurface.get(), 0);
	m_mutex.Unlock();
}

int DeviceController::GetDeviceId() const
{
	return m_device != NULL ? m_device->GetDeviceId() : CAMERA_CORE_DEVICE_NONE;
}

int DeviceController::SetDevice(int deviceId)
{
	int curId = GetDeviceId();

	if (deviceId == curId)
		return 1;
	if (m_device != NULL) {
		m_device->RemoveListener(this);
		UpdateFrame(intrusive_ptr<Surface>(), CameraFrameInfo());
	}
	if (deviceId != CAMERA_CORE_DEVICE_NONE) {
		m_device = CameraCoreGetDevice(deviceId);
		if (m_device == NULL)
			return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
		m_device->AddListener(this);
	} else {
		m_device = NULL;
	}
	return 0;
}

int DeviceController::GetCurrentFrame(intrusive_ptr<Surface> *outSurface, CameraFrameInfo *outInfo)
{
	m_mutex.Lock();
	if (outSurface != NULL)
		*outSurface = m_currentSurface;
	if (outInfo != NULL)
		*outInfo = m_frameInfo;
	int ret = (m_currentSurface.get() == NULL) ? 1 : 0;

	m_mutex.Unlock();
	return ret;
}

void DeviceController::OnDeviceEvent(int event, void *arg, int result)
{
	if (event == CAMERA_CORE_EVENT_FRAME_READY) {
		if (arg != NULL && !m_paused) {
			CameraFramePacket *pkt = (CameraFramePacket *)arg;

			UpdateFrame(pkt->surface, pkt->info);
		}
		return;
	}
	if (m_callback != NULL)
		m_callback->OnCameraEvent(event, (int)arg, result);
}

/* ImageEncodeJobItem */
ImageEncodeJobItem::ImageEncodeJobItem(const CameraEncodeInput &input,
				       const CameraEncodeOverlay &overlay,
				       const CameraEncodeOption &option)
	: paf::common::SlotJobItem("SceCameraCoreImageEncodeJobItem", NULL), m_input(input), m_frameInfoCopy(),
	  m_locationCopy(), m_photoMetaCopy(), m_overlay(overlay), m_option(option),
	  m_photoId((uint64_t)-1), m_outputWidth(0), m_outputHeight(0), m_outputOrientation(0),
	  m_result(0), m_encodedSize(0), m_makerNoteCopy()
{
	if (m_input.frameInfo != NULL) {
		m_frameInfoCopy = *m_input.frameInfo;
		m_input.frameInfo = &m_frameInfoCopy;
	}
	if (m_input.locationInfo != NULL) {
		m_locationCopy = *m_input.locationInfo;
		m_input.locationInfo = &m_locationCopy;
	}
	if (m_input.photoMeta != NULL) {
		m_photoMetaCopy = *m_input.photoMeta;
		m_input.photoMeta = &m_photoMetaCopy;
	}
	if (m_input.makerNote != NULL) {
		m_makerNoteCopy = *m_input.makerNote;
		m_input.makerNote = &m_makerNoteCopy;
	}
}

ImageEncodeJobItem::~ImageEncodeJobItem()
{
	m_input.surface = NULL;
	m_overlay.surface = NULL;
}

int ImageEncodeJobItem::Run()
{
	int ret = PictureFileCreatorEncode(this);

	m_input.surface = NULL;
	m_overlay.surface = NULL;
	return ret;
}

void ImageEncodeJobItem::Finish(int result)
{
	m_result = result;
}

/* Global SceCameraCore API */
int CameraCoreInit(CameraCoreListener *errorListener)
{
	if (s_coreManager != NULL)
		return CAMERA_CORE_ERROR_INVALID_STATE;
	s_coreManager = new CameraCoreManager();
	if (s_coreManager == NULL)
		return CAMERA_CORE_ERROR_OUT_OF_MEMORY;
	s_coreManager->m_errorListener = errorListener;

	int devices[2] = { CAMERA_CORE_DEVICE_FRONT, CAMERA_CORE_DEVICE_REAR };
	uint32_t err = 0;

	for (int i = 0; i < 2; ++i) {
		CameraDeviceThread *dev = new CameraDeviceThread();
		uint32_t initErr;

		if (dev != NULL) {
			initErr = (uint32_t)dev->Initialize(devices[i], s_coreManager);
			s_coreManager->m_devices[devices[i]] = dev;
		} else {
			initErr = (uint32_t)CAMERA_CORE_ERROR_OUT_OF_MEMORY;
		}
		if (initErr > err)
			err = initErr;
	}
	if ((int)err >= 0) {
		err = (uint32_t)PictureFileCreatorInit();
		if (err == 0)
			err = (uint32_t)PhotoExportInit();
	}
	if (err != 0) {
		CameraCoreTerm();
		return (int)err;
	}
	return 0;
}

int CameraCoreTerm()
{
	if (s_coreManager == NULL)
		return CAMERA_CORE_ERROR_NOT_INITIALIZED;
	uint32_t err = 0;

	if (PictureFileCreatorTerm() != 0)
		err = (uint32_t)CAMERA_CORE_ERROR_GENERIC;
	PhotoExportTerm();
	for (map<int, CameraDeviceThread *>::iterator it = s_coreManager->m_devices.begin();
	     it != s_coreManager->m_devices.end(); ++it) {
		if (it->second != NULL) {
			uint32_t termErr = (uint32_t)it->second->Terminate();

			if (termErr > err)
				err = termErr;
			delete it->second;
		}
	}
	delete s_coreManager;
	s_coreManager = NULL;
	return (int)err;
}

CameraDeviceThread *CameraCoreGetDevice(int deviceId)
{
	if (s_coreManager == NULL)
		return NULL;
	map<int, CameraDeviceThread *>::iterator it = s_coreManager->m_devices.find(deviceId);

	return (it != s_coreManager->m_devices.end()) ? it->second : NULL;
}

int CameraCoreGetConfig(int deviceId, int *outRes, int *outFps, int *outFmt)
{
	if (s_coreManager == NULL)
		return CAMERA_CORE_ERROR_NOT_INITIALIZED;
	CameraDeviceThread *dev = CameraCoreGetDevice(deviceId);

	return (dev != NULL) ? dev->GetConfig(outRes, outFps, outFmt)
			     : CAMERA_CORE_ERROR_INVALID_ARGUMENT;
}

int CameraCoreSetConfig(int deviceId, int resolutionPreset, int framerateType, int formatType)
{
	if (s_coreManager == NULL)
		return CAMERA_CORE_ERROR_NOT_INITIALIZED;
	CameraDeviceThread *dev = CameraCoreGetDevice(deviceId);

	return (dev != NULL) ? dev->SetConfig(resolutionPreset, framerateType, formatType)
			     : CAMERA_CORE_ERROR_INVALID_ARGUMENT;
}

int CameraCoreSetProperty(int deviceId, uint32_t propId, int value)
{
	if (s_coreManager == NULL)
		return CAMERA_CORE_ERROR_NOT_INITIALIZED;
	CameraDeviceThread *dev = CameraCoreGetDevice(deviceId);

	return (dev != NULL) ? dev->SetProperty(propId, value)
			     : CAMERA_CORE_ERROR_INVALID_ARGUMENT;
}

int CameraCoreSetSurfacePool(int deviceId, SurfaceInstancePool *pool)
{
	if (s_coreManager == NULL)
		return CAMERA_CORE_ERROR_NOT_INITIALIZED;
	CameraDeviceThread *dev = CameraCoreGetDevice(deviceId);

	return (dev != NULL) ? dev->SetSurfacePool(pool) : CAMERA_CORE_ERROR_INVALID_ARGUMENT;
}

ImageEncodeJobItem *CameraCoreCreateEncodeJob(const CameraEncodeInput &input,
					      const CameraEncodeOverlay &overlay,
					      const CameraEncodeOption &option)
{
	if (s_coreManager == NULL)
		return NULL;
	return new ImageEncodeJobItem(input, overlay, option);
}
