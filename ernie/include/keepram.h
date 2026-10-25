/*
 * The RAM kept over a reset (0xfd900, shared by the application and the updater): cstart
 * clears the rest of the RAM, and keeps this block while its magic and checksum hold.
 */
#ifndef KEEPRAM_H
#define KEEPRAM_H

#define KEEP_RAM_MAGIC	0xa55a

struct KeepRam {
	unsigned int	magic;		/* KEEP_RAM_MAGIC */
	unsigned int	checksum;	/* 0xffff - the sum of the bytes from resetCause on */
	unsigned char	resetCause;	/* why the last reset was made */
	unsigned char	data[0xfb];
};

extern __far struct KeepRam keepRam;

#endif
