/*
 * The touch panels (app/touch.c): two maXTouch controllers on the CSI10 link (app/csi10.c),
 * whose registers are addressed through each controller's object table. Each frame holds both
 * panels' reports, which the host reads through streams 0x0300-0x0302.
 */
#ifndef ERNIE_TOUCH_H
#define ERNIE_TOUCH_H

#include "ernie.h"
#include "board.h"

/* the slim's touch lines (in a file with #pragma sfr): panel 0's select, the panels' presence
   and the mode pin; DOL-1001 has none (boardDummyPin) */
/* USS-1001's rear panel is on IICA1 (iica1.c), DOL-1001's panel 1 on the SPI link as the fat
   models' */
#define TOUCH_PANEL1_IICA1	BOARD_USS1001

#if BOARD_DOL1001
#define TOUCH_SELECT0		boardDummyPin
#define TOUCH_SELECT1		boardDummyPin
#define TOUCH_PRESENT0		boardDummyPin
#define TOUCH_PRESENT1		boardDummyPin
#define TOUCH_MODE_PIN		boardDummyPin
#else
#define TOUCH_SELECT0		P12.6
#define TOUCH_PRESENT0		P4.1
#define TOUCH_PRESENT1		P4.2
#define TOUCH_MODE_PIN		P3.2
#endif

#define TOUCH_FRAMES		2

/* a panel's part of a frame: its controller's report, then bytes of the touch module's
   (0x00090804: the report alone, Touch.state after both parts) */
#if FW_SINCE(0x00090903)
#define TOUCH_REPORT_SIZE	0x34
#else
#define TOUCH_REPORT_SIZE	0x3c	/* the streams then write over its bytes 0x34-0x36 */
#endif
#define TOUCH_PANEL_AGE		0x34	/* the frame's age when its stream sent it (TCR02 counts
					   down), least significant byte first */
#define TOUCH_PANEL_STATE	0x36	/* Touch.state when its command went out */
#if FW_SINCE(0x00090903)
#define TOUCH_PANEL_STATUS	0x37	/* its status (TOUCH_STATUS0, TOUCH_STATUS1) */
#endif
#define TOUCH_PANEL_SIZE	0x3c

/* a frame: panel 0's part, panel 1's, and TCR02 at each one's command */
struct TouchFrame {
	unsigned char	panel[2][TOUCH_PANEL_SIZE];
	unsigned int	time0;
	unsigned int	time1;
#if FW_SINCE(0x00090804) && !FW_SINCE(0x00090903)
	unsigned char	state0;		/* Touch.state at panel 0's command */
	unsigned char	state1;		/* and at panel 1's */
#endif
};

/* Touch.state at panel p's command, in frame f (0x00090206 keeps none) */
#if FW_SINCE(0x00090903)
#define TOUCH_FRAME_STATE(f, p)	touchFrames[f].panel[p][TOUCH_PANEL_STATE]
#else
#define TOUCH_FRAME_STATE(f, p)	touchFrames[f].state##p
#endif

/* a controller's information block, at its address 0: family, variant, firmware version and
   build, matrix size X and Y, then the object table's entries */
#define TOUCH_INFO_SIZE		7
#define TOUCH_INFO_VERSION	2
#define TOUCH_INFO_BUILD	3
#define TOUCH_INFO_OBJECTS	6
#define TOUCH_OBJECTS_MAX	0x2e	/* USS-1001 reads no longer object table */

/* an entry of the object table, which follows the information block */
struct TouchObject {
	unsigned char	type;		/* the object's number, Tn */
	unsigned char	addressLow;
	unsigned char	addressHigh;
	unsigned char	size;		/* less one */
	unsigned char	instances;	/* less one */
	unsigned char	reportIds;
};

/* the registers the module uses, from the object table (report and status already shifted
   for the link, bit 0 the read bit); 0x00080504 reads no object table: its three are fixed by
   the controller's firmware version (touchIdentify0) */
