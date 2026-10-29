#include <stdint.h>
#include <stddef.h>

extern void *sceKernelMemset(void *s, int c, size_t n);
extern int sceKernelLockLwMutex(void *workarea, int lockCount, uint32_t *pTimeout);
extern int sceKernelUnlockLwMutex(void *workarea, int unlockCount);
extern void *sceKernelMemcpy(void *dest, const void *src, size_t n);
extern int SysMemUserForUser_945E45DA(char *env);
extern int sceKernelDelayThread(uint32_t delay);
extern int sceKernelDeleteLwMutex(void *workarea);
extern int sceKernelCreateLwMutex(void *workarea, const char *name, uint32_t attr, int initCount, uint32_t *opt);

extern int sceHttpGetContentLength(int req, uint64_t *len);
extern int sceHttpDeleteHeader(int id, const char *name);
extern int sceHttpSetRecvTimeOut(int id, uint32_t timeout);
extern int sceHttpGetStatusCode(int req, int *code);
extern int sceHttpDeleteConnection(int conn);
extern int sceHttpSetConnectTimeOut(int id, uint32_t timeout);
extern int sceHttpSetSendTimeOut(int id, uint32_t timeout);
extern int sceHttpCreateTemplate(const char *agent, int ver, int autoProxy);
extern int sceHttpDeleteRequest(int req);
extern int sceHttpCreateRequestWithURL(int conn, int method, const char *url, uint64_t contentLength);
extern int sceHttpSendRequest(int req, const void *data, uint32_t size);
extern int sceHttpAbortRequest(int req);
extern int sceHttpCreateConnectionWithURL(int tmpl, const char *url, int enableKeepalive);
extern int sceHttpReadData(int req, void *data, uint32_t size);
extern int sceHttpDeleteTemplate(int tmpl);
extern int sceHttp_3C478044(int tmpl, int val);
extern int sceHttp_87F1E666(int tmpl, int val);

extern int sceHttpStorage_bridge_EFE06A20(int cmd, void *buf, uint32_t size);
extern int sceNetApctlGetInfo(int code, void *pInfo);

extern uint32_t sceNet_lib_2A73ADDC(const char *str, char **endptr, int base);
extern uint64_t sceNet_lib_303A2F80(const char *str, char **endptr, int base);
extern int sceNet_lib_750F705D(int c);
extern int sceNet_lib_9CFBC7E3(const char *s1, const char *s2);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, size_t n);
extern int sceNet_lib_B9085A96(const char *s1, const char *s2, size_t n);
extern size_t sceNet_lib_D8722983(const char *s);

extern int sceUtilityGetSystemParamInt(int id, int *value);

typedef struct PackageNode {
	struct PackageNode *next; /* 0x00 */
	uint16_t ver_major;       /* 0x04 */
	uint16_t ver_minor;       /* 0x06 */
	uint64_t size;            /* 0x08 */
	uint8_t sha1sum[20];      /* 0x10 */
	char url[256];            /* 0x24 */
	char title[128];          /* 0x124 */
	uint16_t psp_sysver_maj;  /* 0x1a4 */
	uint16_t psp_sysver_min;  /* 0x1a6 */
} PackageNode;                    /* size 0x1a8 */

typedef struct XmlTagEntry {
	const char *name;
	uint32_t has_text;
} XmlTagEntry;

typedef struct XmlSchema {
	const XmlTagEntry * const *levels;
	uint32_t max_depth;
} XmlSchema;

typedef int (*XmlCallback)(void *parser, int event, uint32_t depth, uint32_t tag_idx, const char *str1, const char *str2, void *user_arg);

typedef struct XmlParser {
	const XmlSchema *schema;  /* 0x000 */
	char tag_buf[255];        /* 0x004..0x103 */
	char val_buf[511];        /* 0x103..0x302 */
	char utf8_buf[6];         /* 0x302..0x308 */
	uint32_t utf8_have;       /* 0x308 */
	uint32_t utf8_need;       /* 0x30c */
	uint32_t depth;           /* 0x310 */
	uint32_t tag_indices[6];  /* 0x314..0x32c */
	uint32_t state;           /* 0x32c */
	uint32_t substate;        /* 0x330 */
	uint32_t buf_pos;         /* 0x334 */
	uint32_t empty_tag;       /* 0x338 */
	XmlCallback callback;     /* 0x33c */
	void *user_arg;           /* 0x340 */
	uint32_t parse_state;     /* 0x344 */
	uint32_t parse_flags;     /* 0x348 */
	uint32_t pad_34c;         /* 0x34c */
	PackageNode cur_pkg;      /* 0x350..0x4f8 */
} XmlParser;                      /* size 0x4f8 */

typedef struct GameUpdateCtx {
	int state;                /* 0x00 */
	uint32_t mutex[8];        /* 0x04..0x24 */
	void *(*alloc_cb)(uint32_t size); /* 0x24 */
	void (*free_cb)(void *ptr);       /* 0x28 */
	char title_id[24];        /* 0x2c..0x44 */
	int tag_status;           /* 0x44 */
	char tag_name[16];        /* 0x48..0x58 */
	uint32_t pad_58;          /* 0x58 */
	uint16_t cur_ver_maj;     /* 0x3c? Wait: 0x3c..0x44 overlaps title_id! */
} GameUpdateCtxDummy;

typedef struct GameUpdateInitParam {
	void *(*alloc_cb)(uint32_t size); /* 0x00 */
	void (*free_cb)(void *ptr);       /* 0x04 */
	char title_id[13];                /* 0x08..0x15 */
	char app_ver[6];                  /* 0x15..0x1b */
	char sys_ver[9];                  /* 0x1b..0x24 */
} GameUpdateInitParam;

typedef struct GameUpdateResult {
	uint32_t size_struct;     /* 0x00 (must be 0x10) */
	uint32_t result_code;     /* 0x04 */
	char latest_ver[8];       /* 0x08..0x10 */
} GameUpdateResult;

typedef struct GameUpdateDetail {
	char tag_name[20];        /* 0x00..0x14 */
	char title[128];          /* 0x14..0x94 */
	uint32_t pkg_count;       /* 0x94 */
} GameUpdateDetail;

typedef struct GameUpdatePkgInfo {
	uint64_t size;            /* 0x00 */
	uint8_t sha1sum[20];      /* 0x08..0x1c */
	char url[256];            /* 0x1c..0x11c */
} GameUpdatePkgInfo;

typedef struct HttpState {
	int tmpl_id;              /* 0x00 (0x34b0) */
	int conn_id;              /* 0x04 (0x34b4) */
	int req_id;               /* 0x08 (0x34b8) */
	int status_code;          /* 0x0c (0x34bc) */
	uint64_t content_length;  /* 0x10 (0x34c0..0x34c8) */
	uint8_t read_buf[0x800];  /* 0x18 (0x34c8..0x3cc8) */
} HttpState;

