/*
 * SceUsbd: the OHCI host controller driver (full and low speed), the EHCI's
 * companion on each bus.
 *
 * The controller's descriptors live in one 16 KiB block: the HCCA, then the
 * isochronous TD, general TD and ED pools, then the static EDs. Each pool
 * descriptor has a record (OhciDescriptor) on a free list; a busy TD's record
 * is on its pipe's descriptor list and maps to the request it carries. Every
 * ED keeps an empty TD at its tail (UsbdPipe.dummy): a transfer fills it and
 * appends a new empty one. The static EDs are the interrupt tree (63 nodes,
 * the 32 leaves in the HCCA), the control and bulk list heads and the
 * isochronous anchor after the tree's root.
 */

#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <usbd/usb_regs.h>
#include "usbd_internal.h"

/* the operational registers, 0x200 into the EHCI's register page */
typedef struct OhciRegs {
	SceUInt32 revision;
	SceUInt32 control;
	SceUInt32 commandStatus;
	SceUInt32 interruptStatus;
	SceUInt32 interruptEnable;
	SceUInt32 interruptDisable;
	SceUInt32 hcca;
	SceUInt32 periodCurrentEd;
	SceUInt32 controlHeadEd;
	SceUInt32 controlCurrentEd;
	SceUInt32 bulkHeadEd;
	SceUInt32 bulkCurrentEd;
	SceUInt32 doneHead;
	SceUInt32 fmInterval;
	SceUInt32 fmRemaining;
	SceUInt32 fmNumber;
	SceUInt32 periodicStart;
	SceUInt32 lsThreshold;
	SceUInt32 rhDescriptorA;
	struct {
		SceUInt32 deviceRemovable : 16;
		SceUInt32 portPowerControlMask : 16;
	} rhDescriptorB;
	SceUInt32 rhStatus;
	SceUInt32 rhPortStatus;
} OhciRegs;

#define OHCI_REGS_OFFSET		0x200

/* HcControl */
#define OHCI_CONTROL_CBSR_4_1		0x00000003	/* four control EDs per bulk ED */
#define OHCI_CONTROL_PERIODIC		0x00000004
#define OHCI_CONTROL_ISOCHRONOUS	0x00000008
#define OHCI_CONTROL_CONTROL		0x00000010
#define OHCI_CONTROL_BULK		0x00000020
#define OHCI_CONTROL_LISTS		0x0000003c
#define OHCI_CONTROL_OPERATIONAL	0x00000080

/* HcCommandStatus */
#define OHCI_COMMAND_RESET		0x00000001
#define OHCI_COMMAND_CONTROL_FILLED	0x00000002
#define OHCI_COMMAND_BULK_FILLED	0x00000004

/* HcInterruptStatus, HcInterruptEnable and HcInterruptDisable */
#define OHCI_INT_SCHEDULING_OVERRUN	0x00000001
#define OHCI_INT_WRITEBACK_DONE		0x00000002
#define OHCI_INT_START_OF_FRAME		0x00000004
#define OHCI_INT_RESUME_DETECTED	0x00000008
#define OHCI_INT_UNRECOVERABLE_ERROR	0x00000010
#define OHCI_INT_FRAME_OVERFLOW		0x00000020
#define OHCI_INT_ROOT_HUB_CHANGE	0x00000040
#define OHCI_INT_OWNERSHIP_CHANGE	0x40000000
#define OHCI_INT_MASTER			0x80000000
#define OHCI_INT_SOF_BIT		2	/* Ohci.interruptCount */

/* HcFmInterval and HcPeriodicStart: a 12000 bit time frame */
#define OHCI_FM_INTERVAL		0x27782edf
#define OHCI_PERIODIC_START		0x2a2f

/* HcRhDescriptorA */
#define OHCI_RH_PORT_COUNT		0x000000ff
#define OHCI_RH_PER_PORT_POWER		0x00000100
#define OHCI_RH_PER_PORT_OVER_CURRENT	0x00000800

/* HcRhPortStatus; written, the low bits are commands */
#define OHCI_PORT_CONNECT		0x00000001
#define OHCI_PORT_ENABLE		0x00000002
#define OHCI_PORT_SUSPEND		0x00000004
#define OHCI_PORT_OVER_CURRENT		0x00000008
#define OHCI_PORT_RESET			0x00000010
#define OHCI_PORT_POWER			0x00000100
#define OHCI_PORT_LOW_SPEED		0x00000200	/* written: power off */
#define OHCI_PORT_CONNECT_CHANGE	0x00010000
#define OHCI_PORT_ENABLE_CHANGE		0x00020000
#define OHCI_PORT_SUSPEND_CHANGE	0x00040000
#define OHCI_PORT_OVER_CURRENT_CHANGE	0x00080000
#define OHCI_PORT_RESET_CHANGE		0x00100000
#define OHCI_PORT_CHANGES		0x001f0000

/* the host controller communications area */
typedef struct OhciHcca {
	SceUInt32 interruptTable[32];
	SceUInt16 frameNumber;
	SceUInt16 pad1;
	SceUInt32 doneHead;
	SceUInt8 reserved[116];
} OhciHcca;

/* the hardware descriptors */
typedef struct OhciEd {
	SceUInt32 control;
	SceUInt32 tailTd;
	SceUInt32 headTd;		/* and the halted and toggle carry bits */
	SceUInt32 nextEd;
} OhciEd;

typedef struct OhciTd {
	SceUInt32 control;
	SceUInt32 currentBuffer;
	SceUInt32 nextTd;
	SceUInt32 bufferEnd;
} OhciTd;

typedef struct OhciItd {
	SceUInt32 control;
	SceUInt32 bufferPage;
	SceUInt32 nextTd;
	SceUInt32 bufferEnd;
	SceUInt16 packet[8];		/* offset, then the packet status word */
} OhciItd;

/* OhciEd.control */
#define OHCI_ED_ADDRESS			0x0000007f
#define OHCI_ED_ENDPOINT_SHIFT		7
#define OHCI_ED_OUT			0x00000800
#define OHCI_ED_IN			0x00001000
#define OHCI_ED_LOW_SPEED		0x00002000
#define OHCI_ED_SKIP			0x00004000
#define OHCI_ED_ISOCHRONOUS		0x00008000
#define OHCI_ED_MAX_PACKET_SHIFT	16
#define OHCI_ED_MAX_PACKET		0x07ff0000

/* OhciEd.headTd */
#define OHCI_ED_HALTED			0x00000001
#define OHCI_ED_ADDRESS_MASK		0xfffffff0

/* OhciTd.control */
#define OHCI_TD_ROUNDING		0x00040000
#define OHCI_TD_DP			0x00180000
#define OHCI_TD_DP_SETUP		0x00000000
#define OHCI_TD_DP_OUT			0x00080000
#define OHCI_TD_DP_IN			0x00100000
#define OHCI_TD_DI			0x00e00000
#define OHCI_TD_DI_NONE			0x00e00000	/* no interrupt */
#define OHCI_TD_DATA0			0x02000000
#define OHCI_TD_DATA1			0x03000000
#define OHCI_TD_CC_SHIFT		28
#define OHCI_TD_NOT_ACCESSED		0xe0000000

#define OHCI_TD_SETUP			(OHCI_TD_NOT_ACCESSED | OHCI_TD_DATA0 | OHCI_TD_DI_NONE | OHCI_TD_DP_SETUP)
#define OHCI_TD_DATA_IN			(OHCI_TD_NOT_ACCESSED | OHCI_TD_DATA1 | OHCI_TD_DI_NONE | \
					 OHCI_TD_DP_IN | OHCI_TD_ROUNDING)
#define OHCI_TD_DATA_OUT		(OHCI_TD_NOT_ACCESSED | OHCI_TD_DATA1 | OHCI_TD_DI_NONE | \
					 OHCI_TD_DP_OUT | OHCI_TD_ROUNDING)
#define OHCI_TD_STATUS_IN		(OHCI_TD_NOT_ACCESSED | OHCI_TD_DATA1 | OHCI_TD_DP_IN)
#define OHCI_TD_STATUS_OUT		(OHCI_TD_NOT_ACCESSED | OHCI_TD_DATA1 | OHCI_TD_DP_OUT)
#define OHCI_TD_BULK			(OHCI_TD_NOT_ACCESSED | OHCI_TD_DI_NONE | OHCI_TD_DP | OHCI_TD_ROUNDING)
#define OHCI_TD_BULK_LAST		(OHCI_TD_NOT_ACCESSED | OHCI_TD_DP | OHCI_TD_ROUNDING)

/* the condition codes */
#define OHCI_CC_STALL			4

