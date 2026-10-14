/*
 * The SDfCtl host interface on the debug UART (port 1, interrupt 0xe1). A packet
 * header arriving raises the interrupt; the rest is polled byte by byte. Sending
 * starts with a 4-byte sync sequence.
 */

#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/uart.h>
#include "sce/debug.h"
#include "sce/intrmgr.h"
#include "sdfctl.h"

#define UART_PORT		1
#define UART_INTR		0xe1

/* SDfCtlIf.state */
#define UART_UP			0x1
#define UART_SEND_READY		0x20
#define UART_SEND_DONE		0x40
#define UART_SEND_BLOCKED	0x10
#define UART_RECV_STARTED	0x100
#define UART_RECV_BLOCKED	0x1000
#define UART_RECV_HEADER	0x2000	/* (also: more to read) */
#define UART_RECV_DONE		0x4000

/* SDfCtlIf.intr */
#define UART_INTR_SEND		0x1
#define UART_INTR_RECV		0x100

static SceKernelSpinlock s_uartLock;
static SDfCtlIf s_uart;

static void uartDispatch(SDfCtlIf *ifp);

static int uartSendDone(SDfCtlIf *ifp)
{
	ifp->state &= ~(UART_SEND_READY | UART_UP);
	ifp->intr &= ~UART_INTR_SEND;
	return 0;
}

static int uartSendBlock(SDfCtlIf *ifp)
{
	ifp->state |= UART_SEND_BLOCKED;
	return 0;
}

static int uartSendUnblock(SDfCtlIf *ifp)
{
	ifp->state &= ~UART_SEND_BLOCKED;
	return 0;
}

static int uartNop(SDfCtlIf *ifp)
{
	return 0;
}

static int recvStart(SDfCtlIf *ifp)
{
	SCE_DBG_ASSERT_CHECK(ifp != NULL, 203);
	ifp->state |= UART_RECV_STARTED;
	return 0;
}

static int uartStart(SDfCtlIf *ifp)
{
	ksceKernelEnableIntr(UART_INTR);
	ifp->state |= UART_UP;
	ifp->intr |= UART_INTR_SEND;
	return 0;
}

static int uartRecvUnblock(SDfCtlIf *ifp)
{
	SceUInt32 state = ifp->state;

	ifp->state = state & ~UART_RECV_BLOCKED;
	if (!(state & (UART_RECV_HEADER | UART_RECV_DONE)))
		ksceKernelEnableIntr(UART_INTR);
	return 0;
}

static int uartRecv(SDfCtlIf *ifp, void *buf, SceSize size)
{
	SceUInt8 *p = buf;
	SceSize n;

	n = ifp->recvSize - ifp->recvDone;
	if (size < n)
		n = size;
	for (; p != (SceUInt8 *)buf + n; p++)
		*p = ksceUartRead(UART_PORT);
	ifp->recvDone += n;
	if (ifp->recvDone < ifp->recvSize)
		ifp->state |= UART_RECV_HEADER;
	else
		ifp->state |= UART_RECV_DONE;
	return n;
}

static int uartStop(SDfCtlIf *ifp)
{
	ifp->state &= ~UART_UP;
	ifp->intr &= ~UART_INTR_SEND;
	ksceKernelDisableIntr(UART_INTR);
	return 0;
}

static int uartRecvBlock(SDfCtlIf *ifp)
{
	ifp->state |= UART_RECV_BLOCKED;
	ksceKernelDisableIntr(UART_INTR);
	return 0;
}

static int uartSend(SDfCtlIf *ifp, void *buf, SceSize size)
{
	const signed char *p = buf;
	SceSize n;

	if (ifp->sendDone == 0)
		sdfctlMemcpy(&ifp->sendHdr, buf, sizeof(ifp->sendHdr));
	n = sdfctlGetSize(&ifp->sendHdr) - ifp->sendDone;
	if (size <= n)
		n = size;
	if (n == 0) {
		ifp->state |= UART_SEND_DONE;
		ifp->sendDone = sdfctlGetSize(&ifp->sendHdr);
		return 0;
	}
	for (; p != (const signed char *)buf + n; p++)
		ksceUartWrite(UART_PORT, *p);
	ifp->sendDone += n;
	if (ifp->sendDone < sdfctlGetSize(&ifp->sendHdr))
		ifp->state |= UART_SEND_READY;
	else
		ifp->state |= UART_SEND_DONE;
	return n;
}

static int uartRecvDone(SDfCtlIf *ifp)
{
	ifp->state &= ~(UART_RECV_HEADER | UART_RECV_STARTED);
	ifp->intr &= ~UART_INTR_RECV;
	sdfctlSetSize(&ifp->recvHdr, 0);
	if (!(ifp->state & UART_RECV_BLOCKED))
		ksceKernelEnableIntr(UART_INTR);
	ifp->recvSize = 0;
	ifp->recvDone = 0;
	return 0;
}

/* name unknown: ready to send now, or on the send interrupt */
static void uartKick(SDfCtlIf *ifp, SceUInt32 *intr)
{
	int s;

	s = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_uartLock);
	if (ifp->state & UART_UP) {
		if (*intr & UART_INTR_SEND)
			ifp->state |= UART_SEND_READY;
		else
			ksceKernelEnableIntr(UART_INTR);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_uartLock, s);
}