static HttpState g_http = {
	-1, -1, -1, 0, 0, { 0 }
};

static char g_lang_title_tag[16] = "title_00"; /* 0x3cc8 */

static const XmlTagEntry g_tags_lvl0[] = {
	{ "titlepatch", 0 },
	{ "", 1 }
};

static const XmlTagEntry g_tags_lvl1[] = {
	{ "tag", 0 },
	{ "", 1 }
};

static const XmlTagEntry g_tags_lvl2[] = {
	{ "package", 0 },
	{ "", 1 }
};

static const XmlTagEntry g_tags_lvl3[] = {
	{ "paramsfo", 0 },
	{ "", 1 }
};

static const XmlTagEntry g_tags_lvl4[] = {
	{ "title", 1 },
	{ g_lang_title_tag, 1 },
	{ "", 1 }
};

static const XmlTagEntry * const g_tag_levels[5] = {
	g_tags_lvl0,
	g_tags_lvl1,
	g_tags_lvl2,
	g_tags_lvl3,
	g_tags_lvl4
};

static const XmlSchema g_xml_schema = {
	g_tag_levels,
	5
};

typedef struct SceGameUpdateContext {
	int state;                        /* 0x00 (0x3ce0) */
	uint32_t mutex[8];                /* 0x04 (0x3ce4..0x3d04) */
	void *(*alloc_cb)(uint32_t size); /* 0x24 (0x3d04) */
	void (*free_cb)(void *ptr);       /* 0x28 (0x3d08) */
	char title_id[16];                /* 0x2c (0x3d0c..0x3d1c) */
	uint16_t cur_ver_maj;             /* 0x3c (0x3d1c) */
	uint16_t cur_ver_min;             /* 0x3e (0x3d1e) */
	uint16_t cur_sysver_maj;          /* 0x40 (0x3d20) */
	uint16_t cur_sysver_min;          /* 0x42 (0x3d22) */
	int tag_status;                   /* 0x44 (0x3d24) */
	char tag_name[24];                /* 0x48 (0x3d28..0x3d40) */
	uint16_t max_ver_maj;             /* 0x60 (0x3d40) */
	uint16_t max_ver_min;             /* 0x62 (0x3d42) */
	uint16_t req_sysver_maj;          /* 0x64 (0x3d44) */
	uint16_t req_sysver_min;          /* 0x66 (0x3d46) */
	PackageNode *pkg_list;            /* 0x68 (0x3d48) */
} SceGameUpdateContext;                   /* size 0x6c (0x3ce0..0x3d4c) */

static SceGameUpdateContext g_ctx;
static XmlParser g_xml;

static int sub_047c(int mode, const GameUpdateInitParam *param);
static int sub_0650(GameUpdateResult *res, GameUpdateDetail *detail);
static int sub_0918(void);
static int sub_0a3c(void);
static int sub_0bf4(void);
static int sub_0c6c(void);
static int sub_0e48(void);
static int sub_0e7c(void);
static int sub_0ecc(void);
static int sub_0efc(void);
static int sub_0f3c(const char *str, uint16_t *out_maj, uint16_t *out_min, uint32_t flags);
static int sub_119c(void);
static int sub_1250(const char *buf, int len);
static int sub_127c(void *parser, int event, uint32_t depth, uint32_t tag_idx, const char *str1, const char *str2, void *user_arg);
static int sub_1bcc(PackageNode *node);
static int sub_1c68(XmlParser *p, const char *buf, int len);
static int sub_23b4(XmlParser *p, const XmlSchema *schema, XmlCallback cb, void *user_arg);
static int sub_2414(XmlParser *p, uint32_t *out_matched);
static int sub_2520(XmlParser *p, uint32_t *out_matched);
static int sub_2644(char *dst, uint32_t dst_size, const char *src, uint32_t src_len, uint32_t *out_len);
static int sub_28dc(char *dst, uint32_t dst_size, const char *src, uint32_t src_len, uint32_t *out_written, uint32_t *out_consumed);

int sceGameUpdate_BB4B68DE(void)
{
	if (g_ctx.state == 0) {
		return 0x80560001;
	}

	int ret = sceKernelLockLwMutex(g_ctx.mutex, 1, NULL);
	if (ret < 0) {
		return ret;
	}

	if (g_ctx.state == 2) {
		sub_0ecc();
		g_ctx.state = 3;
		goto wait_state_4;
	} else if (g_ctx.state < 3) {
		if (g_ctx.state == 1) {
			goto cleanup;
		}
		sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		return 0x80560080;
	} else if (g_ctx.state == 3) {
wait_state_4:
		sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		for (;;) {
			sceKernelDelayThread(100000);
			ret = sceKernelLockLwMutex(g_ctx.mutex, 1, NULL);
			if (ret < 0) {
				return ret;
			}
			if (g_ctx.state == 4) {
				break;
			}
			sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		}
	} else if (g_ctx.state != 4) {
		sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		return 0x80560080;
	}

	for (PackageNode *cur = g_ctx.pkg_list; cur != NULL; ) {
		PackageNode *next = cur->next;
		if (g_ctx.free_cb != NULL) {
			g_ctx.free_cb(cur);
		}
		cur = next;
	}

cleanup:
	sub_0bf4();
	sub_0e7c();
	g_ctx.state = 0;
	sceKernelDeleteLwMutex(g_ctx.mutex);
	return ret;
}

int sceGameUpdate_5F5D98A6(void)
{
	if (g_ctx.state == 0) {
		return 0x80560001;
	}

	int ret = sceKernelLockLwMutex(g_ctx.mutex, 1, NULL);
	if (ret < 0) {
		return ret;
	}

	if (g_ctx.state == 2) {
		sub_0ecc();
		g_ctx.state = 3;
	}

	sceKernelUnlockLwMutex(g_ctx.mutex, 1);
	return ret;
}

int sceGameUpdate_D92F50CA(uint32_t index, GameUpdatePkgInfo *info)
{
	if (g_ctx.state == 0) {
		return 0x80560001;
	}
	if (info == NULL) {
		return 0x80560003;
	}

	int ret = sceKernelLockLwMutex(g_ctx.mutex, 1, NULL);
	if (ret < 0) {
		return ret;
	}

	if (g_ctx.state <= 0) {
		ret = 0x80560080;
	} else if (g_ctx.state < 4) {
		ret = 0x80560082;
	} else if (g_ctx.state == 4) {
		PackageNode *cur = g_ctx.pkg_list;
		uint32_t i = 0;
		while (cur != NULL && i != index) {
			cur = cur->next;
			i++;
		}
		if (cur == NULL) {
			ret = 0x80560082;
		} else {
			info->size = cur->size;
			sceKernelMemcpy(info->sha1sum, cur->sha1sum, 20);
			sceKernelMemcpy(info->url, cur->url, 256);
		}
	} else {
		ret = 0x80560080;
	}

	sceKernelUnlockLwMutex(g_ctx.mutex, 1);
	return ret;
}