/* OhciItd.control and packet */
#define OHCI_ITD_FRAME			0x0000ffff
#define OHCI_ITD_FRAME_COUNT_SHIFT	24
#define OHCI_ITD_PAGE_OFFSET		0x0fff
#define OHCI_ITD_PACKET_CC		0xf000
#define OHCI_ITD_PACKET_NOT_ACCESSED	0xe000
#define OHCI_ITD_PACKET_SIZE		0x07ff

#define OHCI_ED_SIZE			16
#define OHCI_TD_SIZE			16
#define OHCI_ITD_SIZE			32

#define OHCI_FRAME_COUNT		32	/* the HCCA's interrupt table */
#define OHCI_TREE_SIZE			63	/* static EDs of the interrupt tree */
#define OHCI_TREE_LEAF			31
#define OHCI_CONTROL_SLOT		63	/* the control list head */
#define OHCI_BULK_SLOT			64	/* the bulk list head */
#define OHCI_ISOCHRONOUS_SLOT		65	/* after the tree's root */
#define OHCI_STATIC_ED_COUNT		66
#define OHCI_DESCRIPTOR_BLOCK_SIZE	0x4000
#define OHCI_RESET_TIMEOUT		1000
#define OHCI_START_TIMEOUT		10

/* the bookkeeping record of a pool descriptor */
typedef struct OhciDescriptor {
	struct OhciDescriptor *next;
	struct OhciDescriptor *prev;
	union {
		OhciEd *ed;
		OhciTd *td;
		OhciItd *itd;
	} hw;
	SceUIntPtr pa;
} OhciDescriptor;

/* an ED's record */
typedef struct OhciEndpoint {
	OhciDescriptor descriptor;
	int slot;			/* the static ED it follows */
} OhciEndpoint;

typedef struct OhciArea {
	SceUInt8 *base;
	SceUInt8 *end;
} OhciArea;

typedef struct OhciRange {
	SceUIntPtr base;
	SceUIntPtr end;
} OhciRange;

typedef struct OhciRecords {
	OhciDescriptor *base;
	OhciDescriptor *end;
} OhciRecords;

typedef struct OhciDescriptorList {
	OhciDescriptor *head;
	OhciDescriptor *tail;
} OhciDescriptorList;

/* the pool sizes of a bus */
typedef struct OhciDescriptorCount {
	SceUInt8 ed;
	SceUInt8 td;
	SceUInt8 itd;
} OhciDescriptorCount;

/* Ohci.portState */
#define OHCI_PORT_OFF			0
#define OHCI_PORT_POWERED		1
#define OHCI_PORT_CONNECTED		2	/* debouncing */
#define OHCI_PORT_RESETTING		3
#define OHCI_PORT_ENABLED		5	/* attached */

typedef struct Ohci {
	SceUInt8 bus;
	OhciDescriptorCount count;
	OhciDescriptorCount freeCount;
	SceUInt8 portCount;
	int bandwidth[OHCI_FRAME_COUNT];	/* per frame: bytes of the interrupt EDs */
	int intrCode;
	SceUID descriptorBlockId;
	SceUID registerBlockId;
	unsigned int portState;		/* OHCI_PORT_* */
	int lock;
	UsbdHcd *hcd;
	volatile OhciRegs *regs;
	OhciHcca *hcca;
	OhciEd *staticEd;
	UsbdRequest **tdRequest;	/* the request of each busy TD */
	UsbdRequest **itdRequest;
	OhciArea edArea;
	OhciArea tdArea;
	OhciArea itdArea;
	OhciArea staticArea;
	OhciRange edRange;
	OhciRange tdRange;
	OhciRange itdRange;
	OhciRange staticRange;
	OhciEndpoint *edRecords;
	OhciEndpoint *edRecordsEnd;
	OhciRecords tdRecords;
	OhciRecords itdRecords;
	OhciDescriptorList freeEd;
	OhciDescriptorList freeTd;
	OhciDescriptorList freeItd;
	UsbdPipe *waitHead;		/* pipes waiting for descriptors */
	UsbdPipe *waitTail;
	unsigned int interruptCount[8];	/* per HcInterruptStatus bit: only start of frame */
} Ohci;

/* SceSysmemForDriver_F3BBE2E1 of uncached memory: no cache maintenance */
#define USBD_MEMTYPE_UNCACHED		0x8000

#define USBD_CACHE_LINE			64
#define USBD_CACHE_BASE(addr)		((SceUIntPtr)(addr) & ~(USBD_CACHE_LINE - 1))
#define USBD_CACHE_SIZE(addr, size)	\
	((((SceUIntPtr)(addr) + (size) + USBD_CACHE_LINE - 1) & ~(USBD_CACHE_LINE - 1)) - \
	 USBD_CACHE_BASE(addr))

static const OhciDescriptorCount s_ohciDescriptorCount[USBD_BUS_COUNT] = {
	{ 16, 32, 64 },
	{ 16, 32, 0 },
	{ 16, 32, 64 },
};

static Ohci *s_ohci[USBD_BUS_COUNT];

static __always_inline OhciDescriptor *ohciAllocDescriptor(OhciDescriptorList *list,
							   SceUInt8 *count)
{
	OhciDescriptor *descriptor = list->head;

	if (descriptor != NULL) {
		USBD_LIST_REMOVE(list->head, list->tail, descriptor);
		(*count)--;
	}
	return descriptor;
}

/* a TD or isochronous TD, unlinked */
static __always_inline OhciDescriptor *ohciAllocTd(OhciDescriptorList *list, SceUInt8 *count)
{
	OhciDescriptor *descriptor = list->head;

	if (descriptor != NULL) {
		USBD_LIST_REMOVE(list->head, list->tail, descriptor);
		descriptor->hw.td->nextTd = 0;
		(*count)--;
	}
	return descriptor;
}

/* a descriptor already free is left alone */
static __always_inline void ohciFreeDescriptor(OhciDescriptorList *list, SceUInt8 *count,
					       OhciDescriptor *descriptor)
{
	OhciDescriptor *entry;

	if (descriptor == NULL)
		return;
	for (entry = list->head; entry != NULL; entry = entry->next) {
		if (entry == descriptor)
			return;
	}
	USBD_LIST_APPEND(list->head, list->tail, descriptor);
	(*count)++;
}

static void ohciFreeEd(Ohci *ohci, OhciDescriptor *descriptor)
{
	ohciFreeDescriptor(&ohci->freeEd, &ohci->freeCount.ed, descriptor);
}

/* a descriptor by its bus address */
static __always_inline void *ohciFromPhys(Ohci *ohci, SceUIntPtr pa)
{
	if (pa >= ohci->tdRange.base && pa < ohci->tdRange.end)
		return ohci->tdArea.base + ((pa - ohci->tdRange.base) & ~(OHCI_TD_SIZE - 1));
	if (pa >= ohci->itdRange.base && pa < ohci->itdRange.end)
		return ohci->itdArea.base + ((pa - ohci->itdRange.base) & ~(OHCI_ITD_SIZE - 1));
	if (pa >= ohci->edRange.base && pa < ohci->edRange.end)
		return ohci->edArea.base + ((pa - ohci->edRange.base) & ~(OHCI_ED_SIZE - 1));
	if (pa >= ohci->staticRange.base && pa < ohci->staticRange.end)
		return ohci->staticArea.base + ((pa - ohci->staticRange.base) & ~(OHCI_ED_SIZE - 1));
	return NULL;
}

static __always_inline int ohciTdIndex(Ohci *ohci, OhciTd *td)
{
	return td - (OhciTd *)ohci->tdArea.base;
}

static __always_inline int ohciItdIndex(Ohci *ohci, OhciItd *itd)
{
	return itd - (OhciItd *)ohci->itdArea.base;
}

