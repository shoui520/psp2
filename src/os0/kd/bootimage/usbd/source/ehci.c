/*
 * SceUsbd: the EHCI host controller driver (high speed), one controller per bus.
 *
 * The controller's descriptors live in one physically contiguous block: the
 * periodic frame list, then the iTD, QH, qTD and siTD pools, then the static
 * QHs. Each pool descriptor has a record (EhciDescriptor) on a free list; the
 * record of a busy one is on its pipe's descriptor list and maps to the
 * request it carries. The static QHs are the interrupt tree (511 nodes, the
 * 256 leaves in the frame list) and the anchors of the asynchronous ring.
 */

#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <sce/sysmem.h>
#include <sce/debug.h>
#include <sce/intrmgr.h>
#include <sce/threadmgr.h>
#include "usbd_internal.h"

/* the controller's registers: capability registers, then the operational ones */
typedef struct EhciRegs {
	SceUInt32 capLength;		/* CAPLENGTH, HCIVERSION */
	SceUInt32 hcsParams;
	SceUInt32 hccParams;
	SceUInt32 hcspPortRoute;
	SceUInt32 usbCmd;
	SceUInt32 usbSts;
	SceUInt32 usbIntr;
	SceUInt32 frIndex;
	SceUInt32 ctrlDsSegment;
	SceUInt32 periodicListBase;
	SceUInt32 asyncListAddr;
	SceUInt32 reserved[9];
	SceUInt32 configFlag;
	SceUInt32 portSc;
	SceUInt32 reserved1[15];
	SceUInt32 insnReg01;		/* Synopsys: OUT and IN packet buffer thresholds */
} EhciRegs;

#define EHCI_HCSPARAMS_N_PORTS		0x0000000f

#define EHCI_USBCMD_RUN			0x00000001
#define EHCI_USBCMD_RESET		0x00000002
#define EHCI_USBCMD_FRAME_LIST_256	0x00000008
#define EHCI_USBCMD_PERIODIC		0x00000010
#define EHCI_USBCMD_ASYNC		0x00000020
#define EHCI_USBCMD_ASYNC_ADVANCE	0x00000040
#define EHCI_USBCMD_ITC_1		0x00010000	/* an interrupt every microframe */

/* USBSTS and USBINTR */
#define EHCI_STS_INT			0x00000001
#define EHCI_STS_ERROR			0x00000002
#define EHCI_STS_PORT_CHANGE		0x00000004
#define EHCI_STS_FRAME_ROLLOVER		0x00000008
#define EHCI_STS_HOST_ERROR		0x00000010
#define EHCI_STS_ASYNC_ADVANCE		0x00000020

#define EHCI_CONFIGFLAG_EHCI		0x00000001	/* the ports route to this controller */

#define EHCI_PORTSC_CONNECT		0x00000001
#define EHCI_PORTSC_CONNECT_CHANGE	0x00000002
#define EHCI_PORTSC_ENABLE		0x00000004
#define EHCI_PORTSC_ENABLE_CHANGE	0x00000008
#define EHCI_PORTSC_OVER_CURRENT_CHANGE	0x00000020
#define EHCI_PORTSC_RESET		0x00000100
#define EHCI_PORTSC_LINE_STATUS		0x00000c00
#define EHCI_PORTSC_LINE_K		0x00000400	/* a low speed device */
#define EHCI_PORTSC_LINE_J		0x00000800
#define EHCI_PORTSC_POWER		0x00001000
#define EHCI_PORTSC_OWNER		0x00002000	/* handed to the companion OHCI */

/* a link pointer: the next descriptor's bus address and type */
#define EHCI_LINK_TERMINATE		0x00000001
#define EHCI_LINK_ITD			0x00000000
#define EHCI_LINK_QH			0x00000002
#define EHCI_LINK_SITD			0x00000004
#define EHCI_LINK_ADDRESS		0xffffffe0

/* the hardware descriptors; every pool slot is 64 bytes */
typedef struct EhciQh {
	SceUInt32 link;
	SceUInt32 endpoint;		/* endpoint characteristics */
	SceUInt32 capability;		/* endpoint capabilities */
	SceUInt32 currentQtd;
	SceUInt32 nextQtd;
	SceUInt32 altNextQtd;
	SceUInt32 token;
	SceUInt32 buffer[5];
	SceUInt32 padding[4];
} EhciQh;

typedef struct EhciQtd {
	SceUInt32 next;
	SceUInt32 altNext;
	SceUInt32 token;
	SceUInt32 buffer[5];
	SceUInt32 padding[8];
} EhciQtd;

typedef struct EhciItd {
	SceUInt32 link;
	SceUInt32 transaction[8];
	SceUInt32 buffer[7];		/* the first three also hold the endpoint */
} EhciItd;

typedef struct EhciSitd {
	SceUInt32 link;
	SceUInt32 endpoint;
	SceUInt32 schedule;		/* split completion and start masks */
	SceUInt32 state;
	SceUInt32 buffer[2];
	SceUInt32 back;
	SceUInt32 padding[9];
} EhciSitd;

#define EHCI_DESCRIPTOR_SIZE		64

/* EhciQh.endpoint */
#define EHCI_QH_ADDRESS			0x0000007f
#define EHCI_QH_ENDPOINT_SHIFT		8
#define EHCI_QH_FULL_SPEED		0x00000000
#define EHCI_QH_LOW_SPEED		0x00001000
#define EHCI_QH_HIGH_SPEED		0x00002000
#define EHCI_QH_TOGGLE_CONTROL		0x00004000
#define EHCI_QH_HEAD			0x00008000	/* head of the asynchronous ring */
#define EHCI_QH_MAX_PACKET_SHIFT	16
#define EHCI_QH_MAX_PACKET		0x07ff0000
#define EHCI_QH_CONTROL			0x08000000	/* a full or low speed control endpoint */
#define EHCI_QH_NAK_RELOAD_4		0x40000000

/* EhciQh.capability */
#define EHCI_QH_SPLIT_COMPLETION	0x0000e000
#define EHCI_QH_HUB_SHIFT		16
#define EHCI_QH_PORT_SHIFT		23
#define EHCI_QH_MULT_1			0x40000000

/* EhciQh.token and EhciQtd.token */
#define EHCI_QTD_ERRORS			0x0000007c
#define EHCI_QTD_HALTED			0x00000040
#define EHCI_QTD_ACTIVE			0x00000080
#define EHCI_QTD_PID_OUT		0x00000000
#define EHCI_QTD_PID_IN			0x00000100
#define EHCI_QTD_PID_SETUP		0x00000200
#define EHCI_QTD_PID			0x00000300
#define EHCI_QTD_ERROR_COUNTER_3	0x00000c00
#define EHCI_QTD_IOC			0x00008000
#define EHCI_QTD_LENGTH_SHIFT		16
#define EHCI_QTD_LENGTH			0x7fff
#define EHCI_QTD_TOGGLE			0x80000000
#define EHCI_QTD_MAX_LENGTH		0x5000	/* five pages */
#define EHCI_QTD_PAGE			0x1000

/* EhciItd.transaction */
#define EHCI_ITD_LENGTH_SHIFT		16
#define EHCI_ITD_LENGTH			0xfff
#define EHCI_ITD_IOC			0x00008000
#define EHCI_ITD_XACT_ERROR		0x10000000
#define EHCI_ITD_BABBLE			0x20000000
#define EHCI_ITD_BUFFER_ERROR		0x40000000
#define EHCI_ITD_ACTIVE			0x80000000
#define EHCI_ITD_MAX_LENGTH		0x8000	/* eight pages */
#define EHCI_ITD_DIRECTION_IN		0x00000800	/* in buffer[1] */

/* EhciSitd.endpoint, schedule and state */
#define EHCI_SITD_HUB_SHIFT		16
#define EHCI_SITD_PORT_SHIFT		24
#define EHCI_SITD_DIRECTION_IN		0x80000000
#define EHCI_SITD_START_IN		0x01
#define EHCI_SITD_COMPLETION_IN		0xfc
#define EHCI_SITD_COMPLETION_SHIFT	8
#define EHCI_SITD_MISSED_MICROFRAME	0x00000004
#define EHCI_SITD_ERRORS		0x0000007c
#define EHCI_SITD_ACTIVE		0x00000080
#define EHCI_SITD_LENGTH_SHIFT		16
#define EHCI_SITD_LENGTH		0x3ff
#define EHCI_SITD_IOC			0x80000000
#define EHCI_SITD_POSITION_ALL		0x00000000	/* in buffer[1] */
#define EHCI_SITD_POSITION_BEGIN	0x00000008
#define EHCI_SITD_COUNT			0x7
#define EHCI_SPLIT_PAYLOAD		188	/* bytes of a full speed isochronous OUT per microframe */

/* UsbdIsochPacket.status */
#define EHCI_ISOCH_MISSED		2
#define EHCI_ISOCH_ERROR		4

#define EHCI_FRAME_LIST_SIZE		256
#define EHCI_TREE_SIZE			511	/* static QHs of the interrupt tree */
#define EHCI_STATIC_QH_COUNT		514	/* the tree, the two anchors, the dummies */
#define EHCI_TREE_LEAF			255
#define EHCI_RING_TIMEOUT		100000

/* the bookkeeping record of a pool descriptor */
typedef struct EhciDescriptor {
	struct EhciDescriptor *next;
	struct EhciDescriptor *prev;
	union {
		EhciQh *qh;
		EhciQtd *qtd;
		EhciItd *itd;
		EhciSitd *sitd;
		SceUInt32 *link;
	} hw;
	SceUIntPtr pa;
	int frame;			/* its frame, or its QH's static QH */
} EhciDescriptor;

/* an siTD's record: one per packet */
typedef struct EhciSplitDescriptor {
	EhciDescriptor descriptor;
	int packet;
} EhciSplitDescriptor;

typedef struct EhciPipeEntry {
	struct EhciPipeEntry *next;
	struct EhciPipeEntry *prev;
	UsbdPipe *pipe;
} EhciPipeEntry;

typedef struct EhciArea {
	SceUInt8 *base;
	SceUInt8 *end;
} EhciArea;

typedef struct EhciRange {
	SceUIntPtr base;
	SceUIntPtr end;
} EhciRange;

typedef struct EhciRecords {
	EhciDescriptor *base;
	EhciDescriptor *end;
} EhciRecords;

typedef struct EhciDescriptorList {
	EhciDescriptor *head;
	EhciDescriptor *tail;
} EhciDescriptorList;

/* EhciDescriptorCount: the pool sizes of a bus */
typedef struct EhciDescriptorCount {
	int qh;
	int qtd;
	int itd;
	int sitd;
} EhciDescriptorCount;

