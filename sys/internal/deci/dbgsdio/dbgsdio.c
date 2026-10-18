/* SceDbgSdio: two SDIO debug channels with mailbox and DMA transfers. */
#include <sysmem/sysmem.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <dbgsdio/dbgsdio.h>
#include <sysmem/debug.h>
#include <intrmgr/intrmgr.h>
#include <sdbgsdio/sdio_window.h>
#include <sysmem/sysevent.h>
#include <sysmem/barrier.h>

#define DBGSDIO_CHANNELS	2
#define DBGSDIO_PADDR		SDIO_WINDOW_PADDR(1)
#define DBGSDIO_FUNCTION	1		/* the SDIO function: SceDbgSdio1 */
#define DBGSDIO_INTR_BASE	0xb8		/* + channel */
#define DBGSDIO_BUF_SIZE	0x11000
#define DBGSDIO_DMA_ALIGN	64		/* DMA moves whole 64-byte blocks */
#define DBGSDIO_DMA_MIN		64		/* shorter messages are copied */
#define DBGSDIO_RANGES		32		/* physical ranges of one write */

#define DBGSDIO_ERROR_BUSY	0x80380001
#define DBGSDIO_ERROR		0x80380005
#define DBGSDIO_ERROR_ARGUMENT	0x80380006
#define DBGSDIO_ERROR_OVERFLOW	0x80380007

/* the DMA transfers to and from the window (DMAC6, channel 0: tx, 1: rx) */
#define DMA_TX_LENGTH		0x40000000	/* len flags: to the device */
#define DMA_TX_COMMAND		0x00100000
#define DMA_TX_BLOCK		(DBGSDIO_DMA_ALIGN << SCE_KERNEL_DMAC_BLOCKSIZE_DST_SHIFT)
#define DMA_RX_LENGTH		0x04000000	/* len flags: from the device */
#define DMA_RX_COMMAND		0x0000a000
#define DMA_RX_BLOCK		(DBGSDIO_DMA_ALIGN << SCE_KERNEL_DMAC_BLOCKSIZE_SRC_SHIFT)
#define DMA_COMMAND_LAST	0x00001000	/* the last tag of the chain */
#define DMA_COHERENCY		0x73		/* L2 coherent virtual accesses */

/* SceSysmemForDriver_0AAA4FDD: mapped at its physical address */
#define PHYSICAL_MEMORY_TYPE_DIRECT	3

struct DbgSdio;

typedef struct DbgSdioChannel {
	struct DbgSdio *owner;
	volatile SdioSharedMemory *shm;
	volatile SceUInt32 *status;
	volatile SceUInt32 *txControl;
	volatile SceUInt32 *rxControl;		/* same register as txControl */
	SdioPipe tx;
	SdioPipe rx;
	SceUInt32 active;			/* a client attached its event flag */
	SceUInt32 index;
	SceUID event;				/* the status bits, set by the interrupt */
	SceUID notify;				/* the client's: bit 0 = data arrived */
	SceUInt32 reserved[5];
	SceUInt8 *pending;
	SceSize pendingSize;
	SceUInt8 txTail[DBGSDIO_DMA_ALIGN];	/* the last partial DMA block */
	SceUInt8 rxTail[DBGSDIO_DMA_ALIGN];
	SceUInt8 buffer[DBGSDIO_BUF_SIZE];	/* received bytes not read yet */
} DbgSdioChannel;

static struct DbgSdio {
	DbgSdioChannel channel[DBGSDIO_CHANNELS];
	SceUInt32 nChannels;
	SceKernelSpinlock lock;
	SceUInt32 reserved0;
	SceUInt32 function;			/* DBGSDIO_FUNCTION */
	SceUID memblock;
	SceKernelFastMutex mutex;
	SceKernelDmaOpId txDma;
	SceKernelDmaOpId rxDma;
	SceUID sysEvent;
	SceUInt32 suspended;
	SceUInt8 reserved1[0x1c];
} s_sdio;

_Static_assert(sizeof(DbgSdioChannel) == 0x11100, "DbgSdioChannel size");
_Static_assert(sizeof(s_sdio) == 0x22280, "DbgSdio size");

