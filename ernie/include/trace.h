/*
 * The debug trace (app/trace.c): text and bytes written to UART3 while the UART3 link is in
 * its debug mode (serial.mode 3), each under a mask of categories; a category whose bit is set
 * in traceMask is not written. The logging builds' formatted traces are compiled out
 * (tracePrintf does nothing).
 */
#ifndef ERNIE_TRACE_H
#define ERNIE_TRACE_H

#define TRACE_PORT		3	/* the UART */

extern unsigned int traceMask;

extern int traceInit(void);
extern void traceDisable(unsigned int mask);
extern void traceEnable(unsigned int mask);
extern char traceIsEnabled(unsigned int mask);
extern unsigned int traceGetMask(void);
extern unsigned char traceLength(const char *text);
extern void tracePuts(unsigned int mask, const char *text);
extern void traceWrite(unsigned int mask, unsigned char *data, unsigned char size);
extern void tracePrintf(const char *format, ...);
extern unsigned char traceUartWrite(unsigned char *data, unsigned char size);
extern unsigned char traceUartRead(unsigned char *data, unsigned char size);

#endif
