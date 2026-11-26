#include <common_imp.h>
#include <crypto/kirk.h>

extern SceUID sceKernelCreateSema(const char *name, SceUInt attr, int initVal, int maxVal, void *option);
extern int sceKernelDeleteSema(SceUID semaid);
extern int sceKernelWaitSema(SceUID semaid, int signal, SceUInt *timeout);
extern int sceKernelSignalSema(SceUID semaid, int signal);
extern int sceIdStorageLookup(u16 key, u32 offset, void *buf, u32 len);
extern int sceUtilsBufferCopyWithRange(void *outbuf, SceSize outsize, void *inbuf, int insize, int cmd);

typedef struct MemabAesBuf {
	KirkAESHeader hdr;
	u8 data[0x42C];
} MemabAesBuf;

static u8 g_sha1_buf[0x80];     /* 0x2FC0 */
static u8 g_kirk_work_buf[0x938];    /* 0x3140 */
static MemabAesBuf g_aes_work;       /* 0x3A80 (hdr) + 0x3A94 (data) */

static u8 *g_sha1_ptr = g_sha1_buf;         /* 0x2E00 */
static u8 *g_kirk_work_ptr = g_kirk_work_buf;         /* 0x2E04 */
static u8 *g_aes_hdr_ptr = (u8 *)&g_aes_work;         /* 0x2E08 */
static u8 *g_aes_data_ptr = g_aes_work.data;          /* 0x2E10 */

static int g_inited;                        /* 0x2E40 */
static u64 g_seq_counter;                   /* 0x2E48..0x2E4F */
static u8 g_session_key[0x20];              /* 0x2E80 */
static SceUID g_auth_sema;                  /* 0x2EA0 */
static u8 g_rand_buf[0x40];                 /* 0x2EC0 */
static u8 g_idstorage_cert[0xC0];           /* 0x2F00 */
static u8 g_ecdh_buf[0x40];                 /* 0x3040 */
static u8 g_ecdsa_buf[0x80];                /* 0x3080 */
static u8 g_aes_buf[0x40];                  /* 0x3100 */
static SceUID g_cipher_sema;                /* 0x3A78 */

static inline void copy_bytes(u8 *dst, const u8 *src, u32 len)
{
	for (u32 i = 0; i < len; i++) {
		dst[i] = src[i];
	}
}

static inline void zero_bytes(u8 *dst, u32 len)
{
	for (u32 i = 0; i < len; i++) {
		dst[i] = 0;
	}
}

static inline void pack_u64_le(u8 *dst, u64 val)
{
	dst[0] = (u8)(val);
	dst[1] = (u8)(val >> 8);
	dst[2] = (u8)(val >> 16);
	dst[3] = (u8)(val >> 24);
	dst[4] = (u8)(val >> 32);
	dst[5] = (u8)(val >> 40);
	dst[6] = (u8)(val >> 48);
	dst[7] = (u8)(val >> 56);
}

static inline u64 unpack_u64_le(const u8 *src)
{
	return (u64)src[0] |
	       ((u64)src[1] << 8) |
	       ((u64)src[2] << 16) |
	       ((u64)src[3] << 24) |
	       ((u64)src[4] << 32) |
	       ((u64)src[5] << 40) |
	       ((u64)src[6] << 48) |
	       ((u64)src[7] << 56);
}

/* 0x0000: packAuthBuffer */
static void __attribute__((noinline)) packAuthBuffer(u8 *dst, const u8 *partA, u32 lenA, const u8 *partB, u32 lenB,
                     const u8 *partC, u32 lenC, const u8 *partD, u32 lenD)
{
	if (lenA != 0) copy_bytes(dst, partA, lenA);
	dst += lenA;
	if (lenB != 0) copy_bytes(dst, partB, lenB);
	dst += lenB;
	if (lenC != 0) copy_bytes(dst, partC, lenC);
	dst += lenC;
	if (lenD != 0) copy_bytes(dst, partD, lenD);
	dst += lenD;
	u32 rem = 0xC0 - (lenA + lenB + lenC + lenD);
	if (rem != 0) zero_bytes(dst, rem);
}

