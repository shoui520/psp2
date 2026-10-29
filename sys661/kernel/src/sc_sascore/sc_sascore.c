#include <common_imp.h>

static inline u32 pspSdkGetK1(void)
{
	u32 k1;
	__asm__ volatile("move %0, $k1" : "=r"(k1));
	return k1;
}

static inline void pspSdkSetK1(u32 k1)
{
	__asm__ volatile("move $k1, %0" : : "r"(k1));
}

extern void *sceMeCalloc(int num, int size);
extern void sceMeFree(void *ptr);
extern int sceMeCore_driver_FA398D71(int cmd, ...);
extern int sceMeCore_driver_635397BB(int cmd, ...);
extern SceUID sceKernelCreateSema(const char *name, SceUInt attr, int initVal, int maxVal, void *option);
extern int sceKernelDeleteSema(SceUID semaid);
extern int sceKernelWaitSema(SceUID semaid, int signal, SceUInt *timeout);
extern int sceKernelSignalSema(SceUID semaid, int signal);
extern void sceKernelDcacheWritebackInvalidateRange(const void *p, unsigned int size);
extern void sceKernelDcacheWritebackRange(const void *p, unsigned int size);
extern void sceKernelDcacheWritebackInvalidateAll(void);
extern int sceKernelDcacheInvalidateRangeForUser(const void *p, unsigned int size);
extern int sceKernelGetCompiledSdkVersion(void);

typedef struct SceSasVoice {
	u32 src_addr;      /* 0x00 (+0x14 in core) */
	u32 loop_addr;     /* 0x04 (+0x18 in core) */
	u8 type;           /* 0x08 (+0x1C in core) */
	u8 loop_mode;      /* 0x09 (+0x1D in core) */
	u16 pitch;         /* 0x0A (+0x1E in core) */
	u16 vol_l;         /* 0x0C (+0x20 in core) */
	u16 vol_r;         /* 0x0E (+0x22 in core) */
	u16 vol_wl;        /* 0x10 (+0x24 in core) */
	u16 vol_wr;        /* 0x12 (+0x26 in core) */
	u16 vol_unk;       /* 0x14 (+0x28 in core) */
	u16 pad16;         /* 0x16 (+0x2A in core) */
	s32 ar;            /* 0x18 (+0x2C in core) */
	s32 dr;            /* 0x1C (+0x30 in core) */
	s32 sr;            /* 0x20 (+0x34 in core) */
	s32 rr;            /* 0x24 (+0x38 in core) */
	s32 sl;            /* 0x28 (+0x3C in core) */
	u32 curve_mode;    /* 0x2C (+0x40 in core) */
	u8 unk30;          /* 0x30 (+0x44 in core) */
	u8 unk31;          /* 0x31 (+0x45 in core) */
	u8 unk32;          /* 0x32 (+0x46 in core) */
	s8 env_state;      /* 0x33 (+0x47 in core) */
	s32 env_height;    /* 0x34 (+0x48 in core) */
} SceSasVoice; /* size = 0x38 */

typedef struct SceSasCore {
	u32 me_buf;                 /* 0x000 */
	u32 flags;                  /* 0x004 */
	u8 grain_shift;             /* 0x008 */
	u8 output_mode;             /* 0x009 */
	u8 wet_dry;                 /* 0x00A */
	s8 busy;                    /* 0x00B */
	u16 rev_vol_l;              /* 0x00C */
	u16 rev_vol_r;              /* 0x00E */
	u32 end_flags;              /* 0x010 */
	SceSasVoice voices[32];     /* 0x014..0x714 */
	SceSasVoice voices_bak[32]; /* 0x714..0xE14 */
	u32 end_flags_bak;          /* 0xE14 */
	u8 wet_dry_bak;             /* 0xE18 */
	u8 pad_e19;                 /* 0xE19 */
	u16 rev_vol_l_bak;          /* 0xE1A */
	u16 rev_vol_r_bak;          /* 0xE1C */
	u16 pad_e1e;                /* 0xE1E */
} SceSasCore; /* size = 0xE20 */

static const s8 g_keyon_state[8] = { -2, -2, 0, 1, 2, 5, 5, 5 };
static const s8 g_keyoff_state[8] = { 4, -1, 3, 3, 3, 3, 4, -1 };
static const s16 g_voice_prio[12] = { -254, -249, 0, 1, 2, 2, 3, 3, 4, 4, 5, 6 };

static void *g_me_sas_buf; /* 0x2E90 */
static SceUID g_sas_sema;  /* 0x2E94 */

static inline int check_valid_ram(u32 addr, u32 mask)
{
	return (mask >> (addr >> 27)) & 1;
}

