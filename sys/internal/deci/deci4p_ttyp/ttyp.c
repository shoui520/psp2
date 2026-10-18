/*
 * SceDeci4pTtyp: the TTYP channels (see ttyp.h).
 */

#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <psp2kern/kernel/modulemgr.h>
#include <sysmem/pmmgr.h>
#include <sysmem/procevent.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/threadmgr/debugger.h>
#include <sysmem/debug.h>
#include <deci4p_dfmgr/deci4p.h>
#include <pamgr/pamgr.h>
#include <threadmgr/threadmgr.h>
#include "ttyp.h"


#if FW_DEBUG
#define TTYP_PRINTF(_line, _fmt, ...)	SCE_PRINTF_WITH_INFO(0xf, _line, _fmt, ##__VA_ARGS__)
/* in code the original inlined into _func */
#define TTYP_PRINTF_IN(_func, _line, _fmt, ...)	\
	SceDebugForDriver_02B04343(0xf, SCE_DBGINFO_FUNC(_line, _func), _fmt, ##__VA_ARGS__)
#else
#define TTYP_PRINTF(_line, _fmt, ...)	do { } while (0)
#define TTYP_PRINTF_IN(_func, _line, _fmt, ...)	do { } while (0)
#endif

/* (inlined as in the original: their assertions report the caller's caller) */
#define TTYP_INLINE		static inline __attribute__((always_inline))

#if FW_DEBUG
#define TTYP_RESOURCE_HELPER	static
#else
#define TTYP_RESOURCE_HELPER	TTYP_INLINE
#endif

/* weak: the timestamp source may be absent */
SceUInt64 ScePamgrForDriver_FE9BFD27(void) __attribute__((weak));

static SceUID s_mutex;			/* queues, rings and resources */
static SceUID s_sema;			/* free resources */
static TtypResource s_resource[TTYP_RESOURCES];
static SceKernelSpinlock s_resourceLock;
static SceUID s_heap;
static TtypInput s_input[TTYP_CHANNELS];
static TtypChannel s_channel[TTYP_CHANNELS];

static int procExitHandler(SceUID pid, ProcEventParam *param, int args);
static int procKillHandler(SceUID pid, ProcEventParam *param, int args);

static const SceKernelProcEventHandler s_procHandler = {
	.size = sizeof(SceKernelProcEventHandler),
	.exit = procExitHandler,
	.kill = procKillHandler,
};

/* -- resources and the per-channel queues ------------------------------- */

TTYP_RESOURCE_HELPER TtypResource *ttypReserveResource(void)
{
	TtypResource *r;
	int ret, i;

	ret = ksceKernelWaitSema(s_sema, 1, NULL);
	if (ret < 0) {
		TTYP_PRINTF(369, "WaitSema failed (0x%08x)\n", ret);
		return NULL;
	}
	ret = ksceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0) {
		TTYP_PRINTF(376, "LockMutex failed (0x%08X)\n", ret);
		return NULL;
	}
	i = s_resource[0].state;
	if (i != 0) {
		if (s_resource[1].state != 0) {
			if (s_resource[2].state != 0) {
				SCE_DBG_ASSERT_LEVEL(0, 0, 391);
				r = NULL;
				goto out;
			}
			i = 2;
		} else {
			i = 1;
		}
	}
	r = &s_resource[i];
	r->state = 1;
out:
	ksceKernelUnlockMutex(s_mutex, 1);
	return r;
}

/* name unknown */
TTYP_INLINE void ttypReleaseResource(TtypResource *r)
{
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&s_resourceLock);
	memset(r, 0, sizeof(*r));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_resourceLock, intr);
	ksceKernelSignalSema(s_sema, 1);
}

