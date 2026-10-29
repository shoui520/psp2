#include <common_imp.h>

extern SceUID sceKernelCreateSema(const char *name, SceUInt attr, int initVal, int maxVal, void *option);
extern int sceKernelDeleteSema(SceUID semaid);
extern int sceKernelWaitSema(SceUID semaid, int signal, SceUInt *timeout);
extern int sceKernelSignalSema(SceUID semaid, int signal);
extern int sceIdStorageLookup(u16 key, u32 offset, void *buf, u32 len);
extern int sceUtilsBufferCopyWithRange(void *outbuf, SceSize outsize, void *inbuf, int insize, int cmd);

static u8 g_buf_2fc0[0x80];     /* 0x2FC0 */
static u8 g_buf_3140[0x938];    /* 0x3140 */
static u8 g_buf_3a80[0x14];     /* 0x3A80 */
static u8 g_buf_3a94[0x42C];    /* 0x3A94 */

static u8 *g_ptr_2fc0 = g_buf_2fc0;         /* 0x2E00 */
static u8 *g_ptr_3140 = g_buf_3140;         /* 0x2E04 */
static u8 *g_ptr_3a80 = g_buf_3a80;         /* 0x2E08 */
static u8 *g_ptr_3a94_0 = g_buf_3a94;       /* 0x2E0C */
static u8 *g_ptr_3a94 = g_buf_3a94;         /* 0x2E10 */

static int g_inited;                        /* 0x2E40 */
static u64 g_seq_counter;                   /* 0x2E48..0x2E4F */
static u8 g_session_key[0x20];              /* 0x2E80 */
static SceUID g_sema1;                      /* 0x2EA0 */
static u8 g_rand_buf[0x40];                 /* 0x2EC0 */
static u8 g_idstorage_cert[0xC0];           /* 0x2F00 */
static u8 g_ecdh_buf[0x40];                 /* 0x3040 */
static u8 g_ecdsa_buf[0x80];                /* 0x3080 */
static u8 g_aes_buf[0x40];                  /* 0x3100 */
static SceUID g_sema2;                      /* 0x3A78 */

static void copy_bytes(u8 *dst, const u8 *src, u32 len)
{
	for (u32 i = 0; i < len; i++) {
		dst[i] = src[i];
	}
}

static void zero_bytes(u8 *dst, u32 len)
{
	for (u32 i = 0; i < len; i++) {
		dst[i] = 0;
	}
}

static void pack_u64_le(u8 *dst, u64 val)
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

static u64 unpack_u64_le(const u8 *src)
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

/* 0x0000: sub_0000 */
static void sub_0000(u8 *dst, const u8 *s1, u32 l1, const u8 *s2, u32 l2,
                     const u8 *s3, u32 l3, const u8 *s4, u32 l4)
{
	if (l1 != 0) copy_bytes(dst, s1, l1);
	dst += l1;
	if (l2 != 0) copy_bytes(dst, s2, l2);
	dst += l2;
	if (l3 != 0) copy_bytes(dst, s3, l3);
	dst += l3;
	if (l4 != 0) copy_bytes(dst, s4, l4);
	dst += l4;
	u32 rem = 0xC0 - (l1 + l2 + l3 + l4);
	if (rem != 0) zero_bytes(dst, rem);
}

/* 0x00E4: sub_00e4 (SHA-1 via Kirk cmd 11) */
static int sub_00e4(u8 *buf, const u8 *s1, u32 l1, const u8 *s2, u32 l2,
                    const u8 *s3, u32 l3)
{
	u32 total = l1 + l2 + l3;
	*(u32 *)buf = total;
	u8 *p = buf + 4;
	if (l1 != 0) copy_bytes(p, s1, l1);
	p += l1;
	if (l2 != 0) copy_bytes(p, s2, l2);
	p += l2;
	if (l3 != 0) copy_bytes(p, s3, l3);
	u32 in_sz = total + 4;
	if (in_sz < 0x14) in_sz = 0x14;
	int r = sceUtilsBufferCopyWithRange(buf, in_sz, buf, 0x14, 11);
	return (r == 0) ? 0 : -0x102;
}

/* 0x01B8: sub_01b8 (ECDSA verify via Kirk cmd 17) */
static int sub_01b8(u8 *work, const u8 *pub, const u8 *hash, const u8 *sig)
{
	copy_bytes(work, pub, 0x28);
	copy_bytes(work + 0x28, hash, 0x14);
	copy_bytes(work + 0x3C, sig, 0x28);
	int r = sceUtilsBufferCopyWithRange(NULL, 0, work, 0x64, 17);
	return (r == 0) ? 0 : -0x10C;
}

