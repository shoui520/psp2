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
	unsigned char	reserved3;
	unsigned int	cookieRegHigh;		/* the Cookie's registers are numbered from 0xa0 */
	unsigned char	reserved9[0xdc - 0xd6];
	unsigned int	batteryTemp;		/* the battery's answers when none answered: */
	unsigned int	batteryBicTemp;		/* 0x0981, 0x098b, 0x0982, 0x0983, 0x0984, */
	unsigned int	batteryFullCap;		/* 0x0985 and 0x098a */
	unsigned int	batteryElec;
	unsigned int	batteryTte;
	unsigned int	batterySoh;
	unsigned int	batteryFullCap2;
	unsigned char	reserved10[0xf3 - 0xea];
	unsigned char	voltageTrim;		/* passed with a regulator's level (0x088e) */
	unsigned char	reserved7[0x184 - 0xf4];
	unsigned char	startCheckOff:1;	/* set: mode 1's command 0x0105 starts state 9
						   although sub_25664 and sub_256E1 refuse */
	unsigned char	reserved4:7;
	unsigned char	reserved5[0x18c - 0x185];
	unsigned char	chargeLedHost;		/* the host may hand the charge LED over (0x089e
						   mode 2) */
	unsigned char	reserved8[0x1a6 - 0x18d];
	unsigned char	suspendCheck:1;		/* set: a suspend runs sub_2818A also when
						   sub_267C2 holds */
	unsigned char	reserved15:7;
	unsigned char	reserved16;
	unsigned int	hardwareInfoSelect;	/* power-off's hardware info flags: 1 sets 0x80,
						   3 sets 0x40, others clear both */
	unsigned char	reserved11:2;
	unsigned char	indicatorOn0:1;		/* sub_2DF86's indicator on at a reset, and while
						   sub_126C9 holds */
	unsigned char	reserved12:1;
	unsigned char	indicatorOn1:1;		/* sub_2DFAA's indicator on at a reset */
	unsigned char	reserved13:3;
	unsigned char	reserved14[0x1b0 - 0x1ab];
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