static int ohciSetMaxPacketSize(UsbdPipe *pipe, int size)
{
	OhciEd *ed;

	if (pipe->type != SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	switch (pipe->device->speed) {
	case SCE_USBD_DEVICE_SPEED_LS:
		if (size != 8)
			return SCE_USBD_ERROR_INVALID_PARAM;
		break;
	case SCE_USBD_DEVICE_SPEED_FS:
		if (size != 8 && size != 16 && size != 32 && size != 64)
			return SCE_USBD_ERROR_INVALID_PARAM;
		break;
	default:
		return SCE_USBD_ERROR_FATAL;
	}
	ed = pipe->endpoint.ohci->hw.ed;
	ed->control = (ed->control & ~OHCI_ED_MAX_PACKET) | size << OHCI_ED_MAX_PACKET_SHIFT;
	return 0;
}

static int ohciSetAddress(UsbdPipe *pipe, int address)
{
	pipe->endpoint.ohci->hw.ed->control |= address & OHCI_ED_ADDRESS;
	return 0;
}

/* in microframes, as the EHCI counts */
static int ohciGetFrameNumber(UsbdBus *bus, unsigned int *frame, unsigned int *mask)
{
	*frame = (SceUInt16)s_ohci[bus->index]->regs->fmNumber << 3;
	*mask = 0xffff << 3;
	return 0;
}

static int ohciGetSpeed(UsbdBus *bus)
{
	if (s_ohci[bus->index]->regs->rhPortStatus & OHCI_PORT_LOW_SPEED)
		return SCE_USBD_DEVICE_SPEED_LS;
	return SCE_USBD_DEVICE_SPEED_FS;
}

static int ohciGetPortCount(UsbdBus *bus)
{
	return s_ohci[bus->index]->portCount;
}

static int ohciPowerOffPort(UsbdBus *bus, int port)
{
	Ohci *ohci = s_ohci[bus->index];
	int state;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&ohci->lock);
	ohci->portState = OHCI_PORT_OFF;
	ohci->regs->rhPortStatus = OHCI_PORT_LOW_SPEED;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ohci->lock, state);
	return 0;
}

static int ohciResetPort(UsbdBus *bus, int port)
{
	Ohci *ohci = s_ohci[bus->index];
	int state;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&ohci->lock);
	ohci->portState = OHCI_PORT_RESETTING;
	ohci->regs->rhPortStatus = OHCI_PORT_RESET;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ohci->lock, state);
	return 0;
}

static int ohciPowerOnPort(UsbdBus *bus, int port)
{
	Ohci *ohci = s_ohci[bus->index];
	int state;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&ohci->lock);
	if (ohci->portState <= OHCI_PORT_POWERED) {
		ohci->portState = OHCI_PORT_POWERED;
		ohci->regs->rhPortStatus = OHCI_PORT_POWER;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ohci->lock, state);
	return 0;
}

/* a connection held 200 ms: reset the device */
static SceUInt32 ohciConnectDone(void *arg)
{
	Ohci *ohci = arg;
	int state;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&ohci->lock);
	if (ohci->portState == OHCI_PORT_CONNECTED) {
		ohci->portState = OHCI_PORT_RESETTING;
		ohci->regs->rhPortStatus = OHCI_PORT_RESET;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ohci->lock, state);
	return 0;
}

static void ohciUnwaitPipe(Ohci *ohci, UsbdPipe *pipe)
{
	USBD_LIST_REMOVE_LINK(ohci->waitHead, ohci->waitTail, pipe, waitNext, waitPrev);
	pipe->waiting = 0;
}

static __always_inline void ohciWaitPipe(Ohci *ohci, UsbdPipe *pipe, int reason)
{
	USBD_LIST_APPEND_LINK(ohci->waitHead, ohci->waitTail, pipe, waitNext, waitPrev);
	pipe->waiting = reason;
}

/* halt the controller and reset it */
static __always_inline int ohciReset(Ohci *ohci)
{
	int timeout = OHCI_RESET_TIMEOUT;

	ohci->regs->interruptDisable = ~0;
	ohci->regs->control &= ~OHCI_CONTROL_LISTS;
	ohci->regs->commandStatus = OHCI_COMMAND_RESET;
	ohci->regs->control = 0;
	while (ohci->regs->commandStatus & OHCI_COMMAND_RESET) {
		sceKernelDelayThread(1000);
		if (--timeout == 0)
			return SCE_USBD_ERROR_FATAL;
	}
	return 0;
}

static int ohciSuspend(UsbdBus *bus)
{
	return ohciStop(bus);
}

static int ohciClosePipe(UsbdPipe *pipe)
{
	OhciDescriptor *endpoint, *descriptor;
	UsbdRequest *request;
	OhciEd *ed, *prev;
	SceUIntPtr pa;
	Ohci *ohci;
	int i, frame, cost, state;

	if (pipe == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	ohci = s_ohci[pipe->device->bus->index];
	if (ohci == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&ohci->lock);
	if (pipe->device == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ohci->lock, state);
		return SCE_USBD_ERROR_DEVICE_NOT_FOUND;
	}

	/* skip the ED and unlink it, with its list off */
	endpoint = pipe->endpoint.ohci;
	ed = endpoint->hw.ed;
	ed->control |= OHCI_ED_SKIP;
	if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL)
		ohci->regs->control &= ~OHCI_CONTROL_CONTROL;
	else if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK)
		ohci->regs->control &= ~OHCI_CONTROL_BULK;
	for (prev = &ohci->staticEd[((OhciEndpoint *)endpoint)->slot]; ; prev = ohciFromPhys(ohci, pa)) {
		pa = prev->nextEd;
		if (pa == 0)
			break;
		if (pa == endpoint->pa) {
			prev->nextEd = ed->nextEd;
			break;
		}
	}
	if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL)
		ohci->regs->control |= OHCI_CONTROL_CONTROL;
	else if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK)
		ohci->regs->control |= OHCI_CONTROL_BULK;

	while ((request = pipe->requestHead) != NULL) {
		USBD_LIST_REMOVE(pipe->requestHead, pipe->requestTail, request);
		usbdFreeRequest(request);
	}

	if (pipe->slot == OHCI_ISOCHRONOUS_SLOT) {
		for (i = 0; i < ohci->count.itd; i++) {
			request = ohci->itdRequest[i];
			if (request == NULL || request->pipe != pipe)
				continue;
			ohci->itdRequest[i] = NULL;
			descriptor = &ohci->itdRecords.base[i];
			ohciFreeDescriptor(&ohci->freeItd, &ohci->freeCount.itd, descriptor);
			usbdFreeRequest(request);
		}
		ohciFreeDescriptor(&ohci->freeItd, &ohci->freeCount.itd, pipe->dummy.ohci);
	} else {
		for (i = 0; i < ohci->count.td; i++) {
			request = ohci->tdRequest[i];
			if (request == NULL || request->pipe != pipe)
				continue;
			ohci->tdRequest[i] = NULL;
			descriptor = &ohci->tdRecords.base[i];
			ohciFreeDescriptor(&ohci->freeTd, &ohci->freeCount.td, descriptor);
			usbdFreeRequest(request);
		}
		ohciFreeDescriptor(&ohci->freeTd, &ohci->freeCount.td, pipe->dummy.ohci);
	}
	ed->tailTd = 0;
	ed->headTd = 0;

	/* give back its interrupt bandwidth */
	cost = pipe->bandwidthCost;
	if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_LS)
		cost *= 8;
	for (i = 0; i < pipe->bandwidthCount; i++) {
		frame = pipe->bandwidthFrame + i * pipe->bandwidthPeriod;
		ohci->bandwidth[frame] -= cost;
		if (ohci->bandwidth[frame] < 0)
			ohci->bandwidth[frame] = 0;
	}

	if (pipe->waiting)
		ohciUnwaitPipe(ohci, pipe);
	ohciFreeEd(ohci, pipe->endpoint.ohci);
	usbdFreePipe(pipe);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ohci->lock, state);
	return 0;
}

/* the least loaded branch of the interrupt tree at the endpoint's interval */
static __always_inline void ohciScheduleInterrupt(Ohci *ohci, UsbdPipe *pipe, int interval,
						  int maxPacketSize, int speed)
{
	int period, count, frame, best, load, least, cost, i;

	if (interval >= 32) {
		period = 32;
		count = 1;
	} else if (interval >= 16) {
		period = 16;
		count = 2;
	} else if (interval >= 8) {
		period = 8;
		count = 4;
	} else if (interval >= 4) {
		period = 4;
		count = 8;
	} else if (interval >= 2) {
		period = 2;
		count = 16;
	} else {
		period = 1;
		count = 32;
	}
	best = -1;
	least = 0;
	for (frame = 0; frame < period; frame++) {
		load = 0;
		for (i = 0; i < count; i++)
			load += ohci->bandwidth[frame + i * period];
		if (best < 0 || load < least) {
			best = frame;
			least = load;
		}
	}
	pipe->bandwidthFrame = best;
	pipe->bandwidthCount = count;
	cost = (maxPacketSize < 64 ? maxPacketSize : 64) + 13;
	pipe->bandwidthPeriod = period;
	pipe->bandwidthCost = cost;
	if (speed == SCE_USBD_DEVICE_SPEED_LS)
		cost *= 8;
	for (i = 0; i < count; i++)
		ohci->bandwidth[best + i * period] += cost;
	pipe->slot = period - 1 + best;
}

