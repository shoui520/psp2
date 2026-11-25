#include <stdint.h>
#include <stddef.h>

/* SCE PSP SDK 6.60 libgameupdate.h error codes and result statuses */
#define SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED        0x80560001
#define SCE_GAMEUPDATE_ERROR_ALREADY_INITIALIZED    0x80560002
#define SCE_GAMEUPDATE_ERROR_INVALID_ARG            0x80560003
#define SCE_GAMEUPDATE_ERROR_HTTP_OUT_OF_MEMORY     0x80560004
#define SCE_GAMEUPDATE_ERROR_HTTP_LOCAL_ERROR       0x80560005
#define SCE_GAMEUPDATE_ERROR_HTTP_CONNECTION_ERROR  0x80560006
#define SCE_GAMEUPDATE_ERROR_HTTP_PROXY_ERROR       0x80560007
#define SCE_GAMEUPDATE_ERROR_HTTP_BAD_RESPONSE      0x80560008
#define SCE_GAMEUPDATE_ERROR_PARSER_FAILED          0x80560009
#define SCE_GAMEUPDATE_ERROR_BAD_RESPONSE           0x8056000a
#define SCE_GAMEUPDATE_ERROR_BUSY                   0x8056000b
#define SCE_GAMEUPDATE_ERROR_ABORTED                0x8056000c
#define SCE_GAMEUPDATE_ERROR_INTERNAL               0x80560080
#define SCE_GAMEUPDATE_ERROR_OUT_OF_MEMORY          0x80560081
#define SCE_GAMEUPDATE_ERROR_NOT_FOUND              0x80560082

#define SCE_GAMEUPDATE_RESULT_STATUS_NO_UPDATE            0
#define SCE_GAMEUPDATE_RESULT_STATUS_UPDATE_FOUND         1
#define SCE_GAMEUPDATE_RESULT_STATUS_SYSTEM_UPDATE_NEEDED 2
#define SCE_GAMEUPDATE_RESULT_STATUS_MAINTENANCE          3

#define SCE_GAMEUPDATE_APP_VER_SIZE                 6

#define SCE_HTTP_ERROR_OUT_OF_MEMORY                0x80431022
#define SCE_XML_ERROR_INVALID_ARG                   0x80550601
#define SCE_XML_ERROR_BUFFER_TOO_SHORT              0x80550602

extern void *sceKernelMemset(void *dest, int ch, size_t len);
extern int sceKernelLockLwMutex(void *workarea, int lockCount, uint32_t *pTimeout);
extern int sceKernelUnlockLwMutex(void *workarea, int unlockCount);
extern void *sceKernelMemcpy(void *dest, const void *src, size_t len);
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
extern int sceNet_lib_750F705D(int ch);
extern int sceNet_lib_9CFBC7E3(const char *lhs, const char *rhs);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, size_t len);
extern int sceNet_lib_B9085A96(const char *lhs, const char *rhs, size_t len);
extern size_t sceNet_lib_D8722983(const char *str);

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

typedef int (*XmlCallback)(void *parser, int event, uint32_t depth, uint32_t tag_idx,
                           const char *name_or_text, const char *attr_val, void *user_arg);

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
	uint32_t pad;             /* 0x34c (8-byte alignment padding for cur_pkg.size) */
	PackageNode cur_pkg;      /* 0x350..0x4f8 */
} XmlParser;                      /* size 0x4f8 */

typedef struct GameUpdateInitParam {
	void *(*alloc_cb)(uint32_t size); /* 0x00 */
	void (*free_cb)(void *ptr);       /* 0x04 */
	char title_id[13];                /* 0x08..0x15 */
	char app_ver[6];                  /* 0x15..0x1b */
	char sys_ver[9];                  /* 0x1b..0x24 */
} GameUpdateInitParam;

typedef struct SceGameUpdateResult {
	unsigned int size;                        /* 0x00 (must be 0x10) */
	int status;                               /* 0x04 */
	char app_ver[SCE_GAMEUPDATE_APP_VER_SIZE]; /* 0x08..0x0e */
	char padding[2];                          /* 0x0e..0x10 */
} SceGameUpdateResult;

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

static const XmlTagEntry g_tags_root[] = {
	{ "titlepatch", 0 },
	{ "", 1 }
};

static const XmlTagEntry g_tags_tag[] = {
	{ "tag", 0 },
	{ "", 1 }
};

static const XmlTagEntry g_tags_package[] = {
	{ "package", 0 },
	{ "", 1 }
};

static const XmlTagEntry g_tags_paramsfo[] = {
	{ "paramsfo", 0 },
	{ "", 1 }
};

static const XmlTagEntry g_tags_title[] = {
	{ "title", 1 },
	{ g_lang_title_tag, 1 },
	{ "", 1 }
};

