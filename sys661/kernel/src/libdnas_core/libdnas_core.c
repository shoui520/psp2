#include <stdint.h>
#include <stddef.h>

extern int sceKernelUnloadModule(int modid);
extern int sceKernelStartModule(int modid, int argsize, void *argp, int *status, void *option);
extern int sceKernelLoadModuleForKernel(const char *path, int flags, void *option);
extern int sceKernelSearchModuleByName(const char *name);
extern int sceKernelStopModule(int modid, int argsize, void *argp, int *status, void *option);

extern int sceKernelSetDNAS(int flag);
extern int sceKernelGetDNAS(void);
extern void *sceKernelGetGameInfo(void);

extern char *strstr(const char *haystack, const char *needle);
extern void *memset(void *s, int c, size_t n);
extern int look_ctype_table(int c);
extern long strtol(const char *nptr, char **endptr, int base);
extern size_t strlen(const char *s);
extern int sprintf(char *str, const char *format, ...);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern int memcmp(const void *s1, const void *s2, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern char *strchr(const char *s, int c);
extern char *strncpy(char *dest, const char *src, size_t n);

extern int sceChkregGetPsCode(uint8_t *pscode);

extern int sceMcctrl_3EF531DB(void *ctx, void *arg1, void *arg2, void *arg3, void *arg4);
extern int sceMcctrl_877CD3A5(void);
extern int sceMcctrl_1EDFD6BB(void *ctx, void *out);
extern int sceMcctrl_7CAC25B2(void *ctx, void *in, void *a2, void *a3, void *a4, void *a5);
extern int sceMcctrl_9618EE57(void *ctx, void *in, int len);
extern int sceMcctrl_61550814(void *ctx, void *in, int len, void *out);

extern int sceRtc_driver_66054C2A(void *tick);
extern int sceRtcGetTick(void *date, void *tick);

static inline uint32_t pspGetK1(void)
{
	uint32_t k1;
	__asm__ volatile("move %0, $k1" : "=r"(k1));
	return k1;
}

static inline void pspSetK1(uint32_t k1)
{
	__asm__ volatile("move $k1, %0" : : "r"(k1));
}

typedef struct DnasRespHeader {
	int content_length;      /* 0x00 */
	char cookie[0x400];      /* 0x04 */
	char location[0x80];     /* 0x404 */
} DnasRespHeader;                /* size 0x484 */

typedef struct DnasCtx {
	int mode;                /* 0x000 (0x2ff8) */
	char host[0x80];         /* 0x004 (0x2ffc) */
	char path[0x80];         /* 0x084 (0x307c) */
	uint8_t session_id[0x30];/* 0x104 (0x30fc) */
	DnasRespHeader resp;     /* 0x134 (0x312c..0x35b0) */
	uint8_t token[0x130];    /* 0x5b8 (0x35b0..0x36e0) */
	uint8_t mc_ctx[0x120];   /* 0x6e8 (0x36e0..0x3800) */
	int mc_res_len;          /* 0x808 (0x3800) */
	uint8_t key1[0x14];      /* 0x80c (0x3804) */
	uint32_t flag_be;        /* 0x820 (0x3818) */
	uint8_t pad_824[0x08];   /* 0x824 */
	uint8_t key2[0x18];      /* 0x82c (0x3824) */
	uint8_t game_id[0x10];   /* 0x844 (0x383c) */
	uint8_t game_key[0x10];  /* 0x854 (0x384c) */
	uint8_t pad_864[0x08];   /* 0x864 */
	uint8_t key3[0x08];      /* 0x86c (0x3864) */
	uint8_t game_param[0x08];/* 0x874 (0x386c) */
	int chkreg_modid;        /* 0x87c (0x3874) */
} DnasCtx;                       /* size 0x880 */

static int g_redirected;         /* 0x2fe0 */
static uint32_t g_proxy_len;     /* 0x2fe4 */
static uint32_t g_proxy_off;     /* 0x2fe8 */
static uint32_t g_buf_len;       /* 0x2fec */
static uint32_t g_buf_off;       /* 0x2ff0 */
static int g_state;              /* 0x2ff4 */
static DnasCtx g_ctx;            /* 0x2ff8..0x3878 */
static char g_proxy_buf[0x80];   /* 0x3878..0x38f8 */
static uint8_t g_io_buf[0x1000]; /* 0x38f8..0x48f8 */
static uint8_t g_rtc_buf[0x1d0]; /* 0x48f8..0x50c8 */

static int sub_174c(DnasCtx *ctx, int content_len, char *out_buf, uint32_t max_len);
static int sub_1a04(DnasRespHeader *resp, char *in_buf, uint32_t in_len);
static int sub_1e00(int expected_state, DnasCtx *ctx, const uint8_t *pkt, uint32_t *out_val);
static void sub_20f0(uint8_t *pkt_out, uint32_t hdr_flags, const uint8_t *sess_id, uint32_t payload_len);
static int sub_22f0(int state, DnasCtx *ctx, char *buf, uint32_t len);
static int sub_24bc(DnasCtx *ctx, char *buf, uint32_t max_len);
static int sub_256c(DnasCtx *ctx, const char *buf, uint32_t len);
static int sub_25cc(DnasCtx *ctx, char *buf, uint32_t max_len);
static int sub_2768(int state, DnasCtx *ctx, char *buf, uint32_t len);
static int sub_28e8(DnasCtx *ctx, char *buf, uint32_t max_len);

int sceDNASCore_lib_BF6A7475(uint16_t *out_code)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	if (out_code == NULL || (int32_t)((k1 << 11) & (uint32_t)out_code) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	int res = sceKernelSearchModuleByName("sceChkreg");
	int loaded_modid = -1;
	int status = 0;

	if (res == (int)0x8002012e) {
		res = sceKernelLoadModuleForKernel("flash0:/kd/chkreg.prx", 0, NULL);
		if (res < 0) {
			pspSetK1(k1);
			return res;
		}
		int start_res = sceKernelStartModule(res, 0, NULL, &status, NULL);
		loaded_modid = res;
		if (start_res < 0) {
			sceKernelStopModule(loaded_modid, 0, NULL, NULL, NULL);
			res = sceKernelUnloadModule(loaded_modid);
			pspSetK1(k1);
			return res;
		}
	} else if (res < 0) {
		pspSetK1(k1);
		return res;
	}

	uint8_t pscode[8];
	res = sceChkregGetPsCode(pscode);
	if (res >= 0) {
		*out_code = *(uint16_t *)&pscode[2];
	}

	if (loaded_modid >= 0) {
		sceKernelStopModule(loaded_modid, 0, NULL, NULL, NULL);
		res = sceKernelUnloadModule(loaded_modid);
	}

	pspSetK1(k1);
	return res;
}

int sceDNASCore_lib_BA0D27F8(void)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int res = sub_24bc(&g_ctx, g_proxy_buf, 0x80);
	if (res >= 0) {
		g_proxy_len = res;
		g_proxy_off = 0;
	} else {
		g_state = 1;
	}

	pspSetK1(k1);
	return res;
}