/* 0x0000: __sceSasCore */
int sceSasCore_A3589D81(SceSasCore *sasCore, void *sasOut)
{
	if (((u32)sasCore | (u32)sasOut) & 0x3F) {
		return 0x80420005;
	}
	if (!check_valid_ram((u32)sasCore, 0x00220202) ||
	    !check_valid_ram((u32)sasOut, 0x80420202)) {
		return 0x80420005;
	}

	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		return 0x80000023;
	}
	pspSdkSetK1(k1_check);

	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	int flags = res;
	if (res >= 0) {
		if (sasCore->busy != 0) {
			sceKernelSignalSema(g_sas_sema, 1);
			pspSdkSetK1(old_k1);
			return 0x80420030;
		}
		sasCore->busy = 1;
		u32 *src = (u32 *)sasCore->voices;
		u32 *dst = (u32 *)sasCore->voices_bak;
		u32 *end = dst;
		while (src != end) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
			src += 4;
			dst += 4;
		}
		for (int i = 0; i < 32; i++) {
			s8 st = sasCore->voices[i].env_state;
			if (st == -2 || st == 5) {
				sasCore->voices[i].env_state = 0;
			}
		}
		sasCore->wet_dry_bak = sasCore->wet_dry;
		flags = sasCore->flags;
		sasCore->rev_vol_l_bak = sasCore->rev_vol_l;
		sasCore->rev_vol_r_bak = sasCore->rev_vol_r;
		*((u8 *)&sasCore->flags + 1) = 0;
		sceKernelSignalSema(g_sas_sema, 1);
	}
	if (flags < 0) {
		pspSdkSetK1(old_k1);
		return flags;
	}

	u32 out_size = (u32)sasCore->grain_shift * (((u32)sasCore->output_mode << 7) + 0x80);
	u32 addr_or = (u32)sasOut | ((u32)sasOut + out_size);
	for (int i = 0; i < 32; i++) {
		if (sasCore->voices[i].env_state != -1) {
			u8 type = sasCore->voices[i].type;
			if (type == 1) {
				addr_or |= sasCore->voices[i].src_addr | sasCore->voices[i].loop_addr;
			}
			if (type == 5) {
				addr_or |= sasCore->voices[i].src_addr;
			}
			if (type == 6) {
				u32 atr = sasCore->voices[i].src_addr;
				sceKernelDcacheWritebackInvalidateRange((void *)(atr + 0x80), 0x48);
				addr_or |= atr;
			}
		}
	}
	if ((s32)(k1_check & addr_or) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}

	sceKernelDcacheWritebackInvalidateRange(sasCore->voices_bak, 0x70C);
	out_size = (u32)sasCore->grain_shift * (((u32)sasCore->output_mode << 7) + 0x80);
	sceKernelDcacheInvalidateRangeForUser(sasOut, out_size);

	int me_ret = sceMeCore_driver_FA398D71(0x100, sasCore, 0x40000000 | ((u32)sasOut & 0x1FFFFFFF), flags);

	sceKernelWaitSema(g_sas_sema, 1, NULL);
	for (int i = 0; i < 32; i++) {
		u8 type = sasCore->voices[i].type;
		if (type & 0x10) {
			sasCore->voices[i].type = type | 0x30;
		} else {
			if (type & 0x20) {
				sasCore->voices[i].type = type - 0x10;
			}
			s8 st0 = sasCore->voices[i].env_state;
			s8 st1 = sasCore->voices_bak[i].env_state;
			if (st0 != st1) {
				s16 p0 = g_voice_prio[st0 + 2];
				s16 p1 = g_voice_prio[st1 + 2];
				sasCore->voices[i].env_state = (p0 > p1) ? p0 : p1;
			}
			sasCore->voices[i].env_height = sasCore->voices_bak[i].env_height;
		}
	}
	sasCore->busy = 0;
	sasCore->end_flags = sasCore->end_flags_bak;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return me_ret;
}

