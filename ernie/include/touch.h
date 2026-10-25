/*
 * The touch panels (app/touch.c): two maXTouch controllers on the CSI10 link (app/csi10.c),
 * whose registers are addressed through each controller's object table. Each frame holds both
 * panels' reports, which the host reads through streams 0x0300-0x0302.
 */
#ifndef ERNIE_TOUCH_H
#define ERNIE_TOUCH_H

#define TOUCH_FRAMES		2

/* a panel's part of a frame: its controller's report, then bytes of the touch module's */
#define TOUCH_REPORT_SIZE	0x34
#define TOUCH_PANEL_AGE		0x34	/* the frame's age when its stream sent it (TCR02 counts
					   down), least significant byte first */
#define TOUCH_PANEL_STATE	0x36	/* Touch.state when its command went out */
#define TOUCH_PANEL_STATUS	0x37	/* its status (TOUCH_STATUS0, TOUCH_STATUS1) */
#define TOUCH_PANEL_SIZE	0x3c

/* a frame: panel 0's part, panel 1's, and TCR02 at each one's command */
struct TouchFrame {
	unsigned char	panel[2][TOUCH_PANEL_SIZE];
	unsigned int	time0;
	unsigned int	time1;
};

/* a controller's information block, at its address 0: family, variant, firmware version and
   build, matrix size X and Y, then the object table's entries */
#define TOUCH_INFO_SIZE		7
#define TOUCH_INFO_VERSION	2
#define TOUCH_INFO_BUILD	3
#define TOUCH_INFO_OBJECTS	6

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
   for the link, bit 0 the read bit) */
struct TouchObjects {
	unsigned int	report;		/* T34's byte 0x0e: what a frame reads */
	unsigned int	numTouch;	/* T9's NUMTOUCH (byte 14): the touches reported */
	unsigned int	reset;		/* T6 (command processor): its RESET */
	unsigned int	backup;		/* T6's BACKUPNV */
	unsigned int	power;		/* T7 (power configuration) */
	unsigned int	selfTest;	/* T25 */
	unsigned int	userData;	/* T38 */
	unsigned int	t34;
	unsigned int	noise;		/* T22 (noise suppression) */
	unsigned int	touchScreen;	/* T9 (multiple touch screen) */
	unsigned int	t51;
	unsigned int	status;		/* T34's byte 0x54: read after a frame's report */
	unsigned int	grip;		/* T20 (grip and face suppression) */
	unsigned int	cte;		/* T28 (CTE configuration) */
};

/* a set of the controller's settings (touchSetMode) */
struct TouchMode {
	unsigned char	t51;		/* T51's byte 3 */
	unsigned char	threshold;	/* T9's TCHTHR (byte 7) */
	unsigned char	hysteresis;	/* T9's byte 31 */
	unsigned char	noiseThreshold;	/* T22's NOISETHR (byte 8) */
	unsigned char	mergeThreshold;	/* T9's MRGTHR (byte 16) */
	unsigned char	frequency[4];	/* T22's bytes 12-15 */
	unsigned char	gcafDepth;	/* T28's ACTVGCAFDEPTH (byte 4) */
};

/* a panel's controller: mode 0 is what the controller had, modes 1 and 2 come from its user
   data */
struct TouchPanel {
	unsigned char	family;		/* the information block's first bytes (0xff: none) */
	unsigned char	variant;
	unsigned char	version;
	unsigned char	build;
	unsigned int	configVersion;	/* T38's first two bytes, big-endian */
	struct TouchObjects objects;
	struct TouchMode modes[3];
};

/* Touch.flags: which panels a frame reads, and its end */
#define TOUCH_PANEL0		0x01
#define TOUCH_PANEL1		0x02
#define TOUCH_STATUS0		0x04	/* panel 0's status too */
#define TOUCH_STATUS1		0x08	/* panel 1's status too */
#define TOUCH_BUSY		0x10	/* a command has the link: no frame starts */
#define TOUCH_FRAME		0x80	/* a frame is complete */

/* Touch.state, copied into each panel's part of a frame */
union TouchState {
	unsigned char	value;
	struct {
		unsigned char	mode:2;		/* the panels' settings (touchSetMode) */
		unsigned char	reserved:2;
		unsigned char	input0:1;	/* sub_11D23() */
		unsigned char	input5:1;	/* sub_11D1B() */
		unsigned char	input3:1;	/* sub_11D2B() */
		unsigned char	valid:1;
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

/* the link's delays, in NOPs (from the settings block) */
struct TouchDelays {
	unsigned char	select;			/* after a chip select */
	unsigned char	byte;			/* after each byte */
	unsigned char	deselect;		/* before a deselect */
};

/* what goes to a panel and what comes back: an address in it (bit 0 set: a read), then the
   data written or read there */
struct TouchPacket {
	unsigned int	address;
	unsigned char	data[0x112];
};

#define TOUCH_READ		0x0001
#define TOUCH_STATUS_LENGTH	3	/* a status read: the address, then the status */

extern struct TouchFrame touchFrames[TOUCH_FRAMES];
extern struct TouchPacket touchCommand;
extern struct TouchPacket touchReply;
extern struct Touch touch;
extern struct TouchBuffer touchBuffer;
extern struct TouchDelays touchDelays;

extern void touchClearPanels(void);
extern void touchOpen(void);
extern void touchClose(void);
extern void touchClearFrames0(void);
extern void touchClearFrames1(void);
extern void touchStartFrame(void);
extern void touchStartFrameNoLock(void);
extern void touchEnable0(void);
extern void touchDisable0(void);
extern void touchEnable1(void);
extern void touchDisable1(void);
extern unsigned char touchSpiWrite(unsigned int address, unsigned char *data,
				   unsigned int length, unsigned char panel);
extern unsigned char touchSpiRead(unsigned int address, unsigned char *data,
				  unsigned int length, unsigned char panel);
extern unsigned char touchWriteVerified(unsigned int address, unsigned char *data,
					unsigned int length, unsigned char panel);
extern unsigned char touchWrite(unsigned int address, unsigned char *data, unsigned int length,
				unsigned char panel);
extern unsigned char touchRead(unsigned int address, unsigned char *data, unsigned int length,
			       unsigned char panel);
extern void touchIdentify0(void);
extern void touchIdentify1(void);
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
extern void touchReadObjects(struct TouchObjects *objects, unsigned char count,
			     unsigned char panel);
extern void touchSetMode(unsigned char mode);
extern void touchReadModes0(void);
extern void touchReadModes1(void);
extern void touchUpdateState(void);
extern unsigned char touchStatusOn0(void);
extern unsigned char touchStatusOff0(void);
extern unsigned char touchStatusOn1(void);
extern unsigned char touchStatusOff1(void);
extern unsigned char touchSetGrip0(unsigned char *data);
extern unsigned char touchSetGrip1(unsigned char *data);
extern void touchSetFlags(unsigned char flags);
extern void touchClearFlags(unsigned char flags);

#endif
