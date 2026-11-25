/*
 * SceCamImportDialogPlugin: SceCameraCore subsystem (SurfaceInstancePool, CameraIf,
 * MainThreadCaller, CameraDeviceThread, DeviceController, CameraCoreManager,
 * and ImageEncodeJobItem).
 */
#ifndef CAMERAIMPORT_DIALOG_CAMERA_CORE_H
#define CAMERAIMPORT_DIALOG_CAMERA_CORE_H

#include <stdint.h>
#include <scetypes.h>
#include <camera.h>
#include <std/list>
#include <std/map>
#include <std/string>
#include <std/memory>
#include <job/job.h>
#include <common/job_slots.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <thread/thread.h>

/* SceCameraCore error codes */
#define CAMERA_CORE_ERROR_GENERIC		((int)0x80103E01)
#define CAMERA_CORE_ERROR_INVALID_ARGUMENT	((int)0x80103E02)
#define CAMERA_CORE_ERROR_INVALID_STATE		((int)0x80103E03)
#define CAMERA_CORE_ERROR_NOT_INITIALIZED	((int)0x80103E08)
#define CAMERA_CORE_ERROR_OUT_OF_MEMORY		((int)0x80103E0B)
#define CAMERA_CORE_ERROR_NOT_STABLE		((int)0x80103E0C)
#define CAMERA_CORE_ERROR_READ_ERROR		((int)0x80103E0D)
#define CAMERA_CORE_ERROR_FORCED_STOP		((int)0x80103E0E)
#define CAMERA_CORE_ERROR_FULL			((int)0x80103E0F)
#define CAMERA_CORE_ERROR_EXPORT_NO_SPACE	((int)0x80103E7F)

/* SceCameraCore device IDs */
enum {
	CAMERA_CORE_DEVICE_NONE = 0,
	CAMERA_CORE_DEVICE_FRONT = 1,
	CAMERA_CORE_DEVICE_REAR = 2
};

/* SceCameraCore resolution presets */
enum {
	CAMERA_CORE_RES_NONE = 0,
	CAMERA_CORE_RES_640X480 = 1,
	CAMERA_CORE_RES_320X240 = 2,
	CAMERA_CORE_RES_640X360 = 3,
	CAMERA_CORE_RES_320X180 = 4,
	CAMERA_CORE_RES_480X272 = 5,
	CAMERA_CORE_RES_480X270 = 6,
	CAMERA_CORE_RES_480X480 = 7
};

/* SceCameraCore camera property indices */
enum {
	CAMERA_CORE_PROP_ISO = 0,
	CAMERA_CORE_PROP_NIGHTMODE = 1,
	CAMERA_CORE_PROP_EFFECT = 2,
	CAMERA_CORE_PROP_EV = 3,
	CAMERA_CORE_PROP_WHITE_BALANCE = 4,
	CAMERA_CORE_PROP_SATURATION = 5,
	CAMERA_CORE_PROP_BACKLIGHT = 6,
	CAMERA_CORE_PROP_COUNT = 7
};

/* SceCameraCore events */
#define CAMERA_CORE_EVENT_RESOLUTION_CHANGING	0x1001
#define CAMERA_CORE_EVENT_RESOLUTION_CHANGED	0x1002
#define CAMERA_CORE_EVENT_PROPERTY_CHANGING	0x1003
#define CAMERA_CORE_EVENT_PROPERTY_CHANGED	0x1004
#define CAMERA_CORE_EVENT_SURFACE_UPDATED	0x1005
#define CAMERA_CORE_EVENT_FRAME_READY		0x8001
#define CAMERA_CORE_EVENT_DEVICE_ERROR		((int)0x80001001)

/* Unset property sentinel */
#define CAMERA_CORE_UNSET_VALUE			(-9999)
#define CAMERA_CORE_UNSET_COORD			(-9999.0)
#define CAMERA_CORE_UNSET_FLOAT			(-9999.0f)

