/*
 * The settings block (1 KiB) that sub_2C658 returns: its owner is not written yet.
 */
#ifndef ERNIE_CONFIG_H
#define ERNIE_CONFIG_H

/* PLACEHOLDER: only the fields read so far */
struct ConfigBlock {
	unsigned char	reserved1[0xcd];
	unsigned char	pressAnyState:1;	/* a button's presses go on being counted when the
						   system state changes */
	unsigned char	reserved2:7;
	unsigned char	reserved3[0x1b6 - 0xce];
	unsigned int	hostBusyLimit;		/* the host link's busy time before a pause */
	unsigned int	hostIdleLimit;		/* the idle time that clears the busy time */
	unsigned int	hostPause;		/* the pause, in ticks */
};

extern struct ConfigBlock *sub_2C658(void);

#endif
