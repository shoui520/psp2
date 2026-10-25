/*
 * The settings block (1 KiB) that sub_2C658 returns: its owner is not written yet.
 */
#ifndef ERNIE_CONFIG_H
#define ERNIE_CONFIG_H

#include "analog.h"

/* PLACEHOLDER: only the fields read so far */
struct ConfigBlock {
	struct AnalogCalibration analogCalibration;	/* the sticks' default calibration */
	unsigned char	analogScale0;		/* set 0's margin, in 1/1000 of the range */
	unsigned char	analogScale1;		/* set 1's */
	unsigned char	reserved1[0xcd - 0x12];
	unsigned char	pressAnyState:1;	/* a button's presses go on being counted when the
						   system state changes */
	unsigned char	reserved2:7;
	unsigned int	analogMin;		/* a calibration's min at a reset, and the most a
						   loaded one's may be */
	unsigned int	analogMax;		/* its max at a reset, the least a loaded one's */
	unsigned char	analogMinRange;		/* the least high byte of a calibration's range */
	unsigned char	reserved3[0x184 - 0xd3];
	unsigned char	startCheckOff:1;	/* set: mode 1's command 0x0105 starts state 9
						   although sub_25664 and sub_256E1 refuse */
	unsigned char	reserved4:7;
	unsigned char	reserved5[0x1b0 - 0x185];
	unsigned char	touchDelaySelect;	/* Touch's delays, in NOPs */
	unsigned char	touchDelayByte;
	unsigned char	touchDelayDeselect;
	unsigned char	touchWriteTries;	/* a verified write's tries */
	unsigned char	touchWriteReads;	/* and the reads each try compares */
	unsigned char	reserved6;
	unsigned int	hostBusyLimit;		/* the host link's busy time before a pause */
	unsigned int	hostIdleLimit;		/* the idle time that clears the busy time */
	unsigned int	hostPause;		/* the pause, in ticks */
};

extern struct ConfigBlock *sub_2C658(void);

#endif