/* Ehci.portState */
#define EHCI_PORT_OFF			0
#define EHCI_PORT_POWERED		1
#define EHCI_PORT_CONNECTED		2	/* debouncing */
#define EHCI_PORT_RESETTING		3
#define EHCI_PORT_RESET			4	/* settling */
#define EHCI_PORT_ENABLED		5	/* attached */

typedef struct Ehci {
	SceUInt8 bus;
	SceUInt8 portCount;
	SceUInt8 padding[2];
	EhciDescriptorCount count;
	EhciDescriptorCount freeCount;
	int frameListSize;
	int controlSlot;		/* the static QHs the async QHs follow */
	int bulkSlot;
	int intrCode;
	int *bandwidth;			/* per frame: bytes of the interrupt QHs */
	SceUID descriptorBlockId;
	SceUID registerBlockId;
	int lock;
	SceUInt32 *frameList;
	volatile int portState;		/* EHCI_PORT_*; the timers and the interrupt change it */
	unsigned int intCount;		/* per USBSTS bit */
	unsigned int errorCount;
	unsigned int portChangeCount;
	unsigned int frameRolloverCount;
	unsigned int hostErrorCount;
	unsigned int asyncAdvanceCount;
	UsbdHcd *hcd;
	volatile EhciRegs *regs;
	EhciDescriptor *asyncHead;	/* the ring's head and last QH */
	EhciDescriptor *asyncTail;
	EhciQh *staticQh;
	EhciArea staticArea;
	EhciArea qhArea;
	EhciArea qtdArea;
	EhciArea itdArea;
	EhciArea sitdArea;
	EhciRecords qhRecords;
	EhciRecords qtdRecords;
	EhciRecords itdRecords;
	EhciRecords sitdRecords;
	EhciRange qhRange;
	EhciRange qtdRange;
	EhciRange itdRange;
	EhciRange sitdRange;
	EhciRange staticRange;
	UsbdRequest **qtdRequest;	/* the request of each busy descriptor */
	UsbdRequest **itdRequest;
	UsbdRequest **sitdRequest;
	EhciDescriptorList freeQh;
	EhciDescriptorList freeQtd;
	EhciDescriptorList freeItd;
	EhciDescriptorList freeSitd;
	EhciPipeEntry *freePipeHead;
	EhciPipeEntry *freePipeTail;
	UsbdPipe *waitHead;		/* pipes waiting for descriptors */
	UsbdPipe *waitTail;
	EhciPipeEntry *pipeHead;	/* the open pipes */
	EhciPipeEntry *pipeTail;
} Ehci;

/* ksceKernelGetMemBlockMemtypeByAddr of uncached memory: no cache maintenance */
#define USBD_MEMTYPE_UNCACHED		0x8000

#define USBD_CACHE_LINE			64
#define USBD_CACHE_BASE(addr)		((SceUIntPtr)(addr) & ~(USBD_CACHE_LINE - 1))
#define USBD_CACHE_SIZE(addr, size)	\
	((((SceUIntPtr)(addr) + (size) + USBD_CACHE_LINE - 1) & ~(USBD_CACHE_LINE - 1)) - \
	 USBD_CACHE_BASE(addr))

#define EHCI_PAGE(pa)			((pa) >> 12 << 12)

/* bInterval - 1 of a high speed interrupt endpoint: its start mask */
static const SceUInt32 s_ehciInterruptStartMask[] = { 0xff, 0xaa, 0x44 };

static const EhciDescriptorCount s_ehciDescriptorCount[USBD_BUS_COUNT] = {
	{ 32, 64, 32, 64 },
	{ 32, 64, 0, 0 },
	{ 32, 64, 32, 64 },
};

static Ehci *s_ehci[USBD_BUS_COUNT];

static int ehciInterruptHandler(int intrCode, void *arg);

static __always_inline EhciDescriptor *ehciAllocDescriptor(EhciDescriptorList *list, int *count)
{
	EhciDescriptor *descriptor = list->head;

	if (descriptor != NULL) {
		USBD_LIST_REMOVE(list->head, list->tail, descriptor);
		(*count)--;
	}
	return descriptor;
}

/* a descriptor already free is left alone */
static __always_inline void ehciFreeDescriptor(EhciDescriptorList *list, int *count,
					       EhciDescriptor *descriptor)
{
	EhciDescriptor *entry;

	for (entry = list->head; entry != NULL; entry = entry->next) {
		if (entry == descriptor)
			return;
	}
	USBD_LIST_APPEND(list->head, list->tail, descriptor);
	(*count)++;
}

/* the descriptor whose link points at pa */
static __always_inline SceUInt32 *ehciLinkFromPhys(Ehci *ehci, SceUIntPtr pa)
{
	if (pa >= ehci->qhRange.base && pa < ehci->qhRange.end)
		return (SceUInt32 *)(ehci->qhArea.base + ((pa - ehci->qhRange.base) & ~(EHCI_DESCRIPTOR_SIZE - 1)));
	if (pa >= ehci->itdRange.base && pa < ehci->itdRange.end)
		return (SceUInt32 *)(ehci->itdArea.base + ((pa - ehci->itdRange.base) & ~(EHCI_DESCRIPTOR_SIZE - 1)));
	if (pa >= ehci->sitdRange.base && pa < ehci->sitdRange.end)
		return (SceUInt32 *)(ehci->sitdArea.base + ((pa - ehci->sitdRange.base) & ~(EHCI_DESCRIPTOR_SIZE - 1)));
	if (pa >= ehci->staticRange.base && pa < ehci->staticRange.end)
		return (SceUInt32 *)(ehci->staticArea.base + ((pa - ehci->staticRange.base) & ~(EHCI_DESCRIPTOR_SIZE - 1)));
	return NULL;
}

/* a QH of the asynchronous ring or the interrupt tree by its bus address */
static __always_inline EhciQh *ehciQhFromPhys(Ehci *ehci, SceUIntPtr pa)
{
	return (EhciQh *)(ehci->qhArea.base +
			  ((pa - ehci->qhRecords.base->pa) & ~(EHCI_DESCRIPTOR_SIZE - 1)));
}

static __always_inline void ehciUnwaitPipe(Ehci *ehci, UsbdPipe *pipe)
{
	USBD_LIST_REMOVE_LINK(ehci->waitHead, ehci->waitTail, pipe, waitNext, waitPrev);
	pipe->waiting = 0;
}

static int ehciSetMaxPacketSize(UsbdPipe *pipe, int size)
{
	EhciQh *qh;

	switch (pipe->device->speed) {
	case SCE_USBD_DEVICE_SPEED_FS:
		if (size != 8 && size != 16 && size != 32 && size != 64)
			return SCE_USBD_ERROR_INVALID_PARAM;
		break;
	case SCE_USBD_DEVICE_SPEED_LS:
		if (size != 8)
			return SCE_USBD_ERROR_INVALID_PARAM;
		break;
	case SCE_USBD_DEVICE_SPEED_HS:
		if (size != 64)
			return SCE_USBD_ERROR_INVALID_PARAM;
		break;
	default:
		return SCE_USBD_ERROR_FATAL;
	}
	qh = pipe->endpoint.ehci->hw.qh;
	qh->endpoint = (qh->endpoint & ~EHCI_QH_MAX_PACKET) | size << EHCI_QH_MAX_PACKET_SHIFT;
	return 0;
}

