/*
 * What depends on the board (app/board.c): the counts of the Elmo mask steps the power
 * sequences take, the multi-connector's output switch (P2.2 and P13.0, the host's output to
 * the connector or to a dongle: sceSysconCtrlHostOutputViaDongle), and the ports' first state.
 */
#ifndef ERNIE_BOARD_H
#define ERNIE_BOARD_H

#include "ernie.h"

/* CEM-3000NE2 (0x00404600, and 0x00404800, whose image is the same): PCH-1000's code (its
   ERNIE_BOARD), with what it keeps of the DEM boards' */
#define BOARD_CEM3000NE2	(HW_IS(0x00404600) || HW_IS(0x00404800))

/* the IRS-1001 boards (0x006xxxxx): PCH-1000's code (its ERNIE_BOARD) for a board with another
   Elmo wiring and power steps */
#define BOARD_IRS1001		((ERNIE_HW & 0x00ff0000) == 0x00600000)

/* the slim models' USS-1001 boards (0x008xxxxx, PCH-2000): PCH-1000's code (its ERNIE_BOARD)
   above drivers of their own, on an RL78/G13 (CHIP_RL78) */
#define BOARD_USS1001		(HW_FAMILY == 0x008)

/* 0x00102603: 0x00411000's code (its ERNIE_BOARD) for a board with another Elmo wiring, ports
   and multi-connector switch; 0x00315200 (DEM-3000H) has most of it, its own differences by
   HW_IS */
#define BOARD_102603		(HW_IS(0x00102603) || HW_IS(0x00315200))

/* input 1's bit 7 and the touch panels' CHG line: P14.5 and P7.5, on IRS-1001 (but 0x00601000)
   the other way round */
#if BOARD_IRS1001 && !HW_IS(0x00601000)
#define BOARD_INPUT1_BIT7_PIN	P7.5
#define BOARD_TOUCH_CHG_PIN	P14.5
#elif BOARD_USS1001
#define BOARD_INPUT1_BIT7_PIN	P14.5
#define BOARD_TOUCH_CHG_PIN	P12.5	/* the slim's panel 0 */
#else
#define BOARD_INPUT1_BIT7_PIN	P14.5
#define BOARD_TOUCH_CHG_PIN	P7.5
#endif

/* the rear panel's (panel 1's) CHG line */
#if BOARD_USS1001
#define BOARD_TOUCH_CHG1_PIN	P6.7
#else
#define BOARD_TOUCH_CHG1_PIN	P6.4
#endif

/* P9.2 and P1.5 cleared with P9.1 by a resume's other requests, a shutdown and a cut: not on
   0x00603000 and 0x00603200, which clear them only for an off */
#define BOARD_POWER_P92_P15	(!HW_IS(0x00603000) && !HW_IS(0x00603200))

/* sleepFactorGetOffInfo's bits 0 and 1 from the settings' offInfoBit0 and offInfoBit1, and bit 2
   always: not on 0x00603200, whose bits 0 and 1 are offInfo2's and offInfo3's, bit 2 never */
#define BOARD_OFF_INFO_BITS	(!HW_IS(0x00603200))

/* the off and suspend states wake by INTP1 and INTP13 (and INTP11), the inputs scanned in mode 2:
   not on 0x00601000, whose inputs keep scanning in mode 1 */
#define BOARD_OFF_WAKE		(!HW_IS(0x00601000) && (!BOARD_102603 || HW_IS(0x00315200)))

/* the probe pins show the power state (debugPinReserved: 1 off, 2 suspend, 3 resume, else 0) and
   a resume's mode (debugPinCode), and P4.0 toggles at each of task 2's ticks (not task 3's scans):
   0x00601000's */
#define BOARD_DEBUG_PINS_POWER	HW_IS(0x00601000)

/* the Elmo mask steps of each power sequence (app/powerseq.c's tables) */
#define BOARD_POWER_STEPS	(BOARD_IRS1001 || BOARD_102603 || BOARD_USS1001 ? 4 : 3)

/* the multi-connector's switch: the output's bits 0 and 1 are P13.0 and P2.2, set at once
   (0x00102603); the others' a mode, with a wait between the two pins */
#define BOARD_MULTICN_BITS	BOARD_102603

/* the multi-connector's level that is a dongle (analogGetMultiCn) */
#define BOARD_MULTICN_DONGLE_ID	(BOARD_102603 ? 0x51 : 2)

/* the host's output to the multi-connector (P2.2): not on DEM-3000K and 0x00411000, which
   switch only the dongle's (P13.0) */
#define BOARD_HOST_OUTPUT	(ERNIE_BOARD != 0x00414000 && !BOARD_IS(0x00411000))