struct TouchObjects {
	unsigned int	report;		/* T34's byte 0x0e: what a frame reads */
	unsigned int	numTouch;	/* T9's NUMTOUCH (byte 14): the touches reported */
	unsigned int	reset;		/* T6 (command processor): its RESET */
#if FW_SINCE(0x00090009)
	unsigned int	backup;		/* T6's BACKUPNV */
	unsigned int	power;		/* T7 (power configuration) */
#endif
#if FW_SINCE(0x00090804)	/* 0x00090206 keeps only the five above */
	unsigned int	selfTest;	/* T25 */
	unsigned int	userData;	/* T38 */
	unsigned int	t34;
	unsigned int	noise;		/* T22 (noise suppression) */
	unsigned int	touchScreen;	/* T9 (multiple touch screen) */
	unsigned int	t51;
#endif
#if FW_SINCE(0x00090903)
#if !SOURCE_0103
	unsigned int	status;		/* T34's byte 0x54: read after a frame's report */
#endif
	unsigned int	grip;		/* T20 (grip and face suppression) */
#endif
#if FW_SINCE(0x01000109)
	unsigned int	cte;		/* T28 (CTE configuration) */
#endif
#if SOURCE_0103
	unsigned int	comms;		/* T18 (communications configuration) */
#endif
};

#if SOURCE_0103
/* touchReadObjects' record of the objects a table had, by the controller's family and variant:
   all of them for 0x800a; the others need no T20, T22 or T28 */
#define TOUCH_FOUND_RESET	0x0001
#define TOUCH_FOUND_POWER	0x0002
#define TOUCH_FOUND_TOUCH	0x0004
#define TOUCH_FOUND_GRIP	0x0008
#define TOUCH_FOUND_NOISE	0x0010
#define TOUCH_FOUND_SELF_TEST	0x0020
#define TOUCH_FOUND_CTE		0x0040
#define TOUCH_FOUND_REPORT	0x0080
#define TOUCH_FOUND_USER_DATA	0x0100
#define TOUCH_FOUND_T51		0x0200
#define TOUCH_FOUND_COMMS	0x0400
#define TOUCH_FOUND_ALL		0x07ff
#define TOUCH_FOUND_REAR	0x07a7
#endif

/* a set of the controller's settings (touchSetMode) */
struct TouchMode {
	unsigned char	t51;		/* T51's byte 3 */
	unsigned char	threshold;	/* T9's TCHTHR (byte 7) */
	unsigned char	hysteresis;	/* T9's byte 31 */
	unsigned char	noiseThreshold;	/* T22's NOISETHR (byte 8) */
	unsigned char	mergeThreshold;	/* T9's MRGTHR (byte 16) */
#if FW_SINCE(0x01000109)
	unsigned char	frequency[4];	/* T22's bytes 12-15 */
	unsigned char	gcafDepth;	/* T28's ACTVGCAFDEPTH (byte 4) */
#endif
#if !FW_SINCE(0x00090903)
	unsigned char	reserved;
#endif
};

/* the modes a panel keeps (before 0x01000109 two, mode 1 from the user data), and the bytes of
   a mode the user data holds */
#if FW_SINCE(0x01000109)
#define TOUCH_MODES		3
#define TOUCH_MODE_SIZE		10
#else
#define TOUCH_MODES		2
#define TOUCH_MODE_SIZE		5
#endif

/* a panel's controller: mode 0 is what the controller had, modes 1 and 2 come from its user
   data */
struct TouchPanel {
	unsigned char	family;		/* the information block's first bytes (0xff: none) */
	unsigned char	variant;
	unsigned char	version;
	unsigned char	build;
#if FW_SINCE(0x01000104)
	unsigned int	configVersion;	/* T38's first two bytes, big-endian */
#endif
#if FW_SINCE(0x0100060B)
	unsigned int	gripEdges[2];	/* T20's bytes 1-4 (XLOGRIP, XHIGRIP, YLOGRIP, YHIGRIP),
					   as touchReadGripEdges last read them */
#endif
	struct TouchObjects objects;
#if FW_SINCE(0x00090804)	/* 0x00090206 has no modes */
	struct TouchMode modes[TOUCH_MODES];
#endif
#if SOURCE_0103
	unsigned char	objectsUnread:1;	/* touchIdentify could not read the object table: read it again */
	unsigned char	modesUnread:1;	/* a read of the modes failed: touchReadConfigVersion reads
					   them again */
#endif
};

/* Touch.flags: which panels a frame reads, and its end */
#define TOUCH_PANEL0		0x01
#define TOUCH_PANEL1		0x02
#define TOUCH_STATUS0		0x04	/* panel 0's status too */
#define TOUCH_STATUS1		0x08	/* panel 1's status too */
#define TOUCH_BUSY		0x10	/* a command has the link: no frame starts */
#define TOUCH_RUNNING		0x20	/* USS-1001: a frame is being read */
#define TOUCH_FRAME		0x80	/* a frame is complete */