/* name unknown: byte accesses keep to the window protocol */
static void copyBytes(volatile void *dst, const volatile void *src, SceSize size)
{
	volatile SceUInt8 *d = dst;
	const volatile SceUInt8 *s = src;

	while (size--)
		*d++ = *s++;
}

static void barrier(void)
{
	dsb();
}

/* name unknown: publish the CIS and enable the SDIO function */
static int configureFunction(void)
{
	static const SceUInt8 cis0[] = {
		0x20, 0x04, 0x55, 0xff, 0xbb, 0xaa,
		0x22, 0x04, 0x00, 0x00, 0x08, 0x32,
		0xff, 0x01,
	};
	/* MANFID, FUNCE, FUNCID and the vendor tuple; byte 11 is the function */
	static const SceUInt8 cis1[0x3c] = {
		[0x00] = 0x20, 0x04, 0x55, 0xff, 0xbb, 0xaa,
		[0x06] = 0x22, 0x2a, 0x01, 0x01,
		[0x15] = 0x08, 0x00, 0x00, 0xfc,
		[0x32] = 0x21, 0x02, 0x0c, 0x00,
		[0x36] = 0x80, 0x02, 0x01, 0x01,
		[0x3a] = 0xff, 0x01,
	};
	volatile SdioSharedMemory *shm = s_sdio.channel[0].shm;
	volatile SceUInt32 *valid = &shm->cisValid;
	unsigned int i;

	ScePervasiveForDriver_F345FB53(s_sdio.function);
	ScePervasiveForDriver_EDCA3961(s_sdio.function);
	ScePervasiveForDriver_D34E74C5(s_sdio.function);
	copyBytes(shm->cisFunc0, cis0, sizeof(cis0));
	for (i = 0; i < sizeof(cis1); i++)
		shm->cisFunc1[i] = i == 11 ? s_sdio.function : cis1[i];
	*valid = 0;
	*valid = 1;
	return 0;
}

/* name unknown: suspend/resume event handler */
static int sysEvent(int resume, int event, void *args, void *opt)
{
	if (!resume) {
		if (event == SYSEVENT_SUSPEND_START)
			s_sdio.suspended = 1;
		else if (event == SYSEVENT_SUSPEND_DEVICE(3))
			s_sdio.channel[0].shm->cisValid = 0;
	} else if (resume == 1 && event == SYSEVENT_RESUME_DEVICE(3)) {
		s_sdio.suspended = 0;
		configureFunction();
	}
	return 0;
}

static int sceDbgSdioMboxInterrupt(int code, void *arg)
{
	DbgSdioChannel *c = arg;
	struct DbgSdio *sdio = c->owner;
	SceUInt32 bits, state;
	int ret;

	if (!(c->shm->intrPending & (1 << c->index)))
		return -1;
	bits = *c->status;
	*c->status = bits & STATUS_PENDING;
	(void)*c->status;
	barrier();
	ret = sceKernelSetEventFlag(c->event, bits & ~STATUS_RX_NOTIFY);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(0, 1674, "0x%x %d 0x%x\n", DBGSDIO_ERROR, 1674, ret);
	state = SceCpuForDriver_D32ACE9E(&sdio->lock);
	if (c->active && (bits & STATUS_RX_NOTIFY)) {
		ret = sceKernelSetEventFlag(c->notify, 1);
		if (ret < 0)
			SCE_PRINTF_WITH_INFO(0, 1685, "0x%x %d 0x%x\n", DBGSDIO_ERROR, 1685, ret);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&sdio->lock, state);
	return -1;
}

static void initPipe(SdioPipe *pipe, volatile SdioWindow *window, SceUInt32 offset)
{
	pipe->desc = window->desc;
	pipe->reply = window->reply;
	pipe->inlineData = window->inlineData;
	pipe->data = window->desc;		/* chunks fill the whole window */
	pipe->inlineMax = INLINE_MAX;
	pipe->dmaOffset = 0;
	pipe->offset = offset;
	pipe->chunkSize = WINDOW_SIZE;
}

int sceDbgSdioInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	volatile SdioSharedMemory *shm;
	SceKernelDmaOpId tx, rx;
	unsigned int i;
	int ret;

	memset(&s_sdio, 0, sizeof(s_sdio));
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = DBGSDIO_PADDR;
	ret = sceKernelAllocMemBlockForKernel("SceDbgSdio1", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
				      SDIO_WINDOW_SIZE, &opt);
	if (ret < 0)
		return ret;
	sceKernelGetMemBlockBase(ret, (void **)&shm);
	s_sdio.memblock = ret;

	ret = sceKernelInitializeFastMutex(&s_sdio.mutex, "SceDbgSdio1", 2, 0);
	if (ret < 0)
		goto freeBlock;

	tx = sceKernelDmaOpAlloc("SceDbgSdioTx");
	if (tx < 0) {
		SCE_DBG_PRINTF(0, 295, "%s. DmaOpAlloc error:0x%x\n", __func__, tx);
		ret = tx;
		goto freeMutex;
	}
	ret = sceKernelDmaOpAssign(tx, SCE_KERNEL_DMAC_ID_DMAC6, 0);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 302, "%s. DmaOpAssign error:0x%x\n", __func__, ret);
		goto freeTx;
	}
	rx = sceKernelDmaOpAlloc("SceDbgSdioRx");
	if (rx < 0) {
		SCE_DBG_PRINTF(0, 310, "%s. DmaOpAlloc error:0x%x\n", __func__, rx);
		ret = rx;
		goto freeTx;
	}
	ret = sceKernelDmaOpAssign(rx, SCE_KERNEL_DMAC_ID_DMAC6, 1);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 318, "%s. DmaOpAssign error:0x%x\n", __func__, ret);
		sceKernelDmaOpFree(rx);
		goto freeTx;
	}

	for (i = 0; i < DBGSDIO_CHANNELS; i++) {
		DbgSdioChannel *c = &s_sdio.channel[i];

		c->owner = &s_sdio;
		c->shm = shm;
		c->status = &shm->channel[i].status;
		c->txControl = &shm->channel[i].control;
		c->rxControl = &shm->channel[i].control;
		initPipe(&c->tx, &shm->txWindow[i], SHM_TX_WINDOW(i));
		initPipe(&c->rx, &shm->rxWindow[i], SHM_RX_WINDOW(i));
		c->active = 0;
		c->index = i;
	}
	s_sdio.nChannels = DBGSDIO_CHANNELS;
	s_sdio.lock = 0;
	s_sdio.function = DBGSDIO_FUNCTION;
	s_sdio.txDma = tx;
	s_sdio.rxDma = rx;
	configureFunction();
	s_sdio.sysEvent = sceKernelRegisterSysEventHandler("SceDbgSdio1", sysEvent, NULL);
	return 0;

freeTx:
	sceKernelDmaOpFree(tx);
freeMutex:
	sceKernelFinalizeFastMutex(&s_sdio.mutex);
freeBlock:
	SceSysmemForDriver_009E1C61(s_sdio.memblock);
	return ret;
}

int _sceDbgSdioModuleStart(SceSize args, void *argp)
{
	if (!SceDipswForDriver_A98FC2FD(0xc2)) {
		SCE_DBG_PRINTF(0xb, 225, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (sceDbgSdioInit() < 0)
		return SCE_KERNEL_START_NO_RESIDENT;
	return SCE_KERNEL_START_SUCCESS;
}

/* name unknown: attach a client event flag to a channel's mailbox interrupt */
int SceDbgSdioForDriver_3D3FAF3A(SceUInt32 index, SceUID notify)
{
	DbgSdioChannel *c;
	SceUInt32 state;
	SceUID event;
	int ret;

	if (index >= s_sdio.nChannels)
		return DBGSDIO_ERROR_ARGUMENT;
	c = &s_sdio.channel[index];

	state = SceCpuForDriver_D32ACE9E(&s_sdio.lock);
	if (c->active) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdio.lock, state);
		return DBGSDIO_ERROR_BUSY;
	}
	event = sceKernelCreateEventFlag("SceDbgSdio1", 0, 0, NULL);
	if (event < 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdio.lock, state);
		return event;
	}
	ret = sceKernelRegisterIntrHandler(DBGSDIO_INTR_BASE + index, "SceDbgSdio1Mbox0", 0,
					    sceDbgSdioMboxInterrupt, c, 0x80, 0xf, NULL);
	if (ret < 0) {
		sceKernelDeleteEventFlag(event);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdio.lock, state);
		return ret;
	}
	c->active = 1;
	c->event = event;
	c->notify = notify;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdio.lock, state);
	sceKernelEnableIntr(DBGSDIO_INTR_BASE + index);
	return 0;
}

