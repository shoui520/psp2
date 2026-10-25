/*
 * PLACEHOLDER: the system's mode, the word sub_2AB39 returns (its owner, at 0x2ab39, is not
 * written yet).
 */
#ifndef ERNIE_SYSMODE_H
#define ERNIE_SYSMODE_H

/* a 7-bit mode (sub_2457F compares it with 0x0f) and a flag that keeps a button's presses
   counted across a state change */
struct SystemMode {
	unsigned int	mode:7;
	unsigned int	flag:1;
	unsigned int	reserved:8;
};

extern struct SystemMode *sub_2AB39(void);

#endif