int sceGameUpdate_CBE69FB3(int mode)
{
	if (g_ctx.state != 0) {
		return 0x80560002;
	}
	if (mode != 0) {
		return 0x80560003;
	}
	return sub_047c(0, NULL);
}

int sceGameUpdate_596AD78C(GameUpdateResult *res)
{
	if (g_ctx.state == 0) {
		return 0x80560001;
	}
	if (res == NULL) {
		return 0x80560003;
	}
	return sub_0650(res, NULL);
}

int sceGameUpdate_381CB9B3(uint32_t mode, const GameUpdateInitParam *param)
{
	if (g_ctx.state != 0) {
		return 0x80560002;
	}
	if (mode > 0 || param == NULL) {
		return 0x80560003;
	}
	return sub_047c(mode, param);
}

int sceGameUpdate_3EBB6055(GameUpdateResult *res, GameUpdateDetail *detail)
{
	if (g_ctx.state == 0) {
		return 0x80560001;
	}
	if (res == NULL || detail == NULL) {
		return 0x80560003;
	}
	return sub_0650(res, detail);
}

static int sub_047c(int mode, const GameUpdateInitParam *param)
{
	(void)mode;
	char ver_buf[16];

	sceKernelMemset(&g_ctx, 0, sizeof(g_ctx));
	int ret = sceKernelCreateLwMutex(g_ctx.mutex, "SceGameUpdate", 0x200, 0, NULL);
	if (ret < 0) {
		return ret;
	}

	if (param != NULL) {
		g_ctx.alloc_cb = param->alloc_cb;
		g_ctx.free_cb = param->free_cb;
		sceNet_lib_B5CE388A(g_ctx.title_id, param->title_id, 13);
		if (param->app_ver[0] != '\0') {
			ret = sub_0f3c(param->app_ver, &g_ctx.cur_ver_maj, &g_ctx.cur_ver_min, 1);
			if (ret < 0) {
				goto err_mutex;
			}
		}
		ret = sub_0f3c(param->sys_ver, &g_ctx.cur_sysver_maj, &g_ctx.cur_sysver_min, 5);
		if (ret < 0) {
			goto err_mutex;
		}
	} else {
		ret = sceHttpStorage_bridge_EFE06A20(1, g_ctx.title_id, 16);
		if (ret < 0) {
			goto err_mutex;
		}
		ret = sceHttpStorage_bridge_EFE06A20(2, ver_buf, 8);
		if (ret < 0) {
			goto err_mutex;
		}
		if (ver_buf[0] != '\0') {
			ret = sub_0f3c(ver_buf, &g_ctx.cur_ver_maj, &g_ctx.cur_ver_min, 1);
			if (ret < 0) {
				goto err_mutex;
			}
		}
		ret = sceHttpStorage_bridge_EFE06A20(3, ver_buf, 8);
		if (ret < 0) {
			goto err_mutex;
		}
		if (ver_buf[0] != '\0') {
			ret = sub_0f3c(ver_buf, &g_ctx.cur_sysver_maj, &g_ctx.cur_sysver_min, 5);
			if (ret < 0) {
				goto err_mutex;
			}
		}
	}

	ret = sub_0e48();
	if (ret < 0) {
		goto err_mutex;
	}
	ret = sub_0918();
	if (ret < 0) {
		goto err_mutex;
	}
	ret = sub_0a3c();
	if (ret < 0) {
		goto err_tmpl;
	}
	ret = sub_119c();
	if (ret < 0) {
		goto err_conn;
	}

	g_ctx.state = 1;
	return ret;

err_conn:
	sub_0bf4();
err_tmpl:
	sub_0e7c();
err_mutex:
	sceKernelDeleteLwMutex(g_ctx.mutex);
	return ret;
}

static int sub_0650(GameUpdateResult *res, GameUpdateDetail *detail)
{
	int ret = sceKernelLockLwMutex(g_ctx.mutex, 1, NULL);
	if (ret < 0) {
		return ret;
	}

	if (g_ctx.state >= 4) {
		if (g_ctx.state == 4) {
			sceKernelUnlockLwMutex(g_ctx.mutex, 1);
			return 0;
		}
		sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		return 0x80560080;
	}
	if (g_ctx.state >= 2) {
		sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		return 0x8056000b;
	}
	if (g_ctx.state != 1) {
		sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		return 0x80560080;
	}

	g_ctx.state = 2;
	sceKernelUnlockLwMutex(g_ctx.mutex, 1);

	int check_ret = 0x80560003;
	if (res->size_struct == 0x10) {
		check_ret = sub_0c6c();
		if (check_ret >= 0) {
			if (g_ctx.tag_status == 1) {
				res->result_code = 3;
			} else {
				if (detail != NULL) {
					sceNet_lib_B5CE388A(detail->tag_name, g_ctx.tag_name, 16);
				}
				if (g_ctx.cur_sysver_maj < g_ctx.req_sysver_maj ||
				    (g_ctx.cur_sysver_maj == g_ctx.req_sysver_maj && g_ctx.cur_sysver_min < g_ctx.req_sysver_min)) {
					res->result_code = 2;
				} else if (g_ctx.max_ver_maj == 0 && g_ctx.max_ver_min == 0) {
					res->result_code = 0;
				} else {
					if (detail != NULL) {
						PackageNode *cur = g_ctx.pkg_list;
						uint32_t count = 1;
						while (cur->next != NULL) {
							cur = cur->next;
							count++;
						}
						detail->pkg_count = count;
						sceNet_lib_B5CE388A(detail->title, cur->title, 127);
					}
					res->result_code = 1;
					uint16_t maj = g_ctx.max_ver_maj;
					res->latest_ver[0] = (char)((maj / 10) + '0');
					res->latest_ver[2] = '.';
					res->latest_ver[1] = (char)((maj % 10) + '0');
					uint16_t min = g_ctx.max_ver_min;
					res->latest_ver[3] = (char)((min / 10) + '0');
					res->latest_ver[5] = '\0';
					res->latest_ver[4] = (char)((min % 10) + '0');
				}
			}
		}
	}

	ret = sceKernelLockLwMutex(g_ctx.mutex, 1, NULL);
	if (ret < 0) {
		return ret;
	}
	int old_state = g_ctx.state;
	g_ctx.state = 4;
	if (old_state == 3) {
		check_ret = 0x8056000c;
	}
	sceKernelUnlockLwMutex(g_ctx.mutex, 1);
	return check_ret;
}

