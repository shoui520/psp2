/*
 * location_internal.h: shared internal interfaces, structs, and classes for SceShell's
 * liblocation service (vsh/app/shell/src/liblocation/src/location_service/).
 */
#ifndef LOCATION_INTERNAL_H
#define LOCATION_INTERNAL_H

#include <stddef.h>
#include <stdarg.h>
#include <scetypes.h>
#include <liblocation.h>
#include <rtc.h>
#include <motion.h>
#include <kernel/threadmgr.h>
#include <gps/gps.h>
#include <std/vector>
#include <memory/heap_allocator.h>
#include <timer/timer.h>
#include <timer/timerlistener.h>
#include "location_service.h"

class LocationTimerMgr;
class LocationLogger;
class LocationSetting;
class LocationPrivacySettings;
class LocationIntervalCtrl;
class LocationSusWatcher;
class LocationScanMode;
class ClientManager;
class HeadingManager;
class PositionManager;
class LocationConfirmMgr;
class LocationXtra;
class DriverController;
class DriverManager;
class GpsCtrlManager;
class ProviderGpsController;
class FlyingStartController;
class QualCommAdapter;
class LocationLogWriter;
class LocationLogParserList;

/* globals shared across liblocation (0x815496B0-0x815496C0) */
extern LocationTimerMgr *g_locationManager;
extern LocationLogger *g_locationLogger;
extern LocationXtra *g_locationXtra;
extern LocationSetting *g_locationConfig;

/*
 * Base class for periodic tasks scheduled on LocationTimerMgr (location_timer.cpp).
 */
class LocationTask {
public:
	virtual ~LocationTask()
	{
	}

	virtual int OnNotify() = 0;

	int Schedule(unsigned int delayMs);
	int Cancel();

	LocationTimerMgr *m_mgr;
	int m_slotIndex;
};

class LocationTimerObserver {
public:
	virtual ~LocationTimerObserver()
	{
	}

	virtual void OnResume() = 0;
	virtual void OnSuspend() = 0;
};

struct LocationTimerSlot {
	LocationTask *task;
	SceUInt64 interval;
	SceUInt64 startTime;
	int state;
};

class LocationTimerListener : public paf::TimerListener {
public:
	LocationTimerListener(paf::Timer *timer, LocationTimerMgr *mgr)
		: paf::TimerListener(timer, NULL), m_mgr(mgr)
	{
	}

	virtual ~LocationTimerListener()
	{
	}

	virtual void OnTick(void *userArg, void *extraArg);
	virtual void OnFinish(void *userArg, void *extraArg);

	LocationTimerMgr *m_mgr;
};

class LocationTimerMgr {
public:
	LocationTimerMgr(paf::Timer *timer);
	~LocationTimerMgr();

	int Unlock();
	int Lock();
	int Term();
	int Init();
	int RegisterTask(LocationTask *task);
	int UnregisterTask(LocationTask *task);
	int ReleaseActive();
	int AcquireActive();
	int RegisterObserver(LocationTimerObserver *observer);
	int UnregisterObserver(LocationTimerObserver *observer);
	void CheckTasks();

	static int ThreadEntry(SceSize args, void *argp);

	unsigned char m_stopping;
	int m_mutexResult;
	SceKernelLwMutexWork m_mutex;
	SceUID m_threadId;
	SceUID m_eventFlag;
	LocationTimerSlot m_slots[16];
	int m_slotCount;
	unsigned char m_idle;
	int m_activeCount;
	LocationTimerListener m_listener;
	LocationTimerObserver *m_observers[1];
	SceUID m_completeEvent;
};

/*
 * ProviderController / XpsController virtual interface exported by
 * vs0:vsh/shell/liblocation_provider.suprx (vtable at 0x8107E72C in 3.65-CEX).
 */
class LocationProvider {
public:
	virtual int GetLocation(SceLocationLocationInfo *info) = 0;
	virtual int GetLocationCache(SceLocationLocationInfo *info) = 0;
	virtual int StartPeriodicLocation() = 0;
	virtual int StopPeriodicLocation() = 0;
	virtual int StopEngine() = 0;
	virtual int Suspend() = 0;
	virtual int Resume() = 0;
	virtual int IsRunning() = 0;
	virtual int SetInterval(int intervalMs) = 0;
	virtual int SetHeadingMgr(HeadingManager *headingMgr) = 0;
	virtual int NeedsHeading() = 0;
	virtual int StartWifiHistory() = 0;
	virtual int StopWifiHistory() = 0;
	virtual int SaveHistory(const char *titleId, SceRtcTick *timestampOut, int option) = 0;
	virtual int QueryLocation(SceLocationHandle handle, const char *titleId,
				  SceRtcTick *timestamp, int option) = 0;
	virtual int CancelQueryLocation(SceLocationHandle handle) = 0;
};

/*
 * Suspend/resume watcher listener and WiFi scan mode listener (location_poll.cpp).
 */
class LocationSusListener {
public:
	virtual void OnResume() {}
	virtual void OnSuspend() {}
	virtual ~LocationSusListener() {}
};

class LocationSusWatcher : public LocationTask {
public:
	virtual ~LocationSusWatcher();
	virtual int OnNotify();

	int Init(ClientManager *handleMgr);
	int CheckState();

	int m_mutexResult;
	SceKernelLwMutexWork m_mutex;
	unsigned char m_suspended;
	ClientManager *m_handleMgr;
	LocationProvider *m_provider;
	paf::vector<LocationSusListener *> m_listeners;
};

class LocationScanModeListener {
public:
	virtual int OnModeChange(int prevMode, int newMode) = 0;
};

class LocationScanMode {
public:
	int Init();
	int Term();
	int SetMode(int mode);

	int m_apScanMode;
	paf::vector<LocationScanModeListener *> m_listeners;
	int m_mutexResult;
	SceKernelLwMutexWork m_mutex;
};

/*
 * Debug telemetry/state snapshot owned by LocationLogger (432 bytes, allocated on demand
 * by debug API commands and updated by liblocation_provider and QualCommAdapter).
 */
struct LocationGpsSnapshot {
	char fixData[0x34];
	GpsSatellites satellites;
};

