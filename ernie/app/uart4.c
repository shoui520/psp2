/*
 * UART4 on SAU2 (channel 0 sends, channel 1 receives; TxD4 P9.4, RxD4 P9.3): opened at one of
 * uart4Bauds' speeds, and read and written through uart4's ring buffers.
 */
#pragma rtos_interrupt INTSRE1 uart4ErrorInterrupt
#pragma rtos_interrupt INTSR4 uart4ReceiveInterrupt
#pragma rtos_interrupt INTST4 uart4SendInterrupt
#pragma sfr

#include "ernie.h"
#include "chip.h"
#include "kernel.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "uart4.h"

/* uart4Open's speeds */
struct Uart4Baud {
	unsigned char	baud;
	unsigned char	prescaler;
	unsigned char	divisor;
	unsigned char	reserved;
};

/* a channel's speed: SPS's prescaler, SDR's divisor and the channels' SCR */
struct Uart4Config {
	unsigned char	prescaler;
	unsigned char	divisor;
	unsigned int	scr;
};

unsigned char uart4Rx[0x80];
unsigned char uart4Tx[0x80];

static const struct Uart4Baud uart4Bauds[4] = {
	{ 0, 4, 0x19 },
	{ 1, 4, 0x0c },
	{ 2, 3, 0x0c },
	{ 3, 1, 0x11 },
};

/* opens UART4 at one of uart4Bauds' speeds (38400 bit/s when none is baud) */
int uart4Open(unsigned char baud)
{
	unsigned char i;
	struct Uart4Config config = { 3, 0x0c, 0x0097 };

	for (i = 0; i < 4; i++) {
		if (uart4Bauds[i].baud == baud) {
			config.prescaler = uart4Bauds[i].prescaler;
			config.divisor = uart4Bauds[i].divisor;
			break;
		}
	}
	PM93 = 1;
	PM94 = 1;
	uart4Close();
	SAU2EN = 1;
	uart4.txBuffer = uart4Tx;
	uart4.rxBuffer = uart4Rx;
	uart4.txMask = sizeof(uart4Tx) - 1;
	uart4.rxMask = sizeof(uart4Rx) - 1;
	uart4.rxWrite = 0;
	uart4.rxRead = 0;
	uart4.txWrite = 0;
	uart4.txRead = 0;
	SPS2L = config.prescaler << 4 | config.prescaler;
	SMR20 = 0x0023;
	SMR21 = 0x0122;
	SCR20 = config.scr | 0x8000;
	SCR21 = config.scr | 0x4400;
	SDR21 = (unsigned int)config.divisor << 9;
	SDR20 = (unsigned int)config.divisor << 9;
	SOL2 = 0;
	SOE2L.0 = 0;
	SO2 = 0x0f0f;
	SOE2L.0 = 1;
	SIR20L = 0;
	SIR21L = 0x07;
	PU93 = 1;
	PM93 = 1;
	P94 = 1;
	PM94 = 0;
	NFEN0.4 = 1;
	STIF4 = 0;
	SRIF4 = 0;
	SREIF1 = 0;
	STMK4 = 0;
	SRMK4 = 0;
	SREMK1 = 0;
	SS2L.0 = 1;
	SS2L.1 = 1;
	return 0;
}

void uart4StopTx(void)
{
	ST2L = 0x01;
	SOE2L.0 = 0;
}

void uart4StopRx(void)
{
	ST2L = 0x02;
}

void uart4Disable(void)
{
	SAU2EN = 0;
}

/* stops UART4 and leaves TxD4 high */
void uart4Close(void)
{
	STMK4 = 1;
	SRMK4 = 1;
	SREMK1 = 1;
	STIF4 = 0;
	SRIF4 = 0;
	SREIF1 = 0;
	uart4StopTx();
	uart4StopRx();
	uart4Disable();
	PU93 = 1;
	PM93 = 1;
	P94 = 1;
	PM94 = 0;
}

/* takes up to size bytes received; their count */
unsigned char uart4Read(unsigned char *buf, unsigned char size)
{
	unsigned char length;
	unsigned char count;
	unsigned char i;
	struct UartPort *p = &uart4;

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

/* queues up to size bytes to send, and starts sending when the channel is idle; their count */
unsigned char uart4Write(unsigned char *buf, unsigned char size)
{
	unsigned char length;
	unsigned char space;
	unsigned char i;
	struct UartPort *p = &uart4;
	unsigned char data;

	dis_dsp();
	space = p->txRead - 1 - p->txWrite & p->txMask;
	length = space < size ? space : size;
	for (i = 0; i < length; i++) {
		p->txBuffer[p->txWrite] = *buf++;
		p->txWrite = p->txWrite + 1 & p->txMask;
	}
	if (!(SSR20 & 0x0040)) {
		data = p->txBuffer[p->txRead];
		p->txRead = p->txRead + 1 & p->txMask;
		SDR20 = data;
	}
	ena_dsp();
	return length;
}

/* a reception error: cleared (SREIF1 is UART4's error interrupt) */
void uart4ErrorInterrupt(void)
{
	unsigned char status;

	status = SDR21;	/* the byte in error, dropped */
	status = SSR21;
	SIR21 = status & 0x07;
}

void uart4ReceiveInterrupt(void)
{
	unsigned char data;

	data = SDR21;
	if ((uart4.rxWrite + 1 & uart4.rxMask) != uart4.rxRead) {
		uart4.rxBuffer[uart4.rxWrite] = data;
		uart4.rxWrite = uart4.rxWrite + 1 & uart4.rxMask;
		timerStart(0x0f, 2);
	}
}

/* the next byte queued, or, with none left, event 0x0200 of flag 3 */
void uart4SendInterrupt(void)
{
	struct UartPort *p = &uart4;

	if (p->txWrite - p->txRead & p->txMask) {
		SDR20 = p->txBuffer[p->txRead];
		p->txRead = p->txRead + 1 & p->txMask;
	} else
		task2SetFlag(3, 0x0200);
}
