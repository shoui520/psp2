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
	unsigned char	flgid;
	unsigned int	pattern;
};

extern void uartStopTx(unsigned char port);
extern void uartStopRx(unsigned char port);
extern void uartReset(unsigned char port);
extern int uart3Open(unsigned char baud);
extern int uartRead(unsigned char port, unsigned char *buf, unsigned char size);
extern int uartWrite(unsigned char port, unsigned char *buf, unsigned char size);
extern int uartReceived(unsigned char port);
extern int uartFlush(unsigned char port);
extern int uartSetTimeout(unsigned char port, struct UartTimeout *timeout);
extern int uart2Open(void);
extern int uart2Close(unsigned char keep);

#endif