struct LocationDebugInfo {
	LocationDebugInfo();

	int gpsSatelliteCount;
	int wifiApCount;
	int xtraAssistEnabled;
	int fakeWifiAs3g;
	int intervalOverrideSec;
	char callbackTag[11];
	signed char sensorActiveMask;
	int xtraState;
	int xtraLocalFileOnly;
	float dopBounds[8];
	float dopInfo[4];
	LocationGpsSnapshot gpsSnapshot;
	int forceXtraDownload;
	int xtraSourceMode;
	int cellRadioIf;
	int cellInfo[5];
};

struct LocationDebugCounts {
	int gpsSatelliteCount;
	int wifiApCount;
};

struct LocationDebugCallbackStatus {
	char callbackTag[11];
	unsigned char sensorActiveMask;
};

struct LocationDebugCellStatus {
	int radioIf;
	int cellInfo[5];
};

struct LocationHistoryStats {
	unsigned char tag;
};

class LocationLogParser {
public:
	virtual ~LocationLogParser()
	{
	}

	virtual void Parse(const char *line, LocationLogger *logger);
};

class LocationLogParserList {
public:
	virtual ~LocationLogParserList()
	{
	}

	virtual void Dispatch(const char *line, LocationLogger *logger)
	{
		for (size_t i = 0; i < m_parsers.size(); i++)
			m_parsers[i]->Parse(line, logger);
	}

	paf::vector<LocationLogParser *> m_parsers;
};

class LocationDebugBuffer {
public:
	LocationDebugBuffer();
	virtual ~LocationDebugBuffer()
	{
	}

	int Init(unsigned char index);
	int Term();
	int DeleteMutex();
	int Append(const void *data, SceSize len);
	void Clear();
	int Lock();
	int Unlock();

	char *m_data;
	SceSize m_used;
	unsigned char m_full;
	SceKernelLwMutexWork m_mutex;
	int m_mutexResult;
};

class LocationLogWriter {
public:
	LocationLogWriter(LocationLogger *logger);
	virtual ~LocationLogWriter();

	int DestroySync();
	int CreateSync();
	int Start();
	int Stop();
	int WriteToBuffer(const char *data, SceSize len);
	int Write(const char *fmt, const char *tag, va_list args,
		  LocationLogParserList *parser);
	int FlushOne();
	int FlushAll();
	int FlushLoop();

	static int FlushThreadEntry(SceSize args, void *argp);

	LocationLogger *m_logger;
	SceUID m_threadId;
	unsigned char m_running;
	char *m_bufMem;
	LocationDebugBuffer m_buffers[2];
	char m_formatBuf[0x8000];
	unsigned char m_writeIdx;
	unsigned char m_readIdx;
	SceUID m_opSema;
	SceUID m_freeSema;
	SceUID m_flushEvent;
	SceKernelLwMutexWork m_readMutex;
	SceKernelLwMutexWork m_writeMutex;
	int m_readMutexResult;
	int m_writeMutexResult;
};

class LocationLogTimeoutTask : public LocationTask {
public:
	virtual int OnNotify();

	LocationLogger *m_logger;
	SceDateTime m_startTime;
	char m_reserved[24];
};

class LocationLogger {
public:
	LocationLogger();

	int StopLog();
	int StartLog(SceUInt32 mask);
	void ResetDebugInfo();
	int Init();
	int Term();
	const char *GetTag(int category);
	int VLog(int category, const char *fmt, va_list args);
	int Log(int category, const char *fmt, ...);

	int GetGpsSatelliteCount();
	int GetWifiApCount();
	int ClearUr0Files();
	int SetXtraAssistEnabled(int enabled);
	int GetXtraAssistEnabled();
	int SetFakeWifiAs3g(int enabled);
	int SetFileIndex(int index);
	const char *GetCallbackTag();
	int TakeSensorActiveMask();
	void SetIntervalOverrideSec(int sec);
	int GetIntervalOverrideSec();
	int ResetDebugPermission();
	int GetXtraState();
	int SetXtraState(int state);
	int SetXtraLocalFileOnly(int enabled);
	int GetXtraLocalFileOnly();
	int SetDopBounds(const float *bounds);
	int GetDopBounds(float *outBounds);
	int GetDopInfo(float *outDop);
	int SetDopInfo(const float *dop);
	int CountUr0Files(int *outCount);
	int GetGpsSnapshot(void *dst);
	SceSSize CopyFile(const char *src, const char *dst, int unused, SceOff size);
	int CopyDatFiles(const char *srcDir, const char *dstDir);
	int SyncDatFiles(int fromUx0);
	int SetForceXtraDownload(int enabled);
	int GetForceXtraDownload();
	int SetXtraSourceMode(int mode);
	int GetXtraSourceMode();
	int ExportHistoryFiles();
	int ImportHistoryFiles();
	int ClearHistoryFiles();
	int GetCellInfo(int *outRadioIf, int *outCellInfo);
	SceSSize WriteRaw(const void *buf, SceSize size);
	int SyncRaw();
	SceUInt32 GetLogMask() const { return m_mask; }

	SceUID m_fd;
	SceUInt32 m_mask;
	LocationLogWriter *m_writer;
	int m_fileIndex;
	unsigned char m_ioError;
	int m_mutexResult;
	SceKernelLwMutexWork m_mutex;
	unsigned char m_logWriteEnabled;
	int m_logFlushMode;
	unsigned char m_logPrefixEnabled;
	unsigned char m_logTimestampEnabled;
	LocationLogParserList *m_parsers;
	LocationDebugInfo *m_debugInfo;
	SceUID m_logSyncFd;
	LocationLogTimeoutTask m_timeoutTask;
	unsigned char m_timeoutExpired;
	LocationHistoryStats m_historyStats;
};

/*
 * XTRA ephemeris downloader (location_xtra.cpp).
 */
class LocationXtraPowerTask : public LocationTask {
public:
	virtual int OnNotify();

	LocationXtra *m_xtra;
};

class LocationXtra {
public:
	int Init();
	int CountLocationApps(int *outCount);
	int DoHttpGet(const char *url, void *buf, SceUInt32 *outSize);
	int DownloadAndSave();