/* 0x0368: __sceSasCoreWithMix */
int sceSasCore_50A14DFC(SceSasCore *sasCore, void *sasInOut, int leftVol, int rightVol)
{
	if (((u32)sasCore | (u32)sasInOut) & 0x3F) {
		return 0x80420005;
	}
	if (!check_valid_ram((u32)sasCore, 0x00220202) ||
	    !check_valid_ram((u32)sasInOut, 0x80420202)) {
		return 0x80420005;
	}

	if (sceKernelGetCompiledSdkVersion() > 0x0300FFFF) {
		if ((u32)leftVol > 0x1000 || (u32)rightVol > 0x1000) {
			return 0x80420023;
		}
	} else {
		if (((u32)leftVol | (u32)rightVol) > 0x1000) {
			return 0x80420018;
		}
	}

	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		return 0x80000023;
	}
	pspSdkSetK1(k1_check);

	u32 out_size = (u32)sasCore->grain_shift * (((u32)sasCore->output_mode << 7) + 0x80);
	u32 addr_or = (u32)sasInOut | ((u32)sasInOut + out_size);
	for (int i = 0; i < 32; i++) {
		if (sasCore->voices[i].env_state != -1) {
			u8 type = sasCore->voices[i].type;
			if (type == 1) {
				addr_or |= sasCore->voices[i].src_addr | sasCore->voices[i].loop_addr;
			}
			if (type == 5) {
				addr_or |= sasCore->voices[i].src_addr;
			}
			if (type == 6) {
				u32 atr = sasCore->voices[i].src_addr;
				sceKernelDcacheWritebackInvalidateRange((void *)(atr + 0x80), 0x48);
				addr_or |= atr;
			}
		}
	}
	if ((s32)(k1_check & addr_or) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if (sasCore->output_mode != 0) {
		pspSdkSetK1(old_k1);
		return 0x80000004;
	}

	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	int flags = res;
	if (res >= 0) {
		if (sasCore->busy != 0) {
			sceKernelSignalSema(g_sas_sema, 1);
			pspSdkSetK1(old_k1);
			return 0x80420030;
		}
		sasCore->busy = 1;
		u32 *src = (u32 *)sasCore->voices;
		u32 *dst = (u32 *)sasCore->voices_bak;
		u32 *end = dst;
		while (src != end) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
			src += 4;
			dst += 4;
		}
		for (int i = 0; i < 32; i++) {
			s8 st = sasCore->voices[i].env_state;
			if (st == -2 || st == 5) {
				sasCore->voices[i].env_state = 0;
			}
		}
		sasCore->wet_dry_bak = sasCore->wet_dry;
		flags = sasCore->flags;
		sasCore->rev_vol_l_bak = sasCore->rev_vol_l;
		sasCore->rev_vol_r_bak = sasCore->rev_vol_r;
		*((u8 *)&sasCore->flags + 1) = 0;
		sceKernelSignalSema(g_sas_sema, 1);
	}
	if (flags < 0) {
		pspSdkSetK1(old_k1);
		return flags;
	}

	sceKernelDcacheWritebackInvalidateRange(sasCore->voices_bak, 0x70C);
	out_size = (u32)sasCore->grain_shift * (((u32)sasCore->output_mode << 7) + 0x80);
	sceKernelDcacheWritebackInvalidateRange(sasInOut, out_size);

	int me_ret = sceMeCore_driver_FA398D71(0x101, sasCore, sasInOut, flags, leftVol, rightVol);

	sceKernelWaitSema(g_sas_sema, 1, NULL);
	for (int i = 0; i < 32; i++) {
		u8 type = sasCore->voices[i].type;
		if (type & 0x10) {
			sasCore->voices[i].type = type | 0x30;
		} else {
			if (type & 0x20) {
				sasCore->voices[i].type = type - 0x10;
			}
			s8 st0 = sasCore->voices[i].env_state;
			s8 st1 = sasCore->voices_bak[i].env_state;
			if (st0 != st1) {
				s16 p0 = g_voice_prio[st0 + 2];
				s16 p1 = g_voice_prio[st1 + 2];
				sasCore->voices[i].env_state = (p0 > p1) ? p0 : p1;
			}
			sasCore->voices[i].env_height = sasCore->voices_bak[i].env_height;
		}
	}
	sasCore->busy = 0;
	sasCore->end_flags = sasCore->end_flags_bak;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return me_ret;
}

/* 0x0758: __sceSasGetEndFlag */
u32 sceSasCore_68A46B95(SceSasCore *sasCore)
{
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & (pspSdkGetK1() << 11)) < 0) {
		return 0x80000023;
	}
	return sasCore->end_flags;
}

/* 0x0780: __sceSasSetKeyOn */
int sceSasCore_76F01ACA(SceSasCore *sasCore, int vnum)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	if (v->type & 0x30) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80420016;
	}
	s8 old_st = v->env_state;
	s8 new_st = g_keyon_state[old_st + 2];
	v->env_state = new_st;
	if (new_st == old_st) {
		res = 0x80420016;
	}
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return res;
}

/* 0x08A0: __sceSasSetKeyOff */
int sceSasCore_A0CF2FA4(SceSasCore *sasCore, int vnum)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	if (v->type & 0x30) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80420016;
	}
	s8 old_st = v->env_state;
	s8 new_st = g_keyoff_state[old_st + 2];
	v->env_state = new_st;
	if (new_st == old_st) {
		res = 0x80420016;
	}
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return res;
}

