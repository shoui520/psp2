/*
 * The updater's requests (app/updatereq.c): host request 0x11 takes the firmware update's
 * commands 0x1180-0x1185 (the image's signature, its run mode, its segments, their data and
 * their programming, and the final digest), and the host commands 0x1100/0x1101 read the
 * downloader's version and the update's progress. The image comes in 1 KiB segments of
 * 0x80-byte blocks, each AES-128-CBC decrypted with the updater key (the PS Dev Wiki's "Ernie
 * Updater AES128CBC Key", flash 0xfe64/0xfe74) into the flash module's buffer, its ciphertext
 * summed; the SHA-1 of the programmed segments (app/sha1.c) is checked at the end.
 */
#ifndef ERNIE_UPDATEREQ_H
#define ERNIE_UPDATEREQ_H

#include "aes.h"
#include "host.h"
#include "kernel.h"
#include "sha1.h"

#define UPDATER_BLOCK_SIZE	0x80	/* the decrypt works eight AES blocks at a time */
#define UPDATER_SEGMENT_SIZE	0x400	/* a segment, a flash block: 1 KiB */
#define UPDATER_SEGMENTS	0x100	/* the segments of the 256 KiB of flash */

/*
 * The update's state. The states: 0 none, 1 the signature checked (0x1185), 0x10 and 0x20 the
 * segments coming in (run modes 0xc5e7 and 0x3665), 0x11 and 0x21 their digest being taken
 * (0x1184), 0x12 and 0x22 the digest checked, 0x13 the downloader to start (run modes 0x9a54
 * and 0x152e).
 */
struct Updater {
	unsigned char	state;
	unsigned char	segment;		/* the segment written, then the one hashed */
	unsigned char	segmentCount;		/* the segments received, counted at 0x1184 */
	unsigned char	segmentDone;		/* the segments hashed */
	unsigned char	progress;		/* segmentDone in percent of segmentCount */
	unsigned char	fill;			/* the bytes held in block */
	unsigned char	*output;		/* the decrypted segment: the flash module's buffer */
	unsigned char	block[UPDATER_BLOCK_SIZE];	/* the block being decrypted */
	unsigned char	received[UPDATER_SEGMENTS / 8];	/* a bit for each segment programmed */
	unsigned char	hash[SHA1HashSize];	/* the programmed segments' digest */
	unsigned long	sum;			/* the sum of every ciphertext byte of a segment */
	SHA1Context	sha;
	unsigned char	expected[SHA1HashSize];	/* cleared with the digest; nothing reads it */
	unsigned long	shaW[SHA1WordsSize];
	unsigned char	shaBlock[SHA1BlockSize];
	struct AesKey	key;
	unsigned long	roundKeys[44];		/* AES-128: (nr + 1) * nb words */
	unsigned char	iv[AES_BLOCK_SIZE];
};

extern struct HostRequest updaterRequest;
extern struct Updater updater;

extern void updaterRequestInit(void);
extern unsigned char hostCommandErnieDlVersion(unsigned char *out, unsigned char *length);
extern unsigned char hostCommandUpdaterProgress(unsigned char *out, unsigned char *length);
extern void updaterRequestNotify(void);
extern void updaterStartDownloader(void);
extern void updaterRequestAnswerOk(void);
extern void updaterRequestEvent(FLGPTN ptn);
extern unsigned char updaterDecryptInit(void);
extern unsigned char updaterDecryptReset(void);
extern unsigned char updaterDecryptData(unsigned int length, unsigned char remaining,
					unsigned char *data);

#endif