	static int DailyCronCB(SceUID handle, void *arg);
	static void NetCtlCB(int eventType, void *arg);
	static int ThreadEntry(SceSize args, void *argp);

	SceUID m_cronHandle;
	int m_netCtlCid;
	int m_retryNeeded;
	int m_dayCount;
	AppInfoQClient m_lsdbClient;
	void *m_httpBuf;
	SceUInt32 m_xtraDataSize;
	int m_downloading;
	LocationXtraPowerTask m_powerTask;
};

/*
 * Registry & LSDB settings manager (location_setting.cpp; vtable at 0x815317D0).
 */
class LocationRegWatcher : public LocationTask, public LocationSusListener {
public:
	virtual ~LocationRegWatcher();
	virtual int OnNotify();
	virtual void OnResume();

	int Stop();
	int HandleRegChange();
	static int RegCallback(int notifyId, int notifyCount, int notifyArg, void *common);

	LocationSetting *m_setting;
	int m_state;
	SceUID m_cbId;
};

class LocationMethodState {
public:
	virtual ~LocationMethodState() {}

	int m_flags;
	SceUInt32 m_lmethod;
	SceUInt32 m_hmethod;
};

class LocationSetting {
public:
	LocationSetting();

	virtual LocationMethodState *GetMethodState();
	virtual bool IsAdhocAllowed();
	virtual int CountUp(int id, unsigned int delta);
	virtual int SetTelemetryValue(int id, int value);
	virtual int HasGps();
	virtual int IsDeviceEnabled();
	virtual int IsWlanOrGpsAvailable();
	virtual int IsApInvisible();
	virtual int GetGpsTimeoutMs();
	virtual ~LocationSetting();

	int GetDebugPermStatus();
	int CheckLocaleChanged();
	int LoadRegistry();
	int Init();
	int GetAppTitleId(int appId, char *outTitleId);
	int GetParental();
	int SetParental(int val);
	bool IsServiceEnabled();
	int SetServiceEnabled(int val);
	int IsDebugApp(const char *titleId);
	int GetAppPermission(const char *titleId, int *outPerm, unsigned char *outFlag);
	int UpdateLsdbLiblocationAttr(const char *titleId, SceUInt64 mask, SceUInt64 value);
	int SetAppPermission(const char *titleId, int perm);
	int SetAppLastCounter(const char *titleId, int counter);
	int GetAppLastCounter(const char *titleId, SceUInt32 *outCounter);
	int LoadDebugPermission();
	int GetUpdateCounter();
	int IsEmulationMode();
	int GetValue(SceUInt32 *outVal);
	int GetUserAge(int *outAge);
	int ReloadAgePermit();
	void GetMethod(SceUInt32 *lmethod, SceUInt32 *hmethod);
	int SetLMethod(const SceUInt32 *lmethod);
	int SetHMethod(const SceUInt32 *hmethod);

	int m_mutexResult;
	SceKernelLwMutexWork m_mutex;
	int m_enableService;
	int m_enableParental;
	int m_enableLocale;
	int m_updateCounter;
	unsigned char m_isDevMode;
	unsigned char m_debugPermEnabled;
	int m_debugPermStatus;
	int m_agePermit;
	AppInfoQClient m_lsdbClient;
	LocationRegWatcher m_regWatcher;
	unsigned char m_hasTelephony;
	LocationMethodState m_methodState;
	int m_gpsTimeoutMs;
	SceUInt64 m_flyingStartInterval;
};

/*
 * Per-application privacy/permission table (location_privacy.cpp).
 */
struct LocationPrivacyEntry {
	char titleId[32];
	int permission;
	unsigned char dirty;
	int lastCounter;
	unsigned char refCount;
};

class LocationPrivacySettings {
public:
	LocationPrivacySettings(LocationSetting *setting);
	virtual ~LocationPrivacySettings();

	int Init();
	signed char AddOrIncrement(const LocationPrivacyEntry *entry);
	int OpenApp(int appId, signed char *outSlot, const SceUInt32 *lmethod);
	int CloseApp(signed char slot);
	signed char FindSlot(const char *titleId);
	int GetPermission(signed char slot, int *outPerm);
	int SetPermission(const char *titleId, int perm, signed char slot);
	int CheckPermission(signed char slot);
	bool IsGloballyEnabled();
	bool HasAnyAllowedApp();
	int ProviderStart(LocationProvider *provider);
	int ProviderStop(LocationProvider *provider);
	int ProviderSuspend(LocationProvider *provider);
	int ProviderResume(LocationProvider *provider);
	int ConfirmApp(signed char slot);
	int NeedsConfirm(signed char slot, bool *outNeeds);
	int SetAllPermissions(int perm);

	LocationPrivacyEntry m_entries[8];
	unsigned char m_count;
	int m_mutexResult;
	SceKernelLwMutexWork m_mutex;
	LocationSetting *m_setting;
	unsigned char m_providerSuspended;
};

/*
 * GPS hardware state machine (gps_ctrl_sm.cpp), embedded inside DriverManager.
 */
enum GpsCtrlStateId {
	GPS_CTRL_STATE_CLOSED = 0,
	GPS_CTRL_STATE_OPENED = 1,
	GPS_CTRL_STATE_START_REQ = 2,
	GPS_CTRL_STATE_STARTED = 3,
	GPS_CTRL_STATE_STOP_REQ = 4
};

class GpsCtrlStateMachine;
class GpsCtrlEvent;

class GpsCtrlListener {
public:
	virtual void OnStateChanged(int nextState) = 0;
	virtual void OnGpsEvent(int notifyId, int notifyCount, int notifyArg) = 0;
};

class GpsCtrlState {
public:
	virtual int HandleEvent(GpsCtrlEvent *ev, GpsCtrlStateMachine *sm) = 0;
	virtual int GetStateId() = 0;
	virtual ~GpsCtrlState() {}
};

class GpsCtrlEvent {
public:
	virtual int Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm) = 0;
	virtual ~GpsCtrlEvent() {}
};

