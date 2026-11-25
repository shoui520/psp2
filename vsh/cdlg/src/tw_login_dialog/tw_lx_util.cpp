/*
 * SceTwLoginDialogPlugin / SceTwitterDialogPlugin: Lx utility implementations.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <regmgr/regmgr.h>
#include <rtc.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/ctype.h>
#include <common/main_thread_call_list.h>
#include <common/string_util.h>
#include <memory/heap_allocator.h>
#include <vsh/paf/include/rtc/rtc.h>
#include <thread/thread.h>
#include "tw_lx.h"

extern "C" {
paf::memory::HeapAllocator *GetGlobalHeapAllocator();
SceUInt64 sceKernelGetProcessTimeWide(void);
SceUInt64 sceRtcGetAccumulativeTime(void);
}

using namespace paf;

/* Global state */
static memory::HeapAllocator *s_lxHeap = NULL;
static LxNodeList *s_lxTaskRoot = NULL;
static LxNodeList *s_lxRootParent = NULL;
static LxRegistry *s_lxRegistry = NULL;
static Plugin *s_lxPlugin = NULL;
static void *s_lxSystemInterface = NULL;
static uint64_t s_rtcBaseTime = 0;
static uint64_t s_rtcBaseAccum = 0;

static char s_lxEmptyStr[1] = { '\0' };
static wchar_t s_lxEmptyWStr[1] = { L'\0' };
static thread::Mutex s_fileBlockResumeMutex("SceLxFileBlockResumeOpen", 0);

/* Registry parameter mapping */
struct LxRegKeyEntry {
	int keyId;
	int regParamId;
};

static const LxRegKeyEntry s_regKeyTable[] = {
	{ 1, 0x005F6737 },
	{ 2, 0x0091F34F },
	{ 0, 0 },
};

/* Base64 alphabet */
static const char s_base64Chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Heap allocator */
memory::HeapAllocator *LxGetHeapAllocator()
{
	return s_lxHeap;
}

void LxInitHeapAllocator()
{
	s_lxHeap = GetGlobalHeapAllocator();
}

void LxTermHeapAllocator()
{
	s_lxHeap = NULL;
}

void *LxMalloc(SceSize size)
{
	if (s_lxHeap == NULL)
		return NULL;
	return s_lxHeap->sce_paf_malloc(size);
}

void *LxRealloc(void *ptr, SceSize size)
{
	if (s_lxHeap == NULL)
		return NULL;
	return s_lxHeap->sce_paf_realloc(ptr, size);
}

void LxFree(void *ptr)
{
	if (s_lxHeap != NULL && ptr != NULL)
		s_lxHeap->sce_paf_free(ptr);
}

/* LxRefCounted */
void LxRefCounted::AddRef()
{
	unsigned int val;

	__builtin_dmb();
	do {
		val = __builtin_ldrex(&m_refCount);
	} while (__builtin_strex(val + 1, &m_refCount) != 0);
	__builtin_dmb();
}

void LxRefCounted::Release()
{
	unsigned int val;

	__builtin_dmb();
	do {
		val = __builtin_ldrex(&m_refCount);
	} while (__builtin_strex(val - 1, &m_refCount) != 0);
	__builtin_dmb();
	if (val == 1)
		delete this;
}

/* LxBuffer */
bool LxBuffer::Resize(SceSize len)
{
	if (len == 0) {
		if (begin != NULL) {
			LxFree(begin);
			begin = NULL;
			end = NULL;
		}
		return true;
	}
	uint8_t *buf = (uint8_t *)LxRealloc(begin, len);

	if (buf == NULL)
		return false;
	begin = buf;
	end = buf + len;
	return true;
}

bool LxBuffer::Grow(SceSize len)
{
	SceSize curCap = Capacity();

	if (len <= curCap)
		return true;
	uint8_t *newBuf = NULL;
	uint8_t *newEnd = NULL;

	if (len != 0) {
		newBuf = (uint8_t *)LxRealloc(NULL, len);
		if (newBuf == NULL)
			return false;
		newEnd = newBuf + len;
	}
	if (begin != NULL && curCap != 0)
		sce_paf_memcpy(newBuf, begin, curCap);
	uint8_t *oldBuf = begin;

	begin = newBuf;
	end = newEnd;
	if (oldBuf != NULL)
		LxFree(oldBuf);
	return true;
}

