/*
 * USS-1001's (BOARD_USS1001) serial lines on the multi-connector (app/serialline.c): which of
 * the UARTs' pins are connected, a bit per line, also in SERIALSEL.
 */
#ifndef ERNIE_SERIALLINE_H
#define ERNIE_SERIALLINE_H

#define SERIAL_LINE_P3_6	0x04	/* P3.6 out, P3.7 in */
#define SERIAL_LINE_P11_3	0x08	/* P11.3 out, P11.2 in */
#define SERIAL_LINE_UART4	0x10	/* P9.2 out (TxD), P9.3 in (RxD) */
#define SERIAL_LINE_P3_5	0x20	/* P3.5 out, P3.4 in */

/* the two sets serialLineSet takes */
#define SERIAL_LINES_HOST	(SERIAL_LINE_P11_3 | SERIAL_LINE_UART4)
#define SERIAL_LINES_ALL	(SERIAL_LINE_P3_6 | SERIAL_LINES_HOST | SERIAL_LINE_P3_5)

extern unsigned char serialLines;

extern unsigned char serialLineInit(void);
extern unsigned char serialLineGet(void);
extern unsigned char serialLineSet(unsigned char lines, unsigned char on);

#endif