/* the power LED: DEM-3000K's and 0x00411000's is the Elmo's LED register's bit 15 (powerLedSet) for a suspend
   and sceSysconCtrlLED's 0x40, which goes on to the other LEDs' bits; the others' the LED
   driver's (powerLedSetMode) */
#define BOARD_POWER_LED_ELMO	(ERNIE_BOARD == 0x00414000 || BOARD_IS(0x00411000))

/* the serial commands 0x0910-0x0916 on the kernel's objects: 0x00090804's, kept by 0x00411000,
   which keeps that kernel (0x00315200 runs 0x00411000's code on DEM-3000L's kernel) */
#define BOARD_KERNEL_COMMANDS	(!FW_SINCE(0x00090903) || ERNIE_KERNEL == 0x00411000)

/* the power-on test (resume mode 0x0d, powerErrorGet) steps P2.4-P2.7 and P1.7 (not on
   0x00315200) */
#define BOARD_POWER_TEST_PINS	(BOARD_IS(0x00411000) && !HW_IS(0x00315200))

/* inputKeys.state's bit 0x0080 converts channel 8 (ANI8), taken by analogEvent (not on
   0x00315200) */
#define BOARD_ANALOG_CHANNEL8	(BOARD_IS(0x00411000) && !HW_IS(0x00315200))

/* the settings' cookieRegHigh: the Cookie and the battery at 0xa4, the Cookie's registers from
   0xa0; PCH-1000 has neither choice (the Cookie at 0xa8, the battery at 0xaa), nor have
   0x00411000 after 0x00090206, 0x00315200 and 0x00102603 before 0x00090206 */
#define BOARD_COOKIE_REG_HIGH	(ERNIE_BOARD != 0x00406000 && !HW_IS(0x00315200) && \
				 (!BOARD_IS(0x00411000) || !FW_SINCE(0x00090804)) && \
				 (!BOARD_102603 || FW_SINCE(0x00090206)))

/* the settings stand in for a battery that does not answer (its temperature, capacities, ...);
   PCH-1000 always asks the battery, and so do 0x00411000 after 0x00090206, 0x00315200 and
   0x00102603 before 0x00090206 */
#define BOARD_BATTERY_STANDIN	(ERNIE_BOARD != 0x00406000 && !HW_IS(0x00315200) && \
				 (!BOARD_IS(0x00411000) || !FW_SINCE(0x00090804)) && \
				 (!BOARD_102603 || FW_SINCE(0x00090206)))

/* the touch panels' resets (P0.5, P0.6): released at a resume, and the panels identified again
   after one (0x088f, a resume) by event flag 2's 0x0040 and 0x0080; not on PCH-1000 or PDEL */
#define BOARD_TOUCH_RESET	((!BOARD_IS(0x00406000) || BOARD_CEM3000NE2) && !BOARD_IS(0x00416000))

/* timer 0x1f, started and stopped by chargeUpdate (the settings' chargeTimer1f), and the
   charge's transition 0x16 with the AC on (offWakeUnmask22): PCH-1000's; the others keep
   chargeTimer1fStart and chargeTimer1fStop empty (0x00102603 from 0x01000109;
   not 0x00315200) */
#define BOARD_CHARGE_TIMER1F	(ERNIE_BOARD == 0x00406000 ||				\
				 BOARD_102603 && FW_SINCE(0x01000109) && !HW_IS(0x00315200))

/* the NVS's data blocks (app/nvs.c), each kept with a copy, the rest of its 16 blocks spares:
   PCH-1000's NVS holds 0xba0 bytes, the others' 0xf80 */
#define BOARD_NVS_DATA_BLOCKS	(ERNIE_BOARD == 0x00406000 ? 3 : 4)

/* the lowest NVS offset the host may read or write (app/nvsreq.c): PCH-1000 and PDEL keep their
   first 0x400 bytes from the host (the hardware ID's, not the code's: 0x00405x00 and
   0x00415200 run their code with 0) */
#define BOARD_NVS_HOST_OFFSET_MIN	(HW_IS(0x00406000) || HW_IS(0x00416000) || BOARD_USS1001 ? 0x0400 : 0)

/* the RTC's interrupt in state 0x0010 watches input 1's bit 7 (P14.5) once a shutdown is
   marked, and runs transition 0x62 when it settles low (input1Bit7Poll): PCH-1000's; the others
   keep input1Bit7Poll empty */
#define BOARD_INPUT1_BIT7_POLL	((ERNIE_BOARD == 0x00406000 || BOARD_102603 && !HW_IS(0x00315200)) && !BOARD_USS1001)

/* INTP11 (P7.7, intp11Level): not on PCH-1000 or 0x00102603, whose intp11Enable does nothing
   and whose handler is empty */
#define BOARD_INTP11		(ERNIE_BOARD != 0x00406000 && (!BOARD_102603 || HW_IS(0x00315200)))
/* and its level kept on both edges since 0x00090206; 0x00090009's INTP11 takes the rising edge
   only, holding the scan (intp11Interrupt), and intp11Low reads input group 3 */