int sceDNASCore_lib_2370130E(void)
{
	uint32_t k1 = pspGetK1();
	uint32_t len = g_proxy_len;
	int res = 0x80530411;

	pspSetK1(k1 << 11);
	if (len < 0x80) {
		res = sub_256c(&g_ctx, g_proxy_buf, len);
		if (res >= 0) {
			pspSetK1(k1);
			return res;
		}
	}

	g_state = 1;
	pspSetK1(k1);
	return res;
}

int sceDNASCore_lib_4108128B(void)
{
	uint32_t k1 = pspGetK1();
	int res = 0x80530402;
	pspSetK1(k1 << 11);

	if (g_state == 2) {
		res = sub_25cc(&g_ctx, (char *)g_io_buf, 0x1000);
		if (res >= 0) {
			g_state = 3;
			g_buf_off = 0;
			g_buf_len = res;
		} else {
			g_state = 1;
		}
	}

	pspSetK1(k1);
	return res;
}

int sceDNASCore_lib_80CEC43A(void)
{
	uint32_t k1 = pspGetK1();
	int res = 0x80530402;
	pspSetK1(k1 << 11);

	if (g_state == 8) {
		res = sub_28e8(&g_ctx, (char *)g_io_buf, 0x1000);
		if (res >= 0) {
			g_state = 9;
			g_buf_off = 0;
			g_buf_len = res;
		} else {
			g_state = 1;
		}
	}

	pspSetK1(k1);
	return res;
}

int sceDNASCore_lib_BA0A32CA(void)
{
	uint32_t k1 = pspGetK1();
	int res = 0x80530402;
	pspSetK1(k1 << 11);

	if (g_state == 13) {
		res = sub_22f0(13, &g_ctx, (char *)g_io_buf, g_buf_len);
		if (res >= 0) {
			sceKernelSetDNAS(1);
		} else {
			g_state = 1;
		}
	}

	pspSetK1(k1);
	return res;
}