/* 0x025C: sub_025c (Read & verify IDStorage certificate via Kirk cmd 18) */
static int sub_025c(u8 *cert_buf)
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

/* 0x02E0: sub_02e0 (Kirk cmd 4 encrypt with IV from PRNG cmd 14) */
static int sub_02e0(u8 *work, u32 keyslot, const u8 *s1, u32 l1, const u8 *s2, u32 l2,
                    const u8 *s3, u32 l3, const u8 *s4, u32 l4, const u8 *s5, u32 l5,
                    const u8 *s6, u32 l6)
{
	u32 total = l1 + l2 + l3 + l4 + l5 + l6;
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
	*(u32 *)(work + 0x00) = 4;
	*(u32 *)(work + 0x04) = 0;
	*(u32 *)(work + 0x08) = 0;
	*(u32 *)(work + 0x0C) = keyslot;
	*(u32 *)(work + 0x10) = aligned;
	if (l1 != 0) copy_bytes(p, s1, l1);
	p += l1;
	if (l2 != 0) copy_bytes(p, s2, l2);
	p += l2;
	if (l3 != 0) copy_bytes(p, s3, l3);
	p += l3;
	if (l4 != 0) copy_bytes(p, s4, l4);
	p += l4;
	if (l5 != 0) copy_bytes(p, s5, l5);
	p += l5;
	if (l6 != 0) copy_bytes(p, s6, l6);
	u32 sz = aligned + 0x14;
	int r = sceUtilsBufferCopyWithRange(work, sz, work, sz, 4);
	return (r == 0) ? 0 : -0x10A;
}

/* 0x0534: sub_0534 (Kirk cmd 4 encrypt in-place) */
static int sub_0534(u8 *work, u32 len, u32 keyslot)
{
	*(u32 *)(work + 0x00) = 4;
	*(u32 *)(work + 0x04) = 0;
	*(u32 *)(work + 0x08) = 0;
	*(u32 *)(work + 0x0C) = keyslot;
	*(u32 *)(work + 0x10) = len;
	u32 sz = len + 0x14;
	int r = sceUtilsBufferCopyWithRange(work, sz, work, sz, 4);
	return (r == 0) ? 0 : -0x10A;
}

/* 0x0580: sub_0580 (Kirk cmd 7 decrypt in-place) */
static int sub_0580(u8 *work, u32 len, u32 keyslot)
{
	*(u32 *)(work + 0x00) = 5;
	*(u32 *)(work + 0x04) = 0;
	*(u32 *)(work + 0x08) = 0;
	*(u32 *)(work + 0x0C) = keyslot;
	*(u32 *)(work + 0x10) = len;
	u32 sz = len + 0x14;
	int r = sceUtilsBufferCopyWithRange(work, sz, work, sz, 7);
	return (r == 0) ? 0 : -0x10A;
}

/* 0x05D0: sub_05d0 (Copy & Kirk cmd 7 decrypt) */
static int sub_05d0(u8 *work, u32 keyslot, const u8 *src, u32 len)
{
	if (len != 0) {
		copy_bytes(work + 0x14, src, len);
	}
	return sub_0580(work, len, keyslot);
}

/* 0x0620: sub_0620 (Generate keystream blocks via Kirk cmd 7 keyslot 0x44) */
static int sub_0620(u8 *work, u32 len, u64 *counter, const u32 *key)
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
	return sub_0580(work, len, 0x44);
}