/* LxString */
LxString::LxString()
	: m_data(s_lxEmptyStr), m_length(0), m_reserved(0)
{
}

LxString::LxString(const char *str)
	: m_data(s_lxEmptyStr), m_length(0), m_reserved(0)
{
	if (str != NULL)
		Assign(str, sce_paf_strlen(str));
}

LxString::LxString(const char *str, SceSize len)
	: m_data(s_lxEmptyStr), m_length(0), m_reserved(0)
{
	Assign(str, len);
}

LxString::LxString(const LxString &other)
	: m_data(s_lxEmptyStr), m_length(0), m_reserved(0)
{
	Assign(other.m_data, other.m_length);
}

LxString::~LxString()
{
	if (m_data != s_lxEmptyStr)
		LxFree(m_data);
	m_data = NULL;
	m_length = 0;
}

LxString &LxString::Assign(const char *str, SceSize len)
{
	if (m_data == str)
		return *this;
	if (m_data != s_lxEmptyStr)
		LxFree(m_data);
	if (str != NULL && len > 0) {
		char *buf = (char *)LxMalloc(len + 1);

		if (buf != NULL) {
			sce_paf_memcpy(buf, str, len);
			buf[len] = '\0';
			m_data = buf;
			m_length = len;
			return *this;
		}
	}
	m_data = s_lxEmptyStr;
	m_length = 0;
	return *this;
}

LxString &LxString::Append(const char *str, SceSize len)
{
	if (str == NULL || len == 0)
		return *this;
	SceSize newLen = m_length + len;
	char *buf = (char *)LxMalloc(newLen + 1);

	if (buf != NULL) {
		if (m_length > 0)
			sce_paf_memcpy(buf, m_data, m_length);
		sce_paf_memcpy(buf + m_length, str, len);
		buf[newLen] = '\0';
		if (m_data != s_lxEmptyStr)
			LxFree(m_data);
		m_data = buf;
		m_length = newLen;
	}
	return *this;
}

LxString &LxString::AppendChar(char ch)
{
	return Append(&ch, 1);
}

LxString LxString::Substr(SceSize pos, SceSize count) const
{
	if (pos >= m_length)
		return LxString();
	SceSize avail = m_length - pos;

	if (count > avail)
		count = avail;
	return LxString(m_data + pos, count);
}

bool LxString::operator==(const LxString &other) const
{
	if (m_length != other.m_length)
		return false;
	return sce_paf_strcmp(m_data, other.m_data) == 0;
}

/* LxWString */
LxWString::LxWString()
	: m_data(s_lxEmptyWStr), m_length(0), m_reserved(0)
{
}

LxWString::LxWString(const wchar_t *str, SceSize len)
	: m_data(s_lxEmptyWStr), m_length(0), m_reserved(0)
{
	Assign(str, len);
}

LxWString::LxWString(const LxWString &other)
	: m_data(s_lxEmptyWStr), m_length(0), m_reserved(0)
{
	Assign(other.m_data, other.m_length);
}

LxWString::~LxWString()
{
	if (m_data != s_lxEmptyWStr)
		LxFree(m_data);
	m_data = NULL;
	m_length = 0;
}

LxWString &LxWString::Assign(const wchar_t *str, SceSize len)
{
	if (m_data == str)
		return *this;
	if (m_data != s_lxEmptyWStr)
		LxFree(m_data);
	if (str != NULL && len > 0) {
		wchar_t *buf = (wchar_t *)LxMalloc((len + 1) * sizeof(wchar_t));

		if (buf != NULL) {
			sce_paf_memcpy(buf, str, len * sizeof(wchar_t));
			buf[len] = L'\0';
			m_data = buf;
			m_length = len;
			return *this;
		}
	}
	m_data = s_lxEmptyWStr;
	m_length = 0;
	return *this;
}

/* LxVariant */
LxVariant::LxVariant()
	: m_type(TYPE_NULL), m_padding(0)
{
	m_value.uintVal = 0;
}

LxVariant::LxVariant(const LxVariant &other)
	: m_type(TYPE_NULL), m_padding(0)
{
	m_value.uintVal = 0;
	CopyFrom(other);
}

