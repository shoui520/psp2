/*
 * SceDeci4pSCTtyp: the development host's console on UART 6, bridged to a
 * DECI4P channel (protocol 0x900300C0, "SCTTYP") through SceDeci4pSDfMgr.
 *
 * Characters received on the UART are collected into a 255-byte line and
 * sent to the host on a newline (from the interrupt handler) or whenever the
 * debugger polls with the system stopped (break hook). The UART belongs to
 * CPU 1; data from the host is read and dropped.
 */

#include <sysmem/dipsw.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/uart.h>
#include <psp2kern/lowio/pervasive.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/qafmgr.h>
#include <deci4p_sdfmgr/sdfmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>

#define SCTTYP_UART		6
#define SCTTYP_INTR		0xe6
#define SCTTYP_CPU		1
#define SCTTYP_LINE_MAX		0xff
#define SCTTYP_SUSPEND_IDX	0x15

#define SDFMGR_ERROR_BUSY	0x80090007

#if FW_DEBUG
#define SCTTYP_PRINTF(_line, _fmt, ...)					\
	SceDeci4pSDfMgrForKernel_501CE61D(0xf, SCE_DBGINFO(_line), _fmt, ##__VA_ARGS__)
#else
#define SCTTYP_PRINTF(_line, _fmt, ...)	do { } while (0)
#endif

typedef struct ScttypPort {
	int handle;				/* SceDeci4pSDfMgrForKernel_A55F5F0D */
	SceUInt32 connected;
	SceSize len;				/* characters in buf */
	SceSize sent;				/* bytes of the packet sent */
	TtypHeader hdr;				/* the packet: hdr, then buf[len] */
	char buf[0x100];
} ScttypPort;

_Static_assert(sizeof(ScttypPort) == 0x13c, "ScttypPort layout");

static ScttypPort s_port;

static int resumeHandler(void *args)
{
	int ret;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == SCTTYP_CPU) {
		kscePervasiveUartResetEnable(SCTTYP_UART);
		kscePervasiveUartClockEnable(SCTTYP_UART);
		kscePervasiveUartResetDisable(SCTTYP_UART);
		ret = SceUartForKernel_4C02AA05(SCTTYP_UART);
		if (ret < 0) {
			/* (returns with the core lock held) */
			SCTTYP_PRINTF(739, "UartChStart() failed (0x%08X)\n", ret);
			return ret;
		}
		ret = ksceKernelEnableIntr(SCTTYP_INTR);
		if (ret < 0) {
			SCTTYP_PRINTF(746, "EnableIntr() failed (0x%08X)\n", ret);
			return ret;
		}
	}
	sceKernelSysrootCorelockUnlock();
	return 0;
}

static int suspendHandler(void *args)
{
	int ret;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == SCTTYP_CPU) {
		ret = ksceKernelDisableIntr(SCTTYP_INTR);
		if (ret < 0)
			SCTTYP_PRINTF(712, "DisableIntr() failed (0x%08X)\n", ret);
	}
	sceKernelSysrootCorelockUnlock();
	return 0;
}

/* data from the host: read and drop it */
static inline void deciEventRecv(ScttypPort *port, int size)
{
	char data[16];
	int ret;

	for (; size > 0; size -= ret) {
		ret = sceSDfMgrExRecv(port->handle, data, sizeof(data));
		if (ret < 0) {
			SCTTYP_PRINTF(442, "SDfMgrExRecv() failed (0x%08X)\n", ret);
			break;
		}
	}
}

/* send (the rest of) the packet */
static inline void deciEventSend(ScttypPort *port)
{
	SceSize sent = port->sent;
	int ret;

	ret = sceSDfMgrExSend(port->handle, (SceUInt8 *)&port->hdr + sent,
			      SceDeci4pSDfMgrForKernel_68903C1D(&port->hdr) - port->sent);
	if (ret < 0)
		SCTTYP_PRINTF(474, "ExSend() failed (0x%08X)\n", ret);
	else
		port->sent += ret;
}