int sceDNASCore_lib_D5E80301(void)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	g_state = 0;
	g_buf_off = 0;
	g_buf_len = 0;
	g_proxy_off = 0;
	g_redirected = 0;
	g_proxy_len = 0;
	memset(&g_ctx, 0, sizeof(g_ctx));
	sceMcctrl_877CD3A5();

	if (g_ctx.chkreg_modid > 0) {
		sceKernelStopModule(g_ctx.chkreg_modid, 0, NULL, NULL, NULL);
		sceKernelUnloadModule(g_ctx.chkreg_modid);
		g_ctx.chkreg_modid = -1;
	}

	pspSetK1(k1);
	return 0;
}

int sceDNASCore_lib_2B6C67EA(void)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);
	int dnas = sceKernelGetDNAS();
	pspSetK1(k1);
	return (dnas == 1) ? 1 : 0;
}

int sceDNASCore_lib_FA571A75(int mode, int region_mode)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	if (g_state != 0 && g_state != 2) {
		pspSetK1(k1);
		return 0x80530402;
	}

	memset(g_io_buf, 0, sizeof(g_io_buf));
	memset(g_proxy_buf, 0, sizeof(g_proxy_buf));
	memset(&g_ctx, 0, sizeof(g_ctx));

	g_buf_off = 0;
	g_buf_len = 0;
	g_proxy_off = 0;
	g_proxy_len = 0;
	g_redirected = 0;
	g_ctx.mode = mode;

	int res = sceKernelSearchModuleByName("sceChkreg");
	int status = 0;
	if (res == (int)0x8002012e) {
		res = sceKernelLoadModuleForKernel("flash0:/kd/chkreg.prx", 0, NULL);
		if (res >= 0) {
			g_ctx.chkreg_modid = res;
			res = sceKernelStartModule(res, 0, NULL, &status, NULL);
		}
	} else if (res >= 0) {
		g_ctx.chkreg_modid = -1;
	}

	if (res >= 0) {
		uint8_t pscode[8];
		res = sceChkregGetPsCode(pscode);
		if (res >= 0) {
			if (g_ctx.mode == 0) {
				sprintf(g_ctx.host, "%04x.%s", pscode[2], "pspgate1.dnas.playstation.org");
				sprintf(g_ctx.path, "%s%04x", "/dnas-scei/psp-connect_", pscode[2]);
			} else if (g_ctx.mode == 1) {
				memcpy(g_ctx.host, "pspdnas-ld02.rd.scei.sony.co.jp", 32);
				memcpy(g_ctx.path, "/dnas-scei/psp-connect_debug", 29);
			}
		}
	}

	if (g_ctx.chkreg_modid > 0) {
		sceKernelStopModule(g_ctx.chkreg_modid, 0, NULL, NULL, NULL);
		res = sceKernelUnloadModule(g_ctx.chkreg_modid);
		g_ctx.chkreg_modid = -1;
	}

	if (res < 0) {
		pspSetK1(k1);
		return res;
	}

	uint8_t *game_info = (uint8_t *)sceKernelGetGameInfo();
	uint32_t flag = 0;
	if (region_mode == 1) {
		flag = __builtin_bswap32(0x80000001);
	} else if (region_mode == 0) {
		flag = __builtin_bswap32(0);
	} else if (region_mode == 2) {
		flag = __builtin_bswap32(0x80000002);
	} else if (region_mode == 3) {
		flag = __builtin_bswap32(0x80000003);
	}

	g_ctx.flag_be = flag;
	memcpy(g_ctx.game_id, game_info + 0x08, 16);
	memcpy(g_ctx.game_key, game_info + 0x44, 16);
	memcpy(g_ctx.game_param, game_info + 0x28, 8);

	res = sceMcctrl_3EF531DB(&g_ctx.mc_ctx, &g_ctx.key1, &g_ctx.key2, &g_ctx.key3, &g_ctx.game_param);
	if (res < 0) {
		pspSetK1(k1);
		return res;
	}

	int dnas = sceKernelGetDNAS();
	g_state = 2;
	pspSetK1(k1);
	return (dnas == 1) ? 1 : 0;
}

int sceDNASCore_lib_F479F616(char *out_host)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	if ((int32_t)(((uint32_t)out_host | ((uint32_t)out_host + 0x80)) & (k1 << 11)) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	memset(out_host, 0, 0x80);
	memcpy(out_host, "pspgate1.dnas.playstation.org", 29);
	pspSetK1(k1);
	return 0;
}