/* 0x09C0: __sceSasSetVolume */
int sceSasCore_440CA7D8(SceSasCore *sasCore, int vnum, int l, int r, int wl, int wr)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	if (l < -0x1000 || l > 0x1000 || r < -0x1000 || r > 0x1000 ||
	    wl < -0x1000 || wl > 0x1000 || wr < -0x1000 || wr > 0x1000) {
		pspSdkSetK1(old_k1);
		return 0x80420018;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	v->vol_l = (u16)l;
	v->vol_r = (u16)r;
	v->vol_wl = (u16)wl;
	v->vol_wr = (u16)wr;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x0AD8: __sceSasSetPitch */
int sceSasCore_AD84D37F(SceSasCore *sasCore, int vnum, int pitch)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	if ((u32)pitch > 0x4000) {
		pspSdkSetK1(old_k1);
		return 0x80420012;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	if (sceKernelGetCompiledSdkVersion() <= 0x0602FFFF && pitch > 0x1000) {
		if ((v->type & 0x0F) == 5) {
			pitch = 0x1000;
		}
	}
	v->pitch = (u16)pitch;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x0BEC: __sceSasSetVoice */
int sceSasCore_99944089(SceSasCore *sasCore, int vnum, void *vagAddr, int size, int loopmode)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if (size == 0 || (size & 0x0F) != 0) {
		pspSdkSetK1(old_k1);
		return 0x80420014;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	if ((u32)loopmode >= 2) {
		pspSdkSetK1(old_k1);
		return 0x80420015;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	u8 t = v->type;
	u8 pause_bits = t & 0x30;
	u8 base_type = t - pause_bits;
	if (base_type != 1) {
		if (base_type == 6) {
			sceKernelSignalSema(g_sas_sema, 1);
			pspSdkSetK1(old_k1);
			return 0x80420040;
		}
		v->env_state = -1;
	}
	v->type = pause_bits + 1;
	v->src_addr = (u32)vagAddr;
	v->loop_addr = (u32)vagAddr + size;
	v->loop_mode = (u8)loopmode;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x0D48: __sceSasSetVoicePCM */
int sceSasCore_E1CD9561(SceSasCore *sasCore, int vnum, void *pcmBuf, int size, int loopsize)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)(size - 1) > 0xFFFF) {
		pspSdkSetK1(old_k1);
		return 0x8042001A;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	u8 loop_mode = 0;
	if (loopsize < 0) {
		loopsize = 0;
	} else if ((u32)loopsize >= (u32)size) {
		pspSdkSetK1(old_k1);
		return 0x80420015;
	} else {
		loop_mode = 1;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	u8 t = v->type;
	u8 pause_bits = t & 0x30;
	u8 base_type = t - pause_bits;
	if (base_type != 5) {
		if (base_type == 6) {
			sceKernelSignalSema(g_sas_sema, 1);
			pspSdkSetK1(old_k1);
			return 0x80420040;
		}
		v->env_state = -1;
	}
	v->type = pause_bits + 5;
	v->src_addr = (u32)pcmBuf;
	v->loop_addr = (u16)(size - 1) | ((u32)(u16)loopsize << 16);
	v->loop_mode = loop_mode;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x0EC0: __sceSasSetVoiceATRAC3 */
int sceSasCore_4AA9EAD6(SceSasCore *sasCore, int vnum, void *atrac3Context)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	u8 t = v->type;
	u8 pause_bits = t & 0x30;
	if ((u8)(t - pause_bits) == 6) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80420040;
	}
	v->src_addr = (u32)atrac3Context;
	v->env_state = -1;
	v->type = pause_bits + 6;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x0FC8: __sceSasConcatenateATRAC3 */
int sceSasCore_7497EA85(SceSasCore *sasCore, int vnum, void *buf, int len)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	if (sasCore->busy != 0) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80420030;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	if ((v->type & 0xCF) != 6) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80420041;
	}
	u32 ctx = v->src_addr;
	if ((s32)(((ctx + 0x100) | ctx) & k1_check) < 0) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if (*(s32 *)(ctx + 0xBC) != -1) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80420042;
	}
	*(u32 *)(ctx + 0xBC) = (u32)buf;
	*(u32 *)(ctx + 0xC4) = (u32)len;
	sceKernelDcacheWritebackRange(buf, len);
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x1144: __sceSasUnsetATRAC3 */
int sceSasCore_F6107F00(SceSasCore *sasCore, int vnum)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	if (sasCore->busy != 0) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80420030;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	if ((v->type & 0xCF) != 6) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80420041;
	}
	v->type = 0;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x124C: __sceSasSetNoise */
