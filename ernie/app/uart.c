/*
 * The UARTs: each a pair of serial channels, one sending and one receiving (UART0 and 1 on
 * SAU0, 2 and 3 on SAU1), through two ring buffers, the receiving one filled and the sending
 * one emptied by interrupt. A port may have a timeout: after each byte received a timer
 * runs, and when it runs out an event flag is set. UART2 (INTST2, INTSR2, INTSRE2) and UART3
 * (INTST3, INTSR3, INTSRE3) are set up here; UART4 has its port here and its channels in
 * app/uart4.c.
 */
#if (ERNIE_HW >> 20) == 0x008	/* CHIP_RL78 (#pragma comes first) */
/* USS-1001 has no UART3; UART1's vectors, one up from the device file's, are in vectors_rl78.asm */
#define UART1_INTERRUPT	__rtos_interrupt
#else
#pragma rtos_interrupt INTSRE3 uart3ErrorInterrupt
#pragma rtos_interrupt INTSR3 uart3ReceiveInterrupt
#pragma rtos_interrupt INTST3 uart3SendInterrupt
#endif
#pragma interrupt INTSRE2 uart2ErrorInterrupt
#pragma interrupt INTSR2 uart2ReceiveInterrupt
#pragma interrupt INTST2 uart2SendInterrupt
#pragma sfr
#pragma DI
#pragma EI

#include <stddef.h>

#include "ernie.h"
#include "chip.h"
#include "board.h"
#include "kernel.h"
#include "serial.h"
#include "task.h"
#include "timer.h"
#include "uart.h"

/* UartPort.flags */
#define UART_OPEN		0x01
#define UART_CLOSED		0x02	/* opened before, then closed */
#define UART_SENDING		0x04	/* INTST sends the next byte */

/* a port's channels: the data registers and the status registers */
struct UartRegisters {
	volatile unsigned int *tx;
	volatile unsigned int *rx;
	volatile unsigned int *txStatus;
	volatile unsigned int *rxStatus;
};

/* a channel's speed: SPS's prescaler, SDR's divisor and the channels' SCR */
struct UartConfig {
	unsigned char	prescaler;
	unsigned char	divisor;
	unsigned int	scr;
};

/* uart3Open's speeds */
struct UartBaud {
	unsigned char	baud;
	unsigned char	prescaler;
	unsigned char	divisor;
};

void uart2Timeout(void);
void uart3Timeout(void);
#if CHIP_RL78
void uart4Timeout(void);

/* USS-1001: UART1 (SAU0's channels 2 and 3) in UART3's place, UART4 the serial link */
struct UartPort uart1;
struct UartPort uart2;
struct UartPort uart4;
#else
struct UartPort uart2;
struct UartPort uart3;
struct UartPort uart4;
#endif
unsigned char uart2Rx[0x20];
unsigned char uart2Tx[0x20];
unsigned char uart3Rx[0x80];
unsigned char uart3Tx[0x80];

#if CHIP_RL78
static struct UartPort * const uartPorts[5] = {
	NULL, &uart1, &uart2, NULL, &uart4,
};

static void (* const uartTimeoutHandlers[5])(void) = {
	NULL, NULL, uart2Timeout, uart3Timeout, uart4Timeout,
};
#else
static struct UartPort * const uartPorts[5] = {
	NULL, NULL, &uart2, &uart3, &uart4,
};

static void (* const uartTimeoutHandlers[5])(void) = {
	NULL, NULL, uart2Timeout, uart3Timeout, NULL,
};
#endif

/* port 3's status registers are SAU0's channels 0 and 1, as port 0's (nothing here reads them) */
static const struct UartRegisters uartRegisters[5] = {
	{ (volatile unsigned int *)0xfff10, (volatile unsigned int *)0xfff12,
	  (volatile unsigned int *)0xf0100, (volatile unsigned int *)0xf0102 },
	{ (volatile unsigned int *)0xfff44, (volatile unsigned int *)0xfff46,
	  (volatile unsigned int *)0xf0104, (volatile unsigned int *)0xf0106 },
	{ (volatile unsigned int *)0xfff48, (volatile unsigned int *)0xfff4a,
	  (volatile unsigned int *)0xf0140, (volatile unsigned int *)0xf0142 },
	{ (volatile unsigned int *)0xfff14, (volatile unsigned int *)0xfff16,
	  (volatile unsigned int *)0xf0100, (volatile unsigned int *)0xf0102 },
#if CHIP_RL78
	/* SAU2 in the RL78's layout */
	{ (volatile unsigned int *)0xfff78, (volatile unsigned int *)0xfff7a,
	  (volatile unsigned int *)0xf0280, (volatile unsigned int *)0xf0282 },
#else
	{ (volatile unsigned int *)0xfff4c, (volatile unsigned int *)0xfff4e,
	  (volatile unsigned int *)0xf0200, (volatile unsigned int *)0xf0202 },
#endif
};