int sceDNASCore_lib_B6C76A14(char *out_host)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	if (g_state != 7) {
		pspSetK1(k1);
		return 0x80530402;
	}

	if ((int32_t)(((uint32_t)out_host | ((uint32_t)out_host + 0x80)) & (k1 << 11)) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	memset(out_host, 0, 0x80);
	int res = sub_2768(g_state, &g_ctx, (char *)g_io_buf, g_buf_len);
	if (res != (int)0x80530421) {
		if (res < 0) {
			g_state = 1;
		} else {
			g_state = 8;
		}
		pspSetK1(k1);
		return res;
	}

	if (g_redirected != 0) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530423;
	}

	char *https = strstr(g_ctx.resp.location, "https://");
	if (https == NULL) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530424;
	}

	char *host_start = https + 8;
	char *slash = strchr(host_start, '/');
	if (slash == NULL) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530424;
	}

	char *path_start = slash + 1;
	*slash = '\0';

	if (strlen(path_start) >= 0x7f || strlen(host_start) >= 0x7f) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530424;
	}

	strncpy(g_ctx.host, host_start, 0x80);
	sprintf(g_ctx.path, "/%s", path_start);
	memset(g_ctx.resp.location, 0, 0x80);

	if (out_host == NULL) {
		pspSetK1(k1);
		return 0x80530403;
	}

	memcpy(out_host, g_ctx.host, 0x80);
	g_redirected = 1;
	g_state = 2;
	pspSetK1(k1);
	return 0;
}

int sceDNASCore_lib_822357BB(void *buf, uint32_t len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int state = g_state;
	if ((uint32_t)(state - 9) >= 2) {
		pspSetK1(k1);
		return 0x80530402;
	}

	if ((int32_t)((((uint32_t)buf + len) | (uint32_t)buf | len) & (k1 << 11)) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	if (buf == NULL || len == 0) {
		pspSetK1(k1);
		return 0x80530403;
	}

	if (state != 3 && state != 9 && state != 4 && state != 10) {
		pspSetK1(k1);
		return 0x80530402;
	}

	uint32_t off = g_buf_off;
	uint32_t total = g_buf_len;
	if (total < off) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530402;
	}

	uint32_t copy_len = len;
	if (off + len >= total) {
		copy_len = total - off;
	}

	if (copy_len == 0) {
		if (state == 4) {
			g_state = 5;
			g_buf_off = 0;
			g_buf_len = 0;
			pspSetK1(k1);
			return 0;
		} else if (state == 10) {
			g_state = 11;
			g_buf_off = 0;
			g_buf_len = 0;
			pspSetK1(k1);
			return 0;
		} else {
			g_state = 1;
			g_buf_off = 0;
			g_buf_len = 0;
			pspSetK1(k1);
			return 0x80530402;
		}
	}

	memcpy(buf, g_io_buf + off, copy_len);
	g_buf_off += copy_len;
	state = g_state;
	if ((uint32_t)state >= 3 && (uint32_t)state < 5) {
		g_state = 4;
	} else if ((uint32_t)(state - 9) < 2) {
		g_state = 10;
	} else {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530402;
	}

	pspSetK1(k1);
	return copy_len;
}

int sceDNASCore_lib_8309549E(const void *buf, uint32_t len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int state = g_state;
	if ((uint32_t)(state - 11) >= 2) {
		pspSetK1(k1);
		return 0x80530402;
	}

	if ((int32_t)((((uint32_t)buf + len) | (uint32_t)buf | len) & (k1 << 11)) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	if ((uint32_t)(state - 5) < 2) {
		g_state = 6;
	} else {
		g_state = 12;
	}

	if (buf == NULL || len == 0) {
		uint32_t off = g_buf_off;
		if (off >= 0xfff) {
			g_state = 1;
			pspSetK1(k1);
			return 0x80530411;
		}
		g_io_buf[off] = 0;
		if (g_state == 6) {
			g_state = 7;
			g_buf_len = off;
			g_buf_off = 0;
			pspSetK1(k1);
			return 0;
		} else if (g_state == 12) {
			g_state = 13;
			g_buf_len = off;
			g_buf_off = 0;
			pspSetK1(k1);
			return 0;
		}
		pspSetK1(k1);
		return 0x80530402;
	}

	uint32_t off = g_buf_off;
	if (off + len >= 0xfff) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530404;
	}

	memcpy(g_io_buf + off, buf, len);
	g_buf_off += len;
	pspSetK1(k1);
	return len;
}

int sceDNASCore_lib_15096ECD(char *out_host)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	if ((int32_t)(((uint32_t)out_host | ((uint32_t)out_host + 0x80)) & (k1 << 11)) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	memset(out_host, 0, 0x80);
	if (((uintptr_t)out_host | (uintptr_t)g_ctx.host) & 3) {
		for (int i = 0; i < 0x80; i++) {
			((volatile char *)out_host)[i] = g_ctx.host[i];
		}
	} else {
		uint32_t *dst = (uint32_t *)out_host;
		const uint32_t *src = (const uint32_t *)g_ctx.host;
		for (int i = 0; i < 32; i += 4) {
			dst[i] = src[i];
			dst[i + 1] = src[i + 1];
			dst[i + 2] = src[i + 2];
			dst[i + 3] = src[i + 3];
		}
	}
	pspSetK1(k1);
	return 0;
}