int sceSasCore_B7660A23(SceSasCore *sasCore, int vnum, int freq)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	if ((u32)freq >= 0x40) {
		pspSdkSetK1(old_k1);
		return 0x80420011;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	u8 pause_bits = v->type & 0x30;
	v->type = pause_bits + 2;
	v->src_addr = 0xFFFFFFFF;
	s32 step = 0x4000 >> (freq >> 2);
	if (step != 0) {
		v->loop_addr = step;
		u32 b = (0xFFFFEEAAu >> ((freq & 3) << 2)) & 0xFF;
		v->src_addr = b * 0x01010101u;
	} else {
		v->loop_addr = 1;
	}
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x1378: __sceSasSetSimpleADSR */
int sceSasCore_A232CBE6(SceSasCore *sasCore, int vnum, u32 env1, u32 env2)
{
	/* Note: disassembly @ 0x1378 takes (sasCore, vnum, duty_or_freq), wait:
	 * Let's check 0x1378: $a0=sasCore, $a1=vnum, $a2=val (0..100, < 0x65).
	 * Computes (val * 44100) / 100 (mult 0xAC44, multu 0x51EB851F, srl 5), sets type = pause_bits + 3!
	 * That's __sceSasSetTriangularWave / __sceSasSetSteepWave!
	 */
	(void)env2;
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	if (env1 >= 0x65) {
		pspSdkSetK1(old_k1);
		return 0x80420017;
	}
	u32 prod = env1 * 44100u;
	u32 val = (u32)(((u64)prod * 0x51EB851FULL) >> 37);
	if (val == 0) {
		val = 1;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	v->src_addr = val;
	v->type = (v->type & 0x30) + 3;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x148C: __sceSasSetSteepWave */
int sceSasCore_D5EBBBCD(SceSasCore *sasCore, int vnum, u32 freq)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	if (freq >= 0x65) {
		pspSdkSetK1(old_k1);
		return 0x80420017;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	u32 prod = freq * 44100u;
	u32 val = (u32)(((u64)prod * 0x51EB851FULL) >> 37);
	v->type = (v->type & 0x30) + 4;
	v->src_addr = val;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x15A0: __sceSasSetADSR */
int sceSasCore_019B25EB(SceSasCore *sasCore, int vnum, int flag, int a, int d, int s, int r)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	if (!(flag & 1)) a = v->ar;
	if (!(flag & 2)) d = v->dr;
	if (!(flag & 4)) s = v->sr;
	if (!(flag & 8)) r = v->rr;

	if ((a | d | s | r) < 0) {
		sceKernelSignalSema(g_sas_sema, 1);
		pspSdkSetK1(old_k1);
		return 0x80420019;
	}
	v->ar = a;
	v->dr = d;
	v->sr = s;
	v->rr = r;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x1708: __sceSasSetADSRmode */
int sceSasCore_9EC3676A(SceSasCore *sasCore, int vnum, int flag, int am, int dm, int sm, int rm)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	SceSasVoice *v = &sasCore->voices[vnum];
	u32 cm = v->curve_mode;
	if (flag & 1) {
		if ((u32)am >= 32 || !((0x15 >> am) & 1)) {
			sceKernelSignalSema(g_sas_sema, 1);
			pspSdkSetK1(old_k1);
			return 0x80420013;
		}
		cm = (cm & ~0xFFu) | (am & 0xFFu);
	}
	if (flag & 2) {
		if ((u32)dm >= 32 || !((0x2A >> dm) & 1)) {
			sceKernelSignalSema(g_sas_sema, 1);
			pspSdkSetK1(old_k1);
			return 0x80420013;
		}
		cm = (cm & ~0xFF00u) | ((dm & 0xFFu) << 8);
	}
	if (flag & 4) {
		if ((u32)sm >= 32 || !((0x3F >> sm) & 1)) {
			sceKernelSignalSema(g_sas_sema, 1);
			pspSdkSetK1(old_k1);
			return 0x80420013;
		}
		cm = (cm & ~0xFF0000u) | ((sm & 0xFFu) << 16);
	}
	if (flag & 8) {
		if ((u32)rm >= 32 || !((0x2A >> rm) & 1)) {
			sceKernelSignalSema(g_sas_sema, 1);
			pspSdkSetK1(old_k1);
			return 0x80420013;
		}
		cm = (cm & ~0xFF000000u) | ((rm & 0xFFu) << 24);
	}
	v->curve_mode = cm;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x18C4: __sceSasSetSL */
int sceSasCore_5F9529F6(SceSasCore *sasCore, int vnum, int sl)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	if ((u32)sl > 0x40000000u) {
		pspSdkSetK1(old_k1);
		return 0x80420019;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	sasCore->voices[vnum].sl = sl;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x19A8: __sceSasGetEnvelopeHeight */
int sceSasCore_74AE582A(SceSasCore *sasCore, int vnum)
{
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & (pspSdkGetK1() << 11)) < 0) {
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		return 0x80420010;
	}
	return sasCore->voices[vnum].env_height;
}

/* 0x19F4: __sceSasGetAllEnvelopeHeights */
int sceSasCore_07F58C24(SceSasCore *sasCore, int *heights)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0 ||
	    (s32)(((u32)heights | ((u32)heights + 0x80)) & k1_check) < 0) {
		return 0x80000023;
	}
	for (int i = 0; i < 32; i++) {
		heights[i] = sasCore->voices[i].env_height;
	}
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x1A70: __sceSasSetSimpleADSR */
int sceSasCore_CBCD4F79(SceSasCore *sasCore, int vnum, u32 env1, u32 env2)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)vnum >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420010;
	}
	u32 am = (env1 & 0x8000) ? 2 : 0;
	u32 sm_bits = (env2 >> 13) & 7;
	u32 sm;
	if (sm_bits == 0) {
		sm = 0;
	} else if (sm_bits == 2) {
		sm = 1;
	} else if (sm_bits == 4) {
		sm = 2;
	} else if (sm_bits == 6) {
		sm = 3;
	} else {
		pspSdkSetK1(old_k1);
		return 0x80420013;
	}
	u32 rm = (env2 & 0x20) ? 3 : 1;

	if (sceKernelWaitSema(g_sas_sema, 1, NULL) >= 0) {
		u32 cm = sasCore->voices[vnum].curve_mode;
		cm = (cm & ~0xFFu) | am;
		cm = (cm & ~0xFF00u) | (3u << 8);
		cm = (cm & ~0xFF0000u) | (sm << 16);
		cm = (cm & ~0xFF000000u) | (rm << 24);
		sasCore->voices[vnum].curve_mode = cm;
		sceKernelSignalSema(g_sas_sema, 1);
	}

	u32 sl_val = ((env1 & 0x0F) + 1) << 26;
	if (sl_val <= 0x40000000u) {
		if (sceKernelWaitSema(g_sas_sema, 1, NULL) >= 0) {
			sasCore->voices[vnum].sl = sl_val;
			sceKernelSignalSema(g_sas_sema, 1);
		}
	}

	u32 ar_shift = (env1 >> 8) & 0x7F;
	s32 ar;
	if (ar_shift == 0x7F) {
		ar = 0;
	} else {
		u32 v = ((7 - (ar_shift & 3)) << 26) >> (ar_shift >> 2);
		ar = (v == 0) ? 1 : (s32)v;
	}

	u32 dr_shift = (env1 >> 4) & 0x0F;
	s32 dr = (s32)(0x80000000u >> dr_shift);

	u32 sr_shift = (env2 >> 6) & 0x7F;
	s32 sr;
	if (sr_shift == 0x7F) {
		sr = 0;
	} else {
		u32 v;
		if (sm == 3) {
			v = ((7 - (sr_shift & 3)) << 24) >> (sr_shift >> 2);
		} else {
			v = ((7 - (sr_shift & 3)) << 26) >> (sr_shift >> 2);
		}
		sr = (v == 0) ? 1 : (s32)v;
	}

	u32 rr_shift = env2 & 0x1F;
	s32 rr;
	if (rr_shift == 0x1F) {
		rr = 0;
	} else {
		u32 v;
		if (rm == 1) {
			v = (u32)(0x40000000 >> (rr_shift + 2));
		} else {
			v = 0x80000000u >> rr_shift;
		}
		rr = (v == 0) ? 1 : (s32)v;
	}

	if (ar < 0) ar = 0x7FFFFFFF;
	if (dr < 0) dr = 0x7FFFFFFF;
	if (sr < 0) sr = 0x7FFFFFFF;
	if (rr < 0) rr = 0x7FFFFFFF;

	if (sceKernelWaitSema(g_sas_sema, 1, NULL) >= 0) {
		sasCore->voices[vnum].ar = ar;
		sasCore->voices[vnum].dr = dr;
		sasCore->voices[vnum].sr = sr;
		sasCore->voices[vnum].rr = rr;
		sceKernelSignalSema(g_sas_sema, 1);
	}

	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x1E38: __sceSasSetGrain */