TTYP_INLINE void ttypAddPendingRequest(TtypChannel *ch, TtypResource *r)
{
	int ret;

	ret = ksceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0) {
		TTYP_PRINTF(422, "LockMutex failed (0x%08X)\n", ret);
		return;
	}
	SCE_DBG_ASSERT_LEVEL(0, ch->tail->next == NULL, 426);
	SCE_DBG_ASSERT_LEVEL(0, r->next == NULL, 427);
	ch->tail->next = r;
	ch->tail = r;
	r->next = NULL;
	ksceKernelUnlockMutex(s_mutex, 1);
}

/* take r off the queue and release it */
TTYP_INLINE void ttypCancelRequest(TtypChannel *ch, TtypResource *r)
{
	TtypResource *prev, *p;
	int ret;

	ret = ksceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0) {
		TTYP_PRINTF(445, "LockMutex failed (0x%08X)\n", ret);
	} else {
		SCE_DBG_ASSERT_LEVEL(0, ch->tail->next == NULL, 449);
		prev = &ch->head;
		for (p = ch->head.next; p != NULL; prev = p, p = p->next) {
			if (p == r) {
				prev->next = r->next;
				if (r->next == NULL)
					ch->tail = prev;
				r->next = NULL;
				break;
			}
		}
		ksceKernelUnlockMutex(s_mutex, 1);
	}
	ttypReleaseResource(r);
}

/* name unknown: queue r on ch and ask SceDeci4pDfMgr to let the channel send */
TTYP_INLINE int ttypSendRequest(TtypChannel *ch, TtypResource *r)
{
	int ret;

	ttypAddPendingRequest(ch, r);
	ret = sceDfMgrReqSend(ch->handle);
	if (ret < 0 && ret != DFMGR_ERROR_BUSY)
		return ret;
	return 0;
}

static TtypResource *ttypCheckPendingRequest(TtypChannel *ch)
{
	TtypResource *r;
	int ret;

	ret = ksceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0) {
		TTYP_PRINTF(478, "LockMutex failed (0x%08X)\n", ret);
		return NULL;
	}
	r = ch->head.next;
	ksceKernelUnlockMutex(s_mutex, 1);
	return r;
}

/* dequeue the first request */
TTYP_INLINE TtypResource *ttypFindPendingRequest(TtypChannel *ch)
{
	TtypResource *r;
	int ret;

	ret = ksceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0) {
		TTYP_PRINTF(497, "LockMutex failed (0x%08X)\n", ret);
		return NULL;
	}
	r = ch->head.next;
	if (r != NULL) {
		ch->head.next = r->next;
		if (r->next == NULL)
			ch->tail = &ch->head;
		r->next = NULL;
	}
	ksceKernelUnlockMutex(s_mutex, 1);
	return r;
}

/* name unknown: the host went away, drop everything queued */
TTYP_RESOURCE_HELPER void ttypFlushPendingRequests(TtypChannel *ch)
{
	TtypResource *r;

	while ((r = ttypFindPendingRequest(ch)) != NULL)
		ttypReleaseResource(r);
}

/* -- DfMgr events ------------------------------------------------------- */

TTYP_INLINE void _prepareAck(TtypChannel *ch)
{
	TtypResource *r;
	int ret;

	r = ttypReserveResource();
	if (r == NULL) {
		TTYP_PRINTF(1114, "couldn't reserve resource\n");
		return;
	}
	r->hdr.pid = ch->rhdr.pid;
	r->hdr.port = ch->rhdr.port;
	r->hdr.tid = ch->rhdr.tid;
	r->hdr.tag = ch->rhdr.tag;
	r->hdr.type = TTYP_TYPE_ACK;
	SceDeci4pDfMgrForDebugger_6D26CC56(&r->hdr.hdr, sizeof(TtypHeader));
	r->hdr.len = 0;
	r->handle = ch->handle;
	r->state = 2;
	ret = ttypSendRequest(ch, r);
	if (ret != 0) {
		TTYP_PRINTF(1136, "ReqSend failed (0x%08X)\n", ret);
		ttypCancelRequest(ch, r);
	}
}