class GpsCtrlStateMachine {
public:
	virtual ~GpsCtrlStateMachine() {}
	virtual int ChangeState(int nextState);
	virtual int InitState();
	virtual int TermState() { return 0; }

	SceKernelLwMutexWork m_mutex;
	int m_mutexResult;
	GpsCtrlState *m_state;
	GpsCtrlManager *m_mgr;
};

class GpsCtrlPollTask : public LocationTask {
public:
	virtual ~GpsCtrlPollTask() {}
	virtual int OnNotify();

	GpsCtrlManager *m_gpsCtrl;
};

class GpsCtrlCreateCbTask : public LocationTask {
public:
	virtual ~GpsCtrlCreateCbTask() {}
	virtual int OnNotify();

	GpsCtrlManager *m_gpsCtrl;
};

class GpsCtrlManager : public LocationTimerObserver {
public:
	virtual ~GpsCtrlManager() {}
	virtual void OnResume() {}
	virtual void OnSuspend() {}

	int CheckStatus();
	int Init();
	int Term();
	int AddListener(GpsCtrlListener *listener);
	int RemoveListener(GpsCtrlListener *listener);
	int Open();
	int Start(int mode);
	int Stop();
	int Close();
	int HandleGpsEvent(int notifyId, int notifyCount, int notifyArg);

	GpsCtrlListener *m_listeners[3];
	GpsCtrlStateMachine m_sm;
	GpsCtrlPollTask m_pollTask;
	GpsCtrlCreateCbTask m_createCbTask;
	SceUID m_callbackId;
	SceUInt64 m_lastStartedTime;
	unsigned char m_needCreateCb;
};

/*
 * FlyingStartStateMachine (flying_start_sm.cpp) and ProviderGpsStateMachine (provider_gps_sm.cpp).
 */
class ProviderGpsStateMachine;
class ProviderGpsEvent;

class ProviderGpsResetListener {
public:
	virtual void OnReset() = 0;
};

class ProviderGpsState {
public:
	virtual int HandleEvent(ProviderGpsEvent *ev, ProviderGpsStateMachine *sm) = 0;
	virtual int GetStateId() = 0;
	virtual ~ProviderGpsState() {}
};

class ProviderGpsEvent {
public:
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm) = 0;
	virtual ~ProviderGpsEvent() {}
};

class ProviderGpsStateMachine {
public:
	virtual ~ProviderGpsStateMachine() {}
	virtual int ChangeState(int nextState);
	virtual int InitState();
	virtual int TermState() { return 0; }

	SceKernelLwMutexWork m_mutex;
	int m_mutexResult;
	ProviderGpsState *m_state;
	ProviderGpsController *m_ctrl;
};

class ProviderGpsNotifyTask : public LocationTask {
public:
	virtual ~ProviderGpsNotifyTask() {}
	virtual int OnNotify();

	int m_notifyArg;
	ProviderGpsController *m_ctrl;
};

class ProviderGpsController : public GpsCtrlListener {
public:
	virtual void OnStateChanged(int nextState);
	virtual void OnGpsEvent(int notifyId, int notifyCount, int notifyArg);

	int Init();
	int Term();
	int Open(SceUID cbId);
	int Start(int mode);
	int Stop();
	int Close();
	int GetData(GpsLocation *data, GpsSatellites *satellites);
	int GetStatus(int *outStatus);
	int AddResetListener(ProviderGpsResetListener *listener);

	GpsCtrlManager *m_gpsCtrl;
	ProviderGpsStateMachine m_sm;
	ProviderGpsNotifyTask m_notifyTask;
	int m_startMode;
	SceUID m_cbId;
	ProviderGpsResetListener *m_resetListener;
};

class FlyingStartStateMachine;
class FlyingStartEvent;

class FlyingStartState {
public:
	virtual int HandleEvent(FlyingStartEvent *ev, FlyingStartStateMachine *sm) = 0;
	virtual int GetStateId() = 0;
	virtual ~FlyingStartState() {}
};

class FlyingStartEvent {
public:
	virtual int Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm) = 0;
	virtual ~FlyingStartEvent() {}
};

class FlyingStartStateMachine {
public:
	virtual ~FlyingStartStateMachine() {}
	virtual int ChangeState(int nextState);
	virtual int InitState();
	virtual int TermState() { return 0; }

	SceKernelLwMutexWork m_mutex;
	int m_mutexResult;
	FlyingStartState *m_state;
	FlyingStartController *m_ctrl;
};

class FlyingStartTimerTask : public LocationTask {
public:
	virtual ~FlyingStartTimerTask() {}
	virtual int OnNotify();

	FlyingStartStateMachine *m_sm;
	int m_retryCount;
};

class FlyingStartController : public GpsCtrlListener, public ProviderGpsResetListener {
public:
	virtual void OnStateChanged(int nextState);
	virtual void OnGpsEvent(int notifyId, int notifyCount, int notifyArg);
	virtual void OnReset();

	int Init();
	int Term();
	int Trigger();

	GpsCtrlManager *m_gpsCtrl;
	ProviderGpsController *m_gpsXps;
	FlyingStartStateMachine m_sm;
	FlyingStartTimerTask m_timerTask;
	SceUInt64 m_deadline;
};

/*
 * DriverController (driver_controller.cpp), DriverManager (driver_manager.cpp),
 * and QualCommAdapter (qualcomm_adapter.cpp).
 */
class DriverController {
public:
	DriverController(PositionManager *posMgr);
	virtual ~DriverController();
	virtual void OnUnused();

	int SetEmulationFile(const char *path);
	const char *GetEmulationFile() const { return m_emuFile; }
	int SetPowerStatus(unsigned char status);

	PositionManager *m_posMgr;
	unsigned char m_powerStatus;
	char m_emuFile[256];
	unsigned char m_emuFileDirty;
};

struct BbmcGpsCellInfo {
	SceUInt32 radioInterface;
	SceUInt8 cellPresent;
	SceUInt8 measurementPresent;
	SceUInt32 cellId;
	SceUInt32 timingAdvance;
	SceUInt16 lac;
	SceUInt16 mcc;
	SceUInt16 mnc;
	SceInt16 signalStrength;
	SceUInt16 scramblingCode;
	SceUInt8 reserved[302];
};