static const XmlTagEntry * const g_tag_levels[5] = {
	g_tags_root,
	g_tags_tag,
	g_tags_package,
	g_tags_paramsfo,
	g_tags_title
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

static int gameUpdateInitInternal(int mode, const GameUpdateInitParam *param);
static int gameUpdateCheckInternal(SceGameUpdateResult *res, GameUpdateDetail *detail);
static __attribute__((noinline)) int gameUpdateHttpInitTemplate(void);
static __attribute__((noinline)) int gameUpdateHttpOpenRequest(void);
static int gameUpdateHttpCloseRequest(void);
static int gameUpdateHttpFetchXml(void);
static __attribute__((noinline)) int gameUpdateHttpReset(void);
static int gameUpdateHttpTermTemplate(void);
static int gameUpdateHttpAbort(void);
static int gameUpdateIsProxyEnabled(void);
static int gameUpdateParseVersion(const char *str, uint16_t *out_maj, uint16_t *out_min, uint32_t flags);
static __attribute__((noinline)) int gameUpdateXmlInit(void);
static int gameUpdateXmlFeed(const char *buf, int len);
static int gameUpdateXmlCallback(void *parser, int event, uint32_t depth, uint32_t tag_idx,
                                 const char *name_or_text, const char *attr_val, void *user_arg);
static int gameUpdateInsertPackage(PackageNode *node);
static int xmlParserFeed(XmlParser *parser, const char *buf, int len);
static __attribute__((noinline)) int xmlParserInit(XmlParser *parser, const XmlSchema *schema, XmlCallback cb, void *user_arg);
static int xmlParserMatchStartTag(XmlParser *parser, uint32_t *out_matched);
static int xmlParserMatchEndTag(XmlParser *parser, uint32_t *out_matched);
static int xmlUnescapeString(char *dst, uint32_t dst_size, const char *src, uint32_t src_len, uint32_t *out_len);
static int xmlDecodeCharOrEntity(char *dst, uint32_t dst_size, const char *src, uint32_t src_len,
                                 uint32_t *out_written, uint32_t *out_consumed);

int sceGameUpdate_BB4B68DE(void)
{
	if (g_ctx.state == 0) {
		return SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED;
	}

	int ret = sceKernelLockLwMutex(g_ctx.mutex, 1, NULL);
	if (ret < 0) {
		return ret;
	}

	int free_pkgs = 0;
	if (g_ctx.state == 2) {
		gameUpdateHttpAbort();
		g_ctx.state = 3;
	}

	if (g_ctx.state == 3) {
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
		free_pkgs = 1;
	} else if (g_ctx.state == 4) {
		free_pkgs = 1;
	} else if (g_ctx.state != 1) {
		sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		return SCE_GAMEUPDATE_ERROR_INTERNAL;
	}

	if (free_pkgs) {
		for (PackageNode *cur = g_ctx.pkg_list; cur != NULL; ) {
			PackageNode *next = cur->next;
			if (g_ctx.free_cb != NULL) {
				g_ctx.free_cb(cur);
			}
			cur = next;
		}
	}

	gameUpdateHttpCloseRequest();
	gameUpdateHttpTermTemplate();
	g_ctx.state = 0;
	sceKernelDeleteLwMutex(g_ctx.mutex);
	return ret;
}

int sceGameUpdate_5F5D98A6(void)
{
	if (g_ctx.state == 0) {
		return SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED;
	}

	int ret = sceKernelLockLwMutex(g_ctx.mutex, 1, NULL);
	if (ret < 0) {
		return ret;
	}

	if (g_ctx.state == 2) {
		gameUpdateHttpAbort();
		g_ctx.state = 3;
	}

	sceKernelUnlockLwMutex(g_ctx.mutex, 1);
	return ret;
}

int sceGameUpdate_D92F50CA(uint32_t index, GameUpdatePkgInfo *info)
{
	if (g_ctx.state == 0) {
		return SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED;
	}
	if (info == NULL) {
		return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
	}

	int ret = sceKernelLockLwMutex(g_ctx.mutex, 1, NULL);
	if (ret < 0) {
		return ret;
	}

	if (g_ctx.state <= 0) {
		ret = SCE_GAMEUPDATE_ERROR_INTERNAL;
	} else if (g_ctx.state < 4) {
		ret = SCE_GAMEUPDATE_ERROR_NOT_FOUND;
	} else if (g_ctx.state == 4) {
		PackageNode *cur = g_ctx.pkg_list;
		uint32_t idx = 0;
		while (cur != NULL && idx != index) {
			cur = cur->next;
			idx++;
		}
		if (cur == NULL) {
			ret = SCE_GAMEUPDATE_ERROR_NOT_FOUND;
		} else {
			info->size = cur->size;
			sceKernelMemcpy(info->sha1sum, cur->sha1sum, 20);
			sceKernelMemcpy(info->url, cur->url, 256);
		}
	} else {
		ret = SCE_GAMEUPDATE_ERROR_INTERNAL;
	}

	sceKernelUnlockLwMutex(g_ctx.mutex, 1);
	return ret;
}

int sceGameUpdate_CBE69FB3(int mode)
{
	if (g_ctx.state != 0) {
		return SCE_GAMEUPDATE_ERROR_ALREADY_INITIALIZED;
	}
	if (mode != 0) {
		return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
	}
	return gameUpdateInitInternal(0, NULL);
}

int sceGameUpdate_596AD78C(SceGameUpdateResult *res)
{
	if (g_ctx.state == 0) {
		return SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED;
	}
	if (res == NULL) {
		return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
	}
	return gameUpdateCheckInternal(res, NULL);
}

int sceGameUpdate_381CB9B3(uint32_t mode, const GameUpdateInitParam *param)
{
	if (g_ctx.state != 0) {
		return SCE_GAMEUPDATE_ERROR_ALREADY_INITIALIZED;
	}
	if ((mode > 0) | (param == NULL)) {
		return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
	}
	return gameUpdateInitInternal((int)mode, param);
}

int sceGameUpdate_3EBB6055(SceGameUpdateResult *res, GameUpdateDetail *detail)
{
	if (g_ctx.state == 0) {
		return SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED;
	}
	if ((res == NULL) | (detail == NULL)) {
		return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
	}
	return gameUpdateCheckInternal(res, detail);
}

static int gameUpdateInitInternal(int mode, const GameUpdateInitParam *param)
{
	(void)mode;
	char ver_buf[16];

	sceKernelMemset(&g_ctx, 0, sizeof(g_ctx));
	int ret = sceKernelCreateLwMutex(g_ctx.mutex, "SceGameUpdate", 0x200, 0, NULL);
	if (ret < 0) {
		return ret;
	}

	int stage = 0;
	do {
		if (param != NULL) {
			g_ctx.alloc_cb = param->alloc_cb;
			g_ctx.free_cb = param->free_cb;
			sceNet_lib_B5CE388A(g_ctx.title_id, param->title_id, 13);
			if (param->app_ver[0] != '\0') {
				ret = gameUpdateParseVersion(param->app_ver, &g_ctx.cur_ver_maj, &g_ctx.cur_ver_min, 1);
				if (ret < 0) {
					break;
				}
			}
			ret = gameUpdateParseVersion(param->sys_ver, &g_ctx.cur_sysver_maj, &g_ctx.cur_sysver_min, 5);
			if (ret < 0) {
				break;
			}
		} else {
			ret = sceHttpStorage_bridge_EFE06A20(1, g_ctx.title_id, 16);
			if (ret < 0) {
				break;
			}
			ret = sceHttpStorage_bridge_EFE06A20(2, ver_buf, 8);
			if (ret < 0) {
				break;
			}
			if (ver_buf[0] != '\0') {
				ret = gameUpdateParseVersion(ver_buf, &g_ctx.cur_ver_maj, &g_ctx.cur_ver_min, 1);
				if (ret < 0) {
					break;
				}
			}
			ret = sceHttpStorage_bridge_EFE06A20(3, ver_buf, 8);
			if (ret < 0) {
				break;
			}
			if (ver_buf[0] != '\0') {
				ret = gameUpdateParseVersion(ver_buf, &g_ctx.cur_sysver_maj, &g_ctx.cur_sysver_min, 5);
				if (ret < 0) {
					break;
				}
			}
		}

		ret = gameUpdateHttpReset();
		if (ret < 0) {
			break;
		}
		ret = gameUpdateHttpInitTemplate();
		if (ret < 0) {
			break;
		}
		stage = 1;
		ret = gameUpdateHttpOpenRequest();
		if (ret < 0) {
			break;
		}
		stage = 2;
		ret = gameUpdateXmlInit();
		if (ret < 0) {
			break;
		}

		g_ctx.state = 1;
		return ret;
	} while (0);

	if (stage >= 2) {
		gameUpdateHttpCloseRequest();
	}
	if (stage >= 1) {
		gameUpdateHttpTermTemplate();
	}
	sceKernelDeleteLwMutex(g_ctx.mutex);
	return ret;
}

static int gameUpdateCheckInternal(SceGameUpdateResult *res, GameUpdateDetail *detail)
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
		return SCE_GAMEUPDATE_ERROR_INTERNAL;
	}
	if (g_ctx.state >= 2) {
		sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		return SCE_GAMEUPDATE_ERROR_BUSY;
	}
	if (g_ctx.state != 1) {
		sceKernelUnlockLwMutex(g_ctx.mutex, 1);
		return SCE_GAMEUPDATE_ERROR_INTERNAL;
	}

	g_ctx.state = 2;
	sceKernelUnlockLwMutex(g_ctx.mutex, 1);

	int check_ret = SCE_GAMEUPDATE_ERROR_INVALID_ARG;
	if (res->size == sizeof(SceGameUpdateResult)) {
		check_ret = gameUpdateHttpFetchXml();
		if (check_ret >= 0) {
			if (g_ctx.tag_status == 1) {
				res->status = SCE_GAMEUPDATE_RESULT_STATUS_MAINTENANCE;
			} else {
				if (detail != NULL) {
					sceNet_lib_B5CE388A(detail->tag_name, g_ctx.tag_name, 16);
				}
				if (g_ctx.cur_sysver_maj < g_ctx.req_sysver_maj ||
				    (g_ctx.cur_sysver_maj == g_ctx.req_sysver_maj && g_ctx.cur_sysver_min < g_ctx.req_sysver_min)) {
					res->status = SCE_GAMEUPDATE_RESULT_STATUS_SYSTEM_UPDATE_NEEDED;
				} else if (g_ctx.max_ver_maj == 0 && g_ctx.max_ver_min == 0) {
					res->status = SCE_GAMEUPDATE_RESULT_STATUS_NO_UPDATE;
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
					res->status = SCE_GAMEUPDATE_RESULT_STATUS_UPDATE_FOUND;
					uint16_t maj = g_ctx.max_ver_maj;
					res->app_ver[0] = (char)((maj / 10) + '0');
					res->app_ver[2] = '.';
					res->app_ver[1] = (char)((maj % 10) + '0');
					uint16_t min = g_ctx.max_ver_min;
					res->app_ver[3] = (char)((min / 10) + '0');
					res->app_ver[5] = '\0';
					res->app_ver[4] = (char)((min % 10) + '0');
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
		check_ret = SCE_GAMEUPDATE_ERROR_ABORTED;
	}
	sceKernelUnlockLwMutex(g_ctx.mutex, 1);
	return check_ret;
}

static int gameUpdateHttpInitTemplate(void)
{
	int failed = 0;
	int ret = sceHttpCreateTemplate("PSPGameUpdate-agent/1.0.0 libhttp/1.0.0", 1, 1);
	if (ret < 0) {
		failed = 1;
	} else {
		g_http.tmpl_id = ret;
		sceHttpSetConnectTimeOut(ret, 30000000);
		sceHttpSetRecvTimeOut(g_http.tmpl_id, 60000000);
		sceHttpSetSendTimeOut(g_http.tmpl_id, 60000000);
		sceHttpDeleteHeader(g_http.tmpl_id, "Accept");
		sceHttpDeleteHeader(g_http.tmpl_id, "Accept-Encoding");
		sceHttpDeleteHeader(g_http.tmpl_id, "Accept-Charset");

		ret = sceHttp_87F1E666(g_http.tmpl_id, 0x165);
		if (ret < 0) {
			failed = 1;
		} else {
			ret = sceHttp_3C478044(g_http.tmpl_id, 0x1a);
			if (ret < 0) {
				failed = 1;
			}
		}
	}

	if (failed && g_http.tmpl_id >= 0) {
		sceHttpDeleteTemplate(g_http.tmpl_id);
		g_http.tmpl_id = -1;
	}
	if (ret == (int)SCE_HTTP_ERROR_OUT_OF_MEMORY) {
		return SCE_GAMEUPDATE_ERROR_HTTP_OUT_OF_MEMORY;
	}
	if (ret < 0) {
		ret = SCE_GAMEUPDATE_ERROR_HTTP_LOCAL_ERROR;
	}
	return ret;
}

static int gameUpdateHttpOpenRequest(void)
{
	char url[256];
	char env[16];

	sceKernelMemset(url, 0, 256);
	sceKernelMemset(env, 0, 9);
	int ret = SysMemUserForUser_945E45DA(env);
	if (ret >= 0) {
		sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), "https://" "a" "0.ww.", 15);
		sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), env, 9);
		sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), ".dl.playstation.net/tpl/", 25);
		sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), env, 9);
		sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), "/", 2);
		sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), g_ctx.title_id, 16);
		sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), "/", 2);
		sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), g_ctx.title_id, 16);
		sceNet_lib_B5CE388A(url + sceNet_lib_D8722983(url), "-ver.xml", 9);

		ret = sceHttpCreateConnectionWithURL(g_http.tmpl_id, url, 0);
		if (ret >= 0) {
			g_http.conn_id = ret;
			ret = sceHttpCreateRequestWithURL(ret, 0, url, 0);
			if (ret >= 0) {
				g_http.req_id = ret;
			}
		}
	}

	if (ret == (int)SCE_HTTP_ERROR_OUT_OF_MEMORY) {
		return SCE_GAMEUPDATE_ERROR_HTTP_OUT_OF_MEMORY;
	}
	if (ret < 0) {
		ret = SCE_GAMEUPDATE_ERROR_HTTP_LOCAL_ERROR;
	}
	return ret;
}

