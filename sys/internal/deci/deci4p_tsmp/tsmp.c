/*
 * SceDeci4pTsmp (Sony's tsmp.c): see tsmp.h.
 */
#include <scetypes.h>
#include <sysmem/memblock.h>
#include <sysmem/uid_guid.h>
#include <sysmem/dipsw.h>
#include <rtc/rtc.h>
#include <sysmem/sysmem.h>
#include <string.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <kernel/modulemgr.h>
#include "tsmp.h"
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <regmgr/regmgr.h>

#define SCE_ERROR_ERRNO_ENODATA			0x8001003d
#define SCE_KERNEL_ERROR_INVALID_FLAGS		0x8002000a
#define SCE_KERNEL_ERROR_NO_MEMORY		0x8002710b

#define TSMP_DIPSW			0xc2

/* Sony's build inlines every helper into its callers */
#define TSMP_INLINE		static inline __attribute__((always_inline))
#define TSMP_MEMTYPE			SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW	/* kernel read/write */

/* 4.00 is built with _FORTIFY_SOURCE: copies into fixed buffers are checked */
#if FW_VERSION >= 0x04000000
/* called by name: the compiler would fold the builtin, the length being bounded */
#define tsmpMemcpyChk(dst, src, n) \
	__memcpy_chk(dst, src, n, sizeof(dst))	/* dst: an array */
#else
#define tsmpMemcpyChk(dst, src, n)	memcpy(dst, src, n)
#endif

/* SceSysrootForDriver: the software information hooks other modules call */

static TsmpState tsmp;

/* Name and caller unknown; this retained original function returns success. */
static int __attribute__((used)) tsmpReturnSuccess(void)
{
	return 0;
}

/* ===== status and buffers ===== */

TSMP_INLINE void tsmpSetSystemStatus(SceUInt32 bits, int set)
{
	int ret;

	ret = lockFastMutex(&tsmp.statusLock);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3454, "0x%08X\n", ret);
		return;
	}
	if (set)
		tsmp.status |= bits;
	else
		tsmp.status &= ~bits;
	sceKernelUnlockFastMutex(&tsmp.statusLock);
}

TSMP_INLINE void tsmpSetBufStatus(TsmpBuffer *buf, SceUInt32 bits, int set)
{
	int ret;

	ret = lockFastMutex(&buf->lock);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3484, "0x%08X\n", ret);
		return;
	}
	if (set)
		buf->status |= bits;
	else
		buf->status &= ~bits;
	sceKernelUnlockFastMutex(&buf->lock);
}

TSMP_INLINE void *tsmpAllocSendBuf(void)
{
	if (tsmp.send.status & TSMP_BUF_BUSY) {
		SCE_PRINTF_WITH_INFO(0xf, 3557, "\n");
		return NULL;
	}
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_BUSY, 1);
	return tsmp.send.base;
}

TSMP_INLINE void tsmpFreeSendBuf(void)
{
	if (!(tsmp.send.status & TSMP_BUF_BUSY)) {
		SCE_PRINTF_WITH_INFO(0xf, 3578, "\n");
		return;
	}
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_BUSY | TSMP_BUF_FILLED | TSMP_BUF_HEADER, 0);
}

/* the first 16 bytes (the DECI4P header) are filled by SceDeci4pDfMgr */
TSMP_INLINE void tsmpSetHeader(TsmpHeader *pkt, SceUInt8 type, SceUInt8 code,
			  SceUInt32 id0, SceUInt32 id1)
{
	TsmpHeader header;

	header.type = type;
	header.code = code;
	header.count = 0;
	header.id[0] = id0;
	header.id[1] = id1;
	__builtin_memcpy(pkt, &header, sizeof(header));
}

/* hand the send buffer to SceDeci4pDfMgr; on failure release it again */
#define TSMP_REQ_SEND(_line) do {						\
	int _ret;								\
	tsmpSetSystemStatus(TSMP_STATUS_SENDING, 1);				\
	_ret = sceDfMgrReqSend(tsmp.handle);					\
	if (_ret < 0 && _ret != (int)DFMGR_ERROR_BUSY) {	\
		SCE_PRINTF_WITH_INFO(0xf, _line, "0x%08X\n", _ret);		\
		tsmpFreeSendBuf();						\
		tsmpSetSystemStatus(TSMP_STATUS_SENDING, 0);			\
	}									\
} while (0)

/* ===== notification queues ===== */

TSMP_INLINE int tsmpInitNotification(TsmpNotificationQueue *queue, const char *heapName)
{
	SceKernelHeapCreateOpt opt;
	int ret;

	ret = sceKernelInitializeFastMutex(&queue->lock, "SceDeci4pTsmpNtfcBuf", 0, 0);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3623, "0x%08X\n", ret);
		return ret;
	}
	opt.size = sizeof(opt);
	opt.attr = 0x401;
	opt.memtype = TSMP_MEMTYPE;
	ret = SceSysmemForDriver_9328E0E8(heapName, 0x4000, &opt);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3641, "0x%08X\n", ret);
		return ret;
	}
	queue->count = 0;
	queue->peak = 0;
	queue->heap = ret;
	return 0;
}

/* a registry burst grew the heap: start a fresh one */
TSMP_INLINE int tsmpResetNotificationForRegistry(TsmpNotificationQueue *queue)
{
	SceKernelHeapCreateOpt opt;
	int ret;

	ret = sceKernelDeleteHeap(queue->heap);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(0xf, 3668, "0x%08X\n", ret);
	opt.size = sizeof(opt);
	opt.attr = 0x401;
	opt.memtype = TSMP_MEMTYPE;
	ret = SceSysmemForDriver_9328E0E8("SceDeci4pTsmpRegistry", 0x4000, &opt);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3678, "0x%08X\n", ret);
		return ret;
	}
	queue->heap = ret;
	queue->count = 0;
	queue->peak = 0;
	return 0;
}

TSMP_INLINE int tsmpPushNotificaiton(TsmpNotificationQueue *queue, const RegMgrNotifyInfo *ntf)
{
	TsmpNotificationNode *node;
	int ret;

	ret = lockFastMutex(&queue->lock);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3730, "0x%08X\n", ret);
		return ret;
	}
	node = SceSysmemForDriver_7B4CB60A(queue->heap, sizeof(*node));
	if (node == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 3738, "\n");
		sceKernelUnlockFastMutex(&queue->lock);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	if (queue->head == NULL)
		queue->head = node;
	else
		queue->tail->next = node;
	node->next = NULL;
	queue->tail = node;
	memcpy(&node->ntf, ntf, sizeof(node->ntf));
	if (++queue->count > queue->peak)
		queue->peak = queue->count;
	sceKernelUnlockFastMutex(&queue->lock);
	return 0;
}

/* SceRegMgrForDebugger callback: a registry key changed */
static int tsmpPushRegistryNotification(RegMgrNotifyInfo *entry)
{
	int ret;

	ret = tsmpPushNotificaiton(&tsmp.registry, entry);
	if (ret < 0)
		return ret;
	if (!(tsmp.status & TSMP_STATUS_SYNCING)) {
		ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_REG);
		return ret < 0 ? ret : 0;
	}
	return 0;
}