static int sub_0918(void)
{
	int ret = sceHttpCreateTemplate("PSPGameUpdate-agent/1.0.0 libhttp/1.0.0", 1, 1);
	if (ret < 0) {
		goto err;
	}
	g_http.tmpl_id = ret;
	sceHttpSetConnectTimeOut(ret, 30000000);
	sceHttpSetRecvTimeOut(g_http.tmpl_id, 60000000);
	sceHttpSetSendTimeOut(g_http.tmpl_id, 60000000);
	sceHttpDeleteHeader(g_http.tmpl_id, "Accept");
	sceHttpDeleteHeader(g_http.tmpl_id, "Accept-Encoding");
	sceHttpDeleteHeader(g_http.tmpl_id, "Accept-Charset");

	ret = sceHttp_87F1E666(g_http.tmpl_id, 0x165);
	if (ret < 0) {
		goto err;
	}
	ret = sceHttp_3C478044(g_http.tmpl_id, 0x1a);
	if (ret < 0) {
		goto err;
	}
	goto out;

err:
	if (g_http.tmpl_id >= 0) {
		sceHttpDeleteTemplate(g_http.tmpl_id);
		g_http.tmpl_id = -1;
	}
out:
	if (ret == (int)0x80431022) {
		return 0x80560004;
	}
	if (ret < 0) {
		ret = 0x80560005;
	}
	return ret;
}

static int sub_0a3c(void)
{
	char url[256];
	char env[16];

	sceKernelMemset(url, 0, 256);
	sceKernelMemset(env, 0, 9);
	int ret = SysMemUserForUser_945E45DA(env);
	if (ret < 0) {
		goto out;
	}

	sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), "https://a0.ww.", 15);
	sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), env, 9);
	sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), ".dl.playstation.net/tpl/", 25);
	sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), env, 9);
	sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), "/", 2);
	sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), g_ctx.title_id, 16);
	sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), "/", 2);
	sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), g_ctx.title_id, 16);
	sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), "-ver.xml", 9);

	ret = sceHttpCreateConnectionWithURL(g_http.tmpl_id, url, 0);
	if (ret < 0) {
		goto out;
	}
	g_http.conn_id = ret;

	ret = sceHttpCreateRequestWithURL(ret, 0, url, 0);
	if (ret < 0) {
		goto out;
	}
	g_http.req_id = ret;

out:
	if (ret == (int)0x80431022) {
		return 0x80560004;
	}
	if (ret < 0) {
		ret = 0x80560005;
	}
	return ret;
}

static int sub_0bf4(void)
{
	int res = 0;
	if (g_http.req_id >= 0) {
		int r = sceHttpDeleteRequest(g_http.req_id);
		g_http.req_id = -1;
		if (r < 0) {
			res = r;
		}
	}
	if (g_http.conn_id >= 0) {
		int r = sceHttpDeleteConnection(g_http.conn_id);
		g_http.conn_id = -1;
		if (r < 0) {
			res = r;
		}
	}
	return res;
}

static int sub_0c6c(void)
{
	uint64_t total_read = 0;
	int ret = sceHttpSendRequest(g_http.req_id, NULL, 0);
	if (ret < 0) {
		goto check_err;
	}

	ret = sceHttpGetStatusCode(g_http.req_id, &g_http.status_code);
	if (ret < 0) {
		goto check_err;
	}

	if ((uint32_t)(g_http.status_code - 403) < 2U) {
		g_http.content_length = 0;
		return ret;
	}
	if (g_http.status_code != 200) {
		return 0x80560008;
	}

	ret = sceHttpGetContentLength(g_http.req_id, &g_http.content_length);
	if (ret < 0) {
		goto check_err;
	}
	if (g_http.content_length > 0xffffULL) {
		return 0x80560008;
	}
	if (g_http.content_length == 0) {
		return ret;
	}

	while (total_read < g_http.content_length) {
		int n = sceHttpReadData(g_http.req_id, g_http.read_buf, 0x800);
		ret = n;
		if (n < 0) {
			goto check_err;
		}
		ret = sub_1250((const char *)g_http.read_buf, n);
		if (ret < 0) {
			goto check_err;
		}
		total_read += (uint32_t)n;
	}

check_err:
	if (ret == (int)0x80431022) {
		return 0x80560004;
	}
	if (ret < 0) {
		if (sub_0efc() == 0) {
			return 0x80560006;
		}
		return 0x80560007;
	}
	return ret;
}

static int sub_0e48(void)
{
	g_http.tmpl_id = -1;
	g_http.content_length = 0;
	g_http.req_id = -1;
	g_http.conn_id = -1;
	g_http.status_code = 0;
	return 0;
}

static int sub_0e7c(void)
{
	int res = 0;
	if (g_http.tmpl_id >= 0) {
		int r = sceHttpDeleteTemplate(g_http.tmpl_id);
		g_http.tmpl_id = -1;
		if (r < 0) {
			res = r;
		}
	}
	return res;
}

static int sub_0ecc(void)
{
	if (g_http.req_id >= 0) {
		return sceHttpAbortRequest(g_http.req_id);
	}
	return 0;
}

static int sub_0efc(void)
{
	int info[32];
	int ret = sceNetApctlGetInfo(13, info);
	if (ret < 0) {
		return 0;
	}
	return (info[0] == 1) ? 1 : 0;
}

static int sub_0f3c(const char *str, uint16_t *out_maj, uint16_t *out_min, uint32_t flags)
{
	size_t len = sceNet_lib_D8722983(str);
	if (len == sceNet_lib_D8722983("xx.yy") && (flags & 1)) {
		goto parse;
	}
	len = sceNet_lib_D8722983(str);
	if (len == sceNet_lib_D8722983("xx.yyyy") && (flags & 2)) {
		goto parse;
	}
	len = sceNet_lib_D8722983(str);
	if (len == sceNet_lib_D8722983("x.yy") && (flags & 4)) {
		goto parse;
	}
	return 0x80560003;

parse:
	;
	const char *end = str + sceNet_lib_D8722983(str);
	const char *p;
	uint16_t maj;

	if ((flags & 3) != 0) {
		if ((sceNet_lib_750F705D(str[0]) & 4) &&
		    (sceNet_lib_750F705D(str[1]) & 4) &&
		    str[2] == '.') {
			maj = (uint16_t)((str[0] - '0') * 10 + (str[1] - '0'));
			p = str + 3;
			goto parse_min;
		}
	}
	if ((flags & 4) == 0) {
		return 0x80560003;
	}
	if ((sceNet_lib_750F705D(str[0]) & 4) == 0 || str[1] != '.') {
		return 0x80560003;
	}
	maj = (uint16_t)(str[0] - '0');
	p = str + 2;

parse_min:
	if ((sceNet_lib_750F705D(p[0]) & 4) == 0 || (sceNet_lib_750F705D(p[1]) & 4) == 0) {
		return 0x80560003;
	}
	uint16_t min = (uint16_t)((p[0] - '0') * 10 + (p[1] - '0'));
	if (flags & 2) {
		p += 2;
		if (p < end) {
			min *= 100;
			if ((sceNet_lib_750F705D(p[0]) & 4) == 0 || (sceNet_lib_750F705D(p[1]) & 4) == 0) {
				return 0x80560003;
			}
			min = (uint16_t)(min + (p[0] - '0') * 10 + (p[1] - '0'));
		}
	}
	*out_maj = maj;
	*out_min = min;
	return 0;
}