/* 0x06E0: sub_06e0 (CTR encrypt + HMAC-SHA1 tag) */
static int sub_06e0(u8 *work, u8 *data, u32 len, u64 *counter, const u8 *key, u8 *hdr, int do_enc)
{
	pack_u64_le(hdr, *counter);
	zero_bytes(hdr + 8, 8);
	if (do_enc != 0) {
		if (len == 0) {
			return -0x400;
		}
		int r = sub_0620(work, len, counter, (const u32 *)key);
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
	int r = sub_00e4(work, key, 0x14, hdr, 0x10, data, len);
	if (r != 0) {
		return r;
	}
	copy_bytes(hdr + 0x10, work, 0x10);
	return 0;
}

/* 0x089C: sub_089c (Verify HMAC-SHA1 tag + CTR decrypt) */
static int sub_089c(u8 *work, u8 *data, u32 len, u64 *counter, u64 *min_counter,
                    const u8 *key, const u8 *hdr, int do_dec, int check_seq)
{
	int r = sub_00e4(work, key, 0x14, hdr, 0x10, data, len);
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
		r = sub_0620(work, len, counter, (const u32 *)key);
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
	SceUID s1 = sceKernelCreateSema("SceMemab1", 0, 1, 1, NULL);
	g_sema1 = s1;
	if (s1 <= 0) {
		return 1;
	}
	SceUID s2 = sceKernelCreateSema("SceMemab2", 0, 1, 1, NULL);
	g_sema2 = s2;
	if (s2 <= 0) {
		sceKernelDeleteSema(g_sema1);
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
	if (sceKernelWaitSema(g_sema1, 1, &timeout) != 0) {
		return 1;
	}
	if (sceKernelWaitSema(g_sema2, 1, &timeout) != 0) {
		return 1;
	}
	sceKernelDeleteSema(g_sema1);
	sceKernelDeleteSema(g_sema2);
	return 0;
}

/* 0x0B2C: sceMemab_driver_6DD7339A */
int sceMemab_driver_6DD7339A(void)
{
	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
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
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x0BE4: sceMemab_driver_D57856A7 */
int sceMemab_driver_D57856A7(void)
{
	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = 0;
	if (g_inited != 0) {
		zero_bytes(g_session_key, 0x20);
		g_inited = 0;
		g_seq_counter = 0;
	}
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x0C8C: sceMemab_driver_4B54EAAD */
int sceMemab_driver_4B54EAAD(u8 *out_plain, u8 *out_enc)
{
	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		ret = sub_025c(g_idstorage_cert);
		if (ret == 0) {
			if (sceUtilsBufferCopyWithRange(g_rand_buf, 0x3C, NULL, 0, 12) != 0) {
				ret = -0x100;
			} else {
				sub_0000(out_plain, g_rand_buf, 0x3C, g_idstorage_cert, 0x10, NULL, 0, NULL, 0);
				ret = sub_02e0(g_buf_3140, 2, g_rand_buf + 0x14, 0x28, g_idstorage_cert, 0x60,
				               NULL, 0, NULL, 0, NULL, 0, NULL, 0);
				if (ret == 0) {
					copy_bytes(out_enc, g_ptr_3140 + 0x14, 0xA0);
				}
			}
		}
	}
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x0DF8: sceMemab_driver_9BF0C95D */
int sceMemab_driver_9BF0C95D(u8 *out_plain, const u8 *in_enc, u8 *out_enc)
{
	u8 sp_buf[0x60];
	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		ret = sub_05d0(g_buf_3140, 2, in_enc, 0xA0);
		if (ret == 0) {
			ret = sub_00e4(g_buf_2fc0, g_ptr_3140 + 0x38, 0x38, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			ret = sub_025c(g_idstorage_cert);
		}
		if (ret == 0) {
			if (sub_01b8(g_ecdsa_buf, g_idstorage_cert + 0x60, g_ptr_2fc0, g_ptr_3140 + 0x70) != 0) {
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
			copy_bytes(g_ecdh_buf + 0x14, g_ptr_3140 + 0x10, 0x28);
			if (sceUtilsBufferCopyWithRange(g_ecdh_buf, 0x3C, g_ecdh_buf, 0x3C, 13) != 0) {
				ret = -0x103;
			}
		}
		if (ret == 0) {
			copy_bytes(sp_buf, g_rand_buf + 0x14, 0x28);
			copy_bytes(sp_buf + 0x28, g_ptr_3140 + 0x10, 0x28);
			copy_bytes(sp_buf + 0x50, g_ptr_3140 + 0x38, 0x10);
			ret = sub_00e4(g_buf_2fc0, sp_buf, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			ret = sub_025c(g_idstorage_cert);
		}
		if (ret == 0) {
			copy_bytes(g_aes_buf, g_idstorage_cert + 0x88, 0x20);
			copy_bytes(g_aes_buf + 0x20, g_ptr_2fc0, 0x14);
			if (sceUtilsBufferCopyWithRange(g_aes_buf, 0x34, g_aes_buf, 0x34, 16) != 0) {
				ret = -0x104;
			}
		}
		if (ret == 0) {
			ret = sub_00e4(g_buf_2fc0, g_ecdh_buf, 0x14, sp_buf, 0x60, NULL, 0);
		}
		if (ret == 0) {
			sub_0000(out_plain, g_ptr_3140 + 0x48, 0x28, sp_buf, 0x50,
			         g_idstorage_cert, 0x10, g_ecdh_buf, 0x14);
			ret = sub_02e0(g_buf_3140, 2, sp_buf, 0x60, g_aes_buf, 0x28,
			               g_ptr_2fc0, 0x14, g_idstorage_cert, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			copy_bytes(out_enc, g_ptr_3140 + 0x14, 0x110);
		}
	}
	zero_bytes(sp_buf, 0x60);
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x1224: sceMemab_driver_C3981EE1 */
int sceMemab_driver_C3981EE1(u8 *in_out_plain, const u8 *in_enc, u8 *out_enc)
{
	u8 sp20[0x60];
	u8 sp80[0x08];
	u8 sp90[0x20];
	u8 spb0[0x20];
	u64 ctr = 0;

	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		ret = sub_05d0(g_buf_3140, 2, in_enc, 0x110);
		if (ret == 0) {
			ret = sub_00e4(g_buf_2fc0, g_ptr_3140 + 0xAC, 0x38, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			ret = sub_025c(g_idstorage_cert);
		}
		if (ret == 0) {
			if (sub_01b8(g_ecdsa_buf, g_idstorage_cert + 0x60, g_ptr_2fc0, g_ptr_3140 + 0xE4) != 0) {
				ret = -0x301;
			}
		}
		if (ret == 0) {
			ret = sub_00e4(g_buf_2fc0, g_ptr_3140 + 0x10, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			if (sub_01b8(g_ecdsa_buf, g_ptr_3140 + 0xBC, g_ptr_2fc0, g_ptr_3140 + 0x70) != 0) {
				ret = -0x303;
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x28; i++) {
				if (in_out_plain[0x14 + i] != g_ptr_3140[0x38 + i]) {
					ret = -0x308;
					break;
				}
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x10; i++) {
				if (in_out_plain[0x3C + i] != g_ptr_3140[0x60 + i]) {
					ret = -0x304;
					break;
				}
			}
		}
		if (ret == 0) {
			copy_bytes(g_ecdh_buf, in_out_plain, 0x14);
			copy_bytes(g_ecdh_buf + 0x14, g_ptr_3140 + 0x10, 0x28);
			if (sceUtilsBufferCopyWithRange(g_ecdh_buf, 0x3C, g_ecdh_buf, 0x3C, 13) != 0) {
				ret = -0x105;
			}
		}
		if (ret == 0) {
			ret = sub_00e4(g_buf_2fc0, g_ecdh_buf, 0x14, g_ptr_3140 + 0x10, 0x60, NULL, 0);
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x14; i++) {
				if (g_ptr_3140[0x98 + i] != g_ptr_2fc0[i]) {
					ret = -0x305;
					break;
				}
			}
		}
		if (ret == 0) {
			copy_bytes(sp20, g_ptr_3140 + 0x38, 0x28);
			copy_bytes(sp20 + 0x28, g_ptr_3140 + 0x10, 0x28);
			copy_bytes(sp20 + 0x50, g_ptr_3140 + 0xAC, 0x10);
			ret = sub_00e4(g_buf_2fc0, sp20, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			ret = sub_025c(g_idstorage_cert);
		}
		if (ret == 0) {
			copy_bytes(g_aes_buf, g_idstorage_cert + 0x88, 0x20);
			copy_bytes(g_aes_buf + 0x20, g_ptr_2fc0, 0x14);
			if (sceUtilsBufferCopyWithRange(g_aes_buf, 0x34, g_aes_buf, 0x34, 16) != 0) {
				ret = -0x104;
			}
		}
		if (ret == 0) {
			ret = sub_00e4(g_buf_2fc0, g_ecdh_buf, 0x14, sp20, 0x60, NULL, 0);
		}
		if (ret == 0) {
			sub_0000(in_out_plain, g_ecdh_buf, 0x14, NULL, 0, NULL, 0, NULL, 0);
			copy_bytes(spb0, g_session_key, 0x20);
			ret = sub_06e0(g_ptr_3140, spb0, 0x20, &ctr, in_out_plain, sp90, 1);
		}
		if (ret == 0) {
			pack_u64_le(sp80, g_seq_counter);
			ret = sub_02e0(g_buf_3140, 2, sp20, 0x60, g_aes_buf, 0x28,
			               g_ptr_2fc0, 0x14, spb0, 0x20, sp80, 0x08, sp90, 0x20);
		}
		if (ret == 0) {
			copy_bytes(out_enc, g_ptr_3140 + 0x14, 0x100);
		}
	}
	zero_bytes(sp20, 0x60);
	zero_bytes(sp80, 0x08);
	zero_bytes(sp90, 0x20);
	zero_bytes(spb0, 0x20);
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x18B4: sceMemab_driver_23E4659B */
int sceMemab_driver_23E4659B(u8 *in_out_plain, const u8 *in_enc, u8 *out_enc)
{
	u8 sp20[0x08];
	u8 sp30[0x20];
	u8 sp50[0x20];
	u64 ctr = 0;
	u64 min_ctr = 0;

	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		ret = sub_05d0(g_buf_3140, 2, in_enc, 0x100);
		if (ret == 0) {
			ret = sub_00e4(g_buf_2fc0, g_ptr_3140 + 0x10, 0x60, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			if (sub_01b8(g_ecdsa_buf, in_out_plain, g_ptr_2fc0, g_ptr_3140 + 0x70) != 0) {
				ret = -0x307;
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x28; i++) {
				if (in_out_plain[0x28 + i] != g_ptr_3140[0x38 + i]) {
					ret = -0x30D;
					break;
				}
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x28; i++) {
				if (in_out_plain[0x50 + i] != g_ptr_3140[0x10 + i]) {
					ret = -0x308;
					break;
				}
			}
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x10; i++) {
				if (in_out_plain[0x78 + i] != g_ptr_3140[0x60 + i]) {
					ret = -0x30E;
					break;
				}
			}
		}
		if (ret == 0) {
			ret = sub_00e4(g_buf_2fc0, in_out_plain + 0x88, 0x14, g_ptr_3140 + 0x10, 0x60, NULL, 0);
		}
		if (ret == 0) {
			for (u32 i = 0; i < 0x14; i++) {
				if (g_ptr_2fc0[i] != g_ptr_3140[0x98 + i]) {
					ret = -0x309;
					break;
				}
			}
		}
		if (ret == 0) {
			sub_0000(in_out_plain, in_out_plain + 0x88, 0x14, g_ptr_3140 + 0xAC, 0x20,
			         g_ptr_3140 + 0xCC, 0x08, NULL, 0);
			ret = sub_089c(g_ptr_3140, in_out_plain + 0x14, 0x20, &ctr, &min_ctr,
			               in_out_plain, g_ptr_3140 + 0xD4, 1, 0);
		}
		if (ret == 0) {
			ctr = 0;
			copy_bytes(sp50, g_session_key, 0x20);
			ret = sub_06e0(g_ptr_3140, sp50, 0x20, &ctr, in_out_plain, sp30, 1);
		}
		if (ret == 0) {
			pack_u64_le(sp20, g_seq_counter);
			ret = sub_02e0(g_buf_3140, 2, sp50, 0x20, sp20, 0x08, sp30, 0x20,
			               NULL, 0, NULL, 0, NULL, 0);
		}
		if (ret == 0) {
			copy_bytes(out_enc, g_ptr_3140 + 0x14, 0x60);
		}
	}
	zero_bytes(sp20, 0x08);
	zero_bytes(sp30, 0x20);
	zero_bytes(sp50, 0x20);
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x1D6C: sceMemab_driver_8ABE3445 */
int sceMemab_driver_8ABE3445(u8 *in_out_plain, const u8 *in_enc)
{
	u8 sp10[0x60];
	u64 ctr = 0;
	u64 min_ctr = 0;

	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x200;
	if (g_inited != 0) {
		copy_bytes(sp10, in_enc, 0x60);
		ret = sub_05d0(g_buf_3140, 2, sp10, 0x60);
		if (ret == 0) {
			copy_bytes(sp10, g_ptr_3140 + 0x10, 0x60);
			sub_0000(in_out_plain, in_out_plain, 0x14, sp10, 0x20, sp10 + 0x20, 0x08, NULL, 0);
			ret = sub_089c(g_ptr_3140, in_out_plain + 0x14, 0x20, &ctr, &min_ctr,
			               in_out_plain, sp10 + 0x28, 1, 0);
		}
	}
	zero_bytes(sp10, 0x60);
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x1EE4: sceMemab_driver_3C15BC8C */
int sceMemab_driver_3C15BC8C(u8 *ctx, u8 *data, u32 len, u8 *hdr, int do_enc)
{
	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
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
			ret = sub_06e0(g_ptr_3140, data, len, &ctr, ctx, hdr, do_enc);
			if (ret == 0) {
				pack_u64_le(ctx + 0x3C, ctr);
			}
		}
	}
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x2100: sceMemab_driver_D47A50B1 */
int sceMemab_driver_D47A50B1(u8 *ctx, u8 *data, u32 len, const u8 *hdr, int do_dec, int check_seq)
{
	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
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
			ret = sub_089c(g_ptr_3140, data, len, &ctr, &min_ctr, ctx, hdr, do_dec, check_seq);
			if (ret == 0 && check_seq != 0) {
				pack_u64_le(ctx + 0x44, ctr);
			}
		}
	}
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x23E4: sceMemab_driver_F742F283 */
int sceMemab_driver_F742F283(u8 *data, u32 len, u8 *hdr, int do_enc)
{
	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
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
			ret = sub_06e0(g_ptr_3140, data, len, &ctr, g_session_key, hdr, do_enc);
			if (ret == 0) {
				g_seq_counter = ctr;
			}
		}
	}
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x24F4: sceMemab_driver_CB5D3916 */
int sceMemab_driver_CB5D3916(u8 *ctx, u8 *data, u32 len, const u8 *hdr, int do_dec, int check_seq)
{
	if (sceKernelWaitSema(g_sema1, 1, NULL) != 0) {
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
			ret = sub_089c(g_ptr_3140, data, len, &ctr, &min_ctr, ctx + 0x14, hdr, do_dec, check_seq);
			if (ret == 0 && check_seq != 0) {
				pack_u64_le(ctx + 0x34, ctr);
			}
		}
	}
	if (sceKernelSignalSema(g_sema1, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x27D8: sceMemab_driver_9DE8C8CD */
int sceMemab_driver_9DE8C8CD(const u32 *mask, u32 *data, u32 len)
{
	if (sceKernelWaitSema(g_sema2, 1, NULL) != 0) {
		return -0x109;
	}
	int ret;
	if ((len & 0x0F) != 0 || len == 0) {
		ret = -0x400;
	} else if (len > 0x400) {
		ret = -0x401;
	} else {
		u32 blocks = len >> 4;
		u32 *work = (u32 *)g_ptr_3a94;
		for (u32 i = 0; i < blocks; i++) {
			u32 *src = data + (i * 4);
			u32 *dst = work + (i * 4);
			dst[0] = (src[0] ^ mask[0]) ^ i;
			dst[1] = src[1] ^ mask[1];
			dst[2] = src[2] ^ mask[2];
			dst[3] = src[3] ^ mask[3];
		}
		ret = sub_0534(g_buf_3a80, len, 7);
		if (ret == 0) {
			work = (u32 *)g_ptr_3a94;
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
	if (sceKernelSignalSema(g_sema2, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x2984: sceMemab_driver_9BF1A0A4 */
int sceMemab_driver_9BF1A0A4(const u32 *mask, u32 *data, u32 len)
{
	if (sceKernelWaitSema(g_sema2, 1, NULL) != 0) {
		return -0x109;
	}
	int ret;
	if ((len & 0x0F) != 0 || len == 0) {
		ret = -0x400;
	} else if (len > 0x400) {
		ret = -0x401;
	} else {
		u32 blocks = len >> 4;
		u32 *work = (u32 *)g_ptr_3a94;
		for (u32 i = 0; i < blocks; i++) {
			u32 *src = data + (i * 4);
			u32 *dst = work + (i * 4);
			dst[0] = src[0] ^ mask[0];
			dst[1] = src[1] ^ mask[1];
			dst[2] = src[2] ^ mask[2];
			dst[3] = (src[3] ^ mask[3]) ^ i;
		}
		ret = sub_0580(g_buf_3a80, len, 7);
		if (ret == 0) {
			u32 *out_work = (u32 *)g_buf_3a80;
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
	if (sceKernelSignalSema(g_sema2, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}

/* 0x2B38: sceMemab_driver_16594684 */
int sceMemab_driver_16594684(u32 *out_rand)
{
	if (sceKernelWaitSema(g_sema2, 1, NULL) != 0) {
		return -0x109;
	}
	int ret = -0x100;
	if (sceUtilsBufferCopyWithRange(g_ptr_3a80, 0x14, NULL, 0, 14) == 0) {
		u32 *src = (u32 *)g_buf_3a80;
		for (u32 i = 0; i < 4; i++) {
			out_rand[i] = src[i];
		}
		ret = 0;
	}
	if (sceKernelSignalSema(g_sema2, 1) != 0) {
		ret = -0x109;
	}
	return ret;
}