struct GpsXtraInfo {
	unsigned short maxSize;
	unsigned char numUrls;
	unsigned char urls[4][256];
};

class QualCommAdapter : public GpsCtrlListener {
public:
	QualCommAdapter();
	virtual void OnStateChanged(int state);
	virtual void OnGpsEvent(int notifyId, int notifyCount, int notifyArg);
	virtual ~QualCommAdapter();

	int StartAGpsThread();
	int StartXtraThread(char fromUx0Only);
	int StartXtraDownload();
	SceInt64 DateTimeToMsec(SceDateTime dt);
	int InjectTime();
	int SendNextXtraPart(int mode);
	int DoFileGet(const char *path, void *buf, int *outSize);
	int DoHttpGet(const unsigned char *urlEntry, void *buf, int *outSize);
	int DownloadAndInjectXtra();
	int AgpsCallback(int notifyId, int notifyCount, int notifyArg);

	static int AGpsThreadEntry(SceSize args, void *argp);
	static int XtraGpsCallback(int notifyId, int notifyCount, int notifyArg, void *common);
	static int XtraResetGps(void);
	static int XtraThreadEntry(SceSize args, void *argp);

	void *m_xtraBuf;
	int m_xtraDataSize;
	int m_partIndex;
	GpsXtraData m_xtraSetArg;
	GpsXtraInfo m_xtraInfo;
	unsigned int m_timeInfo;
	unsigned char m_injectState;
	int m_xtraStatus;
	SceUID m_agpsEvf;
	SceUID m_agpsThread;
	unsigned char m_agpsRunning;
	int m_xtraOffset;
	unsigned char m_enabled;
	SceUID m_xtraEvf;
	SceUID m_xtraThread;
	unsigned char m_xtraRunning;
	SceUID m_xtraCbId;
};

int gpsAssistInit(void);
void gpsAssistTerm(void);
void gpsAssistAck(void);
QualCommAdapter *gpsAssistGetInstance(void);

class DriverNetCtlTask : public LocationTask {
public:
	virtual ~DriverNetCtlTask() {}
	virtual int OnNotify();
};

class DriverManager {
public:
	DriverManager();
	virtual ~DriverManager();

	int Init();
	int SetGpsPower(unsigned char on);
	int SetWifiPower(unsigned char on);
	int Set3GPower(unsigned char on);
	int SetEmuPower(unsigned char on);
	int GpsOpen(SceUID xpsCbId);
	int GpsStart(int mode);
	int GpsStop();
	int GpsClose();
	int GpsGetData(GpsLocation *data, GpsSatellites *satellites);
	int GpsGetSatellites(int *outStatus);
	int GpsGetStatus();
	int GpsSetParam(int param);
	int SetEmuFile(const char *path);
	int WifiScanStart(int scanType, int param);
	int WifiGetScanInfo(int outBuf, int maxCount);
	int WifiGetScanInfoByBssid(void *bssid, void *info);
	int BbmcSubscribe(int cbId);
	int BbmcUnsubscribe(int handle);
	int BbmcClearEvent(int handle, int mask);
	int BbmcResumeCallback(int handle);
	int BbmcGetCellStart(int handle);
	int BbmcGetCellDone(void *outInfo);
	void SetAssistPower(unsigned char on);

	static int GpsCallback(int notifyId, int notifyCount, int notifyArg, void *common);

	unsigned char m_gpsPower;
	SceUID m_xpsCbId;
	SceUID m_gpsCbId;
	int m_gpsMutexRes;
	SceKernelLwMutexWork m_gpsMutex;
	unsigned char m_wifiInitialized;
	unsigned char m_wifiPower;
	int m_wifiMutexRes;
	SceKernelLwMutexWork m_wifiMutex;
	unsigned char m_3gPower;
	int m_3gMutexRes;
	SceKernelLwMutexWork m_3gMutex;
	int m_bbmcHandle;
	unsigned char m_emuPower;
	unsigned char m_assistPower;
	DriverNetCtlTask m_netCtlTask;
	GpsCtrlManager m_gpsCtrl;
	ProviderGpsController m_gpsXps;
	FlyingStartController m_flyingStart;
};

/*
 * ClientManager (client_manager.cpp).
 */
struct ClientPendingRequest {
	int cmd;
	int arg;
	SceRtcTick timestamp;
	int error;
};

struct ClientLocationState {
	SceUInt32 lmethod;
	SceUInt32 distance;
	int lastLocResult;
	SceUInt64 lastFailTime;
	SceUInt32 locInterval;
	SceUInt32 locElapsed;
	SceLocationLocationInfo lastLocInfo;
	SceUInt64 getStartTime;
	SceUInt32 getTimeout;
	SceUInt32 getTypeFlags;
	SceRtcTick queryTimestamp;
	SceUInt64 queryStartTime;
	SceUInt32 queryTimeout;
	SceLocationLocationInfo queryResultInfo;
	int queryResultCode;
};

struct ClientHeadingState {
	SceUInt32 hmethod;
	float headAngle;
	int lastHeadResult;
	SceUInt32 headInterval;
	SceUInt32 headElapsed;
	float cachedHeading;
	float lastTrueHeading;
	SceLocationHeadingInfo lastHeadInfo;
};

struct ClientSlot {
	int appId;
	SceUInt32 mode;
	SceUInt32 stateFlags;
	int shellLockHandle;
	signed char privacySlot;
	unsigned char pendingConfirm;
	ClientPendingRequest pending;
	ClientLocationState location;
	ClientHeadingState heading;
};

struct MethodBitSummary {
	int unusedMode;
	unsigned char hasGps;
	unsigned char hasWifi;
	unsigned char has3G;
	unsigned char hasEmu;
};

class ClientManager {
public:
	ClientManager(PositionManager *posMgr);
	virtual ~ClientManager();