static int ehciResetFrame(UsbdPipe *pipe)
{
	if (pipe == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	pipe->frame = -1;
	return 0;
}

static int ehciSetAddress(UsbdPipe *pipe, int address)
{
	pipe->endpoint.ehci->hw.qh->endpoint |= address & EHCI_QH_ADDRESS;
	return 0;
}

static int ehciGetFrameNumber(UsbdBus *bus, unsigned int *frame, unsigned int *mask)
{
	*frame = s_ehci[bus->index]->regs->frIndex & 0x3fff;
	*mask = 0x3fff;
	return 0;
}

/* a device the reset left enabled is high speed; the others go to the OHCI */
static int ehciGetSpeed(UsbdBus *bus)
{
	if (s_ehci[bus->index]->regs->portSc & EHCI_PORTSC_ENABLE)
		return SCE_USBD_DEVICE_SPEED_HS;
	return SCE_USBD_DEVICE_SPEED_FS;
}

static int ehciGetPortCount(UsbdBus *bus)
{
	return s_ehci[bus->index]->portCount;
}

static int ehciPowerOffPort(UsbdBus *bus, int port)
{
	Ehci *ehci = s_ehci[bus->index];
	int state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	ehci->portState = EHCI_PORT_OFF;
	ehci->regs->portSc &= ~EHCI_PORTSC_POWER;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return 0;
}

static int ehciResetPort(UsbdBus *bus, int port)
{
	Ehci *ehci = s_ehci[bus->index];
	int state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	ehci->portState = EHCI_PORT_RESETTING;
	ehci->regs->portSc |= EHCI_PORTSC_RESET;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return 0;
}

static int ehciPowerOnPort(UsbdBus *bus, int port)
{
	Ehci *ehci = s_ehci[bus->index];
	int state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	if (ehci->portState == EHCI_PORT_OFF || ehci->portState == EHCI_PORT_POWERED) {
		ehci->portState = EHCI_PORT_POWERED;
		ehci->regs->portSc |= EHCI_PORTSC_POWER;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return 0;
}

static SceUInt32 ehciResetDone(void *arg);

/* a connection held 100 ms: reset a full or high speed device, hand off a low speed one */
static SceUInt32 ehciConnectDone(void *arg)
{
	Ehci *ehci = arg;
	volatile EhciRegs *regs;
	SceUInt32 portSc;
	int state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	regs = ehci->regs;
	portSc = regs->portSc;
	if (ehci->portState == EHCI_PORT_CONNECTED) {
		portSc &= EHCI_PORTSC_LINE_STATUS;
		if (portSc == EHCI_PORTSC_LINE_K) {
			ehci->portState = EHCI_PORT_POWERED;
			regs->portSc |= EHCI_PORTSC_OWNER;
		} else if (portSc == EHCI_PORTSC_LINE_J) {
			ehci->portState = EHCI_PORT_RESETTING;
			regs->portSc |= EHCI_PORTSC_RESET;
			ksceKernelRegisterTimer("SceUsbdAlarm1", 100000, ehciResetDone, ehci);
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return 0;
}

static SceUInt32 ehciRecoveryDone(void *arg);

/* the reset held 100 ms: end it, then let the device recover */
static SceUInt32 ehciResetDone(void *arg)
{
	Ehci *ehci = arg;
	int state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	if (ehci->portState == EHCI_PORT_RESETTING) {
		ehci->regs->portSc &= ~EHCI_PORTSC_RESET;
		ehci->portState = EHCI_PORT_RESET;
		ksceKernelRegisterTimer("SceUsbdAlarm1", 50000, ehciRecoveryDone, ehci);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return 0;
}

/* a high speed device is attached; the others go to the OHCI */
static SceUInt32 ehciRecoveryDone(void *arg)
{
	Ehci *ehci = arg;
	int state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	if (ehci->portState == EHCI_PORT_RESET) {
		if (ehci->regs->portSc & EHCI_PORTSC_ENABLE) {
			ehci->portState = EHCI_PORT_ENABLED;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
			usbdNotifyAttach(USBD_ID(ehci->bus, 0), 0, SCE_USBD_DEVICE_SPEED_HS);
			state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
		} else {
			ehci->portState = EHCI_PORT_POWERED;
			ehci->regs->portSc |= EHCI_PORTSC_OWNER;
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return 0;
}

/* unlink an isochronous descriptor from its frame */
static void ehciUnlinkPeriodic(Ehci *ehci, EhciDescriptor *descriptor)
{
	SceUIntPtr pa = ehci->frameList[descriptor->frame] & EHCI_LINK_ADDRESS;
	SceUInt32 *link;

	if (pa == 0)
		return;
	for (;;) {
		link = ehciLinkFromPhys(ehci, pa);
		if (link == NULL)
			return;
		pa = *link & EHCI_LINK_ADDRESS;
		if (pa == descriptor->pa)
			break;
		if (*link == EHCI_LINK_TERMINATE)
			return;
	}
	*link = *descriptor->hw.link;
}

static int ehciResume(UsbdBus *bus)
{
	return ehciStart(bus);
}

/*
 * The transaction translator of a full or low speed device: the high speed hub
 * nearest to it, and the port of that hub it is behind.
 */
static __always_inline void ehciGetTranslator(UsbdDevice *device, unsigned int *hub,
					      unsigned int *port)
{
	UsbdDevice *parent = device->parent;
	UsbdDevice *child, *tt;

	*hub = parent->address;
	*port = *hub;
	if (*hub == 0)
		return;
	if (parent->speed == SCE_USBD_DEVICE_SPEED_HS) {
		*port = device->port;
		return;
	}
	child = parent;
	tt = parent->parent;
	while (tt->speed != SCE_USBD_DEVICE_SPEED_HS) {
		child = tt;
		tt = tt->parent;
	}
	*hub = tt->address;
	*port = child->port;
}

/* the QH follows the static QH of slot */
static __always_inline void ehciLinkQh(Ehci *ehci, EhciDescriptor *descriptor, int slot)
{
	descriptor->frame = slot;
	descriptor->hw.qh->link = ehci->staticQh[slot].link;
	ehci->staticQh[slot].link = descriptor->pa | EHCI_LINK_QH;
}

/*
 * The interrupt tree slot of the least used frames of the period: the leaves
 * are the 256 frames, each level up serves twice the period.
 */
static __always_inline int ehciScheduleInterrupt(Ehci *ehci, UsbdDevice *device,
						 int interval, int maxPacketSize)
{
	int *bandwidth = ehci->bandwidth;
	int period, levels, best, least, sum;
	int frame, level, cost;

	if (device->speed == SCE_USBD_DEVICE_SPEED_HS) {
		/* 2^(bInterval - 1) microframes */
		if (interval >= 12) {
			period = 256;
			levels = 1;
		} else if (interval == 11) {
			period = 128;
			levels = 2;
		} else if (interval == 10) {
			period = 64;
			levels = 3;
		} else if (interval == 9) {
			period = 32;
			levels = 4;
		} else if (interval == 8) {
			period = 16;
			levels = 5;
		} else if (interval == 7) {
			period = 8;
			levels = 6;
		} else if (interval == 6) {
			period = 4;
			levels = 7;
		} else if (interval == 5) {
			period = 2;
			levels = 8;
		} else {
			period = 1;
			levels = 9;
		}
	} else {
		/* bInterval frames */
		if (interval >= 128) {
			period = 128;
			levels = 2;
		} else if (interval >= 64) {
			period = 64;
			levels = 3;
		} else if (interval >= 32) {
			period = 32;
			levels = 4;
		} else if (interval >= 16) {
			period = 16;
			levels = 5;
		} else if (interval >= 8) {
			period = 8;
			levels = 6;
		} else if (interval >= 4) {
			period = 4;
			levels = 7;
		} else if (interval >= 2) {
			period = 2;
			levels = 8;
		} else {
			period = 1;
			levels = 9;
		}
	}

	best = -1;
	least = 0;
	for (frame = 0; frame < period; frame++) {
		sum = 0;
		for (level = 0; level < levels; level++)
			sum += bandwidth[frame + level * period];
		if (best < 0 || sum < least) {
			best = frame;
			least = sum;
		}
	}

	cost = (maxPacketSize < 1024 ? maxPacketSize : 1024) + 13;
	if (device->speed == SCE_USBD_DEVICE_SPEED_LS)
		cost *= 8;
	for (level = 0; level < levels; level++)
		bandwidth[best + level * period] += cost;
	return period - 1 + best;
}

static UsbdPipe *ehciAllocPipe(UsbdDevice *device, const SceUsbdEndpointDescriptor *endpoint)
{
	EhciDescriptor *descriptor;
	EhciPipeEntry *entry;
	UsbdPipe *pipe;
	Ehci *ehci;
	unsigned int hub, port;
	SceUInt32 characteristics, speed, startMask;
	unsigned int maxPacketSize, index;
	int state;

	if (device == NULL)
		return NULL;
	ehci = s_ehci[device->bus->index];
	if (ehci == NULL)
		return NULL;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	pipe = usbdAllocPipe(device, endpoint);
	if (pipe == NULL)
		goto fail;
	entry = ehci->freePipeHead;
	if (entry == NULL)
		goto free;
	if (pipe->type != SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS)
		descriptor = ehciAllocDescriptor(&ehci->freeQh, &ehci->freeCount.qh);
	else if (device->speed == SCE_USBD_DEVICE_SPEED_HS)
		descriptor = ehciAllocDescriptor(&ehci->freeItd, &ehci->freeCount.itd);
	else
		descriptor = ehciAllocDescriptor(&ehci->freeSitd, &ehci->freeCount.sitd);
	if (descriptor == NULL)
		goto free;

	pipe->endpoint.ehci = descriptor;
	pipe->hcd = ehci->hcd;
	USBD_LIST_REMOVE(ehci->freePipeHead, ehci->freePipeTail, entry);
	USBD_LIST_APPEND(ehci->pipeHead, ehci->pipeTail, entry);
	entry->pipe = pipe;

	ehciGetTranslator(device, &hub, &port);

	if (endpoint == NULL) {
		/* the default pipe */
		EhciQh *qh = descriptor->hw.qh;

		memset(qh, 0, sizeof(*qh));
		if (device->speed == SCE_USBD_DEVICE_SPEED_HS) {
			characteristics = EHCI_QH_NAK_RELOAD_4 | 64 << EHCI_QH_MAX_PACKET_SHIFT;
			speed = EHCI_QH_HIGH_SPEED;
		} else {
			speed = device->speed == SCE_USBD_DEVICE_SPEED_FS ?
				EHCI_QH_FULL_SPEED : EHCI_QH_LOW_SPEED;
			characteristics = EHCI_QH_NAK_RELOAD_4 | EHCI_QH_CONTROL |
					  device->maxPacketSize0 << EHCI_QH_MAX_PACKET_SHIFT;
		}
		qh->endpoint = device->address | EHCI_QH_TOGGLE_CONTROL | speed | characteristics;
		qh->capability = EHCI_QH_MULT_1 | (hub & 0x7f) << EHCI_QH_HUB_SHIFT |
				 (port & 0x7f) << EHCI_QH_PORT_SHIFT;
		qh->nextQtd = EHCI_LINK_TERMINATE;
		qh->altNextQtd = EHCI_LINK_TERMINATE;
		ehciLinkQh(ehci, descriptor, ehci->controlSlot);
		goto out;
	}

	if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS) {
		/* the template each transfer's descriptors copy */
		if (device->speed == SCE_USBD_DEVICE_SPEED_HS) {
			EhciItd *itd = descriptor->hw.itd;

			memset(itd, 0, sizeof(*itd));
			itd->buffer[0] = (device->address & 0x7f) |
					 (endpoint->bEndpointAddress & 0xf) << 8;
			maxPacketSize = endpoint->wMaxPacketSize;
			itd->buffer[1] = (maxPacketSize & 0x7ff) |
					 (pipe->direction ? EHCI_ITD_DIRECTION_IN : 0);
			itd->buffer[2] = (((maxPacketSize >> 11) & 3) + 1) & 3;
		} else if (device->speed == SCE_USBD_DEVICE_SPEED_FS) {
			EhciSitd *sitd = descriptor->hw.sitd;

			memset(sitd, 0, sizeof(*sitd));
			sitd->back = EHCI_LINK_TERMINATE;
			sitd->endpoint = (pipe->direction ? EHCI_SITD_DIRECTION_IN : 0) |
					 (device->address & 0x7f) |
					 (endpoint->bEndpointAddress & 0xf) << 8 |
					 (hub & 0x7f) << EHCI_SITD_HUB_SHIFT |
					 (port & 0x7f) << EHCI_SITD_PORT_SHIFT;
		}
		goto out;
	}

	{
		EhciQh *qh = descriptor->hw.qh;

		memset(qh, 0, sizeof(*qh));
		maxPacketSize = endpoint->wMaxPacketSize & 0x7ff;
		if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL)
			characteristics = EHCI_QH_NAK_RELOAD_4 | EHCI_QH_TOGGLE_CONTROL |
					  (device->speed == SCE_USBD_DEVICE_SPEED_HS ? 0 : EHCI_QH_CONTROL);
		else if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT)
			characteristics = 0;
		else
			characteristics = EHCI_QH_NAK_RELOAD_4;
		if (device->speed == SCE_USBD_DEVICE_SPEED_HS)
			speed = EHCI_QH_HIGH_SPEED;
		else if (device->speed == SCE_USBD_DEVICE_SPEED_FS)
			speed = EHCI_QH_FULL_SPEED;
		else
			speed = EHCI_QH_LOW_SPEED;
		qh->endpoint = characteristics | maxPacketSize << EHCI_QH_MAX_PACKET_SHIFT | speed |
			       (device->address & 0x7f) |
			       (endpoint->bEndpointAddress & 0xf) << EHCI_QH_ENDPOINT_SHIFT;
		qh->nextQtd = EHCI_LINK_TERMINATE;
		qh->altNextQtd = EHCI_LINK_TERMINATE;
		qh->capability = (port & 0x7f) << EHCI_QH_PORT_SHIFT | (hub & 0x7f) << EHCI_QH_HUB_SHIFT |
				 EHCI_QH_MULT_1 |
				 (device->speed != SCE_USBD_DEVICE_SPEED_HS &&
				  pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT ?
				  EHCI_QH_SPLIT_COMPLETION : 0);

		if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL) {
			ehciLinkQh(ehci, descriptor, ehci->controlSlot);
		} else if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK) {
			ehciLinkQh(ehci, descriptor, ehci->bulkSlot);
		} else {
			descriptor->frame = ehciScheduleInterrupt(ehci, device, endpoint->bInterval,
								  maxPacketSize);
			switch (device->speed) {
			case SCE_USBD_DEVICE_SPEED_LS:
			case SCE_USBD_DEVICE_SPEED_FS:
				startMask = 0x08;
				break;
			case SCE_USBD_DEVICE_SPEED_HS:
				index = (SceUInt8)(endpoint->bInterval - 1);
				startMask = index < 3 ? s_ehciInterruptStartMask[index] : 0x08;
				break;
			default:
				startMask = 0;
				break;
			}
			qh->capability |= startMask;
			ehciLinkQh(ehci, descriptor, descriptor->frame);
		}
	}
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return pipe;

free:
	usbdFreePipe(pipe);
fail:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return NULL;
}

static int ehciSuspend(UsbdBus *bus)
{
	return ehciStop(bus);
}

/* a high speed isochronous transfer: one iTD, a packet per microframe */
static int ehciSubmitItd(Ehci *ehci, UsbdPipe *pipe, UsbdRequest *request)
{
	EhciDescriptor *descriptor, *template;
	EhciItd *itd;
	SceUIntPtr pa, page;
	unsigned int offset;
	int i, last, packets, frame;

	if (request != NULL)
		USBD_LIST_APPEND(pipe->requestHead, pipe->requestTail, request);
	request = pipe->requestHead;
	if (request == NULL) {
		if (pipe->waiting)
			ehciUnwaitPipe(ehci, pipe);
		return SCE_USBD_ERROR_INVALID_PARAM;
	}
	offset = (SceUIntPtr)request->buffer & 0xfff;
	if (request->length > EHCI_ITD_MAX_LENGTH - offset || request->isochRequest.numPackets > 8)
		return SCE_USBD_ERROR_INVALID_PARAM;

	template = pipe->endpoint.ehci;
	descriptor = ehciAllocDescriptor(&ehci->freeItd, &ehci->freeCount.itd);
	if (descriptor == NULL) {
		if (pipe->waiting)
			ehciUnwaitPipe(ehci, pipe);
		return 0;
	}
	if (ksceKernelGetMemBlockMemtypeByAddr(request->buffer) != USBD_MEMTYPE_UNCACHED) {
		if (pipe->direction)
			ksceKernelDcacheCleanInvalidateRange((void *)USBD_CACHE_BASE(request->buffer),
							     USBD_CACHE_SIZE(request->buffer, request->length));
		else
			ksceKernelDcacheCleanRange((void *)USBD_CACHE_BASE(request->buffer),
						   USBD_CACHE_SIZE(request->buffer, request->length));
	}
	USBD_LIST_REMOVE(pipe->requestHead, pipe->requestTail, request);
	if (pipe->requestHead == NULL && pipe->waiting)
		ehciUnwaitPipe(ehci, pipe);
	USBD_LIST_APPEND(pipe->descHead.ehci, pipe->descTail.ehci, descriptor);

	itd = descriptor->hw.itd;
	*itd = *template->hw.itd;
	page = EHCI_PAGE(usbdVAtoPA(request->buffer));
	packets = request->isochRequest.numPackets;
	for (i = 0; i < packets; i++) {
		unsigned int length = request->isochRequest.packets[i].length;

		itd->transaction[i] = EHCI_ITD_ACTIVE | (length & EHCI_ITD_LENGTH) << EHCI_ITD_LENGTH_SHIFT |
				      offset;
		offset += length;
	}
	last = packets - 1;
	itd->transaction[last] |= EHCI_ITD_IOC;
	for (i = 0; i < 7; i++)
		itd->buffer[i] |= page + i * 0x1000;
	ehci->itdRequest[descriptor - ehci->itdRecords.base] = request;

	if (pipe->frame >= 0)
		frame = pipe->frame + 1;
	else
		frame = (ehci->regs->frIndex >> 3) + 2;
	frame %= ehci->frameListSize;
	pipe->frame = frame;
	descriptor->frame = frame;
	{
		SceUInt32 *link = ehciLinkFromPhys(ehci, ehci->frameList[frame] & EHCI_LINK_ADDRESS);

		pa = descriptor->pa;
		*descriptor->hw.link = *link;
		*link = pa | EHCI_LINK_ITD;
	}
	return 0;
}

/* a full speed isochronous transfer through a hub: an siTD per packet */
static int ehciSubmitSitd(Ehci *ehci, UsbdPipe *pipe, UsbdRequest *request)
{
	EhciDescriptor *descriptor[8];
	EhciDescriptor *template;
	EhciSitd *sitd;
	SceUIntPtr pa;
	SceUInt32 startMask, completionMask, position, count;
	int i, packets, frame;

	if (request != NULL)
		USBD_LIST_APPEND(pipe->requestHead, pipe->requestTail, request);
	request = pipe->requestHead;
	if (request == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	packets = request->isochRequest.numPackets;
	if (ehci->freeCount.sitd < packets) {
		if (pipe->waiting)
			ehciUnwaitPipe(ehci, pipe);
		return 0;
	}
	for (i = 0; i < packets; i++)
		descriptor[i] = ehciAllocDescriptor(&ehci->freeSitd, &ehci->freeCount.sitd);

	template = pipe->endpoint.ehci;
	USBD_LIST_REMOVE(pipe->requestHead, pipe->requestTail, request);
	pa = usbdVAtoPA(request->isochRequest.buffer);
	if (ksceKernelGetMemBlockMemtypeByAddr(request->buffer) != USBD_MEMTYPE_UNCACHED) {
		if (pipe->direction)
			ksceKernelDcacheCleanInvalidateRange((void *)USBD_CACHE_BASE(request->buffer),
							     USBD_CACHE_SIZE(request->buffer, request->length));
		else
			ksceKernelDcacheCleanRange((void *)USBD_CACHE_BASE(request->buffer),
						   USBD_CACHE_SIZE(request->buffer, request->length));
	}

	startMask = 0;
	completionMask = 0;
	position = EHCI_SITD_POSITION_ALL;
	count = 0;
	for (i = 0; i < packets; i++) {
		unsigned int length = request->isochRequest.packets[i].length;
		SceUInt32 *link;
		unsigned int k;

		sitd = descriptor[i]->hw.sitd;
		ehci->sitdRequest[(EhciSplitDescriptor *)descriptor[i] -
				  (EhciSplitDescriptor *)ehci->sitdRecords.base] = request;
		*sitd = *template->hw.sitd;
		if (pipe->direction) {
			startMask = EHCI_SITD_START_IN;
			completionMask = EHCI_SITD_COMPLETION_IN;
		} else {
			/* an OUT goes out 188 bytes a microframe */
			if (length == 0) {
				count = 1;
			} else {
				count = length / EHCI_SPLIT_PAYLOAD;
				if (length % EHCI_SPLIT_PAYLOAD != 0)
					count++;
			}
			position = count == 1 ? EHCI_SITD_POSITION_ALL : EHCI_SITD_POSITION_BEGIN;
			for (k = 0; k < count; k++)
				startMask |= 2 << k;
		}
		sitd->schedule = (SceUInt8)startMask | completionMask << EHCI_SITD_COMPLETION_SHIFT;
		sitd->state = (i == packets - 1 ? EHCI_SITD_IOC : 0) |
			      (length & EHCI_SITD_LENGTH) << EHCI_SITD_LENGTH_SHIFT | EHCI_SITD_ACTIVE;
		sitd->buffer[0] = pa;
		sitd->buffer[1] = (EHCI_PAGE(pa) + 0x1000) | (count & EHCI_SITD_COUNT) | position;

		if (pipe->frame >= 0)
			frame = pipe->frame + 1;
		else
			frame = ((ehci->frameListSize - 1) & (ehci->regs->frIndex >> 3)) + 10;
		frame %= ehci->frameListSize;
		pipe->frame = frame;
		descriptor[i]->frame = frame;
		((EhciSplitDescriptor *)descriptor[i])->packet = i;
		USBD_LIST_APPEND(pipe->descHead.ehci, pipe->descTail.ehci, descriptor[i]);

		link = ehciLinkFromPhys(ehci, ehci->frameList[frame] & EHCI_LINK_ADDRESS);
		sitd->link = *link;
		*link = descriptor[i]->pa | EHCI_LINK_SITD;
		pa += length;
	}
	return 0;
}

static __always_inline void ehciFillQtdBuffer(EhciQtd *qtd, SceUIntPtr pa)
{
	qtd->buffer[0] = pa;
	qtd->buffer[1] = EHCI_PAGE(pa + 0x1000);
	qtd->buffer[2] = EHCI_PAGE(pa + 0x2000);
	qtd->buffer[3] = EHCI_PAGE(pa + 0x3000);
	qtd->buffer[4] = EHCI_PAGE(pa + 0x4000);
}

static __always_inline void ehciQueueQtd(Ehci *ehci, UsbdPipe *pipe, EhciDescriptor *descriptor,
					 UsbdRequest *request)
{
	USBD_LIST_APPEND(pipe->descHead.ehci, pipe->descTail.ehci, descriptor);
	ehci->qtdRequest[descriptor - ehci->qtdRecords.base] = request;
}

static __always_inline int ehciWaitPipe(Ehci *ehci, UsbdPipe *pipe)
{
	if (!pipe->waiting) {
		USBD_LIST_APPEND_LINK(ehci->waitHead, ehci->waitTail, pipe, waitNext, waitPrev);
		pipe->waiting = USBD_PIPE_WAIT_DESCRIPTORS;
	}
	return 0;
}

/* a control transfer: the setup, data and status stages */
static int ehciSubmitControl(Ehci *ehci, UsbdPipe *pipe, UsbdRequest *request)
{
	EhciDescriptor *setupStage, *dataStage, *statusStage;
	SceUsbdDeviceRequest *setup;
	EhciQh *qh;
	EhciQtd *qtd;
	int count;

	if (request != NULL)
		USBD_LIST_APPEND(pipe->requestHead, pipe->requestTail, request);
	request = pipe->requestHead;
	if (request == NULL) {
		if (pipe->waiting)
			ehciUnwaitPipe(ehci, pipe);
		return SCE_USBD_ERROR_INVALID_PARAM;
	}
	if (pipe->busy)
		return ehciWaitPipe(ehci, pipe);

	setup = request->setup;
	qh = pipe->endpoint.ehci->hw.qh;
	if (qh->token & EHCI_QTD_ERRORS)
		qh->token &= ~EHCI_QTD_ERRORS;
	if (ksceKernelGetMemBlockMemtypeByAddr(setup) != USBD_MEMTYPE_UNCACHED)
		ksceKernelDcacheCleanRange((void *)USBD_CACHE_BASE(setup),
					   USBD_CACHE_SIZE(setup, sizeof(*setup)));

	if (setup->wLength != 0) {
		count = ehci->freeCount.qtd;
		if (count <= 2)
			return ehciWaitPipe(ehci, pipe);
		setupStage = ehciAllocDescriptor(&ehci->freeQtd, &ehci->freeCount.qtd);
		statusStage = ehciAllocDescriptor(&ehci->freeQtd, &ehci->freeCount.qtd);
		dataStage = ehciAllocDescriptor(&ehci->freeQtd, &ehci->freeCount.qtd);
	} else {
		count = ehci->freeCount.qtd;
		if (count <= 1)
			return ehciWaitPipe(ehci, pipe);
		setupStage = ehciAllocDescriptor(&ehci->freeQtd, &ehci->freeCount.qtd);
		statusStage = ehciAllocDescriptor(&ehci->freeQtd, &ehci->freeCount.qtd);
		dataStage = NULL;
	}

	pipe->busy = 1;
	USBD_LIST_REMOVE(pipe->requestHead, pipe->requestTail, request);
	if (pipe->requestHead == NULL && pipe->waiting)
		ehciUnwaitPipe(ehci, pipe);

	qtd = setupStage->hw.qtd;
	qtd->next = dataStage != NULL ? dataStage->pa : statusStage->pa;
	qtd->altNext = EHCI_LINK_TERMINATE;
	qtd->token = sizeof(*setup) << EHCI_QTD_LENGTH_SHIFT | EHCI_QTD_ERROR_COUNTER_3 |
		     EHCI_QTD_PID_SETUP | EHCI_QTD_ACTIVE;
	ehciFillQtdBuffer(qtd, usbdVAtoPA(setup));
	ehciQueueQtd(ehci, pipe, setupStage, request);

	if (dataStage != NULL) {
		qtd = dataStage->hw.qtd;
		qtd->next = statusStage->pa;
		qtd->altNext = EHCI_LINK_TERMINATE;
		qtd->token = (setup->bmRequestType & SCE_USBD_REQTYPE_DIR_BITS ? EHCI_QTD_PID_IN : 0) |
			     EHCI_QTD_TOGGLE | (setup->wLength & EHCI_QTD_LENGTH) << EHCI_QTD_LENGTH_SHIFT |
			     EHCI_QTD_ERROR_COUNTER_3 | EHCI_QTD_ACTIVE;
		ehciFillQtdBuffer(qtd, usbdVAtoPA(request->buffer));
		ehciQueueQtd(ehci, pipe, dataStage, request);
		if (ksceKernelGetMemBlockMemtypeByAddr(request->buffer) != USBD_MEMTYPE_UNCACHED) {
			if (setup->bmRequestType & SCE_USBD_REQTYPE_DIR_BITS)
				ksceKernelDcacheCleanInvalidateRange((void *)USBD_CACHE_BASE(request->buffer),
								     USBD_CACHE_SIZE(request->buffer, request->length));
			else
				ksceKernelDcacheCleanRange((void *)USBD_CACHE_BASE(request->buffer),
							   USBD_CACHE_SIZE(request->buffer, request->length));
		}
	}

	/* the status stage goes the other way */
	qtd = statusStage->hw.qtd;
	qtd->next = EHCI_LINK_TERMINATE;
	qtd->altNext = EHCI_LINK_TERMINATE;
	qtd->buffer[0] = 0;
	qtd->buffer[1] = 0;
	qtd->buffer[2] = 0;
	qtd->buffer[3] = 0;
	qtd->buffer[4] = 0;
	qtd->token = (setup->bmRequestType & SCE_USBD_REQTYPE_DIR_BITS ? EHCI_QTD_PID_OUT : EHCI_QTD_PID_IN) |
		     EHCI_QTD_TOGGLE | EHCI_QTD_IOC | EHCI_QTD_ERROR_COUNTER_3 | EHCI_QTD_ACTIVE;
	ehciQueueQtd(ehci, pipe, statusStage, request);

	pipe->endpoint.ehci->hw.qh->nextQtd = usbdVAtoPA(setupStage->hw.qtd);
	return 0;
}

/* a bulk or interrupt transfer: a qTD per five pages */
static int ehciSubmitBulk(Ehci *ehci, UsbdPipe *pipe, UsbdRequest *request)
{
	EhciDescriptor *descriptor;
	EhciQtd *qtd, *first, *prev;
	EhciQh *qh;
	SceUIntPtr pa, page;
	unsigned int offset, chunk, size, remaining;

	if (request != NULL)
		USBD_LIST_APPEND(pipe->requestHead, pipe->requestTail, request);
	request = pipe->requestHead;
	if (request == NULL) {
		if (pipe->waiting)
			ehciUnwaitPipe(ehci, pipe);
		return SCE_USBD_ERROR_INVALID_PARAM;
	}
	if (pipe->busy)
		return ehciWaitPipe(ehci, pipe);

	/* an unaligned buffer loses the fifth page */
	offset = (SceUIntPtr)request->buffer & 0xfff;
	chunk = offset != 0 ? EHCI_QTD_MAX_LENGTH - EHCI_QTD_PAGE : EHCI_QTD_MAX_LENGTH;
	if ((request->length - 1 + chunk) / chunk > (unsigned int)ehci->freeCount.qtd)
		return ehciWaitPipe(ehci, pipe);

	pipe->busy = 1;
	USBD_LIST_REMOVE(pipe->requestHead, pipe->requestTail, request);
	if (pipe->requestHead == NULL && pipe->waiting)
		ehciUnwaitPipe(ehci, pipe);

	if (request->buffer != NULL) {
		pa = usbdVAtoPA(request->buffer);
		page = EHCI_PAGE(pa);
	} else {
		page = 0;
		pa = 0;
	}
	if (request->syncCache &&
	    ksceKernelGetMemBlockMemtypeByAddr(request->buffer) != USBD_MEMTYPE_UNCACHED) {
		if (pipe->direction)
			ksceKernelDcacheCleanInvalidateRange((void *)USBD_CACHE_BASE(request->buffer),
							     USBD_CACHE_SIZE(request->buffer, request->length));
		else
			ksceKernelDcacheCleanRange((void *)USBD_CACHE_BASE(request->buffer),
						   USBD_CACHE_SIZE(request->buffer, request->length));
	}

	remaining = request->length;
	qh = pipe->endpoint.ehci->hw.qh;
	qh->currentQtd = 0;
	first = NULL;
	prev = NULL;
	for (;;) {
		descriptor = ehciAllocDescriptor(&ehci->freeQtd, &ehci->freeCount.qtd);
		size = remaining < chunk ? remaining : chunk;
		ehci->qtdRequest[descriptor - ehci->qtdRecords.base] = request;
		if (offset != 0 && remaining - size <= EHCI_QTD_PAGE - offset)
			size = remaining;
		qtd = descriptor->hw.qtd;
		if (qtd == NULL)
			return SCE_USBD_ERROR_FATAL;
		qtd->next = EHCI_LINK_TERMINATE;
		qtd->altNext = EHCI_LINK_TERMINATE;
		if (first == NULL)
			first = qtd;
		qtd->buffer[0] = pa;
		qtd->buffer[1] = page + 0x1000;
		qtd->buffer[2] = page + 0x2000;
		qtd->buffer[3] = page + 0x3000;
		qtd->buffer[4] = page + 0x4000;
		qtd->token = (pipe->direction ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT) |
			     (size & EHCI_QTD_LENGTH) << EHCI_QTD_LENGTH_SHIFT |
			     EHCI_QTD_ERROR_COUNTER_3 | EHCI_QTD_ACTIVE;
		if (prev != NULL)
			prev->next = usbdVAtoPA(qtd);
		USBD_LIST_APPEND(pipe->descHead.ehci, pipe->descTail.ehci, descriptor);
		remaining -= size;
		if (remaining == 0)
			break;
		pa += size;
		prev = qtd;
		page = EHCI_PAGE(pa);
	}
	qtd->token |= EHCI_QTD_IOC;
	qh->nextQtd = usbdVAtoPA(first);
	return 0;
}

static int ehciClosePipe(UsbdPipe *pipe)
{
	EhciDescriptor *descriptor, *qhDescriptor;
	EhciPipeEntry *entry;
	UsbdRequest *request;
	Ehci *ehci;
	int i, j, result, state;

	if (pipe == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	if (pipe->device == NULL)
		return SCE_USBD_ERROR_DEVICE_NOT_FOUND;
	ehci = s_ehci[pipe->device->bus->index];
	if (ehci == NULL)
		return SCE_USBD_ERROR_NOT_INITIALIZED;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	if (pipe->device == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
		goto out;
	}
	while ((request = pipe->requestHead) != NULL) {
		USBD_LIST_REMOVE(pipe->requestHead, pipe->requestTail, request);
		usbdFreeRequest(request);
	}

	if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS) {
		if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_HS) {
			for (i = 0; i < ehci->count.itd; i++) {
				request = ehci->itdRequest[i];
				if (request == NULL || request->pipe != pipe)
					continue;
				ehci->itdRequest[i] = NULL;
				descriptor = &ehci->itdRecords.base[i];
				for (j = 0; j < 8; j++)
					descriptor->hw.itd->transaction[j] &= ~EHCI_ITD_ACTIVE;
				ehciUnlinkPeriodic(ehci, descriptor);
				ehciFreeDescriptor(&ehci->freeItd, &ehci->freeCount.itd, descriptor);
			}
			if (pipe->endpoint.ehci != NULL)
				ehciFreeDescriptor(&ehci->freeItd, &ehci->freeCount.itd, pipe->endpoint.ehci);
		} else {
			for (i = 0; i < ehci->count.sitd; i++) {
				request = ehci->sitdRequest[i];
				if (request == NULL || request->pipe != pipe)
					continue;
				ehci->sitdRequest[i] = NULL;
				descriptor = &((EhciSplitDescriptor *)ehci->sitdRecords.base)[i].descriptor;
				descriptor->hw.sitd->state &= ~EHCI_SITD_ACTIVE;
				ehciUnlinkPeriodic(ehci, descriptor);
				ehciFreeDescriptor(&ehci->freeSitd, &ehci->freeCount.sitd, descriptor);
			}
			if (pipe->endpoint.ehci != NULL)
				ehciFreeDescriptor(&ehci->freeSitd, &ehci->freeCount.sitd, pipe->endpoint.ehci);
		}
		goto release;
	}

	qhDescriptor = pipe->endpoint.ehci;
	qhDescriptor->hw.qh->token |= EHCI_QTD_HALTED;
	for (i = 0; i < ehci->count.qtd; i++) {
		request = ehci->qtdRequest[i];
		if (request == NULL || request->pipe != pipe)
			continue;
		ehci->qtdRequest[i] = NULL;
		descriptor = &ehci->qtdRecords.base[i];
		descriptor->hw.qtd->token &= ~EHCI_QTD_ACTIVE;
		ehciFreeDescriptor(&ehci->freeQtd, &ehci->freeCount.qtd, descriptor);
		usbdFreeRequest(request);
	}

	if (qhDescriptor->frame == ehci->controlSlot || qhDescriptor->frame == ehci->bulkSlot) {
		/* off the asynchronous ring, then wait for the controller to let go */
		EhciQh *prev = ehci->asyncHead->hw.qh;
		EhciQh *last = ehci->asyncTail->hw.qh;
		SceUIntPtr pa;
		int timeout;

		if (prev == last)
			goto release;
		pa = prev->link & EHCI_LINK_ADDRESS;
		while (pa != qhDescriptor->pa) {
			prev = ehciQhFromPhys(ehci, pa);
			if (prev == last)
				goto release;
			pa = prev->link & EHCI_LINK_ADDRESS;
		}
		prev->link = qhDescriptor->hw.qh->link;
		ehci->regs->usbCmd |= EHCI_USBCMD_ASYNC_ADVANCE;
		for (timeout = EHCI_RING_TIMEOUT; timeout != 0; timeout--) {
			if (ehci->regs->usbSts & EHCI_STS_ASYNC_ADVANCE)
				break;
		}
	} else {
		/* out of the interrupt tree, with the periodic schedule off */
		EhciQh *prev = &ehci->staticQh[qhDescriptor->frame];
		SceUInt32 link = prev->link;

		while ((link & EHCI_LINK_ADDRESS) != qhDescriptor->pa) {
			if (link == EHCI_LINK_TERMINATE)
				goto release;
			prev = ehciQhFromPhys(ehci, link & EHCI_LINK_ADDRESS);
			link = prev->link;
		}
		ehci->regs->usbCmd &= ~EHCI_USBCMD_PERIODIC;
		prev->link = qhDescriptor->hw.qh->link;
		ehci->regs->usbCmd |= EHCI_USBCMD_PERIODIC;
	}

release:
	for (entry = ehci->pipeHead; entry != NULL; entry = entry->next) {
		if (entry->pipe == pipe)
			break;
	}
	if (entry == NULL) {
		result = 0;
		goto out;
	}
	pipe = entry->pipe;
	if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS) {
		if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_HS) {
			if (pipe->endpoint.ehci != NULL)
				ehciFreeDescriptor(&ehci->freeItd, &ehci->freeCount.itd, pipe->endpoint.ehci);
		} else {
			if (pipe->endpoint.ehci != NULL)
				ehciFreeDescriptor(&ehci->freeSitd, &ehci->freeCount.sitd, pipe->endpoint.ehci);
		}
	} else {
		if (pipe->endpoint.ehci != NULL)
			ehciFreeDescriptor(&ehci->freeQh, &ehci->freeCount.qh, pipe->endpoint.ehci);
	}
	pipe->endpoint.ehci = NULL;
	pipe->hcd = NULL;
	usbdFreePipe(pipe);
	USBD_LIST_REMOVE(ehci->pipeHead, ehci->pipeTail, entry);
	USBD_LIST_APPEND(ehci->freePipeHead, ehci->freePipeTail, entry);
	result = 0;
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return result;
}

