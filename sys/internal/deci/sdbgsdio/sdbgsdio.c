/*
 * SceSDbgSdio: byte-stream channels to the development host over the debug
 * SDIO link (DevKit/TestKit only; dipsw 0xC1 enables it).
 *
 * The window protocol is in sdbgsdio/sdio_window.h. Received bytes that do not
 * fit the caller's buffer are kept in the channel and returned first by the
 * next read.
 */

#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sdbgsdio/sdbgsdio.h>
#include <sdbgsdio/sdio_window.h>
#include <sysmem/suspend.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/barrier.h>

#define SDBG_SDIO_CHANNELS	4
#define SDBG_SDIO_PADDR		SDIO_WINDOW_PADDR(0)
#define SDBG_SDIO_INTR_BASE	0xb0		/* + channel */
#define SDBG_SDIO_BUF_SIZE	0x11000
#define SDBG_SDIO_SUSPEND_IDX	0x1a

typedef struct SDbgSdioChannel {
	volatile SdioSharedMemory *shm;
	volatile SceUInt32 *status;
	volatile SceUInt32 *txControl;
	volatile SceUInt32 *rxControl;		/* same register as txControl */
	volatile SceUInt32 *status2;		/* same register as status, unused */
	SdioPipe tx;
	SdioPipe rx;
	SceUInt32 index;
	SceUInt8 buffer[SDBG_SDIO_BUF_SIZE];	/* received bytes not read yet */
	SceUInt8 *pending;
	SceSize pendingSize;
} SDbgSdioChannel;

static struct {
	SceUInt32 index;			/* of the SDIO function: 0 */
	SceUID memblock;
	SceUInt32 initialized;
	SceUInt32 nChannels;
	SDbgSdioChannel channel[SDBG_SDIO_CHANNELS];
} s_sdbgSdio;

/* CIS tuples: MANFID 0xff55/0xaabb, FUNCE, FUNCID 0x0c, vendor tuple 0x80 */
static const SceUInt8 s_cisFunc0[] = {
	0x20, 0x04, 0x55, 0xff, 0xbb, 0xaa,
	0x22, 0x04, 0x00, 0x00, 0x08, 0x32,
	0xff, 0x01,
};

static const SceUInt8 s_cisFunc1[] = {
	[0x00] = 0x20, 0x04, 0x55, 0xff, 0xbb, 0xaa,
	[0x06] = 0x22, 0x2a, 0x01, 0x01,
	[0x15] = 0x08, 0x00, 0x00, 0xfc,
	[0x32] = 0x21, 0x02, 0x0c, 0x00,
	[0x36] = 0x80, 0x02, 0x01, 0x01,
	[0x3a] = 0xff, 0x01,
};

static inline void CopyToShm(volatile SceUInt8 *dst, const void *src, SceSize size)
{
	const SceUInt8 *s = src, *end = s + size;

	while (s != end)
		*dst++ = *s++;
}

static inline void CopyFromShm(void *dst, const volatile SceUInt8 *src, SceSize size)
{
	SceUInt8 *d = dst, *end = d + size;

	while (d != end)
		*d++ = *src++;
}

/* wait for a status event and acknowledge it */
static inline void WaitStatus(volatile SceUInt32 *status, SceUInt32 bits)
{
	while ((*status & bits) == 0)
		;
	*status = bits;
	(void)*status;
	dsb();
}

/* name unknown: publish the CIS once (cpu 0 only on resume) */
static int SetupCis(void)
{
	volatile SdioSharedMemory *shm = s_sdbgSdio.channel[0].shm;
	volatile SceUInt32 *valid = &shm->cisValid;

	if (*valid == 1)
		return 0;

	CopyToShm(shm->cisFunc0, s_cisFunc0, sizeof(s_cisFunc0));
	CopyToShm(shm->cisFunc1, s_cisFunc1, sizeof(s_cisFunc1));
	*valid = 0;
	*valid = 1;
	return 0;
}

/* name unknown: suspend handler */
static int SDbgSdioSuspend(void *args)
{
	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0)
		s_sdbgSdio.channel[0].shm->cisValid = 0;
	sceKernelSysrootCorelockUnlock();
	return 0;
}

