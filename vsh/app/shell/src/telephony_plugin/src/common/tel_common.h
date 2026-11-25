/*
 * tel_common.h: shared base types, observer/observable, attribute storage,
 * INI/registry accessors, threading, timers, and domain model objects for
 * telephony_plugin (vsh/app/shell/src/telephony_plugin/src/common/).
 */
#ifndef TELEPHONY_COMMON_H
#define TELEPHONY_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <std/map>
#include <common/shared_ptr.h>
#include <datetime/datetime.h>
#include <thread/thread.h>
#include <memory/heap_allocator.h>
#include <ini_file_processor/ini_file_processor.h>
#include "plugins.h"

namespace paf {
class Plugin;
}

namespace telephony {

/* Error codes returned in TelephonyResult */
enum TelErrorCode {
	TEL_OK = 0,
	TEL_ERR_NO_MEMORY = -2146424831,		/* 0x80102001 */
	TEL_ERR_NOT_SUPPORTED = -2146424830,		/* 0x80102002 */
	TEL_ERR_NOT_FOUND = -2146424829,		/* 0x80102003 */
	TEL_ERR_NOT_INITIALIZED = -2146424827,		/* 0x80102005 */
	TEL_ERR_IO_FAILED = -2146424826,		/* 0x80102006 */
	TEL_ERR_BUFFER_TOO_SMALL = -2146424825,		/* 0x80102007 */
	TEL_ERR_INVALID_ARG = -2146424824,		/* 0x80102008 */
	TEL_ERR_FAILED = -2146424822,			/* 0x8010200A */
	TEL_ERR_ALREADY_INIT = -2146424820,		/* 0x8010200C */
	TEL_ERR_THREAD_START = -2146424819,		/* 0x8010200D */
	TEL_ERR_DB_FAILED = -2146424814,		/* 0x80102012 */
	TEL_ERR_PROFILE_NOT_FOUND = -2146424671		/* 0x801020A1 */
};

/* Common event IDs used by TelMessageThread and TelAttributeManager */
enum TelCommonEventId {
	TEL_EVENT_THREAD_START = 257,
	TEL_EVENT_THREAD_STOP = 258,
	TEL_EVENT_ATTRIBUTE_CHANGED = 1793,
	TEL_EVENT_ATTRIBUTE_ERROR = 1795
};

/* Plugin allocator and string formatting utilities (0x8124E580..0x8124E795) */
paf::memory::HeapAllocator *telSetPluginAllocator(paf::Plugin *plugin);
paf::memory::HeapAllocator *telGetAllocator(void);
void *telMalloc(SceSize size);
void telFree(void *ptr);
paf::string telFormatString(const char *format, ...);
paf::wstring telFormatWString(const wchar_t *format, ...);

/* Generic base object with clone and validation interface */
class TelObject {
public:
	TelObject() {}
	virtual ~TelObject() {}
	virtual paf::common::SharedPtr<TelObject> Clone() const;
	virtual bool IsValid() const { return true; }
};

/* Event payload passed through TelObservable and TelMessageThread (0x813DD648) */
class TelEventInfo : public TelObject {
public:
	TelEventInfo(int eventId, int eventArg, int eventResult)
		: m_timerHandle(0), m_eventId(eventId), m_eventArg(eventArg),
		  m_eventResult(eventResult)
	{
	}
	virtual ~TelEventInfo() {}
	virtual paf::common::SharedPtr<TelObject> Clone() const;
	virtual bool IsValid() const { return true; }

	int GetEventId() const { return m_eventId; }
	int GetEventArg() const { return m_eventArg; }
	int GetEventResult() const { return m_eventResult; }
	void SetEventResult(int result) { m_eventResult = result; }
	void SetTimerHandle(SceUID timerId) { m_timerHandle = timerId; }
	SceUID GetTimerHandle() const { return m_timerHandle; }
	void SetCategory(int category) { m_timerHandle = (SceUID)category; }
	int GetCategory() const { return (int)m_timerHandle; }

protected:
	SceUID m_timerHandle;
	int m_eventId;
	int m_eventArg;
	int m_eventResult;
};

/* Observer interface (notified by TelObservable::NotifyObservers) */
class TelObserver {
public:
	virtual ~TelObserver() {}
	virtual void OnNotify(const TelEventInfo &event) = 0;
};

/* Thread-safe observable base class */
class TelObservable {
public:
	TelObservable(const char *mutexName);
	virtual ~TelObservable();
	virtual void AddObserver(TelObserver *observer);
	virtual void RemoveObserver(TelObserver *observer);
	virtual void NotifyObservers(const TelEventInfo &event);

protected:
	paf::vector<TelObserver *> m_observers;
	paf::thread::RMutex m_observerMutex;
	bool m_notifying;
};

/* Attribute value types and descriptor table entries */
enum TelAttributeValueType {
	TEL_ATTR_VAL_NONE = 0,
	TEL_ATTR_VAL_INT = 1,
	TEL_ATTR_VAL_BOOL = 2,
	TEL_ATTR_VAL_FLOAT = 3,
	TEL_ATTR_VAL_STRING = 4
};

enum TelAttributeStorageType {
	TEL_ATTR_STORE_INT = 0,
	TEL_ATTR_STORE_FLOAT = 2,
	TEL_ATTR_STORE_STRING = 3
};

struct TelAttributeEntry {
	const char *categoryOrPath;
	int attributeId;
	const char *keyName;
	int storageType;
	SceSize maxStringSize;
};

struct TelAttributeValue {
	int attributeId;
	int valueType;
	bool boolValue;
	int intValue;
	float floatValue;
	paf::string stringValue;