static __always_inline void ehciInvalidateRequest(UsbdRequest *request)
{
	if (ksceKernelGetMemBlockMemtypeByAddr(request->buffer) != USBD_MEMTYPE_UNCACHED &&
	    request->pipe->direction == SCE_USBD_ENDPOINT_DIRECTION_IN)
		ksceKernelDcacheInvalidateRange((void *)USBD_CACHE_BASE(request->buffer),
						USBD_CACHE_SIZE(request->buffer, request->length));
}

/* the qTDs of a finished control, bulk or interrupt transfer */
static __always_inline void ehciCompleteQtd(Ehci *ehci, UsbdPipe *pipe)
{
	EhciDescriptor *descriptor, *last = pipe->descTail.ehci;
	UsbdRequest *request;
	EhciQh *qh;
	SceUInt32 token;
	int result, count;

	if (last == NULL)
		return;
	request = ehci->qtdRequest[last - ehci->qtdRecords.base];
	if (request == NULL)
		return;
	count = request->length;
	qh = pipe->endpoint.ehci->hw.qh;
	if ((last->hw.qtd->token & EHCI_QTD_ACTIVE) && !(qh->token & EHCI_QTD_HALTED))
		return;

	pipe->busy = 0;
	result = request->result;
	while ((descriptor = pipe->descHead.ehci) != NULL) {
		USBD_LIST_REMOVE(pipe->descHead.ehci, pipe->descTail.ehci, descriptor);
		token = descriptor->hw.qtd->token;
		ehci->qtdRequest[descriptor - ehci->qtdRecords.base] = NULL;
		if ((token & EHCI_QTD_ERRORS) && (token & EHCI_QTD_HALTED) && result == 0) {
			result = SCE_USBD_ERROR_8024000A;
			request->result = result;
		}
		if ((token & EHCI_QTD_PID) != EHCI_QTD_PID_SETUP)
			count -= (token >> EHCI_QTD_LENGTH_SHIFT) & EHCI_QTD_LENGTH;
		ehciFreeDescriptor(&ehci->freeQtd, &ehci->freeCount.qtd, descriptor);
	}
	token = qh->token;
	if (result != 0)
		count = 0;
	request->count = count;
	if (token & EHCI_QTD_HALTED) {
		token &= ~EHCI_QTD_TOGGLE;
		qh->token = token;
	}
	if (token & EHCI_QTD_ERRORS)
		qh->token = token & ~EHCI_QTD_ERRORS;
	if (request->syncCache)
		ehciInvalidateRequest(request);
	if (request->complete != NULL)
		request->complete(request);
}