/* move the oldest notification to tsmp.current */
__attribute__((noinline)) static int tsmpPopNotification(TsmpNotificationQueue *queue)
{
	TsmpNotificationNode *node;
	int peak, ret;

	ret = lockFastMutex(&queue->lock);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3810, "0x%08X\n", ret);
		return ret;
	}
	if (queue->count < 1 || (node = queue->head) == NULL) {
		ret = SCE_ERROR_ERRNO_ENODATA;
	} else {
		memcpy(&tsmp.current, &node->ntf, sizeof(tsmp.current));
		queue->count--;
		queue->head = node->next;
		if (queue->head == NULL) {
			queue->tail = NULL;
			peak = queue->peak;
			ret = sceKernelFreeHeapMemory(queue->heap, node);
			if (ret < 0)
				SCE_PRINTF_WITH_INFO(0xf, 3856, "\n");
			else if (peak > 8) {
				ret = tsmpResetNotificationForRegistry(queue);
				if (ret < 0)
					SCE_PRINTF_WITH_INFO(0xf, 3865, "\n");
			}
		} else {
			ret = sceKernelFreeHeapMemory(queue->heap, node);
			if (ret < 0)
				SCE_PRINTF_WITH_INFO(0xf, 3856, "\n");
		}
	}
	sceKernelUnlockFastMutex(&queue->lock);
	return ret;
}

static inline int tsmpSyncLocalTime(void)
{
	int ret;

	if (tsmp.status & TSMP_STATUS_SYNCING)
		return 0;
	ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_TICK);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(0xf, 1064, "0x%08X\n", ret);
	return 0;
}

/*
 * Append tsmp.current as a registry entry:
 *	SceUInt32 size;		(whole entry)
 *	SceUInt32 nameLength;
 *	char name[];		(padded to words)
 *	SceUInt32 kind;		(length | type << 16: 0 int, 1 binary, 2 string)
 *	value;			(padded to words)
 */
__attribute__((noinline)) static int tsmpConvertRegMgrToTsmp(SceUInt32 *out)
{
	const RegMgrNotifyInfo *ntf = &tsmp.current;
	SceUInt32 nameWords, kind, valueBytes, length;
	int size;

	if (ntf->size != sizeof(*ntf)) {
		/* Sony passes no value for the %d */
		SCE_PRINTF_WITH_INFO(0xf, 3330, "%d\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	length = strnlen(ntf->path, sizeof(ntf->path));
	out[1] = length + 1;
	memcpy(&out[2], ntf->path, length);	/* out is zeroed: the NUL is there */
	nameWords = (out[1] + 3) >> 2;

	switch (ntf->type) {
	case TSMP_VALUE_STR:
		length = ntf->valueSize;
		if (length)
			memcpy(&out[nameWords + 3], ntf->data, length);
		((char *)&out[nameWords + 3])[length] = '\0';
		valueBytes = length + 4;
		kind = ((length + 1) & 0xffff) | 0x20000;
		break;
	case TSMP_VALUE_INT:
		out[nameWords + 3] = ntf->value;
		valueBytes = 4;
		kind = 4;
		break;
	case TSMP_VALUE_BIN:
		length = ntf->valueSize;
		if (length == 0) {
			valueBytes = 0;
			kind = 0x10000;
			break;
		}
		memcpy(&out[nameWords + 3], ntf->data, length);
		valueBytes = length + 3;
		kind = length | 0x10000;
		break;
	default:
		SCE_PRINTF_WITH_INFO(0xf, 3391, "%d\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	out[nameWords + 2] = kind;
	size = ((valueBytes >> 2) + nameWords + 3) * 4;
	out[0] = size;
	if (ntf->type == TSMP_VALUE_INT
	    && strncmp(ntf->path, "/CONFIG/DATE/time_zone", sizeof(ntf->path)) == 0)
		tsmpSyncLocalTime();
	return size;
}

/* ===== software information (SceSysrootForDriver hooks) ===== */

TSMP_INLINE int tsmpQueueSwInfo(RegMgrNotifyInfo *ntf)
{
	if (tsmpPushNotificaiton(&tsmp.swInfo, ntf) >= 0)
		sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_SWINFO);
	return sceKernelFreeHeapMemory(tsmp.miscHeap, ntf);
}

static int tsmpSetSwInfoInt(const char *name, int id, SceUInt32 value)
{
	RegMgrNotifyInfo *ntf;
	int ret;

	if (name == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 827, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (id >= TSMP_SWINFO_ID_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 831, "%d\n", id);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ntf = SceSysmemForDriver_7B4CB60A(tsmp.miscHeap, sizeof(*ntf));
	if (ntf == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 845, "\n");
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ntf->size = sizeof(*ntf);
	ntf->value = value;
	ntf->type = TSMP_VALUE_INT;
	ntf->valueSize = sizeof(ntf->value);
	strncpy(ntf->path, name, sizeof(ntf->path));
	ntf->path[sizeof(ntf->path) - 1] = '\0';
	ret = tsmpQueueSwInfo(ntf);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 878, "\n");
		return ret;
	}
	return ret;
}

static int tsmpSetSwInfoStr(const char *name, int id, const char *value, int length)
{
	RegMgrNotifyInfo *ntf;
	int ret;

	if (name == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 896, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (id >= TSMP_SWINFO_ID_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 900, "%d\n", id);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (value == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 904, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (length >= TSMP_SWINFO_LEN_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 908, "%d\n", length);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ntf = SceSysmemForDriver_7B4CB60A(tsmp.miscHeap, sizeof(*ntf));
	if (ntf == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 922, "\n");
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ntf->valueSize = length + 1;
	ntf->size = sizeof(*ntf);
	ntf->type = TSMP_VALUE_STR;
	ntf->value = 0;
	strncpy(ntf->path, name, sizeof(ntf->path));
	ntf->path[sizeof(ntf->path) - 1] = '\0';
	strncpy(ntf->data, value, sizeof(ntf->data));
	ntf->data[sizeof(ntf->data) - 1] = '\0';
	ret = tsmpQueueSwInfo(ntf);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 957, "\n");
		return ret;
	}
	return ret;
}

static int tsmpSetSwInfoBin(const char *name, int id, const void *value, SceSize length)
{
	RegMgrNotifyInfo *ntf;
	int ret;

	if (name == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 975, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (id >= TSMP_SWINFO_ID_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 979, "%d\n", id);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (value == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 983, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (length >= TSMP_SWINFO_LEN_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 988, "%d\n", length);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ntf = SceSysmemForDriver_7B4CB60A(tsmp.miscHeap, sizeof(*ntf));
	if (ntf == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 1002, "\n");
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ntf->size = sizeof(*ntf);
	ntf->type = TSMP_VALUE_BIN;
	ntf->valueSize = length + 1;
	ntf->value = 0;
	strncpy(ntf->path, name, sizeof(ntf->path));
	ntf->path[sizeof(ntf->path) - 1] = '\0';
	tsmpMemcpyChk(ntf->data, value, length);
	ntf->data[length] = '\0';
	ret = tsmpQueueSwInfo(ntf);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 1037, "\n");
		return ret;
	}
	return ret;
}

/* ===== SceDeci4pDfMgr ===== */

TSMP_INLINE int tsmpEventRecv(SceSize size)
{
	char *p;
	int left, n;

	if (tsmp.recv.status & TSMP_BUF_BUSY) {
		sceKernelDelayThread(100000);
		return 0;
	}
	tsmpSetBufStatus(&tsmp.recv, TSMP_BUF_BUSY, 1);
	p = tsmp.recv.base;
	if (p == NULL) {
		sceKernelDelayThread(100000);
		return 0;
	}
	for (left = size; left > 0; left -= n, p += n) {
		n = sceDfMgrRecv(tsmp.handle, p, size);
		if (n < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 655, "0x%08X\n", n);
			return n;
		}
	}
	tsmp.recv.length = size;
	return 0;
}

TSMP_INLINE int tsmpEventSend(void)
{
	const char *p;
	int left, n;

	left = tsmp.send.length;
	p = tsmp.send.base;
	if (!(tsmp.send.status & TSMP_BUF_BUSY)
	    || (tsmp.send.status & (TSMP_BUF_FILLED | TSMP_BUF_HEADER))
	       != (TSMP_BUF_FILLED | TSMP_BUF_HEADER)
	    || left == 0) {
		SCE_PRINTF_WITH_INFO(0xf, 706, "0x%08X:%d\n", tsmp.send.status, tsmp.send.length);
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	}
	SceDeci4pDfMgrForDebugger_6D26CC56(p, left);
	for (; left > 0; left -= n, p += n) {
		n = sceDfMgrSend(tsmp.handle, p, left);
		if (n < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 730, "0x%08X\n", n);
			return n;
		}
	}
	return 0;
}