/* Camera frame metadata (48 bytes) */
struct CameraFrameInfo {
	CameraFrameInfo()
		: deviceId(0), timestamp(0), framerate(0), format(0), cropY(0), cropX(0),
		  cropWidth(0), cropHeight(0), iso(CAMERA_CORE_UNSET_VALUE),
		  nightmode(CAMERA_CORE_UNSET_VALUE)
	{
	}

	int deviceId;
	uint64_t timestamp;
	int framerate;
	int format;
	int cropY;
	int cropX;
	int cropWidth;
	int cropHeight;
	int iso;
	int nightmode;
};

/* Frame packet passed to device listeners */
struct CameraFramePacket {
	paf::intrusive_ptr<paf::graph::Surface> surface;
	CameraFrameInfo info;
};

/* GPS & orientation metadata (56 bytes) */
struct CameraLocationInfo {
	CameraLocationInfo()
		: latitude(CAMERA_CORE_UNSET_COORD), longitude(CAMERA_CORE_UNSET_COORD),
		  altitude(CAMERA_CORE_UNSET_COORD), horizontalAccuracy(CAMERA_CORE_UNSET_FLOAT),
		  verticalAccuracy(CAMERA_CORE_UNSET_FLOAT),
		  headingAccuracy(CAMERA_CORE_UNSET_FLOAT), utcTimestamp(0),
		  heading(CAMERA_CORE_UNSET_FLOAT), exifOrientation(0)
	{
	}

	double latitude;
	double longitude;
	double altitude;
	float horizontalAccuracy;
	float verticalAccuracy;
	float headingAccuracy;
	uint64_t utcTimestamp;
	float heading;
	int exifOrientation;
};

/* EXIF device/make/model/software strings (40 bytes) */
struct CameraPhotoMeta {
	CameraPhotoMeta() : isSelfie(false) {}

	bool isSelfie;
	paf::string make;
	paf::string model;
	paf::string software;
};

/* Optional string or rational MakerNote tag */
struct CameraMakerNoteStringTag {
	CameraMakerNoteStringTag() : present(false) {}

	paf::string value;
	bool present;
};

struct CameraMakerNoteRationalTag {
	CameraMakerNoteRationalTag() : numerator(0), denominator(0), present(false) {}

	uint32_t numerator;
	uint32_t denominator;
	bool present;
};

/* PlayStation MakerNote metadata */
struct CameraMakerNoteParam {
	CameraMakerNoteStringTag album;
	CameraMakerNoteStringTag gameTitle;
	CameraMakerNoteStringTag gameDescription;
	CameraMakerNoteStringTag gameContentId;
	CameraMakerNoteStringTag comment;
	CameraMakerNoteStringTag effectVersion;
	CameraMakerNoteStringTag frameId;
	CameraMakerNoteRationalTag altitudeAccuracy;
	CameraMakerNoteRationalTag directionAccuracy;
	CameraMakerNoteRationalTag locationAccuracy;
	CameraMakerNoteRationalTag yawPitchRoll;
};

/* Input image description for ImageEncodeJobItem */
struct CameraEncodeInput {
	CameraEncodeInput()
		: frameInfo(NULL), locationInfo(NULL), photoMeta(NULL), makerNote(NULL)
	{
	}

	paf::intrusive_ptr<paf::graph::Surface> surface;
	CameraFrameInfo *frameInfo;
	CameraLocationInfo *locationInfo;
	CameraPhotoMeta *photoMeta;
	CameraMakerNoteParam *makerNote;
};

/* Overlay image description for ImageEncodeJobItem */
struct CameraEncodeOverlay {
	CameraEncodeOverlay() : offsetX(0), offsetY(0), width(0), height(0) {}

	paf::intrusive_ptr<paf::graph::Surface> surface;
	int offsetX;
	int offsetY;
	int width;
	int height;
};

