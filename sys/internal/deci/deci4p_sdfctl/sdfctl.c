/*
 * SceDeci4pSDfCtl: remote power control over the debug UART. The manager keeps
 * two protocol descriptors (SDCMP, STSMP) and one host interface; a protocol
 * asks to send (sceSDfCtlExReqSend), the interface drives the transfer and reports
 * back through sceSDfCtlIf2MgrEvent, which hands the data to the protocol.
 */

#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/modulemgr.h>
#include <sysmem/debug.h>
#include <sysmem/suspend.h>
#include <sysmem/sysroot.h>
#include "sdfctl.h"
#include <deci4p_sdfmgr/deci4p_memory.h>

#define SDFCTL_PDS		2

static SceKernelSpinlock s_lock;		/* sdfctlReqSend */

static struct {
	SceUInt32 reserved;
	SceUInt32 status;			/* SDFCTL_STATUS_* */
	int nextPd;				/* the next to send */
	SDfCtlPd pd[SDFCTL_PDS];
	SDfCtlIf *ifp;
} s_mgr;

static SceKernelSpinlock s_statusLock;

/* the 24-bit packet size */
#define HDR_SIZE(_hdr)		(*(const SceUInt16 *)(_hdr)->size | (_hdr)->size[2] << 16)

#if FW_DEBUG
__attribute__((noinline))
#else
static __attribute__((always_inline)) inline
#endif
SDfCtlPd *_getPdCB(SceUInt32 id)
{
	const void *lr = __builtin_return_address(0);

	if (id < SDFCTL_PDS)
		return &s_mgr.pd[id];
	SDFCTL_PRINTF(1085, "invalid argument (id: %d)\n", id);
	SCE_DBG_ASSERT_CHECK_LR(0, 1086, lr);
	return NULL;
}

/* name unknown: report every registered protocol to the host */
static void sdfctlSendProtocolStatus(void)
{
	SDfCtlPd *pd;
	SceUInt32 status;
	int i;

	for (i = 0; i != SDFCTL_PDS; i++) {
		pd = _getPdCB(i);
		if (!(pd->flags & SDFCTL_PD_REGISTERED))
			continue;
		status = (pd->flags & SDFCTL_PD_STARTED) ? 3 : 1;
		if (pd->desc.flags & 2)
			status |= 4;
		sdcmpSendStatusProtocol(status, &pd->desc);
		if (pd->desc.flags & 2)
			sdcmpSendStatusProtocolAlias(&pd->desc);
	}
}

/*
 * name unknown: start sending for the next protocol (round robin) that asked to;
 * the original keeps scanning after starting one
 */
static __attribute__((noinline)) void sdfctlSendNext(void)
{
	SDfCtlIf *ifp = s_mgr.ifp;
	SDfCtlPd *pd;
	int i;

	if (ifp->sendPd != NULL)
		return;
	pd = &s_mgr.pd[s_mgr.nextPd];
	for (i = 1; ; ) {
		if (pd->flags & SDFCTL_PD_SEND_REQ) {
			SceUInt32 flags = ifp->flags;
			SDfCtlIf *currentIf = s_mgr.ifp;

			pd->flags &= ~SDFCTL_PD_SEND_REQ;
			ifp->sendPd = pd;
			ifp->sendSize = 0;
			ifp->sendDone = 0;
			ifp->flags = flags | SDFCTL_IF_SEND_STARTED;
			currentIf->ops.sendStart(ifp);
			s_mgr.nextPd = i;
			if (i == SDFCTL_PDS)
				s_mgr.nextPd = 0;
		}
		pd++;
		i++;
		if (pd >= &s_mgr.pd[SDFCTL_PDS])
			pd = &s_mgr.pd[0];
		if (i == SDFCTL_PDS + 1)
			return;
	}
}

/* the protocol of sock, if registered */
static inline SDfCtlPd *sdfctlFindSock(SceUID sock)
{
	if ((s_mgr.pd[0].flags & SDFCTL_PD_REGISTERED) && sock == s_mgr.pd[0].sock)
		return &s_mgr.pd[0];
	if ((s_mgr.pd[1].flags & SDFCTL_PD_REGISTERED) && sock == s_mgr.pd[1].sock)
		return &s_mgr.pd[1];
	return NULL;
}

