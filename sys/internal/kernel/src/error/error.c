/*
 * SceError: the system error history and error code strings.
 *
 * The last ten errors posted by the shell live in a ring of 0x1B0-byte
 * entries, saved to vd0:history/data.bin (and data.bak) on suspend. Error codes
 * are shown as "<group>-<number>-<check digit>" using os0:kd/error_table.bin,
 * which maps known codes to numbers and facilities to group names.
 *
 * Sony's internal function names are not known for this module (no debug
 * records); the ones below are descriptive.
 */

#include <sysmem/sysclib.h>
#include <sysmem/uid_guid.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <rtc/rtc.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <iofilemgr/iofilemgr.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/sysevent.h>
#include <intrmgr/syscall.h>

#define HISTORY_ENTRIES		10
#define HISTORY_PATH		"vd0:history/data.bin"
#define HISTORY_BACKUP		"vd0:history/data.bak"
#define TABLE_PATH		"os0:kd/error_table.bin"
#define TABLE_HEAP_SIZE		80000
#define TABLE_GROUPS_MAX	32
#define TABLE_CODES_MAX		20000

typedef struct ErrorGroup {
	SceUInt16 start;			/* first facility */
	SceUInt16 count;
	char name[4];
} ErrorGroup;

typedef struct ErrorTable {
	char prefix[4];				/* group name of other facilities */
	SceUInt16 groupNum;
	SceUInt16 maxIndex;			/* codes after this one show as "*-" */
	ErrorGroup *groups;
	SceUInt16 base;				/* number of the first code */
	SceUInt16 codeNum;
	SceUInt32 *codes;
	SceUID heap;
} ErrorTable;

typedef struct ErrorRecord {
	SceUInt8 data[0x140];
	SceUInt32 code;
	SceUInt32 id;
	SceUInt32 swVersion;			/* SceKernelSystemSwVersion.version */
	SceUInt32 format;			/* the default format, when set */
	SceUInt8 key[10];
	SceUInt8 padding[2];
	SceUInt32 swVersionExtra;		/* the word after it */
	SceUInt8 reserved[0x20];
} ErrorRecord;

typedef struct ErrorEntry {
	SceUInt8 reserved0;
	SceUInt8 seqInfo;			/* _sceErrorHistoryUpdateSequenceInfo */
	SceUInt8 seq;
	SceUInt8 reserved1[5];
	SceUInt64 tick;
	char code[0x10];			/* the formatted error code */
	SceUInt8 reserved2[0x10];
	ErrorRecord rec;
} ErrorEntry;

typedef struct ErrorHistory {
	SceUInt8 version;
	SceUInt8 head;				/* the next entry to write */
	SceUInt8 seq;
	SceUInt8 reserved[0xd];
	ErrorEntry entry[HISTORY_ENTRIES];
} ErrorHistory;

typedef struct ErrorSequenceInfo {
	SceUInt8 seq;
	SceUInt8 seqInfo;
	SceUInt8 reserved0[6];
	SceUInt64 tick;
	SceUInt8 reserved1[0x10];
} ErrorSequenceInfo;

/* a lock on an event flag: signal words updated atomically */
typedef struct ErrorSync {
	SceUID evf;
	SceUInt32 signal;
	SceUInt32 waiters;
} ErrorSync;

typedef struct ErrorSysEvent {
	SceUID uid;
	const char *name;
	int (*suspend)(void);
	int (*resume)(void);
} ErrorSysEvent;

_Static_assert(sizeof(ErrorEntry) == 0x1b0, "ErrorEntry layout");
_Static_assert(sizeof(ErrorHistory) == 0x10f0, "ErrorHistory layout");

static ErrorSysEvent s_sysEvent;
static ErrorTable s_table;
static ErrorSync s_sync;
static SceUInt32 s_defaultFormat;
static SceUInt32 s_defaultFormatSet;
static ErrorHistory s_history;