/* Output options for ImageEncodeJobItem */
struct CameraEncodeOption {
	CameraEncodeOption()
		: format(0), destination(0), filePath(NULL), registerToPhotoDb(false),
		  outputBuffer(NULL), outputBufferSize(0), jpegCompressionRatio(64),
		  overlayAlpha(0xFF)
	{
	}

	int format;
	int destination;
	const char *filePath;
	bool registerToPhotoDb;
	void *outputBuffer;
	uint32_t outputBufferSize;
	uint8_t jpegCompressionRatio;
	uint8_t overlayAlpha;
};

/* Listener interface for camera core notifications */
class CameraCoreListener {
public:
	virtual ~CameraCoreListener() {}
	virtual void OnCameraEvent(int event, int arg, int result) = 0;
};

/* Pool of reusable PAF graphics surfaces for camera frames */
class SurfaceInstancePool {
public:
	SurfaceInstancePool();
	~SurfaceInstancePool();

	int Initialize(paf::graph::SurfacePool *pool, uint32_t count, ImageMode format);
	int Terminate();
	void ReallocateSurfaces(int width, int height);
	paf::intrusive_ptr<paf::graph::Surface> AcquireSurface(int width, int height);

	bool m_initialized;
	bool m_ownsPool;
	void *m_poolMemory;
	paf::graph::SurfacePool *m_surfacePool;
	paf::intrusive_ptr<paf::graph::Surface> *m_surfaces;
	uint32_t m_surfaceCount;
	ImageMode m_format;
	int m_width;
	int m_height;
	paf::thread::Mutex m_mutex;
};

/* Low-level SceCamera wrapper */
class CameraIf {
public:
	CameraIf();
	~CameraIf();

	int Initialize();
	int Close();
	int Stop();
	int Terminate();
	int Open(int deviceId, int resolutionPreset, int framerateType, int formatType);
	int Start();
	int GetPropertyCached(uint32_t propId, int *outValue);
	int QueryProperty(int propId, int *outValue);
	int SetProperty(int propId, int value);
	int GetResolution(int *outWidth, int *outHeight);
	int ReadFrame(paf::intrusive_ptr<paf::graph::Surface> *surface, CameraFrameInfo *outInfo);

	static void FillCameraInfo(SceCameraInfo *info, int width, int height, int framerateType,
				   int formatType);
	static int ResolvePreset(int preset, int *outWidth, int *outHeight, int outCropRect[4]);

	int m_state;
	paf::thread::RMutex m_mutex;
	int m_framerateType;
	int m_deviceId;
	int m_width;
	int m_height;
	int m_formatType;
	int m_cropRect[4];
	SceCameraInfo m_cameraInfo;
	int m_hwDevice;
	int m_properties[CAMERA_CORE_PROP_COUNT];
};

/* Synchronous marshaller onto the PAF main thread */
class MainThreadCaller {
public:
	MainThreadCaller();
	~MainThreadCaller();

	void Invoke(CameraCoreListener *listener, int event, int arg, int result);
	static void CallbackEntry(void *userData);

	paf::thread::Mutex m_mutex;
	paf::thread::Cond m_cond;
	CameraCoreListener *m_listener;
	int m_event;
	int m_arg;
	int m_result;
};

/* Listener list helper for CameraDeviceThread */
class CameraDeviceListener {
public:
	virtual ~CameraDeviceListener() {}
	virtual void OnDeviceEvent(int event, void *arg, int result) = 0;
};

/* Background camera capture thread */
class CameraDeviceThread : public paf::thread::Thread {
public:
	CameraDeviceThread();
	virtual ~CameraDeviceThread();
	virtual void EntryFunction();

	int GetDeviceId() const { return m_deviceId; }
	int Initialize(int deviceId, CameraCoreListener *errorListener);
	int Terminate();
	int GetConfig(int *outRes, int *outFps, int *outFmt);
	int SetConfig(int resolutionPreset, int framerateType, int formatType);
	int SetProperty(uint32_t propId, int value);
	int SetSurfacePool(SurfaceInstancePool *pool);
	void AddListener(CameraDeviceListener *listener);
	void RemoveListener(CameraDeviceListener *listener);

private:
	int OpenAndStart(int resolutionPreset, int framerateType, int formatType);
	int CaptureFrame(CameraFramePacket *packet);
	void DispatchEvent(int event, void *arg, int result);