/* name unknown: the pending transfer events to SDfCtl */
static void uartDispatch(SDfCtlIf *ifp)
{
	SceUInt32 state = ifp->state;
	int progress;

	for (;;) {
		progress = 0;
		if (state & UART_SEND_DONE) {
			ifp->state = state & ~UART_SEND_DONE;
			if (sceSDfCtlIf2MgrEvent(ifp, SDFCTL_EVENT_SEND_DONE, ifp->sendDone, 0, 0) < 0)
				return;
			state = ifp->state;
			if (state & UART_UP) {
				uartKick(ifp, &ifp->intr);
				state = ifp->state;
			}
			progress = 1;
		}
		if (!(state & UART_SEND_BLOCKED) &&
		    (ifp->state & (UART_SEND_READY | UART_UP)) == (UART_SEND_READY | UART_UP)) {
			ifp->state = state & ~UART_SEND_READY;
			if (sceSDfCtlIf2MgrEvent(ifp, SDFCTL_EVENT_SEND, (SceUIntPtr)ifp, 0, 0) < 0)
				return;
			state = ifp->state;
			progress = 1;
		}
		if (state & UART_RECV_DONE) {
			ifp->state = state & ~UART_RECV_DONE;
			if (sceSDfCtlIf2MgrEvent(ifp, SDFCTL_EVENT_RECV_DONE, ifp->recvDone, 0, 0) < 0)
				return;
			state = ifp->state;
			progress = 1;
		}
		if (!(state & UART_RECV_BLOCKED) && (state & UART_RECV_HEADER)) {
			ifp->state = state & ~UART_RECV_HEADER;
			if (sceSDfCtlIf2MgrEvent(ifp, SDFCTL_EVENT_RECV, sdfctlGetSize(&ifp->recvHdr),
						 ifp->recvHdr.protocol, ifp->recvHdr.dst) < 0)
				return;
			state = ifp->state;
			continue;
		}
		if (!progress)
			return;
	}
}

static int uartPoll(SDfCtlIf *ifp)
{
	uartDispatch(ifp);
	return 0;
}

static int uartSendStart(SDfCtlIf *ifp)
{
	ifp->sendDone = 0;
	ifp->state |= UART_UP;
	ifp->intr |= UART_INTR_SEND;
	uartKick(ifp, &ifp->intr);
	ksceUartWrite(UART_PORT, 0xd);
	ksceUartWrite(UART_PORT, 0xe);
	ksceUartWrite(UART_PORT, 0xc);
	ksceUartWrite(UART_PORT, 1);
	return 0;
}

/* a packet header arrived */
static int _uartIntrHandler(int code, void *arg)
{
	SDfCtlIf *ifp = arg;
	int i;

	if (ifp == NULL)
		return -1;
	if (code != UART_INTR) {
		SDFCTL_PRINTF(526, "Unknown intrcode\n");
		return -2;
	}
	for (i = 0; i != sizeof(ifp->recvHdr); i++)
		((SceUInt8 *)&ifp->recvHdr)[i] = ksceUartRead(UART_PORT);
	ifp->state |= UART_RECV_HEADER;
	uartDispatch(ifp);
	return -1;
}

static const SDfCtlIfOps s_uartOps = {
	.recvStart	= recvStart,
	.recv		= uartRecv,
	.recvDone	= uartRecvDone,
	.sendStart	= uartSendStart,
	.send		= uartSend,
	.sendDone	= uartSendDone,
	.poll		= uartPoll,
	.recvBlock	= uartRecvBlock,
	.recvUnblock	= uartRecvUnblock,
	.sendBlock	= uartSendBlock,
	.sendUnblock	= uartSendUnblock,
	.reserved0	= NULL,
	.reserved1	= uartNop,
	.stop		= uartStop,
	.start		= uartStart,
};

int uartInit(void)
{
	int ret, ret2;

	s_uartLock = 0;
	s_uart.ops = s_uartOps;
	s_uart.name = "UART";
	ret = ksceKernelRegisterIntrHandler(UART_INTR, "SceUart1", 0, _uartIntrHandler, &s_uart,
					    0x50, 0x10000, NULL);
	if (ret < 0) {
		SDFCTL_PRINTF(113, "register uart interrupt handler failed\n");
		SCE_DBG_ASSERT_FAIL(114);
	}
	ret = sdfctlRegisterIf(0, &s_uart);
	if (ret < 0) {
		SDFCTL_PRINTF(120, "RegisterIf failed (0x%08x)\n", ret);
		SCE_DBG_ASSERT_FAIL(121);
		return ret;
	}
	ksceUartInit(UART_PORT);
	ret2 = sceSDfCtlIf2MgrEvent(&s_uart, 5, 0, 0, 0);
	if (ret2 < 0) {
		SDFCTL_PRINTF(131, "sceSDfCtlIf2MgrEvent failed (0x%08x)\n", ret2);
		SCE_DBG_ASSERT_FAIL(132);
		return ret2;
	}
	s_uart.intr |= UART_INTR_SEND;
	ret2 = ksceKernelEnableIntr(UART_INTR);
	if (ret2 < 0) {
		SDFCTL_PRINTF(145, "sceKernelEnableIntr failed (0x%08x)\n", ret2);
		SCE_DBG_ASSERT_FAIL(146);
		return ret2;
	}
	return 0;
}