/*
 * Sony keeps every helper out of line (39 functions), in this order: the sysevent
 * callbacks, the work task and the locked operations, module_start, the error
 * table and history, the lock and sysevent helpers, then the syscall entries.
 */

static int ErrorOnResume(void);
static int ErrorInitWork(void *arg);
static int ErrorOnSuspend(void);
static int ErrorGetExternalString(char *buf, SceUInt32 code);
static int ErrorPostLocked(ErrorRecord *rec);
static int ErrorSetDefaultFormatLocked(const SceUInt32 *format);
static int ErrorGetLocked(SceUInt32 index, ErrorEntry *buf);
static int ErrorUpdateSequenceInfoLocked(const ErrorSequenceInfo *info, int reserved);
static int ErrorClearLocked(int reserved);
static int ErrorFormatCode(ErrorTable *table, char *buf, SceUInt32 code);
static int ErrorTableInit(ErrorTable *table);
static int ErrorTableLoad(ErrorTable *table);
static int ErrorHistoryPost(ErrorTable *table, ErrorRecord *rec);
static int ErrorHistorySetDefaultFormat(const SceUInt32 *format);
static int ErrorHistoryGet(SceUInt32 index, ErrorEntry *buf);
static int ErrorHistoryUpdateSequenceInfo(const ErrorSequenceInfo *info, int reserved);
static int ErrorHistoryClear(int reserved);
static int ErrorHistoryLoad(void);
static int ErrorHistorySave(void);
static int ErrorSysEventHandler(int resume, int eventid, void *args, void *opt);
static int ErrorSyncCreate(ErrorSync *sync, SceUInt32 signal, const char *name);
static int ErrorSyncDelete(ErrorSync *sync);
static SceUInt32 ErrorSyncLock(ErrorSync *sync);
static int ErrorSyncUnlock(ErrorSync *sync, SceUInt32 signal);
static int ErrorSysEventRegister(ErrorSysEvent *ev);
static int ErrorSysEventUnregister(SceUID uid);

static inline ErrorEntry *ErrorEntryAt(int i)
{
	return &s_history.entry[i];
}