int sceDNASCore_lib_C54657B7(const void *buf, uint32_t len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	if ((int32_t)((((uint32_t)buf + len) | (uint32_t)buf | len) & (k1 << 11)) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	if (buf == NULL || len == 0) {
		uint32_t off = g_proxy_off;
		if (off >= 0x7f) {
			g_state = 1;
			pspSetK1(k1);
			return 0x80530411;
		}
		g_proxy_len = off;
		g_proxy_buf[off] = '\0';
		g_proxy_off = 0;
		pspSetK1(k1);
		return 0;
	}

	uint32_t off = g_proxy_off;
	if (off + len >= 0x7f) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530101;
	}

	memcpy(g_proxy_buf + off, buf, len);
	g_proxy_off += len;
	pspSetK1(k1);
	return len;
}

int sceDNASCore_lib_26E1E2BD(const void *buf, uint32_t len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int state = g_state;
	if ((uint32_t)(state - 5) >= 2) {
		pspSetK1(k1);
		return 0x80530402;
	}

	if ((int32_t)((((uint32_t)buf + len) | (uint32_t)buf | len) & (k1 << 11)) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	g_state = 6;
	if (buf == NULL || len == 0) {
		uint32_t off = g_buf_off;
		if (off >= 0xfff) {
			g_state = 1;
			pspSetK1(k1);
			return 0x80530411;
		}
		g_state = 7;
		g_buf_len = off;
		g_io_buf[off] = 0;
		g_buf_off = 0;
		pspSetK1(k1);
		return 0;
	}

	uint32_t off = g_buf_off;
	if (off + len >= 0xfff) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530404;
	}

	memcpy(g_io_buf + off, buf, len);
	g_buf_off += len;
	pspSetK1(k1);
	return len;
}

int sceDNASCore_lib_DA5939B4(void *buf, uint32_t len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	if ((int32_t)((((uint32_t)buf + len) | (uint32_t)buf | len) & (k1 << 11)) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	if (buf == NULL || len == 0) {
		pspSetK1(k1);
		return 0x80530403;
	}

	uint32_t off = g_proxy_off;
	uint32_t total = g_proxy_len;
	if (total < off) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530402;
	}

	uint32_t copy_len = len;
	if (off + len >= total) {
		copy_len = total - off;
	}

	if (copy_len == 0) {
		g_proxy_off = 0;
		g_proxy_len = 0;
		pspSetK1(k1);
		return 0;
	}

	memcpy(buf, g_proxy_buf + off, copy_len);
	g_proxy_off += copy_len;
	pspSetK1(k1);
	return copy_len;
}

int sceDNASCore_lib_F0EB4367(void *buf, uint32_t len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int state = g_state;
	if ((uint32_t)(state - 3) >= 2) {
		pspSetK1(k1);
		return 0x80530402;
	}

	if ((int32_t)((((uint32_t)buf + len) | (uint32_t)buf | len) & (k1 << 11)) < 0) {
		pspSetK1(k1);
		return 0x80000023;
	}

	if (buf == NULL || len == 0) {
		pspSetK1(k1);
		return 0x80530403;
	}

	if (state != 3 && state != 9 && state != 4 && state != 10) {
		pspSetK1(k1);
		return 0x80530402;
	}

	uint32_t off = g_buf_off;
	uint32_t total = g_buf_len;
	if (total < off) {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530402;
	}

	uint32_t copy_len = len;
	if (off + len >= total) {
		copy_len = total - off;
	}

	if (copy_len == 0) {
		if (state == 4) {
			g_state = 5;
			g_buf_off = 0;
			g_buf_len = 0;
			pspSetK1(k1);
			return 0;
		} else if (state == 10) {
			g_state = 11;
			g_buf_off = 0;
			g_buf_len = 0;
			pspSetK1(k1);
			return 0;
		} else {
			g_state = 1;
			g_buf_off = 0;
			g_buf_len = 0;
			pspSetK1(k1);
			return 0x80530402;
		}
	}

	memcpy(buf, g_io_buf + off, copy_len);
	g_buf_off += copy_len;
	state = g_state;
	if ((uint32_t)state >= 3 && (uint32_t)state < 5) {
		g_state = 4;
	} else if ((uint32_t)(state - 9) < 2) {
		g_state = 10;
	} else {
		g_state = 1;
		pspSetK1(k1);
		return 0x80530402;
	}

	pspSetK1(k1);
	return copy_len;
}