/* Touch.state, copied into each panel's part of a frame */
union TouchState {
	unsigned char	value;
	struct {
#if FW_SINCE(0x01000109)
		unsigned char	mode:2;		/* the panels' settings (touchSetMode) */
		unsigned char	reserved:2;
#else
		unsigned char	mode:1;
		unsigned char	reserved:3;
#endif
		unsigned char	input0:1;	/* input1Bit0Active() */
		unsigned char	input5:1;	/* input1Bit5Active() */
		unsigned char	input3:1;	/* input1Bit3Active() */
		unsigned char	power:1;	/* AC or charging (PCH-1000; 1 on the others) */
	} bits;
};

/* the touch module's state */
struct Touch {
	unsigned char	flags;			/* TOUCH_* */
	unsigned char	frame;			/* the frame being filled */
	unsigned char	setting[2];		/* each panel's, 1-8, from the host (8 at start) */
	unsigned char	rate;			/* the frame rate TDR04 runs (0: none) */
	union TouchState state;
	struct TouchPanel panel[2];
};

/* where a controller's bootloader answers, in place of the objects; what the host sends it is
   gathered in touchBuffer */
#define TOUCH_BOOTLOADER	0x7f80
#define TOUCH_BUFFER_SIZE	0x114

struct TouchBuffer {
	unsigned int	frame;			/* the host's number for the frame gathered */
	unsigned int	end;			/* where the data ends */
	unsigned char	*data;
};

/* the link's delays, in NOPs (from the settings block; since 0x01000306) */
struct TouchDelays {
	unsigned char	select;			/* after a chip select */
	unsigned char	byte;			/* after each byte */
	unsigned char	deselect;		/* before a deselect */
#if SOURCE_0103
	unsigned char	writeByte;		/* after each byte written */
	unsigned char	release;		/* after a deselect */
#endif
};

/* what goes to a panel and what comes back: an address in it (bit 0 set: a read), then the
   data written or read there */
struct TouchPacket {
	unsigned int	address;
#if SOURCE_0103
	unsigned char	data[0x114];
#else
	unsigned char	data[0x112];
#endif
};

#define TOUCH_READ		0x0001
#define TOUCH_STATUS_LENGTH	3	/* a status read: the address, then the status */

#if SOURCE_0103
/*
 * The rear panel's report, read on IICA1 into the frame's panel 1: its first touch (4 bytes)
 * and status come at TOUCH_REAR_REPORT_OLD for the controller's version 9 before build 0x12,
 * at TOUCH_REAR_REPORT otherwise, and TOUCH_REAR_LENGTH_OLD or TOUCH_REAR_LENGTH bytes are read.
 */
#define TOUCH_REAR_REPORT_OLD	0x0c
#define TOUCH_REAR_REPORT	0x18
#define TOUCH_REAR_LENGTH_OLD	0x12
#define TOUCH_REAR_LENGTH	0x1e
#define TOUCH_REAR_TOUCH	0x30
#define TOUCH_REAR_TOUCH_SIZE	4

extern unsigned char touchRearStatus;	/* the frame's IICA1 transfer: 0 when it is done */
#endif
extern struct TouchFrame touchFrames[TOUCH_FRAMES];
extern struct TouchPacket touchCommand;
extern struct TouchPacket touchReply;
extern struct Touch touch;
extern struct TouchBuffer touchBuffer;
#if FW_SINCE(0x01000306)
extern struct TouchDelays touchDelays;
#endif

/* the link's delays, in NOPs: the settings block's since 0x01000306, fixed before, with no
   delay before a deselect */
#if FW_SINCE(0x01000306)
#define TOUCH_DELAY_SELECT	touchDelays.select
#define TOUCH_DELAY_BYTE	touchDelays.byte
#else
#define TOUCH_DELAY_SELECT	40
#define TOUCH_DELAY_BYTE	3
#endif

extern void touchClearPanels(void);
extern void touchOpen(void);
extern void touchClose(void);
extern void touchClearFrames0(void);
extern void touchClearFrames1(void);
extern void touchStartFrame(void);
#if !FW_SINCE(0x00090009)
extern void touchIdentify0(void);
extern void touchIdentify1(void);
#endif
extern void touchStartFrameNoLock(void);
extern void touchEnable0(void);
extern void touchDisable0(void);
extern void touchEnable1(void);
extern void touchDisable1(void);
#if FW_SINCE(0x01000306)
extern unsigned char touchSpiWrite(unsigned int address, unsigned char *data,
				   unsigned int length, unsigned char panel);
extern unsigned char touchSpiRead(unsigned int address, unsigned char *data,
				  unsigned int length, unsigned char panel);
extern unsigned char touchWriteVerified(unsigned int address, unsigned char *data,
					unsigned int length, unsigned char panel);