static int tsmpEventHandler(int event, void *arg, void *ctx)
{
	int ret;

	(void)ctx;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		ret = tsmpEventRecv((SceSize)arg);
		break;
	case SCE_DFMGR_EVENT_REQ_DONE:
		ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_RECV);
		if (ret >= 0)
			return 0;
		break;
	case SCE_DFMGR_EVENT_SEND:
		ret = tsmpEventSend();
		break;
	case SCE_DFMGR_EVENT_SENT:
		tsmpFreeSendBuf();
		ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_SENT);
		ret = ret < 0 ? ret : 0;
		break;
	case SCE_DFMGR_EVENT_STATUS:
	case 6:
		return 0;
	default:
		SCE_PRINTF_WITH_INFO(0xf, 617, "0x%08X\n", event);
		ret = SCE_KERNEL_ERROR_UNSUP;
		break;
	}
	if (ret != 0)
		SCE_PRINTF_WITH_INFO(0xf, 623, "0x%08X:0x%08X\n", event, ret);
	return 0;
}

/* reply with an error result and no data */
__attribute__((noinline)) static void tsmpSendError(SceUInt8 type, SceUInt8 code, SceUInt32 result)
{
	TsmpPacket *pkt;

	pkt = tsmpAllocSendBuf();
	if (pkt == NULL) {
		tsmpFreeSendBuf();
		pkt = tsmpAllocSendBuf();
		if (pkt == NULL) {
			SCE_PRINTF_WITH_INFO(0xf, 3129, "\n");
			return;
		}
	}
	memset(pkt, 0, sizeof(*pkt));
	tsmp.send.length = sizeof(*pkt);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);
	tsmpSetHeader((TsmpHeader *)tsmp.send.base, type, code, 0, 0);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);
	pkt->result = result;
	TSMP_REQ_SEND(3159);
}

/* ===== threads and start ===== */

/* ===== requests from the host ===== */

#define TSMP_ERROR_BAD_SIZE	0x80080006	/* name unknown */

/* request payload of a registry setting (type 5, code 1) */
typedef struct TsmpSettingRequest {
	SceUInt32 reserved0;
	SceUInt16 count;
	SceUInt16 reserved1;
	SceUInt32 entry[];			/* one registry entry */
} TsmpSettingRequest;

/* the reply to a setting the host sent */
TSMP_INLINE int tsmpSettingAck(void)
{
	TsmpPacket *pkt;

	pkt = tsmpAllocSendBuf();
	if (pkt == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(pkt, 0, sizeof(*pkt));
	tsmp.send.length = sizeof(*pkt);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);
	pkt->entries = 1;
	tsmpSetHeader((TsmpHeader *)tsmp.send.base, 5, 0, 0, 0);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);
	return 0;
}

/* tell the host a setting it sent failed: echo the entry with the result */
TSMP_INLINE int tsmpNotifyUpdateRegError(const SceUInt32 *entry, SceUInt32 result)
{
	TsmpPacket *pkt;
	SceSize length;

	pkt = tsmpAllocSendBuf();
	if (pkt == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 2071, "\n");
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	memset(pkt, 0, TSMP_BUF_SIZE);
	length = entry[0];
	/* the entry, then its result and a terminating word */
	memcpy(pkt->data, entry, length);
	pkt->data[length / 4] = result;
	pkt->data[length / 4 + 1] = 0;
	tsmp.send.length = sizeof(*pkt) + length + 8;
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);
	tsmpSetHeader((TsmpHeader *)tsmp.send.base, 6, 1, 0, 0);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);
	pkt->entries = 1;
	TSMP_REQ_SEND(2123);
	return 0;
}

/* type 5 code 1: the host sets a registry key */
TSMP_INLINE int tsmpGetSettingRes(const TsmpHeader *pkt, SceSize size)
{
	const TsmpSettingRequest *req = (const TsmpSettingRequest *)(pkt + 1);
	const SceUInt32 *entry = req->entry;
	RegMgrKeyInfo set;
	SceUInt32 kind, words, result;
	int ret;

	if (size < sizeof(TsmpHeader) + sizeof(TsmpSettingRequest)) {
		SCE_PRINTF_WITH_INFO(0xf, 1595, "%d\n", size);
		goto bad_size;
	}
	if (size > TSMP_PACKET_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 1604, "%d\n", size);
		goto bad_size;
	}
	if (req->count == 0)
		return SCE_ERROR_ERRNO_ENODATA;

	set.size = sizeof(set);
	set.name = (const char *)&entry[2];
	words = ((entry[1] + 3) >> 2) + 2;
	kind = entry[words];
	set.valueSize = kind;
	switch (kind >> 16) {
	case 1:
		set.data = &entry[words + 1];
		set.type = 2;
		break;
	case 2:
		set.data = &entry[words + 1];
		set.type = 1;
		break;
	case 0:
		set.type = 0;
		set.value = entry[words + 1];
		break;
	case 0xff:
		/* delete the key */
		set.type = 0;
		ret = SceRegMgrForDebugger_46AC0CCB(4, &set);
		goto done;
	default:
		result = TSMP_RESULT_REG_ERROR;
		goto error;
	}
	ret = SceRegMgrForDebugger_810D2F09(&set);
done:
	if (ret >= 0)
		return tsmpSettingAck();

	switch (ret) {
	case 0x800d0008:
	case 0x800d0009:
	case 0x800d000a:
	case 0x800d000b:
	case 0x800d000c:
	case 0x800d0014:
	case 0x800d0015:
	case 0x800d0017:
		result = TSMP_RESULT_REG_NOT_FOUND;
		break;
	case 0x800d001f:
		result = TSMP_RESULT_REG_DENIED;
		break;
	default:
		result = TSMP_RESULT_REG_ERROR;
		break;
	}
error:
	ret = tsmpNotifyUpdateRegError(entry, result);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(0xf, 1690, "0x%08X\n", ret);
	return ret;

bad_size:
	tsmpSendError(pkt->type, pkt->code + 1, TSMP_RESULT_BAD_SIZE);
	return TSMP_ERROR_BAD_SIZE;
}

