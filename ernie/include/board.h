/*
 * What depends on the board (app/board.c): the counts of the Elmo mask steps the power
 * sequences take, the multi-connector's output switch (P2.2 and P13.0, the host's output to
 * the connector or to a dongle: sceSysconCtrlHostOutputViaDongle), and the ports' first state.
 */
#ifndef ERNIE_BOARD_H
#define ERNIE_BOARD_H

/* boardSetMultiCnOutput's and boardGetMultiCnOutput's outputs */
#define BOARD_MULTICN_HOST	0	/* P13.0 low, P2.2 high */
#define BOARD_MULTICN_DONGLE	1	/* P13.0 high */
#define BOARD_MULTICN_OFF	4	/* both low (boardSetMultiCnOutput: 2-4) */

extern void boardNone(int value);
extern unsigned char boardResumeSteps(void);
extern unsigned char boardOffSteps(void);
extern unsigned char boardSuspendSteps(void);
extern unsigned char boardMultiCnOutputOff(void);
extern unsigned char boardGetMultiCnOutput(void);
extern unsigned char boardSetMultiCnOutput(unsigned char output);
extern unsigned char boardMultiCnStop(void);
extern unsigned char boardMultiCnIsDongle(void);
extern unsigned char boardMultiCnIsDebug(void);
extern void boardPortInit(void);

#endif