static int gameUpdateHttpCloseRequest(void)
{
	int res = 0;
	if (g_http.req_id >= 0) {
		int del_ret = sceHttpDeleteRequest(g_http.req_id);
		g_http.req_id = -1;
		res = (del_ret > 0) ? del_ret : 0;
	}
	if (g_http.conn_id >= 0) {
		int del_ret = sceHttpDeleteConnection(g_http.conn_id);
		g_http.conn_id = -1;
		res = (del_ret > 0) ? del_ret : 0;
	}
	return res;
}

static int gameUpdateHttpFetchXml(void)
{
	uint64_t total_read = 0;
	int ret = sceHttpSendRequest(g_http.req_id, NULL, 0);
	if (ret >= 0) {
		ret = sceHttpGetStatusCode(g_http.req_id, &g_http.status_code);
	}
	if (ret >= 0) {
		if ((uint32_t)(g_http.status_code - 403) < 2U) {
			g_http.content_length = 0;
			return ret;
		}
		if (g_http.status_code != 200) {
			return SCE_GAMEUPDATE_ERROR_HTTP_BAD_RESPONSE;
		}

		ret = sceHttpGetContentLength(g_http.req_id, &g_http.content_length);
		if (ret >= 0) {
			if (g_http.content_length > 0xffffULL) {
				return SCE_GAMEUPDATE_ERROR_HTTP_BAD_RESPONSE;
			}
			if (g_http.content_length == 0) {
				return ret;
			}

			while (total_read < g_http.content_length) {
				int read_len = sceHttpReadData(g_http.req_id, g_http.read_buf, 0x800);
				ret = read_len;
				if (read_len < 0) {
					break;
				}
				ret = gameUpdateXmlFeed((const char *)g_http.read_buf, read_len);
				if (ret < 0) {
					break;
				}
				total_read += (uint32_t)read_len;
			}
		}
	}

	if (ret == (int)SCE_HTTP_ERROR_OUT_OF_MEMORY) {
		return SCE_GAMEUPDATE_ERROR_HTTP_OUT_OF_MEMORY;
	}
	if (ret < 0) {
		if (gameUpdateIsProxyEnabled() == 0) {
			return SCE_GAMEUPDATE_ERROR_HTTP_CONNECTION_ERROR;
		}
		return SCE_GAMEUPDATE_ERROR_HTTP_PROXY_ERROR;
	}
	return ret;
}