/* type 5 code 6: the host writes the setting UI file */
TSMP_INLINE int tsmpGetSettingUIFile(const TsmpHeader *pkt, SceSize size)
{
	const SceUInt32 *body = (const SceUInt32 *)(pkt + 1);
	TsmpRegWriteRequest write;
	TsmpPacket *reply;
	SceUInt32 result;
	int ret;

	if (size < 0x28) {
		SCE_PRINTF_WITH_INFO(0xf, 1729, "%d\n", size);
		goto bad_size;
	}
	if (size > TSMP_PACKET_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 1738, "%d\n", size);
		goto bad_size;
	}
	write.data = &body[1];
	write.size = sizeof(write);
	write.length = body[0] - 1;
	ret = SceRegMgrForDebugger_46AC0CCB(1, &write);
	result = TSMP_RESULT_OK;
	if (ret < 0)
		result = ret == (int)0x800d001f ? TSMP_RESULT_REG_DENIED : TSMP_RESULT_FAILED;

	reply = tsmpAllocSendBuf();
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(reply, 0, sizeof(*reply));
	tsmp.send.length = sizeof(*reply);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);
	reply->result = result;
	tsmpSetHeader((TsmpHeader *)tsmp.send.base, 5, 7, pkt->id[0], pkt->id[1]);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);
	return 0;

bad_size:
	tsmpSendError(pkt->type, pkt->code + 1, TSMP_RESULT_BAD_SIZE);
	return TSMP_ERROR_BAD_SIZE;
}

/* type 6 code 3: the host received a setting */
TSMP_INLINE int tsmpSettingReceived(const TsmpHeader *pkt, SceSize size)
{
	if (size < 0x20) {
		SCE_PRINTF_WITH_INFO(0xf, 1813, "%d\n", size);
		goto bad_size;
	}
	if (size > TSMP_PACKET_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 1822, "%d\n", size);
		goto bad_size;
	}
	return tsmpSettingAck();

bad_size:
	tsmpSendError(pkt->type, pkt->code + 1, TSMP_RESULT_BAD_SIZE);
	return TSMP_ERROR_BAD_SIZE;
}

/* type 6 code 4: what the host wants synchronised (no reply) */
TSMP_INLINE int tsmpSettingSyncRequest(const TsmpHeader *pkt, SceSize size)
{
	SceUInt32 flags;
	int ret;

	if (size < 0x1c) {
		SCE_PRINTF_WITH_INFO(0xf, 1877, "%d\n", size);
		goto bad_size;
	}
	if (size > TSMP_PACKET_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 1886, "%d\n", size);
		goto bad_size;
	}
	if (size == 0x1c) {
		/* an old host: everything */
		tsmpSetSystemStatus(TSMP_STATUS_SYNC_REQ | TSMP_STATUS_SYNC_REG
				    | TSMP_STATUS_SYNC_SETTING | TSMP_STATUS_SYNC_EVENTLOG, 1);
	} else if (*(const SceUInt32 *)(pkt + 1) != 0) {
		tsmpSetSystemStatus(TSMP_STATUS_SYNC_REQ, 1);
		flags = *(const SceUInt32 *)(pkt + 1);
		if (flags & 1)
			tsmpSetSystemStatus(TSMP_STATUS_SYNC_REG, 1);
		if (flags & 2)
			tsmpSetSystemStatus(TSMP_STATUS_SYNC_SETTING, 1);
		if (flags & 4)
			tsmpSetSystemStatus(TSMP_STATUS_SYNC_EVENTLOG, 1);
	}
	if (tsmp.syncEvf != 0) {
		ret = sceKernelSetEventFlag(tsmp.syncEvf, TSMP_SYNC_START);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 1927, "0x%08X\n", ret);
			return ret;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;

bad_size:
	tsmpSendError(pkt->type, pkt->code + 1, TSMP_RESULT_BAD_SIZE);
	return TSMP_ERROR_BAD_SIZE;
}

/* type 4 code 4: activate the development kit; reply with the expiry date */
TSMP_INLINE int tsmpUpdateActivation(const TsmpHeader *pkt, SceSize size)
{
	const SceUInt32 *body = (const SceUInt32 *)(pkt + 1);
	TsmpPacket *reply;
	SceUInt32 expire, result;
	int ret;

	if (size < sizeof(TsmpPacket) + 4) {
		SCE_PRINTF_WITH_INFO(0xf, 1471, "%d\n", size);
		goto bad_size;
	}
	if (size > TSMP_PACKET_MAX) {
		SCE_PRINTF_WITH_INFO(0xf, 1480, "%d\n", size);
		goto bad_size;
	}
	if (body[0] + 0x20 > size) {
		SCE_PRINTF_WITH_INFO(0xf, 1489, "%d\n", size);
		goto bad_size;
	}

	if (body[0] >= 2 && (ret = SceSblPostSsMgrForDriver_0298382B(&body[1])) < 0) {
		if (ret == (int)0x800f1302)
			result = TSMP_RESULT_ACT_EXPIRED;
		else if (ret == (int)0x800f1324 || ret == (int)0x800f1327)
			result = TSMP_RESULT_ACT_UNSUPPORTED;
		else if (ret == (int)0x800f1305)
			result = TSMP_RESULT_ACT_BAD_DATA;
		else if (ret == (int)0x800f1311)
			result = TSMP_RESULT_ACT_INVALID;
		else
			result = TSMP_RESULT_FAILED;
	} else {
		ret = SceSblPostSsMgrForDriver_4FF2682F(&expire, 1);
		result = TSMP_RESULT_OK;
		if (ret < 0) {
			expire = 0;
			if (ret == (int)0x800f1302)
				result = TSMP_RESULT_ACT_EXPIRED;
			else if (ret == (int)0x800f1324 || ret == (int)0x800f1327)
				result = TSMP_RESULT_ACT_UNSUPPORTED;
			else if (ret == (int)0x800f1305)
				result = TSMP_RESULT_ACT_BAD_DATA;
			else
				result = TSMP_RESULT_FAILED;
		}
	}

	reply = tsmpAllocSendBuf();
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(reply, 0, sizeof(*reply) + sizeof(reply->data[0]));
	tsmp.send.length = sizeof(*reply) + sizeof(reply->data[0]);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);
	reply->result = result;
	reply->data[0] = expire;
	tsmpSetHeader((TsmpHeader *)tsmp.send.base, 4, 5, pkt->id[0], pkt->id[1]);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);
	return 0;

bad_size:
	tsmpSendError(pkt->type, pkt->code + 1, TSMP_RESULT_BAD_SIZE);
	return TSMP_ERROR_BAD_SIZE;
}