/* name unknown: received characters into the ring (with ^S / ^Q / ^C unless raw) */
TTYP_INLINE int ttypInput(TtypChannel *ch)
{
	TtypInput *in = ch->in;
	unsigned char *p, *next;
	SceSize n;
	SceUInt32 flags;
	unsigned char c;
	int ret;

	n = ch->rdata;
	if (n == 0 || ch->rlen <= sizeof(TtypHeader))
		return 0;
	ch->rdata = 0;
	ret = ksceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0) {
		TTYP_PRINTF_IN("TtyHandler", 583, "LockMutex failed (0x%08X)\n", ret);
		return ret;
	}
	p = ch->rbuf;
	do {
		flags = in->flags;
		c = *p;
		if (!(flags & TTYP_IN_RAW)) {
			if (c == 3) {
				if (flags & TTYP_IN_INTR) {
					ch->flags |= TTYP_CH_INTR;
					break;
				}
			} else if (c == 0) {
				goto store;
			}
			if ((c & 0x7f) == 0x13) {
				in->flags = flags | TTYP_IN_STOPPED;
				goto next;
			}
			if ((c & 0x7f) == 0x11) {
				in->flags = flags & ~TTYP_IN_STOPPED;
				goto next;
			}
		}
store:
		if (!(flags & TTYP_IN_STOPPED)) {
			next = in->wp + 1;
			if (next >= in->data + TTYP_IN_SIZE)
				next = in->data;
			if (in->rp != next) {
				*in->wp = c;
				in->wp = next;
			}
		}
next:
		n--;
		p++;
	} while (n != 0);
	ksceKernelUnlockMutex(s_mutex, 1);
	return 0;
}

static int TtyHandler(int event, void *arg, void *ctx)
{
	TtypChannel *ch = ctx;
	SceDfMgrStatus *status = arg;
	TtypInput *in = ch->in;
	TtypResource *r;
	SceSize len;
	int ret;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
	case SCE_DFMGR_EVENT_REQ_DONE:
		if (ttypInput(ch) < 0)
			return 0;
		if (event == SCE_DFMGR_EVENT_REQ_DONE) {
			ksceKernelSetEventFlag(ch->evf, 3);
			_prepareAck(ch);
			ch->rlen = 0;
		} else {
			len = ch->rlen;
			if (len < sizeof(TtypHeader)) {
				ch->rlen = sceDfMgrRecv(ch->handle, &ch->rhdr, sizeof(TtypHeader)) + len;
			} else {
				ch->rdata = sceDfMgrRecv(ch->handle, ch->rbuf, sizeof(ch->rbuf));
				ch->rlen += ch->rdata;
			}
		}
		break;
	case SCE_DFMGR_EVENT_SEND:
		r = ttypFindPendingRequest(ch);
		if (r != NULL) {
			SCE_DBG_ASSERT_LEVEL(0, ch->handle == r->handle, 545);
			ch->sending = r;
			len = SceDeci4pDfMgrForDebugger_CACAB5F9(&r->hdr.hdr);
			sceDfMgrSend(ch->handle, r, len);
			return 0;
		}
		TTYP_PRINTF(541, "unexpected WRITE event received.\n");
		break;
	case SCE_DFMGR_EVENT_SENT:
		r = ch->sending;
		if (r == NULL) {
			TTYP_PRINTF(554, "unexpected WRITEDONE event received.\n");
			break;
		}
		ttypReleaseResource(r);
		ch->sending = NULL;
		r = ttypCheckPendingRequest(ch);
		if (r != NULL) {
			SCE_DBG_ASSERT_LEVEL(0, ch->handle == r->handle, 564);
			ret = sceDfMgrReqSend(r->handle);
			if (ret < 0 && ret != DFMGR_ERROR_BUSY)
				TTYP_PRINTF(568, "ReqSend failed (0x%08X)\n", ret);
		}
		break;
	case SCE_DFMGR_EVENT_STATUS:
		if (status->kind == 0) {
			if (status->connected == 0) {
				in->flags |= TTYP_IN_STOPPED;
				ksceKernelSetEventFlag(ch->evf, 2);
				ttypFlushPendingRequests(ch);
			} else if (status->connected == 1) {
				in->flags &= ~TTYP_IN_STOPPED;
				ksceKernelSetEventFlag(ch->evf, 2);
			} else {
				TTYP_PRINTF(657, "unknown packet\n");
			}
		} else if (status->kind == 1) {
			if (status->enabled == 0) {
				ch->flags &= ~TTYP_CH_ENABLED;
				ksceKernelSetEventFlag(ch->evf, 2);
				ttypFlushPendingRequests(ch);
			} else if (status->enabled == 1) {
				ch->flags |= TTYP_CH_ENABLED;
				ksceKernelSetEventFlag(ch->evf, 2);
			} else {
				TTYP_PRINTF(676, "unknown packet\n");
			}
		} else {
			TTYP_PRINTF(681, "unknown code\n");
		}
		break;
	case 6:
		if (status->kind == 1) {
			in->flags |= TTYP_IN_STOPPED;
			ksceKernelSetEventFlag(ch->evf, 2);
		} else if (status->kind == 2) {
			ch->flags &= ~TTYP_CH_ENABLED;
			ksceKernelSetEventFlag(ch->evf, 2);
		}
		break;
	default:
		TTYP_PRINTF(687, "unknown event 0x%x.\n", event);
		return 0;
	}
	return 0;
}