static const struct UartConfig uart2Config = { 4, 0x0c, 0x0097 };
/* kept from 0x01000306, when nothing reads it any more */
static const struct UartConfig uart3Config = { 1, 0x11, 0x0097 };

static const struct UartBaud uartBauds[4] = {
	{ 0, 4, 0x19 },
	{ 1, 4, 0x0c },
	{ 2, 3, 0x0c },
	{ 3, 1, 0x11 },
};

#if !FW_SINCE(0x01000306)
/* both ports' buffers, and UART3 at uart3Config's speed (before 0x01000306 nothing calls it;
   the updater's is the same) */
int uartInit(void)
{
	SAU1EN = 1;
	uart2.txBuffer = uart2Tx;
	uart2.rxBuffer = uart2Rx;
	uart2.txMask = sizeof(uart2Tx) - 1;
	uart2.rxMask = sizeof(uart2Rx) - 1;
	uart2.rxWrite = 0;
	uart2.rxRead = 0;
	uart2.txWrite = 0;
	uart2.txRead = 0;
	uart3.txBuffer = uart3Tx;
	uart3.rxBuffer = uart3Rx;
	uart3.txMask = sizeof(uart3Tx) - 1;
	uart3.rxMask = sizeof(uart3Rx) - 1;
	uart3.rxWrite = 0;
	uart3.rxRead = 0;
	uart3.txWrite = 0;
	uart3.txRead = 0;
	SPS1 = uart3Config.prescaler << 4 | uart2Config.prescaler;
	SMR12 = 0x8023;
	SMR13 = 0x8122;
	SCR12 = uart3Config.scr | 0x8000;
	SCR13 = uart3Config.scr | 0x4400;
	SDR12 = (unsigned int)uart3Config.divisor << 9;
	SDR13 = (unsigned int)uart3Config.divisor << 9;
	SOL1 = 0;
	SO1 = 0x0f0f;
	SOE1L.2 = 1;
	SIR11L = 0x07;
	SIR13L = 0x07;
	PU1.4 = 1;
	PM1.4 = 1;
	PM1.3 = 0;
	P1.3 = 1;
	NFEN0.6 = 1;
	SS1L.2 = 1;
	SS1L.3 = 1;
	STIF3 = 0;
	SRIF3 = 0;
	SREIF3 = 0;
	STMK3 = 0;
	SRMK3 = 0;
	SREMK3 = 0;
	return 0;
}
#endif

/* stops a port's sending channel and its output */
void uartStopTx(unsigned char port)
{
	unsigned char mask;

	if (port < 2) {
		mask = 1 << port * 2;
		ST0L = 0x0f & mask;
		SOE0L &= ~mask;
	} else if (port < 4) {
		mask = 1 << (port - 2) * 2;
		ST1L = 0x0f & mask;
#if FW_SINCE(0x00090804)
		SOE1L &= ~mask;
#else
		SOE1L |= ~mask;		/* sets the other outputs: fixed by 0x00090804 */
#endif
	}
}

/* stops a port's receiving channel */
void uartStopRx(unsigned char port)
{
	if (port < 2)
		ST0L = (unsigned char)(1 << port * 2 + 1) & 0x0f;
	else if (port < 4)
		ST1L = (unsigned char)(1 << (port - 2) * 2 + 1) & 0x0f;
}

/* empties a port's buffers */
void uartReset(unsigned char port)
{
	struct UartPort *p;

	p = uartPorts[port];
	if (port < 5 && p != NULL) {
		p->rxWrite = 0;
		p->rxRead = 0;
		p->txWrite = 0;
		p->txRead = 0;
	}
}