static int gameUpdateHttpReset(void)
{
	g_http.tmpl_id = -1;
	g_http.content_length = 0;
	g_http.req_id = -1;
	g_http.conn_id = -1;
	g_http.status_code = 0;
	return 0;
}

static int gameUpdateHttpTermTemplate(void)
{
	int res = 0;
	if (g_http.tmpl_id >= 0) {
		int del_ret = sceHttpDeleteTemplate(g_http.tmpl_id);
		g_http.tmpl_id = -1;
		res = (del_ret > 0) ? del_ret : 0;
	}
	return res;
}

static int gameUpdateHttpAbort(void)
{
	if (g_http.req_id >= 0) {
		return sceHttpAbortRequest(g_http.req_id);
	}
	return 0;
}

static int gameUpdateIsProxyEnabled(void)
{
	int info[32];
	int ret = sceNetApctlGetInfo(13, info);
	if (ret < 0) {
		return 0;
	}
	return (info[0] == 1) ? 1 : 0;
}

static int gameUpdateParseVersion(const char *str, uint16_t *out_maj, uint16_t *out_min, uint32_t flags)
{
	int valid_len = 0;
	if (sceNet_lib_D8722983(str) == sceNet_lib_D8722983("xx.yy") && (flags & 1)) {
		valid_len = 1;
	} else if (sceNet_lib_D8722983(str) == sceNet_lib_D8722983("xx.yyyy") && (flags & 2)) {
		valid_len = 1;
	} else if (sceNet_lib_D8722983(str) == sceNet_lib_D8722983("x.yy") && (flags & 4)) {
		valid_len = 1;
	}
	if (!valid_len) {
		return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
	}

	const char *end = str + sceNet_lib_D8722983(str);
	const char *pos = NULL;
	uint16_t maj = 0;

	if ((flags & 3) != 0 &&
	    (sceNet_lib_750F705D(str[0]) & 4) &&
	    (sceNet_lib_750F705D(str[1]) & 4) &&
	    str[2] == '.') {
		maj = (uint16_t)((str[0] - '0') * 10 + (str[1] - '0'));
		pos = str + 3;
	} else {
		if ((flags & 4) == 0) {
			return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
		}
		if ((sceNet_lib_750F705D(str[0]) & 4) == 0 || str[1] != '.') {
			return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
		}
		maj = (uint16_t)(str[0] - '0');
		pos = str + 2;
	}

	if ((sceNet_lib_750F705D(pos[0]) & 4) == 0 || (sceNet_lib_750F705D(pos[1]) & 4) == 0) {
		return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
	}
	uint16_t min = (uint16_t)((pos[0] - '0') * 10 + (pos[1] - '0'));
	if (flags & 2) {
		pos += 2;
		min = (uint16_t)(min * 100);
		if (pos < end) {
			if ((sceNet_lib_750F705D(pos[0]) & 4) == 0 || (sceNet_lib_750F705D(pos[1]) & 4) == 0) {
				return SCE_GAMEUPDATE_ERROR_INVALID_ARG;
			}
			min = (uint16_t)(min + (pos[0] - '0') * 10 + (pos[1] - '0'));
		}
	}
	*out_maj = maj;
	*out_min = min;
	return 0;
}