/* (SDFCTL_EVENT_RECV handlers) the received packet, the header first */
int sceSDfCtlExRecv(SceUID sock, void *buf, SceSize size)
{
	SDfCtlIf *ifp = s_mgr.ifp;
	SDfCtlIfXfer recv;
	SceSize recvDone;
	int i;

	if (buf == NULL) {
		SDFCTL_PRINTF(396, "illegal buffer\n");
		return SDFCTL_ERROR_INVALID_BUFFER;
	}
	if (sdfctlFindSock(sock) == NULL) {
		SDFCTL_PRINTF(403, "invalid argument (sock: 0x%08X)\n", sock);
		return SDFCTL_ERROR_INVALID_SOCK;
	}
	ifp = s_mgr.ifp;
	if (ifp == NULL) {
		SDFCTL_PRINTF(410, "no host if\n");
		return SDFCTL_ERROR_NO_HOST_IF;
	}
	if (!(ifp->recvFlags & SDFCTL_IF_IN_HANDLER))
		return SDFCTL_ERROR_BUSY;
	if (ifp->recvSize != 0)
		return ifp->ops.recv(ifp, buf, size);
	if (size < SDFCTL_HDR_SIZE)
		return SDFCTL_ERROR_INVALID_SIZE;
	for (i = 0; i != SDFCTL_HDR_SIZE; i++)
		((SceUInt8 *)buf)[i] = ((SceUInt8 *)&ifp->recvHdr)[i];
	recv = ifp->ops.recv;
	recvDone = ifp->recvDone + SDFCTL_HDR_SIZE;
	ifp->recvSize = *(const SceUInt16 *)ifp->recvHdr.size |
		ifp->recvHdr.size[2] << 16;
	ifp->recvDone = recvDone;
	return recv(ifp, (char *)buf + SDFCTL_HDR_SIZE, size - SDFCTL_HDR_SIZE) + SDFCTL_HDR_SIZE;
}

/* ask to send: SDFCTL_EVENT_SEND follows when the interface is free */
int sceSDfCtlExReqSend(SceUID sock)
{
	SDfCtlPd *pd;
	SceUInt32 flags;

	if ((s_mgr.pd[0].flags & SDFCTL_PD_REGISTERED) && sock == s_mgr.pd[0].sock)
		pd = &s_mgr.pd[0];
	else if ((s_mgr.pd[1].flags & SDFCTL_PD_REGISTERED) && sock == s_mgr.pd[1].sock)
		pd = &s_mgr.pd[1];
	else {
		SDFCTL_PRINTF(463, "invalid argument (sock: 0x%08X)\n", sock);
		return SDFCTL_ERROR_INVALID_SOCK;
	}
	if (s_mgr.ifp == NULL) {
		SDFCTL_PRINTF(470, "no host if\n");
		return SDFCTL_ERROR_NO_HOST_IF;
	}
	flags = pd->flags;
	if (flags & SDFCTL_PD_SEND_REQ)
		return SDFCTL_ERROR_BUSY;
	if (pd->desc.protocol != SDFCTL_PROTOCOL_MGR &&
	    (!(s_mgr.status & SDFCTL_STATUS_CONNECTED) || !(flags & SDFCTL_PD_STARTED)))
		return SDFCTL_ERROR_NO_ROUTE;
	pd->flags = flags | SDFCTL_PD_SEND_REQ;
	sdfctlSendNext();
	return 0;
}

/* name unknown: sceSDfCtlExReqSend, then kick the interface if it started sending */
int sdfctlReqSend(SceUID sock)
{
	int intr, ret;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	ret = sceSDfCtlExReqSend(sock);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	if (ret < 0)
		return ret;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	if (s_mgr.ifp != NULL && (s_mgr.ifp->flags & SDFCTL_IF_SEND_STARTED))
		s_mgr.ifp->ops.poll(s_mgr.ifp);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, intr);
	return 0;
}

