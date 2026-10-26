/*
 * The UARTs, as the updater has them (an older revision of the application's): each a pair of
 * serial channels, one sending and one receiving, through two ring buffers. UART3 is set up
 * here (uartInit, uart3Open); UART2 is not, and its timeout and UART3's do nothing.
 */
#pragma interrupt INTSRE3 uart3ErrorInterrupt
#pragma interrupt INTSR3 uart3ReceiveInterrupt
#pragma interrupt INTST3 uart3SendInterrupt
#pragma sfr
#pragma DI
#pragma EI

#include <stddef.h>

#include "chip.h"
#include "timer.h"
#include "uart.h"
#include "updater.h"

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

struct UartPort uart2;
struct UartPort uart3;
struct UartPort uart4;
unsigned char uart2Rx[0x20];
unsigned char uart2Tx[0x20];
unsigned char uart3Rx[0x80];
unsigned char uart3Tx[0x80];

static struct UartPort * const __far uartPorts[5] = {
	NULL, NULL, &uart2, &uart3, &uart4,
};

static void (* const __far uartTimeoutHandlers[5])(void) = {
	NULL, NULL, uart2Timeout, uart3Timeout, NULL,
};

/* port 3's status registers are SAU0's channels 0 and 1, as port 0's (nothing here reads them) */
static const struct UartRegisters __far uartRegisters[5] = {
	{ (volatile unsigned int *)0xfff10, (volatile unsigned int *)0xfff12,
	  (volatile unsigned int *)0xf0100, (volatile unsigned int *)0xf0102 },
	{ (volatile unsigned int *)0xfff44, (volatile unsigned int *)0xfff46,
	  (volatile unsigned int *)0xf0104, (volatile unsigned int *)0xf0106 },
	{ (volatile unsigned int *)0xfff48, (volatile unsigned int *)0xfff4a,
	  (volatile unsigned int *)0xf0140, (volatile unsigned int *)0xf0142 },
	{ (volatile unsigned int *)0xfff14, (volatile unsigned int *)0xfff16,
	  (volatile unsigned int *)0xf0100, (volatile unsigned int *)0xf0102 },
	{ (volatile unsigned int *)0xfff4c, (volatile unsigned int *)0xfff4e,
	  (volatile unsigned int *)0xf0200, (volatile unsigned int *)0xf0202 },
};

static const struct UartConfig __far uart2Config = { 4, 0x0c, 0x0097 };
static const struct UartConfig __far uart3Config = { 1, 0x11, 0x0097 };

static const struct UartBaud __far uartBauds[4] = {
	{ 0, 4, 0x19 },
	{ 1, 4, 0x0c },
	{ 2, 3, 0x0c },
	{ 3, 1, 0x11 },
};

/* both ports' buffers, and UART3 at uart3Config's speed */
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
		SOE1L &= ~mask;
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
	count = p->rxWrite - p->rxRead & p->rxMask;
	length = count < size ? count : size;
	for (i = 0; i < length; i++) {
		*buf++ = p->rxBuffer[p->rxRead];
		p->rxRead = p->rxRead + 1 & p->rxMask;
	}
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

/* the timeouts do nothing in the updater */
void uart2Timeout(void)
{
}

void uart3Timeout(void)
{
}

/* UART2 is not used */
int uart2Open(void)
{
	return 0;
}

int uart2Close(unsigned char keep)
{
	return 0;
}

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

/* a byte received; with the log out, the port's timeout runs again, else timer 0x12 */
void uart3ReceiveInterrupt(void)
{
	struct UartPort *p;
	unsigned char data;

	p = &uart3;
	data = SDR13;
	if ((p->rxWrite + 1 & p->rxMask) != p->rxRead) {
		p->rxBuffer[p->rxWrite] = data;
		p->rxWrite = p->rxWrite + 1 & p->rxMask;
		if (logMode == 3) {
			if (p->timeout.flgid != 0)
				timerStart(p->timeout.timer, p->timeout.timeout);
		} else
			timerStart(0x12, 2);
	}
}

/* the next byte queued, or, with none left, event 0x33 unless the log is out */
void uart3SendInterrupt(void)
{
	struct UartPort *p;

	p = &uart3;
	if (p->txWrite - p->txRead & p->txMask) {
		SDR12 = p->txBuffer[p->txRead];
		p->txRead = p->txRead + 1 & p->txMask;
	} else {
		p->flags &= ~UART_SENDING;
		if (logMode != 3)
			updEventSet(0x33);
	}
}