#if !CHIP_RL78
/* opens UART3 at one of uartBauds' speeds (115200 bit/s when none is baud) */
unsigned char uart3Open(unsigned char baud)
{
	unsigned char i;
	struct UartConfig config = { 1, 0x11, 0x0097 };

	for (i = 0; i < 4; i++) {
		if (uartBauds[i].baud == baud) {
			config.prescaler = uartBauds[i].prescaler;
			config.divisor = uartBauds[i].divisor;
			break;
		}
	}
	if (!SAU1EN)
		SAU1EN = 1;
	uartStopTx(3);
	uartStopRx(3);
	uart3.txBuffer = uart3Tx;
	uart3.rxBuffer = uart3Rx;
	uart3.txMask = sizeof(uart3Tx) - 1;
	uart3.rxMask = sizeof(uart3Rx) - 1;
	uart3.rxWrite = 0;
	uart3.rxRead = 0;
	uart3.txWrite = 0;
	uart3.txRead = 0;
	SPS1 = SPS1 & 0x000f | config.prescaler << 4;
	SDR12 = (unsigned int)config.divisor << 9;
	SDR13 = (unsigned int)config.divisor << 9;
	SMR12 = 0x8023;
	SMR13 = 0x8122;
	SCR12 = config.scr | 0x8000;
	SCR13 = config.scr | 0x4400;
	SOL1 &= ~0x0004;
	SOE1L.2 = 0;
	SIR13L = 0x07;
	SO1 |= 0x0c0c;
	SOE1L.2 = 1;
	PM1.4 = 1;
	P1.3 = 1;
	PM1.3 = 0;
	NFEN0.6 = 1;
	STIF3 = 0;
	SRIF3 = 0;
	SREIF3 = 0;
	STMK3 = 0;
	SRMK3 = 0;
	SREMK3 = 0;
	SS1L.2 = 1;
	SS1L.3 = 1;
	uart3.flags = UART_OPEN;
	return 0;
}
#endif

/* takes up to size bytes received; their count */
unsigned char uartRead(unsigned char port, unsigned char *buf, unsigned char size)
{
	unsigned char length;
	unsigned char count;
	unsigned char i;
	struct UartPort *p;

	p = uartPorts[port];
	if (port >= 5 || p == NULL)
		return 0;
	dis_dsp();
	count = p->rxWrite - p->rxRead & p->rxMask;
	length = count < size ? count : size;
	for (i = 0; i < length; i++) {
		*buf++ = p->rxBuffer[p->rxRead];
		p->rxRead = p->rxRead + 1 & p->rxMask;
	}
	ena_dsp();
	return length;
}

/* queues up to size bytes to send, and starts sending; their count */
unsigned char uartWrite(unsigned char port, unsigned char *buf, unsigned char size)
{
	unsigned char length;
	unsigned char space;
	unsigned char i;
	struct UartPort *p;
	unsigned char data;

	p = uartPorts[port];
	if (port >= 5 || p == NULL)
		return 0;
	if (!(p->flags & UART_OPEN))
		return 0;
	dis_dsp();
	space = p->txRead - 1 - p->txWrite & p->txMask;
	length = space < size ? space : size;
	for (i = 0; i < length; i++) {
		p->txBuffer[p->txWrite] = *buf++;
		p->txWrite = p->txWrite + 1 & p->txMask;
	}
	if (!(p->flags & UART_SENDING)) {
		p->flags |= UART_SENDING;
		data = p->txBuffer[p->txRead];
		p->txRead = p->txRead + 1 & p->txMask;
		*uartRegisters[port].tx = data;
	}
	ena_dsp();
	return length;
}

/* the count of bytes received */
unsigned char uartReceived(unsigned char port)
{
	struct UartPort *p;

	p = uartPorts[port];
	if (port >= 5 || p == NULL)
		return 0;
	return p->rxWrite - p->rxRead & p->rxMask;
}

/* drops what is received and what is still to send */
int uartFlush(unsigned char port)
{
	struct UartPort *p;

	p = uartPorts[port];
	if (port >= 5 || p == NULL)
		return 0;
	DI();
	p->rxRead = p->rxWrite;
	p->txRead = p->txWrite;
	EI();
	return 0;
}

/* sets a port's timeout (NULL: none) */
unsigned char uartSetTimeout(unsigned char port, struct UartTimeout *timeout)
{
	struct TimerHandler handler;
	struct UartPort *p;
	unsigned char result;

	if (port >= 5)
		return 1;
	if ((p = uartPorts[port]) == NULL)
		return 1;
	if (timeout != NULL) {
		p->timeout = *timeout;
		handler.handler = uartTimeoutHandlers[port];
		result = timerSetHandler(timeout->timer, &handler);
	} else {
		p->timeout.timeout = 0;
		p->timeout.flgid = 0;
		p->timeout.pattern = 0;
		result = 0;
	}
	return result;
}