/* handle the received packet, then send the reply built for it */
TSMP_INLINE int tsmpParseRecvPacket(void)
{
	const TsmpHeader *pkt = tsmp.recv.base;
	SceSize size;
	int ret;

	size = SceDeci4pDfMgrForDebugger_CACAB5F9(&pkt->hdr);
	if (size < sizeof(*pkt)) {
		ret = SCE_KERNEL_ERROR_UNSUP;
	} else if (pkt->type == 4) {
		if (pkt->code == 4) {
			ret = tsmpUpdateActivation(pkt, tsmp.recv.length);
		} else {
			SCE_PRINTF_WITH_INFO(0xf, 1366, "0x%02x\n", pkt->code);
			tsmpSendError(pkt->type, pkt->code + 1, TSMP_RESULT_BAD_COMMAND);
			ret = SCE_KERNEL_ERROR_UNSUP;
		}
	} else if (pkt->type == 5) {
		if (pkt->code == 1) {
			ret = tsmpGetSettingRes(pkt, tsmp.recv.length);
		} else if (pkt->code == 6) {
			ret = tsmpGetSettingUIFile(pkt, tsmp.recv.length);
		} else {
			SCE_PRINTF_WITH_INFO(0xf, 1387, "0x%02x\n", pkt->code);
			tsmpSendError(pkt->type, pkt->code + 1, TSMP_RESULT_BAD_COMMAND);
			ret = SCE_KERNEL_ERROR_UNSUP;
		}
	} else if (pkt->type == 6) {
		if (pkt->code == 3) {
			ret = tsmpSettingReceived(pkt, tsmp.recv.length);
		} else if (pkt->code == 4) {
			ret = tsmpSettingSyncRequest(pkt, tsmp.recv.length);
		} else {
			SCE_PRINTF_WITH_INFO(0xf, 1402, "0x%02x\n", pkt->code);
			tsmpSendError(pkt->type, pkt->code + 1, TSMP_RESULT_BAD_COMMAND);
			ret = SCE_KERNEL_ERROR_UNSUP;
		}
	} else {
		SCE_PRINTF_WITH_INFO(0xf, 1409, "0x%02x\n", pkt->type);
		tsmpSendError(pkt->type + 1, pkt->code, TSMP_RESULT_BAD_COMMAND);
		ret = SCE_KERNEL_ERROR_UNSUP;
	}

	if (tsmp.recv.status & TSMP_BUF_BUSY)
		tsmpSetBufStatus(&tsmp.recv, TSMP_BUF_BUSY, 0);
	if (ret < 0)
		return ret;

	tsmpSetSystemStatus(TSMP_STATUS_SENDING, 1);
	ret = sceDfMgrReqSend(tsmp.handle);
	if (ret < 0 && ret != (int)DFMGR_ERROR_BUSY) {
		SCE_PRINTF_WITH_INFO(0xf, 1439, "0x%08X\n", ret);
		tsmpFreeSendBuf();
		tsmpSetSystemStatus(TSMP_STATUS_SENDING, 0);
		return ret;
	}
	return 0;
}

/* ===== notifications to the host ===== */

/* the queued registry changes, as many as fit */
TSMP_INLINE void tsmpNotifyUpdateReg(void)
{
	TsmpPacket *pkt;
	char *out;
	SceUInt16 count;
	int total, n, ret;

	pkt = tsmpAllocSendBuf();
	if (pkt == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 1952, "\n");
		return;
	}
	total = 0;
	out = (char *)pkt->data;
	count = 0;
	memset(pkt, 0, TSMP_BUF_SIZE);
	do {
		count++;
		ret = tsmpPopNotification(&tsmp.registry);
		if (ret < 0) {
			if (ret != (int)SCE_ERROR_ERRNO_ENODATA)
				SCE_PRINTF_WITH_INFO(0xf, 1977, "0x%08X\n", ret);
			tsmpFreeSendBuf();
			return;
		}
		n = tsmpConvertRegMgrToTsmp((SceUInt32 *)out);
		total += n + 8;
		if (n < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 1986, "0x%08X\n", n);
			tsmpFreeSendBuf();
			return;
		}
		*(SceUInt32 *)(out + n) = 0;
		*(SceUInt32 *)(out + n + 4) = 1;
		out += n + 8;
	} while (tsmp.registry.count > 0 && total < 0x7000);

	if (tsmp.registry.count == 0 && tsmp.syncEvf != 0) {
		ret = sceKernelSetEventFlag(tsmp.syncEvf, TSMP_SYNC_REG_DONE);
		if (ret < 0)
			SCE_PRINTF_WITH_INFO(0xf, 2014, "0x%08X\n", ret);
	}
	tsmp.send.length = sizeof(*pkt) + total;
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);
	tsmpSetHeader((TsmpHeader *)tsmp.send.base, 6, 1, 0, 0);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);
	pkt->entries = count;
	TSMP_REQ_SEND(2049);
}

/* the queued software information (the loop tests the registry queue) */
TSMP_INLINE void tsmpNotifyUpdateSwInfo(void)
{
	TsmpPacket *pkt;
	char *out;
	SceUInt16 count;
	int total, n, ret;

	pkt = tsmpAllocSendBuf();
	if (pkt == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 2341, "\n");
		return;
	}
	total = 0;
	count = 0;
	out = (char *)pkt->data;
	memset(pkt, 0, TSMP_BUF_SIZE);
	do {
		count++;
		ret = tsmpPopNotification(&tsmp.swInfo);
		if (ret < 0) {
			if (ret != (int)SCE_ERROR_ERRNO_ENODATA)
				SCE_PRINTF_WITH_INFO(0xf, 2366, "0x%08X\n", ret);
			tsmpFreeSendBuf();
			return;
		}
		n = tsmpConvertRegMgrToTsmp((SceUInt32 *)out);
		total += n + 8;
		if (n < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2375, "0x%08X\n", n);
			tsmpFreeSendBuf();
			return;
		}
		*(SceUInt32 *)(out + n) = 0;
		*(SceUInt32 *)(out + n + 4) = 1;
	} while (tsmp.registry.count > 0 && (out += n + 8, total < 0x7000));

	tsmp.send.length = sizeof(*pkt) + total;
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);
	tsmpSetHeader((TsmpHeader *)tsmp.send.base, 6, 6, 0, 0);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);
	pkt->entries = count;
	TSMP_REQ_SEND(2428);
}

/* the next chunk of the setting UI file or event log */
#define TSMP_SEND_FILE_CHUNK(_code, _status, _done, _line0, _line1, _line2, _line3) ({	\
	TsmpFileChunk *_pkt;								\
	SceSize _left;									\
	int _ret = 0;									\
	_pkt = tsmpAllocSendBuf();							\
	if (_pkt == NULL) {								\
		SCE_PRINTF_WITH_INFO(0xf, _line0, "\n");				\
		_ret = SCE_KERNEL_ERROR_NO_MEMORY;					\
	} else {									\
		memset(_pkt, 0, TSMP_BUF_SIZE);						\
		_left = tsmp.fileRequest.total - tsmp.fileSent;				\
		if (_left <= TSMP_DATA_MAX) {						\
			tsmpSetSystemStatus(_status, 0);				\
			_ret = sceKernelSetEventFlag(tsmp.syncEvf, _done);		\
			if (_ret < 0)							\
				SCE_PRINTF_WITH_INFO(0xf, _line1, "0x%08X\n", _ret);	\
			tsmp.send.length = sizeof(*_pkt) + _left;			\
		} else {								\
			_left = TSMP_DATA_MAX;						\
			tsmp.send.length = sizeof(*_pkt) + TSMP_DATA_MAX;		\
		}									\
		tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);			\
		tsmpSetHeader((TsmpHeader *)tsmp.send.base, 6, _code, 0, 0);		\
		tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);			\
		_pkt->result = 0;							\
		_pkt->offset = tsmp.fileSent;						\
		_pkt->size = _left;							\
		_pkt->total = tsmp.fileRequest.total;					\
		memcpy(_pkt->data, (char *)tsmp.fileRequest.buf + tsmp.fileSent, _left);	\
		tsmp.fileSent += _left;							\
		if (!(tsmp.status & (_status))) {					\
			_ret = SceSysmemForDriver_009E1C61(tsmp.fileBlock);			\
			if (_ret < 0)							\
				SCE_PRINTF_WITH_INFO(0xf, _line2, "0x%08X\n", _ret);	\
			tsmp.fileBlock = 0;						\
		}									\
		TSMP_REQ_SEND(_line3);							\
		_ret = 0;								\
	}										\
	_ret;										\
})