static int sub_119c(void)
{
	int lang = 1;
	sceUtilityGetSystemParamInt(8, &lang);
	g_lang_title_tag[7] = (char)(g_lang_title_tag[7] + (lang % 10));
	g_lang_title_tag[6] = (char)(g_lang_title_tag[6] + (lang / 10));
	sceKernelMemset(&g_xml, 0, sizeof(g_xml));
	return sub_23b4(&g_xml, &g_xml_schema, sub_127c, NULL);
}

static int sub_1250(const char *buf, int len)
{
	return sub_1c68(&g_xml, buf, len);
}

static int sub_127c(void *parser, int event, uint32_t depth, uint32_t tag_idx, const char *str1, const char *str2, void *user_arg)
{
	(void)parser;
	(void)user_arg;
	int ret = 0;

	if (event == 1) {
		if (depth == 0 && tag_idx == 0) {
			if (g_xml.parse_state != 0) {
				return 0x80560009;
			}
			g_xml.parse_state = 1;
		} else if (depth == 1 && tag_idx == 0) {
			if (g_xml.parse_state != 1) {
				return 0x80560009;
			}
			g_xml.parse_state = 2;
		} else if (depth == 2 && tag_idx == 0) {
			if (g_xml.parse_state != 2) {
				return 0x80560009;
			}
			sceKernelMemset(&g_xml.cur_pkg, 0, sizeof(PackageNode));
			g_xml.parse_state = 3;
		} else if (depth == 3 && tag_idx == 0) {
			if (g_xml.parse_state != 3) {
				return 0x80560009;
			}
			g_xml.parse_state = 4;
		} else if (depth == 4 && tag_idx == 0) {
			if (g_xml.parse_state != 4) {
				return 0x80560009;
			}
			g_xml.parse_state = 5;
		} else if (depth == 4 && tag_idx == 1) {
			if (g_xml.parse_state != 4) {
				return 0x80560009;
			}
			g_xml.parse_state = 6;
		}
	} else if (event == 2) {
		if (depth == 0 && tag_idx == 0) {
			if (g_xml.parse_state != 1) {
				return 0x80560009;
			}
			if ((g_xml.parse_flags & 3) != 3) {
				return 0x8056000a;
			}
			g_xml.parse_state = 0;
		} else if (depth == 1 && tag_idx == 0) {
			if (g_xml.parse_state != 2) {
				return 0x80560009;
			}
			if ((g_xml.parse_flags & 4) == 0) {
				return 0x8056000a;
			}
			g_xml.parse_state = 1;
		} else if (depth == 2 && tag_idx == 0) {
			if (g_xml.parse_state != 3) {
				return 0x80560009;
			}
			if ((g_xml.parse_flags & 0xf0) != 0xf0) {
				return 0x8056000a;
			}
			if (g_ctx.cur_ver_maj < g_xml.cur_pkg.ver_major ||
			    (g_ctx.cur_ver_maj == g_xml.cur_pkg.ver_major && g_ctx.cur_ver_min < g_xml.cur_pkg.ver_minor)) {
				if (g_ctx.max_ver_maj < g_xml.cur_pkg.ver_major ||
				    (g_ctx.max_ver_maj == g_xml.cur_pkg.ver_major && g_ctx.max_ver_min < g_xml.cur_pkg.ver_minor)) {
					g_ctx.max_ver_maj = g_xml.cur_pkg.ver_major;
					g_ctx.max_ver_min = g_xml.cur_pkg.ver_minor;
				}
				if (g_ctx.req_sysver_maj < g_xml.cur_pkg.psp_sysver_maj ||
				    (g_ctx.req_sysver_maj == g_xml.cur_pkg.psp_sysver_maj && g_ctx.req_sysver_min < g_xml.cur_pkg.psp_sysver_min)) {
					g_ctx.req_sysver_maj = g_xml.cur_pkg.psp_sysver_maj;
					g_ctx.req_sysver_min = g_xml.cur_pkg.psp_sysver_min;
				}
				if (g_ctx.alloc_cb != NULL && g_ctx.free_cb != NULL) {
					PackageNode *node = (PackageNode *)g_ctx.alloc_cb(sizeof(PackageNode));
					if (node == NULL) {
						return 0x80560081;
					}
					sceKernelMemcpy(node, &g_xml.cur_pkg, sizeof(PackageNode));
					ret = sub_1bcc(node);
					if (ret < 0) {
						g_ctx.free_cb(node);
						return ret;
					}
				}
			}
			sceKernelMemset(&g_xml.cur_pkg, 0, sizeof(PackageNode));
			g_xml.parse_state = 2;
			g_xml.parse_flags &= ~0x7f0U;
		} else if (depth == 3 && tag_idx == 0) {
			if (g_xml.parse_state != 4) {
				return 0x80560009;
			}
			g_xml.parse_state = 3;
		} else if (depth == 4 && tag_idx == 0) {
			if (g_xml.parse_state != 5) {
				return 0x80560009;
			}
			g_xml.parse_state = 4;
		} else if (depth == 4 && tag_idx == 1) {
			if (g_xml.parse_state != 6) {
				return 0x80560009;
			}
			g_xml.parse_state = 4;
		}
	} else if (event == 3) {
		if (depth == 4 && tag_idx == 0) {
			if (g_xml.parse_state != 5) {
				return 0x80560009;
			}
			if ((g_xml.parse_flags & 0x400) == 0) {
				sceNet_lib_B5CE388A(g_xml.cur_pkg.title, str1, 127);
			}
			g_xml.parse_state = 4;
			g_xml.parse_flags |= 0x200;
		} else if (depth == 4 && tag_idx == 1) {
			if (g_xml.parse_state != 6) {
				return 0x80560009;
			}
			if ((g_xml.parse_flags & 0x200) != 0) {
				sceKernelMemset(g_xml.cur_pkg.title, 0, 128);
			}
			sceNet_lib_B5CE388A(g_xml.cur_pkg.title, str1, 127);
			g_xml.parse_state = 4;
			g_xml.parse_flags |= 0x400;
		}
	} else if (event == 4) {
		if (depth == 0 && tag_idx == 0) {
			if (g_xml.parse_state != 1) {
				return 0x80560009;
			}
			if (sceNet_lib_B9085A96(str1, "titleid", sceNet_lib_D8722983("titleid") + 1) == 0) {
				if (sceNet_lib_B9085A96(str2, g_ctx.title_id, sceNet_lib_D8722983(g_ctx.title_id) + 1) != 0) {
					return 0x8056000a;
				}
				g_xml.parse_flags |= 1;
			} else if (sceNet_lib_B9085A96(str1, "status", sceNet_lib_D8722983("status") + 1) == 0) {
				if (sceNet_lib_B9085A96(str2, "alive", sceNet_lib_D8722983("alive") + 1) == 0) {
					g_ctx.tag_status = 0;
				} else {
					g_ctx.tag_status = 1;
				}
				g_xml.parse_flags |= 2;
			}
		} else if (depth == 1 && tag_idx == 0) {
			if (g_xml.parse_state != 2) {
				return 0x80560009;
			}
			if (sceNet_lib_B9085A96(str1, "name", sceNet_lib_D8722983("name") + 1) == 0) {
				sceNet_lib_B5CE388A(g_ctx.tag_name, str2, 16);
				g_xml.parse_flags |= 4;
			} else if (sceNet_lib_B9085A96(str1, "min_system_ver", sceNet_lib_D8722983("min_system_ver") + 1) == 0) {
				uint16_t maj, min;
				ret = sub_0f3c(str2, &maj, &min, 5);
				if (ret < 0) {
					return 0x8056000a;
				}
				g_xml.parse_flags |= 8;
				if (g_ctx.req_sysver_maj < maj ||
				    (g_ctx.req_sysver_maj == maj && g_ctx.req_sysver_min < min)) {
					g_ctx.req_sysver_maj = maj;
					g_ctx.req_sysver_min = min;
				}
			}
		} else if (depth == 2 && tag_idx == 0) {
			if (g_xml.parse_state != 3) {
				return 0x80560009;
			}
			if (sceNet_lib_B9085A96(str1, "version", sceNet_lib_D8722983("version") + 1) == 0) {
				ret = sub_0f3c(str2, &g_xml.cur_pkg.ver_major, &g_xml.cur_pkg.ver_minor, 1);
				if (ret < 0) {
					return 0x8056000a;
				}
				g_xml.parse_flags |= 0x10;
			} else if (sceNet_lib_B9085A96(str1, "size", sceNet_lib_D8722983("size") + 1) == 0) {
				char *endptr;
				g_xml.cur_pkg.size = sceNet_lib_303A2F80(str2, &endptr, 10);
				if (*endptr != '\0') {
					return 0x8056000a;
				}
				g_xml.parse_flags |= 0x20;
			} else if (sceNet_lib_B9085A96(str1, "sha1sum", sceNet_lib_D8722983("sha1sum") + 1) == 0) {
				if (sceNet_lib_D8722983(str2) != 40) {
					return 0x8056000a;
				}
				char hex_byte[4];
				char *endptr;
				for (int i = 0; i < 20; i++) {
					hex_byte[0] = str2[i * 2];
					hex_byte[1] = str2[i * 2 + 1];
					hex_byte[2] = '\0';
					g_xml.cur_pkg.sha1sum[i] = (uint8_t)sceNet_lib_2A73ADDC(hex_byte, &endptr, 16);
					if (*endptr != '\0') {
						return 0x8056000a;
					}
				}
				g_xml.parse_flags |= 0x40;
			} else if (sceNet_lib_B9085A96(str1, "url", sceNet_lib_D8722983("url") + 1) == 0) {
				sceNet_lib_B5CE388A(g_xml.cur_pkg.url, str2, 255);
				g_xml.parse_flags |= 0x80;
			} else if (sceNet_lib_B9085A96(str1, "psp_system_ver", sceNet_lib_D8722983("psp_system_ver") + 1) == 0) {
				ret = sub_0f3c(str2, &g_xml.cur_pkg.psp_sysver_maj, &g_xml.cur_pkg.psp_sysver_min, 5);
				if (ret < 0) {
					return 0x8056000a;
				}
				g_xml.parse_flags |= 0x100;
			}
		}
	}

	return ret;
}