static void deciEventHandler(int event, void *arg, void *ctx)
{
	ScttypPort *port = ctx;
	const SceDfMgrStatus *st;

	switch (event) {
	case 1:
		deciEventRecv(port, (int)arg);
		break;
	case 2:
	case 6:
		break;
	case 3:
		deciEventSend(port);
		break;
	case 4:					/* sent: the line buffer is free */
		port->len = 0;
		break;
	case 5:
		st = arg;
		if (st->kind == 0) {
			if (st->connected == 0)
				port->connected = 0;
			else if (st->connected == 1)
				port->connected = 1;
		} else if (st->kind == 1) {
			if (st->enabled == 0)
				port->connected = 0;
			else if (st->enabled == 1)
				port->connected = 1;
		}
		break;
	default:
		SCTTYP_PRINTF(416, "unknown event (0x%08x)\n", event);
		break;
	}
}

/* send the line and poll until the host took it */
static int sendData(void)
{
	SceUInt64 t;
	int ret;

	if (s_port.len == 0)
		return 0;
	SceDeci4pSDfMgrForKernel_FFC95B63(&s_port.hdr.hdr, sizeof(TtypHeader) + s_port.len);
	s_port.hdr.len = s_port.len;
	t = SceSysrootForDriver_273EAE53();
	s_port.hdr.time[0] = t;
	s_port.hdr.time[1] = t >> 32;
	s_port.sent = 0;
	ret = sceSDfMgrExReqSend(s_port.handle);
	if (ret < 0)
		goto negative;
poll:
	while (s_port.len != 0)
		SceDeci4pSDfMgrForKernel_98AEC60E();
	return 0;
negative:
	if ((SceUInt32)ret == SDFMGR_ERROR_BUSY)
		goto poll;
	SCTTYP_PRINTF(687, "ReqSend failed (0x%08X)\n", ret);
	return ret;
}

/* break hook: the system is stopped, poll the UART */
static void uartPollHandler(void)
{
	char c;
	int ret;

	if (sceKernelCpuId() != SCTTYP_CPU)
		return;
	if (sceKernelGetIntrPendingStatusForDebugger(SCTTYP_INTR) == 0)
		return;

	while (SceUartForKernel_16780BC3(SCTTYP_UART)) {
		c = SceUartForKernel_4E97D3AD(SCTTYP_UART);
		s_port.buf[s_port.len] = c;
		if (++s_port.len > SCTTYP_LINE_MAX)
			break;
	}
	ret = sendData();
	if (ret < 0)
		SCTTYP_PRINTF(577, "error (0x%08X)\n", ret);
}

static int uartIntrHandler(int intrCode, void *userCtx)
{
	SceUInt8 c;
	int ret;

	if (intrCode != SCTTYP_INTR) {
		SCTTYP_PRINTF(594, "Unknown intrcode (0x%08x)\n", intrCode);
		return -1;
	}

	c = 0;
	for (;;) {
		if (!SceUartForKernel_16780BC3(SCTTYP_UART)) {
			if (c != '\n')
				return -1;	/* send whole lines only */
			break;
		}
		c = SceUartForKernel_4E97D3AD(SCTTYP_UART);
		s_port.buf[s_port.len] = c;
		if (++s_port.len > SCTTYP_LINE_MAX)
			break;
	}
	ret = sendData();
	if (ret < 0) {
		SCTTYP_PRINTF(615, "error (0x%08X)\n", ret);
		s_port.len = 0;
	}
	return -1;
}

