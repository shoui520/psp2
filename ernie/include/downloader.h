/*
 * What the app shares with the downloader (the updater in boot cluster 1): its entry, and the
 * RAM the app leaves it when it starts it (updaterStartDownloader). A near address is in the
 * page 0xf0000.
 */
#ifndef ERNIE_DOWNLOADER_H
#define ERNIE_DOWNLOADER_H

#include "host.h"

#define DOWNLOADER_ENTRY	0x2000

/* the app's state, for the downloader to carry on the host link with */
struct DownloaderStart {
	unsigned char	snapshot;	/* sleepFactorSnapshot's */
	unsigned char	sleepFactor[4];	/* sleepFactorGet's */
	unsigned char	frameSize;	/* hostLink.frameSize */
	union {
		unsigned int		size;	/* downloaders to 1.02.ff: the config's size only */
		struct HostLinkConfig	config;	/* later ones: hostLink.config */
	} link;
};

#define DOWNLOADER_START	((struct DownloaderStart __near *)0xd500)

/*
 * the downloader versions (their last three bytes, the first byte's most significant) that
 * expect RAM cleared at their start, each at its own address
 */
#define DOWNLOADER_RAM_20		((unsigned char __near *)0xdedc)	/* 0x2000-0x2600 */
#define DOWNLOADER_RAM_20_SIZE		0xaaa
#define DOWNLOADER_RAM_40_BYTE		((unsigned char __near *)0xd72c)	/* board 0x40: 0x010000, 0x010800 */
#define DOWNLOADER_RAM_40_WORD		((unsigned char __near *)0xd750)
#define DOWNLOADER_RAM_41_BYTE		((unsigned char __near *)0xd724)	/* board 0x41: 0x010100 */
#define DOWNLOADER_RAM_41_WORD		((unsigned char __near *)0xd748)

#endif
