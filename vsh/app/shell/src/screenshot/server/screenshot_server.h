/*
 * screenshot_server.h: SceShell SceScreenShot IPMI server, capture pipeline, and
 * PictureFileCreator EXIF/MakerNote structures.
 * 3.65-CEX: 0x81244AF4 - 0x81248A0F
 */
#ifndef SHELL_SCREENSHOT_SERVER_H
#define SHELL_SCREENSHOT_SERVER_H

#include <stddef.h>
#include <scetypes.h>
#include <screenshot.h>
#include <std/string>
#include <std/memory>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <rtc/rtc.h>

#include "shell.h"

/*
 * PictureFileCreator interface (implemented in shell at 0x81365BFC - 0x8136695F).
 * Handles JPEG/BMP picture encoding with EXIF 2.21 APP1 metadata and Sony MakerNote.
 */
struct PictureExifRational {
	SceUInt32 numerator;
	SceUInt32 denominator;
};

struct PictureMakerNoteGeoPair {
	SceInt32 first;
	SceInt32 second;
};

template <typename ValueType>
struct PictureExifField {
	ValueType value;
	bool present;

	PictureExifField() : present(false) {}

	PictureExifField &operator=(const PictureExifField &other)
	{
		if (this != &other) {
			value = other.value;
			present = other.present;
		}
		return *this;
	}

	void Set(const ValueType &newValue)
	{
		value = newValue;
		present = true;
	}

	void Clear()
	{
		present = false;
	}
};

struct PictureMakerNote {
	PictureExifField<paf::string> formatVersion;		/* PEFV: "1.0" */
	PictureExifField<paf::string> gameContentId;		/* GCID */
	PictureExifField<paf::string> gameProductName;		/* GPNM */
	PictureExifField<paf::string> gameDescription;		/* GDSC */
	PictureExifField<paf::string> albumTitle;		/* ALBM */
	PictureExifField<paf::string> photoSourceInfo;		/* PSIS */
	PictureExifField<paf::string> photoType;		/* PTYP */
	PictureExifField<PictureMakerNoteGeoPair> locationAccuracy;	/* LLAC */
	PictureExifField<PictureMakerNoteGeoPair> panHorizontalRate;	/* PHFR */
	PictureExifField<PictureMakerNoteGeoPair> panVerticalRate;	/* PVFR */
	PictureExifField<PictureMakerNoteGeoPair> panYawPitchRoll;	/* PYPR */
};

struct PictureExifData {
	PictureExifField<SceUInt16> compression;		/* Tag 259 (0x0103) */
	PictureExifField<paf::string> make;			/* Tag 271 (0x010F) */
	PictureExifField<paf::string> model;			/* Tag 272 (0x0110) */
	PictureExifField<SceUInt16> orientation;		/* Tag 274 (0x0112) */
	PictureExifField<PictureExifRational> xResolution;	/* Tag 282 (0x011A) */
	PictureExifField<PictureExifRational> yResolution;	/* Tag 283 (0x011B) */
	PictureExifField<SceUInt16> resolutionUnit;		/* Tag 296 (0x0128) */
	PictureExifField<paf::string> software;			/* Tag 305 (0x0131) */
	PictureExifField<paf::string> dateTime;			/* Tag 306 (0x0132) */
	PictureExifField<SceUInt16> yCbCrPositioning;		/* Tag 531 (0x0213) */
	PictureExifField<char[4]> exifVersion;			/* Tag 36864 (0x9000) */
	PictureExifField<paf::string> dateTimeOriginal;		/* Tag 36867 (0x9003) */
	PictureExifField<paf::string> dateTimeDigitized;	/* Tag 36868 (0x9004) */
	PictureExifField<SceUInt8[4]> componentsConfiguration;	/* Tag 37121 (0x9101) */
	PictureExifField<SceUInt16> flash;			/* Tag 37385 (0x9209) */
	PictureExifField<char[4]> flashpixVersion;		/* Tag 40960 (0xA000) */
	PictureExifField<SceUInt16> colorSpace;			/* Tag 40961 (0xA001) */
	PictureExifField<SceUInt32> pixelXDimension;		/* Tag 40962 (0xA002) */
	PictureExifField<SceUInt32> pixelYDimension;		/* Tag 40963 (0xA003) */
	PictureExifField<SceUInt16> customRendered;		/* Tag 41985 (0xA401) */
	PictureExifField<SceUInt16> exposureMode;		/* Tag 41986 (0xA402) */
	PictureExifField<SceUInt16> whiteBalance;		/* Tag 41987 (0xA403) */
	PictureExifField<SceUInt16> sceneCaptureType;		/* Tag 41990 (0xA406) */
	PictureExifField<paf::string> interopIndex;		/* Tag 1 (0x0001) */
	PictureExifField<char[4]> interopVersion;		/* Tag 2 (0x0002) */
	PictureMakerNote *makerNote;
	void *gpsData;
};