static int sub_1bcc(PackageNode *node)
{
	PackageNode *cur = g_ctx.pkg_list;
	PackageNode *prev = NULL;

	if (cur == NULL) {
		node->next = g_ctx.pkg_list;
		g_ctx.pkg_list = node;
		return 0;
	}

	while (cur != NULL) {
		if (node->ver_major < cur->ver_major) {
			break;
		}
		if (node->ver_major == cur->ver_major && node->ver_minor < cur->ver_minor) {
			break;
		}
		if (node->ver_major == cur->ver_major && node->ver_minor == cur->ver_minor) {
			return 0x8056000a;
		}
		prev = cur;
		cur = cur->next;
	}

	if (prev == NULL) {
		node->next = g_ctx.pkg_list;
		g_ctx.pkg_list = node;
	} else {
		node->next = prev->next;
		prev->next = node;
	}
	return 0;
}

static int sub_1c68(XmlParser *p, const char *buf, int len)
{
	int ret = 0;
	int pos = 0;
	const char *cur = buf;
	uint32_t state = p->state;

	while (pos < len) {
		switch (state) {
		case 0:
			if ((uint8_t)*cur == '<') {
				p->empty_tag = 0;
				p->substate = 0;
				p->buf_pos = 0;
				sceKernelMemset(p->tag_buf, 0, 0xff);
				sceKernelMemset(p->val_buf, 0, 0x1ff);
				state = 1;
			}
			cur++;
			pos = (int)(cur - buf);
			break;

		case 1:
			if ((uint8_t)*cur == '/' && p->buf_pos == 0) {
				cur++;
				pos = (int)(cur - buf);
				state = 4;
				break;
			}
			/* fallthrough */
		case 4: {
			uint8_t ch = (uint8_t)*cur;
			if (p->buf_pos < 0xfe) {
				if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' &&
				    ch != '/' && ch != '<' && ch != '>') {
					p->tag_buf[p->buf_pos++] = (char)ch;
					cur++;
					pos = (int)(cur - buf);
					break;
				}
			}
			if (ch == '/') {
				p->empty_tag = 1;
			}
			p->tag_buf[p->buf_pos] = '\0';
			if (state == 1) {
				uint32_t matched = 0;
				ret = sub_2414(p, &matched);
				if (ret < 0) {
					goto out;
				}
				state = (matched != 0) ? 2 : 0;
			} else {
				uint32_t matched = 0;
				ret = sub_2520(p, &matched);
				if (ret < 0) {
					goto out;
				}
				if (matched == 1) {
					p->depth--;
				}
				state = 0;
				cur++;
				pos = (int)(cur - buf);
			}
			break;
		}

		case 2: {
			uint8_t ch = (uint8_t)*cur;
			if (ch == '\t' || ch == '\n' || ch == '\r' || ch == ' ') {
				cur++;
				pos = (int)(cur - buf);
			} else if (ch == '>') {
				uint32_t d = p->depth;
				p->depth = d + 1;
				if (p->empty_tag == 1) {
					state = 4;
					p->substate = 0;
				} else {
					const XmlTagEntry *entries = p->schema->levels[d];
					if (entries[p->tag_indices[d]].has_text == 1) {
						p->buf_pos = 0;
						sceKernelMemset(p->val_buf, 0, 0x1ff);
						p->utf8_need = 0;
						p->utf8_have = 0;
						sceKernelMemset(p->utf8_buf, 0, 6);
						state = 3;
						cur++;
						pos = (int)(cur - buf);
					} else {
						state = 0;
						cur++;
						pos = (int)(cur - buf);
					}
				}
			} else {
				p->buf_pos = 0;
				p->substate = 0;
				sceKernelMemset(p->tag_buf, 0, 0xff);
				sceKernelMemset(p->val_buf, 0, 0x1ff);
				state = 5;
			}
			break;
		}

		case 3: {
			uint32_t rem = (uint32_t)(len - pos);
			uint32_t written = 0;
			uint32_t consumed = 0;
			char tmp[8];
			uint32_t step;

			if (p->utf8_need != 0) {
				step = p->utf8_need;
				if (rem < step) {
					step = rem;
				}
				sceKernelMemcpy(p->utf8_buf + p->utf8_have, cur, step);
				p->utf8_have += step;
				p->utf8_need -= step;
				if (p->utf8_need != 0) {
					cur += step;
					pos = (int)(cur - buf);
					break;
				}
				ret = sub_28dc(tmp, 6, p->utf8_buf, p->utf8_have, &written, &consumed);
				if (ret == (int)0x80550602) {
					cur += step;
					pos = (int)(cur - buf);
					p->utf8_need = consumed - p->utf8_have;
					break;
				}
				if (ret < 0) {
					written = 1;
					consumed = 1;
					sceKernelMemcpy(tmp, p->utf8_buf, 1);
				} else {
					p->utf8_need = 0;
					p->utf8_have = 0;
				}
			} else {
				step = (rem < 6) ? rem : 6;
				ret = sub_28dc(tmp, 6, cur, step, &written, &consumed);
				if (ret == (int)0x80550602) {
					sceKernelMemcpy(p->utf8_buf, cur, step);
					cur += step;
					pos = (int)(cur - buf);
					p->utf8_need = consumed - step;
					p->utf8_have = step;
					break;
				}
				if (ret < 0) {
					written = 1;
					consumed = 1;
					sceKernelMemcpy(tmp, cur, 1);
				} else {
					step = consumed;
				}
			}

			if (p->buf_pos + written < 0x1ff) {
				if (written == 1 && consumed == 1 && (uint8_t)tmp[0] == '<') {
					goto end_text;
				}
				sceKernelMemcpy(p->val_buf + p->buf_pos, tmp, written);
				cur += step;
				pos = (int)(cur - buf);
				p->buf_pos += written;
			} else {
end_text:
				p->val_buf[p->buf_pos] = '\0';
				p->buf_pos = 0;
				sceKernelMemset(p->tag_buf, 0, 0xff);
				cur++;
				p->empty_tag = 0;
				p->substate = 0;
				state = 1;
				pos = (int)(cur - buf);
			}
			break;
		}

		case 5: {
			uint8_t ch = (uint8_t)*cur;
			if (p->buf_pos < 0xfe) {
				if (p->substate != 1) {
					if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' &&
					    ch != '/' && ch != '=' && ch != '>') {
						p->tag_buf[p->buf_pos++] = (char)ch;
						cur++;
						pos = (int)(cur - buf);
						break;
					}
					p->tag_buf[p->buf_pos] = '\0';
					p->substate = 1;
					ch = (uint8_t)*cur;
				}
				if (ch == '\t' || ch == '\n' || ch == '\r' || ch == ' ') {
					cur++;
					pos = (int)(cur - buf);
					break;
				}
				if (ch == '/') {
					p->empty_tag = 1;
					cur++;
					pos = (int)(cur - buf);
					break;
				}
				if (ch == '=') {
					cur++;
					pos = (int)(cur - buf);
					state = 6;
					p->buf_pos = 0;
					p->substate = 0;
					break;
				}
			}
			uint32_t d = p->depth;
			ret = p->callback(p, 4, d, p->tag_indices[d], p->tag_buf, p->val_buf, p->user_arg);
			if (ret < 0) {
				goto out;
			}
			state = 2;
			break;
		}

		case 6: {
			uint8_t ch = (uint8_t)*cur;
			if (p->buf_pos < 0x1fe) {
				if ((p->substate & 2) != 0) {
					if (ch != '"' && ch != '\'') {
						p->val_buf[p->buf_pos++] = (char)ch;
						cur++;
						pos = (int)(cur - buf);
						break;
					}
				}
				if (ch == '\t' || ch == '\n' || ch == '\r' || ch == ' ') {
					if ((p->substate & 1) == 0) {
						cur++;
						pos = (int)(cur - buf);
						break;
					}
					goto attr_done;
				}
				if (ch == '"' || ch == '\'') {
					cur++;
					pos = (int)(cur - buf);
					if ((p->substate & 2) != 0) {
						p->substate &= ~2U;
					} else {
						p->substate |= 3U;
					}
					break;
				}
				if (ch == '/') {
					p->empty_tag = 1;
					goto attr_done;
				}
				if (ch == '>') {
					goto attr_done;
				}
				p->substate |= 1U;
				p->val_buf[p->buf_pos++] = (char)ch;
				cur++;
				pos = (int)(cur - buf);
				break;
			}
attr_done:
			p->val_buf[p->buf_pos] = '\0';
			uint32_t d = p->depth;
			ret = p->callback(p, 4, d, p->tag_indices[d], p->tag_buf, p->val_buf, p->user_arg);
			if (ret < 0) {
				goto out;
			}
			state = 2;
			break;
		}

		default:
			goto out;
		}
	}

	ret = 0;