static UsbdPipe *ohciAllocPipe(UsbdDevice *device, const SceUsbdEndpointDescriptor *endpoint)
{
	OhciDescriptor *descriptor, *dummy;
	OhciEd *ed, *slot;
	UsbdPipe *pipe;
	Ohci *ohci;
	int maxPacketSize, state;

	if (device == NULL)
		return NULL;
	ohci = s_ohci[device->bus->index];
	if (ohci == NULL)
		return NULL;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&ohci->lock);
	pipe = usbdAllocPipe(device, endpoint);
	if (pipe == NULL)
		goto out;
	descriptor = ohciAllocDescriptor(&ohci->freeEd, &ohci->freeCount.ed);
	if (descriptor == NULL) {
		usbdFreePipe(pipe);
		pipe = NULL;
		goto out;
	}
	pipe->endpoint.ohci = descriptor;
	pipe->hcd = ohci->hcd;
	ed = descriptor->hw.ed;
	memset(ed, 0, sizeof(*ed));

	if (endpoint == NULL) {
		/* the default pipe */
		ed->control = (device->speed == SCE_USBD_DEVICE_SPEED_LS ? OHCI_ED_LOW_SPEED : 0) |
			      device->address |
			      device->maxPacketSize0 << OHCI_ED_MAX_PACKET_SHIFT;
		pipe->slot = OHCI_CONTROL_SLOT;
		((OhciEndpoint *)descriptor)->slot = OHCI_CONTROL_SLOT;
	} else {
		maxPacketSize = endpoint->wMaxPacketSize & 0x7ff;
		ed->control = (pipe->direction == SCE_USBD_ENDPOINT_DIRECTION_IN ? OHCI_ED_IN : OHCI_ED_OUT) |
			      maxPacketSize << OHCI_ED_MAX_PACKET_SHIFT |
			      (endpoint->bEndpointAddress & 0xf) << OHCI_ED_ENDPOINT_SHIFT |
			      device->address |
			      (device->speed == SCE_USBD_DEVICE_SPEED_LS ? OHCI_ED_LOW_SPEED : 0) |
			      (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS ?
			       OHCI_ED_ISOCHRONOUS : 0);
		switch (pipe->type) {
		case SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL:
			pipe->slot = OHCI_CONTROL_SLOT;
			((OhciEndpoint *)descriptor)->slot = OHCI_CONTROL_SLOT;
			break;
		case SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS:
			pipe->slot = OHCI_ISOCHRONOUS_SLOT;
			((OhciEndpoint *)descriptor)->slot = OHCI_ISOCHRONOUS_SLOT;
			dummy = ohciAllocTd(&ohci->freeItd, &ohci->freeCount.itd);
			if (dummy == NULL)
				goto freeEd;
			goto link;
		case SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK:
			pipe->slot = OHCI_BULK_SLOT;
			((OhciEndpoint *)descriptor)->slot = OHCI_BULK_SLOT;
			break;
		case SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
			ohciScheduleInterrupt(ohci, pipe, endpoint->bInterval, maxPacketSize,
					      device->speed);
			break;
		default:
			ohciFreeEd(ohci, pipe->endpoint.ohci);
			usbdFreePipe(pipe);
			pipe = NULL;
			goto out;
		}
	}
	dummy = ohciAllocTd(&ohci->freeTd, &ohci->freeCount.td);
	if (dummy == NULL)
		goto freeEd;

link:
	/* an empty TD at its tail, then into the list after its static ED */
	pipe->dummy.ohci = dummy;
	ed->tailTd = dummy->pa;
	ed->headTd = dummy->pa;
	slot = &ohci->staticEd[pipe->slot];
	ed->nextEd = slot->nextEd;
	slot->nextEd = usbdVAtoPA(ed);
	goto out;

freeEd:
	ohciFreeEd(ohci, pipe->endpoint.ohci);
	usbdFreePipe(pipe);
	pipe = NULL;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ohci->lock, state);
	return pipe;
}

/* whether the controller still takes the ED's TDs */
static __always_inline int ohciEdStopped(UsbdPipe *pipe)
{
	OhciEd *ed = pipe->endpoint.ohci->hw.ed;

	return ed->tailTd == 0 || (ed->headTd & OHCI_ED_HALTED) || (ed->control & OHCI_ED_SKIP);
}

/* the buffer's last byte, for a TD */
static __always_inline SceUIntPtr ohciBufferEnd(UsbdRequest *request, SceUIntPtr pa)
{
	if (request->buffer == NULL || request->length == 0)
		return 0;
	return pa + request->length - 1;
}

static __always_inline void ohciCleanRequest(UsbdPipe *pipe, UsbdRequest *request)
{
	if (SceSysmemForDriver_F3BBE2E1(request->buffer) == USBD_MEMTYPE_UNCACHED)
		return;
	if (pipe->direction)
		sceKernelDcacheCleanInvalidateRange((void *)USBD_CACHE_BASE(request->buffer),
						     USBD_CACHE_SIZE(request->buffer, request->length));
	else
		sceKernelDcacheCleanRange((void *)USBD_CACHE_BASE(request->buffer),
					   USBD_CACHE_SIZE(request->buffer, request->length));
}

/* an isochronous transfer: up to eight packets in consecutive frames */
static int ohciSubmitIsoch(Ohci *ohci, UsbdPipe *pipe, UsbdRequest *request)
{
	OhciDescriptor *descriptor, *dummy;
	OhciEd *ed = pipe->endpoint.ohci->hw.ed;
	OhciItd *itd;
	SceUIntPtr pa;
	SceUInt16 offset;
	int frame, count, i;

	if (ohciEdStopped(pipe)) {
		if (pipe->waiting)
			ohciUnwaitPipe(ohci, pipe);
		return SCE_USBD_ERROR_FATAL;
	}
	if (request != NULL)
		USBD_LIST_APPEND(pipe->requestHead, pipe->requestTail, request);
	request = pipe->requestHead;
	if (request == NULL) {
		if (pipe->waiting)
			ohciUnwaitPipe(ohci, pipe);
		return SCE_USBD_ERROR_INVALID_PARAM;
	}
	descriptor = ohciAllocTd(&ohci->freeItd, &ohci->freeCount.itd);
	if (descriptor == NULL) {
		if (!pipe->waiting)
			ohciWaitPipe(ohci, pipe, USBD_PIPE_WAIT_ISOCHRONOUS);
		return 0;
	}

	/* fill the empty isochronous TD at the tail; the new one becomes the tail */
	itd = ohciFromPhys(ohci, ed->tailTd);
	dummy = pipe->dummy.ohci;
	USBD_LIST_REMOVE(pipe->requestHead, pipe->requestTail, request);
	if (pipe->requestHead == NULL && pipe->waiting)
		ohciUnwaitPipe(ohci, pipe);
	if ((ed->headTd & OHCI_ED_ADDRESS_MASK) == ed->tailTd)
		frame = ohci->hcca->frameNumber + 2;
	else
		frame = pipe->frame;
	count = request->isochRequest.numPackets;
	request->isochStatus.numPackets = count;
	pipe->frame = frame + count;
	pa = usbdVAtoPA(request->buffer);
	ohciCleanRequest(pipe, request);

	count = request->isochRequest.numPackets;
	itd->bufferPage = pa >> 12 << 12;
	itd->nextTd = descriptor->pa;
	itd->control = (count - 1) << OHCI_ITD_FRAME_COUNT_SHIFT | OHCI_TD_NOT_ACCESSED |
		       (SceUInt16)frame;
	itd->bufferEnd = ohciBufferEnd(request, pa);
	offset = (pa & OHCI_ITD_PAGE_OFFSET) | OHCI_ITD_PACKET_NOT_ACCESSED;
	for (i = 0; i < count; i++) {
		itd->packet[i] = offset;
		offset += request->isochRequest.packets[i].length;
	}
	ohci->itdRequest[ohciItdIndex(ohci, itd)] = request;
	USBD_LIST_APPEND(pipe->descHead.ohci, pipe->descTail.ohci, dummy);
	pipe->dummy.ohci = descriptor;
	ed->tailTd = descriptor->pa;
	return 0;
}