struct PictureSourceInfo {
	void *pixelBuffer;
	SceUInt32 width;
	SceUInt32 height;
	SceUInt32 pitchBytes;
	SceUInt32 cropX;
	SceUInt32 cropY;
	SceUInt32 cropWidth;
	SceUInt32 cropHeight;
	SceInt32 pixelFormat;
	SceUInt32 reserved;
};

struct PictureEncodeConfig {
	const PictureSourceInfo *source;
	const PictureSourceInfo *overlay;
	SceInt32 outputFormat;		/* 0 = JPEG, 1 = BMP, 2 = RAW */
	SceUInt32 outputFlags;		/* bit 0 = file output, bit 1 = external thumb temp file */
	SceUInt8 overlayAlpha;
	SceUInt8 jpegCompressionRatio;
	SceUInt8 padBytes[2];
	float scaleX;
	float scaleY;
};

struct PictureWorkSize {
	SceSize outputBufferSize;
	SceSize workMemorySize;
};

struct PictureCreateInput {
	const PictureEncodeConfig *config;
	const PictureExifData *exif;
	int (*cancelFunc)(void *);
	void *cancelUserData;
};

struct PictureCreateOutput {
	void *outputBuffer;
	SceSize outputBufferSize;
	void *workMemory;
	SceSize workMemorySize;
	const char *outputFilePath;
	SceUInt32 ioBufferSize;
	const char *thumbTempPath;
	SceUInt32 thumbIoBufferSize;
};

int pictureFileCreatorInit(void);				/* 0x81365BFC */
int pictureFileCreatorGetWorkSize(const PictureEncodeConfig *config,
				  PictureWorkSize *outSize);	/* 0x81366060 */
int pictureFileCreatorCreate(const PictureCreateInput *input,
			     PictureCreateOutput *output);	/* 0x8136638A */
int pictureExifInitDefaults(PictureExifData *exif);		/* 0x81366678 */
int pictureMakerNoteInit(PictureMakerNote *makerNote);		/* 0x813667FC */
int pictureExifSetDateTime(PictureExifData *exif,
			   paf::rtc::Tick tick);		/* 0x81366876 */
int shellShutterSoundPlay(SceUInt32 soundType, int useCameraVol); /* 0x8124448E */

extern "C" paf::memory::HeapAllocator *GetGlobalHeapAllocator(void);

namespace screenshot {

static const SceUInt32 kScreenWidth = 960;
static const SceUInt32 kScreenHeight = 544;
static const SceUInt32 kScreenStrideBytes = 960 * 4;
static const SceSize kFrameBufferSize = 960 * 544 * 4;
static const SceUInt32 kMaxOverlayWidth = 480;
static const SceUInt32 kMaxOverlayHeight = 272;
static const SceSize kOverlayChunkSize = 0x4000;
static const SceUInt32 kMaxAppEntries = 50;
static const SceUInt32 kMaxIpmiSessions = 10;

static const int kScreenShotErrAlreadyInit = -2146422879;	/* 0x80102FA1 */
static const int kScreenShotErrNotInit = -2146422878;		/* 0x80102FA2 */
static const int kScreenShotErrIpmiConnect = -2146422877;	/* 0x80102FA3 */
static const int kScreenShotErrDisabled = -2146422874;		/* 0x80102FA6 */
static const int kScreenShotErrNoMemory = -2146422873;		/* 0x80102FA7 */
static const int kScreenShotErrShortWrite = -2146422870;	/* 0x80102FAA */
static const int kScreenShotErrCopyDone = -2146422869;		/* 0x80102FAB */
static const int kIpmiErrUnsupported = -2147351103;		/* 0x800205C1 */
static const int kIpmiErrSessionFull = -2147351284;		/* 0x8002050C */
static const int kAppMgrErrNoForeground = -2139086830;		/* 0x80802012 */
static const int kAppMgrErrCaptureBlocked = -2139086807;	/* 0x80802029 */
static const int kPhotoExportErrContentFull = -2146428413;	/* 0x80101A03 */
static const int kPhotoExportErrMediaFull = -2146428409;	/* 0x80101A07 */
static const int kPhotoExportErrMediaNotExist = -2146428403;	/* 0x80101A0D */
static const int kPhotoExportErrUnsupported = -2146428402;	/* 0x80101A0E */
static const int kPhotoExportErrInvalidParam = -2146428411;	/* 0x80101A05 */

typedef void (*CaptureCompleteFn)(int *result, void *userData);
typedef void (*CaptureRequestFn)(void *userData);
typedef void (*CaptureJobDoneFn)(int result, SceInt32 appId, const char *exportedPath,
				 void *userData);

struct SurfaceDesc {
	SceUInt32 width;
	SceUInt32 height;
	void *pixels;
	SceUInt32 pitchBytes;
	SceUInt32 pixelPitch;
};

struct FrameBufferInfo {
	void *pixels;
	SceUInt32 width;
	SceUInt32 height;
	SceUInt32 pitchBytes;
};

struct AppTitleInfo {
	paf::string titleName;
	paf::string titleId;
	paf::string contentId;
};

struct AppEntry {
	SceInt32 appId;
	SceUID processId;
	bool captureEnabled;
	bool reservedState;
	bool overlayEnabled;
	bool notificationEnabled;
	paf::string overlayPath;
	paf::string photoTitle;
	paf::string gameTitle;
	paf::string gameComment;
	SceInt32 overlayOffsetX;
	SceInt32 overlayOffsetY;
	SceUID overlayFileFd;