/* 0x00E4: kirkSha1Multi (SHA-1 via Kirk cmd 11) */
static int __attribute__((noinline)) kirkSha1Multi(u8 *buf, const u8 *partA, u32 lenA, const u8 *partB, u32 lenB,
                    const u8 *partC, u32 lenC)
{
	u32 total = lenA + lenB + lenC;
	*(u32 *)buf = total;
	u8 *p = buf + 4;
	if (lenA != 0) copy_bytes(p, partA, lenA);
	p += lenA;
	if (lenB != 0) copy_bytes(p, partB, lenB);
	p += lenB;
	if (lenC != 0) copy_bytes(p, partC, lenC);
	u32 in_sz = total + 4;
	if (in_sz < 0x14) in_sz = 0x14;
	int r = sceUtilsBufferCopyWithRange(buf, in_sz, buf, in_sz, 11);
	return (r == 0) ? 0 : -0x102;
}

/* 0x01B8: kirkEcdsaVerify (ECDSA verify via Kirk cmd 17) */
static int __attribute__((noinline)) kirkEcdsaVerify(u8 *work, const u8 *pub, const u8 *hash, const u8 *sig)
{
	copy_bytes(work, pub, 0x28);
	copy_bytes(work + 0x28, hash, 0x14);
	copy_bytes(work + 0x3C, sig, 0x28);
	int r = sceUtilsBufferCopyWithRange(NULL, 0, work, 0x64, 17);
	return (r == 0) ? 0 : -0x10C;
}

/* 0x025C: loadAndVerifyIdStorageCert (Read & verify IDStorage certificate via Kirk cmd 18) */
static int __attribute__((noinline)) loadAndVerifyIdStorageCert(u8 *cert_buf)
{
	int r = sceIdStorageLookup(0x100, 0xF0, cert_buf, 0xB8);
	if (r < 0) {
		r = sceIdStorageLookup(0x120, 0xF0, cert_buf, 0xB8);
		if (r < 0) {
			return -0x10D;
		}
	}
	int kr = sceUtilsBufferCopyWithRange(NULL, 0, cert_buf, 0xB8, 18);
	return (kr == 0) ? 0 : -0x106;
}

/* 0x02E0: kirkEncryptWithPrngIv (Kirk cmd 4 encrypt with IV from PRNG cmd 14) */
static int __attribute__((noinline)) kirkEncryptWithPrngIv(u8 *work, u32 keyslot, const u8 *partA, u32 lenA, const u8 *partB, u32 lenB,
                    const u8 *partC, u32 lenC, const u8 *partD, u32 lenD, const u8 *partE, u32 lenE,
                    const u8 *partF, u32 lenF)
{
	u32 total = lenA + lenB + lenC + lenD + lenE + lenF;
	u32 aligned = (total + 0x1F) & ~0x0Fu;
	if (sceUtilsBufferCopyWithRange(work, 0x14, NULL, 0, 14) != 0) {
		return -0x100;
	}
	u8 *p = work + 0x14;
	for (u32 i = 0; i < 8; i++) {
		p[i] = work[i * 4];
	}
	p += 8;
	zero_bytes(p, 8);
	p += 8;
	KirkAESHeader *hdr = (KirkAESHeader *)work;
	hdr->mode = 4;
	hdr->pad[0] = 0;
	hdr->pad[1] = 0;
	hdr->keyIndex = keyslot;
	hdr->dataSize = aligned;
	if (lenA != 0) copy_bytes(p, partA, lenA);
	p += lenA;
	if (lenB != 0) copy_bytes(p, partB, lenB);
	p += lenB;
	if (lenC != 0) copy_bytes(p, partC, lenC);
	p += lenC;
	if (lenD != 0) copy_bytes(p, partD, lenD);
	p += lenD;
	if (lenE != 0) copy_bytes(p, partE, lenE);
	p += lenE;
	if (lenF != 0) copy_bytes(p, partF, lenF);
	u32 sz = aligned + 0x14;
	int r = sceUtilsBufferCopyWithRange(work, sz, work, sz, 4);
	return (r == 0) ? 0 : -0x10A;
}

