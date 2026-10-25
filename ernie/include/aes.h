/*
 * AES (FIPS-197) on 16-byte blocks: the key expansion, the block cipher and its inverse, and
 * the CBC, CFB, OFB and CTR modes over a buffer (the module at 0x127bb). The block and the
 * buffers are near (in RAM); the key and the round keys may be anywhere.
 */
#ifndef ERNIE_AES_H
#define ERNIE_AES_H

#define AES_BLOCK_SIZE		16

#define AES_ERROR_NULL		(-1)	/* a pointer is NULL */
#define AES_ERROR_KEY_LENGTH	(-2)	/* nk is not 4 to 8 */
#define AES_ERROR_BLOCK_LENGTH	(-3)	/* nb is not 4 to 8 */
#define AES_ERROR_LENGTH	(-4)	/* the length is not a multiple of the block */

/* a key schedule: w holds (nr + 1) * 4 words, nr = nk + 6 */
struct AesKey {
	unsigned long	*w;		/* the round keys */
	unsigned char	nk;		/* the key's length in words: 4, 6 or 8 */
	unsigned char	nb;		/* the block's length in words: 4 */
};

extern int aesKeyExpansion(const unsigned long *key, struct AesKey __near *ks);
extern int aesEncrypt(const unsigned char __near *in, unsigned char __near *out,
		      struct AesKey __near *ks);
extern int aesDecrypt(const unsigned char __near *in, unsigned char __near *out,
		      struct AesKey __near *ks);
extern int aesCbcEncrypt(const unsigned char __near *in, unsigned char __near *out,
			 struct AesKey __near *ks, const unsigned char *iv, unsigned int len);
extern int aesCbcDecrypt(const unsigned char __near *in, unsigned char __near *out,
			 struct AesKey __near *ks, const unsigned char *iv, unsigned int len);
extern int aesCfbEncrypt(const unsigned char __near *in, unsigned char __near *out,
			 struct AesKey __near *ks, const unsigned char *iv, unsigned int len);
extern int aesCfbDecrypt(const unsigned char __near *in, unsigned char __near *out,
			 struct AesKey __near *ks, const unsigned char *iv, unsigned int len);
extern int aesOfb(const unsigned char __near *in, unsigned char __near *out,
		  struct AesKey __near *ks, const unsigned char *iv, unsigned int len);
extern int aesCtr(const unsigned char __near *in, unsigned char __near *out,
		  struct AesKey __near *ks, const unsigned char *iv, unsigned int len);

#endif