LxVariant::~LxVariant()
{
	SetType(TYPE_NULL);
}

void LxVariant::SetType(int type)
{
	if (m_type == type)
		return;
	if (m_type == TYPE_STRING) {
		delete m_value.strVal;
	} else if (m_type == TYPE_LIST) {
		delete m_value.listVal;
	} else if (m_type == TYPE_MAP) {
		delete m_value.mapVal;
	}
	m_type = type;
	m_value.uintVal = 0;
	switch (type) {
	case TYPE_BOOL:
		m_value.boolVal = false;
		break;
	case TYPE_INT64:
	case TYPE_UINT64:
	case TYPE_DOUBLE:
		m_value.uintVal = 0;
		break;
	case TYPE_STRING:
		m_value.strVal = new LxString();
		break;
	case TYPE_LIST:
		m_value.listVal = new list<LxVariant>();
		break;
	case TYPE_MAP:
		m_value.mapVal = new map<LxString, LxVariant>();
		break;
	default:
		break;
	}
}

void LxVariant::SetString(const LxString &value)
{
	SetType(TYPE_STRING);
	if (m_value.strVal != NULL)
		*m_value.strVal = value;
}

void LxVariant::CopyFrom(const LxVariant &other)
{
	switch (other.m_type) {
	case TYPE_NULL:
		SetType(TYPE_NULL);
		break;
	case TYPE_BOOL:
		SetType(TYPE_BOOL);
		m_value.boolVal = other.m_value.boolVal;
		break;
	case TYPE_INT64:
		SetType(TYPE_INT64);
		m_value.intVal = other.m_value.intVal;
		break;
	case TYPE_UINT64:
		SetType(TYPE_UINT64);
		m_value.uintVal = other.m_value.uintVal;
		break;
	case TYPE_DOUBLE:
		SetType(TYPE_DOUBLE);
		m_value.doubleVal = other.m_value.doubleVal;
		break;
	case TYPE_STRING:
		SetType(TYPE_STRING);
		if (m_value.strVal != NULL && other.m_value.strVal != NULL)
			*m_value.strVal = *other.m_value.strVal;
		break;
	case TYPE_LIST:
		SetType(TYPE_LIST);
		if (m_value.listVal != NULL && other.m_value.listVal != NULL)
			*m_value.listVal = *other.m_value.listVal;
		break;
	case TYPE_MAP:
		SetType(TYPE_MAP);
		if (m_value.mapVal != NULL && other.m_value.mapVal != NULL)
			*m_value.mapVal = *other.m_value.mapVal;
		break;
	default:
		break;
	}
}

/* LxNode */
LxNode::LxNode(LxNode *parent, int tag)
	: m_tag(tag), m_parent(NULL), m_children(), m_reserved(0), m_active(0)
{
	if (parent != NULL)
		parent->InsertChild(this, 0x7FFFFFFF);
}

LxNode::~LxNode()
{
	DestroyChildren();
	if (m_parent != NULL)
		m_parent->RemoveChild(this);
}

void LxNode::DestroyChildren()
{
	for (int i = (int)m_children.size() - 1; i >= 0; --i) {
		LxNode *child = m_children[i];

		if (child != NULL) {
			child->m_parent = NULL;
			delete child;
		}
	}
	m_children.clear();
	OnChildrenCleared();
}

void LxNode::InsertChild(LxNode *child, int index)
{
	if (child == NULL)
		return;
	if (child->m_parent == this) {
		int curIdx = -1;

		for (size_t i = 0; i < m_children.size(); ++i) {
			if (m_children[i] == child) {
				curIdx = (int)i;
				break;
			}
		}
		if (curIdx < 0)
			return;
		int target = index;

		if (target < 0)
			target = 0;
		if (target >= (int)m_children.size())
			target = (int)m_children.size() - 1;
		if (curIdx != target) {
			if (curIdx < target) {
				for (int i = curIdx; i < target; ++i)
					m_children[i] = m_children[i + 1];
			} else {
				for (int i = curIdx; i > target; --i)
					m_children[i] = m_children[i - 1];
			}
			m_children[target] = child;
			OnChildAdded(child);
		}
		return;
	}
	if (child->m_parent != NULL)
		child->m_parent->RemoveChild(child);
	child->m_parent = this;
	int target = index;

	if (target < 0)
		target = 0;
	if (target > (int)m_children.size())
		target = (int)m_children.size();
	m_children.insert(m_children.begin() + target, child);
	OnChildAdded(child);
}