out:
	p->state = state;
	return ret;
}

static int sub_23b4(XmlParser *p, const XmlSchema *schema, XmlCallback cb, void *user_arg)
{
	sceKernelMemset(p, 0, 0x344);
	p->user_arg = user_arg;
	p->schema = schema;
	p->callback = cb;
	return 0;
}

static int sub_2414(XmlParser *p, uint32_t *out_matched)
{
	int ret = 0;
	if (p->depth < p->schema->max_depth) {
		const XmlTagEntry *entries = p->schema->levels[p->depth];
		uint32_t idx = 0;
		while (sceNet_lib_D8722983(entries[idx].name) != 0) {
			if (sceNet_lib_9CFBC7E3(entries[idx].name, p->tag_buf) == 0) {
				break;
			}
			idx++;
		}
		if (sceNet_lib_D8722983(entries[idx].name) != 0) {
			*out_matched = 1;
			p->tag_indices[p->depth] = idx;
			ret = p->callback(p, 1, p->depth, idx, NULL, NULL, p->user_arg);
			return ret;
		}
	}
	*out_matched = 0;
	return ret;
}

static int sub_2520(XmlParser *p, uint32_t *out_matched)
{
	if (p->depth == 0) {
		*out_matched = 0;
		return 0;
	}
	if (p->depth > p->schema->max_depth) {
		*out_matched = 0;
		return 0;
	}

	uint32_t d = p->depth - 1;
	uint32_t idx = p->tag_indices[d];
	const XmlTagEntry *entries = p->schema->levels[d];

	if (p->empty_tag != 1) {
		if (sceNet_lib_9CFBC7E3(entries[idx].name, p->tag_buf) != 0) {
			*out_matched = 0;
			return 0;
		}
	}

	*out_matched = 1;
	if (entries[idx].has_text == 1) {
		return p->callback(p, 3, d, idx, p->val_buf, NULL, p->user_arg);
	}
	return p->callback(p, 2, d, idx, NULL, NULL, p->user_arg);
}