TSMP_INLINE int tsmpNotifyUpdateSettingXml(void)
{
	return TSMP_SEND_FILE_CHUNK(9, TSMP_STATUS_SETTING_XML, TSMP_SYNC_SETTING_DONE,
				    2150, 2170, 2204, 2220);
}

TSMP_INLINE int tsmpNotifyUpdateEventLogXml(void)
{
	return TSMP_SEND_FILE_CHUNK(0xd, TSMP_STATUS_EVENTLOG_XML, TSMP_SYNC_EVENTLOG_DONE,
				    2247, 2267, 2301, 2317);
}

/* the target's time, so the host's clock and time zone agree */
TSMP_INLINE int tsmpSyncCurrentTick(void)
{
	TsmpPacket *pkt;
	SceDateTime time;
	SceUInt64 unixTime;
	int ret;

	pkt = tsmpAllocSendBuf();
	if (pkt == NULL) {
		SCE_PRINTF_WITH_INFO(0xf, 3054, "\n");
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	memset(pkt, 0, sizeof(*pkt) + sizeof(pkt->data[0]));
	tsmp.send.length = sizeof(*pkt) + sizeof(pkt->data[0]);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);
	tsmpSetHeader((TsmpHeader *)tsmp.send.base, 6, 0xa, 0, 0);
	tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);
	ret = sceRtcGetCurrentClockLocalTime(&time);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3075, "0x%08X\n", ret);
		tsmpFreeSendBuf();
		return ret;
	}
	ret = SceRtcForDriver_497B5DC9(&time, &unixTime);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3083, "0x%08X\n", ret);
		tsmpFreeSendBuf();
		return ret;
	}
	/* the result word and the one after it carry the 64-bit time */
	pkt->result = (SceUInt32)unixTime;
	pkt->data[0] = (SceUInt32)(unixTime >> 32);
	TSMP_REQ_SEND(3101);
	return 0;
}

/* registry synchronisation begins (0) / ends (1) */
#define TSMP_SYNC_REGISTRY(_phase, _done, _line0, _line1, _line2) ({	\
	TsmpPacket *_pkt;						\
	int _ret;							\
	_pkt = tsmpAllocSendBuf();					\
	if (_pkt == NULL) {						\
		SCE_PRINTF_WITH_INFO(0xf, _line0, "\n");		\
		_ret = SCE_KERNEL_ERROR_NO_MEMORY;			\
	} else {							\
		memset(_pkt, 0, sizeof(*_pkt));				\
		tsmp.send.length = sizeof(*_pkt);			\
		tsmpSetBufStatus(&tsmp.send, TSMP_BUF_FILLED, 1);	\
		tsmpSetHeader((TsmpHeader *)tsmp.send.base, 6, 8, 0, 0); \
		tsmpSetBufStatus(&tsmp.send, TSMP_BUF_HEADER, 1);	\
		_pkt->result = _phase;					\
		TSMP_REQ_SEND(_line1);					\
		_ret = sceKernelSetEventFlag(tsmp.syncEvf, _done);	\
		if (_ret < 0)						\
			SCE_PRINTF_WITH_INFO(0xf, _line2, "0x%08X\n", _ret); \
		_ret = 0;						\
	}								\
	_ret;								\
})

TSMP_INLINE int tsmpSyncRegistryPrologue(void)
{
	return TSMP_SYNC_REGISTRY(0, TSMP_SYNC_TICK_DONE, 2769, 2800, 2809);
}

TSMP_INLINE int tsmpSyncRegistryEpilogue(void)
{
	return TSMP_SYNC_REGISTRY(1, TSMP_SYNC_REG_END_DONE, 2828, 2859, 2868);
}

TSMP_INLINE void tsmpSyncTerm(void)
{
	int ret;

	ret = sceKernelDeleteThread(tsmp.syncThread);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(0xf, 2489, "0x%08X\n", ret);
	tsmp.syncThread = 0;
	ret = sceKernelDeleteEventFlag(tsmp.syncEvf);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(0xf, 2497, "0x%08X\n", ret);
	tsmp.syncEvf = 0;
}

/* one event per pass; the others are raised again for the next */
static int tsmpMainThread(SceSize args, void *argp)
{
	SceUInt32 pattern, bits;
	int ret;

	(void)args;
	(void)argp;

	for (;;) {
		pattern = (tsmp.status & TSMP_STATUS_SENDING) ? TSMP_EVENT_SENT : TSMP_EVENT_ALL;
		bits = 0;
		ret = sceKernelWaitEventFlag(tsmp.evf, pattern,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 1119, "0x%08X\n", ret);
			continue;
		}
		bits &= pattern;

		if (bits & TSMP_EVENT_SENT) {
			tsmpSetSystemStatus(TSMP_STATUS_SENDING, 0);
			if (tsmp.registry.count > 0)
				sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_REG);
			if (tsmp.status & TSMP_STATUS_SETTING_XML)
				sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_SETTING_XML);
			if (tsmp.status & TSMP_STATUS_EVENTLOG_XML)
				sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_EVENTLOG_XML);
			if (tsmp.swInfo.count > 0)
				sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_SWINFO);
			bits &= ~TSMP_EVENT_SENT;
		} else if (bits & TSMP_EVENT_RECV) {
			ret = tsmpParseRecvPacket();
			if (ret < 0 && ret != (int)SCE_ERROR_ERRNO_ENODATA
			    && ret != (int)DFMGR_ERROR_BUSY)
				SCE_PRINTF_WITH_INFO(0xf, 1188, "0x%08X\n", ret);
			bits &= ~TSMP_EVENT_RECV;
		} else if (bits & TSMP_EVENT_TICK) {
			ret = tsmpSyncCurrentTick();
			if (ret < 0)
				SCE_PRINTF_WITH_INFO(0xf, 1203, "0x%08X\n", ret);
			bits &= ~TSMP_EVENT_TICK;
		} else if (bits & TSMP_EVENT_REG_BEGIN) {
			ret = tsmpSyncRegistryPrologue();
			if (ret < 0)
				SCE_PRINTF_WITH_INFO(0xf, 1218, "0x%08X\n", ret);
			bits &= ~TSMP_EVENT_REG_BEGIN;
		} else if (bits & TSMP_EVENT_REG) {
			tsmpNotifyUpdateReg();
			bits &= ~TSMP_EVENT_REG;
		} else if (bits & TSMP_EVENT_SETTING_XML) {
			ret = tsmpNotifyUpdateSettingXml();
			if (ret < 0)
				SCE_PRINTF_WITH_INFO(0xf, 1248, "0x%08X\n", ret);
			bits &= ~TSMP_EVENT_SETTING_XML;
		} else if (bits & TSMP_EVENT_EVENTLOG_XML) {
			ret = tsmpNotifyUpdateEventLogXml();
			if (ret < 0)
				SCE_PRINTF_WITH_INFO(0xf, 1263, "0x%08X\n", ret);
			bits &= ~TSMP_EVENT_EVENTLOG_XML;
		} else if (bits & TSMP_EVENT_SWINFO) {
			tsmpNotifyUpdateSwInfo();
			bits &= ~TSMP_EVENT_SWINFO;
		} else if (bits & TSMP_EVENT_REG_END) {
			ret = tsmpSyncRegistryEpilogue();
			if (ret < 0)
				SCE_PRINTF_WITH_INFO(0xf, 1294, "0x%08X\n", ret);
			bits &= ~TSMP_EVENT_REG_END;
		} else if (bits & TSMP_EVENT_SYNC_TERM) {
			tsmpSyncTerm();
			bits &= ~TSMP_EVENT_SYNC_TERM;
		}

		if (bits != 0) {
			ret = sceKernelSetEventFlag(tsmp.evf, bits);
			if (ret < 0)
				SCE_PRINTF_WITH_INFO(0xf, 1323, "0x%08X\n", ret);
		}
	}
	return 0;
}