void LxNode::RemoveChild(LxNode *child)
{
	if (child == NULL || child->m_parent != this)
		return;
	for (size_t i = 0; i < m_children.size(); ++i) {
		if (m_children[i] == child) {
			m_children.erase(m_children.begin() + i);
			LxNode *next = (i < m_children.size()) ? m_children[i] : NULL;

			OnChildRemoved(child, next);
			break;
		}
	}
	child->m_parent = NULL;
}

/* LxNodeList */
LxNodeList::LxNodeList(LxNode *parent, int tag)
	: LxNode(parent, tag), m_iter(NULL)
{
}

void LxNodeList::OnUpdate()
{
	IterState state;

	state.item = NULL;
	state.adjusted = 0;
	state.current = (m_children.size() > 0) ? &m_children[0] : NULL;
	m_iter = &state;
	while (state.current != NULL && state.current != &m_children[0] + m_children.size()) {
		state.item = *state.current;
		state.item->OnUpdate();
		if (state.adjusted != 0)
			state.adjusted = 0;
		else
			++state.current;
	}
	m_iter = NULL;
}

void LxNodeList::OnChildAdded(LxNode *child)
{
	(void)child;
	if (m_iter == NULL || m_children.size() == 0)
		return;
	LxNode **base = &m_children[0];
	LxNode **end = base + m_children.size();
	LxNode **it = base;

	while (it != end && *it != m_iter->item)
		++it;
	m_iter->current = it;
}

void LxNodeList::OnChildRemoved(LxNode *child, LxNode *next)
{
	if (m_iter == NULL)
		return;
	LxNode **base = (m_children.size() > 0) ? &m_children[0] : NULL;
	LxNode **end = base + m_children.size();

	if (m_iter->item == child) {
		if (next != NULL && base != NULL) {
			LxNode **it = base;

			while (it != end && *it != next)
				++it;
			m_iter->current = it;
			m_iter->adjusted = 1;
			m_iter->item = next;
		} else {
			m_iter->current = end;
			m_iter->adjusted = 1;
			m_iter->item = NULL;
		}
	} else if (base != NULL) {
		LxNode **it = base;

		while (it != end && *it != m_iter->item)
			++it;
		m_iter->current = it;
	}
}

void LxNodeList::OnChildrenCleared()
{
	if (m_iter == NULL)
		return;
	m_iter->current = (m_children.size() > 0) ? &m_children[0] + m_children.size() : NULL;
	m_iter->adjusted = 1;
	m_iter->item = NULL;
}

/* LxAbortWatcher */
LxAbortWatcher::LxAbortWatcher(LxAbortCondition *condition, AbortCallback callback, void *userData,
			       bool oneShot)
	: m_condition(condition), m_callback(callback), m_userData(userData), m_oneShot(oneShot)
{
	common::MainThreadCallList::Register(OnMainThreadUpdate, this);
}

LxAbortWatcher::~LxAbortWatcher()
{
	common::MainThreadCallList::Unregister(OnMainThreadUpdate, this);
}

void LxAbortWatcher::OnMainThreadUpdate(void *arg)
{
	LxAbortWatcher *self = (LxAbortWatcher *)arg;

	if (self->m_condition->IsAborted() != 0) {
		if (self->m_oneShot)
			common::MainThreadCallList::Unregister(OnMainThreadUpdate, self);
		self->m_callback(self->m_userData);
	}
}

/* LxRegistry */
LxRegistry::LxRegistry()
	: m_reserved(0), m_mutex("SceTwitterLxRegMutexRegistry", 0), m_cache()
{
}

LxRegistry::~LxRegistry()
{
}

void LxRegistry::Init()
{
	s_lxRegistry = new LxRegistry();
}

void LxRegistry::Term()
{
	if (s_lxRegistry != NULL) {
		delete s_lxRegistry;
		s_lxRegistry = NULL;
	}
}