#define BOARD_INTP11_LEVEL	(BOARD_INTP11 && FW_SINCE(0x00090206))

/* the hardware watchdog (WDTE): PCH-1000's runs, kicked by the tasks, the NVS and the
   configuration's flash writes; the others' never */
#define BOARD_WDT		(ERNIE_BOARD == 0x00406000)

/* a power-on reset clears the scratch pad under an updater older than 0x16 (getErnieDlVersion's
   bytes 1 and 2): PCH-1000's systemInit, not since 0x0100060B */
#define BOARD_DL_SCRATCHPAD	(ERNIE_BOARD == 0x00406000 && !FW_SINCE(0x0100060B))

/* the HDMI output: HDMI CEC's power (the Elmo's register 0xa1 bit 15: powerHdmiCecOn and
   Off), with 0xa2's 0x0200 switched off by the power sequences' steps; not on PCH-1000, whose
   commands 0x0886 do nothing */
#define BOARD_HDMI		(ERNIE_BOARD != 0x00406000)

/* the configuration's wake masks the board reads (ConfigBlock.wakeMasks0-2): 0x00601000's and
   0x00411000's the first set (not 0x00315200's), DEM-3000L's the second, PCH-1000's the third */
#if HW_IS(0x00601000) || BOARD_IS(0x00411000) && !HW_IS(0x00315200)
#define BOARD_WAKE_MASKS	wakeMasks0
#elif ERNIE_BOARD == 0x00406000
#define BOARD_WAKE_MASKS	wakeMasks2
#else
#define BOARD_WAKE_MASKS	wakeMasks1
#endif

/* input 3's pins (P7.3, P7.7: sampleInputs3); PCH-1000 samples none, its input 3 keeps the
   value inputInit gives it */
#define BOARD_INPUT3_PINS	(ERNIE_BOARD != 0x00406000 || HW_IS(0x00601000))

/* input 3's bits 2-4 from P2.3, P2.7 and P2.5 (sampleInputs3) */
#define BOARD_INPUT3_PORT2	(BOARD_IS(0x00411000) && !HW_IS(0x00315200))

/* 0x00102603's inputs: input 1's bit 1 (P4.7) and input 3's bits 1 (P7.2, not P7.7) and 5
   (P2.1); the wake inputs INTP2 (rising), INTP4 and INTP7 (falling) and their pin monitors 3
   (P3.1) and 2 (P14.1, a transition 3); INTP8 and pin monitor 6 only ever disabled; the
   WLAN's line on P6.7 (P3.1 an input) */
#define BOARD_INPUTS_102603	BOARD_102603
/* its own pin monitor 3 (P3.1, no WLAN line on it) from 0x01000104; before, the others' */
#define BOARD_PIN3_102603	(BOARD_INPUTS_102603 && FW_SINCE(0x01000104))
/* its WLAN's line on P6.7 (the power sequences', powerJobCut's) from 0x01000104; before, P3.1
   as the others' */
#define BOARD_WLAN_102603	(BOARD_102603 && FW_SINCE(0x01000104))

/* P9.5, driven high at a resume and low at an off or a suspend (powerseq.c): not on
   0x00102603, whose WLAN line is P6.7 and whose SD power comes first, retimed, with no RMR or
   multi-connector power */
#define BOARD_P95		(!BOARD_102603)

/* the motion sensor's ready line, read by inputreq.c's 0x0484-0x0488 and powerMotionOn: P7.2;
   0x00102603's P7.7 (its input 3 takes P7.2; 0x00315200's input 3 does not) */
#if BOARD_102603 && !HW_IS(0x00315200)
#define BOARD_MOTION_READY	P7.7
#else
#define BOARD_MOTION_READY	P7.2
#endif

/* the serial link's mode 2 (app/serial2.c: its commands, notifications and test area, P7.1,
   its request and link inputs): not on PCH-1000, whose link answers mode 2 with no data */
#define BOARD_SERIAL2		(ERNIE_BOARD != 0x00406000 || HW_IS(0x00601000))

/* the command link on UART4 (app/uart4.c, app/serial4.c): not on PCH-1000, whose serial4Event
   steps UART3's link only */
#define BOARD_SERIAL4		(ERNIE_BOARD != 0x00406000 || HW_IS(0x00601000))

/* the keys' pins on port 5 (inputKeysOpen, inputKeysClose): P5.0-P5.3; PCH-1000's P5.1 and P5.2
   (its PM5 still opened with 0x0f) */
#define BOARD_KEYS_P5		(ERNIE_BOARD == 0x00406000 ? 0x06 : 0x0f)