/* setup, data and status stages in three TDs */
static int ohciSubmitControl(Ohci *ohci, UsbdPipe *pipe, UsbdRequest *request)
{
	OhciDescriptor *setupStage, *dataStage, *statusStage, *dummy;
	SceUsbdDeviceRequest *setup;
	OhciEd *ed = pipe->endpoint.ohci->hw.ed;
	OhciTd *td;

	if (ohciEdStopped(pipe)) {
		if (pipe->waiting)
			ohciUnwaitPipe(ohci, pipe);
		return SCE_USBD_ERROR_FATAL;
	}
	if (request != NULL)
		USBD_LIST_APPEND(pipe->requestHead, pipe->requestTail, request);
	request = pipe->requestHead;
	if (request == NULL) {
		if (pipe->waiting)
			ohciUnwaitPipe(ohci, pipe);
		return SCE_USBD_ERROR_INVALID_PARAM;
	}
	if (pipe->busy)
		goto wait;

	setupStage = pipe->dummy.ohci;
	if (request->buffer != NULL && request->length != 0) {
		if (ohci->freeCount.td <= 2)
			goto wait;
		dummy = ohciAllocTd(&ohci->freeTd, &ohci->freeCount.td);
		statusStage = ohciAllocTd(&ohci->freeTd, &ohci->freeCount.td);
		dataStage = ohciAllocTd(&ohci->freeTd, &ohci->freeCount.td);
	} else {
		if (ohci->freeCount.td <= 1)
			goto wait;
		dummy = ohciAllocTd(&ohci->freeTd, &ohci->freeCount.td);
		statusStage = ohciAllocTd(&ohci->freeTd, &ohci->freeCount.td);
		dataStage = NULL;
	}
	USBD_LIST_REMOVE(pipe->requestHead, pipe->requestTail, request);
	if (pipe->requestHead == NULL && pipe->waiting)
		ohciUnwaitPipe(ohci, pipe);
	pipe->busy = 1;

	setup = request->setup;
	if (SceSysmemForDriver_F3BBE2E1(setup) != USBD_MEMTYPE_UNCACHED)
		sceKernelDcacheCleanRange((void *)USBD_CACHE_BASE(setup),
					   USBD_CACHE_SIZE(setup, sizeof(*setup)));
	td = setupStage->hw.td;
	td->control = OHCI_TD_SETUP;
	td->currentBuffer = usbdVAtoPA(setup);
	td->nextTd = dataStage != NULL ? dataStage->pa : statusStage->pa;
	td->bufferEnd = usbdVAtoPA(setup + 1) - 1;
	USBD_LIST_APPEND(pipe->descHead.ohci, pipe->descTail.ohci, setupStage);
	ohci->tdRequest[ohciTdIndex(ohci, td)] = request;

	if (dataStage != NULL) {
		td = dataStage->hw.td;
		td->control = setup->bmRequestType & SCE_USBD_ENDPOINT_DIRECTION_IN ?
			      OHCI_TD_DATA_IN : OHCI_TD_DATA_OUT;
		td->currentBuffer = usbdVAtoPA(request->buffer);
		td->nextTd = statusStage->pa;
		td->bufferEnd = usbdVAtoPA(request->buffer) + request->length - 1;
		USBD_LIST_APPEND(pipe->descHead.ohci, pipe->descTail.ohci, dataStage);
		ohci->tdRequest[ohciTdIndex(ohci, td)] = request;
		if (SceSysmemForDriver_F3BBE2E1(request->buffer) != USBD_MEMTYPE_UNCACHED) {
			if (setup->bmRequestType & SCE_USBD_ENDPOINT_DIRECTION_IN)
				sceKernelDcacheCleanInvalidateRange((void *)USBD_CACHE_BASE(request->buffer),
								     USBD_CACHE_SIZE(request->buffer, request->length));
			else
				sceKernelDcacheCleanRange((void *)USBD_CACHE_BASE(request->buffer),
							   USBD_CACHE_SIZE(request->buffer, request->length));
		}
	}

	td = statusStage->hw.td;
	td->control = setup->bmRequestType & SCE_USBD_ENDPOINT_DIRECTION_IN ?
		      OHCI_TD_STATUS_OUT : OHCI_TD_STATUS_IN;
	td->currentBuffer = 0;
	td->nextTd = dummy->pa;
	td->bufferEnd = 0;
	USBD_LIST_APPEND(pipe->descHead.ohci, pipe->descTail.ohci, statusStage);
	ohci->tdRequest[ohciTdIndex(ohci, td)] = request;

	pipe->dummy.ohci = dummy;
	ed->tailTd = dummy->pa;
	ohci->regs->commandStatus |= OHCI_COMMAND_CONTROL_FILLED;
	return 0;

wait:
	if (!pipe->waiting)
		ohciWaitPipe(ohci, pipe, USBD_PIPE_WAIT_TRANSFER);
	return 0;
}

/* a bulk or interrupt transfer, one TD per two pages (or one, when unaligned) */
static int ohciSubmitBulk(Ohci *ohci, UsbdPipe *pipe, UsbdRequest *request)
{
	OhciDescriptor *descriptor, *dummy;
	OhciEd *ed = pipe->endpoint.ohci->hw.ed;
	OhciTd *td, *prev;
	SceUIntPtr pa, end;
	SceSize chunk;
	int count, i;

	if (ohciEdStopped(pipe)) {
		if (pipe->waiting)
			ohciUnwaitPipe(ohci, pipe);
		return SCE_USBD_ERROR_FATAL;
	}
	if (request != NULL)
		USBD_LIST_APPEND(pipe->requestHead, pipe->requestTail, request);
	request = pipe->requestHead;
	if (request == NULL) {
		if (pipe->waiting)
			ohciUnwaitPipe(ohci, pipe);
		return SCE_USBD_ERROR_INVALID_PARAM;
	}
	if (pipe->busy)
		goto wait;
	if (request->buffer == NULL || request->length == 0) {
		chunk = 0;
		count = 1;
	} else {
		chunk = ((SceUIntPtr)request->buffer & 0xfff) == 0 ? 0x2000 : 0x1000;
		count = (request->length - 1 + chunk) / chunk;
	}
	if (count > ohci->freeCount.td)
		goto wait;

	USBD_LIST_REMOVE(pipe->requestHead, pipe->requestTail, request);
	if (pipe->requestHead == NULL && pipe->waiting)
		ohciUnwaitPipe(ohci, pipe);
	dummy = ohciAllocTd(&ohci->freeTd, &ohci->freeCount.td);
	pa = request->buffer != NULL ? usbdVAtoPA(request->buffer) : 0;
	ohciCleanRequest(pipe, request);

	/* the empty TD at the tail first, then new ones */
	end = ohciBufferEnd(request, pa);
	descriptor = NULL;
	prev = NULL;
	for (i = 0; i < count; i++) {
		if (descriptor == NULL)
			descriptor = pipe->dummy.ohci;
		else
			descriptor = ohciAllocTd(&ohci->freeTd, &ohci->freeCount.td);
		td = descriptor->hw.td;
		td->control = OHCI_TD_BULK;
		td->currentBuffer = request->length != 0 ? pa : 0;
		if (i == count - 1) {
			td->nextTd = dummy->pa;
			td->bufferEnd = end;
			td->control = OHCI_TD_BULK_LAST;
		} else {
			td->bufferEnd = pa + chunk - 1;
		}
		if (prev != NULL)
			prev->nextTd = descriptor->pa;
		pa += chunk;
		ohci->tdRequest[ohciTdIndex(ohci, td)] = request;
		USBD_LIST_APPEND(pipe->descHead.ohci, pipe->descTail.ohci, descriptor);
		prev = td;
	}
	pipe->dummy.ohci = dummy;
	ed->tailTd = dummy->pa;
	return 0;

wait:
	if (!pipe->waiting)
		ohciWaitPipe(ohci, pipe, USBD_PIPE_WAIT_TRANSFER);
	return 0;
}

static __always_inline void ohciInvalidateRequest(UsbdRequest *request)
{
	if (SceSysmemForDriver_F3BBE2E1(request->buffer) != USBD_MEMTYPE_UNCACHED &&
	    request->pipe->direction == SCE_USBD_ENDPOINT_DIRECTION_IN)
		sceKernelDcacheInvalidateRange((void *)USBD_CACHE_BASE(request->buffer),
						USBD_CACHE_SIZE(request->buffer, request->length));
}

/* a request whose TDs a halt threw away, onto the list of those to complete */
static __always_inline void ohciAbortRequest(UsbdRequest **head, UsbdRequest **tail,
					     UsbdRequest *request)
{
	UsbdRequest *entry;

	for (entry = *head; entry != NULL; entry = entry->next) {
		if (entry == request)
			return;
	}
	request->result = -1;
	USBD_LIST_APPEND(*head, *tail, request);
}