int LxRegistry::SetParamString(int keyId, const char *value)
{
	for (int i = 0; s_regKeyTable[i].regParamId != 0; ++i) {
		if (s_regKeyTable[i].keyId == keyId)
			return sceRegMgrSystemParamSetStr(s_regKeyTable[i].regParamId, value, 64);
	}
	return 0;
}

/* URL encoding / decoding (RFC 3986) */
static bool IsUrlUnreserved(unsigned char ch)
{
	if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))
		return true;
	return ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

static bool AppendBytesToBuffer(LxBuffer &buf, SceSize &used, const void *data, SceSize len)
{
	SceSize cap = buf.Capacity();

	if (len > cap - used) {
		SceSize newCap = (len > cap) ? (cap + len) : (cap * 2);

		if (!buf.Grow(newCap))
			return false;
	}
	sce_paf_memcpy(buf.begin + used, data, len);
	used += len;
	return true;
}

LxString LxUrlEncode(const char *src)
{
	static const char hexDigits[] = "0123456789ABCDEF";
	LxBuffer buf;
	SceSize used = 0;

	buf.Resize(256);
	if (src != NULL) {
		for (const unsigned char *p = (const unsigned char *)src; *p != 0; ++p) {
			unsigned char ch = *p;

			if (IsUrlUnreserved(ch)) {
				AppendBytesToBuffer(buf, used, &ch, 1);
			} else {
				char encoded[3];

				encoded[0] = '%';
				encoded[1] = hexDigits[(ch >> 4) & 0x0F];
				encoded[2] = hexDigits[ch & 0x0F];
				AppendBytesToBuffer(buf, used, encoded, 3);
			}
		}
	}
	return LxString((const char *)buf.begin, used);
}

LxString LxUrlDecode(const char *src)
{
	LxBuffer buf;
	SceSize used = 0;

	buf.Resize(256);
	if (src != NULL) {
		const char *p = src;

		while (*p != '\0') {
			char outCh;

			if (*p == '%' && (sce_paf_look_ctype_table((unsigned char)p[1]) & 0x44) != 0 &&
			    (sce_paf_look_ctype_table((unsigned char)p[2]) & 0x44) != 0) {
				char hex[3];

				hex[0] = p[1];
				hex[1] = p[2];
				hex[2] = '\0';
				outCh = (char)sce_paf_strtoul(hex, NULL, 16);
				p += 3;
			} else {
				outCh = *p++;
			}
			AppendBytesToBuffer(buf, used, &outCh, 1);
		}
	}
	return LxString((const char *)buf.begin, used);
}

LxString LxFormEncode(const char *src)
{
	static const char hexDigits[] = "0123456789ABCDEF";
	LxBuffer buf;
	SceSize used = 0;

	buf.Resize(256);
	if (src != NULL) {
		for (const unsigned char *p = (const unsigned char *)src; *p != 0; ++p) {
			unsigned char ch = *p;

			if (ch == ' ') {
				char plus = '+';

				AppendBytesToBuffer(buf, used, &plus, 1);
			} else if (IsUrlUnreserved(ch)) {
				AppendBytesToBuffer(buf, used, &ch, 1);
			} else {
				char encoded[3];

				encoded[0] = '%';
				encoded[1] = hexDigits[(ch >> 4) & 0x0F];
				encoded[2] = hexDigits[ch & 0x0F];
				AppendBytesToBuffer(buf, used, encoded, 3);
			}
		}
	}
	return LxString((const char *)buf.begin, used);
}

/* Base64 encoding */
size_t LxBase64Encode(char *dst, const uint8_t *src, size_t len)
{
	char *out = dst;
	uint32_t triple = 0;

	while (len >= 3) {
		triple = ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | src[2];
		out[0] = s_base64Chars[(triple >> 18) & 0x3F];
		out[1] = s_base64Chars[(triple >> 12) & 0x3F];
		out[2] = s_base64Chars[(triple >> 6) & 0x3F];
		out[3] = s_base64Chars[triple & 0x3F];
		src += 3;
		out += 4;
		len -= 3;
	}
	if (len != 0) {
		triple = (uint32_t)src[0] << 16;
		if (len > 1)
			triple |= (uint32_t)src[1] << 8;
		out[0] = s_base64Chars[(triple >> 18) & 0x3F];
		out[1] = s_base64Chars[(triple >> 12) & 0x3F];
		out[2] = (len > 1) ? s_base64Chars[(triple >> 6) & 0x3F] : '=';
		out[3] = '=';
		out += 4;
	}
	return (size_t)(out - dst);
}