/* the siTDs the controller finished */
static __always_inline void ehciCompleteSitd(Ehci *ehci, UsbdPipe *pipe)
{
	EhciDescriptor *descriptor = pipe->descHead.ehci;
	UsbdRequest *request, *done;
	UsbdIsochPacket *status;
	SceUInt32 state;
	int index, error;

	if (descriptor == NULL || (descriptor->hw.sitd->state & EHCI_SITD_ACTIVE))
		return;
	request = ehci->sitdRequest[(EhciSplitDescriptor *)descriptor -
				    (EhciSplitDescriptor *)ehci->sitdRecords.base];
	if (request == NULL)
		return;
	do {
		USBD_LIST_REMOVE(pipe->descHead.ehci, pipe->descTail.ehci, descriptor);
		index = (EhciSplitDescriptor *)descriptor - (EhciSplitDescriptor *)ehci->sitdRecords.base;
		status = &request->isochStatus.packets[((EhciSplitDescriptor *)descriptor)->packet];
		done = ehci->sitdRequest[index];
		ehci->sitdRequest[index] = NULL;
		state = descriptor->hw.sitd->state;
		if (state & EHCI_SITD_ERRORS) {
			error = state & EHCI_SITD_MISSED_MICROFRAME ? EHCI_ISOCH_MISSED : EHCI_ISOCH_ERROR;
			status->status = error;
			if (done->result == 0)
				done->result = error;
			status->length = 0;
		} else {
			status->status = 0;
			status->length = request->isochRequest.packets[((EhciSplitDescriptor *)descriptor)->packet].length -
					 ((state >> EHCI_SITD_LENGTH_SHIFT) & 0x7ff);
		}
		if (state & EHCI_SITD_IOC) {
			ehciInvalidateRequest(done);
			if (done->complete != NULL)
				done->complete(done);
		}
		ehciUnlinkPeriodic(ehci, descriptor);
		ehciFreeDescriptor(&ehci->freeSitd, &ehci->freeCount.sitd, descriptor);
		descriptor = pipe->descHead.ehci;
	} while (descriptor != NULL && !(descriptor->hw.sitd->state & EHCI_SITD_ACTIVE));
}