/* (SDFCTL_EVENT_SEND handlers) a packet, or the next part of one */
int sceSDfCtlExSend(SceUID sock, void *buf, SceSize size)
{
	SDfCtlIf *ifp = s_mgr.ifp;
	SceDeci4pHeader *hdr = buf;
	SDfCtlPd *pd;
	SceUInt32 protocol, routeProtocol;

	if (buf == NULL) {
		SDFCTL_PRINTF(538, "invalid argument\n");
		return SDFCTL_ERROR_INVALID_BUFFER;
	}
	if ((s_mgr.pd[0].flags & SDFCTL_PD_REGISTERED) && sock == s_mgr.pd[0].sock)
		pd = &s_mgr.pd[0];
	else if ((s_mgr.pd[1].flags & SDFCTL_PD_REGISTERED) && sock == s_mgr.pd[1].sock)
		pd = &s_mgr.pd[1];
	else {
		SDFCTL_PRINTF(545, "invalid argument (sock: 0x%08X)\n", sock);
		return SDFCTL_ERROR_INVALID_ARG;
	}
	ifp = s_mgr.ifp;
	if (ifp == NULL) {
		SDFCTL_PRINTF(552, "no host if\n");
		return SDFCTL_ERROR_NO_HOST_IF;
	}
	if (!(ifp->flags & SDFCTL_IF_SENDING)) {
		SDFCTL_PRINTF(557, "blocked\n");
		return SDFCTL_ERROR_BUSY;
	}
	if (pd->desc.protocol != SDFCTL_PROTOCOL_MGR &&
	    (!(s_mgr.status & SDFCTL_STATUS_CONNECTED) || !(pd->flags & SDFCTL_PD_STARTED)))
		return SDFCTL_ERROR_NO_ROUTE;
	if (ifp->sendSize == 0) {
		if (size < SDFCTL_HDR_SIZE) {
			SDFCTL_PRINTF(586, "invalid packet size %d (%s)\n", size, pd->desc.name);
			return SDFCTL_ERROR_INVALID_SIZE;
		}
		ifp->sendSize = *(const SceUInt16 *)hdr->size | hdr->size[2] << 16;
		hdr->version = SCE_DECI4P_VERSION;
		protocol = pd->desc.protocol;
		hdr->reserved0 = 0;
		hdr->reserved1 = 0;
		routeProtocol = pd->desc.protocol;
		hdr->protocol = protocol;
		hdr->reserved2 = 0;
		hdr->src = SDFCTL_PORT;
		if (routeProtocol != SDFCTL_PROTOCOL_MGR)
			hdr->dst = pd->desc.destination;
	}
	return ifp->ops.send(ifp, buf, size);
}

/* name unknown */
void sdfctlPoll(void)
{
	s_mgr.ifp->ops.poll(s_mgr.ifp);
}

/* name unknown */
SceUInt32 sdfctlTestStatus(SceUInt32 mask)
{
	return mask & s_mgr.status;
}

/* name unknown */
int sdfctlSetStatus(SceUInt32 mask, int on)
{
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_statusLock);
	if (on)
		s_mgr.status |= mask;
	else
		s_mgr.status &= ~mask;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_statusLock, intr);
	return 0;
}

/* name unknown: resume handler */
static int sdfctlResume(void *args)
{
	char pkt[24];
	int cpu;

	sceKernelSysrootCorelockLock(0);
	cpu = ksceKernelCpuId();
	if (cpu == 0) {
		s_mgr.ifp->ops.start(s_mgr.ifp);
		sdfctlSetStatus(SDFCTL_STATUS_CONNECTED, 1);
		sdfctlSendProtocolStatus();
		sdcmpMakeStatusConnection(1, pkt);
		sdcmpSendStatusPower(0xf, 1, cpu);
	}
	sceKernelSysrootCorelockUnlock();
	return 0;
}

/* name unknown: suspend handler */
static int sdfctlSuspend(void *args)
{
	char pkt[24];
	int cpu;

	sceKernelSysrootCorelockLock(0);
	cpu = ksceKernelCpuId();
	if (cpu == 0) {
		sdfctlSetStatus(SDFCTL_STATUS_CONNECTED, cpu);
		sdcmpMakeStatusConnection(cpu, pkt);
		s_mgr.ifp->ops.stop(s_mgr.ifp);
	}
	sceKernelSysrootCorelockUnlock();
	return 0;
}

/* name unknown */
int sdfctlSetProtocolStatus(SDfCtlPd *pd, SceUInt32 mask, int on)
{
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_statusLock);
	if (on)
		pd->flags |= mask;
	else
		pd->flags &= ~mask;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_statusLock, intr);
	return 0;
}

/* name unknown */
int sdfctlRegisterIf(int id, SDfCtlIf *ifp)
{
	s_mgr.ifp = ifp;
	return 0;
}