/* name unknown: resume handler */
static int SDbgSdioResume(void *args)
{
	if (sceKernelCpuId() == 0)
		SetupCis();
	return 0;
}

int sceSDbgSdioInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SDbgSdioChannel *ch;
	volatile SdioSharedMemory *shm;
	SceUID uid;
	int i;

	memset(&s_sdbgSdio, 0, sizeof(s_sdbgSdio));

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = SDBG_SDIO_PADDR;
	uid = SceSysmemForDriver_C94850C9("SceSDbgSdio0", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
				      SDIO_WINDOW_SIZE, &opt);
	if (uid < 0) {
		SCE_DBG_ASSERT(0, 277);
		return uid;
	}
	SceSysmemForDriver_A841EDDA(uid, (void **)&shm);

	s_sdbgSdio.index = 0;
	s_sdbgSdio.memblock = uid;
	s_sdbgSdio.initialized = 1;
	s_sdbgSdio.nChannels = SDBG_SDIO_CHANNELS;

	for (i = 0; i < SDBG_SDIO_CHANNELS; i++) {
		ch = &s_sdbgSdio.channel[i];
		ch->index = i;
		ch->shm = shm;
		ch->status = &shm->channel[i].status;
		ch->txControl = &shm->channel[i].control;
		ch->rxControl = &shm->channel[i].control;
		ch->status2 = &shm->channel[i].status;

		ch->tx.desc = shm->txWindow[i].desc;
		ch->tx.reply = shm->txWindow[i].reply;
		ch->tx.inlineData = shm->txWindow[i].inlineData;
		ch->tx.data = shm->txWindow[i].desc;
		ch->tx.inlineMax = INLINE_MAX;
		ch->tx.dmaOffset = 0;
		ch->tx.offset = SHM_TX_WINDOW(i);
		ch->tx.chunkSize = WINDOW_SIZE;

		ch->rx.desc = shm->rxWindow[i].desc;
		ch->rx.reply = shm->rxWindow[i].reply;
		ch->rx.inlineData = shm->rxWindow[i].inlineData;
		ch->rx.data = shm->rxWindow[i].desc;
		ch->rx.inlineMax = INLINE_MAX;
		ch->rx.dmaOffset = 0;
		ch->rx.offset = SHM_RX_WINDOW(i);
		ch->rx.chunkSize = WINDOW_SIZE;

		ch->pending = NULL;
		ch->pendingSize = 0;
	}

	SetupCis();
	return 0;
}