void uart2Timeout(void)
{
	set_flg(uart2.timeout.flgid, uart2.timeout.pattern);
}

#if CHIP_RL78
/* no UART3 */
void uart3Timeout(void)
{
}

void uart4Timeout(void)
{
	set_flg(uart4.timeout.flgid, uart4.timeout.pattern);
}
#else
void uart3Timeout(void)
{
	set_flg(uart3.timeout.flgid, uart3.timeout.pattern);
}
#endif

/* opens UART2 at uart2Config's speed */
int uart2Open(void)
{
#if !BOARD_BBMC
	/* no modem on UART2 */
	return 0;
#else
	if (!SAU1EN)
		SAU1EN = 1;
#if FW_SINCE(0x01000104)
	if (uart2.flags & UART_OPEN)
		return 1;
#endif
	if (uart2.flags & UART_CLOSED) {
		uartStopTx(2);
		uartStopRx(2);
	}
	uart2.txBuffer = uart2Tx;
	uart2.rxBuffer = uart2Rx;
	uart2.txMask = sizeof(uart2Tx) - 1;
	uart2.rxMask = sizeof(uart2Rx) - 1;
	uart2.rxWrite = 0;
	uart2.rxRead = 0;
	uart2.txWrite = 0;
	uart2.txRead = 0;
	SPS1 = SPS1 & 0x00f0 | uart2Config.prescaler;
	SMR10 = 0x0023;
	SMR11 = 0x0122;
	SCR10 = uart2Config.scr | 0x8000;
	SCR11 = uart2Config.scr | 0x4400;
	SDR10 = (unsigned int)uart2Config.divisor << 9;
	SDR11 = (unsigned int)uart2Config.divisor << 9;
	SOL1 &= ~0x0001;
	SOE1L.0 = 0;
	SIR11L = 0x07;
	SO1 |= 0x0c09;
	SOE1L.0 = 1;
	PM14.3 = 1;
	PM14.4 = 0;
	P14.4 = 1;
	NFEN0.4 = 1;
	PM36 = 1;
	SS1L.0 = 1;
	SS1L.1 = 1;
	STIF2 = 0;
	SRIF2 = 0;
	SREIF2 = 0;
	STMK2 = 0;
	SRMK2 = 0;
	SREMK2 = 0;
#if HW_IS(0x00102603) && !FW_SINCE(0x00090206)
	uart2.flags = UART_OPEN;	/* 0x00102603's 0x00090009 drops the closed flag */
#else
	uart2.flags |= UART_OPEN;
#endif
	return 0;
#endif
}

#if !FW_SINCE(0x00090804)
/* closes UART2 and its pins; SAU1 is enabled (not disabled) when channel 2's output is off */
int uart2Close(void)
{
#if BOARD_BBMC
	if (uart2.flags & UART_OPEN) {
		uartStopTx(2);
		uartStopRx(2);
		SS1L.0 = 0;
		SS1L.1 = 0;
#if HW_IS(0x00102603) && !FW_SINCE(0x00090206)
		P14.3 = 1;	/* 0x00102603's 0x00090009: P14.3 an output, high */
		PM14.3 = 0;
#endif
		P14.4 = 0;
		SOE1L.0 = 0;
		P36 = 0;
		PM36 = 0;
		if (!SOE1L.2)
			SAU1EN = 1;
		uart2.flags = (uart2.flags & ~UART_OPEN) | UART_CLOSED;
	}
#endif
	return 0;	/* no modem on UART2 */
}
#else
/* closes UART2; its pins stay up when keep is set, else they go low */
int uart2Close(unsigned char keep)
{
#if !BOARD_BBMC
	return 0;
#else
	if (uart2.flags & UART_OPEN) {
#if FW_SINCE(0x0100010B)
		STMK2 = 1;
		SRMK2 = 1;
		SREMK2 = 1;
#endif
		uartStopTx(2);
		uartStopRx(2);
		SS1L.0 = 0;
		SS1L.1 = 0;
		if (keep) {
			P14.4 = 1;
			PM36 = 1;
		} else {
			P14.3 = 0;
			PM14.3 = 0;
			P14.4 = 0;
			P36 = 0;
			PM36 = 0;
		}
		SOE1L.0 = 0;
		if (!SOE1L.2)
			SAU1EN = 0;
#if FW_SINCE(0x0100010B)
		uart2.flags = UART_CLOSED;
#else
		uart2.flags = (uart2.flags & ~UART_OPEN) | UART_CLOSED;
#endif
	}
#if FW_SINCE(0x01000104)
	else if (!keep) {
		P14.3 = 0;
		PM14.3 = 0;
		P14.4 = 0;
		P36 = 0;
		PM36 = 0;
	}
#endif
	return 0;
#endif
}
#endif