static __attribute__((noinline, noclone)) int ErrorOnResume(void)
{
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorInitWork(void *arg)
{
	if (ErrorHistoryLoad() >= 0)
		ErrorTableLoad(&s_table);
	return 1;
}

static __attribute__((noinline, noclone)) int ErrorOnSuspend(void)
{
	ErrorHistorySave();
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorGetExternalString(char *buf, SceUInt32 code)
{
	return ErrorFormatCode(&s_table, buf, code);
}

static __attribute__((noinline, noclone)) int ErrorPostLocked(ErrorRecord *rec)
{
	int ret;

	ErrorSyncLock(&s_sync);
	ret = ErrorHistoryPost(&s_table, rec);
	ErrorSyncUnlock(&s_sync, 1);
	return ret;
}

static __attribute__((noinline, noclone)) int ErrorSetDefaultFormatLocked(const SceUInt32 *format)
{
	int ret;

	ErrorSyncLock(&s_sync);
	ret = ErrorHistorySetDefaultFormat(format);
	ErrorSyncUnlock(&s_sync, 1);
	return ret;
}

static __attribute__((noinline, noclone)) int ErrorGetLocked(SceUInt32 index, ErrorEntry *buf)
{
	int ret;

	ErrorSyncLock(&s_sync);
	ret = ErrorHistoryGet(index, buf);
	ErrorSyncUnlock(&s_sync, 1);
	return ret;
}

static __attribute__((noinline, noclone)) int ErrorUpdateSequenceInfoLocked(const ErrorSequenceInfo *info, int reserved)
{
	int ret;

	ErrorSyncLock(&s_sync);
	ret = ErrorHistoryUpdateSequenceInfo(info, reserved);
	ErrorSyncUnlock(&s_sync, 1);
	return ret;
}

static __attribute__((noinline, noclone)) int ErrorClearLocked(int reserved)
{
	int ret;

	ErrorSyncLock(&s_sync);
	ret = ErrorHistoryClear(reserved);
	ErrorSyncUnlock(&s_sync, 1);
	return ret;
}

int module_start(SceSize args, void *argp)
{
	if (ErrorSyncCreate(&s_sync, 1, "SceErrorSync") < 0)
		return SCE_KERNEL_START_NO_RESIDENT;

	s_sysEvent.name = "SceError";
	s_sysEvent.suspend = ErrorOnSuspend;
	s_sysEvent.uid = 0;
	s_sysEvent.resume = ErrorOnResume;
	if (ErrorSysEventRegister(&s_sysEvent) < 0)
		goto delete;
	if (ErrorTableInit(&s_table) < 0) {
		ErrorSysEventUnregister(s_sysEvent.uid);
		goto delete;
	}
	sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceErrorInit", ErrorInitWork, NULL);
	return SCE_KERNEL_START_SUCCESS;

delete:
	ErrorSyncDelete(&s_sync);
	return SCE_KERNEL_START_NO_RESIDENT;
}

static __attribute__((noinline, noclone)) int ErrorFormatCode(ErrorTable *table, char *buf, SceUInt32 code)
{
	const char *prefix;
	ErrorGroup *g;
	int i, n, m, sum, check;
	SceUInt32 facility;

	if (buf == NULL)
		return 0;
	if (table->codeNum == 0)
		goto unknown;

	if (table->codes[0] == code) {
		i = 0;
	} else {
		for (i = 1; i < table->codeNum; i++)
			if (table->codes[i] == code)
				break;
		if (i >= table->codeNum)
			goto unknown;
		if (table->maxIndex < i) {
			snprintf(buf, 0x10, "*-%08x", code);
			return 0;
		}
	}

	facility = (code >> 16) & 0xfff;
	if (facility - 0x200 <= 0xff) {
		prefix = "NS";
	} else {
		prefix = table->prefix;
		for (g = table->groups; g < table->groups + table->groupNum; g++) {
			if ((int)facility >= g->start && (int)facility < g->start + g->count) {
				prefix = g->name;
				break;
			}
		}
	}

	n = table->base + i;
	check = 0;
	if (n != 0) {
		sum = 0;
		m = n;
		do {
			sum += m % 10;
			m /= 10;
		} while (m != 0);
		check = sum % 10;
	}
	snprintf(buf, 0x10, "%s-%d-%1d", prefix, n, check);
	return 0;

unknown:
	snprintf(buf, 0x10, "E-%08x", code);
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorTableInit(ErrorTable *table)
{
	SceKernelHeapCreateOpt opt;

	memset(table, 0, sizeof(*table));
	opt.size = sizeof(opt);
	opt.attr = 0;
	opt.maxSize = 0;
	opt.memBlock = 0;
	opt.memtype = 0;
	opt.pid = 0;
	opt.heap = NULL;
	table->heap = sceKernelCreateHeap("SceErrorTable", TABLE_HEAP_SIZE, &opt);
	if (table->heap < 0)
		goto fail;
	table->groups = sceKernelAllocHeapMemory(table->heap, TABLE_GROUPS_MAX * sizeof(ErrorGroup));
	if (table->groups == NULL)
		goto delete;
	memset(table->groups, 0, TABLE_GROUPS_MAX * sizeof(ErrorGroup));
	table->codes = sceKernelAllocHeapMemory(table->heap, TABLE_HEAP_SIZE);
	if (table->codes == NULL) {
		sceKernelFreeHeapMemory(table->heap, table->groups);
		goto delete;
	}
	memset(table->codes, 0, TABLE_HEAP_SIZE);
	return 0;

delete:
	sceKernelDeleteHeap(table->heap);
fail:
	memset(table, 0, sizeof(*table));
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorTableLoad(ErrorTable *table)
{
	SceUID fd;
	int size;

	fd = sceIoOpen(TABLE_PATH, SCE_O_RDONLY, 0);
	if (fd < 0)
		goto fail;
	if (sceIoRead(fd, table, 8) != 8)
		goto close;
	if (table->groupNum > TABLE_GROUPS_MAX) {
		SceDebugForDriver_391B74B7("[SceError] facility_group_num = %d\n", table->groupNum);
		goto close;
	}
	size = table->groupNum * sizeof(ErrorGroup);
	if (sceIoRead(fd, table->groups, size) != size)
		goto close;
	if (sceIoRead(fd, &table->base, 4) != 4)
		goto close;
	if (table->codeNum > TABLE_CODES_MAX) {
		SceDebugForDriver_391B74B7("[SceError] error_table_num = %d\n", table->codeNum);
		goto close;
	}
	size = table->codeNum * sizeof(SceUInt32);
	if (sceIoRead(fd, table->codes, size) != size)
		goto close;
	sceIoClose(fd);
	return 0;

close:
	sceIoClose(fd);
fail:
	memset(table, 0, sizeof(*table));
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorHistoryPost(ErrorTable *table, ErrorRecord *rec)
{
	SceKernelSystemSwVersion fw;
	ErrorEntry *e;
	int idx, k, n, dst, src;

	if (rec == NULL)
		return SCE_ERROR_ERRNO_EINVAL;

	/* drop older entries of the same error */
	idx = s_history.head;
	for (k = 9; k != 0; k--) {
		idx = (idx + 1) % HISTORY_ENTRIES;
		e = ErrorEntryAt(idx);
		if (e->rec.code == rec->code && e->rec.id == rec->id &&
		    memcmp(e->rec.key, rec->key, sizeof(rec->key)) == 0)
			memset(e, 0, sizeof(*e));
	}

	/* close the first gap behind the head, moving older entries up */
	dst = s_history.head;
	for (n = 0; n < 8; n++) {
		dst = (dst + 9) % HISTORY_ENTRIES;
		if (ErrorEntryAt(dst)->rec.code == 0)
			break;
	}
	if (n < 8) {
		src = s_history.head - n - 1;
		do {
			src = (src + 9) % HISTORY_ENTRIES;
			if (ErrorEntryAt(src)->rec.code == 0) {
				n++;
				continue;
			}
			memcpy(ErrorEntryAt(dst), ErrorEntryAt(src), sizeof(ErrorEntry));
			n++;
			dst = (dst + 9) % HISTORY_ENTRIES;
			memset(ErrorEntryAt(src), 0, sizeof(ErrorEntry));
		} while (n != 8);
	}

	e = ErrorEntryAt(s_history.head);
	memset(e, 0, sizeof(*e));
	memset(rec->reserved, 0, sizeof(rec->reserved));
	memcpy(&e->rec, rec, sizeof(*rec));
	memset(&fw, 0, sizeof(fw));
	fw.size = sizeof(fw);
	sceKernelGetSystemSwVersion(&fw);
	e->rec.swVersion = fw.version;
	e->rec.swVersionExtra = fw.versionExtra;
	if (s_defaultFormatSet & 1)
		e->rec.format = s_defaultFormat;
	sceRtcGetCurrentTick((SceRtcTick *)&e->tick);
	ErrorFormatCode(table, e->code, e->rec.code);

	if (s_history.seq == 0)
		s_history.seq = 1;
	e->seq = s_history.seq;
	s_history.seq++;
	s_history.head = (s_history.head + 1) % HISTORY_ENTRIES;
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorHistorySetDefaultFormat(const SceUInt32 *format)
{
	if (format == NULL || !(format[1] & 1) || (format[1] & ~1))
		return SCE_ERROR_ERRNO_EINVAL;
	s_defaultFormat = format[0];
	s_defaultFormatSet |= 1;
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorHistoryGet(SceUInt32 index, ErrorEntry *buf)
{
	if (buf == NULL || index > 9)
		return SCE_ERROR_ERRNO_EINVAL;
	memcpy(buf, ErrorEntryAt((int)(s_history.head + 9 - index) % HISTORY_ENTRIES), sizeof(*buf));
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorHistoryUpdateSequenceInfo(const ErrorSequenceInfo *info, int reserved)
{
	ErrorEntry *e;

	if (info == NULL || reserved != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	for (e = s_history.entry; e < s_history.entry + HISTORY_ENTRIES; e++) {
		if (e->tick == info->tick && e->seq == info->seq) {
			e->seqInfo = info->seqInfo;
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_EINVAL;
}

static __attribute__((noinline, noclone)) int ErrorHistoryClear(int reserved)
{
	if (reserved != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	memset(&s_history, 0, sizeof(s_history));
	s_history.version = 1;
	sceIoRemove(HISTORY_PATH);
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorHistoryLoad(void)
{
	SceUID fd;

	memset(&s_history, 0, sizeof(s_history));
	s_history.version = 1;
	fd = sceIoOpen(HISTORY_PATH, SCE_O_RDONLY, 0);
	if (fd >= 0) {
		sceIoRead(fd, &s_history, sizeof(s_history));
		sceIoClose(fd);
	}
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorHistorySave(void)
{
	SceUID fd;

	sceIoMkdir("vd0:history", 6);
	fd = sceIoOpen(HISTORY_PATH, SCE_O_WRONLY | SCE_O_CREAT, 6);
	if (fd < 0)
		return 0;
	if (sceIoWrite(fd, &s_history, sizeof(s_history)) == sizeof(s_history)) {
		sceIoClose(fd);
		fd = sceIoOpen(HISTORY_BACKUP, SCE_O_WRONLY | SCE_O_CREAT, 6);
		if (fd < 0)
			return 0;
		sceIoWrite(fd, &s_history, sizeof(s_history));
	}
	sceIoClose(fd);
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	ErrorSysEvent *ev = opt;

	if (resume == 0) {
		if (eventid == SYSEVENT_SUSPEND_PREPARE(0) && ev->suspend != NULL)
			return ev->suspend();
	} else if (resume == 1) {
		if (eventid == SYSEVENT_RESUME_IDLE && ev->resume != NULL)
			return ev->resume();
	}
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorSyncCreate(ErrorSync *sync, SceUInt32 signal, const char *name)
{
	SceUID evf;

	evf = sceKernelCreateEventFlag(name, SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (evf < 0)
		return evf;
	sync->evf = evf;
	sync->signal = signal;
	sync->waiters = 0;
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorSyncDelete(ErrorSync *sync)
{
	sceKernelDeleteEventFlag(sync->evf);
	return 0;
}

static __attribute__((noinline, noclone)) SceUInt32 ErrorSyncLock(ErrorSync *sync)
{
	SceUInt32 bits, signal;

	__atomic_fetch_add(&sync->waiters, 1, __ATOMIC_RELEASE);
	for (;;) {
		signal = __atomic_exchange_n(&sync->signal, 0, __ATOMIC_RELEASE);
		if (signal != 0)
			break;
		if (sceKernelWaitEventFlag(sync->evf, 1,
					    SCE_KERNEL_EVF_WAITMODE_OR |
					    SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					    &bits, NULL) < 0)
			break;
	}
	__atomic_fetch_sub(&sync->waiters, 1, __ATOMIC_RELEASE);
	return signal;
}

static __attribute__((noinline, noclone)) int ErrorSyncUnlock(ErrorSync *sync, SceUInt32 signal)
{
	__atomic_fetch_or(&sync->signal, signal, __ATOMIC_RELEASE);
	if (sync->waiters == 0)
		return 0;
	return sceKernelSetEventFlag(sync->evf, 1);
}

static __attribute__((noinline, noclone)) int ErrorSysEventRegister(ErrorSysEvent *ev)
{
	SceUID uid;

	uid = sceKernelRegisterSysEventHandler(ev->name, ErrorSysEventHandler, ev);
	if (uid < 0)
		return uid;
	ev->uid = uid;
	return 0;
}

static __attribute__((noinline, noclone)) int ErrorSysEventUnregister(SceUID uid)
{
	int ret = sceKernelUnregisterSysEventHandler(uid);

	return ret < 0 ? ret : 0;
}

/* run a kernel call from a syscall with TPIDRURO shifted (restored after) */
#define ERROR_SYSCALL(call) ({							\
	SceUInt32 __context = sceSyscallEnter();				\
	int __ret = (call);							\
										\
	sceSyscallLeave(__context);						\
	__ret;									\
})

static __attribute__((noinline, noclone)) int ErrorSyscallGetExternalString(char *buf, SceUInt32 code)
{
	return ERROR_SYSCALL(ErrorGetExternalString(buf, code));
}

static __attribute__((noinline, noclone)) int ErrorSyscallPost(ErrorRecord *rec)
{
	return ERROR_SYSCALL(ErrorPostLocked(rec));
}

static __attribute__((noinline, noclone)) int ErrorSyscallSetDefaultFormat(const SceUInt32 *format)
{
	return ERROR_SYSCALL(ErrorSetDefaultFormatLocked(format));
}

static __attribute__((noinline, noclone)) int ErrorSyscallGet(SceUInt32 index, ErrorEntry *buf)
{
	return ERROR_SYSCALL(ErrorGetLocked(index, buf));
}

static __attribute__((noinline, noclone)) int ErrorSyscallUpdateSequenceInfo(const ErrorSequenceInfo *info, int reserved)
{
	return ERROR_SYSCALL(ErrorUpdateSequenceInfoLocked(info, reserved));
}

static __attribute__((noinline, noclone)) int ErrorSyscallClear(int reserved)
{
	return ERROR_SYSCALL(ErrorClearLocked(reserved));
}

int _sceErrorGetExternalString(char *buf, SceUInt32 code)
{
	SceUInt32 kbuf[4];
	int ret, res;

	if (buf == NULL)
		return ErrorSyscallGetExternalString(NULL, code);
	kbuf[0] = 0;
	kbuf[1] = 0;
	kbuf[2] = 0;
	kbuf[3] = 0;
	res = ErrorSyscallGetExternalString((char *)kbuf, code);
	ret = sceKernelCopyToUser(buf, kbuf, sizeof(kbuf));
	return ret < 0 ? ret : res;
}

int _sceErrorHistoryPostError(const void *rec)
{
	ErrorRecord krec;
	ErrorRecord *p = NULL;
	int ret;

	if (rec != NULL) {
		ret = sceKernelCopyFromUser(&krec, rec, sizeof(krec));
		if (ret < 0)
			return ret;
		p = &krec;
	}
	return ErrorSyscallPost(p);
}

int _sceErrorHistorySetDefaultFormat(const void *format)
{
	SceUInt32 kformat[2];
	SceUInt32 *p = NULL;
	int ret;

	if (format != NULL) {
		ret = sceKernelCopyFromUser(kformat, format, sizeof(kformat));
		if (ret < 0)
			return ret;
		p = kformat;
	}
	return ErrorSyscallSetDefaultFormat(p);
}

int _sceErrorHistoryGetError(SceUInt32 index, void *buf)
{
	ErrorEntry kbuf;
	int ret, res;

	if (buf == NULL)
		return ErrorSyscallGet(index, NULL);
	memset(&kbuf, 0, sizeof(kbuf));
	res = ErrorSyscallGet(index, &kbuf);
	ret = sceKernelCopyToUser(buf, &kbuf, sizeof(kbuf));
	return ret < 0 ? ret : res;
}

int _sceErrorHistoryUpdateSequenceInfo(const void *info, int reserved)
{
	ErrorSequenceInfo kinfo;
	ErrorSequenceInfo *p = NULL;
	int ret;

	if (info != NULL) {
		ret = sceKernelCopyFromUser(&kinfo, info, sizeof(kinfo));
		if (ret < 0)
			return ret;
		p = &kinfo;
	}
	return ErrorSyscallUpdateSequenceInfo(p, reserved);
}

int _sceErrorHistoryClearError(int reserved)
{
	return ErrorSyscallClear(reserved);
}
