/*
 * The touch panels (the module at 0x17159, not written yet): each frame holds both panels'
 * reports, read over CSI10 (app/csi10.c); the host reads the frames through streams
 * 0x0300-0x0302.
 */
#ifndef ERNIE_TOUCH_H
#define ERNIE_TOUCH_H

/* a panel's part of a frame: its controller's report, then bytes of the touch module's */
#define TOUCH_REPORT_SIZE	0x34
#define TOUCH_PANEL_SEQUENCE	0x36	/* Touch.sequence when its command went out */
#define TOUCH_PANEL_STATUS	0x37	/* the third byte of its status reply */
#define TOUCH_PANEL_SIZE	0x3c

/* a frame: panel 0's part, panel 1's, and TCR02 at each one's command */
struct TouchFrame {
	unsigned char	panel[2][TOUCH_PANEL_SIZE];
	unsigned int	time0;
	unsigned int	time1;
};

/* a panel's controller: the addresses of what the module reads and writes in it (sent first,
   bit 0 set for a read) */
struct TouchPanel {
	unsigned char	state;			/* cleared at start */
	unsigned char	reserved1[5];
	unsigned int	reportAddress;
	unsigned int	settingAddress;		/* where Touch.setting goes */
	unsigned char	reserved2[0x12];
	unsigned int	statusAddress;
	unsigned char	reserved3[0x22];
};

/* Touch.flags: which panels a frame reads, and its end */
#define TOUCH_PANEL0		0x01
#define TOUCH_PANEL1		0x02
#define TOUCH_STATUS0		0x04	/* panel 0's status too */
#define TOUCH_STATUS1		0x08	/* panel 1's status too */
#define TOUCH_FRAME		0x80	/* a frame is complete */

/* the touch module's state */
struct Touch {
	unsigned char	flags;			/* TOUCH_* */
	unsigned char	frame;			/* the frame being filled */
	unsigned char	setting[2];		/* each panel's, 1-8, from the host (8 at start) */
	unsigned char	rate;			/* the frame rate TDR04 runs (0: none) */
	unsigned char	sequence;		/* copied into each panel's part of a frame */
	struct TouchPanel panel[2];
	unsigned int	bufferRead;
	unsigned int	bufferWrite;
	unsigned char	*buffer;
	unsigned char	delaySelect;		/* the NOPs after a chip select */
	unsigned char	delayByte;		/* after each byte */
	unsigned char	delayDeselect;		/* before a deselect */
};

/* what goes to a panel: an address in it (bit 0 set: a read), then the data written there */
struct TouchCommand {
	unsigned int	address;
	unsigned char	data[0x112];
};

#define TOUCH_READ		0x0001

extern struct TouchFrame touchFrames[2];
extern struct TouchCommand touchCommand;
extern unsigned char touchReply[3];	/* a panel's answer to a read of its status */
extern struct Touch touch;

#endif