#if CHIP_RL78
/* UART1, as app/uart4.c's UART4 on the other models: a reception error cleared */
UART1_INTERRUPT void uart1ErrorInterrupt(void)
{
	unsigned char status;

	status = SDR03;	/* the byte in error, dropped */
	status = SSR03;
	SIR03 = status & 0x07;
}

/* a byte received restarts timer 0x0f */
UART1_INTERRUPT void uart1ReceiveInterrupt(void)
{
	unsigned char data;

	data = SDR03;
	if ((uart1.rxWrite + 1 & uart1.rxMask) != uart1.rxRead) {
		uart1.rxBuffer[uart1.rxWrite] = data;
		uart1.rxWrite = uart1.rxWrite + 1 & uart1.rxMask;
		timerStart(TIMER(0x0f), 2);
	}
}

/* the next byte queued, or, with none left, event 0x0200 of flag 3 */
UART1_INTERRUPT void uart1SendInterrupt(void)
{
	struct UartPort *p = &uart1;

	if (p->txWrite - p->txRead & p->txMask) {
		SDR02 = p->txBuffer[p->txRead];
		p->txRead = p->txRead + 1 & p->txMask;
	} else
		task2SetFlag(3, 0x0200);
}
#else
/* a reception error: cleared, and on an overrun the channel restarts */
void uart3ErrorInterrupt(void)
{
	unsigned char status;

	status = SDR13;	/* the byte in error, dropped */
	status = SSR13;
	SIR13 = status & 0x07;
	if (status & 0x04) {
		ST1L.3 = 1;
		SS1L.3 = 1;
	}
}

void uart3ReceiveInterrupt(void)
{
	struct UartPort *p;
	unsigned char data;

	p = &uart3;
	data = SDR13;
	if ((p->rxWrite + 1 & p->rxMask) != p->rxRead) {
		p->rxBuffer[p->rxWrite] = data;
		p->rxWrite = p->rxWrite + 1 & p->rxMask;
		if (serial.mode == 3) {
			if (p->timeout.flgid != 0)
				timerStart(p->timeout.timer, p->timeout.timeout);
		} else
			timerStart(TIMER(0x10), 2);
	}
}

void uart3SendInterrupt(void)
{
	struct UartPort *p;

	p = &uart3;
	if (p->txWrite - p->txRead & p->txMask) {
		SDR12 = p->txBuffer[p->txRead];
		p->txRead = p->txRead + 1 & p->txMask;
	} else {
		p->flags &= ~UART_SENDING;
		if (serial.mode != 3)
			task2SetFlag(3, 2);
	}
}
#endif

void uart2ErrorInterrupt(void)
{
	unsigned char status;

	status = SDR11;	/* the byte in error, dropped */
	status = SSR11;
	SIR11 = status & 0x07;
	if (status & 0x04) {
		ST1L.1 = 1;
		SS1L.1 = 1;
	}
}

void uart2ReceiveInterrupt(void)
{
#if BOARD_USS1001 && !FW_SINCE(0x01030511)
	struct UartPort *p = &uart2;	/* 0x01030505: the page through @SEGAX */
#else
	struct UartPort *p;

	p = &uart2;
#endif
	if ((p->rxWrite + 1 & p->rxMask) != p->rxRead) {
		p->rxBuffer[p->rxWrite] = (unsigned char)SDR11;
		p->rxWrite = p->rxWrite + 1 & p->rxMask;
		if (p->timeout.flgid != 0)
			timerStart(p->timeout.timer, p->timeout.timeout);
	}
}

void uart2SendInterrupt(void)
{
#if BOARD_USS1001 && !FW_SINCE(0x01030511)
	struct UartPort *p = &uart2;	/* 0x01030505: the page through @SEGAX */
#else
	struct UartPort *p;

	p = &uart2;
#endif
	if (p->txWrite - p->txRead & p->txMask) {
		SDR10 = p->txBuffer[p->txRead];
		p->txRead = p->txRead + 1 & p->txMask;
	} else
		p->flags &= ~UART_SENDING;
}