/* the host interface's transfer events */
int sceSDfCtlIf2MgrEvent(SDfCtlIf *ifp, int event, SceUIntPtr arg, SceUInt32 protocol, SceUInt16 route)
{
	SDfCtlPd *pd;
	SceSize total, done;
	int ret;

	switch (event) {
	case SDFCTL_EVENT_RECV:
		pd = ifp->recvPd;
		if (pd != NULL) {
			if (ifp->recvSize == 0)
				ifp->recvSize = HDR_SIZE(&ifp->recvHdr);
		} else {
			ifp->recvSize = 0;
			ifp->recvDone = 0;
			if (protocol == s_mgr.pd[0].desc.protocol)
				ifp->recvPd = &s_mgr.pd[0];
			else if (protocol == s_mgr.pd[1].desc.protocol)
				ifp->recvPd = &s_mgr.pd[1];
			ret = ifp->ops.recvStart(ifp);
			if (ret < 0)
				return ret;
			pd = ifp->recvPd;	/* (NULL for an unknown protocol) */
		}
		ifp->recvFlags |= SDFCTL_IF_IN_HANDLER;
		pd->handler(SDFCTL_EVENT_RECV, arg, pd->ctx);
		ifp->recvFlags &= ~SDFCTL_IF_IN_HANDLER;
		return 0;
	case SDFCTL_EVENT_RECV_DONE:
		if (ifp->recvPd == NULL) {
			SDFCTL_PRINTF(751, "IF_EVENT_INDONE: receiving PD was not found.\n");
			return 0;
		}
		total = ifp->recvSize;
		if (total == 0)
			total = ifp->recvSize = HDR_SIZE(&ifp->recvHdr);
		done = ifp->recvDone;
		if (done > total) {
			if (done > total + 3) {
				SDFCTL_PRINTF(760, "IFM_INDONE: Receive packet too large %d > %d.\n", done, total);
				total = ifp->recvSize;
				done = ifp->recvDone;
			}
			arg = total - done;
		}
		ifp->recvDone = done + arg;
		if (done + arg < total)
			return 0;
		ifp->recvPd->handler(SDFCTL_EVENT_RECV_DONE, arg, ifp->recvPd->ctx);
		ifp->recvPd = NULL;
		ifp->ops.recvDone(ifp);
		return 0;
	case SDFCTL_EVENT_SEND:
		pd = ifp->sendPd;
		if (pd == NULL) {
			SDFCTL_PRINTF(775, "IFM_OUT: Sending PD was not found.\n");
			return 0;
		}
		ifp->flags = (ifp->flags & ~SDFCTL_IF_SEND_STARTED) | SDFCTL_IF_SENDING;
		pd->handler(SDFCTL_EVENT_SEND, arg, pd->ctx);
		ifp->flags &= ~SDFCTL_IF_SENDING;
		return 0;
	case SDFCTL_EVENT_SEND_DONE:
		pd = ifp->sendPd;
		if (pd == NULL) {
			SDFCTL_PRINTF(787, "IFM_OUTDONE: Sending PD was not found.\n");
			return 0;
		}
		ifp->sendDone += arg;
		if (ifp->sendSize != 0 && ifp->sendDone >= ifp->sendSize) {
			ifp->sendPd = NULL;
			ifp->ops.sendDone(ifp);
			pd->handler(SDFCTL_EVENT_SEND_DONE, 0, pd->ctx);
			sdfctlSendNext();
		}
		return 0;
	case 5:
		ifp->flags |= SDFCTL_IF_UP;
		return 0;
	case 6:
		ifp->flags &= ~SDFCTL_IF_UP;
		return 0;
	}
	return 0;
}

/* name unknown: the host connected */
void sdfctlConnect(void)
{
	char pkt[24];

	sdfctlSendProtocolStatus();
	sdcmpMakeStatusConnection(1, pkt);
}

/* name unknown: the host disconnected */
void sdfctlDisconnect(void)
{
	char pkt[24];

	sdcmpMakeStatusConnection(0, pkt);
}

