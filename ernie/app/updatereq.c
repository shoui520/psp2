/*
 * The updater's firmware decryption: the resident downloader streams the new firmware image
 * in through host request 0x11; this accumulates it into 0x80-byte blocks (eight AES blocks),
 * AES-128-CBC decrypts each into the staging buffer with the updater key, chains the IV from
 * the last ciphertext block, and keeps a running sum of the ciphertext. updaterDecryptInit
 * expands the key (the PS Dev Wiki's "Ernie Updater AES128CBC Key") and resets the SHA-1 of
 * the decrypted image; updaterDecryptReset restarts a segment, keeping the key schedule.
 */
#include <string.h>

#include "ernie.h"
#include "aes.h"
#include "sha1.h"
#include "updatereq.h"

/* the updater key and IV, in the app image (the PS Dev Wiki's "Ernie Updater AES128CBC Key") */
extern const unsigned long updaterCryptoKey[4];		/* flash 0xfe64 */
extern const unsigned char updaterCryptoIv[AES_BLOCK_SIZE];	/* flash 0xfe74 */

unsigned char updaterState;			/* the download's phase */
unsigned char updaterSegmentCount;		/* the image's segments */
unsigned char updaterSegmentDone;		/* the segments programmed */
unsigned char updaterSegmentPad;		/* the last segment's padding */
unsigned char updaterFill;			/* the bytes held in updaterBlock */
unsigned char __far *updaterOutput;		/* where the next decrypted block goes */
unsigned long updaterSum;			/* the sum of every ciphertext byte */

unsigned char updaterBlock[UPDATER_BLOCK_SIZE];	/* the block being decrypted */
unsigned char updaterSpare[0x20];
SHA1Context updaterSha;				/* the digest of the decrypted image */
unsigned char updaterDigest[SHA1HashSize];
unsigned long updaterShaW[SHA1WordsSize];
unsigned char updaterShaBlock[SHA1BlockSize];
struct AesKey updaterKey;
unsigned long updaterRoundKeys[44];		/* AES-128: (nr + 1) * nb words */
unsigned char updaterIv[AES_BLOCK_SIZE];

/* expands the updater key, copies its IV and resets the image's digest, for a new download */
unsigned char updaterDecryptInit(void)
{
	unsigned char result;

	memset(updaterSpare, 0, sizeof(updaterSpare));
	memset(updaterDigest, 0, sizeof(updaterDigest));
	result = SHA1Reset(&updaterSha, updaterShaBlock, updaterShaW);
	if (result != 0)
		return result;
	updaterKey.w = updaterRoundKeys;
	updaterKey.nb = 4;
	updaterKey.nk = 4;
	result = aesKeyExpansion(updaterCryptoKey, (struct AesKey __near *)&updaterKey);
	memcpy(updaterIv, updaterCryptoIv, AES_BLOCK_SIZE);
	updaterFill = 0;
	updaterSegmentPad = 0;
	updaterSegmentCount = 0;
	updaterSegmentDone = 0;
	updaterOutput = (unsigned char __far *)0xFF706;
	return result;
}

/* restarts a segment: the IV back to the key's, the digest reset, the key schedule kept */
unsigned char updaterDecryptReset(void)
{
	unsigned char result = 0;

	updaterFill = 0;
	updaterSegmentPad = 0;
	updaterSegmentCount = 0;
	updaterSegmentDone = 0;
	memset(updaterSpare, 0, sizeof(updaterSpare));
	memset(updaterDigest, 0, sizeof(updaterDigest));
	memcpy(updaterIv, updaterCryptoIv, AES_BLOCK_SIZE);
	result = SHA1Reset(&updaterSha, updaterShaBlock, updaterShaW);
	return result;
}

/*
 * takes remaining bytes of the image at data (its position in the image is length): fills the
 * block, and for each full block AES-128-CBC decrypts it to the output, chains the IV from its
 * last ciphertext block and adds its ciphertext to the sum. 1 out of phase, 0x3c too long.
 */
unsigned char updaterDecryptData(unsigned int length, unsigned char remaining,
				 unsigned char *data)
{
	unsigned char chunk;
	unsigned char result;
	unsigned char i;
	unsigned char __far *sum;
	unsigned int offset;

	result = 0;
	if (updaterState != 0x10 && updaterState != 0x20)
		return 1;
	if (length >= 0x400)
		return 0x3c;
	while (remaining != 0 && result == 0) {
		if ((unsigned char)(UPDATER_BLOCK_SIZE - updaterFill) > remaining)
			chunk = remaining;
		else
			chunk = UPDATER_BLOCK_SIZE - updaterFill;
		memcpy(&updaterBlock[updaterFill], data, chunk);
		updaterFill += chunk;
		if (updaterFill == UPDATER_BLOCK_SIZE) {
			offset = length & 0xff80;
			result = aesCbcDecrypt((const unsigned char __near *)updaterBlock,
					       (unsigned char __near *)updaterOutput + offset,
					       (struct AesKey __near *)&updaterKey, updaterIv,
					       UPDATER_BLOCK_SIZE);
			memcpy(updaterIv, &updaterBlock[0x70], AES_BLOCK_SIZE);
			updaterFill = 0;
			sum = updaterBlock;
			for (i = 0; i < UPDATER_BLOCK_SIZE; i++)
				updaterSum += *sum++;
		}
		data += chunk;
		remaining -= chunk;
	}
	return result;
}