static int sub_174c(DnasCtx *ctx, int content_len, char *out_buf, uint32_t max_len)
{
	if (strlen(ctx->path) + 0x10 >= max_len) {
		return 0x80530404;
	}

	int n = sprintf(out_buf, "%s %s %s\r\n", "POST", ctx->path, "HTTP/1.0");
	if (n < 0) {
		return 0x80530401;
	}

	uint32_t rem = max_len - n;
	int off = n;
	if (strlen(ctx->host) + 8 >= rem) {
		return 0x80530404;
	}

	n = sprintf(out_buf + off, "%s %s\r\n", "Host:", ctx->host);
	if (n < 0) {
		return 0x80530401;
	}
	rem -= n;
	off += n;
	if (rem < 0x1a) {
		return 0x80530404;
	}

	n = sprintf(out_buf + off, "%s %s\r\n", "Content-Type:", "image/gif");
	if (n < 0) {
		return 0x80530401;
	}
	rem -= n;
	off += n;
	if (rem < 0x17) {
		return 0x80530404;
	}

	n = sprintf(out_buf + off, "%s %d\r\n", "Content-Length:", content_len);
	if (n < 0) {
		return 0x80530401;
	}
	rem -= n;
	off += n;
	if (rem < 0x2d) {
		return 0x80530404;
	}

	int ua_len = 0;
	if (ctx->mode == 0) {
		ua_len = sprintf(out_buf + off, "%s %s\r\n", "User-Agent:", "open sesame asdfjkl");
		if (ua_len < 0) {
			return 0x80530401;
		}
	} else if (ctx->mode == 1) {
		ua_len = sprintf(out_buf + off, "%s %s\r\n", "User-Agent:", "abra cadabra jugemjugem");
		if (ua_len < 0) {
			return 0x80530401;
		}
	}

	off += ua_len;
	rem -= ua_len;

	if (ctx->resp.cookie[0] != '\0') {
		if (strlen(ctx->resp.cookie) + 10 >= rem) {
			return 0x80530404;
		}
		n = sprintf(out_buf + off, "%s %s\r\n", "Cookie:", ctx->resp.cookie);
		if (n < 0) {
			return 0x80530401;
		}
		rem -= n;
		off += n;
	}

	ctx->resp.cookie[0] = '\0';
	if (rem < 3) {
		return 0x80530404;
	}

	out_buf[off] = '\r';
	out_buf[off + 1] = '\n';
	out_buf[off + 2] = '\0';
	return off + 2;
}

static int sub_1a04(DnasRespHeader *resp, char *in_buf, uint32_t in_len)
{
	if (in_len >= 0x1000) {
		return 0x80530404;
	}
	if (in_len == 0) {
		return 0x80530449;
	}

	memset(resp, 0, sizeof(*resp));
	char *crlf = strstr(in_buf, "\r\n");
	if (crlf == NULL) {
		return 0x80530426;
	}
	*crlf = '\0';

	int line_len = strlen(in_buf);
	if ((uint32_t)(line_len + 2) >= 0x401) {
		return 0x8053042f;
	}
	if (line_len < 9) {
		return 0x80530426;
	}
	if (strncmp(in_buf, "HTTP/", 5) != 0) {
		return 0x80530427;
	}
	if ((look_ctype_table(in_buf[5]) & 4) == 0 || in_buf[6] != '.' ||
	    (look_ctype_table(in_buf[7]) & 4) == 0 || in_buf[8] != ' ') {
		return 0x80530427;
	}
	if (in_buf[5] != '1' || (uint8_t)(in_buf[7] - '0') >= 2) {
		return 0x80530427;
	}
	if (line_len - 9 < 3) {
		return 0x80530426;
	}

	int is_302 = 0;
	if (strncmp(in_buf + 9, "200", 3) != 0) {
		if (strncmp(in_buf + 9, "302", 3) != 0) {
			return 0x80530422;
		}
		is_302 = 1;
	}

	int rem = (int)in_len - (int)strlen(in_buf) - 2;
	if (rem < 0) {
		return 0x80530426;
	}

	char *cur = crlf + 2;
	while (1) {
		char *next_crlf = strstr(cur, "\r\n");
		if (next_crlf == NULL) {
			return 0x80530404;
		}
		*next_crlf = '\0';
		int hlen = strlen(cur);
		rem = rem - hlen - 2;
		if (rem < 0) {
			return 0x80530404;
		}
		if (hlen == 0) {
			break;
		}
		if (hlen >= 0x401) {
			return 0x8053042f;
		}
		char *colon = strchr(cur, ':');
		if (colon == NULL) {
			return 0x80530429;
		}
		*colon = '\0';
		char *val = colon + 1;
		while (*val == ' ' || *val == '\t') {
			val++;
		}
		int klen = strlen(cur);
		if (strncmp(cur, "Content-Type:", klen) == 0) {
			if (strncmp(val, "image/gif", strlen(val)) != 0) {
				return 0x8053042a;
			}
		} else if (strncmp(cur, "Content-Length:", klen) == 0) {
			resp->content_length = strtol(val, NULL, 10);
		} else if (strncmp(cur, "Set-Cookie:", klen) == 0) {
			strncpy(resp->cookie, val, 0x400);
		} else if (is_302 && strncmp(cur, "Location:", klen) == 0) {
			strncpy(resp->location, val, 0x80);
		}
		cur = next_crlf + 2;
	}

	if (is_302) {
		if (resp->location[0] == '\0') {
			return 0x80530425;
		}
		return 0x80530421;
	}

	if (resp->content_length <= 0) {
		return 0x8053042e;
	}
	if (resp->content_length >= 0x800) {
		return 0x8053042d;
	}
	return (int)in_len - rem;
}

