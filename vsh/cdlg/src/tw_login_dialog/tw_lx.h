/*
 * SceTwLoginDialogPlugin / SceTwitterDialogPlugin: shared Lx utility types (heap, strings,
 * buffers, URL/Base64 codecs, variant, tree node, registry, RTC time, abort watcher).
 */
#ifndef TW_LX_H
#define TW_LX_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <plugin.h>
#include <memory/heap_allocator.h>
#include <thread/thread.h>
#include <std/string.h>
#include <std/string>
#include <std/list>
#include <std/map>
#include <std/vector>

/* Lx error codes (0xA000xxxx) */
#define LX_ERROR_HTTP_STATUS		((SceInt32)0xA0000301)
#define LX_ERROR_HTTP_NO_MEM		((SceInt32)0xA0000302)
#define LX_ERROR_OAUTH_NO_TOKEN		((SceInt32)0xA0000304)
#define LX_ERROR_OAUTH_UNAUTHORIZED	((SceInt32)0xA0000305)
#define LX_ERROR_NET_401		((SceInt32)0xA0000401)
#define LX_ERROR_NET_405		((SceInt32)0xA0000405)
#define LX_ERROR_HTTP_NO_MEM_POOL	((SceInt32)0xA0000801)
#define LX_ERROR_HTTP_ABORTED		((SceInt32)0xA0000804)
#define LX_ERROR_HTTP_NO_TEMPLATE	((SceInt32)0xA0000805)
#define LX_ERROR_HTTP_NO_POOL		((SceInt32)0xA0000806)

/* Global Lx heap allocator (set to paf::GetGlobalHeapAllocator()) */
paf::memory::HeapAllocator *LxGetHeapAllocator();
void LxInitHeapAllocator();
void LxTermHeapAllocator();
void *LxMalloc(SceSize size);
void *LxRealloc(void *ptr, SceSize size);
void LxFree(void *ptr);

/* Ref-counted base with atomic Cortex-A9 ldrex/strex increment/decrement */
class LxRefCounted {
public:
	LxRefCounted() : m_refCount(0) {}
	virtual ~LxRefCounted() {}

	void AddRef();
	void Release();

	volatile unsigned int m_refCount;
};

template <typename T>
class LxRefPtr {
public:
	LxRefPtr() : m_ptr(NULL) {}
	explicit LxRefPtr(T *ptr) : m_ptr(ptr)
	{
		if (m_ptr != NULL)
			m_ptr->AddRef();
	}
	LxRefPtr(const LxRefPtr &other) : m_ptr(other.m_ptr)
	{
		if (m_ptr != NULL)
			m_ptr->AddRef();
	}
	~LxRefPtr()
	{
		if (m_ptr != NULL)
			m_ptr->Release();
	}

	LxRefPtr &operator=(T *ptr)
	{
		if (m_ptr != ptr) {
			if (ptr != NULL)
				ptr->AddRef();
			T *old = m_ptr;
			m_ptr = ptr;
			if (old != NULL)
				old->Release();
		}
		return *this;
	}

	LxRefPtr &operator=(const LxRefPtr &other) { return (*this = other.m_ptr); }
	T *Get() const { return m_ptr; }
	T *operator->() const { return m_ptr; }
	bool IsNull() const { return m_ptr == NULL; }

	T *m_ptr;
};

/* Heap-backed byte buffer */
struct LxBuffer {
	uint8_t *begin;
	uint8_t *end;

	LxBuffer() : begin(NULL), end(NULL) {}
	~LxBuffer() { Resize(0); }

	bool Resize(SceSize len);
	bool Grow(SceSize len);
	SceSize Capacity() const { return (begin != NULL) ? (SceSize)(end - begin) : 0; }
};

/* C++ string backed by LxMalloc/LxFree */
class LxString {
public:
	LxString();
	LxString(const char *str);
	LxString(const char *str, SceSize len);
	LxString(const LxString &other);
	~LxString();

	LxString &operator=(const LxString &other) { return Assign(other.m_data, other.m_length); }
	LxString &Assign(const char *str, SceSize len);
	LxString &Append(const char *str, SceSize len);
	LxString &Append(const LxString &other) { return Append(other.m_data, other.m_length); }
	LxString &AppendChar(char ch);
	LxString Substr(SceSize pos, SceSize count) const;

	const char *CStr() const { return m_data; }
	SceSize Length() const { return m_length; }
	bool Empty() const { return m_length == 0; }

	bool operator<(const LxString &other) const
	{
		return sce_paf_strcmp(m_data, other.m_data) < 0;
	}
	bool operator==(const LxString &other) const;

	char *m_data;
	SceSize m_length;
	SceSize m_reserved;
};