/* the protocol's status SDFCTL_EVENT_STATUS: started */
int sdfctlStartProtocol(SceUInt32 protocol)
{
	char pkt[40];
	SDfCtlPd *pd;
	int ret;

	if ((s_mgr.pd[0].flags & SDFCTL_PD_REGISTERED) && protocol == s_mgr.pd[0].desc.protocol)
		pd = &s_mgr.pd[0];
	else if ((s_mgr.pd[1].flags & SDFCTL_PD_REGISTERED) && protocol == s_mgr.pd[1].desc.protocol)
		pd = &s_mgr.pd[1];
	else
		return SDFCTL_ERROR_INVALID_PROTOCOL;
	ret = sdfctlSetProtocolStatus(pd, SDFCTL_PD_STARTED, 1);
	if (ret < 0) {
		SDFCTL_PRINTF(877, "SetProtocolStatus failed (0x%08X)\n", ret);
		return ret;
	}
	ret = sdcmpMakeStatusProtocol(1, &pd->desc, pkt);
	if (ret < 0) {
		SDFCTL_PRINTF(885, "couldn't make SDCMP packet (0x%08x)\n", ret);
		return ret;
	}
	pd->handler(SDFCTL_EVENT_STATUS, (SceUIntPtr)pkt, pd->ctx);
	return 0;
}

/* the protocol's status SDFCTL_EVENT_STATUS: stopped */
int sdfctlStopProtocol(SceUInt32 protocol)
{
	char pkt[40];
	SDfCtlPd *pd;
	int ret;

	if ((s_mgr.pd[0].flags & SDFCTL_PD_REGISTERED) && protocol == s_mgr.pd[0].desc.protocol)
		pd = &s_mgr.pd[0];
	else if ((s_mgr.pd[1].flags & SDFCTL_PD_REGISTERED) && protocol == s_mgr.pd[1].desc.protocol)
		pd = &s_mgr.pd[1];
	else
		return SDFCTL_ERROR_INVALID_ARG;
	ret = sdfctlSetProtocolStatus(pd, SDFCTL_PD_STARTED, 0);
	if (ret < 0) {
		SDFCTL_PRINTF(912, "SetProtocolStatus failed (0x%08X)\n", ret);
		return ret;
	}
	ret = sdcmpMakeStatusProtocol(0, &pd->desc, pkt);
	if (ret < 0) {
		SDFCTL_PRINTF(920, "couldn't make SDCMP packet (0x%08x)\n", ret);
		return ret;
	}
	pd->handler(SDFCTL_EVENT_STATUS, (SceUIntPtr)pkt, pd->ctx);
	return 0;
}

/* name unknown: memset (words when clearing aligned memory) */
/* Keep the original call boundary when its body delegates to a shared inline. */
__attribute__((noinline)) void sdfctlMemset(void *p, int c, SceSize n)
{
	if (p == NULL)
		return;
	deci4pFillMemory(p, c, n);
}

int sceSDfCtlModuleStart(SceSize args, void *argp)
{
	const void *lr = __builtin_return_address(0);
	int ret;

	if (!ksceKernelCheckDipsw(0xfd)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 77, "[DECI] remote power control is disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	ret = ksceKernelCheckDipsw(0xc4);
	if (ret != 0) {
		ksceKernelPrintf("[DECI] remote power control is disabled (resource busy)\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	sdfctlMemset(&s_mgr, 0, sizeof(s_mgr));
	s_lock = ret;
	s_statusLock = ret;
	sdfctlSetStatus(SDFCTL_STATUS_CONNECTED, 1);

	ret = sdcmpInit(_getPdCB(0));
	if (ret < 0) {
		SDFCTL_PRINTF(103, "sdcmpInit failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_CHECK_LR(0, 104, lr);
		return ret;
	}
	ret = stsmpInit(_getPdCB(1));
	if (ret < 0) {
		SDFCTL_PRINTF(111, "stsmpInit failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_CHECK_LR(0, 112, lr);
		return ret;
	}
	ret = uartInit();
	if (ret < 0) {
		SDFCTL_PRINTF(119, "uartInit failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_CHECK_LR(0, 120, lr);
		return ret;
	}
	SceKernelSuspendForDriver_CE7A2207(0x18, sdfctlSuspend, NULL);
	SceKernelSuspendForDriver_105C5752(0x19, sdfctlResume, NULL);
	sdfctlSetStatus(SDFCTL_STATUS_POWER, 1);
	sdcmpSendStatusPower(0xf, 1, 0);
	return SCE_KERNEL_START_SUCCESS;
}