/* ===== setting synchronisation (sync thread) ===== */

/* event log XML as sent: a header, then the log */
typedef struct TsmpEventLogHeader {
	SceSize size;				/* header and log bytes */
	SceSize nameLength;
	char name[10];				/* title of the log */
	SceUInt16 padding;
	SceSize dataSize;			/* log bytes */
} TsmpEventLogHeader;

/* read the setting UI file for the main thread to send */
TSMP_INLINE int tsmpSyncSettingXml(void)
{
	SceSize size;
	int ret;

	tsmp.fileBlock = 0;
	ret = SceRegMgrForDebugger_D668ABD2(&size);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2888, "0x%08X\n", ret);
		return ret;
	}
	tsmp.fileSize = (size + 0xfff) & ~0xfff;
	ret = SceSysmemForDriver_C94850C9("SceDeci4pTsmpUifileBuf", TSMP_MEMTYPE, tsmp.fileSize, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2900, "0x%08X\n", ret);
		return ret;
	}
	tsmp.fileBlock = ret;
	ret = SceSysmemForDriver_A841EDDA(ret, &tsmp.fileBase);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2908, "0x%08X\n", ret);
		goto free;
	}
	tsmp.fileRequest.size = sizeof(tsmp.fileRequest);
	tsmp.fileRequest.flags = 0x81000001;
	tsmp.fileRequest.buf = tsmp.fileBase;
	tsmp.fileRequest.bufSize = tsmp.fileSize;
	tsmp.fileSent = 0;
	ret = SceRegMgrForDebugger_46AC0CCB(8, &tsmp.fileRequest);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2924, "0x%08X\n", ret);
		goto free;
	}
	tsmpSetSystemStatus(TSMP_STATUS_SETTING_XML, 1);
	ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_SETTING_XML);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2934, "0x%08X\n", ret);
		goto free;
	}
	return 0;

free:
	SceSysmemForDriver_009E1C61(tsmp.fileBlock);
	return ret;
}

/* read the event log for the main thread to send */
TSMP_INLINE int tsmpSyncEventLogXml(void)
{
	TsmpEventLogHeader *header;
	SceSize size;
	int ret;

	tsmp.fileBlock = 0;
	ret = SceRegMgrForDebugger_8AE5757E(&size);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2960, "0x%08X\n", ret);
		return ret;
	}
	tsmp.fileSize = (size + sizeof(*header) + 0xfff) & ~0xfff;
	ret = SceSysmemForDriver_C94850C9("SceDeci4pTsmpUifileBuf", TSMP_MEMTYPE, tsmp.fileSize, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2973, "0x%08X\n", ret);
		return ret;
	}
	tsmp.fileBlock = ret;
	ret = SceSysmemForDriver_A841EDDA(ret, &tsmp.fileBase);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2981, "0x%08X\n", ret);
		goto free;
	}
	header = tsmp.fileBase;
	tsmp.fileRequest.buf = header + 1;
	tsmp.fileRequest.size = sizeof(tsmp.fileRequest);
	tsmp.fileRequest.flags = 0x81000001;
	tsmp.fileSent = 0;
	tsmp.fileRequest.bufSize = size;
	ret = SceRegMgrForDebugger_46AC0CCB(0x10, &tsmp.fileRequest);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2998, "0x%08X\n", ret);
		goto free;
	}
	tsmp.fileRequest.bufSize += sizeof(*header);
	tsmp.fileRequest.total += sizeof(*header);
	tsmp.fileRequest.buf = header;
	memset(header, 0, sizeof(*header));
	header->size = tsmp.fileRequest.bufSize;
	header->nameLength = sizeof(header->name);
	__builtin_memcpy(header->name, "NPXS19999", sizeof(header->name));
	header->dataSize = size;
	tsmpSetSystemStatus(TSMP_STATUS_EVENTLOG_XML, 1);
	ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_EVENTLOG_XML);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 3031, "0x%08X\n", ret);
		goto free;
	}
	return 0;

free:
	SceSysmemForDriver_009E1C61(tsmp.fileBlock);
	return ret;
}

/*
 * Bring the host up to date once: the registry, the setting UI file and the
 * event log, whichever the host asked for or the registry reports changed.
 */
static int tsmpSyncThread(SceSize args, void *argp)
{
	SceUInt32 timeout = 500000;
	int syncReg, syncSetting, syncEventLog, any;
	int ret;

	(void)args;
	(void)argp;

	ret = sceKernelWaitEventFlag(tsmp.syncEvf, TSMP_SYNC_START, SCE_KERNEL_EVF_WAITMODE_OR,
				      NULL,
				      &timeout);
	if (ret < 0 && ret != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
		SCE_PRINTF_WITH_INFO(0xf, 2535, "0x%08X\n", ret);
		goto term;
	}

	syncReg = SceRegMgrForDebugger_8E798E47();
	syncSetting = SceRegMgrForDebugger_B9A0BB77();
	syncEventLog = SceRegMgrForDebugger_C588D3E7();
	syncReg = (tsmp.status & TSMP_STATUS_SYNC_REG) || syncReg == 1;
	syncSetting = (tsmp.status & TSMP_STATUS_SYNC_SETTING) || syncSetting == 1;
	if (tsmp.status & TSMP_STATUS_SYNC_EVENTLOG)
		syncEventLog = 1;
	any = syncReg || syncSetting || syncEventLog == 1;

	tsmpSetSystemStatus(TSMP_STATUS_SYNCING, 1);
	if (any) {
		ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_REG_BEGIN);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2588, "0x%08X\n", ret);
			goto term;
		}
		ret = sceKernelWaitEventFlag(tsmp.syncEvf, TSMP_SYNC_TICK_DONE,
					      SCE_KERNEL_EVF_WAITMODE_OR,
					      NULL, NULL);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2597, "0x%08X\n", ret);
			goto fail;
		}
	}

	tsmp.regEntry.size = sizeof(tsmp.regEntry);
	ret = SceRegMgrForDebugger_53970232(tsmpPushRegistryNotification, &tsmp.regEntry);
	if (ret < 0)
		goto fail;

	if (syncReg) {
		ret = SceRegMgrForDebugger_8E798E47();
		if (ret == 0 && (tsmp.status & TSMP_STATUS_SYNC_REG)
		    && (ret = SceRegMgrForDebugger_46AC0CCB(2, NULL)) < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2620, "0x%08X\n", ret);
			goto fail;
		}
		ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_REG);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2628, "0x%08X\n", ret);
			goto fail;
		}
		ret = sceKernelWaitEventFlag(tsmp.syncEvf, TSMP_SYNC_REG_DONE,
					      SCE_KERNEL_EVF_WAITMODE_OR,
					      NULL, NULL);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2637, "0x%08X\n", ret);
			goto fail;
		}
	}

	if (syncSetting) {
		ret = tsmpSyncSettingXml();
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2649, "0x%08X\n", ret);
			goto fail;
		}
		ret = sceKernelWaitEventFlag(tsmp.syncEvf, TSMP_SYNC_SETTING_DONE,
					      SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2658, "0x%08X\n", ret);
			goto fail;
		}
	}

	if (syncEventLog == 1) {
		ret = tsmpSyncEventLogXml();
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2670, "0x%08X\n", ret);
			goto fail;
		}
		ret = sceKernelWaitEventFlag(tsmp.syncEvf, TSMP_SYNC_EVENTLOG_DONE,
					      SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2679, "0x%08X\n", ret);
			goto fail;
		}
	}

	if (any) {
		ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_REG_END);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2691, "0x%08X\n", ret);
			goto fail;
		}
		ret = sceKernelWaitEventFlag(tsmp.syncEvf, TSMP_SYNC_REG_END_DONE,
					      SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2700, "0x%08X\n", ret);
			goto term;
		}
	}

	ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_TICK);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(0xf, 2709, "0x%08X\n", ret);
	tsmpSetSystemStatus(TSMP_STATUS_SYNCING, 0);
	if (tsmp.registry.count > 0) {
		ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_REG);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 2724, "0x%08X\n", ret);
			goto term;
		}
	}
	ret = sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_SYNC_TERM);
	if (ret >= 0) {
		sceKernelExitThread(0);
		return 0;
	}
	SCE_PRINTF_WITH_INFO(0xf, 2732, "0x%08X\n", ret);
	goto error;