/* the iTD, when all of its packets are done */
static __always_inline void ehciCompleteItd(Ehci *ehci, UsbdPipe *pipe)
{
	EhciDescriptor *descriptor = pipe->descHead.ehci;
	UsbdRequest *request;
	SceUInt32 transaction;
	EhciItd *itd;
	int i, index, packets, status, length;

	if (descriptor == NULL)
		return;
	index = descriptor - ehci->itdRecords.base;
	request = ehci->itdRequest[index];
	if (request == NULL)
		return;
	itd = descriptor->hw.itd;
	packets = request->isochRequest.numPackets;
	for (i = 0; i < packets; i++) {
		if (itd->transaction[i] & EHCI_ITD_ACTIVE)
			return;
	}
	USBD_LIST_REMOVE(pipe->descHead.ehci, pipe->descTail.ehci, descriptor);
	ehci->itdRequest[index] = NULL;
	ehciUnlinkPeriodic(ehci, descriptor);
	for (i = 0; i < request->isochRequest.numPackets; i++) {
		transaction = itd->transaction[i];
		if ((transaction & EHCI_ITD_BUFFER_ERROR) ||
		    (transaction & (EHCI_ITD_BABBLE | EHCI_ITD_XACT_ERROR))) {
			status = EHCI_ISOCH_ERROR;
			request->result = SCE_USBD_ERROR_80240009;
			length = 0;
		} else {
			status = 0;
			length = request->isochRequest.packets[i].length -
				 ((transaction >> EHCI_ITD_LENGTH_SHIFT) & EHCI_ITD_LENGTH);
		}
		request->isochStatus.packets[i].length = length;
		request->isochStatus.packets[i].status = status;
	}
	ehciFreeDescriptor(&ehci->freeItd, &ehci->freeCount.itd, descriptor);
	ehciInvalidateRequest(request);
	if (request->complete != NULL)
		request->complete(request);
}