static int __attribute__((noinline)) kirkEncryptInPlace(u8 *work, u32 len, u32 keyslot)
{
	KirkAESHeader *hdr = (KirkAESHeader *)work;
	hdr->mode = 4;
	hdr->pad[0] = 0;
	hdr->pad[1] = 0;
	hdr->keyIndex = keyslot;
	hdr->dataSize = len;
	u32 sz = len + 0x14;
	int r = sceUtilsBufferCopyWithRange(work, sz, work, sz, 4);
	return (r == 0) ? 0 : -0x10A;
}

static int __attribute__((noinline)) kirkDecryptInPlace(u8 *work, u32 len, u32 keyslot)
{
	KirkAESHeader *hdr = (KirkAESHeader *)work;
	hdr->mode = 5;
	hdr->pad[0] = 0;
	hdr->pad[1] = 0;
	hdr->keyIndex = keyslot;
	hdr->dataSize = len;
	u32 sz = len + 0x14;
	int r = sceUtilsBufferCopyWithRange(work, sz, work, sz, 7);
	return (r == 0) ? 0 : -0x10A;
}

/* 0x05D0: kirkCopyAndDecrypt (Copy & Kirk cmd 7 decrypt) */
static int __attribute__((noinline)) kirkCopyAndDecrypt(u8 *work, u32 keyslot, const u8 *src, u32 len)
{
	if (len != 0) {
		copy_bytes(work + 0x14, src, len);
	}
	return kirkDecryptInPlace(work, len, keyslot);
}

/* 0x0620: kirkGenKeystream (Generate keystream blocks via Kirk cmd 7 keyslot 0x44) */
static int __attribute__((noinline)) kirkGenKeystream(u8 *work, u32 len, u64 *counter, const u32 *key)
{
	for (u32 off = 0x14; off < len + 0x14; off += 0x10) {
		u32 *blk = (u32 *)(work + off);
		u64 c = *counter;
		blk[0] = (u32)c;
		blk[1] = (u32)(c >> 32);
		if (c & 1) {
			blk[2] = key[2];
			blk[3] = key[3];
		} else {
			blk[2] = key[0];
			blk[3] = key[1];
		}
		*counter = c + 1;
	}
	return kirkDecryptInPlace(work, len, 0x44);
}

/* 0x06E0: ctrEncryptAndMac (CTR encrypt + HMAC-SHA1 tag) */
static int __attribute__((noinline)) ctrEncryptAndMac(u8 *work, u8 *data, u32 len, u64 *counter, const u8 *key, u8 *hdr, int do_enc)
{
	pack_u64_le(hdr, *counter);
	zero_bytes(hdr + 8, 8);
	if (do_enc != 0) {
		if (len == 0) {
			return -0x400;
		}
		int r = kirkGenKeystream(work, len, counter, (const u32 *)key);
		if (r != 0) {
			return r;
		}
		u32 words = len >> 2;
		u32 *w_ptr = (u32 *)work;
		u32 *d_ptr = (u32 *)data;
		for (u32 i = 0; i < words; i++) {
			d_ptr[i] ^= w_ptr[i];
		}
	}
	int r = kirkSha1Multi(work, key, 0x14, hdr, 0x10, data, len);
	if (r != 0) {
		return r;
	}
	copy_bytes(hdr + 0x10, work, 0x10);
	return 0;
}

/* 0x089C: verifyMacAndCtrDecrypt (Verify HMAC-SHA1 tag + CTR decrypt) */
static int __attribute__((noinline)) verifyMacAndCtrDecrypt(u8 *work, u8 *data, u32 len, u64 *counter, u64 *min_counter,
                    const u8 *key, const u8 *hdr, int do_dec, int check_seq)
{
	int r = kirkSha1Multi(work, key, 0x14, hdr, 0x10, data, len);
	if (r != 0) {
		return r;
	}
	for (u32 i = 0; i < 0x10; i++) {
		if (work[i] != hdr[0x10 + i]) {
			return -0x500;
		}
	}
	if (check_seq != 0) {
		if (*counter < *min_counter) {
			return -0x600;
		}
	}
	if (do_dec != 0) {
		if (len == 0) {
			return -0x400;
		}
		r = kirkGenKeystream(work, len, counter, (const u32 *)key);
		if (r != 0) {
			return r;
		}
		u32 words = len >> 2;
		u32 *w_ptr = (u32 *)work;
		u32 *d_ptr = (u32 *)data;
		for (u32 i = 0; i < words; i++) {
			d_ptr[i] ^= w_ptr[i];
		}
	}
	return 0;
}