	TelAttributeValue()
		: attributeId(0), valueType(TEL_ATTR_VAL_NONE), boolValue(false),
		  intValue(0), floatValue(0.0f), stringValue("")
	{
	}

	explicit TelAttributeValue(int id)
		: attributeId(id), valueType(TEL_ATTR_VAL_NONE), boolValue(false),
		  intValue(0), floatValue(0.0f), stringValue("")
	{
	}

	void SetInt(int val)
	{
		valueType = TEL_ATTR_VAL_INT;
		boolValue = (val != 0);
		intValue = val;
		floatValue = (float)val;
		stringValue = "";
	}

	void SetBool(bool val)
	{
		valueType = TEL_ATTR_VAL_BOOL;
		boolValue = val;
		intValue = val ? 1 : 0;
		floatValue = val ? 1.0f : 0.0f;
		stringValue = "";
	}

	void SetFloat(float val)
	{
		valueType = TEL_ATTR_VAL_FLOAT;
		boolValue = (val != 0.0f);
		intValue = (int)val;
		floatValue = val;
		stringValue = "";
	}

	void SetString(const paf::string &val)
	{
		valueType = TEL_ATTR_VAL_STRING;
		boolValue = false;
		intValue = 0;
		floatValue = 0.0f;
		stringValue = val;
	}
};

/* Abstract backend store for TelAttributeManager */
class TelAttributeStore {
public:
	virtual ~TelAttributeStore() {}
	virtual TelephonyResult Initialize() { return TelephonyResult(TEL_OK); }
	virtual TelephonyResult Terminate() { return TelephonyResult(TEL_OK); }
	virtual int GetMask() const = 0;
	virtual TelephonyResult SetAttribute(const TelAttributeValue &attr)
	{
		(void)attr;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult GetAttribute(TelAttributeValue *outAttr)
	{
		(void)outAttr;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult AddAttribute(const TelAttributeValue &delta)
	{
		(void)delta;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult SetInt(int attrId, int value)
	{
		(void)attrId;
		(void)value;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult GetInt(int attrId, int *outValue)
	{
		(void)attrId;
		(void)outValue;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult AddInt(int attrId, int delta)
	{
		(void)attrId;
		(void)delta;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult SetFloat(int attrId, float value)
	{
		(void)attrId;
		(void)value;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult GetFloat(int attrId, float *outValue)
	{
		(void)attrId;
		(void)outValue;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult AddFloat(int attrId, float delta)
	{
		(void)attrId;
		(void)delta;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult SetString(int attrId, const paf::string &value)
	{
		(void)attrId;
		(void)value;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult GetString(int attrId, paf::string *outValue)
	{
		(void)attrId;
		(void)outValue;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult SetBool(int attrId, bool value)
	{
		(void)attrId;
		(void)value;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual TelephonyResult GetBool(int attrId, bool *outValue)
	{
		(void)attrId;
		(void)outValue;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
};

/* Factory interface that populates TelAttributeManager's store list */
class TelAttributeFactory {
public:
	virtual ~TelAttributeFactory() {}
	virtual TelephonyResult CreateStores(paf::vector<TelAttributeStore *> *stores) = 0;
	virtual TelephonyResult DestroyStores(paf::vector<TelAttributeStore *> *stores) = 0;
};

/* Singleton attribute manager (0x813DCC20..0x813DD097) */
class TelAttributeManager : public TelObservable {
public:
	explicit TelAttributeManager(TelAttributeFactory *factory);
	virtual ~TelAttributeManager();

	static TelephonyResult Initialize(TelAttributeFactory *factory);
	static TelephonyResult Terminate();
	static TelAttributeManager *GetInstance();

	TelephonyResult InitStores();
	TelephonyResult TermStores();
	TelephonyResult SetAttribute(const TelAttributeValue &attr);
	TelephonyResult GetAttribute(TelAttributeValue *outAttr);

private:
	paf::vector<TelAttributeStore *> m_stores;
	TelAttributeFactory *m_factory;
};

/* Wrapper around SceIniFileProcessor (0x813DD774..0x813DDF35) */
class TelIniFileAccessor {
public:
	TelIniFileAccessor();
	virtual ~TelIniFileAccessor();

	virtual TelephonyResult Initialize();
	virtual TelephonyResult Terminate();
	virtual TelephonyResult Open(const paf::string &path, bool writable);
	virtual TelephonyResult Close();
	virtual TelephonyResult Flush();
	virtual TelephonyResult GetString(const paf::string &key, paf::string *outValue);
	virtual TelephonyResult GetInt(const paf::string &key, int *outValue);
	virtual TelephonyResult GetFloat(const paf::string &key, float *outValue);
	virtual TelephonyResult SetString(const paf::string &key, const paf::string &value);
	virtual TelephonyResult SetInt(const paf::string &key, int value);
	virtual TelephonyResult SetFloat(const paf::string &key, float value);
	virtual TelephonyResult GetNextString(const paf::string &key, paf::string *outValue);
	virtual TelephonyResult GetNextInt(const paf::string &key, int *outValue);
	virtual TelephonyResult GetNextFloat(const paf::string &key, float *outValue);
	virtual TelephonyResult RemoveKey(const paf::string &key);

protected:
	bool m_isOpen;
	IniFileProcessor m_processor;
	IniAllocatorOps *m_allocatorOps;
};

/* Scoped APN list INI file accessor (0x813DDF36..0x813DE1E7) */
class TelApnIniFileAccessor : public TelIniFileAccessor {
public:
	TelApnIniFileAccessor();
	virtual ~TelApnIniFileAccessor();

	virtual TelephonyResult Open(const paf::string &path, bool writable);
	virtual TelephonyResult OpenForMcc(SceUInt16 mcc);
};

/* INI-backed attribute store base class (0x813DE1E8..0x813DF26F) */
class TelIniFileAttributeBase : public TelAttributeStore {
public:
	TelIniFileAttributeBase();
	virtual ~TelIniFileAttributeBase() {}

	virtual TelephonyResult Initialize();
	virtual TelephonyResult Terminate();
	virtual int GetMask() const = 0;
	virtual TelephonyResult SetAttribute(const TelAttributeValue &attr);
	virtual TelephonyResult GetAttribute(TelAttributeValue *outAttr);
	virtual TelephonyResult SetInt(int attrId, int value);
	virtual TelephonyResult GetInt(int attrId, int *outValue);
	virtual TelephonyResult SetFloat(int attrId, float value);
	virtual TelephonyResult GetFloat(int attrId, float *outValue);
	virtual TelephonyResult SetString(int attrId, const paf::string &value);
	virtual TelephonyResult GetString(int attrId, paf::string *outValue);
	virtual const TelAttributeEntry *FindEntry(int attrId) const = 0;

protected:
	TelIniFileAccessor m_accessor;
	paf::map<int, TelAttributeValue> m_cache;
	paf::thread::Mutex m_mutex;
};

/* Wrapper around sceRegMgr*ForUser (0x813E2590..0x813E284B) */
class TelRegistoryAccessor {
public:
	TelRegistoryAccessor();
	virtual ~TelRegistoryAccessor();

	virtual TelephonyResult Initialize() { return TelephonyResult(TEL_OK); }
	virtual TelephonyResult Terminate() { return TelephonyResult(TEL_OK); }
	virtual TelephonyResult Open(const paf::string &category);
	virtual TelephonyResult Close();
	virtual TelephonyResult GetInt(const paf::string &key, int *outValue);
	virtual TelephonyResult GetFloat(const paf::string &key, float *outValue);
	virtual TelephonyResult GetString(const paf::string &key, paf::string *outValue);
	virtual TelephonyResult GetBinary(const paf::string &key, void *outBuf, SceSize size);
	virtual TelephonyResult SetInt(const paf::string &key, int value);
	virtual TelephonyResult SetFloat(const paf::string &key, float value);
	virtual TelephonyResult SetString(const paf::string &key, const paf::string &value,
					  SceSize maxSize);
	virtual TelephonyResult SetBinary(const paf::string &key, const void *buf, SceSize size);
	virtual TelephonyResult RemoveKey(const paf::string &key)
	{
		(void)key;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}

protected:
	paf::string m_category;
};

/* Registry-backed attribute store base class (0x813E284C..0x813E36AF) */
class TelRegistoryAttributeBase : public TelAttributeStore {
public:
	TelRegistoryAttributeBase();
	virtual ~TelRegistoryAttributeBase() {}

	virtual TelephonyResult Initialize();
	virtual TelephonyResult Terminate();
	virtual int GetMask() const = 0;
	virtual TelephonyResult SetAttribute(const TelAttributeValue &attr);
	virtual TelephonyResult GetAttribute(TelAttributeValue *outAttr);
	virtual TelephonyResult SetInt(int attrId, int value);
	virtual TelephonyResult GetInt(int attrId, int *outValue);
	virtual TelephonyResult SetFloat(int attrId, float value);
	virtual TelephonyResult GetFloat(int attrId, float *outValue);
	virtual TelephonyResult SetString(int attrId, const paf::string &value);
	virtual TelephonyResult GetString(int attrId, paf::string *outValue);
	virtual const TelAttributeEntry *FindEntry(int attrId) const = 0;

protected:
	TelRegistoryAccessor m_accessor;
	paf::thread::Mutex m_mutex;
};

/* In-memory temporary attribute store base class (0x813E3714..0x813E446F) */
class TelTemporaryAttributeBase : public TelAttributeStore {
public:
	TelTemporaryAttributeBase();
	virtual ~TelTemporaryAttributeBase() {}

	virtual TelephonyResult Initialize() { return TelephonyResult(TEL_OK); }
	virtual TelephonyResult Terminate();
	virtual int GetMask() const = 0;
	virtual TelephonyResult SetAttribute(const TelAttributeValue &attr);
	virtual TelephonyResult GetAttribute(TelAttributeValue *outAttr);
	virtual TelephonyResult AddAttribute(const TelAttributeValue &delta);
	virtual TelephonyResult SetInt(int attrId, int value);
	virtual TelephonyResult GetInt(int attrId, int *outValue);
	virtual TelephonyResult AddInt(int attrId, int delta);
	virtual TelephonyResult SetFloat(int attrId, float value);
	virtual TelephonyResult GetFloat(int attrId, float *outValue);
	virtual TelephonyResult AddFloat(int attrId, float delta);
	virtual TelephonyResult SetString(int attrId, const paf::string &value);
	virtual TelephonyResult GetString(int attrId, paf::string *outValue);
	virtual TelephonyResult SetBool(int attrId, bool value);
	virtual TelephonyResult GetBool(int attrId, bool *outValue);

protected:
	paf::map<int, TelAttributeValue> m_values;
	paf::thread::Mutex m_mutex;
};

/* Calendar DateTime wrapper (0x813DD098..0x813DD647) */
class TelDateTime {
public:
	TelDateTime();
	TelDateTime(SceUInt16 year, SceUInt16 month, SceUInt16 day, SceUInt16 hour,
		    SceUInt16 minute, SceUInt16 second, SceUInt16 millisecond);
	virtual ~TelDateTime();

	paf::wstring FormatLocaleString() const;
	paf::string FormatIsoString() const;
	void ParseIsoString(const paf::string &isoStr);
	void ApplyQuarterHourTimezone(int quarterHoursEast);
	void ConvertUtcToLocal();

	paf::datetime::DateTime &GetDateTime() { return m_dateTime; }
	const paf::datetime::DateTime &GetDateTime() const { return m_dateTime; }

private:
	paf::datetime::DateTime m_dateTime;
};

/* IMEI string wrapper (0x813DD678..0x813DD773) */
class TelImeiInfo {
public:
	TelImeiInfo();
	virtual ~TelImeiInfo();
	virtual bool IsValid() const;

	TelephonyResult SetImei(const paf::string &imei);
	TelephonyResult GetImei(paf::string *outImei) const;
	const paf::string &GetValue() const { return m_imei; }

private:
	paf::string m_imei;
};

/* Thread base class wrapping paf::thread::Thread (0x813E4470..0x813E4575) */
class TelThread : public paf::thread::Thread {
public:
	TelThread(const char *name, int priority, SceSize stackSize);
	virtual ~TelThread();

	virtual void EntryFunction();
	virtual TelephonyResult StartThread(void *userArg);
	virtual TelephonyResult StopThread();
	virtual void CancelThread();
	virtual void JoinThread();
	virtual bool IsThreadAlive() { return IsAlive(); }
	virtual int Run(void *userArg) = 0;

	static void SleepMs(SceUInt32 msec);
	bool IsThreadCanceled();

protected:
	const char *m_threadName;
	void *m_userArg;
	int m_exitResult;
};

/* Two-priority thread-safe event queue (0x81454104..0x814544FF) */
class TelMessageQueue {
public:
	TelMessageQueue(const char *name, SceSize maxCount);
	~TelMessageQueue();

	TelephonyResult Activate();
	TelephonyResult Deactivate();
	TelephonyResult Push(const TelEventInfo &event, int priority);
	TelephonyResult WaitForMessage();
	bool Pop(paf::common::SharedPtr<TelEventInfo> *outEvent, int priority);

private:
	SceSize m_maxCount;
	bool m_active;
	paf::vector<paf::common::SharedPtr<TelEventInfo> > m_queues[2];
	paf::thread::Mutex m_mutex;
	paf::thread::Cond m_notFullCond;
	paf::thread::Cond m_notEmptyCond;
};

/* Message-queue receiver interface for TelMessageThread */
class TelMessageReceiver {
public:
	virtual ~TelMessageReceiver() {}
	virtual void OnReceiveMessage(const TelEventInfo &event) = 0;
	virtual TelephonyResult PostMessage(const TelEventInfo &event) = 0;
	virtual TelephonyResult PostUrgentMessage(const TelEventInfo &event) = 0;
};

/* Event-driven worker thread (0x813DF9B8..0x813DFDF7) */
class TelMessageThread : public TelThread, public TelMessageReceiver {
public:
	TelMessageThread(const char *name, int priority, SceSize stackSize);
	virtual ~TelMessageThread();

	virtual TelephonyResult StartThread(void *userArg);
	virtual TelephonyResult StopThread();
	virtual int Run(void *userArg);
	virtual TelephonyResult DispatchEvent(const TelEventInfo &event);
	virtual TelephonyResult PostMessage(const TelEventInfo &event);
	virtual TelephonyResult PostUrgentMessage(const TelEventInfo &event);
	virtual void OnReceiveMessage(const TelEventInfo &event);
	virtual void OnHandleEvent(const TelEventInfo &event) { (void)event; }
	virtual void OnStart() {}
	virtual void OnStop();

protected:
	TelMessageQueue *m_queue;
};

/* Timer manager backed by sceKernelCreateTimer + MainThreadCallList (0x813E4576..0x813E4C2F) */
struct TelTimerEntry {
	paf::common::SharedPtr<TelEventInfo> event;
	TelObserver *observer;
};

class TelTimerManager {
public:
	TelTimerManager();
	~TelTimerManager();

	static TelephonyResult Initialize();
	static TelephonyResult Terminate();
	static TelTimerManager *GetInstance();

	TelephonyResult StartTimer(SceUInt32 msec, TelObserver *observer, SceUID *outTimerId,
				   const TelEventInfo &event);
	TelephonyResult CancelTimer(SceUID timerId);
	TelephonyResult CancelAllTimers();
	void OnTimerFired(SceUID timerId);

private:
	static int TimerCallback(int notifyId, int notifyCount, int notifyArg, void *common);
	static void MainThreadPoll(void *arg);

	paf::map<SceUID, TelTimerEntry> m_timers;
	SceUID m_callbackId;
	paf::thread::RMutex m_mutex;
};

/* Roaming indicator (0x813E36B0..0x813E3713) */
class TelRoamingInfo {
public:
	TelRoamingInfo();
	virtual ~TelRoamingInfo();
	virtual bool IsValid() const { return true; }

	TelephonyResult SetState(int state);
	int GetState() const { return m_state; }

private:
	int m_state;
};

/* Packet bearer capability (0x813E22E0..0x813E235F) */
class TelPacketBearerInfo {
public:
	TelPacketBearerInfo();
	virtual ~TelPacketBearerInfo();
	virtual bool IsValid() const { return true; }

	TelephonyResult SetBearerType(int bearerType);
	TelephonyResult GetMappedBearerType(int *outMappedType) const;
	int GetBearerType() const { return m_bearerType; }

private:
	int m_bearerType;
};

/* Call end reason filter entry (0x813E2360..0x813E24B3) */
class TelCallEndReasonInfo {
public:
	TelCallEndReasonInfo();
	TelCallEndReasonInfo(int callEndType, const paf::vector<int> &reasonCodes);
	virtual ~TelCallEndReasonInfo();
	virtual bool IsValid() const;

	TelephonyResult SetSingleReason(int callEndType, int reasonCode);
	TelephonyResult GetFirstReason(int *outCallEndType, int *outReasonCode) const;
	bool Matches(int callEndType, int reasonCode) const;

private:
	int m_callEndType;
	paf::vector<int> m_reasonCodes;
};

/* Antenna signal threshold list & factory (0x813DBD48..0x813DC28F) */
class TelAntennaLevelThreshold {
public:
	TelAntennaLevelThreshold();
	virtual ~TelAntennaLevelThreshold();
	virtual bool IsValid() const { return true; }

	TelephonyResult SetThreshold(int threshold);
	int GetThreshold() const { return m_threshold; }

private:
	int m_threshold;
};

class TelAntennaThresholdInfo {
public:
	TelAntennaThresholdInfo();
	virtual ~TelAntennaThresholdInfo();
	virtual bool IsValid() const { return true; }

	TelephonyResult SetThresholds(const paf::vector<int> &thresholds);
	TelephonyResult GetThresholds(paf::vector<int> *outThresholds) const;

private:
	paf::vector<int> m_thresholds;
};

class TelAntennaThresholdInfoFactory {
public:
	virtual ~TelAntennaThresholdInfoFactory();
	virtual TelephonyResult Create(int radioType, TelAntennaThresholdInfo *outInfo);
};

class TelAntennaThresholdInfoUtil {
public:
	virtual ~TelAntennaThresholdInfoUtil();
	virtual TelephonyResult LoadFromIni(int radioType, TelAntennaThresholdInfo *outInfo);
};

/* Operator APN info & APN list (0x813DC290..0x813DCC1F) */
struct TelOperatorApnRawRecord {
	char apnName[101];
	char contractStringId[128];
	char userId[127];
	char password[128];
	int billingPlan;
	int autoSelectable;
};

class TelOperatorApnInfo : public TelephonyOperator {
public:
	TelOperatorApnInfo();
	TelOperatorApnInfo(const TelephonyOperator &baseOp);
	virtual ~TelOperatorApnInfo();
	virtual void *CloneObject() const;
	virtual bool IsValid() const { return true; }

	TelephonyResult SetApnCredentials(const TelephonyOperator &credentials);
	TelephonyResult GetApnCredentials(TelephonyOperator *outCredentials) const;
	TelephonyResult SetContractStringId(const paf::string &contractStringId);
	void SetBillingPlan(int billingPlan);
	const int &GetBillingPlan() const;
	void SetAutoSelectable();
	bool IsAutoSelectable() const { return m_autoSelectable; }
	const paf::string &GetContractStringId() const { return m_contractStringId; }

	void UnpackFromRaw(const TelOperatorApnRawRecord *raw);
	void PackToRaw(TelOperatorApnRawRecord *raw) const;

private:
	paf::string m_contractStringId;
	int m_billingPlan;
	bool m_autoSelectable;
};

class TelApnInfoList {
public:
	virtual ~TelApnInfoList() {}

	paf::vector<TelOperatorApnInfo> &GetEntries() { return m_entries; }
	const paf::vector<TelOperatorApnInfo> &GetEntries() const { return m_entries; }
	bool ContainsApnName(const paf::string &apnName) const;

private:
	paf::vector<TelOperatorApnInfo> m_entries;
};

class TelApnInfoListFactory {
public:
	virtual ~TelApnInfoListFactory();
	virtual TelephonyResult Create(const TelephonyPlmn &plmn, TelApnInfoList *outList);
};

class TelApnInfoListUtil {
public:
	virtual ~TelApnInfoListUtil();
	virtual TelephonyResult LoadForPlmn(const TelephonyPlmn &plmn, TelApnInfoList *outList);
};

/* APN list background downloader (0x813DCBEA, 0x813E6264..0x813E75BF) */
TelephonyResult telStartApnInfoListDownloader(void);

/* MCC/MNC utilities (0x813DF270..0x813DF9B7) */
class TelMccMncUtil {
public:
	TelMccMncUtil();
	virtual ~TelMccMncUtil();

	bool HasApnEntry(const paf::string &plmnDigits, SceSize prefixLen);
	TelephonyResult ResolveFromImsi(TelephonyPlmn *outPlmn, const paf::string &imsi);
	TelephonyResult ResolveFromNumeric(TelephonyPlmn *outPlmn, SceUInt16 mcc, SceUInt16 mnc);

private:
	bool m_initialized;
};

/* Network scan & network state objects (0x813DFDF8..0x813E0C77) */
struct TelNetScanRawRecord {
	int mcc;
	int mnc;
	int status;
	int accessTech;
	int roamingState;
	int signalQuality;
	int radioCapability;
	wchar_t operatorName[32];
};

class TelNetScanInfo : public TelObject {
public:
	TelNetScanInfo();
	TelNetScanInfo(const TelNetScanInfo &src);
	TelNetScanInfo &operator=(const TelNetScanInfo &src);
	virtual ~TelNetScanInfo();
	virtual bool IsValid() const { return true; }

	TelephonyResult SetPlmnCodes(SceUInt16 mcc, SceUInt16 mnc);
	TelephonyResult GetPlmn(TelephonyPlmn *outPlmn) const;
	TelephonyResult SetOperatorNameUtf8(const paf::string &nameUtf8);
	TelephonyResult SetOperatorName(const paf::wstring &name);
	TelephonyResult SetStatus(int status);
	TelephonyResult SetAccessTech(int accessTech);
	TelephonyResult SetRoamingState(int roamingState);
	TelephonyResult SetSignalQuality(int quality);
	TelephonyResult SetRadioCapability(int capability);
	int GetRadioCapability() const { return m_radioCapability; }

	void PackToRaw(TelNetScanRawRecord *outRaw) const;
	void UnpackFromRaw(const TelNetScanRawRecord *raw);

private:
	TelephonyPlmn m_plmn;
	paf::wstring m_operatorName;
	int m_status;
	int m_accessTech;
	TelRoamingInfo m_roamingInfo;
	int m_signalQuality;
	int m_radioCapability;
};

class TelNetworkInfo {
public:
	TelNetworkInfo();
	virtual ~TelNetworkInfo();
	virtual bool IsValid() const { return true; }

	TelephonyResult Reset();
	TelephonyResult SetNetworkType(int networkType);
	TelephonyResult GetMappedNetworkType(int *outType) const;
	TelephonyResult SetRegistrationState(int regState);
	TelephonyResult SetPlmnCodes(SceUInt16 mcc, SceUInt16 mnc);
	TelephonyResult GetPlmn(TelephonyPlmn *outPlmn) const;
	TelephonyResult SetSignalRssi(int rssi);
	TelephonyResult GetSignalRssi(int *outRssi) const;
	TelephonyResult SetSignalLevel(int level);
	TelephonyResult GetSignalLevel(int *outLevel) const;
	TelephonyResult SetRoamingState(int roamingState);
	TelephonyResult GetMappedRoamingState(int *outState) const;
	void SetOperatorName(const paf::string &name);

private:
	int m_networkType;
	int m_registrationState;
	SceUInt16 m_mcc;
	SceUInt16 m_mnc;
	int m_signalRssi;
	int m_signalLevel;
	int m_roamingState;
	paf::string m_operatorName;
};

/* Network code -> localized operator display name lookup (0x813E0240..0x813E09DB) */
class TelNetworkCodeUtil {
public:
	static paf::string FormatPlmnFallbackName(const TelephonyPlmn &plmn);
	static TelephonyResult LookupOperatorNames(const TelephonyPlmn &plmn,
						   paf::wstring *outFullName,
						   paf::wstring *outShortName);
};

/* Operator profile & factory (0x813E0C78..0x813E22DF) */
struct TelOperatorProfileRawRecord {
	int mcc;
	int mnc;
	int needSimEnable;
	int networkType;
	int billingPlan;
	char operatorName[32];
	char registrationUrl[256];
	char iconPath[64];
	char apnName[32];
	char apnUserId[32];
	char apnPassword[32];
};

class TelOperatorNetworkSetting {
public:
	TelOperatorNetworkSetting();
	virtual ~TelOperatorNetworkSetting();

	void SetNetworkType(int type) { m_networkType = type; }
	int GetNetworkType() const { return m_networkType; }
	void SetApnCredentials(const paf::string &apn, const paf::string &userId,
			       const paf::string &password);
	void GetApnCredentials(TelephonyOperator *outOp) const;
	void SetBillingPlan(int billingPlan) { m_billingPlan = billingPlan; }
	const int &GetBillingPlan() const { return m_billingPlan; }

private:
	int m_networkType;
	TelephonyOperator m_apn;
	int m_billingPlan;
};

class TelOperatorProfile : public TelObject {
public:
	TelOperatorProfile();
	virtual ~TelOperatorProfile();
	virtual paf::common::SharedPtr<TelObject> Clone() const;
	virtual bool IsValid() const { return true; }

	void SetPlmn(const TelephonyPlmn &plmn);
	void SetOperatorName(const paf::string &name) { m_operatorName = name; }
	void SetRegistrationUrl(const paf::string &url) { m_registrationUrl = url; }
	void SetIconPath(const paf::string &iconPath) { m_iconPath = iconPath; }
	void SetNeedSimEnable(bool needEnable) { m_needSimEnable = needEnable; }
	TelephonyResult SetNetworkSetting(const TelOperatorNetworkSetting &setting);
	void SetCallEndReasons(const paf::vector<TelCallEndReasonInfo> &reasons);
	const paf::vector<TelCallEndReasonInfo> &GetCallEndReasons() const { return m_callEndReasons; }
	void SetVersion(const paf::string &version) { m_version = version; }
	void GetVersion(paf::string *outVersion) const { *outVersion = m_version; }
	void PackToRaw(TelOperatorProfileRawRecord *outRaw) const;

private:
	TelephonyPlmn m_plmn;
	paf::string m_operatorName;
	paf::string m_registrationUrl;
	paf::string m_iconPath;
	TelOperatorNetworkSetting m_networkSetting;
	bool m_needSimEnable;
	paf::vector<TelCallEndReasonInfo> m_callEndReasons;
	paf::string m_version;
};

class TelOperatorProfileFactory {
public:
	TelOperatorProfileFactory(int operatorId, const TelephonyPlmn &plmn, SceUInt8 flags);
	virtual ~TelOperatorProfileFactory();

	void ParseReasonList(const paf::string &csvText, paf::vector<int> *outReasons) const;
	bool MatchesPlmnList(const paf::string &csvText, const TelephonyPlmn &plmn) const;
	TelephonyResult LoadProfiles(paf::vector<TelOperatorProfile> *outProfiles);
	TelephonyResult LoadSupportedPlmns(paf::vector<TelephonyPlmn> *outPlmns);

private:
	int m_operatorId;
	TelephonyPlmn m_plmn;
	bool m_checkPlmnMatch;
	int m_urlVariantIndex;
};

/* Phone number & PIN/PUK/CK validation objects (0x813E24B4..0x813E258F, 0x813E5D3C..0x813E61DB) */
class TelPhoneNumber : public TelObject {
public:
	TelPhoneNumber();
	explicit TelPhoneNumber(const char *digits);
	virtual ~TelPhoneNumber() {}
	virtual paf::common::SharedPtr<TelObject> Clone() const;
	virtual bool IsValid() const;
	virtual bool IsEmpty() const;

	void SetDigits(const char *digits);
	const char *GetDigits() const { return m_digits; }

	static bool ValidateDigits(const char *digits);

private:
	char m_digits[33];
};

class TelPinCode : public TelObject {
public:
	TelPinCode() : m_code("") {}
	explicit TelPinCode(const paf::string &code) : m_code(code) {}
	virtual ~TelPinCode() {}
	virtual bool IsValid() const;
	const paf::string &GetCode() const { return m_code; }

private:
	paf::string m_code;
};

class TelPukCode : public TelObject {
public:
	TelPukCode() : m_code("") {}
	explicit TelPukCode(const paf::string &code) : m_code(code) {}
	virtual ~TelPukCode() {}
	virtual bool IsValid() const;
	const paf::string &GetCode() const { return m_code; }

private:
	paf::string m_code;
};

class TelControlKey : public TelObject {
public:
	TelControlKey() : m_key("") {}
	explicit TelControlKey(const paf::string &key) : m_key(key) {}
	virtual ~TelControlKey() {}
	virtual bool IsValid() const;
	const paf::string &GetKey() const { return m_key; }

private:
	paf::string m_key;
};

class TelUnblockPinInfo : public TelObject {
public:
	TelUnblockPinInfo() {}
	TelUnblockPinInfo(const paf::string &puk, const paf::string &newPin)
		: m_puk(puk), m_newPin(newPin)
	{
	}
	virtual ~TelUnblockPinInfo() {}
	virtual bool IsValid() const;

	const TelPukCode &GetPuk() const { return m_puk; }
	const TelPinCode &GetNewPin() const { return m_newPin; }

private:
	TelPukCode m_puk;
	TelPinCode m_newPin;
};

class TelCarrierLockKey : public TelObject {
public:
	TelCarrierLockKey() : m_key("") {}
	explicit TelCarrierLockKey(const paf::string &key) : m_key(key) {}
	virtual ~TelCarrierLockKey() {}
	virtual bool IsValid() const;
	const paf::string &GetKey() const { return m_key; }

private:
	paf::string m_key;
};

/* SMS data models and GSM-7 / carrier SMS helpers (0x813E4C30..0x813E5D3B) */
class TelSmsMessageId : public TelObject {
public:
	TelSmsMessageId() : m_id(0) {}
	explicit TelSmsMessageId(SceUInt64 id) : m_id(id) {}
	TelSmsMessageId(const TelSmsMessageId &src) : m_id(src.m_id) {}
	virtual ~TelSmsMessageId() {}
	virtual paf::common::SharedPtr<TelObject> Clone() const;
	virtual bool IsValid() const { return m_id != 0; }

	SceUInt64 GetValue() const { return m_id; }
	void SetValue(SceUInt64 id) { m_id = id; }

private:
	SceUInt64 m_id;
};

class TelSmsAddress : public TelObject {
public:
	TelSmsAddress();
	TelSmsAddress(const TelPhoneNumber &number, const char *alphaLabel);
	virtual ~TelSmsAddress() {}
	virtual paf::common::SharedPtr<TelObject> Clone() const;
	virtual bool IsValid() const;
	virtual bool IsEmpty() const;

	void CopyFrom(const TelPhoneNumber &number, const char *alphaLabel);
	const TelPhoneNumber &GetNumber() const { return m_number; }
	const char *GetAlphaLabel() const { return m_alphaLabel; }

private:
	TelPhoneNumber m_number;
	char m_alphaLabel[33];
};

class TelSmsPeerInfo : public TelObject {
public:
	TelSmsPeerInfo(const TelSmsAddress &address, int status, bool unread, int peerFlags,
		       int deliveryResult);
	TelSmsPeerInfo(const TelSmsPeerInfo &src);
	virtual ~TelSmsPeerInfo() {}
	virtual bool IsValid() const;

	int GetStatus() const { return m_status; }
	void SetStatus(int status) { m_status = status; }
	int GetDeliveryResult() const { return m_deliveryResult; }
	void SetDeliveryResult(int result) { m_deliveryResult = result; }
	const TelSmsAddress &GetAddress() const { return m_address; }
	const TelDateTime &GetTimestamp() const { return m_timestamp; }

private:
	TelSmsAddress m_address;
	int m_status;
	int m_peerFlags;
	bool m_unread;
	TelDateTime m_timestamp;
	int m_deliveryResult;
};

class TelSmsMessage : public TelObject {
public:
	TelSmsMessage();
	TelSmsMessage(const TelSmsMessage &src);
	virtual ~TelSmsMessage() {}
	virtual paf::common::SharedPtr<TelObject> Clone() const;
	virtual bool IsValid() const;

	void SetReceivedData(const TelSmsMessageId &msgId, const TelSmsPeerInfo &peer,
			     const paf::wstring &body, const TelDateTime &historyTime);
	void SetSinglePeer(const TelSmsPeerInfo &peer);
	const TelSmsPeerInfo &GetFirstPeer() const;
	void SetPeers(const paf::vector<TelSmsPeerInfo> &peers);
	const paf::vector<TelSmsPeerInfo> &GetPeers() const { return m_peers; }
	void SetBody(const paf::wstring &body) { m_body = body; }
	const paf::wstring &GetBody() const { return m_body; }
	paf::wstring FormatDisplayBody() const;
	void SetHistoryTime(const TelDateTime &dt) { m_historyTime = dt; }
	void SetDeliverTime(const TelDateTime &dt) { m_deliverTime = dt; }
	TelDateTime GetDeliverTime() const { return m_deliverTime; }
	const TelDateTime &GetHistoryTime() const { return m_historyTime; }
	bool NeedsUcs2Encoding() const;
	bool IsConcatenated() const { return m_messageType == 2; }
	void SetConcatenated(bool concat) { m_messageType = concat ? 2 : 1; }
	void SetMessageKind(int kind) { m_messageKind = kind; }
	int GetMessageKind() const { return m_messageKind; }
	int GetMessageType() const { return m_messageType; }
	void SetProtocolId(int protocolId) { m_protocolId = protocolId; }
	void SetReferenceNumber(int refNum) { m_referenceNumber = refNum; }
	int GetReferenceNumber() const { return m_referenceNumber; }
	void SetMessageId(const TelSmsMessageId &id) { m_messageId = id; }
	const TelSmsMessageId &GetMessageId() const { return m_messageId; }

private:
	int m_messageKind;
	int m_messageType;
	int m_protocolId;
	int m_referenceNumber;
	int m_codingScheme;
	TelSmsMessageId m_messageId;
	paf::vector<TelSmsPeerInfo> m_peers;
	paf::wstring m_body;
	TelDateTime m_historyTime;
	TelDateTime m_deliverTime;
};

/* Builder / helper for carrier SMS formatting and GSM-7 encoding (0x813E5174..0x813E5CBB) */
enum TelGsm7Language {
	TEL_GSM7_LANG_DEFAULT = 0,
	TEL_GSM7_LANG_SPANISH = 1,
	TEL_GSM7_LANG_PORTUGUESE = 2
};

class TelSmsMessageHelper {
public:
	virtual ~TelSmsMessageHelper() {}

	static void SplitWString(const paf::wstring &input, paf::vector<paf::wstring> *outTokens,
				 const paf::wstring &delimiter);
	paf::wstring FormatCarrierMessage(const paf::wstring &rawMessage,
					  bool *outIsActivationNotice) const;

	static bool RequiresUcs2(const paf::wstring &text, TelGsm7Language lang);
	static bool RequiresUcs2Default(const paf::wstring &text);
	static SceUInt16 GetGsm7EncodedLength(const paf::wstring &text, TelGsm7Language lang);
	static SceUInt16 EncodeUtf16ToGsm7(const paf::wstring &text, TelGsm7Language lang,
					   SceUInt8 *outSeptets);
	static SceUInt16 DecodeGsm7ToUtf16(const SceUInt8 *septets, SceUInt16 septetCount,
					   int lockingShiftLang, int singleShiftLang,
					   wchar_t *outUtf16);
};

class TelSmsMessageBuilder {
public:
	explicit TelSmsMessageBuilder(TelSmsMessage *target) : m_target(target) {}
	virtual ~TelSmsMessageBuilder() {}

	void SetDirectionOutgoing(bool outgoing);
	void SetConcatenated(bool concat);
	void SetProtocolId(int protocolId);
	void SetReferenceNumber(int refNum);
	void MarkComplete();

private:
	TelSmsMessage *m_target;
};

/* Raw SMS PDU container (up to 256 bytes, 0x813E5CBC..0x813E5D3B) */
class TelSmsProtocolDataUnit {
public:
	TelSmsProtocolDataUnit() : m_length(0) {}
	TelSmsProtocolDataUnit(const TelSmsProtocolDataUnit &src);
	TelSmsProtocolDataUnit &operator=(const TelSmsProtocolDataUnit &src);

	TelephonyResult SetBytes(const void *bytes, SceSize length);
	TelephonyResult GetBytes(void *outBuf, SceSize maxLength) const;
	SceSize GetLength() const { return m_length; }
	const SceUInt8 *GetRawData() const { return m_bytes; }

private:
	SceSize m_length;
	SceUInt8 m_bytes[256];
};

/* Utility to find lowest set bit in a 32-bit mask (0x813DBD14) */
int telFindFirstSetBit(SceUInt32 mask);

} /* namespace telephony */

#endif /* TELEPHONY_COMMON_H */