/* complete what the controller finished, then retry the waiting pipes */
static void ehciProcessDone(Ehci *ehci)
{
	EhciPipeEntry *entry;
	UsbdPipe *pipe;

	for (entry = ehci->pipeHead; entry != NULL; entry = entry->next) {
		pipe = entry->pipe;
		if (pipe->type != SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS)
			ehciCompleteQtd(ehci, pipe);
		else if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_HS)
			ehciCompleteItd(ehci, pipe);
		else
			ehciCompleteSitd(ehci, pipe);
	}

	for (pipe = ehci->waitHead; pipe != NULL; pipe = pipe->waitNext) {
		if (pipe->waiting != USBD_PIPE_WAIT_DESCRIPTORS)
			continue;
		if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL)
			ehciSubmitControl(ehci, pipe, NULL);
		else if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK ||
			 pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT)
			ehciSubmitBulk(ehci, pipe, NULL);
	}
	for (pipe = ehci->waitHead; pipe != NULL; pipe = pipe->waitNext) {
		if (pipe->waiting != USBD_PIPE_WAIT_ISOCHRONOUS ||
		    pipe->type != SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS)
			continue;
		if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_HS)
			ehciSubmitItd(ehci, pipe, NULL);
		else
			ehciSubmitSitd(ehci, pipe, NULL);
	}
}

static __always_inline void ehciDetectPortChange(Ehci *ehci)
{
	volatile EhciRegs *regs = ehci->regs;
	SceUInt32 portSc;

	regs->usbSts = EHCI_STS_PORT_CHANGE;
	portSc = regs->portSc;
	regs->portSc = portSc | EHCI_PORTSC_OVER_CURRENT_CHANGE | EHCI_PORTSC_ENABLE_CHANGE |
		       EHCI_PORTSC_CONNECT_CHANGE;
	if (!(portSc & EHCI_PORTSC_CONNECT) && ehci->portState > EHCI_PORT_POWERED) {
		if (ehci->portState > EHCI_PORT_RESET)
			usbdNotifyDetach(USBD_ID(ehci->bus, 0), 0);
		ehci->portState = EHCI_PORT_POWERED;
	}
	if ((portSc & (EHCI_PORTSC_CONNECT | EHCI_PORTSC_CONNECT_CHANGE)) ==
	    (EHCI_PORTSC_CONNECT | EHCI_PORTSC_CONNECT_CHANGE)) {
		if (ehci->portState != EHCI_PORT_POWERED)
			usbdNotifyDetach(USBD_ID(ehci->bus, 0), 0);
		ehci->portState = EHCI_PORT_CONNECTED;
		ksceKernelRegisterTimer("SceUsbdAlarm1", 100000, ehciConnectDone, ehci);
	}
	if (portSc & EHCI_PORTSC_OVER_CURRENT_CHANGE)
		SCE_DBG_PRINTF_FUNC(15, 1729, "ehciDetectPortChange", "#### Detect Over-current\n");
	ehci->portChangeCount++;
}

static int ehciInterruptHandler(int intrCode, void *arg)
{
	Ehci *ehci = arg;
	SceUInt32 status;
	int state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	status = ehci->regs->usbSts & ehci->regs->usbIntr;
	if (status & EHCI_STS_HOST_ERROR) {
		SCE_DBG_PRINTF(6, 1745, "Host System Error\n");
		ehci->regs->usbSts = EHCI_STS_HOST_ERROR;
		ehci->hostErrorCount++;
	}
	if (status & EHCI_STS_ERROR) {
		ehci->regs->usbSts = EHCI_STS_ERROR;
		ehciProcessDone(ehci);
		ehci->errorCount++;
	}
	if (status & EHCI_STS_INT) {
		ehci->regs->usbSts = EHCI_STS_INT;
		ehciProcessDone(ehci);
		ehci->intCount++;
	}
	if (status & EHCI_STS_PORT_CHANGE)
		ehciDetectPortChange(ehci);
	if (status & EHCI_STS_FRAME_ROLLOVER) {
		ehci->frameRolloverCount++;
		ehci->regs->usbSts = EHCI_STS_FRAME_ROLLOVER;
	}
	if (status & EHCI_STS_ASYNC_ADVANCE) {
		ehci->regs->usbSts = EHCI_STS_ASYNC_ADVANCE;
		ehci->asyncAdvanceCount++;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return -1;
}

static int ehciSubmitRequest(UsbdPipe *pipe, UsbdRequest *request)
{
	Ehci *ehci = s_ehci[pipe->device->bus->index];
	int result, state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ehci->lock);
	switch (pipe->type) {
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL:
		result = ehciSubmitControl(ehci, pipe, request);
		break;
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK:
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
		result = ehciSubmitBulk(ehci, pipe, request);
		break;
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS:
		if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_HS)
			result = ehciSubmitItd(ehci, pipe, request);
		else if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_FS)
			result = ehciSubmitSitd(ehci, pipe, request);
		else
			result = SCE_USBD_ERROR_INVALID_PARAM;
		break;
	default:
		result = SCE_USBD_ERROR_FATAL;
		break;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ehci->lock, state);
	return result;
}

int ehciStart(UsbdBus *bus)
{
	volatile EhciRegs *regs;
	Ehci *ehci;
	int result;

	if (bus == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	ehci = s_ehci[bus->index];
	if (ehci == NULL)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	result = ksceKernelEnableIntr(ehci->intrCode);
	if (result < 0)
		return result;

	regs = ehci->regs;
	ehci->portCount = regs->hcsParams & EHCI_HCSPARAMS_N_PORTS;
	regs->insnReg01 = 0x01000010;
	regs->asyncListAddr = ehci->asyncHead->pa;
	regs->periodicListBase = usbdVAtoPA(ehci->frameList);
	regs = ehci->regs;
	regs->usbCmd = EHCI_USBCMD_ITC_1 | EHCI_USBCMD_FRAME_LIST_256;
	regs->usbCmd |= EHCI_USBCMD_ASYNC | EHCI_USBCMD_PERIODIC | EHCI_USBCMD_RUN;
	regs->usbIntr = EHCI_STS_HOST_ERROR | EHCI_STS_PORT_CHANGE | EHCI_STS_ERROR | EHCI_STS_INT;
	regs->configFlag |= EHCI_CONFIGFLAG_EHCI;
	return 0;
}

int ehciStop(UsbdBus *bus)
{
	volatile EhciRegs *regs;
	Ehci *ehci = s_ehci[bus->index];
	int port, timeout, result;

	result = ksceKernelDisableIntr(ehci->intrCode);
	if (result < 0)
		return result;
	for (port = 0; port < ehci->portCount; port++)
		ehciPowerOffPort(bus, port);

	regs = ehci->regs;
	regs->usbIntr = 0;
	regs->usbCmd &= ~EHCI_USBCMD_RUN;
	regs->usbCmd |= EHCI_USBCMD_RESET;
	for (timeout = EHCI_RING_TIMEOUT; regs->usbCmd & EHCI_USBCMD_RESET; ) {
		if (--timeout == 0)
			return SCE_USBD_ERROR_FATAL;
	}
	return 0;
}

/* frame n of the 256 serves the leaf whose index has n's bits reversed */
static __always_inline unsigned int ehciReverseFrame(unsigned int leaf)
{
	unsigned int frame = 0;
	int bit;

	for (bit = 0; bit < 8; bit++) {
		if (leaf & 1 << bit)
			frame |= 0x80 >> bit;
	}
	return frame;
}

static __always_inline void ehciInitDescriptors(EhciDescriptorList *list, EhciDescriptor *records,
						SceUInt8 *area, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		records[i].hw.link = (SceUInt32 *)(area + i * EHCI_DESCRIPTOR_SIZE);
		records[i].pa = usbdVAtoPA(records[i].hw.link);
		USBD_LIST_APPEND(list->head, list->tail, &records[i]);
	}
}

