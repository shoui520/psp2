/*
 * SceSysmem: the UARTs. Seven ports of 0x10000 bytes of registers each and
 * their clock generator; kernel printf output goes to the debug ports that
 * are enabled. The secure kernel's SceSysmem (SYSMEM_TZS) has no suspend
 * handlers and no reads; the boot loaders (SYSMEM_NSKBL, and SKBL with
 * SYSMEM_SKBL as well) use the registers at their physical addresses and have
 * only the debug output.
 */

#include <psp2kern/kernel/sysclib.h>
#include "sysmem_internal.h"
#include <sysmem/suspend.h>
#include <lowio/uart_regs.h>

/* what each build has */
#if !defined(SYSMEM_TZS) && !defined(SYSMEM_NSKBL)
#define SERIAL_SUSPEND			1
#define SERIAL_READ			1
#endif
#ifndef SYSMEM_NSKBL
#define SERIAL_WRITE			1
#define SERIAL_REMAP			1
#endif

#define UART_DEBUG_PORTS		3
#define UART_SUSPEND_SLOT		0x1b

#define UART_STATUS_TX_READY		0x100	/* 0x28 */
#define UART_STATUS_TX_IDLE		0x200
#define UART_FIFO_COUNT_MASK		0x3f	/* 0x68 */

/* a port kernel printf may write to */
typedef struct UartDebugPort {
	SceUInt32 enabled;
	SceUInt32 port;
} UartDebugPort;

int ksceKernelCpuId(void);
int ksceKernelCheckDipsw(unsigned int bit);
int ksceUartInit(int port);
int SceSysrootForDriver_AE7A8F1D(void);
int kscePervasiveUartResetEnable(int port);
int kscePervasiveUartResetDisable(int port);
int kscePervasiveUartClockEnable(int port);

UartDebugPort s_debugPorts[UART_DEBUG_PORTS] = {
	{ 0, 0 }, { 0, 1 }, { 0, 6 },
};
/* the physical addresses until serial_remap maps them */
#define UART_PORT_BASE(port)		(void *)(UART_REG_BASE + (port) * UART_REG_SIZE)
volatile SceUartRegisters *s_uartRegs[UART_PORTS] = {
	UART_PORT_BASE(0), UART_PORT_BASE(1), UART_PORT_BASE(2), UART_PORT_BASE(3),
	UART_PORT_BASE(4), UART_PORT_BASE(5), UART_PORT_BASE(6),
};
volatile SceUartClkgenRegisters *s_uartClkgen = (void *)UART_CLKGEN_BASE;

/* the clock generator value of each baud rate index */
static const SceUInt32 s_baudRates[] = {
	0x12710, 0x11388, 0x109c4, 0x104e2, 0x10271, 0x10139, 0x100d0, 0x1009c,
	0x10068, 0x1004e, 0x10034, 0x1001a, 0x1000d, 0x1000c, 0x2001a, 0x2000d,
};

static inline void SerialWaitTxReady(volatile SceUartRegisters *regs)
{
	while (!(regs->status & UART_STATUS_TX_READY))
		;
}

#if SERIAL_SUSPEND
/* name unknown: the resume handler */
static int SerialResume(void *args)
{
	return 0;
}
#endif

/* name unknown */
__attribute__((noinline)) void SerialFlush(unsigned int port)
{
	while (!(s_uartRegs[port]->status & UART_STATUS_TX_IDLE))
		;
}

#if SERIAL_SUSPEND
/* name unknown: the suspend handler, the debug ports drained first */
static int SerialSuspend(void *args)
{
	unsigned int i;

	if (ksceKernelCpuId() == 0) {
		for (i = 0; i < UART_DEBUG_PORTS; i++) {
			if (s_debugPorts[i].enabled)
				SerialFlush(s_debugPorts[i].port);
		}
	}
	return 0;
}
#endif

/* name unknown: a character to a port, CR before LF */
__attribute__((noinline)) void SerialPutcharCRLF(unsigned int port, char c)
{
	volatile SceUartRegisters *regs = s_uartRegs[port];

	if (c == '\n') {
		SerialWaitTxReady(regs);
		regs->tx = '\r';
	}
	SerialWaitTxReady(regs);
	regs->tx = c;
}

/* name unknown: the debug putchar handler, to every enabled debug port */
int SerialDebugPutchar(void *args, int c)
{
	unsigned int i;

	/* 0x200 and 0x201 are not characters */
	if (c != 0x200 && c != 0x201) {
		for (i = 0; i < UART_DEBUG_PORTS; i++) {
			if (s_debugPorts[i].enabled) {
				SerialPutcharCRLF(s_debugPorts[i].port, c);
				SerialFlush(s_debugPorts[i].port);
			}
		}
	}
	return 0;
}

#if SERIAL_WRITE
__attribute__((noinline)) void SerialPutchar(unsigned int port, char c)
{
	SerialWaitTxReady(s_uartRegs[port]);
	s_uartRegs[port]->tx = c;
}
#endif

#if SERIAL_READ
__attribute__((noinline)) int SerialGetchar(unsigned int port)
{
	volatile SceUartRegisters *regs = s_uartRegs[port];

	while (!(regs->fifoStatus & UART_FIFO_COUNT_MASK))
		;
	regs->rxAck = 0x77f;
	return regs->rx;
}

__attribute__((noinline)) unsigned int SerialReadAvailable(unsigned int port)
{
	return s_uartRegs[port]->fifoStatus & UART_FIFO_COUNT_MASK;
}
#endif

#if SERIAL_WRITE
int ksceUartWrite(int port, int c)
{
	SerialPutchar(port, c);
	SerialFlush(port);
	return 0;
}
#endif