static int initThread(SceSize args, void *argp)
{
	SceSDfMgrProtocol proto;
	int ret;

	if (args != 4) {
		SCTTYP_PRINTF(243, "invalid args (%d)\n", args);
		sceKernelExitThread(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
	}

	kscePervasiveUartResetEnable(SCTTYP_UART);
	kscePervasiveUartClockEnable(SCTTYP_UART);
	kscePervasiveUartResetDisable(SCTTYP_UART);
	ret = SceUartForKernel_4C02AA05(SCTTYP_UART);
	if (ret < 0) {
		SCTTYP_PRINTF(254, "UartChStart() failed (0x%08X)\n", ret);
		return ret;
	}

	s_port.hdr.hdr.version = SCE_DECI4P_VERSION;
	s_port.hdr.hdr.protocol = 0x900300c0;
	s_port.hdr.hdr.src = 0xa202;
	s_port.hdr.port = TTYP_PORT_KERNEL;
	s_port.hdr.hdr.dst = 0;
	proto.size = sizeof(proto);
	proto.protocol = 0x900300c0;
	proto.alias = 0x800300c0;
	proto.version = 0x01000000;
	proto.subVersion = 0x01000000;
	proto.destination = 0;
	proto.flags = 3;
	proto.name = "SCTTYP";
	ret = SceDeci4pSDfMgrForKernel_A55F5F0D(deciEventHandler, &proto, &s_port);
	if (ret < 0) {
		SCTTYP_PRINTF(282, "SDfMgrOpen() failed (0x%08X)\n", ret);
		return ret;
	}
	s_port.handle = ret;

	ret = SceDeci4pSDfMgrForKernel_B22B42FB(uartPollHandler);
	if (ret < 0) {
		SCTTYP_PRINTF(290, "RegisterBreakHook() failed (0x%08x)\n", ret);
		goto close;
	}
	ret = ksceKernelRegisterIntrHandler(SCTTYP_INTR, "SceUart6", 0, uartIntrHandler, NULL,
					    0x10, 0x20000, NULL);
	if (ret < 0) {
		SCTTYP_PRINTF(304, "RegisterIntrHandler() failed (0x%08X)\n", ret);
		goto unhook;
	}
	ret = ksceKernelEnableIntr(SCTTYP_INTR);
	if (ret >= 0) {
		sceKernelExitThread(0);
		return 0;
	}
	SCTTYP_PRINTF(311, "EnableIntr() failed (0x%08X)\n", ret);
	ret = ksceKernelReleaseIntrHandler(SCTTYP_INTR);
	if (ret < 0)
		SCTTYP_PRINTF(322, "ReleaseIntrHandler() failed (0x%08X)\n", ret);
unhook:
	ret = SceDeci4pSDfMgrForKernel_E53400AE(uartPollHandler);
	if (ret < 0)
		SCTTYP_PRINTF(330, "UnregisterBreakHook() failed (0x%08X)\n", ret);
close:
	SceDeci4pSDfMgrForKernel_86CFC8C6(s_port.handle);
	sceKernelExitThread(ret);
	return ret;
}

int SCTtypModuleStart(SceSize args, void *argp)
{
	SceKernelThreadOptParam opt;
	SceUID thid;
	int ret, status;

	if (!SceDipswForDriver_A98FC2FD(0xc1)) {
		SCE_DBG_PRINTF(0xb, 116, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (!SceQafMgrForDriver_382C71E8())
		return SCE_KERNEL_START_NO_RESIDENT;

	SceDeci4pSDfMgrForKernel_8A3B06F2(&s_port, 0, sizeof(s_port));
	/* only the first three fields are set, as in Sony's code */
	opt.size = sizeof(opt);
	opt.attr = 1;
	opt.kStackMemType = SceDeci4pSDfMgrForKernel_35238B8A();
	thid = sceKernelCreateThread("SceDeci4pSCTtyp", initThread, 0x1e, 0x2000, 0, 0x20000, &opt);
	if (thid < 0) {
		SCTTYP_PRINTF(141, "CreateThread failed (0x%08X)\n", thid);
		return thid;
	}
	ret = sceKernelStartThread(thid, 4, &s_port);
	if (ret < 0) {
		SCTTYP_PRINTF(149, "StartThread failed (0x%08X)\n", ret);
		return ret;
	}
	ret = sceKernelWaitThreadEnd(thid, &status, NULL);
	if (ret < 0)
		SCTTYP_PRINTF(156, "WaitThreadEnd failed (0x%08X)\n", ret);
	ret = sceKernelDeleteThread(thid);
	if (ret < 0)
		SCTTYP_PRINTF(163, "DeleteThread failed (0x%08X)\n", ret);
	if (status < 0) {
		SCTTYP_PRINTF(169, "init failed (0x%08X)\n", status);
		return SCE_KERNEL_START_NO_RESIDENT;
	}

	SceKernelSuspendForDriver_CE7A2207(SCTTYP_SUSPEND_IDX, suspendHandler, NULL);
	SceKernelSuspendForDriver_105C5752(SCTTYP_SUSPEND_IDX, resumeHandler, NULL);
	return SCE_KERNEL_START_SUCCESS;
}