/* a process ended: tell the host (on TTYP0) */
TTYP_INLINE int _prepareClose(SceUID pid)
{
	TtypChannel *ch = &s_channel[0];
	TtypResource *r;
	int ret;

	r = ttypReserveResource();
	if (r == NULL) {
		TTYP_PRINTF(1151, "couldn't reserve resource\n");
		return 0;
	}
	r->hdr.pid = pid;
	r->hdr.port = TTYP_PORT_PROCESS;
	r->hdr.type = TTYP_TYPE_CLOSE;
	r->hdr.tid = 0;
	r->hdr.tag = 0;
	SceDeci4pDfMgrForDebugger_6D26CC56(&r->hdr.hdr, sizeof(TtypHeader));
	r->hdr.len = 0;
	r->state = 2;
	r->handle = ch->handle;
	ret = ttypSendRequest(ch, r);
	if (ret != 0) {
		TTYP_PRINTF(1173, "ReqSend failed (0x%08X)\n", ret);
		ttypCancelRequest(ch, r);
	}
	return 0;
}

/* name unknown */
static int procKillHandler(SceUID pid, ProcEventParam *param, int args)
{
	return _prepareClose(pid);
}

/* name unknown */
static int procExitHandler(SceUID pid, ProcEventParam *param, int args)
{
	return _prepareClose(pid);
}

/* -- module start ------------------------------------------------------- */