	int Init();
	int GetCount();
	bool IsValidHandle(int appId, SceLocationHandle handle);
	int Close(int appId, SceLocationHandle handle);
	int Open(int appId, SceLocationHandle *handle, SceUInt32 *lmethod, SceUInt32 *hmethod,
		 SceUInt32 *mode);
	int Reopen(int appId, SceLocationHandle handle, SceUInt32 *lmethod, SceUInt32 *hmethod);
	int GetMethod(int appId, SceLocationHandle handle, SceUInt32 *lmethod, SceUInt32 *hmethod);
	int SetCachedHeading(SceLocationHandle handle, float val);
	int GetCachedHeading(SceLocationHandle handle, float *out);
	static void AccumulateLMethod(MethodBitSummary *sum, const SceUInt32 *lmethod);
	int GetActiveMethods(SceUInt32 *outLMethod, SceUInt32 *outHMethod);
	int UpdateShellLock(ClientSlot *slot);
	int StartLocationCallback(int appId, SceLocationHandle handle, SceUInt32 distance);
	int HasActiveWifiHistory(unsigned char *out);
	int StartGetLocation(SceLocationHandle handle, SceUInt32 timeout, SceUInt32 typeFlags,
			     const SceRtcTick *queryTs);
	int GetQueryResult(SceLocationHandle handle, SceLocationLocationInfo *outInfo);
	int StartHeadingCallback(int appId, SceLocationHandle handle, float angle = 0.0f);
	int StopLocationCallback(int appId, SceLocationHandle handle);
	int CancelGetLocation(SceLocationHandle handle, SceUInt32 typeFlags);
	int StopHeadingCallback(int appId, SceLocationHandle handle);
	int CheckPositionCondition(ClientSlot *slot, const SceLocationLocationInfo *info,
				   int result, SceUInt32 elapsed);
	int SetPendingRequest(SceLocationHandle handle, int arg, int cmd, const SceRtcTick *ts);
	int SetPendingConfirm(SceLocationHandle handle, unsigned char flag);
	int GetPendingConfirm(SceLocationHandle handle, unsigned char *outFlag);
	int NeedsConfirm(SceLocationHandle handle, bool *outNeeds);
	int GetMode(int appId, SceLocationHandle handle, SceUInt32 *outMode);
	int NotifyIfPositionConditionMet(const SceLocationLocationInfo *info, int result,
					 SceUInt32 elapsed, const SceRtcTick *queryTs);
	int CheckHeadingCondition(ClientSlot *slot, const SceLocationHeadingInfo *info, int result);
	int NotifyHeadingUpdate(int result, SceUInt32 elapsed);
	int GetStateFlags(int appId, SceLocationHandle handle);
	int GetTypeFlags(int appId, SceLocationHandle handle);
	int CheckPermission(SceLocationHandle handle);
	int SetHandlePermission(SceLocationHandle handle, int perm);
	int GetHandlePermission(SceLocationHandle handle, int *outPerm);
	int GetPendingRequest(int *outAppId, SceLocationHandle handle, int *outArg, int *outCmd,
			      SceRtcTick *outTs);
	int GetPendingCmd(SceLocationHandle handle, int *outCmd);
	int SetPendingError(SceLocationHandle handle, int err);
	int GetActiveCount(int *outCount);
	int OnPrivacySlotChanged(signed char privSlot);
	int ClearPermissionErrors();
	int OnGlobalPermissionChanged();
	int ConfirmHandle(int appId, SceLocationHandle handle);
	int AreAllAppsInactive();

	PositionManager *m_posMgr;
	ClientSlot m_clients[8];
	signed char m_count;
	int m_mutexResult;
	SceKernelLwMutexWork m_mutex;
	int m_activeGetCount;
};

/*
 * LocationIntervalCtrl (location_bridge.cpp).
 */
class LocationIntervalCtrl {
public:
	LocationIntervalCtrl(PositionManager *posMgr, HeadingManager *headingMgr,
			     ClientManager *handleMgr);
	virtual ~LocationIntervalCtrl();
	virtual void OnUnused();
	virtual int SetProvider(LocationProvider *provider);
	virtual int UpdateInterval(int flag);

	PositionManager *m_posMgr;
	ClientManager *m_handleMgr;
	LocationProvider *m_provider;
	HeadingManager *m_headingMgr;
	int m_interval;
	int m_flags;
};

/*
 * LocationConfirmMgr (location_confirm.cpp).
 */
typedef void (*LocationPluginDoneFn)(int result, void *arg);
typedef void (*LocationPluginLoadedFn)(int unused, LocationPluginDoneFn doneCb, void *arg);
typedef void (*LocationConfirmCallback)(int appId, int handle, int dialogType, int result,
					int canceled);

struct LocationConfirmApp {
	int appId;
	SceLocationHandle activeHandle;
	signed char handleCount;
	SceLocationHandle handles[8];
};

struct LocationConfirmEntry {
	int paused;
	int dialogType;
	unsigned char abortFlag;
	unsigned char shownPrompt;
	unsigned char skipInitialPrompt;
	int step;
	SceLocationDialogStatus status;
	SceLocationDialogResult result;
	int appId;
	SceLocationHandle handle;
	SceUInt32 arg;
	SceUInt32 dataArg;
	char data[0x800];
	LocationConfirmCallback notifyFn;
	int globalPerm;
	int appPerm;
	SceLocationPermissionInfo permInfo;
};

class LocationConfirmTask : public LocationTask {
public:
	virtual int OnNotify();

	LocationConfirmEntry m_entry;
	int m_canceled;
};

class LocationConfirmMgr {
public:
	LocationConfirmMgr(LocationConfirmCallback cb);
	~LocationConfirmMgr();

	void OnAppDeactivated(int appId);
	void OnAppActivated(int appId);
	int AllocEntry(int appId, SceLocationHandle handle, SceUInt32 arg, SceUInt32 dataArg,
		       const void *data);
	int Show(int appId, SceLocationHandle handle, SceUInt32 arg, SceUInt32 dataArg,
		 const void *data, bool allowBusy);
	SceLocationDialogResult GetResult(int appId, SceLocationHandle handle);
	SceLocationDialogStatus GetStatus(int appId, SceLocationHandle handle);
	int Abort(int appId, SceLocationHandle handle);
	int RemoveEntry(int appId, SceLocationHandle handle);
	int CloseApp(int appId, SceLocationHandle handle);