int ehciInit(UsbdBus *bus)
{
	SceKernelAllocMemBlockKernelOpt opt;
	EhciSplitDescriptor *sitdRecords;
	EhciPipeEntry *pipeEntry;
	EhciQh *staticQh, *anchor;
	EhciItd *dummyItd;
	EhciSitd *dummySitd;
	UsbdHcd *hcd;
	const char *name;
	SceUInt8 *base;
	void *records;
	Ehci *ehci;
	SceUIntPtr paddr;
	SceSize size;
	int i, index, intrCode, result;

	index = bus->index;
	ehci = usbdAlloc(sizeof(*ehci));
	if (ehci == NULL)
		return SCE_USBD_ERROR_NO_MEMORY;
	memset(ehci, 0, sizeof(*ehci));
	ehci->bus = index;
	ehci->hcd = &bus->ehci;
	ehci->lock = 0;
	s_ehci[index] = ehci;

	switch (index) {
	case 0:
		paddr = 0xe40e0000;
		break;
	case 1:
		paddr = 0xe40b0000;
		break;
	case 2:
		paddr = 0xe4020000;
		break;
	default:
		result = SCE_USBD_ERROR_FATAL;
		goto fail;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = paddr;
	result = ksceKernelAllocMemBlock("SceUsbdEhci", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
					 0x1000, &opt);
	if (result < 0)
		goto fail;
	ehci->registerBlockId = result;
	result = ksceKernelGetMemBlockBase(ehci->registerBlockId, (void **)&base);
	if (result < 0)
		goto freeRegisters;
	ehci->frameListSize = EHCI_FRAME_LIST_SIZE;
	ehci->regs = (volatile EhciRegs *)base;
	ehci->count = s_ehciDescriptorCount[ehci->bus];
	ehci->freeCount = ehci->count;

	/* the frame list, the pools and the static QHs share 64 KiB */
	size = (ehci->count.qh + ehci->count.qtd + ehci->count.itd + ehci->count.sitd +
		EHCI_STATIC_QH_COUNT) * EHCI_DESCRIPTOR_SIZE + 0x1400;
	if ((size & ~0xfff) > 0x10000) {
		result = SCE_USBD_ERROR_FATAL;
		goto fail;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	opt.alignment = 0x1000;
	result = ksceKernelAllocMemBlock("SceUsbdEhci", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_DEVICE_RW,
					 0x10000, &opt);
	if (result < 0)
		goto freeRegisters;
	ehci->descriptorBlockId = result;
	result = ksceKernelGetMemBlockBase(ehci->descriptorBlockId, (void **)&base);
	if (result < 0)
		goto freeBlocks;
	memset(base, 0, 0x10000);

	ehci->itdArea.base = base + EHCI_FRAME_LIST_SIZE * sizeof(SceUInt32);
	ehci->itdArea.end = ehci->itdArea.base + ehci->count.itd * EHCI_DESCRIPTOR_SIZE;
	ehci->qhArea.base = ehci->itdArea.end;
	ehci->qhArea.end = ehci->qhArea.base + ehci->count.qh * EHCI_DESCRIPTOR_SIZE;
	ehci->qtdArea.base = ehci->qhArea.end;
	ehci->qtdArea.end = ehci->qtdArea.base + ehci->count.qtd * EHCI_DESCRIPTOR_SIZE;
	ehci->sitdArea.base = ehci->qtdArea.end;
	ehci->sitdArea.end = ehci->sitdArea.base + ehci->count.sitd * EHCI_DESCRIPTOR_SIZE;
	ehci->staticArea.base = ehci->sitdArea.end;
	ehci->staticArea.end = ehci->staticArea.base + EHCI_STATIC_QH_COUNT * EHCI_DESCRIPTOR_SIZE;
	ehci->qhRange.base = usbdVAtoPA(ehci->qhArea.base);
	ehci->qhRange.end = usbdVAtoPA(ehci->qhArea.end);
	ehci->itdRange.base = usbdVAtoPA(ehci->itdArea.base);
	ehci->itdRange.end = usbdVAtoPA(ehci->itdArea.end);
	ehci->sitdRange.base = usbdVAtoPA(ehci->sitdArea.base);
	ehci->sitdRange.end = usbdVAtoPA(ehci->sitdArea.end);
	ehci->qtdRange.base = usbdVAtoPA(ehci->qtdArea.base);
	ehci->qtdRange.end = usbdVAtoPA(ehci->qtdArea.end);
	ehci->staticRange.base = usbdVAtoPA(ehci->staticArea.base);
	ehci->staticRange.end = usbdVAtoPA(ehci->staticArea.end);

	/* the records, the pipe entries and the request maps */
	size = (ehci->count.qh + ehci->count.qtd + ehci->count.itd) * sizeof(EhciDescriptor) +
	       ehci->count.sitd * sizeof(EhciSplitDescriptor) +
	       ehci->count.qh * sizeof(EhciPipeEntry) +
	       (ehci->count.qtd + ehci->count.itd + ehci->count.sitd) * sizeof(UsbdRequest *);
	records = usbdAlloc(size);
	if (records == NULL) {
		result = SCE_USBD_ERROR_NO_MEMORY;
		goto freeBlocks;
	}
	memset(records, 0, size);
	ehci->freeQh.head = NULL;
	ehci->freeQh.tail = NULL;
	ehci->qhRecords.base = records;
	ehci->qhRecords.end = ehci->qhRecords.base + ehci->count.qh;
	ehci->qtdRecords.base = ehci->qhRecords.end;
	ehci->qtdRecords.end = ehci->qtdRecords.base + ehci->count.qtd;
	ehci->itdRecords.base = ehci->qtdRecords.end;
	ehci->itdRecords.end = ehci->itdRecords.base + ehci->count.itd;
	sitdRecords = (EhciSplitDescriptor *)ehci->itdRecords.end;
	ehci->sitdRecords.base = &sitdRecords[0].descriptor;
	pipeEntry = (EhciPipeEntry *)&sitdRecords[ehci->count.sitd];
	ehci->sitdRecords.end = (EhciDescriptor *)pipeEntry;
	ehci->qtdRequest = (UsbdRequest **)&pipeEntry[ehci->count.qh];
	ehci->itdRequest = ehci->qtdRequest + ehci->count.qtd;
	ehci->sitdRequest = ehci->itdRequest + ehci->count.itd;
	ehci->freeQtd.head = NULL;
	ehci->freeQtd.tail = NULL;
	ehci->freeItd.head = NULL;
	ehci->freeItd.tail = NULL;
	ehci->freeSitd.head = NULL;
	ehci->freeSitd.tail = NULL;
	ehci->freePipeHead = NULL;
	ehci->freePipeTail = NULL;

	ehciInitDescriptors(&ehci->freeQh, ehci->qhRecords.base, ehci->qhArea.base, ehci->count.qh);
	ehciInitDescriptors(&ehci->freeQtd, ehci->qtdRecords.base, ehci->qtdArea.base, ehci->count.qtd);
	ehciInitDescriptors(&ehci->freeItd, ehci->itdRecords.base, ehci->itdArea.base, ehci->count.itd);
	for (i = 0; i < ehci->count.sitd; i++) {
		sitdRecords[i].descriptor.hw.link =
			(SceUInt32 *)(ehci->sitdArea.base + i * EHCI_DESCRIPTOR_SIZE);
		sitdRecords[i].descriptor.pa = usbdVAtoPA(sitdRecords[i].descriptor.hw.link);
		USBD_LIST_APPEND(ehci->freeSitd.head, ehci->freeSitd.tail, &sitdRecords[i].descriptor);
	}
	for (i = 0; i < ehci->count.qh; i++)
		USBD_LIST_APPEND(ehci->freePipeHead, ehci->freePipeTail, &pipeEntry[i]);

	/* the asynchronous ring: its head and last QH, never active */
	ehci->asyncHead = ehciAllocDescriptor(&ehci->freeQh, &ehci->freeCount.qh);
	ehci->asyncTail = ehciAllocDescriptor(&ehci->freeQh, &ehci->freeCount.qh);
	ehci->asyncHead->hw.qh->link = ehci->asyncTail->pa | EHCI_LINK_QH;
	ehci->asyncHead->hw.qh->endpoint = EHCI_QH_HEAD;
	ehci->asyncHead->hw.qh->token = EHCI_QTD_HALTED;
	ehci->asyncTail->hw.qh->link = ehci->asyncHead->pa | EHCI_LINK_QH;
	ehci->asyncTail->hw.qh->token = EHCI_QTD_HALTED;

	/* the interrupt tree: each static QH links to its parent, the leaves are the frames */
	staticQh = (EhciQh *)ehci->staticArea.base;
	ehci->staticQh = staticQh;
	for (i = 0; i < EHCI_TREE_SIZE; i++) {
		staticQh[i].token = EHCI_QTD_HALTED;
		if (i == 0) {
			staticQh[i].link = EHCI_LINK_TERMINATE;
		} else {
			staticQh[i].link = usbdVAtoPA(&staticQh[(i - 1) / 2]) | EHCI_LINK_QH;
			if (i >= EHCI_TREE_LEAF)
				((SceUInt32 *)base)[ehciReverseFrame(i - EHCI_TREE_LEAF)] =
					usbdVAtoPA(&staticQh[i]) | EHCI_LINK_QH;
		}
	}
	ehci->frameList = (SceUInt32 *)base;

	/* the anchors the control and bulk QHs follow, in the ring after its head */
	anchor = &staticQh[EHCI_TREE_SIZE];
	anchor->token = EHCI_QTD_HALTED;
	anchor->link = ehci->asyncHead->hw.qh->link;
	ehci->asyncHead->hw.qh->link = usbdVAtoPA(anchor) | EHCI_LINK_QH;
	ehci->controlSlot = EHCI_TREE_SIZE;
	anchor = &staticQh[EHCI_TREE_SIZE + 1];
	anchor->link = ehci->asyncHead->hw.qh->link;
	anchor->token = EHCI_QTD_HALTED;
	ehci->asyncHead->hw.qh->link = usbdVAtoPA(anchor) | EHCI_LINK_QH;
	ehci->bulkSlot = ehci->controlSlot + 1;

	/* an empty iTD and siTD at the root, before the isochronous descriptors */
	dummyItd = (EhciItd *)&staticQh[EHCI_TREE_SIZE + 2];
	dummyItd->link = EHCI_LINK_TERMINATE;
	ehci->staticQh[0].link = usbdVAtoPA(dummyItd) | EHCI_LINK_ITD;
	dummySitd = (EhciSitd *)&staticQh[EHCI_TREE_SIZE + 3];
	dummySitd->link = ehci->staticQh[0].link;
	ehci->staticQh[0].link = usbdVAtoPA(dummySitd) | EHCI_LINK_SITD;

	switch (ehci->bus) {
	case 0:
		name = "SceUsbEhci0";
		intrCode = 146;
		break;
	case 1:
		name = "SceUsbEhci1";
		intrCode = 148;
		break;
	case 2:
		name = "SceUsbEhci2";
		intrCode = 150;
		break;
	default:
		result = SCE_USBD_ERROR_FATAL;
		goto fail;
	}
	result = ksceKernelRegisterIntrHandler(intrCode, name, 0, ehciInterruptHandler, ehci,
					       160, 15, NULL);
	if (result >= 0) {
		ehci->intrCode = intrCode;
		ehci->waitHead = NULL;
		ehci->waitTail = NULL;
		ehci->pipeHead = NULL;
		ehci->pipeTail = NULL;
		ehci->portState = EHCI_PORT_OFF;
		ehci->bandwidth = usbdAlloc(EHCI_FRAME_LIST_SIZE * sizeof(int));
		if (ehci->bandwidth != NULL) {
			if (ehci->frameListSize > 0)
				memset(ehci->bandwidth, 0, ehci->frameListSize * sizeof(int));
			memset(&ehci->intCount, 0, 6 * sizeof(unsigned int));
			goto done;
		}
	}
	usbdFree(records);
	ksceKernelFreeMemBlock(ehci->descriptorBlockId);
	ksceKernelFreeMemBlock(ehci->registerBlockId);
	if (result < 0)
		goto fail;

done:
	hcd = &bus->ehci;
	hcd->getPortCount = ehciGetPortCount;
	hcd->allocPipe = ehciAllocPipe;
	hcd->closePipe = ehciClosePipe;
	hcd->submitRequest = ehciSubmitRequest;
	hcd->setAddress = ehciSetAddress;
	hcd->setMaxPacketSize = ehciSetMaxPacketSize;
	hcd->resetFrame = ehciResetFrame;
	hcd->powerOnPort = ehciPowerOnPort;
	hcd->powerOffPort = ehciPowerOffPort;
	hcd->getSpeed = ehciGetSpeed;
	hcd->getFrameNumber = ehciGetFrameNumber;
	hcd->resetPort = ehciResetPort;
	hcd->suspend = ehciSuspend;
	hcd->resume = ehciResume;
	return 0;

freeBlocks:
	ksceKernelFreeMemBlock(ehci->descriptorBlockId);
freeRegisters:
	ksceKernelFreeMemBlock(ehci->registerBlockId);
fail:
	s_ehci[ehci->bus] = NULL;
	usbdFree(ehci);
	return result;
}

int ehciTerm(UsbdBus *bus)
{
	Ehci *ehci = s_ehci[bus->index];
	int result;

	result = ehciStop(bus);
	if (result < 0)
		return result;
	result = ksceKernelDisableIntr(ehci->intrCode);
	if (result < 0)
		return result;
	result = ksceKernelReleaseIntrHandler(ehci->intrCode);
	if (result < 0)
		return result;
	result = ksceKernelFreeMemBlock(ehci->descriptorBlockId);
	if (result < 0)
		return result;
	result = ksceKernelFreeMemBlock(ehci->registerBlockId);
	if (result < 0)
		return result;
	s_ehci[ehci->bus] = NULL;
	usbdFree(ehci);
	return 0;
}