static int sub_1e00(int expected_state, DnasCtx *ctx, const uint8_t *pkt, uint32_t *out_val)
{
	uint32_t magic;
	memcpy(&magic, pkt, 4);
	uint8_t b4 = pkt[4];
	uint8_t b5 = pkt[5];
	uint16_t b6_7 = (uint16_t)pkt[6] | ((uint16_t)pkt[7] << 8);
	uint8_t pkt_type = pkt[11];
	uint8_t sess_id[0x20];
	memcpy(sess_id, pkt + 0x0c, 0x20);
	uint32_t pkt_len;
	memcpy(&pkt_len, pkt + 0x2c, 4);

	if (out_val != NULL) {
		*out_val = pkt_len;
	}

	if (magic != 0xeddefffaU) {
		return 0x80530441;
	}
	if (b5 != 0) {
		return 0x80530442;
	}
	if (b6_7 != 0) {
		return 0x80530443;
	}

	(void)b4;
	if (pkt_type == 5) {
		if (expected_state != 7) {
			return 0x80530444;
		}
		if (pkt_len != 0x120) {
			return 0x80530447;
		}
		memcpy(ctx->session_id, sess_id, 0x20);
		return (int)pkt_len;
	} else if (pkt_type == 6) {
		if (expected_state != 13) {
			return 0x80530444;
		}
		if (pkt_len >= 0x7d1) {
			return 0x80530447;
		}
		if (memcmp(ctx->session_id, sess_id, 0x20) != 0) {
			return 0x80530445;
		}
		return (int)pkt_len;
	} else if (pkt_type == 4) {
		if (expected_state != 7 && expected_state != 13) {
			return 0x80530402;
		}
		if (pkt_len != 4) {
			return 0x80530447;
		}
		int err_code;
		memcpy(&err_code, pkt + 0x30, 4);
		return err_code;
	}

	return 0x80530444;
}

static void sub_20f0(uint8_t *pkt_out, uint32_t hdr_flags, const uint8_t *sess_id, uint32_t payload_len)
{
	uint32_t magic = 0xeddefffaU;
	memcpy(pkt_out, &magic, 4);
	pkt_out[4] = 0;
	pkt_out[5] = 0;
	pkt_out[6] = 0;
	pkt_out[7] = 0;
	memcpy(pkt_out + 8, &hdr_flags, 4);
	memcpy(pkt_out + 0x0c, sess_id, 0x20);
	memcpy(pkt_out + 0x2c, &payload_len, 4);
}

static int sub_22f0(int state, DnasCtx *ctx, char *buf, uint32_t len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int hdr_len = sub_1a04(&ctx->resp, buf, len);
	if (hdr_len < 0) {
		pspSetK1(k1);
		return hdr_len;
	}

	int body_len = (int)len - hdr_len;
	uint8_t *body = (uint8_t *)buf + hdr_len;
	if (ctx->resp.content_length < body_len || body_len >= 0x801) {
		pspSetK1(k1);
		return 0x8053042b;
	}
	if (body_len < ctx->resp.content_length || body_len < 0x30) {
		pspSetK1(k1);
		return 0x8053042c;
	}

	int pkt_len = sub_1e00(state, ctx, body, NULL);
	if (pkt_len < 0) {
		pspSetK1(k1);
		return pkt_len;
	}

	int mc_res = sceMcctrl_9618EE57(ctx->mc_ctx, body + 0x30, pkt_len);
	if (mc_res < 0) {
		pspSetK1(k1);
		return mc_res;
	}
	if (mc_res >= 0x7d0) {
		pspSetK1(k1);
		return 0x80530447;
	}

	ctx->mc_res_len = mc_res;
	int res = sceMcctrl_61550814(ctx->mc_ctx, body + 0x30, mc_res, g_rtc_buf);
	if (res < 0) {
		pspSetK1(k1);
		return res;
	}

	uint8_t rtc_date[16];
	uint64_t tick;
	memcpy(rtc_date, g_rtc_buf, 16);
	res = sceRtcGetTick(rtc_date, &tick);
	if (res < 0) {
		pspSetK1(k1);
		return res;
	}

	res = sceRtc_driver_66054C2A(&tick);
	pspSetK1(k1);
	return res;
}