	int m_activeAppId;
	LocationConfirmApp m_apps[8];
	LocationConfirmEntry m_entries[8];
	signed char m_entryCount;
	int m_pluginState;
	LocationConfirmCallback m_callback;
	PositionManager *m_context;
	LocationPluginDoneFn m_pluginDoneCb;
	int m_lastResult;
	LocationConfirmTask m_task;
};

/*
 * PositionManager (position_manager.cpp).
 */
class PositionConfigRefreshTask : public LocationTask {
public:
	virtual ~PositionConfigRefreshTask() {}
	virtual int OnNotify();

	PositionManager *m_pm;
};

class PositionPowerRegTask : public LocationTask {
public:
	virtual ~PositionPowerRegTask() {}
	virtual int OnNotify();

	PositionManager *m_pm;
	SceUID m_cbId;
	unsigned char m_registered;
};

class PositionManager : public LocationScanModeListener {
public:
	typedef int (*NotifyEventFn)(SceLocationHandle handle, int result, int eventType);
	typedef int (*NotifyHeadingFn)(SceLocationHandle handle, int unused);

	PositionManager();
	virtual int OnModeChange(int prev, int next);
	virtual ~PositionManager();

	int UpdateFlyingStart();
	int Init(NotifyEventFn locCb, NotifyHeadingFn headCb);
	LocationPrivacySettings *GetPermMgr();
	LocationIntervalCtrl *GetBridge();
	int LoadProvider();
	int UpdateHeadingState();
	int UpdateEmuMode(SceUInt32 *lmethod);
	int StopHeadingCallback(int appId, SceLocationHandle handle);
	int StopLocationCallback(int appId, SceLocationHandle handle);
	int CancelGetLocation(int appId, SceLocationHandle handle);
	int Close(int appId, SceLocationHandle handle);
	int ValidateLMethod(SceUInt32 *lmethod);
	int Open(int appId, SceLocationHandle *handle, SceUInt32 *lmethod,
		 SceUInt32 *hmethod, SceUInt32 *mode);
	int Reopen(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
		   SceUInt32 *hmethod);
	int GetMethod(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
		      SceUInt32 *hmethod);
	int ComparePriority(const SceUInt32 *lhs, const SceUInt32 *rhs);
	int IsAppActive(int appId, bool *outActive);
	int GetQueryLocation(int appId, SceLocationHandle handle, int unused,
			     SceRtcTick timestamp, SceUInt32 timeout,
			     SceLocationLocationInfo *info, bool option);
	int GetQueryLocationCache(int appId, SceLocationHandle handle,
				  SceLocationLocationInfo *info);
	int CancelQueryLocation(int appId, SceLocationHandle handle);
	int GetLocation(int appId, SceLocationHandle handle, SceUInt32 timeout,
			SceLocationLocationInfo *info, bool wifiHistory);
	int GetLocationCache(int appId, SceLocationHandle handle,
			     SceLocationLocationInfo *info, bool wifiHistory,
			     bool option);
	int StartLocationCallback(int appId, SceLocationHandle handle, SceUInt32 distance);
	int GetHeading(int appId, SceLocationHandle handle, SceLocationHeadingInfo *info);
	int StartHeadingCallback(int appId, SceLocationHandle handle, float angle = 0.0f);
	int GetPermission(int appId, SceLocationHandle handle, SceLocationPermissionInfo *info);
	int NotifyHeadingUpdate(int result, SceUInt32 elapsed);
	int NotifyEvent(SceLocationHandle handle, int result, int eventType);
	int NotifyHeading(SceLocationHandle handle, int unused);
	double CalcDistance(const SceLocationLocationInfo *fromPos,
			    const SceLocationLocationInfo *toPos);
	int SetAppPermByTitleId(const char *titleId, int status);
	int ResetAllPerms(int val);
	int GetMainPerm(int *out);
	int SetMainPerm(int val);
	int GetParentalPerm(int *out);
	int SetParentalPerm(int val);
	int GetGlobalPerm(int *out);
	int GetAppPermSetting(int *out);
	int ReloadConfig();
	int SetHandlePermission(SceLocationHandle handle, int status);
	int GetHandleStatus(int appId, SceLocationHandle handle);
	int SetGpsEmulationFile(const char *path);
	int ConfirmHandle(int appId, SceLocationHandle handle);
	int GetValue(SceUInt32 *out);
	int FlyingStartResume(const char *titleId);
	int FlyingStartLaunch(const char *titleId);
	int IsHandlePending(SceLocationHandle handle, unsigned char *out);
	int SetHandlePending(SceLocationHandle handle, unsigned char pending);
	int ResumePending(SceLocationHandle handle);
	int PollActive();

	/* debug command handlers (0x8118E12C-0x8118E4C8) */
	int DbgClearUr0Files();
	int DbgGetGpsSatelliteCount(int *out);
	int DbgGetWifiApCount(int *out);
	int DbgSetLogMask(int mask);
	int DbgGetLogMask(int *out);
	int DbgSetXtraAssistEnabled(int val);
	int DbgSetFakeWifiAs3g(int val);
	int DbgSetLogFileIndex(int idx);
	int DbgGetCallbackTag(char *out);
	int DbgTakeSensorActiveMask(unsigned char *out);
	int DbgGetLogIoError(int *out);
	int DbgSetIntervalOverrideSec(int val);
	int DbgGetIntervalOverrideSec(int *out);
	int DbgResetDebugPermission();
	int DbgGetXtraState(int *out);
	int DbgSetXtraLocalFileOnly(int val);
	int DbgUnloadProvider();
	int DbgSetDopBounds(const float *inBounds);
	int DbgGetDopInfo(float *outDop);
	int DbgCountUr0Files(int *out);
	int DbgGetGpsSnapshot(void *out);
	int DbgSyncDatFiles(int fromUx0);
	int DbgSetForceXtraDownload(int val);
	int DbgSetXtraSourceMode(int val);
	int DbgExportHistory();
	int DbgImportHistory();
	int DbgClearHistory();
	int DbgGetCellInfo(int *outRadioIf, int *outCellInfo);
	int DbgSetGpsTimeoutMs(int val);
	int DbgSetFlyingStartIntervalSec(int sec);

