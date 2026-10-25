/*
 * UART4 (app/uart4.c): SAU2's channels 0 and 1, through uart4's ring buffers.
 */
#ifndef ERNIE_UART4_H
#define ERNIE_UART4_H

extern int uart4Open(unsigned char baud);
extern void uart4StopTx(void);
extern void uart4StopRx(void);
extern void uart4Disable(void);
extern void uart4Close(void);
extern unsigned char uart4Read(unsigned char *buf, unsigned char size);
extern unsigned char uart4Write(unsigned char *buf, unsigned char size);

#endif