	AppEntry();
	void Reset(SceInt32 newAppId, SceUID newProcessId);
};

struct PendingIpcCapture {
	SceInt32 appId;
	SceUInt32 captureMode;
	IPMI::Session *session;
	SceUInt32 requestId;
	bool appExited;
	bool canceled;

	PendingIpcCapture();
	void Clear();
};

struct InitParam {
	paf::memory::HeapAllocator *allocator;
	SceInt32 threadPriority;
	SceSize threadStackSize;
	SceInt32 cpuAffinityMask;
	paf::job::JobQueue *jobQueue;
};

void setAllocator(paf::memory::HeapAllocator *allocator);	/* 0x81246F78 */
void *allocMemory(SceSize size);				/* 0x81246F86 */
void *allocAligned(SceUInt32 align, SceSize size);		/* 0x81246FA2 */
void freeMemory(void *ptr);					/* 0x81246FC0 */
int queryAppTitleInfo(SceInt32 appId, AppTitleInfo *outInfo);	/* 0x81246FD8 */
int blendSurfaceRGBA(SurfaceDesc *dst, const SurfaceDesc *src);	/* 0x81246A78 */
int resizeSurfaceRGBA(SurfaceDesc *surface, SceUInt32 dstWidth,
		      SceUInt32 dstHeight);			/* 0x81246BB4 */

class LockSwapJob : public paf::job::JobItem {
public:
	LockSwapJob();
	virtual ~LockSwapJob();
	virtual int Run();
	virtual void Finish(int result);
};

class ResizeImageJob : public paf::job::JobItem {
public:
	ResizeImageJob(const SurfaceDesc &src, const SurfaceDesc &dst);
	virtual ~ResizeImageJob();
	virtual int Run();
	virtual void Finish(int result);

private:
	SurfaceDesc m_src;
	SurfaceDesc m_dst;
};

class BlendImageJob : public paf::job::JobItem {
public:
	BlendImageJob(const SurfaceDesc &dst, const SurfaceDesc &src, bool lockDisplaySwap);
	virtual ~BlendImageJob();
	virtual int Run();
	virtual void Finish(int result);

private:
	SurfaceDesc m_dst;
	SurfaceDesc m_src;
	bool m_lockedDisplaySwap;
};

class CaptureJob : public paf::job::JobItem {
public:
	CaptureJob(const AppEntry &entry, SceUID processId, const FrameBufferInfo &frameBuf,
		   bool debugMode, char *exportedPathBuf, CaptureJobDoneFn onDone,
		   void *onDoneUserData);
	virtual ~CaptureJob();
	virtual int Run();
	virtual void Finish(int result);

	static void *operator new(size_t size);
	static void operator delete(void *ptr);

private:
	int CopyFile(const char *srcPath, const char *dstPath);
	int ExportToPhoto(const char *capturePath, paf::rtc::Tick tick);
	int EncodePictureFile(const char *capturePath, paf::rtc::Tick tick);
	int BlendOverlayImage(const char *overlayPath);
	int PopulateAppInfo();