	static int PowerCallback(int notifyId, int notifyCount, int powerInfo, void *common);
	static int ProviderEventCB(const SceLocationLocationInfo *info, int result,
				   SceUInt32 elapsed, const SceRtcTick *queryTs);

	ClientManager *m_handleMgr;
	LocationProvider *m_provider;
	HeadingManager *m_headingMgr;
	DriverController *m_driverCtrl;
	LocationIntervalCtrl *m_bridge;
	LocationSetting *m_config;
	LocationPrivacySettings *m_permMgr;
	LocationLogger *m_logger;
	NotifyEventFn m_notifyEventCb;
	NotifyHeadingFn m_notifyHeadingCb;
	unsigned char m_providerLoaded;
	SceUID m_providerModId;
	LocationSusWatcher m_susWatcher;
	PositionConfigRefreshTask m_configRefreshTask;
	PositionPowerRegTask m_powerRegTask;
	char m_flyingTitles[8][32];
	int m_flyingCount;
	LocationScanMode m_scanMode;
};

/*
 * WMM IGRF-11 geomagnetic model context (wmm_geomag.cpp / heading_manager.cpp).
 */
struct WmmContext {
	float gh[196];
	float declination;
	int nmax;
	float decYear;
};

void wmmSetDate(WmmContext *ctx, int month, int day, int year);
void wmmCalcDeclination(WmmContext *ctx, float lat, float lon, float alt);

/*
 * HeadingManager (heading_manager.cpp).
 */
struct LocationQuat {
	float x;
	float y;
	float z;
	float w;
};

class HeadingConfigObserver {
public:
	virtual void OnHeadingConfigChanged() = 0;
	virtual ~HeadingConfigObserver() {}
};

class HeadingPollTask : public LocationTask {
public:
	virtual ~HeadingPollTask() {}
	virtual int OnNotify();

	HeadingManager *m_headingMgr;
};

class HeadingManager : public HeadingConfigObserver, public LocationSusListener {
public:
	HeadingManager(PositionManager *posMgr);
	virtual ~HeadingManager();
	virtual void OnHeadingConfigChanged();
	virtual void OnResume();
	virtual void OnSuspend();

	int StopMotion();
	int IsMotionStarted();
	int Term();
	int LoadLastLocation();
	int Init();
	int StartMotion();
	int EnableMagnetometer();
	int DisableMagnetometer();
	int PollMotion();
	int ComputeHeadingAngle(const float *xyVec, float *outHeading, const SceUInt32 *hmethod,
				float prevHeading);
	int RotateBasisVector(const LocationQuat *quat, const SceUInt32 *hmethod, float *outVec);
	int GetHeading(const SceUInt32 *hmethod, SceLocationHeadingInfo *outInfo);
	int StartPolling();
	int StopPolling();
	int IsMagnetometerEnabled();
	int SaveLastLocation();
	void UpdateLocation(const double *latLon);

	PositionManager *m_posMgr;
	unsigned char m_initialized;
	signed char m_pollRefCount;
	unsigned char m_motionStarted;
	unsigned char m_magEnabled;
	double m_lastLat;
	double m_lastLon;
	int m_mutexResult;
	SceKernelLwMutexWork m_mutex;
	int m_orientation;
	WmmContext m_wmm;
	SceMotionState m_motionState;
	LocationQuat m_curQuat;
	int m_valid;
	LocationQuat m_smoothQuat;
	LocationQuat m_ring[9];
	unsigned int m_ringHead;
	unsigned int m_ringTail;
	unsigned int m_ringLast;
	SceUInt64 m_lastPollTime;
	HeadingPollTask m_pollTask;
};

/*
 * Telemetry counters (location_stats.cpp).
 */
int locationStatsCountWifiHistoryCalled(void);
int locationStatsCountWifiHistoryFailed(void);
int locationStatsCountQueryRequested(void);
int locationStatsCountQuerySucceeded(void);

/*
 * Utility helpers (location_util.cpp).
 */
int locationDecodeInterval(const SceUInt32 *lmethod);
int locationDecodePriority(const SceUInt32 *lmethod);
int locationDecodeDistance(const SceUInt32 *lmethod);
float locationDecodeHeadingAngle(const SceUInt32 *hmethod);
const char *locationStrNStr(const char *haystack, const char *needle, SceSize len);
int locationGetCurrentTick(SceRtcTick *tick);
int locationInitLocationInfo(SceLocationLocationInfo *info);
int locationInitHeadingInfo(SceLocationHeadingInfo *info);
int locationCompareTick(const SceRtcTick *lhs, const SceRtcTick *rhs);
SceLocationHandle locationMakeHandle(int index);
unsigned int locationHandleToIndex(SceLocationHandle handle);

/*
 * Service lifecycle and singleton getters shared across location_service files.
 */
PositionManager *locationGetContext(void);
LocationConfirmMgr *locationConfirmGetInstance(void);
DriverManager *locationDeviceGetInstance(void);
int locationConfirmInit(LocationConfirmCallback cb);
int locationConfirmTerm(void);
int locationCoreInit(PositionManager::NotifyEventFn locCb,
		     PositionManager::NotifyHeadingFn headCb);
int locationCoreTerm(void);
int locationGpsInit(void);
void locationGpsTerm(void);
int locationProviderInit(void);
int locationPermSvcStart(void);
int locationPermSvcStop(void);
int locationPermServerStart(void);
int locationPermServerStop(void);
int locationPermSvcSetAppPerm(int unused, const char *titleId, int perm);
int locationPermSvcGetMainPerm(int unused, int *out);
int locationPermSvcSetMainPerm(int unused, int val);
int locationPermSvcGetParentalPerm(int unused, int *out);
int locationPermSvcSetParentalPerm(int unused, int val);
int locationPermSvcGetGlobalPerm(int unused, int *out);
int locationPermSvcGetAppPermSetting(int unused, int *out);

#endif /* LOCATION_INTERNAL_H */
