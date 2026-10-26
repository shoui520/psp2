/*
 * What depends on the board (app/board.c): the counts of the Elmo mask steps the power
 * sequences take, the multi-connector's output switch (P2.2 and P13.0, the host's output to
 * the connector or to a dongle: sceSysconCtrlHostOutputViaDongle), and the ports' first state.
 */
#ifndef ERNIE_BOARD_H
#define ERNIE_BOARD_H

/* the host's output to the multi-connector (P2.2): not on DEM-3000K, which switches only the
   dongle's (P13.0) */
#define BOARD_HOST_OUTPUT	(ERNIE_HW != 0x00414000)

/* the settings' cookieRegHigh: the Cookie and the battery at 0xa4, the Cookie's registers from
   0xa0; PCH-1000 has neither choice (the Cookie at 0xa8, the battery at 0xaa) */
#define BOARD_COOKIE_REG_HIGH	(ERNIE_HW != 0x00406000)

/* the settings stand in for a battery that does not answer (its temperature, capacities, ...);
   PCH-1000 always asks the battery */
#define BOARD_BATTERY_STANDIN	(ERNIE_HW != 0x00406000)

/* the touch panels' resets (P0.5, P0.6): released at a resume, and the panels identified again
   after one (0x088f, a resume) by event flag 2's 0x0040 and 0x0080; not on PCH-1000 */
#define BOARD_TOUCH_RESET	(ERNIE_HW != 0x00406000)

/* timer 0x1f, started and stopped by chargeUpdate (the settings' chargeTimer1f), and the
   charge's transition 0x16 with the AC on (offWakeUnmask22): PCH-1000's; the others keep
   chargeTimer1fStart and chargeTimer1fStop empty */
#define BOARD_CHARGE_TIMER1F	(ERNIE_HW == 0x00406000)

/* the NVS's data blocks (app/nvs.c), each kept with a copy, the rest of its 16 blocks spares:
   PCH-1000's NVS holds 0xba0 bytes, the others' 0xf80 */
#define BOARD_NVS_DATA_BLOCKS	(ERNIE_HW == 0x00406000 ? 3 : 4)

/* the RTC's interrupt in state 0x0010 watches input 1's bit 7 (P14.5) once a shutdown is
   marked, and runs transition 0x62 when it settles low (input1Bit7Poll): PCH-1000's; the others
   keep input1Bit7Poll empty */
#define BOARD_INPUT1_BIT7_POLL	(ERNIE_HW == 0x00406000)

/* INTP11 (P7.7, intp11Level): not on PCH-1000, whose intp11Enable does nothing */
#define BOARD_INTP11		(ERNIE_HW != 0x00406000)

/* the hardware watchdog (WDTE): PCH-1000's runs, kicked by the tasks, the NVS and the
   configuration's flash writes; the others' never */
#define BOARD_WDT		(ERNIE_HW == 0x00406000)

/* the configuration's wake masks the board reads (ConfigBlock.wakeMasks0-2): DEM-3000L's the
   second set, PCH-1000's the third */
#if ERNIE_HW == 0x00406000
#define BOARD_WAKE_MASKS	wakeMasks2
#else
#define BOARD_WAKE_MASKS	wakeMasks1
#endif

/* input 3's pins (P7.3, P7.7: sampleInputs3); PCH-1000 samples none, its input 3 keeps the
   value inputInit gives it */
#define BOARD_INPUT3_PINS	(ERNIE_HW != 0x00406000)

/* the serial link's mode 2 (app/serial2.c: its commands, notifications and test area, P7.1,
   its request and link inputs): not on PCH-1000, whose link answers mode 2 with no data */
#define BOARD_SERIAL2		(ERNIE_HW != 0x00406000)

/* the command link on UART4 (app/uart4.c, app/serial4.c): not on PCH-1000, whose serial4Event
   steps UART3's link only */
#define BOARD_SERIAL4		(ERNIE_HW != 0x00406000)

/* the keys' pins on port 5 (inputKeysOpen, inputKeysClose): P5.0-P5.3; PCH-1000's P5.1 and P5.2
   (its PM5 still opened with 0x0f) */
#define BOARD_KEYS_P5		(ERNIE_HW == 0x00406000 ? 0x06 : 0x0f)

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