#endif
extern unsigned char touchWrite(unsigned int address, unsigned char *data, unsigned int length,
				unsigned char panel);
extern unsigned char touchRead(unsigned int address, unsigned char *data, unsigned int length,
			       unsigned char panel);
#if SOURCE_0103
extern unsigned char touchIdentify0(void);
extern unsigned char touchIdentify1(void);
#else
extern void touchIdentify0(void);
extern void touchIdentify1(void);
#endif
extern unsigned char touchBootloaderStatus0(unsigned char *status);
extern unsigned char touchBootloaderStatus1(unsigned char *status);
extern unsigned char touchEnterBootloader0(void);
extern unsigned char touchEnterBootloader1(void);
extern unsigned char touchBootloaderUnlock0(void);
extern unsigned char touchBootloaderUnlock1(void);
extern unsigned char touchBackup0(void);
extern unsigned char touchBackupState0(void);
extern unsigned char touchBackup1(void);
extern unsigned char touchBackupState1(void);
extern unsigned char touchReset0(void);
extern unsigned char touchReset1(void);
extern unsigned char touchSetPower0(unsigned char interval);
extern unsigned char touchSetPower1(unsigned char interval);
extern unsigned char touchReadConfigVersion0(unsigned char *data);
extern unsigned char touchReadConfigVersion1(unsigned char *data);
extern unsigned char touchReadUserData0(unsigned char *data);
extern unsigned char touchReadUserData1(unsigned char *data);
extern unsigned char touchSelfTest0(unsigned char test, unsigned char *pass);
extern unsigned char touchSelfTest1(unsigned char test, unsigned char *pass);
extern void touchDelaySelect(void);
#if SOURCE_0103
extern unsigned char touchReadObjects(struct TouchObjects *objects, unsigned char count,
				      unsigned char panel);
#else
extern void touchReadObjects(struct TouchObjects *objects, unsigned char count,
			     unsigned char panel);
#endif
extern void touchSetMode(unsigned char mode);
#if SOURCE_0103
#if !BOARD_DOL1001
extern void touchSetModePin(unsigned char mode);
#endif
extern unsigned char touchSetT51Byte4_0(unsigned char value);
extern unsigned char touchSetT51Byte4_1(unsigned char value);
extern unsigned char touchReadT34Bytes0(unsigned char *data);
extern unsigned char touchReadT34Bytes1(unsigned char *data);
extern unsigned char touchSetT51Bytes5_0(unsigned char *data);
extern unsigned char touchSetT51Bytes5_1(unsigned char *data);
extern unsigned char touchSetT51Bytes9_0(unsigned char *data);
extern unsigned char touchSetT51Bytes9_1(unsigned char *data);
extern unsigned char touchSetComms(unsigned char value, unsigned char panel);
extern unsigned char touchReadT34Bit3(unsigned char *set, unsigned char panel);
extern unsigned char touchTestModePin(unsigned char *out);
extern unsigned char touchBootloaderWrite(unsigned char *data, unsigned int length,
					  unsigned char panel);
extern unsigned char touchBootloaderRead(unsigned char *data, unsigned int length,
					 unsigned char panel);
extern void touchClearMode(void);
#endif
#if SOURCE_0103
extern unsigned char touchReadModes0(void);
extern unsigned char touchReadModes1(void);
#else
extern void touchReadModes0(void);
extern void touchReadModes1(void);
#endif
extern void touchUpdateState(void);
#if FW_SINCE(0x00090903)
extern unsigned char touchStatusOn0(void);
extern unsigned char touchStatusOff0(void);
extern unsigned char touchStatusOn1(void);
extern unsigned char touchStatusOff1(void);
extern unsigned char touchSetGrip0(unsigned char *data);
extern unsigned char touchSetGrip1(unsigned char *data);
#endif
#if FW_SINCE(0x0100060B)
extern unsigned char touchReadGripEdges0(unsigned char *data);
extern unsigned char touchReadGripEdges1(unsigned char *data);
extern void touchGetReportIds(unsigned char *ids);
#endif
#if FW_SINCE(0x01000306)
extern void touchSetFlags(unsigned char flags);
extern void touchClearFlags(unsigned char flags);
#else
/* before 0x01000306 a command does not take the link from the frames (no TOUCH_BUSY), the
   flags change in place, and a write is not read back */
#define touchSetFlags(bits)		(touch.flags |= (bits))
#define touchClearFlags(bits)		(touch.flags &= ~(bits))
#define touchWriteVerified		touchWrite
#endif

#endif