int sceSDbgSdioModuleStart(SceSize args, void *argp)
{
	if (!SceDipswForDriver_A98FC2FD(0xc1)) {
		SCE_DBG_PRINTF(0xb, 210, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (sceSDbgSdioInit() < 0)
		return SCE_KERNEL_START_NO_RESIDENT;

	SceKernelSuspendForDriver_CE7A2207(SDBG_SDIO_SUSPEND_IDX, SDbgSdioSuspend, NULL);
	SceKernelSuspendForDriver_105C5752(SDBG_SDIO_SUSPEND_IDX, SDbgSdioResume, NULL);
	return SCE_KERNEL_START_SUCCESS;
}

int SceSDbgSdioForKernel_356C6621(void)
{
	SceSysmemForDriver_009E1C61(s_sdbgSdio.memblock);
	return 0;
}

/* take up to size bytes of the next host message; keep the rest */
static SceSize ReceiveMessage(SDbgSdioChannel *ch, SceUInt8 *dst, SceSize size)
{
	SdioHostDesc msg;
	SdioWindowDesc reply;
	SceSize left, chunk, ret;
	SceUInt8 *p;
	int buffered, first;

	WaitStatus(ch->status, STATUS_RX_NOTIFY | STATUS_RX_MESSAGE);
	CopyFromShm(&msg, ch->rx.desc, sizeof(msg));

	if (msg.flags & DESC_INLINE) {
		WaitStatus(ch->status, STATUS_RX_DATA);
		if (size < msg.size) {
			CopyFromShm(dst, ch->rx.inlineData, size);
			ch->pendingSize = msg.size - size;
			ch->pending = ch->buffer;
			CopyFromShm(ch->buffer, ch->rx.inlineData + size, msg.size - size);
			ret = size;
		} else {
			CopyFromShm(dst, ch->rx.inlineData, msg.size);
			ret = msg.size;
		}
		*ch->rxControl = CTRL_RX_DATA;
		return ret;
	}

	buffered = size < msg.size;
	ret = buffered ? size : msg.size;

	reply.offset = ch->rx.offset;
	reply.size = msg.size;
	reply.flags = 0;
	reply.chunkSize = ch->rx.chunkSize;
	CopyToShm(ch->rx.reply, &reply, sizeof(reply));
	*ch->rxControl = CTRL_RX_REQUEST;
	WaitStatus(ch->status, STATUS_RX_DATA);

	p = dst;
	if (buffered) {
		p = ch->buffer;
		ch->pending = ch->buffer;
		ch->pendingSize = msg.size;
	}

	for (left = msg.size, first = 1; left != 0; left -= chunk, first = 0) {
		chunk = left < reply.chunkSize ? left : reply.chunkSize;
		if (!first)
			WaitStatus(ch->status, STATUS_RX_DATA);
		CopyFromShm(p, ch->rx.data, chunk);
		p += chunk;
		*ch->rxControl = CTRL_RX_DATA;
	}

	if (buffered) {
		memcpy(dst, ch->pending, size);
		ch->pending += size;
		ch->pendingSize -= size;
	}
	return ret;
}

/* name unknown: read from a channel */
int SceSDbgSdioForKernel_2EC24C58(SceUInt32 channel, void *buf, SceSize size)
{
	SDbgSdioChannel *ch;
	SceUInt8 *dst = buf;
	SceSize n;

	if (channel >= SDBG_SDIO_CHANNELS)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ch = &s_sdbgSdio.channel[channel];

	n = ch->pendingSize;
	if (n != 0) {
		if (size < n)
			n = size;
		dst = memcpy(dst, ch->pending, n);
		ch->pending += n;
		ch->pendingSize -= n;
	}
	if (size <= n)
		return n;
	if ((*ch->status & STATUS_PENDING) == 0)
		return n;

	return n + ReceiveMessage(ch, dst + n, size - n);
}

/* name unknown: write to a channel */
int SceSDbgSdioForKernel_7876806C(SceUInt32 channel, const void *buf, SceSize size)
{
	const SceUInt8 *src = buf;
	SDbgSdioChannel *ch;
	SdioWindowDesc desc;
	SceSize left, chunk;
	int intr, first;

	if (channel >= SDBG_SDIO_CHANNELS)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ch = &s_sdbgSdio.channel[channel];
	intr = SDBG_SDIO_INTR_BASE + ch->index;

	desc.offset = ch->tx.offset;
	desc.size = size;
	desc.flags = 0;
	desc.chunkSize = ch->tx.chunkSize;
	left = size;
	if (size <= ch->tx.inlineMax) {
		CopyToShm(ch->tx.inlineData, src, size);
		src += size;
		left = 0;
		desc.offset += WINDOW_INLINE;
		desc.flags = DESC_INLINE;
	}
	CopyToShm(ch->tx.desc, &desc, sizeof(desc));

	sceKernelDisableIntr(intr);
	*ch->txControl = CTRL_TX_MESSAGE;

	if (desc.flags == 0 || left != 0) {
		WaitStatus(ch->status, STATUS_TX_ACCEPTED);
		sceKernelEnableIntr(intr);

		for (first = 1; left != 0; left -= chunk, first = 0) {
			chunk = left < desc.chunkSize ? left : desc.chunkSize;
			if (!first) {
				WaitStatus(ch->status, STATUS_TX_DATA);
				sceKernelEnableIntr(intr);
			}
			CopyToShm(ch->tx.data, src, chunk);
			src += chunk;
			sceKernelDisableIntr(intr);
			*ch->txControl = CTRL_TX_DATA;
		}
	}

	WaitStatus(ch->status, STATUS_TX_DATA);
	sceKernelEnableIntr(intr);
	return size;
}