#if SERIAL_READ
int ksceUartRead(int port)
{
	return SerialGetchar(port);
}

int ksceUartReadAvailable(int port)
{
	return SerialReadAvailable(port);
}
#endif

void serial_set_baud_rate(unsigned int port, unsigned int baud, SceUInt32 mode,
			  SceUInt32 flowControl, SceUInt32 fifoConfig, SceUInt32 irqMask)
{
	SceUInt32 rate;

	if (baud == 0)
		baud = 9;
	if (mode == 0)
		mode = 3;
	s_uartRegs[port]->enable = 0;
	rate = s_baudRates[baud];
	switch (port) {
	case 0:
		s_uartClkgen->port[0] = rate;
		break;
	case 1:
		s_uartClkgen->port[1] = rate;
		break;
	case 2:
		s_uartClkgen->port[2] = rate;
		break;
	case 3:
		s_uartClkgen->port[3] = rate;
		break;
	case 4:
		s_uartClkgen->port[4] = rate;
		break;
	case 5:
		s_uartClkgen->port[5] = rate;
		break;
	case 6:
		s_uartClkgen->port[6] = rate;
		break;
	default:
		SCE_DBG_ASSERT_LEVEL_CHECK(2, 0, 135);
		break;
	}
	s_uartRegs[port]->mode = mode;
	s_uartRegs[port]->control = 1;
	s_uartRegs[port]->flowControl = flowControl;
	s_uartRegs[port]->fifoConfig = fifoConfig;
	s_uartRegs[port]->irqConfig = 0;
	s_uartRegs[port]->irqMask = irqMask;
	s_uartRegs[port]->fifoControl = 0x10001;
	s_uartRegs[port]->enable = 1;
}

#if SERIAL_REMAP
/* the UART and clock generator registers into kernel memory */
int serial_remap(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned int i;
	void *base;
	SceUID uid;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.paddr = UART_REG_BASE;
	opt.attr = 2;
	uid = ksceKernelAllocMemBlock("SceUartReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW, UART_PORTS * UART_REG_SIZE, &opt);
	if (uid < 0) {
		SCE_DBG_PRINTF(0xb, 242, "allocate uart register area failed. 0x%08x\n", uid);
		SCE_DBG_ASSERT_LEVEL_CHECK(2, 0, 243);
		return uid;
	}
	ksceKernelGetMemBlockBase(uid, &base);
	for (i = 0; i < UART_PORTS; i++)
		s_uartRegs[i] = (volatile SceUartRegisters *)((char *)base + i * UART_REG_SIZE);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.paddr = UART_CLKGEN_BASE;
	opt.attr = 2;
	uid = ksceKernelAllocMemBlock("SceUartClkgenReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW, 0x1000, &opt);
	if (uid < 0) {
		SCE_DBG_PRINTF(0xb, 262, "allocate uart clkgen area failed. 0x%08x\n", uid);
		SCE_DBG_ASSERT_LEVEL_CHECK(2, 0, 263);
		return uid;
	}
	ksceKernelGetMemBlockBase(uid, &base);
	s_uartClkgen = base;
	return 0;
}
#endif

/* name unknown: the registers mapped, the debug ports chosen, the suspend handlers set */
struct SysmemBootContext;

#if defined(SYSMEM_SKBL)
/* the secure boot loader starts the ports itself: port 1 when either debug
 * UART dipsw is set, used for output only with 0xc5 */
int SerialInit(int uart0Enabled)
{
	if (uart0Enabled) {
		ksceUartInit(s_debugPorts[0].port);
		s_debugPorts[0].enabled = 1;
	}
	if (!ksceKernelCheckDipsw(0xc5) && !ksceKernelCheckDipsw(0xfd))
		return 0;
	ksceUartInit(1);
	if (ksceKernelCheckDipsw(0xc5))
		s_debugPorts[1].enabled = 1;
	return 0;
}
#else
#ifdef SYSMEM_NSKBL
/* the boot loader passes its dipsw 0xc4 (the debug UART) */
int SerialInit(int uart0Enabled)
{
	if (uart0Enabled)
		s_debugPorts[0].enabled = 1;
#else
int SerialInit(const struct SysmemBootContext *context)
{
	serial_remap();
	if (SceSysrootForDriver_AE7A8F1D())
		s_debugPorts[0].enabled = 1;
#endif
	if (ksceKernelCheckDipsw(0xc5))
		s_debugPorts[1].enabled = 1;
#if SERIAL_SUSPEND
	SceKernelSuspendForDriver_CE7A2207(UART_SUSPEND_SLOT, SerialSuspend, NULL);
	SceKernelSuspendForDriver_105C5752(UART_SUSPEND_SLOT, SerialResume, NULL);
#endif
	return 0;
}
#endif

int ksceUartInit(int port)
{
	SceUInt32 flowControl, fifoConfig, irqMask, baud;

#ifdef SYSMEM_NSKBL
	kscePervasiveUartResetEnable(port);
	kscePervasiveUartClockEnable(port);
	kscePervasiveUartResetDisable(port);
#endif
	if (port == 1) {
		irqMask = 0x77e;
		fifoConfig = 0x300;
		flowControl = 0;
		baud = 0xb;
	} else if (port == 6) {
		irqMask = 0x77e;
		fifoConfig = 0x300;
		flowControl = 3;
		baud = 0xd;
	} else {
		irqMask = 0;
		fifoConfig = 0x303;
		flowControl = 0;
		baud = 0xb;
	}
	serial_set_baud_rate(port, baud, 3, flowControl, fifoConfig, irqMask);
	SerialFlush(port);
	return 0;
}