fail:
	sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_REG_END);
term:
	sceKernelSetEventFlag(tsmp.evf, TSMP_EVENT_SYNC_TERM);
error:
	SCE_PRINTF_WITH_INFO(0, 2750, "[DECI] Setting/EventLog synchronization failed (0x%08x)\n",
			     ret);
	sceKernelExitThread(ret);
	return ret;
}

/* work queue item: start the setting synchronisation */
static int tsmpSyncInit(void *args)
{
	SceKernelThreadOptParam opt;
	int ret;

	(void)args;

	opt.size = sizeof(opt);
	opt.attr = 1;
	opt.kStackMemType = TSMP_MEMTYPE;
	ret = sceKernelCreateThread("SceDeci4pTsmpSync", tsmpSyncThread, 0x1e, 0x2000, 0,
				     0x80000, &opt);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2464, "0x%08X\n", ret);
		return ret;
	}
	tsmp.syncThread = ret;
	ret = sceKernelStartThread(ret, 0, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 2472, "0x%08X\n", ret);
		return ret;
	}
	return 0;
}

TSMP_INLINE int tsmpInit(void)
{
	SceKernelHeapCreateOpt heapOpt;
	SceKernelThreadOptParam threadOpt;
	SceDfMgrProtocol protocol;
	int ret;

	memset(&tsmp, 0, sizeof(tsmp));

	heapOpt.size = sizeof(heapOpt);
	heapOpt.attr = 0x401;
	heapOpt.memtype = TSMP_MEMTYPE;
	ret = SceSysmemForDriver_9328E0E8("SceDeci4pTsmpMisc", 0x4000, &heapOpt);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 337, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(338);
		return ret;
	}
	tsmp.miscHeap = ret;

	ret = tsmpInitNotification(&tsmp.registry, "SceDeci4pTsmpRegistry");
	if (ret < 0 || (ret = tsmpInitNotification(&tsmp.swInfo, "SceDeci4pTsmpSwInfo")) < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 346, "0x%08X\n", ret);
		return ret;
	}

	ret = SceSysmemForDriver_C94850C9("SceDeci4pTsmpSendBuf", TSMP_MEMTYPE, TSMP_BUF_SIZE, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 355, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(356);
		return ret;
	}
	tsmp.send.block = ret;
	ret = sceKernelInitializeFastMutex(&tsmp.statusLock, "SceDeci4pTsmpStatus", 0, 0);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 367, "0x%08X\n", ret);
		return ret;
	}
	ret = SceSysmemForDriver_A841EDDA(tsmp.send.block, &tsmp.send.base);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 375, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(376);
		return ret;
	}
	ret = sceKernelInitializeFastMutex(&tsmp.send.lock, "SceDeci4pTsmpSendBuf", 0, 0);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 386, "0x%08X\n", ret);
		return ret;
	}

	ret = SceSysmemForDriver_C94850C9("SceDeci4pTsmpRecvBuf", TSMP_MEMTYPE, TSMP_BUF_SIZE, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 395, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(396);
		return ret;
	}
	tsmp.recv.block = ret;
	ret = SceSysmemForDriver_A841EDDA(tsmp.recv.block, &tsmp.recv.base);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 405, "0x%08X\n", ret);
		SCE_DBG_ASSERT_FAIL(406);
		return ret;
	}
	ret = sceKernelInitializeFastMutex(&tsmp.recv.lock, "SceDeci4pTsmpRecvBuf", 0, 0);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 416, "0x%08X\n", ret);
		return ret;
	}

	ret = sceKernelCreateEventFlag("SceDeci4pTsmp", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 425, "0x%08X\n", ret);
		return ret;
	}
	tsmp.evf = ret;
	threadOpt.size = sizeof(threadOpt);
	threadOpt.attr = 1;
	threadOpt.kStackMemType = TSMP_MEMTYPE;
	ret = sceKernelCreateThread("SceDeci4pTsmp", tsmpMainThread, 0x1e, 0x2000, 0, 0x80000,
				     &threadOpt);
	if (ret < 0)
		return ret;
	tsmp.mainThread = ret;
	ret = sceKernelCreateEventFlag("SceDeci4pTsmpSync", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 451, "0x%08X\n", ret);
		return ret;
	}
	tsmp.syncEvf = ret;

	protocol.size = sizeof(protocol);
	protocol.protocol = 0x2000;
	protocol.alias = 0;
	protocol.version = 0x2100000;
	protocol.subVersion = 0x20005;
	protocol.destination = 0x6000;
	protocol.flags = DFMGR_PROTOCOL_ENABLE;
	strncpy(protocol.name, "TSMP", sizeof(protocol.name));
	ret = sceDfMgrOpen(tsmpEventHandler, &protocol, &tsmp);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 469, "0x%08X\n", ret);
		return ret;
	}
	tsmp.handle = ret;

	ret = sceKernelStartThread(tsmp.mainThread, 0, NULL);
	if (ret < 0)
		return ret;
	sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceDeci4pTsmpInit", tsmpSyncInit, NULL);
	SceSysrootForDriver_1FFD2AFA(tsmpSetSwInfoInt);
	SceSysrootForDriver_7C0BE393(tsmpSetSwInfoStr);
	SceSysrootForDriver_05CD0DD9(tsmpSetSwInfoBin);
	SceSysrootForDriver_ED688AEE(tsmpSyncLocalTime);
	return 0;
}

/* Sony's TsmpModuleStart */
int module_start(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	if (!SceDipswForDriver_A98FC2FD(TSMP_DIPSW)) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0xb, SCE_DBGINFO_FUNC(290, "TsmpModuleStart"),
					      "disabled by dipsw\n");
#endif
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (tsmpInit() < 0)
		return SCE_KERNEL_START_FAILED;
	return SCE_KERNEL_START_SUCCESS;
}