static int sub_2644(char *dst, uint32_t dst_size, const char *src, uint32_t src_len, uint32_t *out_len)
{
	if (src == NULL || src_len == 0) {
		return 0x80550601;
	}
	int write_dst = 1;
	if (dst == NULL) {
		if (out_len == NULL) {
			return 0x80550601;
		}
		write_dst = 0;
	}

	const char *end = src + src_len;
	uint32_t count = 0;

	while (src < end) {
		count++;
		if (write_dst && dst_size < count) {
			return 0x80550602;
		}
		uint8_t ch = (uint8_t)src[0];
		if (ch == '&') {
			uint8_t c1 = (uint8_t)src[1];
			if (c1 == 'l' && src[2] == 't' && src[3] == ';') {
				if (write_dst) {
					*dst++ = '<';
				}
				src += 4;
				continue;
			}
			if (c1 == 'g' && src[2] == 't' && src[3] == ';') {
				if (write_dst) {
					*dst++ = '>';
				}
				src += 4;
				continue;
			}
			if (c1 == 'a' && src[2] == 'm' && src[3] == 'p' && src[4] == ';') {
				if (write_dst) {
					*dst++ = '&';
				}
				src += 5;
				continue;
			}
			if (c1 == 'a' && src[2] == 'p' && src[3] == 'o' && src[4] == 's' && src[5] == ';') {
				if (write_dst) {
					*dst++ = '\'';
				}
				src += 6;
				continue;
			}
			if (c1 == 'q' && src[2] == 'u' && src[3] == 'o' && src[4] == 't' && src[5] == ';') {
				if (write_dst) {
					*dst++ = '"';
				}
				src += 6;
				continue;
			}
		}
		if (write_dst) {
			*dst++ = (char)ch;
		}
		src++;
	}

	count++;
	if (write_dst && dst_size < count) {
		return 0x80550602;
	}
	if (write_dst) {
		*dst = '\0';
	}
	if (out_len != NULL) {
		*out_len = count;
	}
	return 0;
}

static int sub_28dc(char *dst, uint32_t dst_size, const char *src, uint32_t src_len, uint32_t *out_written, uint32_t *out_consumed)
{
	if (src == NULL || src_len == 0) {
		return 0x80550601;
	}
	int write_dst = 1;
	if (dst == NULL) {
		if (out_written == NULL) {
			return 0x80550601;
		}
		write_dst = 0;
	}

	uint32_t written = 0;
	uint32_t consumed = 0;
	int ret = 0;
	uint8_t c0 = (uint8_t)src[0];

	if ((int8_t)c0 >= 0) {
		written = 1;
		if (c0 == '&') {
			if (src_len < 4) {
				consumed = 4;
				ret = 0x80550602;
				goto out;
			}
			uint8_t c1 = (uint8_t)src[1];
			if (c1 == 'l' && src[2] == 't' && src[3] == ';') {
				consumed = 4;
				goto check_conv;
			}
			if (c1 == 'g' && src[2] == 't' && src[3] == ';') {
				consumed = 4;
				goto check_conv;
			}
			if (src_len < 5) {
				consumed = 5;
				ret = 0x80550602;
				goto out;
			}
			if (c1 == 'a' && src[2] == 'm' && src[3] == 'p' && src[4] == ';') {
				consumed = 5;
				goto check_conv;
			}
			if (src_len < 6) {
				consumed = 6;
				ret = 0x80550602;
				goto out;
			}
			if (c1 == 'a' && src[2] == 'p' && src[3] == 'o' && src[4] == 's' && src[5] == ';') {
				consumed = 6;
				goto check_conv;
			}
			if (c1 == 'q' && src[2] == 'u' && src[3] == 'o' && src[4] == 't' && src[5] == ';') {
				consumed = 6;
				goto check_conv;
			}
			consumed = 1;
		} else {
			consumed = 1;
		}
	} else if ((c0 & 0xe0) == 0xc0 && c0 >= 0xc2) {
		if (src_len < 2) {
			consumed = 2;
			ret = 0x80550602;
			goto out;
		}
		if (((uint8_t)src[1] & 0xc0) == 0x80) {
			written = 2;
			consumed = 2;
		} else {
			written = 1;
			consumed = 1;
		}
	} else if ((c0 & 0xf0) == 0xe0) {
		if (src_len < 3) {
			consumed = 3;
			ret = 0x80550602;
			goto out;
		}
		if (((uint8_t)src[1] & 0xc0) == 0x80 && ((uint8_t)src[2] & 0xc0) == 0x80) {
			written = 3;
			consumed = 3;
		} else {
			written = 1;
			consumed = 1;
		}
	} else if ((c0 & 0xf8) == 0xf0) {
		if (src_len < 4) {
			consumed = 4;
			ret = 0x80550602;
			goto out;
		}
		if (((uint8_t)src[1] & 0xc0) == 0x80 &&
		    ((uint8_t)src[2] & 0xc0) == 0x80 &&
		    ((uint8_t)src[3] & 0xc0) == 0x80) {
			written = 4;
			consumed = 4;
		} else {
			written = 1;
			consumed = 1;
		}
	} else {
		written = 1;
		consumed = 1;
	}

check_conv:
	if (write_dst && dst_size < written) {
		ret = 0x80550602;
		goto out;
	}

	char tmp[8];
	ret = sub_2644(tmp, 7, src, consumed, NULL);
	if (ret < 0) {
		return ret;
	}
	if (write_dst) {
		sceKernelMemcpy(dst, tmp, written);
	}

out:
	if (out_written != NULL) {
		*out_written = written;
	}
	if (out_consumed != NULL) {
		*out_consumed = consumed;
	}
	return ret;
}