/* an isochronous TD the controller is done with */
static __always_inline void ohciCompleteItd(Ohci *ohci, OhciItd *itd)
{
	OhciDescriptor *descriptor;
	UsbdRequest *request, *last, *head, *tail;
	UsbdPipe *pipe;
	OhciEd *ed;
	SceUInt16 packet;
	int index, condition, i;

	index = ohciItdIndex(ohci, itd);
	request = ohci->itdRequest[index];
	ohci->itdRequest[index] = NULL;
	if (request == NULL)
		return;

	condition = itd->control >> OHCI_TD_CC_SHIFT;
	for (i = 0; i < 8; i++) {
		packet = itd->packet[i];
		request->isochStatus.packets[i].status = packet & OHCI_ITD_PACKET_CC;
		request->isochStatus.packets[i].length =
			packet & OHCI_ITD_PACKET_CC ? 0 : packet & OHCI_ITD_PACKET_SIZE;
	}
	pipe = request->pipe;
	for (descriptor = pipe->descHead.ohci; descriptor != NULL; descriptor = descriptor->next) {
		if (descriptor->hw.itd == itd) {
			USBD_LIST_REMOVE(pipe->descHead.ohci, pipe->descTail.ohci, descriptor);
			ohciFreeDescriptor(&ohci->freeItd, &ohci->freeCount.itd, descriptor);
			break;
		}
	}
	if (condition != 0 && request->result == 0)
		request->result = SCE_USBD_ERROR_80240009;

	/* a halted ED: throw away its other TDs and end their requests too */
	ed = request->pipe->endpoint.ohci->hw.ed;
	head = tail = request;
	request->next = NULL;
	request->prev = NULL;
	last = request;
	if (ed->headTd & OHCI_ED_HALTED) {
		pipe = request->pipe;
		while ((descriptor = pipe->descHead.ohci) != NULL) {
			USBD_LIST_REMOVE(pipe->descHead.ohci, pipe->descTail.ohci, descriptor);
			index = ohciItdIndex(ohci, descriptor->hw.itd);
			last = ohci->itdRequest[index];
			ohci->itdRequest[index] = NULL;
			ohciFreeDescriptor(&ohci->freeItd, &ohci->freeCount.itd, descriptor);
			ohciAbortRequest(&head, &tail, last);
			pipe = last->pipe;
		}
		ed->headTd = ed->tailTd;
	}
	ohciInvalidateRequest(last);
	while ((request = head) != NULL) {
		USBD_LIST_REMOVE(head, tail, request);
		request->complete(request);
	}

	for (pipe = ohci->waitHead; pipe != NULL; pipe = pipe->waitNext) {
		if (pipe->waiting == USBD_PIPE_WAIT_ISOCHRONOUS) {
			ohciSubmitIsoch(ohci, pipe, NULL);
			break;
		}
	}
}

/* a TD the controller is done with; the last TD of a request completes it */
static __always_inline void ohciCompleteTd(Ohci *ohci, OhciTd *td)
{
	OhciDescriptor *descriptor;
	UsbdRequest *request, *head, *tail;
	SceUIntPtr buffer;
	UsbdPipe *pipe;
	OhciEd *ed;
	int index, condition;

	index = ohciTdIndex(ohci, td);
	request = ohci->tdRequest[index];
	ohci->tdRequest[index] = NULL;
	if (request == NULL)
		return;

	buffer = td->currentBuffer;
	condition = td->control >> OHCI_TD_CC_SHIFT;
	if (td->bufferEnd != 0 && (td->control & OHCI_TD_DP) != OHCI_TD_DP_SETUP) {
		if (buffer != 0)
			request->count = buffer - usbdVAtoPA(request->buffer);
		else
			request->count = request->length;
	}
	head = tail = NULL;
	if (condition != 0) {
		if (request->result == 0)
			request->result = condition == OHCI_CC_STALL ? SCE_USBD_ERROR_80240007 :
								       SCE_USBD_ERROR_80240009;
		USBD_LIST_APPEND(head, tail, request);
	} else if ((td->control & OHCI_TD_DI) != OHCI_TD_DI_NONE) {
		USBD_LIST_APPEND(head, tail, request);
	}

	pipe = request->pipe;
	for (descriptor = pipe->descHead.ohci; descriptor != NULL; descriptor = descriptor->next) {
		if (descriptor->hw.td == td) {
			USBD_LIST_REMOVE(pipe->descHead.ohci, pipe->descTail.ohci, descriptor);
			ohciFreeDescriptor(&ohci->freeTd, &ohci->freeCount.td, descriptor);
			break;
		}
	}

	/* an error halts the ED: throw away its other TDs and end their requests too */
	if (condition != 0) {
		ed = request->pipe->endpoint.ohci->hw.ed;
		if (ed->headTd & OHCI_ED_HALTED) {
			pipe = request->pipe;
			while ((descriptor = pipe->descHead.ohci) != NULL) {
				USBD_LIST_REMOVE(pipe->descHead.ohci, pipe->descTail.ohci, descriptor);
				index = ohciTdIndex(ohci, descriptor->hw.td);
				request = ohci->tdRequest[index];
				ohci->tdRequest[index] = NULL;
				ohciFreeDescriptor(&ohci->freeTd, &ohci->freeCount.td, descriptor);
				ohciAbortRequest(&head, &tail, request);
				pipe = request->pipe;
			}
		}
		ed->headTd = ed->tailTd;
	}
	ohciInvalidateRequest(request);
	request->pipe->busy = 0;
	while ((request = head) != NULL) {
		USBD_LIST_REMOVE(head, tail, request);
		if (request->complete != NULL)
			request->complete(request);
	}

	/* the pipes that waited for TDs */
	for (pipe = ohci->waitHead; pipe != NULL; pipe = pipe->waitNext) {
		if (pipe->waiting != USBD_PIPE_WAIT_TRANSFER)
			continue;
		switch (pipe->type) {
		case SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL:
			ohciSubmitControl(ohci, pipe, NULL);
			break;
		case SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK:
			if (ohciSubmitBulk(ohci, pipe, NULL) >= 0)
				ohci->regs->commandStatus |= OHCI_COMMAND_BULK_FILLED;
			break;
		case SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
			ohciSubmitBulk(ohci, pipe, NULL);
			break;
		}
	}
}

/* the done queue, oldest first */
static __always_inline void ohciProcessDone(Ohci *ohci)
{
	OhciTd *td, *next, *prev;

	td = ohciFromPhys(ohci, ohci->hcca->doneHead);
	ohci->hcca->doneHead = 0;
	if (td == NULL)
		return;

	/* the controller adds to the head: reverse it */
	prev = NULL;
	for (;;) {
		next = ohciFromPhys(ohci, td->nextTd);
		td->nextTd = prev != NULL ? usbdVAtoPA(prev) : 0;
		if (next == NULL)
			break;
		prev = td;
		td = next;
	}
	for (; td != NULL; td = next) {
		next = ohciFromPhys(ohci, td->nextTd);
		if ((SceUInt8 *)td >= ohci->tdArea.base && (SceUInt8 *)td < ohci->tdArea.end)
			ohciCompleteTd(ohci, td);
		else
			ohciCompleteItd(ohci, (OhciItd *)td);
	}
}

static __always_inline void ohciRoothubStatusChange(Ohci *ohci)
{
	volatile OhciRegs *regs = ohci->regs;
	SceUInt32 status;

	regs->interruptStatus = OHCI_INT_ROOT_HUB_CHANGE;
	status = regs->rhPortStatus;
	regs->rhPortStatus = OHCI_PORT_CHANGES;
	if (!(status & OHCI_PORT_CONNECT)) {
		if (ohci->portState >= OHCI_PORT_ENABLED)
			usbdNotifyDetach(USBD_ID(ohci->bus, 0), 0);
		ohci->portState = OHCI_PORT_POWERED;
		return;
	}
	if (ohci->portState == OHCI_PORT_POWERED) {
		ohci->portState = OHCI_PORT_CONNECTED;
		SceThreadmgrForDriver_C58DF384("SceUsbdAlarm0", 200000, ohciConnectDone, ohci);
		return;
	}
	if ((status & OHCI_PORT_RESET_CHANGE) && !(status & OHCI_PORT_RESET) &&
	    ohci->portState == OHCI_PORT_RESETTING && (regs->rhPortStatus & OHCI_PORT_ENABLE)) {
		ohci->portState = OHCI_PORT_ENABLED;
		usbdNotifyAttach(USBD_ID(ohci->bus, 0), 0,
				 regs->rhPortStatus & OHCI_PORT_LOW_SPEED ? SCE_USBD_DEVICE_SPEED_LS :
									    SCE_USBD_DEVICE_SPEED_FS);
	}
	if ((status & OHCI_PORT_ENABLE_CHANGE) && !(status & OHCI_PORT_ENABLE))
		SCE_DBG_PRINTF(15, 1261, "### Port Diabled ###\n");
	if ((status & (OHCI_PORT_SUSPEND_CHANGE | OHCI_PORT_SUSPEND)) ==
	    (OHCI_PORT_SUSPEND_CHANGE | OHCI_PORT_SUSPEND))
		SCE_DBG_PRINTF(15, 1266, "### Port Suspended Status ###\n");
	if ((status & (OHCI_PORT_OVER_CURRENT_CHANGE | OHCI_PORT_OVER_CURRENT)) ==
	    (OHCI_PORT_OVER_CURRENT_CHANGE | OHCI_PORT_OVER_CURRENT))
		SCE_DBG_PRINTF(15, 1271, "Port Over Current Indicator\n");
}