static int sub_24bc(DnasCtx *ctx, char *buf, uint32_t max_len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	if (strlen(ctx->host) + 0x19 >= max_len) {
		pspSetK1(k1);
		return 0x80530404;
	}

	int res = sprintf(buf, "%s %s:%s %s\r\n\r\n", "CONNECT", ctx->host, "443", "HTTP/1.0");
	if (res < 0) {
		res = 0x80530401;
	}

	pspSetK1(k1);
	return res;
}

static int sub_256c(DnasCtx *ctx, const char *buf, uint32_t len)
{
	(void)ctx;
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	if (len < 0x0c) {
		pspSetK1(k1);
		return 0x80530415;
	}

	int cmp = memcmp(buf + 9, "200", 3);
	pspSetK1(k1);
	return (cmp == 0) ? 0 : (int)0x80530412;
}

static int sub_25cc(DnasCtx *ctx, char *buf, uint32_t max_len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	memset(&ctx->resp, 0, sizeof(ctx->resp));
	int hdr_len = sub_174c(ctx, 0xe0, buf, max_len);
	if (hdr_len < 0) {
		pspSetK1(k1);
		return hdr_len;
	}

	if (max_len - hdr_len < 0xe0) {
		pspSetK1(k1);
		return 0x80530404;
	}

	uint8_t zero_sess[0x20];
	memset(zero_sess, 0, sizeof(zero_sess));
	sub_20f0((uint8_t *)buf + hdr_len, 0, zero_sess, 0xb0);

	uint8_t mc_out[0xb0];
	int res = sceMcctrl_1EDFD6BB(ctx->mc_ctx, mc_out);
	if (res < 0) {
		pspSetK1(k1);
		return res;
	}

	memcpy(buf + hdr_len + 0x30, mc_out, 0xb0);
	pspSetK1(k1);
	return hdr_len + 0xe0;
}

static int sub_2768(int state, DnasCtx *ctx, char *buf, uint32_t len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int hdr_len = sub_1a04(&ctx->resp, buf, len);
	if (hdr_len < 0) {
		pspSetK1(k1);
		return hdr_len;
	}

	int body_len = (int)len - hdr_len;
	uint8_t *body = (uint8_t *)buf + hdr_len;
	if (ctx->resp.content_length < body_len) {
		pspSetK1(k1);
		return 0x8053042b;
	}
	if (body_len < ctx->resp.content_length) {
		pspSetK1(k1);
		return 0x8053042c;
	}
	if (body_len != 0x150 && body_len != 0x34) {
		pspSetK1(k1);
		return 0x8053042d;
	}

	int res = sub_1e00(state, ctx, body, NULL);
	if (res < 0) {
		pspSetK1(k1);
		return res;
	}

	uint8_t payload[0x120];
	memcpy(payload, body + 0x30, 0x120);
	res = sceMcctrl_7CAC25B2(ctx->mc_ctx, payload, ctx->key1, ctx->key2, ctx->key3, ctx->token);
	pspSetK1(k1);
	return res;
}

static int sub_28e8(DnasCtx *ctx, char *buf, uint32_t max_len)
{
	uint32_t k1 = pspGetK1();
	pspSetK1(k1 << 11);

	int hdr_len = sub_174c(ctx, 0x160, buf, max_len);
	if (hdr_len < 0) {
		pspSetK1(k1);
		return hdr_len;
	}

	if (max_len - hdr_len < 0x160) {
		pspSetK1(k1);
		return 0x80530404;
	}

	uint8_t sess_copy[0x20];
	memcpy(sess_copy, ctx->session_id, 0x20);
	sub_20f0((uint8_t *)buf + hdr_len, 0x01000000U, sess_copy, 0x130);
	memcpy(buf + hdr_len + 0x30, ctx->token, 0x130);

	pspSetK1(k1);
	return hdr_len + 0x160;
}