/* 0x0A1C: module_start */
int module_start(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	SceUID semaFirst = sceKernelCreateSema("SceMemab1", 0, 1, 1, NULL);
	g_auth_sema = semaFirst;
	if (semaFirst <= 0) {
		return 1;
	}
	SceUID semaSecond = sceKernelCreateSema("SceMemab2", 0, 1, 1, NULL);
	g_cipher_sema = semaSecond;
	if (semaSecond <= 0) {
		sceKernelDeleteSema(g_auth_sema);
		return 1;
	}
	return 0;
}

/* 0x0AA8: module_stop */
int module_stop(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	SceUInt timeout = 5000000;
	if (sceKernelWaitSema(g_auth_sema, 1, &timeout) != 0) {
		return 1;
	}
	if (sceKernelWaitSema(g_cipher_sema, 1, &timeout) != 0) {
		return 1;
	}
	sceKernelDeleteSema(g_auth_sema);
	sceKernelDeleteSema(g_cipher_sema);
	return 0;
}

/* 0x0B2C: sceMemab_driver_6DD7339A */
int sceMemab_driver_6DD7339A(void)
{
	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = 0;
	if (g_inited == 0) {
		g_seq_counter = 0;
		if (sceUtilsBufferCopyWithRange(g_session_key, 0x14, NULL, 0, 14) != 0) {
			ret = -0x100;
		} else {
			g_inited = 1;
		}
	}
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x0BE4: sceMemab_driver_D57856A7 */
int sceMemab_driver_D57856A7(void)
{
	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = 0;
	if (g_inited != 0) {
		zero_bytes(g_session_key, 0x20);
		g_inited = 0;
		g_seq_counter = 0;
	}
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x0C8C: sceMemab_driver_4B54EAAD */
int sceMemab_driver_4B54EAAD(u8 *out_plain, u8 *out_enc)
{
	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		ret = loadAndVerifyIdStorageCert(g_idstorage_cert);
		if (ret == 0) {
			if (sceUtilsBufferCopyWithRange(g_rand_buf, 0x3C, NULL, 0, 12) != 0) {
				ret = -0x100;
			} else {
				packAuthBuffer(out_plain, g_rand_buf, 0x3C, g_idstorage_cert, 0x10, NULL, 0, NULL, 0);
				ret = kirkEncryptWithPrngIv(g_kirk_work_buf, 2, g_rand_buf + 0x14, 0x28, g_idstorage_cert, 0x60,
				               NULL, 0, NULL, 0, NULL, 0, NULL, 0);
				if (ret == 0) {
					copy_bytes(out_enc, g_kirk_work_ptr + 0x14, 0xA0);
				}
			}
		}
	}
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x0DF8: sceMemab_driver_9BF0C95D */
int sceMemab_driver_9BF0C95D(u8 *out_plain, const u8 *in_enc, u8 *out_enc)
{
	u8 sp_buf[0x60];
	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		ret = kirkCopyAndDecrypt(g_kirk_work_buf, 2, in_enc, 0xA0);
		if (ret == 0) {
			ret = kirkSha1Multi(g_sha1_buf, g_kirk_work_ptr + 0x38, 0x38, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			ret = loadAndVerifyIdStorageCert(g_idstorage_cert);
		}
		if (ret == 0) {
			if (kirkEcdsaVerify(g_ecdsa_buf, g_idstorage_cert + 0x60, g_sha1_ptr, g_kirk_work_ptr + 0x70) != 0) {
				ret = -0x301;
			}
		}
		if (ret == 0) {
			if (sceUtilsBufferCopyWithRange(g_rand_buf, 0x3C, NULL, 0, 12) != 0) {
				ret = -0x100;
			}
		}
		if (ret == 0) {
			copy_bytes(g_ecdh_buf, g_rand_buf, 0x14);
			copy_bytes(g_ecdh_buf + 0x14, g_kirk_work_ptr + 0x10, 0x28);
			if (sceUtilsBufferCopyWithRange(g_ecdh_buf, 0x3C, g_ecdh_buf, 0x3C, 13) != 0) {
				ret = -0x103;
			}
		}
		if (ret == 0) {
			copy_bytes(sp_buf, g_rand_buf + 0x14, 0x28);
			copy_bytes(sp_buf + 0x28, g_kirk_work_ptr + 0x10, 0x28);
			copy_bytes(sp_buf + 0x50, g_kirk_work_ptr + 0x38, 0x10);
			ret = kirkSha1Multi(g_sha1_buf, sp_buf, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			ret = loadAndVerifyIdStorageCert(g_idstorage_cert);
		}
		if (ret == 0) {
			copy_bytes(g_aes_buf, g_idstorage_cert + 0x88, 0x20);
			copy_bytes(g_aes_buf + 0x20, g_sha1_ptr, 0x14);
			if (sceUtilsBufferCopyWithRange(g_aes_buf, 0x34, g_aes_buf, 0x34, 16) != 0) {
				ret = -0x104;
			}
		}
		if (ret == 0) {
			ret = kirkSha1Multi(g_sha1_buf, g_ecdh_buf, 0x14, sp_buf, 0x60, NULL, 0);
		}
		if (ret == 0) {
			packAuthBuffer(out_plain, g_kirk_work_ptr + 0x48, 0x28, sp_buf, 0x50,
			         g_idstorage_cert, 0x10, g_ecdh_buf, 0x14);
			ret = kirkEncryptWithPrngIv(g_kirk_work_buf, 2, sp_buf, 0x60, g_aes_buf, 0x28,
			               g_sha1_ptr, 0x14, g_idstorage_cert, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			copy_bytes(out_enc, g_kirk_work_ptr + 0x14, 0x110);
		}
	}
	zero_bytes(sp_buf, 0x60);
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x1224: sceMemab_driver_C3981EE1 */
int sceMemab_driver_C3981EE1(u8 *in_out_plain, const u8 *in_enc, u8 *out_enc)
{
	u8 auth_blob[0x60];
	u8 seq_bytes[0x08];
	u8 mac_hdr[0x20];
	u8 sess_key_copy[0x20];
	u64 ctr = 0;

	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		ret = kirkCopyAndDecrypt(g_kirk_work_buf, 2, in_enc, 0x110);
		if (ret == 0) {
			ret = kirkSha1Multi(g_sha1_buf, g_kirk_work_ptr + 0xAC, 0x38, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			ret = loadAndVerifyIdStorageCert(g_idstorage_cert);
		}
		if (ret == 0) {
			if (kirkEcdsaVerify(g_ecdsa_buf, g_idstorage_cert + 0x60, g_sha1_ptr, g_kirk_work_ptr + 0xE4) != 0) {
				ret = -0x301;
			}
		}
		if (ret == 0) {
			ret = kirkSha1Multi(g_sha1_buf, g_kirk_work_ptr + 0x10, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			if (kirkEcdsaVerify(g_ecdsa_buf, g_kirk_work_ptr + 0xBC, g_sha1_ptr, g_kirk_work_ptr + 0x70) != 0) {
				ret = -0x303;
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x28; i++) {
				if (in_out_plain[0x14 + i] != g_kirk_work_ptr[0x38 + i]) {
					ret = -0x308;
					break;
				}
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x10; i++) {
				if (in_out_plain[0x3C + i] != g_kirk_work_ptr[0x60 + i]) {
					ret = -0x304;
					break;
				}
			}
		}
		if (ret == 0) {
			copy_bytes(g_ecdh_buf, in_out_plain, 0x14);
			copy_bytes(g_ecdh_buf + 0x14, g_kirk_work_ptr + 0x10, 0x28);
			if (sceUtilsBufferCopyWithRange(g_ecdh_buf, 0x3C, g_ecdh_buf, 0x3C, 13) != 0) {
				ret = -0x105;
			}
		}
		if (ret == 0) {
			ret = kirkSha1Multi(g_sha1_buf, g_ecdh_buf, 0x14, g_kirk_work_ptr + 0x10, 0x60, NULL, 0);
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x14; i++) {
				if (g_kirk_work_ptr[0x98 + i] != g_sha1_ptr[i]) {
					ret = -0x305;
					break;
				}
			}
		}
		if (ret == 0) {
			copy_bytes(auth_blob, g_kirk_work_ptr + 0x38, 0x28);
			copy_bytes(auth_blob + 0x28, g_kirk_work_ptr + 0x10, 0x28);
			copy_bytes(auth_blob + 0x50, g_kirk_work_ptr + 0xAC, 0x10);
			ret = kirkSha1Multi(g_sha1_buf, auth_blob, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			ret = loadAndVerifyIdStorageCert(g_idstorage_cert);
		}
		if (ret == 0) {
			copy_bytes(g_aes_buf, g_idstorage_cert + 0x88, 0x20);
			copy_bytes(g_aes_buf + 0x20, g_sha1_ptr, 0x14);
			if (sceUtilsBufferCopyWithRange(g_aes_buf, 0x34, g_aes_buf, 0x34, 16) != 0) {
				ret = -0x104;
			}
		}
		if (ret == 0) {
			ret = kirkSha1Multi(g_sha1_buf, g_ecdh_buf, 0x14, auth_blob, 0x60, NULL, 0);
		}
		if (ret == 0) {
			packAuthBuffer(in_out_plain, g_ecdh_buf, 0x14, NULL, 0, NULL, 0, NULL, 0);
			copy_bytes(sess_key_copy, g_session_key, 0x20);
			ret = ctrEncryptAndMac(g_kirk_work_ptr, sess_key_copy, 0x20, &ctr, in_out_plain, mac_hdr, 1);
		}
		if (ret == 0) {
			pack_u64_le(seq_bytes, g_seq_counter);
			ret = kirkEncryptWithPrngIv(g_kirk_work_buf, 2, auth_blob, 0x60, g_aes_buf, 0x28,
			               g_sha1_ptr, 0x14, sess_key_copy, 0x20, seq_bytes, 0x08, mac_hdr, 0x20);
		}
		if (ret == 0) {
			copy_bytes(out_enc, g_kirk_work_ptr + 0x14, 0x100);
		}
	}
	zero_bytes(auth_blob, 0x60);
	zero_bytes(seq_bytes, 0x08);
	zero_bytes(mac_hdr, 0x20);
	zero_bytes(sess_key_copy, 0x20);
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x18B4: sceMemab_driver_23E4659B */
int sceMemab_driver_23E4659B(u8 *in_out_plain, const u8 *in_enc, u8 *out_enc)
{
	u8 seq_bytes[0x08];
	u8 mac_hdr[0x20];
	u8 sess_key_copy[0x20];
	u64 ctr = 0;
	u64 min_ctr = 0;

	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		ret = kirkCopyAndDecrypt(g_kirk_work_buf, 2, in_enc, 0x100);
		if (ret == 0) {
			ret = kirkSha1Multi(g_sha1_buf, g_kirk_work_ptr + 0x10, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			if (kirkEcdsaVerify(g_ecdsa_buf, in_out_plain, g_sha1_ptr, g_kirk_work_ptr + 0x70) != 0) {
				ret = -0x307;
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x28; i++) {
				if (in_out_plain[0x28 + i] != g_kirk_work_ptr[0x38 + i]) {
					ret = -0x30D;
					break;
				}
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x28; i++) {
				if (in_out_plain[0x50 + i] != g_kirk_work_ptr[0x10 + i]) {
					ret = -0x308;
					break;
				}
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x10; i++) {
				if (in_out_plain[0x78 + i] != g_kirk_work_ptr[0x60 + i]) {
					ret = -0x30E;
					break;
				}
			}
		}
		if (ret == 0) {
			ret = kirkSha1Multi(g_sha1_buf, in_out_plain + 0x88, 0x14, g_kirk_work_ptr + 0x10, 0x60, NULL, 0);
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x14; i++) {
				if (g_sha1_ptr[i] != g_kirk_work_ptr[0x98 + i]) {
					ret = -0x309;
					break;
				}
			}
		}
		if (ret == 0) {
			packAuthBuffer(in_out_plain, in_out_plain + 0x88, 0x14, g_kirk_work_ptr + 0xAC, 0x20,
			         g_kirk_work_ptr + 0xCC, 0x08, NULL, 0);
			ret = verifyMacAndCtrDecrypt(g_kirk_work_ptr, in_out_plain + 0x14, 0x20, &ctr, &min_ctr,
			               in_out_plain, g_kirk_work_ptr + 0xD4, 1, 0);
		}
		if (ret == 0) {
			ctr = 0;
			copy_bytes(sess_key_copy, g_session_key, 0x20);
			ret = ctrEncryptAndMac(g_kirk_work_ptr, sess_key_copy, 0x20, &ctr, in_out_plain, mac_hdr, 1);
		}
		if (ret == 0) {
			pack_u64_le(seq_bytes, g_seq_counter);
			ret = kirkEncryptWithPrngIv(g_kirk_work_buf, 2, sess_key_copy, 0x20, seq_bytes, 0x08, mac_hdr, 0x20,
			               NULL, 0, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			copy_bytes(out_enc, g_kirk_work_ptr + 0x14, 0x60);
		}
	}
	zero_bytes(seq_bytes, 0x08);
	zero_bytes(mac_hdr, 0x20);
	zero_bytes(sess_key_copy, 0x20);
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x1D6C: sceMemab_driver_8ABE3445 */
int sceMemab_driver_8ABE3445(u8 *in_out_plain, const u8 *in_enc)
{
	u8 dec_buf[0x60];
	u64 ctr = 0;
	u64 min_ctr = 0;

	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		copy_bytes(dec_buf, in_enc, 0x60);
		ret = kirkCopyAndDecrypt(g_kirk_work_buf, 2, dec_buf, 0x60);
		if (ret == 0) {
			copy_bytes(dec_buf, g_kirk_work_ptr + 0x10, 0x60);
			packAuthBuffer(in_out_plain, in_out_plain, 0x14, dec_buf, 0x20, dec_buf + 0x20, 0x08, NULL, 0);
			ret = verifyMacAndCtrDecrypt(g_kirk_work_ptr, in_out_plain + 0x14, 0x20, &ctr, &min_ctr,
			               in_out_plain, dec_buf + 0x28, 1, 0);
		}
	}
	zero_bytes(dec_buf, 0x60);
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x1EE4: sceMemab_driver_3C15BC8C */
int sceMemab_driver_3C15BC8C(u8 *ctx, u8 *data, u32 len, u8 *hdr, int do_enc)
{
	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		if ((len & 0x0F) != 0) {
			ret = -0x400;
		} else if (len > 0x910) {
			ret = -0x401;
		} else {
			u64 ctr = unpack_u64_le(ctx + 0x3C);
			ret = ctrEncryptAndMac(g_kirk_work_ptr, data, len, &ctr, ctx, hdr, do_enc);
			if (ret == 0) {
				pack_u64_le(ctx + 0x3C, ctr);
			}
		}
	}
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x2100: sceMemab_driver_D47A50B1 */
int sceMemab_driver_D47A50B1(u8 *ctx, u8 *data, u32 len, const u8 *hdr, int do_dec, int check_seq)
{
	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		if ((len & 0x0F) != 0) {
			ret = -0x400;
		} else if (len > 0x910) {
			ret = -0x401;
		} else {
			u64 ctr = unpack_u64_le(hdr);
			u64 min_ctr = unpack_u64_le(ctx + 0x44);
			ret = verifyMacAndCtrDecrypt(g_kirk_work_ptr, data, len, &ctr, &min_ctr, ctx, hdr, do_dec, check_seq);
			if (ret == 0 && check_seq != 0) {
				pack_u64_le(ctx + 0x44, ctr);
			}
		}
	}
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x23E4: sceMemab_driver_F742F283 */
int sceMemab_driver_F742F283(u8 *data, u32 len, u8 *hdr, int do_enc)
{
	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		if ((len & 0x0F) != 0) {
			ret = -0x400;
		} else if (len > 0x910) {
			ret = -0x401;
		} else {
			u64 ctr = g_seq_counter;
			ret = ctrEncryptAndMac(g_kirk_work_ptr, data, len, &ctr, g_session_key, hdr, do_enc);
			if (ret == 0) {
				g_seq_counter = ctr;
			}
		}
	}
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x24F4: sceMemab_driver_CB5D3916 */
int sceMemab_driver_CB5D3916(u8 *ctx, u8 *data, u32 len, const u8 *hdr, int do_dec, int check_seq)
{
	if (sceKernelWaitSema(g_auth_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		if ((len & 0x0F) != 0) {
			ret = -0x400;
		} else if (len > 0x910) {
			ret = -0x401;
		} else {
			u64 ctr = unpack_u64_le(hdr);
			u64 min_ctr = unpack_u64_le(ctx + 0x34);
			ret = verifyMacAndCtrDecrypt(g_kirk_work_ptr, data, len, &ctr, &min_ctr, ctx + 0x14, hdr, do_dec, check_seq);
			if (ret == 0 && check_seq != 0) {
				pack_u64_le(ctx + 0x34, ctr);
			}
		}
	}
	if (sceKernelSignalSema(g_auth_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x27D8: sceMemab_driver_9DE8C8CD */
int sceMemab_driver_9DE8C8CD(const u32 *mask, u32 *data, u32 len)
{
	if (sceKernelWaitSema(g_cipher_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret;
	if ((len & 0x0F) != 0 || len == 0) {
		ret = -0x400;
	} else if (len > 0x400) {
		ret = -0x401;
	} else {
		u32 blocks = len >> 4;
		u32 *work = (u32 *)g_aes_data_ptr;
		for (u32 i = 0; i < blocks; i++) {
			u32 *src = data + (i * 4);
			u32 *dst = work + (i * 4);
			dst[0] = (src[0] ^ mask[0]) ^ i;
			dst[1] = src[1] ^ mask[1];
			dst[2] = src[2] ^ mask[2];
			dst[3] = src[3] ^ mask[3];
		}
		ret = kirkEncryptInPlace((u8 *)&g_aes_work, len, 7);
		if (ret == 0) {
			work = (u32 *)g_aes_data_ptr;
			for (u32 i = 0; i < blocks; i++) {
				u32 *src = work + (i * 4);
				u32 *dst = data + (i * 4);
				dst[0] = src[0] ^ mask[0];
				dst[1] = src[1] ^ mask[1];
				dst[2] = src[2] ^ mask[2];
				dst[3] = (src[3] ^ mask[3]) ^ i;
			}
		}
	}
	if (sceKernelSignalSema(g_cipher_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x2984: sceMemab_driver_9BF1A0A4 */
int sceMemab_driver_9BF1A0A4(const u32 *mask, u32 *data, u32 len)
{
	if (sceKernelWaitSema(g_cipher_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret;
	if ((len & 0x0F) != 0 || len == 0) {
		ret = -0x400;
	} else if (len > 0x400) {
		ret = -0x401;
	} else {
		u32 blocks = len >> 4;
		u32 *work = (u32 *)g_aes_data_ptr;
		for (u32 i = 0; i < blocks; i++) {
			u32 *src = data + (i * 4);
			u32 *dst = work + (i * 4);
			dst[0] = src[0] ^ mask[0];
			dst[1] = src[1] ^ mask[1];
			dst[2] = src[2] ^ mask[2];
			dst[3] = (src[3] ^ mask[3]) ^ i;
		}
		ret = kirkDecryptInPlace((u8 *)&g_aes_work, len, 7);
		if (ret == 0) {
			u32 *out_work = (u32 *)&g_aes_work;
			for (u32 i = 0; i < blocks; i++) {
				u32 *src = out_work + (i * 4);
				u32 *dst = data + (i * 4);
				dst[0] = (src[0] ^ mask[0]) ^ i;
				dst[1] = src[1] ^ mask[1];
				dst[2] = src[2] ^ mask[2];
				dst[3] = src[3] ^ mask[3];
			}
		}
	}
	if (sceKernelSignalSema(g_cipher_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x2B38: sceMemab_driver_16594684 */
int sceMemab_driver_16594684(u32 *out_rand)
{
	if (sceKernelWaitSema(g_cipher_sema, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x100;
	if (sceUtilsBufferCopyWithRange(g_aes_hdr_ptr, 0x14, NULL, 0, 14) == 0) {
		u32 *src = (u32 *)&g_aes_work;
		for (u32 i = 0; i < 4; i++) {
			out_rand[i] = src[i];
		}
		ret = 0;
	}
	if (sceKernelSignalSema(g_cipher_sema, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}