static int ohciInterruptHandler(int intrCode, void *arg)
{
	Ohci *ohci = arg;
	SceUInt32 status;
	int state;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&ohci->lock);
	status = ohci->regs->interruptStatus & ohci->regs->interruptEnable;
	if (status & OHCI_INT_UNRECOVERABLE_ERROR) {
		SCE_DBG_PRINTF(15, 1288, "Unrecoverable Error\n");
		ohci->regs->interruptStatus = OHCI_INT_UNRECOVERABLE_ERROR;
	}
	if (status & OHCI_INT_SCHEDULING_OVERRUN) {
		SCE_DBG_PRINTF(15, 1292, "Scheduing Overrun\n");
		ohci->regs->interruptStatus = OHCI_INT_SCHEDULING_OVERRUN;
	}
	if (status & OHCI_INT_WRITEBACK_DONE) {
		ohci->regs->interruptStatus = OHCI_INT_WRITEBACK_DONE;
		ohciProcessDone(ohci);
	}
	if (status & OHCI_INT_START_OF_FRAME) {
		ohci->interruptCount[OHCI_INT_SOF_BIT]++;
		ohci->regs->interruptStatus = OHCI_INT_START_OF_FRAME;
	}
	if (status & OHCI_INT_RESUME_DETECTED)
		ohci->regs->interruptStatus = OHCI_INT_RESUME_DETECTED;
	if (status & OHCI_INT_FRAME_OVERFLOW)
		ohci->regs->interruptStatus = OHCI_INT_FRAME_OVERFLOW;
	if (status & OHCI_INT_ROOT_HUB_CHANGE)
		ohciRoothubStatusChange(ohci);
	if (status & OHCI_INT_OWNERSHIP_CHANGE)
		ohci->regs->interruptStatus = OHCI_INT_OWNERSHIP_CHANGE;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ohci->lock, state);
	return -1;
}

static int ohciSubmitRequest(UsbdPipe *pipe, UsbdRequest *request)
{
	Ohci *ohci;
	int result, state;

	if (pipe == NULL || pipe->device == NULL || pipe->device->bus == NULL || request == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	ohci = s_ohci[pipe->device->bus->index];
	if (ohci == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&ohci->lock);
	if (pipe->device == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
		goto out;
	}
	switch (pipe->type) {
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL:
		result = ohciSubmitControl(ohci, pipe, request);
		break;
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS:
		if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_FS)
			result = ohciSubmitIsoch(ohci, pipe, request);
		else
			result = SCE_USBD_ERROR_INVALID_PARAM;
		break;
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK:
		if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_FS) {
			result = ohciSubmitBulk(ohci, pipe, request);
			if (result >= 0)
				ohci->regs->commandStatus |= OHCI_COMMAND_BULK_FILLED;
		} else {
			result = SCE_USBD_ERROR_INVALID_PARAM;
		}
		break;
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
		result = ohciSubmitBulk(ohci, pipe, request);
		break;
	default:
		result = SCE_USBD_ERROR_FATAL;
		break;
	}
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ohci->lock, state);
	return result;
}

/* HCCA slot n of the 32 serves the leaf whose index has n's bits reversed */
static __always_inline unsigned int ohciReverseFrame(unsigned int leaf)
{
	return ((leaf & 0x01) << 4) + ((leaf & 0x02) << 2) + (leaf & 0x04) +
	       ((leaf & 0x08) >> 2) + ((leaf & 0x10) >> 4);
}

int ohciStart(UsbdBus *bus)
{
	volatile OhciRegs *regs;
	Ohci *ohci = s_ohci[bus->index];
	OhciEd *staticEd;
	int i, leaf, timeout, result;

	sceKernelDelayThread(100000);
	result = ohciReset(ohci);
	if (result < 0)
		return result;
	result = sceKernelEnableIntr(ohci->intrCode);
	if (result < 0)
		return result;

	/* the interrupt tree: each static ED links to its parent, the leaves are the frames */
	staticEd = ohci->staticEd;
	for (i = 0; i < OHCI_TREE_SIZE; i++) {
		staticEd[i].control = OHCI_ED_SKIP;
		if (i == 0) {
			staticEd[i].nextEd = 0;
		} else {
			staticEd[i].nextEd = usbdVAtoPA(&staticEd[(i - 1) / 2]);
			leaf = i - OHCI_TREE_LEAF;
			if (leaf >= 0)
				ohci->hcca->interruptTable[ohciReverseFrame(leaf)] =
					usbdVAtoPA(&staticEd[i]);
		}
	}
	ohci->regs->hcca = usbdVAtoPA(ohci->hcca);
	staticEd[OHCI_CONTROL_SLOT].control = OHCI_ED_SKIP;
	ohci->regs->controlHeadEd = usbdVAtoPA(&staticEd[OHCI_CONTROL_SLOT]);
	staticEd[OHCI_BULK_SLOT].control = OHCI_ED_SKIP;
	ohci->regs->bulkHeadEd = usbdVAtoPA(&staticEd[OHCI_BULK_SLOT]);
	staticEd[OHCI_ISOCHRONOUS_SLOT].control = OHCI_ED_SKIP;
	staticEd[0].nextEd = usbdVAtoPA(&staticEd[OHCI_ISOCHRONOUS_SLOT]);

	regs = ohci->regs;
	regs->rhDescriptorA |= OHCI_RH_PER_PORT_OVER_CURRENT | OHCI_RH_PER_PORT_POWER;
	regs->rhDescriptorB.portPowerControlMask = 0xffff;	/* every port powered on its own */
	ohci->portCount = regs->rhDescriptorA & OHCI_RH_PORT_COUNT;
	regs->fmInterval = OHCI_FM_INTERVAL;
	regs->periodicStart = OHCI_PERIODIC_START;
	ohci->hcca->frameNumber = 0;

	/* run, and wait for the first frame */
	ohci->interruptCount[OHCI_INT_SOF_BIT] = 0;
	regs->interruptEnable = OHCI_INT_MASTER | OHCI_INT_START_OF_FRAME;
	regs->control |= OHCI_CONTROL_OPERATIONAL;
	for (timeout = OHCI_START_TIMEOUT; ohci->interruptCount[OHCI_INT_SOF_BIT] == 0; ) {
		sceKernelDelayThread(1000);
		if (--timeout == 0)
			return SCE_USBD_ERROR_FATAL;
	}
	regs = ohci->regs;
	regs->interruptDisable = ~0;
	regs->interruptEnable = OHCI_INT_MASTER | OHCI_INT_ROOT_HUB_CHANGE |
				OHCI_INT_UNRECOVERABLE_ERROR | OHCI_INT_WRITEBACK_DONE |
				OHCI_INT_SCHEDULING_OVERRUN;
	regs->control |= OHCI_CONTROL_BULK | OHCI_CONTROL_CONTROL | OHCI_CONTROL_ISOCHRONOUS |
			 OHCI_CONTROL_PERIODIC | OHCI_CONTROL_CBSR_4_1;
	return 0;
}

static int ohciResume(UsbdBus *bus)
{
	int result;

	result = ohciStart(bus);
	if (result < 0)
		return result;
	return 0;
}

int ohciStop(UsbdBus *bus)
{
	Ohci *ohci = s_ohci[bus->index];
	SceUInt8 port;
	int result;

	result = sceKernelDisableIntr(ohci->intrCode);
	if (result < 0)
		return result;
	for (port = 0; port < ohci->portCount; port++)
		ohciPowerOffPort(bus, port);
	return ohciReset(ohci);
}

static __always_inline void ohciInitDescriptors(OhciDescriptorList *list, OhciDescriptor *records,
						SceUInt8 *area, int size, const SceUInt8 *count)
{
	int i;

	for (i = 0; i < *count; i++) {
		records[i].hw.td = (OhciTd *)(area + i * size);
		records[i].pa = usbdVAtoPA(records[i].hw.td);
		USBD_LIST_APPEND(list->head, list->tail, &records[i]);
	}
}