/* name unknown: detach a channel's client */
int SceDbgSdioForDriver_361E1BB6(SceUInt32 index)
{
	DbgSdioChannel *c;
	SceUInt32 state;

	if (index >= s_sdio.nChannels)
		return DBGSDIO_ERROR_ARGUMENT;
	c = &s_sdio.channel[index];

	state = SceCpuForDriver_D32ACE9E(&s_sdio.lock);
	if (c->active) {
		sceKernelDisableIntr(DBGSDIO_INTR_BASE + index);
		sceKernelReleaseIntrHandler(DBGSDIO_INTR_BASE + index);
		sceKernelDeleteEventFlag(c->event);
		c->active = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdio.lock, state);
	return 0;
}

/* name unknown: whether DMA of this virtual address needs no L2 coherency */
static int isUncached(const void *ptr)
{
	SceUIntPtr addr = (SceUIntPtr)ptr;

	if (addr - 0x18000000 < 0x08000000)		/* the game tmpfs window */
		return sceKernelIsUncacheAddressInTmpFsGame((void *)ptr);
	return addr - 0x20000000 < 0x20000000 || addr - 0x60000000 < 0x21000000;
}

static int executeDma(SceKernelDmaOpId op, SceKernelDmaOpTag *chain, SceUInt32 coherency,
		      SceKernelDmaOpFlag flag)
{
	SceKernelDmaOpChainParam param = { sizeof(param), coherency, 0 };
	int ret;

	ret = sceKernelDmaOpSetupChain(op, chain, &param, flag);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 989, "%s. DmaOpSetupChain error:0x%x\n", __func__, ret);
		return ret;
	}
	ret = sceKernelDmaOpEnQueue(op);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 995, "%s. DmaOpEnQueue error:0x%x\n", __func__, ret);
		return ret;
	}
	ret = sceKernelDmaOpSync(op, SCE_KERNEL_DMA_OP_SYNC_WAIT, NULL, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 1002, "%s. DmaOpSync error:0x%x\n", __func__, ret);
		sceKernelDmaOpDeQueue(op);
	}
	return ret;
}

/* name unknown: wait for a host acknowledgement; poll while suspending */
static void waitStatus(DbgSdioChannel *c, SceUInt32 bit)
{
	while (!s_sdio.suspended) {
		if (*c->status & bit)
			goto acknowledge;
		sceKernelDelayThread(1000);
	}
	while (!(*c->status & bit))
		;
acknowledge:
	*c->status = bit;
	(void)*c->status;
	barrier();
}