/* Time and number formatting */
SceUInt64 LxGetProcessTime()
{
	return sceKernelGetProcessTimeWide();
}

LxString LxIntToString(int value)
{
	char buf[32];

	sce_paf_snprintf(buf, sizeof(buf), "%d", value);
	return LxString(buf, sce_paf_strlen(buf));
}

LxString LxUInt64ToString(SceUInt64 value)
{
	char buf[64];

	sce_paf_snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
	return LxString(buf, sce_paf_strlen(buf));
}

void LxUtf8ToWString(LxWString &dst, const LxString &src)
{
	wstring wide;

	common::Utf8ToUtf16(src.CStr(), &wide, 0);
	dst.Assign(wide.c_str(), wide.size());
}

/* String splitting and key=value parsing */
void LxSplitString(list<LxString> &outList, const LxString &src, SceSize startPos, char delim)
{
	const char *str = src.CStr();
	SceSize len = src.Length();
	SceSize pos = startPos;

	while (pos < len && str[pos] == delim)
		++pos;
	while (pos < len) {
		const char *found = sce_paf_strchr(str + pos, delim);

		if (found == NULL) {
			outList.push_back(src.Substr(pos, len - pos));
			break;
		}
		SceSize nextPos = (SceSize)(found - str);

		outList.push_back(src.Substr(pos, nextPos - pos));
		pos = nextPos + 1;
		while (pos < len && str[pos] == delim)
			++pos;
	}
}

void LxParseKeyValue(map<LxString, LxString> &outMap, const LxString &src, SceSize startPos,
		     char pairDelim, char kvDelim)
{
	list<LxString> parts;

	LxSplitString(parts, src, startPos, pairDelim);
	for (list<LxString>::iterator it = parts.begin(); it != parts.end(); ++it) {
		const char *itemStr = it->CStr();
		const char *eq = sce_paf_strchr(itemStr, kvDelim);

		if (eq != NULL) {
			SceSize eqPos = (SceSize)(eq - itemStr);
			LxString key = it->Substr(0, eqPos);
			LxString val = it->Substr(eqPos + 1, it->Length() - eqPos - 1);

			outMap[key] = val;
		} else {
			outMap[*it] = LxString();
		}
	}
}

/* RTC helpers */
int LxConvertDateTimeToTick(const void *dateTime, SceUInt64 *tick)
{
	return ::paf::rtc::ConvertDateTimeToTick((const ::paf::rtc::DateTime *)dateTime,
						 (::paf::rtc::Tick *)tick);
}

int LxTickAddSeconds(SceUInt64 *dstTick, const SceUInt64 *srcTick, SceInt64 seconds)
{
	return ::paf::rtc::TickAddSeconds((::paf::rtc::Tick *)dstTick,
					  (const ::paf::rtc::Tick *)srcTick, seconds);
}

int LxGetCurrentTick(SceUInt64 *tick)
{
	if (s_rtcBaseAccum == 0)
		return ::paf::rtc::GetCurrentTick((::paf::rtc::Tick *)tick);
	if (tick != NULL)
		*tick = s_rtcBaseTime + sceRtcGetAccumulativeTime() - s_rtcBaseAccum;
	return 0;
}

/* System plugin and task root */
void LxInitSystemPlugin(Plugin *plugin)
{
	s_lxPlugin = plugin;
	Plugin *sysPlugin = Plugin::Find("system_plugin");

	s_lxSystemInterface = (sysPlugin != NULL) ? sysPlugin->GetInterface(1) : NULL;
}

void LxTermSystemPlugin()
{
	s_lxPlugin = NULL;
}

const wchar_t *LxGetLocalizedString(const IDParam &id)
{
	return s_lxPlugin->GetString(id);
}

void LxInitTaskRoot()
{
	if (s_lxTaskRoot == NULL)
		s_lxTaskRoot = new LxNodeList(s_lxRootParent, 0);
}

void LxTermTaskRoot()
{
	if (s_lxTaskRoot != NULL) {
		delete s_lxTaskRoot;
		s_lxTaskRoot = NULL;
	}
}