int ohciInit(UsbdBus *bus)
{
	SceKernelAllocMemBlockKernelOpt opt;
	OhciEndpoint *edRecords;
	UsbdHcd *hcd;
	const char *name;
	SceUInt8 *base;
	void *records;
	Ohci *ohci;
	SceUIntPtr paddr;
	SceSize size;
	int i, index, intrCode, result;

	index = bus->index;
	ohci = usbdAlloc(sizeof(*ohci));
	if (ohci == NULL)
		return SCE_USBD_ERROR_NO_MEMORY;
	memset(ohci, 0, sizeof(*ohci));
	ohci->bus = index;
	ohci->hcd = &bus->ohci;
	ohci->count.ed = ohci->freeCount.ed = s_ohciDescriptorCount[index].ed;
	ohci->count.td = ohci->freeCount.td = s_ohciDescriptorCount[index].td;
	ohci->count.itd = ohci->freeCount.itd = s_ohciDescriptorCount[index].itd;
	s_ohci[index] = ohci;
	ohci->lock = 0;

	switch (index) {
	case 0:
		paddr = USB_HOST0_REG_BASE;
		break;
	case 1:
		paddr = USB_HOST1_REG_BASE;
		break;
	case 2:
		paddr = USB_HOST2_REG_BASE;
		break;
	default:
		return SCE_USBD_ERROR_FATAL;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = paddr;
	result = sceKernelAllocMemBlockForKernel("SceUsbdOhci", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
					 USB_REG_SIZE, &opt);
	if (result < 0)
		goto fail;
	ohci->registerBlockId = result;
	result = sceKernelGetMemBlockBase(ohci->registerBlockId, (void **)&base);
	if (result < 0)
		goto freeRegisters;
	ohci->regs = (volatile OhciRegs *)(base + OHCI_REGS_OFFSET);

	/* the HCCA, the pools and the static EDs share 16 KiB */
	size = sizeof(OhciHcca) + ohci->count.itd * OHCI_ITD_SIZE + ohci->count.td * OHCI_TD_SIZE +
	       ohci->count.ed * OHCI_ED_SIZE + OHCI_STATIC_ED_COUNT * OHCI_ED_SIZE;
	if (size > OHCI_DESCRIPTOR_BLOCK_SIZE) {
		result = SCE_USBD_ERROR_FATAL;
		goto fail;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	opt.alignment = 0x1000;
	result = sceKernelAllocMemBlockForKernel("SceUsbdOhci", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_DEVICE_RW,
					 OHCI_DESCRIPTOR_BLOCK_SIZE, &opt);
	if (result < 0)
		goto freeRegisters;
	ohci->descriptorBlockId = result;
	result = sceKernelGetMemBlockBase(ohci->descriptorBlockId, (void **)&base);
	if (result < 0)
		goto freeBlocks;
	memset(base, 0, OHCI_DESCRIPTOR_BLOCK_SIZE);

	ohci->hcca = (OhciHcca *)base;
	ohci->itdArea.base = base + sizeof(OhciHcca);
	ohci->itdArea.end = ohci->itdArea.base + ohci->count.itd * OHCI_ITD_SIZE;
	ohci->tdArea.base = ohci->itdArea.end;
	ohci->tdArea.end = ohci->tdArea.base + ohci->count.td * OHCI_TD_SIZE;
	ohci->edArea.base = ohci->tdArea.end;
	ohci->edArea.end = ohci->edArea.base + ohci->count.ed * OHCI_ED_SIZE;
	ohci->staticEd = (OhciEd *)ohci->edArea.end;
	ohci->staticArea.base = ohci->edArea.end;
	ohci->staticArea.end = ohci->staticArea.base + OHCI_STATIC_ED_COUNT * OHCI_ED_SIZE;
	ohci->edRange.base = usbdVAtoPA(ohci->edArea.base);
	ohci->edRange.end = usbdVAtoPA(ohci->edArea.end);
	ohci->itdRange.base = usbdVAtoPA(ohci->itdArea.base);
	ohci->itdRange.end = usbdVAtoPA(ohci->itdArea.end);
	ohci->tdRange.base = usbdVAtoPA(ohci->tdArea.base);
	ohci->tdRange.end = usbdVAtoPA(ohci->tdArea.end);
	ohci->staticRange.base = usbdVAtoPA(ohci->staticArea.base);
	ohci->staticRange.end = usbdVAtoPA(ohci->staticArea.end);

	/* the records and the request maps */
	size = ohci->count.ed * sizeof(OhciEndpoint) +
	       (ohci->count.td + ohci->count.itd) * sizeof(OhciDescriptor) +
	       (ohci->count.itd + ohci->count.td) * sizeof(UsbdRequest *);
	records = usbdAlloc(size);
	if (records == NULL) {
		result = SCE_USBD_ERROR_NO_MEMORY;
		goto freeBlocks;
	}
	memset(records, 0, size);
	edRecords = records;
	ohci->edRecords = edRecords;
	ohci->freeEd.head = NULL;
	ohci->freeEd.tail = NULL;
	ohci->freeTd.head = NULL;
	ohci->freeTd.tail = NULL;
	ohci->edRecordsEnd = &edRecords[ohci->count.ed];
	ohci->freeItd.head = NULL;
	ohci->freeItd.tail = NULL;
	ohci->tdRecords.base = (OhciDescriptor *)ohci->edRecordsEnd;
	ohci->tdRecords.end = ohci->tdRecords.base + ohci->count.td;
	ohci->itdRecords.base = ohci->tdRecords.end;
	ohci->itdRecords.end = ohci->itdRecords.base + ohci->count.itd;
	ohci->itdRequest = (UsbdRequest **)ohci->itdRecords.end;
	ohci->tdRequest = ohci->itdRequest + ohci->count.itd;

	for (i = 0; i < ohci->count.ed; i++) {
		edRecords[i].descriptor.hw.ed = (OhciEd *)(ohci->edArea.base + i * OHCI_ED_SIZE);
		edRecords[i].descriptor.pa = usbdVAtoPA(edRecords[i].descriptor.hw.ed);
		USBD_LIST_APPEND(ohci->freeEd.head, ohci->freeEd.tail, &edRecords[i].descriptor);
	}
	ohciInitDescriptors(&ohci->freeTd, ohci->tdRecords.base, ohci->tdArea.base, OHCI_TD_SIZE,
			    &ohci->count.td);
	ohciInitDescriptors(&ohci->freeItd, ohci->itdRecords.base, ohci->itdArea.base, OHCI_ITD_SIZE,
			    &ohci->count.itd);
	ohci->waitTail = NULL;
	ohci->waitHead = NULL;
	ohci->portState = OHCI_PORT_OFF;
	memset(ohci->interruptCount, 0, sizeof(ohci->interruptCount));

	switch (ohci->bus) {
	case 0:
		name = "SceUsbOhci0";
		intrCode = 145;
		break;
	case 1:
		name = "SceUsbOhci1";
		intrCode = 147;
		break;
	case 2:
		name = "SceUsbOhci2";
		intrCode = 149;
		break;
	default:
		return SCE_USBD_ERROR_FATAL;
	}
	result = sceKernelRegisterIntrHandler(intrCode, name, 0, ohciInterruptHandler, ohci,
					       160, 15, NULL);
	if (result < 0) {
		usbdFree(records);
		goto freeBlocks;
	}
	ohci->intrCode = intrCode;
	hcd = &bus->ohci;
	hcd->getPortCount = ohciGetPortCount;
	hcd->allocPipe = ohciAllocPipe;
	hcd->closePipe = ohciClosePipe;
	hcd->submitRequest = ohciSubmitRequest;
	hcd->setAddress = ohciSetAddress;
	hcd->setMaxPacketSize = ohciSetMaxPacketSize;
	hcd->powerOnPort = ohciPowerOnPort;
	hcd->powerOffPort = ohciPowerOffPort;
	hcd->resetPort = ohciResetPort;
	hcd->getSpeed = ohciGetSpeed;
	hcd->getFrameNumber = ohciGetFrameNumber;
	hcd->suspend = ohciSuspend;
	hcd->resume = ohciResume;
	return 0;

freeBlocks:
	sceKernelFreeMemBlock(ohci->descriptorBlockId);
freeRegisters:
	sceKernelFreeMemBlock(ohci->registerBlockId);
fail:
	if (ohci->bus < USBD_BUS_COUNT) {
		s_ohci[ohci->bus] = NULL;
		usbdFree(ohci);
	}
	return result;
}

int ohciTerm(UsbdBus *bus)
{
	Ohci *ohci = s_ohci[bus->index];
	int result;

	result = ohciStop(bus);
	if (result < 0)
		return result;
	result = sceKernelDisableIntr(ohci->intrCode);
	if (result < 0)
		return result;
	result = sceKernelReleaseIntrHandler(ohci->intrCode);
	if (result < 0)
		return result;
	result = sceKernelFreeMemBlock(ohci->descriptorBlockId);
	if (result < 0)
		return result;
	result = sceKernelFreeMemBlock(ohci->registerBlockId);
	if (result < 0)
		return result;
	if (ohci->bus < USBD_BUS_COUNT) {
		s_ohci[ohci->bus] = NULL;
		usbdFree(ohci);
	}
	return 0;
}