static int sendData(DbgSdioChannel *c, const SceUInt8 *data, SceSize size)
{
	SdioWindowDesc desc, reply;
	SceKernelDmaOpTag chain, tail, *head;
	SceSize chunk, left, n;
	SceSize whole = size & ~(DBGSDIO_DMA_ALIGN - 1), rest = size & (DBGSDIO_DMA_ALIGN - 1);
	int virtual;

	SCE_DBG_ASSERT(1, 1237);
	sceKernelDisableIntr(DBGSDIO_INTR_BASE + c->index);

	if (size >= DBGSDIO_DMA_MIN) {
		desc.offset = c->tx.dmaOffset;
		desc.flags = DESC_DMA;
		chunk = size;
	} else {
		desc.offset = c->tx.offset;
		desc.flags = 0;
		chunk = c->tx.chunkSize;
	}
	desc.size = size;
	desc.chunkSize = chunk;
	if (size <= c->tx.inlineMax) {
		copyBytes(c->tx.inlineData, data, size);
		desc.offset += WINDOW_INLINE;
		desc.flags |= DESC_INLINE;
	}
	copyBytes(c->tx.desc, &desc, sizeof(desc));
	*c->txControl = CTRL_TX_MESSAGE;

	if (size <= c->tx.inlineMax) {
		waitStatus(c, STATUS_TX_DATA);
		sceKernelEnableIntr(DBGSDIO_INTR_BASE + c->index);
		return size;
	}
	waitStatus(c, STATUS_TX_ACCEPTED);
	copyBytes(&reply, c->tx.reply, sizeof(reply));

	if (!(desc.flags & DESC_DMA)) {
		for (left = size; left != 0; left -= n) {
			n = left < chunk ? left : chunk;
			copyBytes(c->tx.data, data, n);
			*c->txControl = CTRL_TX_DATA;
			waitStatus(c, STATUS_TX_DATA);
			data += n;
		}
		goto complete;
	}

	*c->txControl = CTRL_TX_DATA;
	virtual = SceSysmemForDriver_0AAA4FDD(data) != PHYSICAL_MEMORY_TYPE_DIRECT;
	if (virtual || ((SceUIntPtr)data & (DBGSDIO_DMA_ALIGN - 1)) == 0) {
		head = whole ? &chain : &tail;
		if (whole) {
			chain.src = data;
			chain.dst = (void *)DBGSDIO_PADDR;
			chain.len = whole | DMA_TX_LENGTH;
			chain.cmd = DMA_TX_COMMAND;
			chain.blockSize = DMA_TX_BLOCK;
			chain.pNext = SCE_KERNEL_DMAC_CHAIN_END;
			if (virtual) {
				if (!isUncached(data))
					chain.cmd |= SCE_KERNEL_DMAC_CMD_COHERENT_SRC;
			} else {
				SceCpuForDriver_103872A5((void *)data, whole);
			}
		}
		if (!rest) {
			chain.cmd |= DMA_COMMAND_LAST;
		} else {
			tail.src = memcpy(c->txTail, data + whole, rest);
			tail.dst = (void *)DBGSDIO_PADDR;
			tail.len = DBGSDIO_DMA_ALIGN | DMA_TX_LENGTH;
			tail.cmd = DMA_TX_COMMAND;
			tail.blockSize = DMA_TX_BLOCK;
			tail.pNext = SCE_KERNEL_DMAC_CHAIN_END;
			if (virtual) {
				if (!isUncached(tail.src))
					tail.cmd |= SCE_KERNEL_DMAC_CMD_COHERENT_SRC;
			} else {
				SceCpuForDriver_103872A5((void *)tail.src, DBGSDIO_DMA_ALIGN);
			}
			tail.cmd |= DMA_COMMAND_LAST;
			chain.pNext = &tail;
		}
		executeDma(c->owner->txDma, head, virtual ? DMA_COHERENCY : 0,
			   SCE_KERNEL_DMA_OP_VIRTUAL_SRC_ADDR);
	}

	/* the host clears the data bit when it has taken the window */
	while (!s_sdio.suspended) {
		if (!(*c->txControl & CTRL_TX_DATA))
			goto complete;
		sceKernelDelayThread(1000);
	}
	while (*c->txControl & CTRL_TX_DATA)
		;
complete:
	sceKernelEnableIntr(DBGSDIO_INTR_BASE + c->index);
	return size;
}