int sceSasCore_D1E0A01E(SceSasCore *sasCore, int grain)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		return 0x80000023;
	}
	if (sasCore == NULL) {
		pspSdkSetK1(old_k1);
		return 0x80420005;
	}
	if ((grain & 0x1F) != 0 || grain < 0x40 || grain > 0x800) {
		pspSdkSetK1(old_k1);
		return 0x80420001;
	}
	sasCore->grain_shift = (u8)(grain >> 5);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x1EA4: __sceSasGetGrain */
int sceSasCore_BD11B7C2(SceSasCore *sasCore)
{
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & (pspSdkGetK1() << 11)) < 0) {
		return 0x80000023;
	}
	if (sasCore == NULL) {
		return 0x80420005;
	}
	return (u32)sasCore->grain_shift << 5;
}

/* 0x1EE0: __sceSasSetOutputmode */
int sceSasCore_E855BF76(SceSasCore *sasCore, int outputmode)
{
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & (pspSdkGetK1() << 11)) < 0) {
		return 0x80000023;
	}
	if (sasCore == NULL) {
		return 0x80420005;
	}
	if ((u32)outputmode >= 2) {
		return 0x80420003;
	}
	sasCore->output_mode = (u8)outputmode;
	if (outputmode == 1) {
		sasCore->flags = (*(u16 *)&sasCore->flags) | 0x200;
	}
	return 0;
}

/* 0x1F44: __sceSasGetOutputmode */
int sceSasCore_E175EF66(SceSasCore *sasCore)
{
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & (pspSdkGetK1() << 11)) < 0) {
		return 0x80000023;
	}
	if (sasCore == NULL) {
		return 0x80420005;
	}
	return sasCore->output_mode;
}