	bool m_initialized;
	SurfaceInstancePool *m_surfacePool;
	paf::list<CameraDeviceListener *> m_listeners;
	int m_deviceId;
	int m_resolutionPreset;
	int m_framerateType;
	int m_formatType;
	int m_appliedProps[CAMERA_CORE_PROP_COUNT];
	int m_requestedProps[CAMERA_CORE_PROP_COUNT];
	bool m_propsDirty;
	CameraIf *m_cameraIf;
	paf::thread::RMutex m_propertyMutex;
	paf::thread::Mutex m_loopMutex;
	paf::thread::Cond m_loopCond;
	bool m_needReopen;
	MainThreadCaller m_mainThreadCaller;
	CameraCoreListener *m_errorListener;
};

/* Per-widget camera frame controller */
class DeviceController : public CameraDeviceListener {
public:
	DeviceController(CameraCoreListener *callback);
	virtual ~DeviceController();
	virtual void OnDeviceEvent(int event, void *arg, int result);

	int GetDeviceId() const;
	int SetDevice(int deviceId);
	int GetCurrentFrame(paf::intrusive_ptr<paf::graph::Surface> *outSurface,
			    CameraFrameInfo *outInfo);

private:
	void UpdateFrame(const paf::intrusive_ptr<paf::graph::Surface> &surface,
			 const CameraFrameInfo &info);

	paf::thread::RMutex m_mutex;
	paf::intrusive_ptr<paf::graph::Surface> m_currentSurface;
	CameraFrameInfo m_frameInfo;
	CameraDeviceThread *m_device;
	bool m_paused;
	CameraCoreListener *m_callback;
};

/* Async image encoding job item ("SceCameraCoreImageEncodeJobItem") */
class ImageEncodeJobItem : public paf::common::SlotJobItem {
public:
	ImageEncodeJobItem(const CameraEncodeInput &input, const CameraEncodeOverlay &overlay,
			   const CameraEncodeOption &option);
	virtual ~ImageEncodeJobItem();
	virtual int Run();
	virtual void Finish(int result);
	virtual uint64_t GetPhotoId() const { return m_photoId; }
	virtual int GetResult() const { return m_result; }
	virtual uint32_t GetEncodedSize() const { return m_encodedSize; }

	CameraEncodeInput m_input;
	CameraFrameInfo m_frameInfoCopy;
	CameraLocationInfo m_locationCopy;
	CameraPhotoMeta m_photoMetaCopy;
	CameraEncodeOverlay m_overlay;
	CameraEncodeOption m_option;
	uint64_t m_photoId;
	int m_outputWidth;
	int m_outputHeight;
	int m_outputOrientation;
	int m_result;
	uint32_t m_encodedSize;
	CameraMakerNoteParam m_makerNoteCopy;
};

/* Global SceCameraCore API */
int CameraCoreInit(CameraCoreListener *errorListener);
int CameraCoreTerm();
int CameraCoreGetConfig(int deviceId, int *outRes, int *outFps, int *outFmt);
int CameraCoreSetConfig(int deviceId, int resolutionPreset, int framerateType, int formatType);
int CameraCoreSetProperty(int deviceId, uint32_t propId, int value);
int CameraCoreSetSurfacePool(int deviceId, SurfaceInstancePool *pool);
CameraDeviceThread *CameraCoreGetDevice(int deviceId);
ImageEncodeJobItem *CameraCoreCreateEncodeJob(const CameraEncodeInput &input,
					      const CameraEncodeOverlay &overlay,
					      const CameraEncodeOption &option);

#endif /* CAMERAIMPORT_DIALOG_CAMERA_CORE_H */