/* name unknown (exported): read from a channel */
int receiveData(SceUInt32 index, void *buffer, SceSize size)
{
	DbgSdioChannel *c = &s_sdio.channel[index];
	SdioHostDesc host;
	SdioWindowDesc reply;
	SceKernelDmaOpTag chain, tail, *head;
	SceUInt8 *out = buffer, *dst;
	SceSize used, amount, chunk, left, n, whole, rest;
	unsigned int bits;
	int ret, virtual;

	ret = lockFastMutex(&s_sdio.mutex);
	if (ret < 0)
		return ret;

	used = c->pendingSize < size ? c->pendingSize : size;
	if (c->pendingSize) {
		memcpy(out, c->pending, used);
		c->pending += used;
		c->pendingSize -= used;
	}
	if (used >= size) {
		ret = used;
		goto done;
	}
	size -= used;
	out += used;
	if (!size) {
		ret = used;
		goto done;
	}

	ret = sceKernelWaitEventFlag(c->event, STATUS_ERROR | STATUS_RX_MESSAGE,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (ret < 0 || (bits & STATUS_ERROR)) {
		SCE_PRINTF_WITH_INFO(0, 1418, "0x%x %d 0x%x 0x%x\n", DBGSDIO_ERROR, 1418, bits, ret);
		goto result;
	}
	copyBytes(&host, c->rx.desc, sizeof(host));
	amount = host.size;

	if (host.flags & DESC_INLINE) {
		if (amount > c->rx.chunkSize) {
			SCE_PRINTF_WITH_INFO(0, 1440, "0x%x %d %d %d\n", DBGSDIO_ERROR_ARGUMENT, 1440,
					     amount, c->rx.chunkSize);
			ret = DBGSDIO_ERROR_ARGUMENT;
			goto done;
		}
		ret = sceKernelWaitEventFlag(c->event, STATUS_ERROR | STATUS_RX_DATA,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (ret < 0 || (bits & STATUS_ERROR)) {
			SCE_PRINTF_WITH_INFO(0, 1458, "0x%x %d 0x%x 0x%x\n", DBGSDIO_ERROR, 1458, bits, ret);
			goto result;
		}
		n = amount < size ? amount : size;
		copyBytes(out, c->rx.inlineData, n);
		if (amount > size) {
			c->pending = c->buffer;
			c->pendingSize = amount - size;
			copyBytes(c->buffer, c->rx.inlineData + size, c->pendingSize);
		}
		*c->rxControl = CTRL_RX_DATA;
		ret = n;
		goto result;
	}

	if (size < amount && amount > sizeof(c->buffer)) {
		SCE_PRINTF_WITH_INFO(0, 1497, "0x%x %d 0x%x 0x%x\n", DBGSDIO_ERROR_OVERFLOW, 1497,
				     amount, sizeof(c->buffer));
		SCE_DBG_PRINTF(0, 1498, "Can't continue dbgsdio1 communication\n");
		SCE_DBG_ASSERT(0, 1499);
		ret = DBGSDIO_ERROR_OVERFLOW;
		goto done;
	}
	dst = size < amount ? c->buffer : out;

	SCE_DBG_ASSERT(1, 1511);
	if (amount >= DBGSDIO_DMA_MIN) {
		reply.offset = c->rx.dmaOffset;
		reply.flags = DESC_DMA;
		chunk = amount;
	} else {
		reply.offset = c->rx.offset;
		reply.flags = 0;
		chunk = c->rx.chunkSize;
	}
	reply.size = amount;
	reply.chunkSize = chunk;
	copyBytes(c->rx.reply, &reply, sizeof(reply));
	*c->rxControl = CTRL_RX_REQUEST;

	ret = sceKernelWaitEventFlag(c->event, STATUS_ERROR | STATUS_RX_DATA,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (ret < 0 || (bits & STATUS_ERROR)) {
		SCE_PRINTF_WITH_INFO(0, 1545, "0x%x %d 0x%x 0x%x\n", DBGSDIO_ERROR, 1545, bits, ret);
		goto result;
	}

	if (amount < DBGSDIO_DMA_MIN) {
		for (left = amount; left != 0; ) {
			n = left < chunk ? left : chunk;
			copyBytes(dst, c->rx.data, n);
			*c->rxControl = CTRL_RX_DATA;
			left -= n;
			dst += n;
			if (!left)
				break;
			ret = sceKernelWaitEventFlag(c->event, STATUS_ERROR | STATUS_RX_DATA,
						      SCE_KERNEL_EVF_WAITMODE_OR |
						      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						      &bits, NULL);
			if (ret < 0 || (bits & STATUS_ERROR)) {
				SCE_PRINTF_WITH_INFO(0, 1589, "0x%x %d 0x%x 0x%x\n", DBGSDIO_ERROR, 1589,
						     bits, ret);
				goto result;
			}
		}
	} else {
		virtual = SceSysmemForDriver_0AAA4FDD(dst) != PHYSICAL_MEMORY_TYPE_DIRECT;
		if (virtual || ((SceUIntPtr)dst & (DBGSDIO_DMA_ALIGN - 1)) == 0) {
			whole = amount & ~(DBGSDIO_DMA_ALIGN - 1);
			rest = amount & (DBGSDIO_DMA_ALIGN - 1);
			head = whole ? &chain : &tail;
			if (whole) {
				chain.src = (const void *)DBGSDIO_PADDR;
				chain.dst = dst;
				chain.len = whole | DMA_RX_LENGTH;
				chain.cmd = DMA_RX_COMMAND;
				chain.blockSize = DMA_RX_BLOCK;
				chain.pNext = SCE_KERNEL_DMAC_CHAIN_END;
				if (virtual) {
					if (!isUncached(dst))
						chain.cmd |= SCE_KERNEL_DMAC_CMD_COHERENT_DST;
				} else {
					SceCpuForDriver_103872A5(dst, whole);
				}
			}
			if (!rest) {
				chain.cmd |= DMA_COMMAND_LAST;
			} else {
				tail.src = (const void *)DBGSDIO_PADDR;
				tail.dst = c->rxTail;
				tail.len = DBGSDIO_DMA_ALIGN | DMA_RX_LENGTH;
				tail.cmd = DMA_RX_COMMAND;
				tail.blockSize = DMA_RX_BLOCK;
				tail.pNext = SCE_KERNEL_DMAC_CHAIN_END;
				if (virtual) {
					if (!isUncached(c->rxTail))
						tail.cmd |= SCE_KERNEL_DMAC_CMD_COHERENT_DST;
				} else {
					SceCpuForDriver_103872A5(c->rxTail, DBGSDIO_DMA_ALIGN);
				}
				tail.cmd |= DMA_COMMAND_LAST;
				chain.pNext = &tail;
			}
			ret = executeDma(c->owner->rxDma, head,
					 virtual ? DMA_COHERENCY << SCE_KERNEL_DMAC_COHERENCY_MSK_DST_SHIFT : 0,
					 SCE_KERNEL_DMA_OP_VIRTUAL_DST_ADDR);
			if (ret >= 0) {
				if (!virtual) {
					if (whole)
						SceCpuForDriver_02796361(dst, whole);
					if (rest)
						SceCpuForDriver_02796361(c->rxTail, DBGSDIO_DMA_ALIGN);
				}
				if (rest)
					memcpy(dst + whole, c->rxTail, rest);
			}
		}
	}

	if (size < amount) {
		c->pending = c->buffer;
		memcpy(out, c->buffer, size);
		c->pending += size;
		c->pendingSize = amount - size;
		amount = size;
	}
	ret = amount;
result:
	if (ret >= 0)
		ret += used;
done:
	sceKernelUnlockFastMutex(&s_sdio.mutex);
	return ret;
}

int sceDbgSdioWrite(SceUInt32 index, const void *buffer, SceSize size)
{
	SceKernelVARange range = { (SceUIntPtr)buffer, size };
	SceKernelPARange ranges[DBGSDIO_RANGES];
	SceKernelPAVector vector = { 0 };
	unsigned int i;
	int ret;

	ret = lockFastMutex(&s_sdio.mutex);
	if (ret < 0)
		return ret;
	if (size < DBGSDIO_DMA_MIN) {
		ret = sendData(&s_sdio.channel[index], buffer, size);
		goto done;
	}

	/* DMA goes one physically contiguous range at a time */
	vector.size = sizeof(vector);
	vector.capacity = DBGSDIO_RANGES;
	vector.nDataInVector = 0;
	vector.total = 0;
	vector.pRanges = ranges;
	ret = sceKernelVARangeToPAVector(&range, &vector);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 1867, "%s.%d VARangeToPARange error:0x%x(%d)\n", __func__, 1867, ret, ret);
		SCE_DBG_PRINTF(0, 1869, "%s. VARangeToPARange base:%p size:0x%x\n", __func__, buffer, size);
		goto done;
	}
	SCE_DBG_ASSERT(vector.total <= vector.nDataInVector, 1874);
	if (vector.nDataInVector < vector.total) {
		SCE_PRINTF_WITH_INFO(0, 1878, "0x%x %d %d %d\n", DBGSDIO_ERROR_ARGUMENT, 1878,
				     vector.nDataInVector, vector.total);
		ret = DBGSDIO_ERROR_ARGUMENT;
		goto done;
	}
	for (i = 0; i < vector.nDataInVector; i++) {
		ret = sendData(&s_sdio.channel[index], buffer, ranges[i].size);
		if (ret < 0)
			goto done;
		buffer = (const SceUInt8 *)buffer + ranges[i].size;
	}
	ret = size;
done:
	sceKernelUnlockFastMutex(&s_sdio.mutex);
	return ret;
}