/* 0x1F7C: __sceSasInit */
int sceSasCore_42778A9F(SceSasCore *sasCore, int grainSamples, int maxVoices, int outMode, int sampleRate)
{
	if (((u32)sasCore & 0x3F) != 0 || !check_valid_ram((u32)sasCore, 0x00220202)) {
		return 0x80420005;
	}
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	pspSdkSetK1(k1_check);

	if ((grainSamples & 0x1F) != 0 || grainSamples < 0x40 || grainSamples > 0x800) {
		pspSdkSetK1(old_k1);
		return 0x80420001;
	}
	if (sampleRate != 44100) {
		pspSdkSetK1(old_k1);
		return 0x80420004;
	}
	if ((u32)(maxVoices - 1) >= 32) {
		pspSdkSetK1(old_k1);
		return 0x80420002;
	}
	if ((u32)outMode >= 2) {
		pspSdkSetK1(old_k1);
		return 0x80420003;
	}

	u8 *p = (u8 *)sasCore;
	for (int i = 0; i < 0xE20; i++) {
		p[i] = 0;
	}

	u32 me_ptr = (u32)g_me_sas_buf;
	for (int i = 0; i < 32; i++) {
		SceSasVoice *v = &sasCore->voices[i];
		*(u32 *)&v->vol_unk = me_ptr;
		me_ptr += 0x4C;
		v->env_state = -1;
		v->curve_mode = 0x01010100;
		v->unk30 = 7;
		v->unk31 = 7;
		v->pitch = 0x1000;
		v->vol_l = 0x1000;
		v->vol_r = 0x1000;
		v->vol_wl = 0x1000;
		v->vol_wr = 0x1000;
	}

	u32 *src = (u32 *)sasCore->voices;
	u32 *dst = (u32 *)sasCore->voices_bak;
	u32 *end = dst;
	while (src != end) {
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		src += 4;
		dst += 4;
	}

	sasCore->grain_shift = (u8)(grainSamples / 32);
	sasCore->me_buf = me_ptr;
	sasCore->end_flags_bak = 0xFFFFFFFF;
	sasCore->output_mode = (u8)outMode;
	sasCore->end_flags = 0xFFFFFFFF;
	sceKernelGetCompiledSdkVersion();

	if (sceKernelWaitSema(g_sas_sema, 1, NULL) >= 0) {
		sasCore->rev_vol_l = 0;
		sasCore->rev_vol_r = 0;
		*((u8 *)&sasCore->flags + 1) |= 8;
		sceKernelSignalSema(g_sas_sema, 1);
	}
	if (sceKernelWaitSema(g_sas_sema, 1, NULL) >= 0) {
		sasCore->wet_dry = 1;
		*((u8 *)&sasCore->flags + 1) |= 8;
		sceKernelSignalSema(g_sas_sema, 1);
	}
	if (sceKernelWaitSema(g_sas_sema, 1, NULL) >= 0) {
		sasCore->flags |= 0x1FF;
		sceKernelSignalSema(g_sas_sema, 1);
	}
	if (sceKernelWaitSema(g_sas_sema, 1, NULL) >= 0) {
		sasCore->flags = (*(u16 *)&sasCore->flags) | 0x200;
		sceKernelSignalSema(g_sas_sema, 1);
	}
	sceKernelDcacheWritebackInvalidateAll();
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x2220: __sceSasRevVON */
int sceSasCore_F983B186(SceSasCore *sasCore, int dry, int wet)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res >= 0) {
		sasCore->wet_dry = (u8)(((wet != 0) ? 2 : 0) + ((dry != 0) ? 1 : 0));
		*((u8 *)&sasCore->flags + 1) |= 8;
		sceKernelSignalSema(g_sas_sema, 1);
	}
	pspSdkSetK1(old_k1);
	return res;
}

/* 0x22EC: __sceSasRevEVOL */
int sceSasCore_D5A229C9(SceSasCore *sasCore, int lv, int rv)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if (sceKernelGetCompiledSdkVersion() > 0x0300FFFF) {
		if ((u32)lv > 0x1000 || (u32)rv > 0x1000) {
			pspSdkSetK1(old_k1);
			return 0x80420023;
		}
	} else {
		if (((u32)lv | (u32)rv) > 0x1000) {
			pspSdkSetK1(old_k1);
			return 0x80420023;
		}
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res >= 0) {
		sasCore->rev_vol_l = (u16)lv;
		sasCore->rev_vol_r = (u16)rv;
		*((u8 *)&sasCore->flags + 1) |= 8;
		sceKernelSignalSema(g_sas_sema, 1);
	}
	pspSdkSetK1(old_k1);
	return res;
}

