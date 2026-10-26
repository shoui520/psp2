/*
 * The UARTs (app/uart.c): ports 0-4, each a sending and a receiving serial channel with a ring
 * buffer for each. UART2 and UART3 are set up; a port reads and writes by its number.
 */
#ifndef ERNIE_UART_H
#define ERNIE_UART_H

/* after each byte received, timer runs for timeout ticks, then sets pattern in event flag flgid */
struct UartTimeout {
	unsigned char	timer;
	unsigned char	timeout;
#ifdef ERNIE_UPDATER
	unsigned int	flgid;		/* the updater's a word, at the same place */
#else
	unsigned char	flgid;
#endif
	unsigned int	pattern;
};

/* a port: its buffers (each a power of two, mask its size - 1) and their indexes */
struct UartPort {
	unsigned char	*txBuffer;
	unsigned char	*rxBuffer;
	unsigned char	txMask;
	unsigned char	rxMask;
	unsigned char	rxRead;
	unsigned char	rxWrite;
	unsigned char	txRead;
	unsigned char	txWrite;
	unsigned char	flags;		/* UART_* (app/uart.c) */
	struct UartTimeout timeout;
};

extern struct UartPort uart4;

#ifdef ERNIE_UPDATER
extern int uartInit(void);
#endif
extern void uartStopTx(unsigned char port);
extern void uartStopRx(unsigned char port);
extern void uartReset(unsigned char port);
extern unsigned char uart3Open(unsigned char baud);
extern unsigned char uartRead(unsigned char port, unsigned char *buf, unsigned char size);
extern unsigned char uartWrite(unsigned char port, unsigned char *buf, unsigned char size);
extern unsigned char uartReceived(unsigned char port);
extern int uartFlush(unsigned char port);
extern unsigned char uartSetTimeout(unsigned char port, struct UartTimeout *timeout);
extern int uart2Open(void);
extern int uart2Close(unsigned char keep);

#endif