/* the port modes boardPortInit sets: PCH-1000 drives P7.3 and P7.7 (no input 3 pins there)
   and P9.3 */
#define BOARD_PM7		(ERNIE_BOARD == 0x00406000 ? 0x75 : BOARD_PORTS_315200 ? 0xed : 0xfd)
#define BOARD_PM9		(ERNIE_BOARD == 0x00406000 ? 0x00 : 0x08)
/* 0x00315200's values (below), also in 0x00102603's 0x00090009, where boardPortInit is not
   called (boardInit calls boardPortInit102603) */
#define BOARD_PORTS_315200	(HW_IS(0x00315200) || (HW_IS(0x00102603) && !FW_SINCE(0x00090206)))
/* and 0x00102603's own */
/* (0x00411000's 0x00090206 port modes are 0x00102603's, but for PM14; the other boards'
   0x00090206 keep its PM6 only, with PM2 0xff and PM14 0xec) */
#define BOARD_PORTS_102603		(BOARD_102603 || (!FW_SINCE(0x00090804) && BOARD_IS(0x00411000)))
#define BOARD_PM2		(BOARD_PORTS_315200 ? 0x01 : BOARD_PORTS_102603 ? 0xab : \
				 !FW_SINCE(0x00090804) ? 0xff : 0x01)
#define BOARD_PM4		(BOARD_102603 ? 0xd0 : 0x40)
#define BOARD_PM6		((BOARD_102603 && !HW_IS(0x00315200)) || !FW_SINCE(0x00090804) ? 0x12 : 0x92)
#define BOARD_PM11		(BOARD_PORTS_315200 ? 0xf0 : BOARD_102603 ? 0xf2 : 0xf1)
#define BOARD_P12		(BOARD_102603 ? 0x80 : 0x00)
#define BOARD_PM14		(BOARD_PORTS_315200 ? 0xc6 : BOARD_102603 ? 0xe6 : \
				 !FW_SINCE(0x00090804) && !BOARD_IS(0x00411000) ? 0xec : 0xe4)
/* and 0x00315200's own: P11.1 high, PM3 0x03, PM5 0x30, PM12 0x1f */
#define BOARD_PM3		(BOARD_PORTS_315200 ? 0x03 : 0x07)
#define BOARD_PM5		(BOARD_PORTS_315200 ? 0x30 : 0x00)
#define BOARD_P11		(BOARD_PORTS_315200 ? 0x02 : 0x00)
#define BOARD_PM12		(BOARD_PORTS_315200 ? 0x1f : 0x5f)
#define BOARD_PM15		(BOARD_PORTS_102603 ? 0xff : 0xf8)

/* the 3G modem (bbmc.c's events, task 6's): not on 0x00315200, whose bbmcEvent only refuses
   request 0x0a's commands (status 4) and which task 6 never calls, nor on USS-1001 */
#define BOARD_BBMC		(!HW_IS(0x00315200) && !BOARD_USS1001)

/* bbmcInit starts with the 3G modem's P3.3 pulse (0x39d ticks); PCH-1000's only drives it low */
#define BOARD_BBMC_INIT_PULSE	(ERNIE_BOARD != 0x00406000 && BOARD_BBMC)

/* the touch state's power bit and panel mode follow the charger (touchUpdateState): mode 0
   with neither power nor an input active, 2 on USB, else 1; the others set power and mode 1 */
#define BOARD_TOUCH_CHARGE	(ERNIE_BOARD == 0x00406000)

/* a hang detected by P11.5, P4.6, P5.0 and P5.3 low (hangDetected); the others detect none */
#define BOARD_HANG_PINS		(ERNIE_BOARD == 0x00406000)

/* standby may stop the clock when nothing needs it (taskStandbyHalt); the others always halt */
#define BOARD_STANDBY_STOP	(ERNIE_BOARD == 0x00406000 && !HW_IS(0x00601000))

/* the motion sensor (CSI01, its frame timer TM01, request 4 and stream 0x0400) and timer 11's
   hang interrupt: gone from the 0x0100060x builds */
#define BOARD_MOTION		(!FW_SINCE(0x0100060B))

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
#if BOARD_USS1001
extern unsigned char boardMultiCnPower(unsigned char on);
extern unsigned char boardMultiCnSetSwitch(unsigned char value);
extern unsigned char boardMultiCnSetSerial(unsigned char on);
extern void boardMultiCnUpdate(void);
#endif
#if FW_SINCE(0x00090804)
extern unsigned char boardMultiCnIsDebug(void);
#else
#define boardMultiCnIsDebug()	0	/* 0x00090206 has no debug board */
extern void boardInit(void);
#endif
extern void boardPortInit(void);
#if !FW_SINCE(0x00090206)
extern void boardPortInitA(void);
extern void boardPortInitB(void);
extern void boardPortInit102603(void);
#endif

#endif