static int gameUpdateXmlInit(void)
{
	int lang = 1;
	sceUtilityGetSystemParamInt(8, &lang);
	g_lang_title_tag[7] = (char)(g_lang_title_tag[7] + (lang % 10));
	g_lang_title_tag[6] = (char)(g_lang_title_tag[6] + (lang / 10));
	sceKernelMemset(&g_xml, 0, sizeof(g_xml));
	return xmlParserInit(&g_xml, &g_xml_schema, gameUpdateXmlCallback, NULL);
}

static int gameUpdateXmlFeed(const char *buf, int len)
{
	return xmlParserFeed(&g_xml, buf, len);
}

static int gameUpdateXmlCallback(void *parser, int event, uint32_t depth, uint32_t tag_idx,
                                 const char *name_or_text, const char *attr_val, void *user_arg)
{
	(void)parser;
	(void)user_arg;
	int ret = 0;

	if (event == 1) {
		if (depth == 0 && tag_idx == 0) {
			if (g_xml.parse_state != 0) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			g_xml.parse_state = 1;
		} else if (depth == 1 && tag_idx == 0) {
			if (g_xml.parse_state != 1) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			g_xml.parse_state = 2;
		} else if (depth == 2 && tag_idx == 0) {
			if (g_xml.parse_state != 2) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			sceKernelMemset(&g_xml.cur_pkg, 0, sizeof(PackageNode));
			g_xml.parse_state = 3;
		} else if (depth == 3 && tag_idx == 0) {
			if (g_xml.parse_state != 3) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			g_xml.parse_state = 4;
		} else if (depth == 4 && tag_idx == 0) {
			if (g_xml.parse_state != 4) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			g_xml.parse_state = 5;
		} else if (depth == 4 && tag_idx == 1) {
			if (g_xml.parse_state != 4) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			g_xml.parse_state = 6;
		}
	} else if (event == 2) {
		if (depth == 0 && tag_idx == 0) {
			if (g_xml.parse_state != 1) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			if ((g_xml.parse_flags & 3) != 3) {
				return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
			}
			g_xml.parse_state = 0;
		} else if (depth == 1 && tag_idx == 0) {
			if (g_xml.parse_state != 2) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			if ((g_xml.parse_flags & 4) == 0) {
				return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
			}
			g_xml.parse_state = 1;
		} else if (depth == 2 && tag_idx == 0) {
			if (g_xml.parse_state != 3) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			if ((g_xml.parse_flags & 0xf0) != 0xf0) {
				return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
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
						return SCE_GAMEUPDATE_ERROR_OUT_OF_MEMORY;
					}
					sceKernelMemcpy(node, &g_xml.cur_pkg, sizeof(PackageNode));
					ret = gameUpdateInsertPackage(node);
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
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			g_xml.parse_state = 3;
		} else if (depth == 4 && tag_idx == 0) {
			if (g_xml.parse_state != 5) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			g_xml.parse_state = 4;
		} else if (depth == 4 && tag_idx == 1) {
			if (g_xml.parse_state != 6) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			g_xml.parse_state = 4;
		}
	} else if (event == 3) {
		if (depth == 4 && tag_idx == 0) {
			if (g_xml.parse_state != 5) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			if ((g_xml.parse_flags & 0x400) == 0) {
				sceNet_lib_B5CE388A(g_xml.cur_pkg.title, name_or_text, 127);
			}
			g_xml.parse_state = 4;
			g_xml.parse_flags |= 0x200;
		} else if (depth == 4 && tag_idx == 1) {
			if (g_xml.parse_state != 6) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			if ((g_xml.parse_flags & 0x200) != 0) {
				sceKernelMemset(g_xml.cur_pkg.title, 0, 128);
			}
			sceNet_lib_B5CE388A(g_xml.cur_pkg.title, name_or_text, 127);
			g_xml.parse_state = 4;
			g_xml.parse_flags |= 0x400;
		}
	} else if (event == 4) {
		if (depth == 0 && tag_idx == 0) {
			if (g_xml.parse_state != 1) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			if (sceNet_lib_B9085A96(name_or_text, "titleid", sceNet_lib_D8722983("titleid") + 1) == 0) {
				(void)sceNet_lib_B9085A96(attr_val, g_ctx.title_id, sceNet_lib_D8722983(g_ctx.title_id) + 1);
				g_xml.parse_flags |= 1;
			} else if (sceNet_lib_B9085A96(name_or_text, "status", sceNet_lib_D8722983("status") + 1) == 0) {
				if (sceNet_lib_B9085A96(attr_val, "alive", sceNet_lib_D8722983("alive") + 1) == 0) {
					g_ctx.tag_status = 0;
				} else {
					g_ctx.tag_status = 1;
				}
				g_xml.parse_flags |= 2;
			}
		} else if (depth == 1 && tag_idx == 0) {
			if (g_xml.parse_state != 2) {
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			if (sceNet_lib_B9085A96(name_or_text, "name", sceNet_lib_D8722983("name") + 1) == 0) {
				sceNet_lib_B5CE388A(g_ctx.tag_name, attr_val, 16);
				g_xml.parse_flags |= 4;
			} else if (sceNet_lib_B9085A96(name_or_text, "min_system_ver", sceNet_lib_D8722983("min_system_ver") + 1) == 0) {
				uint16_t maj, min;
				ret = gameUpdateParseVersion(attr_val, &maj, &min, 5);
				if (ret < 0) {
					return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
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
				return SCE_GAMEUPDATE_ERROR_PARSER_FAILED;
			}
			if (sceNet_lib_B9085A96(name_or_text, "version", sceNet_lib_D8722983("version") + 1) == 0) {
				ret = gameUpdateParseVersion(attr_val, &g_xml.cur_pkg.ver_major, &g_xml.cur_pkg.ver_minor, 1);
				if (ret < 0) {
					return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
				}
				g_xml.parse_flags |= 0x10;
			} else if (sceNet_lib_B9085A96(name_or_text, "size", sceNet_lib_D8722983("size") + 1) == 0) {
				char *endptr;
				g_xml.cur_pkg.size = sceNet_lib_303A2F80(attr_val, &endptr, 10);
				if (*endptr != '\0') {
					return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
				}
				g_xml.parse_flags |= 0x20;
			} else if (sceNet_lib_B9085A96(name_or_text, "sha1sum", sceNet_lib_D8722983("sha1sum") + 1) == 0) {
				if (sceNet_lib_D8722983(attr_val) != 40) {
					return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
				}
				char hex_byte[4];
				char *endptr;
				for (int i = 0; i < 20; i++) {
					hex_byte[0] = attr_val[i * 2];
					hex_byte[1] = attr_val[i * 2 + 1];
					hex_byte[2] = '\0';
					g_xml.cur_pkg.sha1sum[i] = (uint8_t)sceNet_lib_2A73ADDC(hex_byte, &endptr, 16);
					if (*endptr != '\0') {
						return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
					}
				}
				g_xml.parse_flags |= 0x40;
			} else if (sceNet_lib_B9085A96(name_or_text, "url", sceNet_lib_D8722983("url") + 1) == 0) {
				sceNet_lib_B5CE388A(g_xml.cur_pkg.url, attr_val, 255);
				g_xml.parse_flags |= 0x80;
			} else if (sceNet_lib_B9085A96(name_or_text, "psp_system_ver", sceNet_lib_D8722983("psp_system_ver") + 1) == 0) {
				ret = gameUpdateParseVersion(attr_val, &g_xml.cur_pkg.psp_sysver_maj, &g_xml.cur_pkg.psp_sysver_min, 5);
				if (ret < 0) {
					return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
				}
				g_xml.parse_flags |= 0x100;
			}
		}
	}

	return ret;
}

static int gameUpdateInsertPackage(PackageNode *node)
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
			return SCE_GAMEUPDATE_ERROR_BAD_RESPONSE;
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

static int xmlParserFeed(XmlParser *parser, const char *buf, int len)
{
	int ret = 0;
	int pos = 0;
	const char *cur = buf;
	uint32_t state = parser->state;

	while (pos < len) {
		switch (state) {
		case 0:
			if ((uint8_t)*cur == '<') {
				parser->empty_tag = 0;
				parser->substate = 0;
				parser->buf_pos = 0;
				sceKernelMemset(parser->tag_buf, 0, 0xff);
				sceKernelMemset(parser->val_buf, 0, 0x1ff);
				state = 1;
			}
			cur++;
			pos = (int)(cur - buf);
			break;

		case 1:
			if ((uint8_t)*cur == '/' && parser->buf_pos == 0) {
				cur++;
				pos = (int)(cur - buf);
				state = 4;
				break;
			}
			/* fallthrough */
		case 4: {
			uint8_t ch = (uint8_t)*cur;
			if (parser->buf_pos < 0xfe) {
				if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' &&
				    ch != '/' && ch != '<' && ch != '>') {
					parser->tag_buf[parser->buf_pos++] = (char)ch;
					cur++;
					pos = (int)(cur - buf);
					break;
				}
			}
			if (ch == '/') {
				parser->empty_tag = 1;
			}
			parser->tag_buf[parser->buf_pos] = '\0';
			if (state == 1) {
				uint32_t matched = 0;
				ret = xmlParserMatchStartTag(parser, &matched);
				if (ret < 0) {
					return ret;
				}
				state = (matched != 0) ? 2 : 0;
			} else {
				uint32_t matched = 0;
				ret = xmlParserMatchEndTag(parser, &matched);
				if (ret < 0) {
					return ret;
				}
				if (matched == 1) {
					parser->depth--;
				}
				state = 0;
				cur++;
				pos = (int)(cur - buf);
			}
			break;
		}

		case 2: {
			uint8_t ch = (uint8_t)*cur;
			if (ch == '\t' || ch == '\n' || ch == '\r' || ch == ' ' || ch == '/') {
				cur++;
				pos = (int)(cur - buf);
			} else if (ch == '>') {
				uint32_t depth = parser->depth;
				parser->depth = depth + 1;
				if (parser->empty_tag == 1) {
					state = 4;
					parser->substate = 0;
				} else {
					const XmlTagEntry *entries = parser->schema->levels[depth];
					if (entries[parser->tag_indices[depth]].has_text == 1) {
						parser->buf_pos = 0;
						sceKernelMemset(parser->val_buf, 0, 0x1ff);
						parser->utf8_need = 0;
						parser->utf8_have = 0;
						sceKernelMemset(parser->utf8_buf, 0, 6);
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
				parser->buf_pos = 0;
				parser->substate = 0;
				sceKernelMemset(parser->tag_buf, 0, 0xff);
				sceKernelMemset(parser->val_buf, 0, 0x1ff);
				state = 5;
			}
			break;
		}

		case 3: {
			uint32_t rem = (uint32_t)(len - pos);
			uint32_t written = 0;
			uint32_t consumed = 0;
			char decoded[8];
			uint32_t step;

			if (parser->utf8_need != 0) {
				step = parser->utf8_need;
				if (rem < step) {
					step = rem;
				}
				sceKernelMemcpy(parser->utf8_buf + parser->utf8_have, cur, step);
				parser->utf8_have += step;
				parser->utf8_need -= step;
				if (parser->utf8_need != 0) {
					cur += step;
					pos = (int)(cur - buf);
					break;
				}
				ret = xmlDecodeCharOrEntity(decoded, 6, parser->utf8_buf, parser->utf8_have, &written, &consumed);
				if (ret == (int)SCE_XML_ERROR_BUFFER_TOO_SHORT) {
					cur += step;
					pos = (int)(cur - buf);
					parser->utf8_need = consumed - parser->utf8_have;
					break;
				}
				if (ret < 0) {
					written = 1;
					consumed = 1;
					sceKernelMemcpy(decoded, parser->utf8_buf, 1);
				} else {
					parser->utf8_need = 0;
					parser->utf8_have = 0;
				}
			} else {
				step = (rem < 6) ? rem : 6;
				ret = xmlDecodeCharOrEntity(decoded, 6, cur, step, &written, &consumed);
				if (ret == (int)SCE_XML_ERROR_BUFFER_TOO_SHORT) {
					sceKernelMemcpy(parser->utf8_buf, cur, step);
					cur += step;
					pos = (int)(cur - buf);
					parser->utf8_need = consumed - step;
					parser->utf8_have = step;
					break;
				}
				if (ret < 0) {
					written = 1;
					consumed = 1;
					step = 1;
					sceKernelMemcpy(decoded, cur, 1);
				} else {
					step = consumed;
				}
			}

			int end_text = 0;
			if (parser->buf_pos + written >= 0x1ff) {
				end_text = 1;
			} else if (written == 1 && consumed == 1 && (uint8_t)decoded[0] == '<') {
				end_text = 1;
			}

			if (end_text) {
				parser->val_buf[parser->buf_pos] = '\0';
				parser->buf_pos = 0;
				sceKernelMemset(parser->tag_buf, 0, 0xff);
				cur++;
				parser->empty_tag = 0;
				parser->substate = 0;
				state = 1;
				pos = (int)(cur - buf);
			} else {
				sceKernelMemcpy(parser->val_buf + parser->buf_pos, decoded, written);
				cur += step;
				pos = (int)(cur - buf);
				parser->buf_pos += written;
			}
			break;
		}

		case 5: {
			uint8_t ch = (uint8_t)*cur;
			if (parser->buf_pos < 0xfe) {
				if (parser->substate != 1) {
					if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' &&
					    ch != '/' && ch != '=' && ch != '>') {
						parser->tag_buf[parser->buf_pos++] = (char)ch;
						cur++;
						pos = (int)(cur - buf);
						break;
					}
					parser->tag_buf[parser->buf_pos] = '\0';
					parser->substate = 1;
					ch = (uint8_t)*cur;
				}
				if (ch == '\t' || ch == '\n' || ch == '\r' || ch == ' ') {
					cur++;
					pos = (int)(cur - buf);
					break;
				}
				if (ch == '/') {
					parser->empty_tag = 1;
					cur++;
					pos = (int)(cur - buf);
					break;
				}
				if (ch == '=') {
					cur++;
					pos = (int)(cur - buf);
					state = 6;
					parser->buf_pos = 0;
					parser->substate = 0;
					break;
				}
			}
			uint32_t depth = parser->depth;
			ret = parser->callback(parser, 4, depth, parser->tag_indices[depth],
			                       parser->tag_buf, parser->val_buf, parser->user_arg);
			if (ret < 0) {
				return ret;
			}
			state = 2;
			break;
		}

		case 6: {
			uint8_t ch = (uint8_t)*cur;
			int attr_done = 0;
			if (parser->buf_pos < 0x1fe) {
				if ((parser->substate & 2) != 0 && ch != '"' && ch != '\'') {
					parser->val_buf[parser->buf_pos++] = (char)ch;
					cur++;
					pos = (int)(cur - buf);
					break;
				}
				if (ch == '\t' || ch == '\n' || ch == '\r' || ch == ' ') {
					if ((parser->substate & 1) == 0) {
						cur++;
						pos = (int)(cur - buf);
						break;
					}
					attr_done = 1;
				} else if (ch == '"' || ch == '\'') {
					cur++;
					pos = (int)(cur - buf);
					if ((parser->substate & 2) != 0) {
						parser->substate &= ~2U;
					} else {
						parser->substate |= 3U;
					}
					break;
				} else if (ch == '/') {
					parser->empty_tag = 1;
					attr_done = 1;
				} else if (ch == '>') {
					attr_done = 1;
				} else {
					parser->substate |= 1U;
					parser->val_buf[parser->buf_pos++] = (char)ch;
					cur++;
					pos = (int)(cur - buf);
					break;
				}
			} else {
				attr_done = 1;
			}
			if (attr_done) {
				parser->val_buf[parser->buf_pos] = '\0';
				uint32_t depth = parser->depth;
				ret = parser->callback(parser, 4, depth, parser->tag_indices[depth],
				                       parser->tag_buf, parser->val_buf, parser->user_arg);
				if (ret < 0) {
					return ret;
				}
				state = 2;
			}
			break;
		}

		default:
			parser->state = state;
			return ret;
		}
	}

	parser->state = state;
	return 0;
}

static int xmlParserInit(XmlParser *parser, const XmlSchema *schema, XmlCallback cb, void *user_arg)
{
	sceKernelMemset(parser, 0, 0x344);
	parser->user_arg = user_arg;
	parser->schema = schema;
	parser->callback = cb;
	return 0;
}

static int xmlParserMatchStartTag(XmlParser *parser, uint32_t *out_matched)
{
	int ret = 0;
	if (parser->depth < parser->schema->max_depth) {
		const XmlTagEntry *entries = parser->schema->levels[parser->depth];
		uint32_t idx = 0;
		while (sceNet_lib_D8722983(entries[idx].name) != 0) {
			if (sceNet_lib_9CFBC7E3(entries[idx].name, parser->tag_buf) == 0) {
				break;
			}
			idx++;
		}
		if (sceNet_lib_D8722983(entries[idx].name) != 0) {
			*out_matched = 1;
			parser->tag_indices[parser->depth] = idx;
			ret = parser->callback(parser, 1, parser->depth, idx, NULL, NULL, parser->user_arg);
			return ret;
		}
	}
	*out_matched = 0;
	return ret;
}

static int xmlParserMatchEndTag(XmlParser *parser, uint32_t *out_matched)
{
	if (parser->depth == 0) {
		*out_matched = 0;
		return 0;
	}
	if (parser->depth > parser->schema->max_depth) {
		*out_matched = 0;
		return 0;
	}

	uint32_t depth = parser->depth - 1;
	uint32_t idx = parser->tag_indices[depth];
	const XmlTagEntry *entries = parser->schema->levels[depth];

	if (parser->empty_tag != 1) {
		if (sceNet_lib_9CFBC7E3(entries[idx].name, parser->tag_buf) != 0) {
			*out_matched = 0;
			return 0;
		}
	}

	*out_matched = 1;
	if (entries[idx].has_text == 1) {
		return parser->callback(parser, 3, depth, idx, parser->val_buf, NULL, parser->user_arg);
	}
	return parser->callback(parser, 2, depth, idx, NULL, NULL, parser->user_arg);
}

static int xmlUnescapeString(char *dst, uint32_t dst_size, const char *src, uint32_t src_len, uint32_t *out_len)
{
	if (src == NULL || src_len == 0) {
		return SCE_XML_ERROR_INVALID_ARG;
	}
	int write_dst = 1;
	if (dst == NULL) {
		if (out_len == NULL) {
			return SCE_XML_ERROR_INVALID_ARG;
		}
		write_dst = 0;
	}

	const char *end = src + src_len;
	uint32_t count = 0;

	while (src < end) {
		count++;
		if (write_dst && dst_size < count) {
			return SCE_XML_ERROR_BUFFER_TOO_SHORT;
		}
		uint8_t ch = (uint8_t)src[0];
		if (ch == '&') {
			uint8_t next_ch = (uint8_t)src[1];
			if (next_ch == 'l' && src[2] == 't' && src[3] == ';') {
				if (write_dst) {
					*dst++ = '<';
				}
				src += 4;
				continue;
			}
			if (next_ch == 'g' && src[2] == 't' && src[3] == ';') {
				if (write_dst) {
					*dst++ = '>';
				}
				src += 4;
				continue;
			}
			if (next_ch == 'a' && src[2] == 'm' && src[3] == 'p' && src[4] == ';') {
				if (write_dst) {
					*dst++ = '&';
				}
				src += 5;
				continue;
			}
			if (next_ch == 'a' && src[2] == 'p' && src[3] == 'o' && src[4] == 's' && src[5] == ';') {
				if (write_dst) {
					*dst++ = '\'';
				}
				src += 6;
				continue;
			}
			if (next_ch == 'q' && src[2] == 'u' && src[3] == 'o' && src[4] == 't' && src[5] == ';') {
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
		return SCE_XML_ERROR_BUFFER_TOO_SHORT;
	}
	if (write_dst) {
		*dst = '\0';
	}
	if (out_len != NULL) {
		*out_len = count;
	}
	return 0;
}

static int xmlDecodeCharOrEntity(char *dst, uint32_t dst_size, const char *src, uint32_t src_len,
                                 uint32_t *out_written, uint32_t *out_consumed)
{
	if (src == NULL || src_len == 0) {
		return SCE_XML_ERROR_INVALID_ARG;
	}
	int write_dst = 1;
	if (dst == NULL) {
		if (out_written == NULL) {
			return SCE_XML_ERROR_INVALID_ARG;
		}
		write_dst = 0;
	}

	uint32_t written = 0;
	uint32_t consumed = 0;
	int ret = 0;
	int need_more = 0;
	uint8_t lead = (uint8_t)src[0];

	if ((int8_t)lead >= 0) {
		written = 1;
		if (lead == '&') {
			if (src_len < 4) {
				consumed = 4;
				ret = SCE_XML_ERROR_BUFFER_TOO_SHORT;
				need_more = 1;
			} else {
				uint8_t next_ch = (uint8_t)src[1];
				if ((next_ch == 'l' && src[2] == 't' && src[3] == ';') ||
				    (next_ch == 'g' && src[2] == 't' && src[3] == ';')) {
					consumed = 4;
				} else if (src_len < 5) {
					consumed = 5;
					ret = SCE_XML_ERROR_BUFFER_TOO_SHORT;
					need_more = 1;
				} else if (next_ch == 'a' && src[2] == 'm' && src[3] == 'p' && src[4] == ';') {
					consumed = 5;
				} else if (src_len < 6) {
					consumed = 6;
					ret = SCE_XML_ERROR_BUFFER_TOO_SHORT;
					need_more = 1;
				} else if ((next_ch == 'a' && src[2] == 'p' && src[3] == 'o' && src[4] == 's' && src[5] == ';') ||
				           (next_ch == 'q' && src[2] == 'u' && src[3] == 'o' && src[4] == 't' && src[5] == ';')) {
					consumed = 6;
				} else {
					consumed = 1;
				}
			}
		} else {
			consumed = 1;
		}
	} else if ((lead & 0xe0) == 0xc0 && lead >= 0xc2) {
		if (src_len < 2) {
			consumed = 2;
			ret = SCE_XML_ERROR_BUFFER_TOO_SHORT;
			need_more = 1;
		} else if (((uint8_t)src[1] & 0xc0) == 0x80) {
			written = 2;
			consumed = 2;
		} else {
			written = 1;
			consumed = 1;
		}
	} else if ((lead & 0xf0) == 0xe0) {
		if (src_len < 3) {
			consumed = 3;
			ret = SCE_XML_ERROR_BUFFER_TOO_SHORT;
			need_more = 1;
		} else if (((uint8_t)src[1] & 0xc0) == 0x80 && ((uint8_t)src[2] & 0xc0) == 0x80) {
			written = 3;
			consumed = 3;
		} else {
			written = 1;
			consumed = 1;
		}
	} else if ((lead & 0xf8) == 0xf0) {
		if (src_len < 4) {
			consumed = 4;
			ret = SCE_XML_ERROR_BUFFER_TOO_SHORT;
			need_more = 1;
		} else if (((uint8_t)src[1] & 0xc0) == 0x80 &&
		           ((uint8_t)src[2] & 0xc0) == 0x80 &&
		           ((uint8_t)src[3] & 0xc0) == 0x80) {
			written = 4;
			consumed = 4;
		}
	} else {
		written = 1;
		consumed = 1;
	}

	if (!need_more) {
		if (write_dst && dst_size < written) {
			ret = SCE_XML_ERROR_BUFFER_TOO_SHORT;
		} else {
			char unescaped[8];
			ret = xmlUnescapeString(unescaped, 7, src, consumed, NULL);
			if (ret < 0) {
				return ret;
			}
			if (write_dst) {
				sceKernelMemcpy(dst, unescaped, written);
			}
		}
	}

	if (out_written != NULL) {
		*out_written = written;
	}
	if (out_consumed != NULL) {
		*out_consumed = consumed;
	}
	return ret;
}
