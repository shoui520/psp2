/*
 * The updater's firmware decryption (app/updatereq.c): the resident downloader's commands
 * (host request 0x11, 0x1100-0x1185) hand the syscon the firmware image a block at a time;
 * updaterDecryptData accumulates it into 0x80-byte blocks, AES-128-CBC decrypts each with the
 * updater key (the PS Dev Wiki's "Ernie Updater AES128CBC Key", flash 0xfe64/0xfe74) and
 * sums the ciphertext, and the SHA-1 of the decrypted image (app/sha1.c) is checked before
 * the downloader flashes it.
 */
#ifndef ERNIE_UPDATEREQ_H
#define ERNIE_UPDATEREQ_H

#include "aes.h"
#include "sha1.h"

#define UPDATER_BLOCK_SIZE	0x80	/* the decrypt works eight AES blocks at a time */

extern unsigned char updaterDecryptInit(void);
extern unsigned char updaterDecryptReset(void);
extern unsigned char updaterDecryptData(unsigned int length, unsigned char remaining,
					unsigned char *data);

#endif