/* Wide string backed by LxMalloc/LxFree */
class LxWString {
public:
	LxWString();
	LxWString(const wchar_t *str, SceSize len);
	LxWString(const LxWString &other);
	~LxWString();

	LxWString &Assign(const wchar_t *str, SceSize len);
	const wchar_t *CStr() const { return m_data; }
	SceSize Length() const { return m_length; }

	wchar_t *m_data;
	SceSize m_length;
	SceSize m_reserved;
};

/* Tagged value (null, bool, int64, uint64, double, string, list, map) */
class LxVariant {
public:
	enum Type {
		TYPE_NULL = 0,
		TYPE_BOOL = 1,
		TYPE_INT64 = 2,
		TYPE_UINT64 = 3,
		TYPE_DOUBLE = 4,
		TYPE_STRING = 5,
		TYPE_LIST = 6,
		TYPE_MAP = 7
	};

	LxVariant();
	LxVariant(const LxVariant &other);
	~LxVariant();

	void SetType(int type);
	void SetString(const LxString &value);
	void CopyFrom(const LxVariant &other);

	int m_type;
	uint32_t m_padding;
	union {
		bool boolVal;
		int64_t intVal;
		uint64_t uintVal;
		double doubleVal;
		LxString *strVal;
		paf::list<LxVariant> *listVal;
		paf::map<LxString, LxVariant> *mapVal;
	} m_value;
};

/* Parent/child tree node */
class LxNode {
public:
	LxNode(LxNode *parent, int tag);
	virtual ~LxNode();

	virtual void DestroyChildren();
	virtual void InsertChild(LxNode *child, int index);
	virtual void RemoveChild(LxNode *child);
	virtual void OnChildAdded(LxNode *child) {}
	virtual void OnChildRemoved(LxNode *child, LxNode *next) {}
	virtual void OnChildrenCleared() {}
	virtual void OnUpdate() {}

	int m_tag;
	LxNode *m_parent;
	paf::vector<LxNode *> m_children;
	int m_reserved;
	uint8_t m_active;
};

/* Safe-iteration node container */
class LxNodeList : public LxNode {
public:
	struct IterState {
		LxNode **current;
		uint8_t adjusted;
		LxNode *item;
	};

	LxNodeList(LxNode *parent, int tag);
	virtual ~LxNodeList() {}

	virtual void OnChildAdded(LxNode *child);
	virtual void OnChildRemoved(LxNode *child, LxNode *next);
	virtual void OnChildrenCleared();
	virtual void OnUpdate();

	IterState *m_iter;
};

/* Abort condition interface polled on main thread */
class LxAbortCondition {
public:
	virtual ~LxAbortCondition() {}
	virtual int IsAborted() = 0;
};

/* Polls LxAbortCondition on MainThreadCallList and invokes callback once aborted */
class LxAbortWatcher {
public:
	typedef void (*AbortCallback)(void *userData);

	LxAbortWatcher(LxAbortCondition *condition, AbortCallback callback, void *userData,
		       bool oneShot);
	~LxAbortWatcher();

	static void OnMainThreadUpdate(void *arg);

	LxAbortCondition *m_condition;
	AbortCallback m_callback;
	void *m_userData;
	bool m_oneShot;
};

/* Twitter registry store (SceTwitterLxRegMutexRegistry) */
class LxRegistry {
public:
	LxRegistry();
	virtual ~LxRegistry();

	static void Init();
	static void Term();
	static int SetParamString(int keyId, const char *value);

	int m_reserved;
	paf::thread::Mutex m_mutex;
	LxVariant m_cache;
};

/* Encoding, string, time, and system plugin helpers */
LxString LxUrlEncode(const char *src);
LxString LxUrlDecode(const char *src);
LxString LxFormEncode(const char *src);
size_t LxBase64Encode(char *dst, const uint8_t *src, size_t len);
SceUInt64 LxGetProcessTime();
LxString LxIntToString(int value);
LxString LxUInt64ToString(SceUInt64 value);
void LxUtf8ToWString(LxWString &dst, const LxString &src);

void LxSplitString(paf::list<LxString> &outList, const LxString &src, SceSize startPos,
		   char delim);
void LxParseKeyValue(paf::map<LxString, LxString> &outMap, const LxString &src, SceSize startPos,
		     char pairDelim, char kvDelim);

int LxConvertDateTimeToTick(const void *dateTime, SceUInt64 *tick);
int LxTickAddSeconds(SceUInt64 *dstTick, const SceUInt64 *srcTick, SceInt64 seconds);
int LxGetCurrentTick(SceUInt64 *tick);

void LxInitSystemPlugin(paf::Plugin *plugin);
void LxTermSystemPlugin();
const wchar_t *LxGetLocalizedString(const paf::IDParam &id);

void LxInitTaskRoot();
void LxTermTaskRoot();

#endif /* TW_LX_H */