/* inlined into TtypModuleStart in the original */
TTYP_INLINE int TtypInit(void)
{
	SceKernelHeapCreateOpt heapOpt;
	SceDfMgrProtocol proto;
	char name[32];
	SceUID uid;
	int i;

	memset(s_input, 0, sizeof(s_input));
	memset(s_channel, 0, sizeof(s_channel));

	heapOpt.size = sizeof(heapOpt);
	heapOpt.attr = 0x401;
	heapOpt.memtype = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
	uid = SceSysmemForDriver_9328E0E8("SceDeci4pTtyp", 0x1000, &heapOpt);
	if (uid < 0) {
		TTYP_PRINTF(242, "CreateHeap failed (0x%08X)\n", uid);
		return uid;
	}
	s_heap = uid;
	uid = ksceKernelCreateMutex("SceDeci4pTtyp", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	if (uid < 0) {
		TTYP_PRINTF(250, "CreateMutex failed (0x%08X)\n", uid);
		return uid;
	}
	s_mutex = uid;
	s_sema = ksceKernelCreateSema("SceDeci4pTtyp", SCE_KERNEL_ATTR_TH_PRIO,
				      TTYP_RESOURCES, TTYP_RESOURCES, NULL);
	if (s_sema < 0) {
		TTYP_PRINTF(262, "CreateSema failed (0x%08X)\n", s_sema);
		return s_sema;
	}

	proto.size = sizeof(proto);
	proto.alias = 0;
	proto.version = 0x1000004;
	proto.subVersion = 0x20001;
	proto.destination = 0;
	proto.flags = DFMGR_PROTOCOL_ENABLE;
	for (i = 0; i != TTYP_CHANNELS; i++) {
		proto.protocol = 0x80030000 + i;
		snprintf(proto.name, sizeof(proto.name), "TTYP%d", i);
		s_input[i].flags = 0;
		snprintf(name, sizeof(name), "SceDeci4pTtyp%d", i);
		s_channel[i].evf = ksceKernelCreateEventFlag(name, 0x9000, 0, NULL);
		if (s_channel[i].evf < 0)
			TTYP_PRINTF(286, "CreateEventFlag failed (0x%08x)\n", s_channel[i].evf);
		s_input[i].rp = s_input[i].data;
		s_input[i].wp = s_input[i].data;
		s_channel[i].in = &s_input[i];
		s_channel[i].flags = TTYP_CH_ENABLED;
		s_channel[i].handle = sceDfMgrOpen(TtyHandler, &proto, &s_channel[i]);
		memset(&s_channel[i].head, 0, sizeof(s_channel[i].head));
		s_channel[i].tail = &s_channel[i].head;
		s_channel[i].sending = NULL;
	}
	memset(s_resource, 0, sizeof(s_resource));
	TtypFsInitVfs();
	return 0;
}

int TtypModuleStart(SceSize args, void *argp)
{
	if (!SceDipswForDriver_A98FC2FD(0xc2) || !SceDipswForDriver_A98FC2FD(0xc7)) {
		SCE_DBG_PRINTF(0xb, 172, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (TtypInit() < 0)
		return SCE_KERNEL_START_FAILED;
	if (sceKernelRegisterProcEventHandler("SceDeci4pTtyp", &s_procHandler, 0) < 0)
		return SCE_KERNEL_START_FAILED;
	return SCE_KERNEL_START_SUCCESS;
}

/* -- the tty interface (ttyp_fs.c) -------------------------------------- */

/* name unknown */
int ttypLock(void)
{
	return ksceKernelLockMutex(s_mutex, 1, NULL);
}

/* name unknown */
void ttypUnlock(void)
{
	ksceKernelUnlockMutex(s_mutex, 1);
}

/* name unknown */
SceUID ttypGetHeap(void)
{
	return s_heap;
}

int TtypOpen(int unit)
{
	TtypChannel *ch;
	TtypInput *in;
	SceUID uid;
	int n;

	if (unit > TTYP_CHANNELS - 1)
		return SCE_ERROR_ERRNO_ENXIO;
	ch = &s_channel[unit];
	n = ch->openCount;
	if (n == 0) {
		in = ch->in;
		in->wp = in->data;
		in->rp = in->data;
		uid = ksceKernelCreateMutex("SceDeci4pTtyRead", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
		if (uid < 0) {
			TTYP_PRINTF(707, "sceKernelCreateMutex[r] failed 0x%x\n", uid);
			ksceKernelUnlockMutex(s_mutex, 1);
			return uid;
		}
		ch->readMutex = uid;
		uid = ksceKernelCreateMutex("SceDeci4pTtyWrite", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
		if (uid < 0) {
			TTYP_PRINTF(715, "sceKernelCreateMutex[w] failed 0x%x\n", uid);
			ksceKernelDeleteMutex(ch->readMutex);
			ch->readMutex = -1;
			ksceKernelUnlockMutex(s_mutex, 1);
			return uid;
		}
		n = ch->openCount;
		ch->writeMutex = uid;
	}
	ch->openCount = n + 1;
	return 0;
}

int TtypClose(int unit)
{
	TtypChannel *ch;
	SceUID r, w;

	if (unit >= TTYP_CHANNELS)
		return SCE_ERROR_ERRNO_ENXIO;
	ch = &s_channel[unit];
	if (--ch->openCount != 0)
		return 0;
	r = ch->readMutex;
	w = ch->writeMutex;
	ch->readMutex = 0;
	ch->writeMutex = 0;
	if (r > 0)
		ksceKernelDeleteMutex(r);
	if (w > 0)
		ksceKernelDeleteMutex(w);
	ksceKernelSetEventFlag(ch->evf, 4);
	ksceKernelClearEventFlag(ch->evf, ~4);
	return 0;
}

static int _circgetc(TtypInput *in)
{
	unsigned char *p = in->rp;
	int c;

	if (in->wp == p) {
		TTYP_PRINTF(1078, "_circgetc empty error.\n");
		return 0;
	}
	c = *in->rp++;
	if (in->rp >= in->data + TTYP_IN_SIZE)
		in->rp = in->data;
	return c;
}

int TtypRead(int unit, void *buf, SceSize size, SceUInt32 flags)
{
	TtypChannel *ch = &s_channel[unit];
	TtypInput *in = ch->in;
	unsigned char *b = buf;
	unsigned int outBits, n, k, j, m;
	int ret, c;

	if ((int)size < 0) {
		TTYP_PRINTF(792, "invalid size (%d)\n", size);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ret = ksceKernelLockMutex(ch->readMutex, 1, NULL);
	if (ret < 0) {
		TTYP_PRINTF(797, "LockMutex failed (0x%08X)\n", ret);
		return ret;
	}
	ret = 0;
	if (size == 0)
		goto out;

	n = 0;
	do {
		ret = n;
		if (!(flags & 4) && in->wp == in->rp) {
			do {
				c = ksceKernelWaitEventFlag(ch->evf, 5, SCE_KERNEL_EVF_WAITMODE_OR,
							    &outBits, NULL);
				if (c < 0) {
					if (c != SCE_KERNEL_ERROR_THREAD_SUSPENDED)
						TTYP_PRINTF(814, "WaitEventFlag failed (0x%08X)\n", c);
					goto out;
				}
				ksceKernelClearEventFlag(ch->evf, ~1);
			} while (!(outBits & 4) && in->wp == in->rp);
		}
		if (in->rp == in->wp)
			break;

		ret = ksceKernelLockMutex(s_mutex, 1, NULL);
		if (ret < 0) {
			/* (returns with the read lock held) */
			TTYP_PRINTF(836, "LockMutex failed (0x%08X)\n", ret);
			return ret;
		}
		c = _circgetc(in);
		ksceKernelUnlockMutex(s_mutex, 1);

		if (!sceKernelIsUserModeThread() || (in->flags & TTYP_IN_RAW) || c == 0)
			goto store;
		ret = n;
		if (c == '\r')
			goto next;
		if (c == '\n') {
			b[n] = '\n';
			ret = n + 1;
			break;
		}
		if (c != 8 && c != 0x7f)
			goto store;

		/*
		 * erase: one character, a whole UTF-8 sequence. Quirk: c becomes
		 * the byte examined last, and that is what an invalid sequence
		 * stores (not the erase character).
		 */
		if (n == 0)
			continue;
		ret = n - 1;
		c = b[n - 1];
		if ((c & 0x80) && (ret = n, (c & 0xc0) == 0x80)) {
			k = 0;
			do {
				j = k;
				k = j + 1;
				if (k > 5 || k >= n)
					break;
				c = b[n - 1 - k];
			} while ((c & 0xc0) == 0x80);
			if (k != 0 && k != 6) {
				m = 1u << (5 - j);
				if ((m ^ ((m - 1) | c)) != 0xff)
					goto store;
				ret = n - k - 1;
			}
		}
		goto next;
store:
		b[n] = c;
		ret = n + 1;
next:
		n = ret;
	} while ((unsigned int)ret < size);
out:
	ksceKernelUnlockMutex(ch->readMutex, 1);
	return ret;
}

/* send buf to the host in packets, stopping when it goes away */
TTYP_INLINE void consput(TtypChannel *ch, const signed char *buf, SceSize size, const SceUID *pid)
{
	SceKernelThreadContextInfo info;
	TtypResource *r;
	SceUInt64 time;
	SceSize left = size, n;
	int ret;

	while ((r = ttypReserveResource()) != NULL) {
		r->hdr.type = TTYP_TYPE_DATA;
		if (!sceKernelIsUserModeThread()) {
			if (pid == NULL) {
				r->hdr.pid = 0;
				r->hdr.tid = 0;
				r->hdr.port = TTYP_PORT_KERNEL;
			} else {
				r->hdr.tid = 0;
				r->hdr.port = TTYP_PORT_PROCESS;
				r->hdr.pid = *pid;
			}
		} else {
			r->hdr.port = TTYP_PORT_PROCESS;
			ksceKernelGetThreadContextInfo(&info);
			r->hdr.pid = info.process_id;
			r->hdr.tid = ksceKernelGetUserThreadId(info.thread_id);
		}
		time = 0;
		if (ScePamgrForDriver_FE9BFD27 != NULL)
			time = ScePamgrForDriver_FE9BFD27();
		n = left > TTYP_DATA_MAX - 1 ? TTYP_DATA_MAX : left;
		r->hdr.time[0] = time;
		r->hdr.time[1] = time >> 32;
		memcpy(r->data, buf, n);
		SceDeci4pDfMgrForDebugger_6D26CC56(&r->hdr.hdr, n + sizeof(TtypHeader));
		r->hdr.len = n;
		r->state = 2;
		r->handle = ch->handle;
		ttypAddPendingRequest(ch, r);
		ret = sceDfMgrReqSend(ch->handle);
		if (ret < 0 && ret != DFMGR_ERROR_BUSY) {
			if (ret == DFMGR_ERROR_NOT_CONNECTED)
				ch->in->flags |= TTYP_IN_STOPPED;
			else
				ch->flags &= ~TTYP_CH_ENABLED;
			ttypCancelRequest(ch, r);
		}
		if (left - n == 0 || !(ch->flags & TTYP_CH_ENABLED))
			return;
		left -= n;
		buf += n;
		if (ch->in->flags & TTYP_IN_STOPPED)
			return;
	}
	TTYP_PRINTF(973, "couldn't reserve resource\n");
}

int TtypWrite(int unit, const void *data, SceSize size, const SceUID *pid)
{
	TtypChannel *ch = &s_channel[unit];
	const signed char *buf = data;
	SceSize i;
	char mode;
	int ret;

	if ((int)size < 0) {
		TTYP_PRINTF(902, "invalid size (%d)\n", size);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ret = ksceKernelLockMutex(ch->writeMutex, 1, NULL);
	if (ret < 0) {
		TTYP_PRINTF(909, "LockMutex failed (0x%08X)\n", ret);
		return ret;
	}

	mode = 0;
	ScePmMgrForDriver_2AC815A2((SceProductMode *)&mode);
	if (mode == 0) {
		if (size != 0) {
			if ((ch->flags & TTYP_CH_ENABLED) && !(ch->in->flags & TTYP_IN_STOPPED))
				consput(ch, buf, size, pid);
			for (i = 0; i != size; i++)
				SceDebugForKernel_3A21194E(buf[i]);
		}
	} else if (mode == 1) {
		for (i = 0; i < size; ) {
			if (i + 1 < size && buf[i] == '\\' && buf[i + 1] == 'n') {
				SceDebugForDriver_391B74B7("\n");
				i += 2;
			} else {
				SceDebugForDriver_391B74B7("%c", buf[i]);
				i++;
			}
		}
	} else {
		SceDebugForDriver_391B74B7("unknown mode %x\n", mode);
		SCE_DBG_ASSERT_LEVEL(0, 0, 937);
	}
	ksceKernelUnlockMutex(ch->writeMutex, 1);
	return size;
}