	AppEntry m_entry;
	SceUID m_processId;
	bool m_debugMode;
	SceInt32 m_outputFormat;
	paf::string m_titleId;
	paf::string m_contentId;
	FrameBufferInfo m_frameBuf;
	CaptureJobDoneFn m_onDone;
	void *m_onDoneUserData;
	char *m_exportedPathBuf;
};

class ScreenShotIpcServer : public IPMI::Server::EventHandler {
public:
	ScreenShotIpcServer();
	virtual ~ScreenShotIpcServer();

	int Create();
	int RunDispatcher();
	int SendCaptureResponse(IPMI::Session *session, SceUInt32 requestId, int result,
				const char *exportedPath);

	virtual void onSyncMethodDispatch(IPMI::Session *session, SceUInt32 methodId,
					  const IPMI::DataInfo *inList, SceUInt32 inCount,
					  IPMI::BufferInfo *outList, SceUInt32 outCount);
	virtual void onAsyncMethodDispatch(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *inList,
					   SceUInt32 inCount);
	virtual int onNewClient(IPMI::Server *server, const IPMI::Session::Config *config,
				const void *param, SceSize paramSize);
	virtual void onSessionClosedByClient(IPMI::Session *session);
	virtual void onSessionKilled(IPMI::Session *session);

	static void *operator new(size_t size);
	static void operator delete(void *ptr);

private:
	IPMI::Server *m_server;
	IPMI::Session *m_sessions[kMaxIpmiSessions];
	SceUInt32 m_sessionCount;
	void *m_workBuffer;
	SceSize m_workBufferSize;
};

class ScreenShotManager {
public:
	explicit ScreenShotManager(const InitParam &param);
	void Start();

	int AllocateSurface(SceUInt32 width, SceUInt32 height);
	int CaptureOverlayAndComposite(SceInt32 appId, SceUInt32 *inOutWidth,
				       SceUInt32 *inOutHeight);
	int StartCapture(CaptureCompleteFn callback, void *userData);
	int CaptureToSurface(paf::intrusive_ptr<paf::graph::Surface> *outSurface, SceInt32 appId);
	int OnAppEvent(SceUID processId, SceInt32 appId, int eventType);

	int EnableCapture(SceInt32 appId);
	int DisableCapture(SceInt32 appId);
	int SetOverlayOffset(SceInt32 appId, SceInt32 offsetX, SceInt32 offsetY);
	int SetParam(SceInt32 appId, const char *photoTitle, const char *gameTitle,
		     const char *gameComment, SceUInt8 legacyLimitMode);
	int ReceiveOverlayData(SceInt32 appId, const void *chunkData, SceSize chunkSize,
			       SceInt32 chunkIndex);
	int EnableOverlay(SceInt32 appId);
	int DisableOverlay(SceInt32 appId);
	int EnableNotification(SceInt32 appId);
	int DisableNotification(SceInt32 appId);
	int RequestIpcCapture(IPMI::Session *session, SceUInt32 requestId, SceUInt32 captureMode);
	int SetRequestCallback(CaptureRequestFn callback, void *userData);
	int CancelIpcCapture(SceInt32 appId);

	void SendNotification(SceInt32 appId, const char *exportedPath);
	void OnCaptureJobFinished(int result, SceInt32 appId, const char *exportedPath);

	static void *operator new(size_t size);
	static void operator delete(void *ptr);

private:
	AppEntry *FindEntry(SceInt32 appId);
	void ResetPendingIpcCapture(int errorResult);

	SceUID m_serverThreadId;
	paf::memory::HeapAllocator *m_allocator;
	SceInt32 m_threadPriority;
	SceSize m_threadStackSize;
	SceInt32 m_cpuAffinityMask;
	paf::job::JobQueue *m_jobQueue;
	AppEntry m_shellEntry;
	AppEntry m_appEntries[kMaxAppEntries];
	void *m_frameBuffer;
	SceSize m_frameBufferSize;
	SceUInt32 m_captureFlags;
	paf::common::SharedPtr<paf::job::JobItem> m_activeJob;
	paf::intrusive_ptr<paf::graph::Surface> m_captureSurface;
	CaptureCompleteFn m_completeCallback;
	void *m_completeUserData;
	bool m_allowAlways;
	bool m_mediaMounted;
	CaptureRequestFn m_requestCallback;
	void *m_requestUserData;
	PendingIpcCapture m_pendingIpc;
	char m_exportedPath[SCE_SCREENSHOT_MAX_FS_PATH];
};

ScreenShotManager *getManager(void);
ScreenShotIpcServer *getIpcServer(void);

} /* namespace screenshot */

int shellScreenshotStart(void);					/* 0x81244AF4 */

#endif /* SHELL_SCREENSHOT_SERVER_H */