/* 0x23F8: __sceSasRevType */
int sceSasCore_33D4AB37(SceSasCore *sasCore, int type)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)(type + 1) >= 10) {
		pspSdkSetK1(old_k1);
		return 0x80420020;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	sasCore->flags = ((sasCore->flags & ~0xFFu) + (type & 0xFF)) | 0x100;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x24C4: __sceSasRevParam */
int sceSasCore_267A6DD2(SceSasCore *sasCore, int delay, int feedback)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if ((u32)delay >= 0x80) {
		pspSdkSetK1(old_k1);
		return 0x80420022;
	}
	if ((u32)feedback >= 0x80) {
		pspSdkSetK1(old_k1);
		return 0x80420021;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	sasCore->flags = ((*(u16 *)&sasCore->flags) + ((u32)feedback << 24) + ((u32)delay << 16)) | 0x200;
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x25AC: __sceSasSetPause */
int sceSasCore_787D04D5(SceSasCore *sasCore, u32 voice_bit, int setFlag)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	for (int i = 0; voice_bit != 0; i++, voice_bit >>= 1) {
		if (voice_bit & 1) {
			u8 t = sasCore->voices[i].type;
			if (setFlag != 0) {
				if (sasCore->busy != 0 && (t & 0x30) == 0) {
					t |= 0x20;
				} else {
					t |= 0x10;
				}
			} else {
				t &= ~0x30;
			}
			sasCore->voices[i].type = t;
		}
	}
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return 0;
}

/* 0x26A8: __sceSasGetPauseFlag */
int sceSasCore_2C8E6AB3(SceSasCore *sasCore)
{
	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	pspSdkSetK1(k1_check);
	if ((s32)(((u32)sasCore | ((u32)sasCore + 0xE20)) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	int res = sceKernelWaitSema(g_sas_sema, 1, NULL);
	if (res < 0) {
		pspSdkSetK1(old_k1);
		return res;
	}
	u32 mask = 0;
	for (int i = 31; i >= 0; i--) {
		mask <<= 1;
		if (sasCore->voices[i].type & 0x30) {
			mask |= 1;
		}
	}
	sceKernelSignalSema(g_sas_sema, 1);
	pspSdkSetK1(old_k1);
	return mask;
}

/* 0x2768: module_start / sceSasCoreInit */
int module_start(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	void *buf = sceMeCalloc(0xB634, 1);
	g_me_sas_buf = buf;
	if (buf == NULL) {
		return 1;
	}
	SceUID sema = sceKernelCreateSema("SceSas", 0, 1, 1, NULL);
	g_sas_sema = sema;
	return (sema < 0) ? 1 : 0;
}

/* 0x27CC: module_stop / sceSasCoreExit */
int module_stop(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	if (g_me_sas_buf != NULL) {
		sceMeFree(g_me_sas_buf);
	}
	sceKernelDeleteSema(g_sas_sema);
	return 0;
}

/* 0x280C: sceP3daBridgeCore */
int sceP3da_013016F3(u32 *work, u32 numChannels, u32 numSamples, u32 *inBufs, void *outBuf)
{
	if (!check_valid_ram((u32)work, 0x00220202) ||
	    !check_valid_ram((u32)inBufs, 0x00220202) ||
	    !check_valid_ram((u32)outBuf, 0x00220202) ||
	    !check_valid_ram(work[0], 0x00220202) ||
	    !check_valid_ram(work[1], 0x00220202)) {
		return 0x80470003;
	}
	if (numChannels != 0) {
		for (int i = (int)numChannels - 1; i >= 0; i--) {
			if (!check_valid_ram(inBufs[i], 0x00220202)) {
				return 0x80470003;
			}
		}
	}

	u32 old_k1 = pspSdkGetK1();
	u32 k1_check = old_k1 << 11;
	if ((s32)((((u32)work + 8) | (u32)work) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	pspSdkSetK1(k1_check);

	sceKernelDcacheWritebackInvalidateRange(work, 8);
	sceKernelDcacheWritebackInvalidateRange((void *)work[0], 8);
	sceKernelDcacheWritebackInvalidateRange((void *)work[1], numChannels * 40);

	u32 in_arr_size = numChannels << 2;
	if ((s32)((((u32)inBufs + in_arr_size) | (u32)inBufs | in_arr_size) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	if (numChannels != 0) {
		for (int i = (int)numChannels - 1; i >= 0; i--) {
			u32 ch_buf = inBufs[i];
			u32 ch_size = numSamples << 1;
			if (ch_buf != 0) {
				if ((s32)(((ch_buf + ch_size) | ch_buf | ch_size) & k1_check) < 0) {
					pspSdkSetK1(old_k1);
					return 0x80000023;
				}
				sceKernelDcacheWritebackInvalidateRange((void *)ch_buf, ch_size);
			}
		}
	}

	u32 out_size = numSamples << 2;
	if ((s32)((((u32)outBuf + out_size) | (u32)outBuf | out_size) & k1_check) < 0) {
		pspSdkSetK1(old_k1);
		return 0x80000023;
	}
	sceKernelDcacheInvalidateRangeForUser(outBuf, out_size);

	int ret = sceMeCore_driver_FA398D71(0x109, work, inBufs, 0x40000000 | ((u32)outBuf & 0x1FFFFFFF));
	pspSdkSetK1(old_k1);
	return ret;
}

/* 0x2A38: sceP3da_driver_374500A5 */
int sceP3da_driver_374500A5(int a0, int a1)
{
	u32 old_k1 = pspSdkGetK1();
	pspSdkSetK1(old_k1 << 11);
	int ret = sceMeCore_driver_635397BB(0x108, a0, a1);
	pspSdkSetK1(old_k1);
	return ret;
}

/* 0x2A70: sceP3da_driver_43F756A2 */
int sceP3da_driver_43F756A2(void)
{
	u32 old_k1 = pspSdkGetK1();
	pspSdkSetK1(old_k1 << 11);
	int ret = sceMeCore_driver_635397BB(0x10A);
	pspSdkSetK1(old_k1);
	return ret;
}
