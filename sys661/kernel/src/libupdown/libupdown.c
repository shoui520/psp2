#include <stdint.h>
#include <stddef.h>

extern void *sceKernelMemset(void *s, int c, size_t n);
extern void *sceKernelMemcpy(void *dest, const void *src, size_t n);
extern uint32_t sceKernelGetSystemTimeLow(void);
extern int sceKernelCreateVpl(const char *name, int part, int attr, uint32_t size, void *opt);
extern int sceKernelDeleteVpl(int uid);
extern int sceKernelTryAllocateVpl(int uid, uint32_t size, void **data);
extern int sceKernelFreeVpl(int uid, void *data);

extern int sceHttpGetContentLength(int req, uint64_t *len);
extern int sceHttpDeleteHeader(int req, const char *name);
extern int sceHttpSetRecvTimeOut(int id, uint32_t timeout);
extern int sceHttpAddExtraHeader(int id, const char *name, const char *value, int mode);
extern int sceHttpSetResolveTimeOut(int id, uint32_t timeout);
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
extern int sceHttpGetNetworkErrno(int req, int *err);
extern int sceHttpGetAllHeader(int req, char **header, uint32_t *headerSize);
extern int sceHttpReadData(int req, void *data, uint32_t size);
extern int sceHttpDeleteTemplate(int tmpl);

extern int sceNetApctlGetInfo(int code, void *pInfo);
extern int sceNetApctlDelHandler(int hid);
extern int sceNetApctlAddHandler(void *handler, void *arg);

extern char *sceNet_lib_5705F6F9(char *dest, const char *src); /* strcat */
extern int sceNet_lib_750F705D(int c);                         /* ctype */
extern char *sceNet_lib_80C9F02A(char *dest, const char *src); /* strcpy */
extern int sceNet_lib_96EF9DA1(int c);                         /* tolower */
extern int sceNet_lib_9CFBC7E3(const char *s1, const char *s2);/* strcmp */
extern char *sceNet_lib_BCBE14CF(const char *s, int c);        /* strchr */
extern size_t sceNet_lib_D8722983(const char *s);              /* strlen */

extern int sceParseHttp_AD7BFDEF(const char *hdr, uint32_t hdrLen, const char *fieldName, char **val, uint32_t *valLen);

typedef struct MtState {
	uint32_t index;
	uint32_t state[624];
} MtState;

typedef struct KvNode {
	char *key;
	char *val;
	struct KvNode *next;
} KvNode;

typedef struct UriNode {
	char *url;
	uint32_t timeout;
	struct UriNode *next;
} UriNode;

typedef struct ParsedLine {
	uint32_t image_version; /* 0x00 */
	uint32_t uri_count;     /* 0x04 */
	uint32_t retry;         /* 0x08 */
	uint32_t timeout;       /* 0x0c */
	uint32_t cdn_timeout;   /* 0x10 */
	char *uris[10];         /* 0x14..0x3c */
	char *cdn;              /* 0x3c */
} ParsedLine;                   /* size 0x40 */

typedef struct Allocator {
	void *(*alloc_fn)(uint32_t size);
	void (*free_fn)(void *ptr);
	int vpl_uid;
	int use_custom;
} Allocator;

/* .data (0x34b0..0x34c0) */
static uint8_t g_dest_code = 1;           /* 0x34b0 */
static uint32_t g_cur_build = 2;          /* 0x34b4 */
static uint32_t g_cur_sysver = 0x01000300;/* 0x34b8 */
static int g_apctl_hid = -1;              /* 0x34bc */

/* .bss (0x34c0..0x3f60) */
static int g_ignore_compat;               /* 0x34c0 */
static int g_http_tmpl;                   /* 0x34c4 */
static uint8_t g_region_idx;              /* 0x34c8 */
static int g_aborted;                     /* 0x34cc */
static int g_list_conn;                   /* 0x34d0 */
static int g_list_req;                    /* 0x34d4 */
static int g_data_conn;                   /* 0x34d8 */
static int g_data_req;                    /* 0x34dc */
static int g_apctl_err;                   /* 0x34e0 */
static int g_apctl_suberr;                /* 0x34e4 */
static UriNode *g_uri_list;               /* 0x34e8 */
static UriNode *g_cur_uri;                /* 0x34ec */
static char *g_custom_url;                /* 0x34f0 */
static char *g_active_list_url;           /* 0x34f4 */
static char g_url_buf[0x60];              /* 0x34f8..0x3558 */
static MtState g_mt;                      /* 0x3558..0x3f1c */
static uint32_t g_perm[10];               /* 0x3f1c..0x3f44 */
static int g_initialized;                 /* 0x3f44 */
static int g_started_flag;                /* 0x3f48 */
static Allocator g_alloc;                 /* 0x3f4c..0x3f5c */
static int g_alloc_count;                 /* 0x3f5c */

static uint32_t sub_0000(MtState *mt);
static int sub_00c0(const char *buf, uint32_t len, UriNode **out_list, int *out_err);
static void sub_0268(KvNode *head);
static void sub_02e8(UriNode *head);
static uint32_t sub_034c(const char *str, uint32_t base);
static int sub_0458(const char *line, uint32_t len, UriNode **out_list, uint32_t *compat_flag);
static int sub_0500(const char *line, uint32_t len, uint8_t dest_code);
static uint32_t sub_0698(const char *buf, uint32_t len);
static uint32_t sub_0710(const char *buf, uint32_t len);
static int sub_0768(uint8_t code);
static int sub_077c(uint8_t *out_code);
static int sub_0790(uint32_t build);
static int sub_07a0(int ignore);
static int sub_07b0(const char *ver_str);
static int sub_07d4(const char *line, uint32_t len, KvNode **out_head);
static int sub_0a14(KvNode *head, ParsedLine *parsed, uint32_t *compat_flag);
static int sub_0dc8(UriNode **out_list, const ParsedLine *parsed);
static int sub_0fb4(const char *ver_str, uint32_t *out_ver);
static int sub_10fc(const char *range_str, uint32_t *out_min, uint32_t *out_max);
static int sub_1228(const char *range_str, uint32_t *out_min, uint32_t *out_max);
static void sub_136c(void);
static int sub_13c0(const char *url);
static void sub_1444(void);
static int sub_1574(int *out_err);
static int sub_1850(uint32_t *out_size, int *out_err);
static int sub_1b84(void *buf, uint32_t size, int *out_err);
static int sub_1e20(void);
static int sub_1ef0(void);
static void sub_1f30(void);
static int sub_1f7c(uint8_t region, int mode);
static void sub_1f98(void);
static int sub_21f0(uint64_t *out_len, int *out_err);
static int sub_23dc(uint64_t *out_len);
static void sub_254c(int old_state, int new_state, int event, int error, void *arg);
static int sub_25d8(int req, uint64_t *out_len);
static uint32_t *sub_272c(uint32_t count);
static void sub_2848(uint32_t seed);
static void *sub_2880(uint32_t size);
static void sub_2904(void *ptr);
static uint32_t sub_2cc4(void);
static int sub_2dd8(MtState *mt, uint32_t seed);

static uint32_t sub_0000(MtState *mt)
{
	uint32_t idx = mt->index;
	uint32_t *st = &mt->state[idx];
	uint32_t y;
	uint32_t next_val;

	if (idx < 623) {
		y = st[0];
		next_val = st[1];
		mt->index = idx + 1;
	} else {
		y = st[0];
		next_val = mt->state[0];
		mt->index = 0;
	}

	uint32_t v0 = y ^ (y >> 11);
	v0 ^= (v0 << 7) & 0x9d2c5680U;
	v0 ^= (v0 << 15) & 0xefc60000U;
	v0 ^= (v0 >> 18);

	uint32_t twist = ((y & 0x80000000U) | (next_val & 0x7fffffffU)) >> 1;
	if (next_val & 1) {
		twist ^= 0x9908b0dfU;
	}
	uint32_t mate = (idx < 227) ? st[397] : st[-227];
	st[0] = twist ^ mate;
	return v0;
}

static int sub_00c0(const char *buf, uint32_t len, UriNode **out_list, int *out_err)
{
	uint32_t compat_flag = 0;
	uint32_t matched_dest = 0;
	uint32_t found = 0;
	uint32_t off = 0;
	int res = 0;

	*out_list = NULL;
	while (off < len && !found) {
		off += sub_0698(buf + off, len - off);
		const char *line = buf + off;
		uint32_t rem = len - off;
		int match = sub_0500(line, rem, g_dest_code);
		if (match == 1) {
			matched_dest = 1;
			res = sub_0458(line, rem, out_list, &compat_flag);
			if (res == 0) {
				found = 1;
			} else if (res == (int)0x805f0004) {
				return res;
			}
			if (compat_flag == 1 && g_ignore_compat == 0) {
				return 0x805f0004;
			}
		}
		off += sub_0710(line, rem);
	}

	if (matched_dest != 1) {
		*out_err = 0x20000006;
		return 0x805f0007;
	}
	if (found != matched_dest) {
		*out_err = 0x20000005;
		return 0x805f0007;
	}
	return res;
}

static void sub_0268(KvNode *head)
{
	while (head != NULL) {
		if (head->key != NULL) {
			sub_2904(head->key);
			head->key = NULL;
		}
		if (head->val != NULL) {
			sub_2904(head->val);
			head->val = NULL;
		}
		KvNode *next = head->next;
		head->next = NULL;
		sub_2904(head);
		head = next;
	}
}

static void sub_02e8(UriNode *head)
{
	while (head != NULL) {
		if (head->url != NULL) {
			sub_2904(head->url);
			head->url = NULL;
		}
		UriNode *next = head->next;
		head->next = NULL;
		sub_2904(head);
		head = next;
	}
}

static uint32_t sub_034c(const char *str, uint32_t base)
{
	if (str == NULL) {
		return 0;
	}

	uint32_t acc = 0;
	if (base - 2 >= 35) {
		base = 10;
	}

	if (str[0] == '0') {
		str++;
		if (str[0] == 'X' || str[0] == 'x') {
			str++;
			base = 16;
		}
	}

	while (1) {
		int c = (int8_t)*str;
		uint32_t digit;
		if (sceNet_lib_750F705D(c) & 4) {
			digit = c - '0';
		} else if (sceNet_lib_750F705D(c) & 3) {
			digit = sceNet_lib_96EF9DA1(c) - 0x57;
		} else {
			break;
		}
		if (digit >= base) {
			break;
		}
		acc = acc * base + digit;
		str++;
	}

	return acc;
}

static int sub_0458(const char *line, uint32_t len, UriNode **out_list, uint32_t *compat_flag)
{
	ParsedLine parsed;
	KvNode *kv_head = NULL;

	sceKernelMemset(&parsed, 0, sizeof(parsed));
	int res = sub_07d4(line, len, &kv_head);
	if (res < 0) {
		return res;
	}

	res = sub_0a14(kv_head, &parsed, compat_flag);
	if (res >= 0) {
		res = sub_0dc8(out_list, &parsed);
	}

	sub_0268(kv_head);
	return res;
}

static int sub_0500(const char *line, uint32_t len, uint8_t dest_code)
{
	size_t tmpl_len = sceNet_lib_D8722983("Dest=XX;");
	if (len < tmpl_len) {
		return 0;
	}
	if (line[0] == '#') {
		return 0;
	}

	char hex[4];
	uint8_t hi = dest_code >> 4;
	hex[0] = (hi < 10) ? (hi + '0') : (hi + 'a' - 10);
	uint8_t lo = dest_code & 0x0f;
	hex[1] = (lo < 10) ? (lo + '0') : (lo + 'a' - 10);
	hex[2] = '\0';

	char expected[16];
	sceNet_lib_80C9F02A(expected, "Dest=");
	sceNet_lib_5705F6F9(expected, hex);
	sceNet_lib_5705F6F9(expected, ";");

	char prefix[16];
	sceKernelMemcpy(prefix, line, tmpl_len);
	prefix[tmpl_len] = '\0';

	return (sceNet_lib_9CFBC7E3(prefix, expected) == 0) ? 1 : 0;
}

static uint32_t sub_0698(const char *buf, uint32_t len)
{
	uint32_t off = 0;
	while (off < len && buf[off] == '#') {
		off += sub_0710(buf + off, len - off);
	}
	return off;
}

static uint32_t sub_0710(const char *buf, uint32_t len)
{
	uint32_t off = 0;
	while (off < len && buf[off] != '\n') {
		off++;
	}
	if (off != len) {
		off++;
	}
	return off;
}

static int sub_0768(uint8_t code)
{
	g_dest_code = code;
	return 0;
}

static int sub_077c(uint8_t *out_code)
{
	*out_code = g_dest_code;
	return 0;
}

static int sub_0790(uint32_t build)
{
	g_cur_build = build;
	return 0;
}

static int sub_07a0(int ignore)
{
	g_ignore_compat = ignore;
	return 0;
}

static int sub_07b0(const char *ver_str)
{
	sub_0fb4(ver_str, &g_cur_sysver);
	return 0;
}

static int sub_07d4(const char *line, uint32_t len, KvNode **out_head)
{
	KvNode *head = NULL;
	KvNode *prev = NULL;
	uint32_t pos = 0;
	int first = 1;

	while (pos < len && line[pos] != '\r' && line[pos] != '\n') {
		KvNode *node = (KvNode *)sub_2880(sizeof(KvNode));
		if (node == NULL) {
			if (head != NULL) {
				sub_0268(head);
			}
			return 0x805f0001;
		}
		sceKernelMemset(node, 0, sizeof(KvNode));
		if (first) {
			head = node;
			first = 0;
		} else {
			prev->next = node;
		}

		uint32_t klen = 0;
		while (line[pos + klen] != '=') {
			if (line[pos + klen] == '\n') {
				sub_0268(head);
				return -1;
			}
			klen++;
		}

		node->key = (char *)sub_2880(klen + 1);
		if (node->key == NULL) {
			sub_0268(head);
			return 0x805f0001;
		}
		sceKernelMemcpy(node->key, line + pos, klen);
		node->key[klen] = '\0';
		pos += klen + 1;

		uint32_t vlen = 0;
		while (line[pos + vlen] != '\n' && line[pos + vlen] != ';') {
			vlen++;
		}

		node->val = (char *)sub_2880(vlen + 1);
		if (node->val == NULL) {
			sub_0268(head);
			return 0x805f0001;
		}
		sceKernelMemcpy(node->val, line + pos, vlen);
		node->val[vlen] = '\0';

		uint32_t next_pos = pos + vlen;
		if (line[next_pos] == ';') {
			next_pos = pos + 1 + vlen;
		}
		prev = node;
		pos = next_pos;
	}

	*out_head = head;
	return 0;
}

static int sub_0a14(KvNode *head, ParsedLine *parsed, uint32_t *compat_flag)
{
	uint32_t min_ver = 0;
	uint32_t max_ver = 0;
	int has_dest = 0;
	int has_imgver = 0;
	uint32_t uri_cnt = 0;
	int has_cdn = 0;
	int has_retry = 0;
	int has_timeout = 0;
	int has_cdn_timeout = 0;
	int has_compat_sysver = 0;
	int has_compat_ver = 0;

	for (KvNode *cur = head; cur != NULL; cur = cur->next) {
		if (sceNet_lib_9CFBC7E3(cur->key, "Dest") == 0) {
			if (has_dest == 1) {
				return -1;
			}
			has_dest = 1;
		} else if (sceNet_lib_9CFBC7E3(cur->key, "ImageVersion") == 0) {
			if (has_imgver == 1) {
				return -1;
			}
			has_imgver = 1;
			parsed->image_version = sub_034c(cur->val, 16);
		} else if (sceNet_lib_9CFBC7E3(cur->key, "URI") == 0) {
			if (uri_cnt >= 10) {
				return -1;
			}
			parsed->uris[uri_cnt++] = cur->val;
		} else if (sceNet_lib_9CFBC7E3(cur->key, "CDN") == 0) {
			if (has_cdn == 1) {
				return -1;
			}
			has_cdn = 1;
			parsed->cdn = cur->val;
		} else if (sceNet_lib_9CFBC7E3(cur->key, "Retry") == 0) {
			if (has_retry == 1) {
				return -1;
			}
			has_retry = 1;
			parsed->retry = sub_034c(cur->val, 10);
		} else if (sceNet_lib_9CFBC7E3(cur->key, "Timeout") == 0) {
			if (has_timeout == 1) {
				return -1;
			}
			has_timeout = 1;
			parsed->timeout = sub_034c(cur->val, 10);
		} else if (sceNet_lib_9CFBC7E3(cur->key, "CDN_Timeout") == 0) {
			if (has_cdn_timeout == 1) {
				return -1;
			}
			has_cdn_timeout = 1;
			parsed->cdn_timeout = sub_034c(cur->val, 10);
		} else if (sceNet_lib_9CFBC7E3(cur->key, "CompatibleVersion") == 0) {
			if (has_compat_sysver || has_compat_ver) {
				return -1;
			}
			int r = sub_10fc(cur->val, &min_ver, &max_ver);
			has_compat_ver = 1;
			if (r < 0) {
				return r;
			}
		} else if (sceNet_lib_9CFBC7E3(cur->key, "CompatibleSystemSoftwareVersion") == 0) {
			if (has_compat_sysver || has_compat_ver) {
				return -1;
			}
			int r = sub_1228(cur->val, &min_ver, &max_ver);
			has_compat_sysver = 1;
			if (r < 0) {
				return r;
			}
		} else {
			return -1;
		}
	}

	if (has_compat_ver == 1) {
		if (min_ver <= g_cur_build && g_cur_build <= max_ver) {
			*compat_flag = has_compat_ver;
		}
	}
	if (has_compat_sysver == 1) {
		if (min_ver <= g_cur_sysver && g_cur_sysver <= max_ver) {
			*compat_flag = has_compat_sysver;
		}
	}

	if (!has_dest || !has_imgver) {
		return -1;
	}
	if ((int)uri_cnt < (int)parsed->retry) {
		return -1;
	}
	if (uri_cnt == 0 && !has_cdn) {
		return -1;
	}

	int res = 0;
	if (g_cur_build >= parsed->image_version) {
		res = 0x805f0004;
	}
	parsed->uri_count = uri_cnt;
	return res;
}

static int sub_0dc8(UriNode **out_list, const ParsedLine *parsed)
{
	uint32_t *perm = sub_272c(parsed->uri_count);
	if (perm == NULL) {
		return 0x805f0008;
	}

	UriNode *head = NULL;
	UriNode *prev = NULL;
	UriNode *cur = NULL;
	int first = 1;

	for (uint32_t i = 0; i < parsed->retry; i++) {
		cur = (UriNode *)sub_2880(sizeof(UriNode));
		if (cur == NULL) {
			if (head != NULL) {
				sub_02e8(head);
			}
			return 0x805f0001;
		}
		sceKernelMemset(cur, 0, sizeof(UriNode));
		if (first) {
			head = cur;
			first = 0;
		} else {
			prev->next = cur;
		}
		prev = cur;

		const char *src_url = parsed->uris[perm[i]];
		size_t ulen = sceNet_lib_D8722983(src_url);
		cur->url = (char *)sub_2880(ulen + 1);
		if (cur->url == NULL) {
			sub_02e8(head);
			return 0x805f0001;
		}
		sceNet_lib_80C9F02A(cur->url, src_url);
		cur->timeout = parsed->timeout;
	}

	if (parsed->cdn != NULL) {
		cur = (UriNode *)sub_2880(sizeof(UriNode));
		if (cur == NULL) {
			if (head != NULL) {
				sub_02e8(head);
			}
			return 0x805f0001;
		}
		sceKernelMemset(cur, 0, sizeof(UriNode));
		if (first) {
			head = cur;
		} else {
			prev->next = cur;
		}
		size_t ulen = sceNet_lib_D8722983(parsed->cdn);
		cur->url = (char *)sub_2880(ulen + 1);
		if (cur->url == NULL) {
			sub_02e8(head);
			return 0x805f0001;
		}
		sceNet_lib_80C9F02A(cur->url, parsed->cdn);
		cur->timeout = parsed->cdn_timeout;
	}

	cur->next = NULL;
	*out_list = head;
	return 0;
}

static int sub_0fb4(const char *ver_str, uint32_t *out_ver)
{
	if (ver_str == NULL || out_ver == NULL) {
		return -1;
	}

	char *dot = sceNet_lib_BCBE14CF(ver_str, '.');
	uint32_t maj_len;
	uint32_t min_len;
	if (dot != NULL) {
		min_len = sceNet_lib_D8722983(dot) - 1;
		maj_len = (uint32_t)(dot - ver_str);
	} else {
		maj_len = sceNet_lib_D8722983(ver_str);
		min_len = 0;
	}

	if (maj_len >= 3) {
		maj_len = 2;
	}
	if (min_len >= 3) {
		min_len = 2;
	}

	char buf[16];
	sceKernelMemcpy(buf, ver_str, maj_len);
	buf[maj_len] = '\0';
	uint32_t maj = sub_034c(buf, 10);

	uint32_t min_val = 0;
	if (dot != NULL) {
		sceKernelMemcpy(buf, dot + 1, min_len);
		buf[min_len] = '\0';
		for (uint32_t i = 0; i < 2; i++) {
			if (i < min_len) {
				min_val += (uint8_t)(buf[i] - '0');
			}
			min_val <<= 8;
		}
	}

	if (maj >= 100 || min_val > 0x090900U) {
		return -1;
	}

	*out_ver = (maj << 24) + min_val;
	return 0;
}

static int sub_10fc(const char *range_str, uint32_t *out_min, uint32_t *out_max)
{
	if (range_str == NULL || out_min == NULL || out_max == NULL) {
		return -1;
	}
	if (sceNet_lib_D8722983(range_str) >= 18) {
		return -1;
	}

	char *dash = sceNet_lib_BCBE14CF(range_str, '-');
	if (dash == NULL) {
		return -1;
	}

	uint32_t left_len = (uint32_t)(dash - range_str);
	uint32_t right_len = sceNet_lib_D8722983(dash) - 1;
	if (left_len >= 9 || right_len >= 9) {
		return -1;
	}

	char buf[16];
	if (left_len != 0) {
		sceKernelMemcpy(buf, range_str, left_len);
		buf[left_len] = '\0';
		*out_max = sub_034c(buf, 16);
	} else {
		*out_max = 0;
	}

	if (right_len != 0) {
		sceKernelMemcpy(buf, dash + 1, right_len);
		buf[right_len] = '\0';
		*out_min = sub_034c(buf, 16);
	} else {
		*out_min = 0xffffffffU;
	}

	return 0;
}

static int sub_1228(const char *range_str, uint32_t *out_min, uint32_t *out_max)
{
	if (range_str == NULL || out_min == NULL || out_max == NULL) {
		return -1;
	}
	if (sceNet_lib_D8722983(range_str) >= 16) {
		return -1;
	}

	char *dash = sceNet_lib_BCBE14CF(range_str, '-');
	if (dash == NULL) {
		return -1;
	}

	uint32_t left_len = (uint32_t)(dash - range_str);
	uint32_t right_len = sceNet_lib_D8722983(dash) - 1;
	if (left_len >= 8 || right_len >= 8) {
		return -1;
	}

	char buf[16];
	if (left_len != 0) {
		sceKernelMemcpy(buf, range_str, left_len);
		buf[left_len] = '\0';
		int r = sub_0fb4(buf, out_max);
		if (r < 0) {
			return r;
		}
	} else {
		*out_max = 0;
	}

	if (right_len != 0) {
		sceKernelMemcpy(buf, dash + 1, right_len);
		buf[right_len] = '\0';
		int r = sub_0fb4(buf, out_min);
		if (r < 0) {
			return r;
		}
	} else {
		*out_min = 0xffffffffU;
	}

	return 0;
}

static void sub_136c(void)
{
	if (g_apctl_hid >= 0) {
		sceNetApctlDelHandler(g_apctl_hid);
		g_apctl_hid = -1;
	}
	int hid = sceNetApctlAddHandler((void *)sub_254c, NULL);
	if (hid >= 0) {
		g_apctl_hid = hid;
	}
}

static int sub_13c0(const char *url)
{
	if (g_custom_url != NULL) {
		sub_2904(g_custom_url);
		g_custom_url = NULL;
	}
	size_t len = sceNet_lib_D8722983(url);
	g_custom_url = (char *)sub_2880(len + 1);
	if (g_custom_url == NULL) {
		return 0x805f0001;
	}
	sceNet_lib_80C9F02A(g_custom_url, url);
	return 0;
}

static void sub_1444(void)
{
	if (g_data_req != 0) {
		sceHttpAbortRequest(g_data_req);
		sceHttpDeleteRequest(g_data_req);
		g_data_req = 0;
	}
	if (g_data_conn != 0) {
		sceHttpDeleteConnection(g_data_conn);
		g_data_conn = 0;
	}
	if (g_list_req != 0) {
		sceHttpAbortRequest(g_list_req);
		sceHttpDeleteRequest(g_list_req);
		g_list_req = 0;
	}
	if (g_list_conn != 0) {
		sceHttpDeleteConnection(g_list_conn);
		g_list_conn = 0;
	}
	if (g_http_tmpl != 0) {
		sceHttpDeleteTemplate(g_http_tmpl);
		g_http_tmpl = 0;
	}
	g_aborted = 0;
	if (g_uri_list != NULL) {
		sub_02e8(g_uri_list);
		g_uri_list = NULL;
	}
	if (g_custom_url != NULL) {
		sub_2904(g_custom_url);
		g_custom_url = NULL;
	}
}

static int sub_1574(int *out_err)
{
	uint64_t content_len = 0;
	int res = 0;

	g_aborted = 0;
	if (g_http_tmpl == 0) {
		res = sceHttpCreateTemplate("PSPUpdate-agent/1.0.0 libhttp/1.0.0", 1, 1);
		if (res < 0) {
			*out_err = res;
			res = 0x805f0007;
			goto finish;
		}
		g_http_tmpl = res;
	}

	sceHttpSetResolveTimeOut(g_http_tmpl, 3000000);
	sceHttpSetRecvTimeOut(g_http_tmpl, 60000000);
	sceHttpSetSendTimeOut(g_http_tmpl, 60000000);
	sub_1f98();

	if (g_active_list_url == NULL) {
		res = 0x805f000c;
		goto finish;
	}

	if (g_list_conn == 0) {
		res = sceHttpCreateConnectionWithURL(g_http_tmpl, g_active_list_url, 0);
		if (res == (int)0x80431022) {
			res = 0x805f0001;
			goto finish;
		}
		if (res < 0) {
			*out_err = res;
			res = 0x805f0007;
			goto finish;
		}
		g_list_conn = res;
	}

	if (g_list_req != 0) {
		sceHttpAbortRequest(g_list_req);
		sceHttpDeleteRequest(g_list_req);
		g_list_req = 0;
	}

	res = sub_21f0(&content_len, out_err);
	if (res < 0) {
		goto finish;
	}

	if (content_len > 0xffffULL) {
		*out_err = 0x20000003;
		res = 0x805f0007;
		goto finish;
	}

	uint32_t list_size = (uint32_t)content_len;
	res = 0x805f0001;
	char *buf = (char *)sub_2880(list_size);
	if (buf != NULL) {
		sceKernelMemset(buf, 0, list_size);
		uint32_t read_off = 0;
		int eof = 0;
		while (!eof) {
			int n = sceHttpReadData(g_list_req, buf + read_off, list_size - read_off);
			if (n == 0) {
				eof = 1;
			} else if (n < 0) {
				*out_err = n;
				res = 0x805f0007;
				goto free_buf;
			}
			read_off += n;
		}
		if (read_off != list_size) {
			*out_err = 0x20000004;
		}
		sceHttpDeleteRequest(g_list_req);
		g_list_req = 0;
		sceHttpDeleteConnection(g_list_conn);
		g_list_conn = 0;
		res = sub_00c0(buf, list_size, &g_uri_list, out_err);
free_buf:
		sub_2904(buf);
	}

finish:
	if (g_apctl_err < 0) {
		res = g_apctl_err;
		*out_err = g_apctl_suberr;
		g_apctl_err = 0;
		g_apctl_suberr = 0;
		return res;
	}
	if (g_aborted == 1) {
		res = 0x805f0009;
	}
	return res;
}

static int sub_1850(uint32_t *out_size, int *out_err)
{
	int status = 0;
	uint64_t content_len = 0;
	int res = 0;

	*out_err = 0;
	if (g_aborted == 1) {
		res = 0x805f0009;
		goto finish;
	}

	UriNode *cur = g_uri_list;
	if (g_data_req != 0) {
		sceHttpDeleteRequest(g_data_req);
		g_data_req = 0;
	}
	if (g_data_conn != 0) {
		sceHttpDeleteConnection(g_data_conn);
		g_data_conn = 0;
	}

	while (cur != NULL) {
		res = sceHttpCreateConnectionWithURL(g_http_tmpl, cur->url, 0);
		if (res < 0) {
			cur = cur->next;
			continue;
		}
		g_data_conn = res;
		res = sceHttpSetConnectTimeOut(g_data_conn, cur->timeout * 1000000);
		if (res < 0) {
			sceHttpDeleteConnection(g_data_conn);
			g_data_conn = 0;
			cur = cur->next;
			continue;
		}
		res = sceHttpCreateRequestWithURL(g_data_conn, 2, cur->url, 0);
		if (res < 0) {
			sceHttpDeleteConnection(g_data_conn);
			g_data_conn = 0;
			cur = cur->next;
			continue;
		}
		g_data_req = res;
		sceHttpDeleteHeader(g_data_req, "Accept-Encoding");
		sceHttpDeleteHeader(g_data_req, "Accept");
		res = sceHttpSendRequest(g_data_req, NULL, 0);
		if (res < 0) {
			sceHttpDeleteRequest(g_data_req);
			g_data_req = 0;
			sceHttpDeleteConnection(g_data_conn);
			g_data_conn = 0;
			cur = cur->next;
			continue;
		}
		res = sceHttpGetStatusCode(g_data_req, &status);
		if (res >= 0 && status == 200) {
			res = sceHttpGetContentLength(g_data_req, &content_len);
			if (res < 0 || (content_len >> 32) != 0) {
				sceHttpDeleteRequest(g_data_req);
				g_data_req = 0;
				sceHttpDeleteConnection(g_data_conn);
				g_data_conn = 0;
				res = sub_23dc(&content_len);
				if (res < 0) {
					cur = cur->next;
					continue;
				}
			}
			g_cur_uri = cur;
			*out_size = (uint32_t)content_len;
			sceHttpDeleteRequest(g_data_req);
			g_data_req = 0;
			if (cur != NULL) {
				goto finish;
			}
			break;
		}
		sceHttpDeleteRequest(g_data_req);
		g_data_req = 0;
		sceHttpDeleteConnection(g_data_conn);
		g_data_conn = 0;
		cur = cur->next;
	}

	*out_err = res;
	if (res == (int)0x80431063 && g_data_req != 0) {
		if (sceHttpGetNetworkErrno(g_data_req, out_err) == 0) {
			*out_err |= 0x10000000;
		}
	}
	res = 0x805f0007;

finish:
	if (g_apctl_err < 0) {
		res = g_apctl_err;
		*out_err = g_apctl_suberr;
		g_apctl_err = 0;
		g_apctl_suberr = 0;
		return res;
	}
	if (g_aborted == 1) {
		res = 0x805f0009;
	}
	return res;
}

static int sub_1b84(void *buf, uint32_t size, int *out_err)
{
	int status = 0;
	int res = 0x805f0009;

	*out_err = 0;
	if (g_aborted == 1) {
		goto cleanup;
	}
	if (g_data_conn == 0) {
		res = 0x805f0003;
		goto cleanup;
	}

	if (g_data_req == 0) {
		res = sceHttpCreateRequestWithURL(g_data_conn, 0, g_cur_uri->url, 0);
		if (res == (int)0x80431022) {
			res = 0x805f0001;
			goto cleanup;
		}
		if (res < 0) {
			*out_err = res;
			res = 0x805f0007;
			goto cleanup;
		}
		g_data_req = res;
		sceHttpDeleteHeader(g_data_req, "Accept-Encoding");
		sceHttpDeleteHeader(g_data_req, "Accept");
		res = sceHttpSendRequest(g_data_req, NULL, 0);
		if (res == (int)0x80431063) {
			int r = sceHttpGetNetworkErrno(g_data_req, out_err);
			if (r < 0) {
				*out_err = r;
			} else {
				*out_err |= 0x10000000;
			}
			res = 0x805f0007;
			goto cleanup;
		}
		if (res == (int)0x80431022) {
			res = 0x805f0001;
			goto cleanup;
		}
		if (res < 0) {
			*out_err = res;
			res = 0x805f0007;
			goto cleanup;
		}
		if (sceHttpGetStatusCode(g_data_req, &status) < 0 || status != 200) {
			*out_err = 0x20000001;
			res = 0x805f0007;
			goto cleanup;
		}
	}

	res = sceHttpReadData(g_data_req, buf, size);
	if (res == 0) {
		sceHttpDeleteRequest(g_data_req);
		g_data_req = 0;
		sceHttpDeleteConnection(g_data_conn);
		g_data_conn = 0;
		return 0;
	}
	if (res == (int)0x80431063) {
		int r = sceHttpGetNetworkErrno(g_data_req, out_err);
		if (r < 0) {
			*out_err = r;
		} else {
			*out_err |= 0x10000000;
		}
		res = 0x805f0007;
		goto cleanup;
	}
	if (res == (int)0x80431022) {
		res = 0x805f0001;
		goto cleanup;
	}
	if (res >= 0) {
		return res;
	}
	*out_err = res;
	res = 0x805f0007;

cleanup:
	if (g_data_req != 0) {
		sceHttpDeleteRequest(g_data_req);
		g_data_req = 0;
	}
	if (g_data_conn != 0) {
		sceHttpDeleteConnection(g_data_conn);
		g_data_conn = 0;
	}
	if (g_apctl_err < 0) {
		res = g_apctl_err;
		*out_err = g_apctl_suberr;
		g_apctl_err = 0;
		g_apctl_suberr = 0;
		return res;
	}
	if (g_aborted == 1) {
		res = 0x805f0009;
	}
	return res;
}

static int sub_1e20(void)
{
	if (g_list_req == 0 && g_data_req == 0) {
		return 0x805f000a;
	}
	if (g_aborted == 1) {
		return 0;
	}
	if (g_list_req != 0) {
		sceHttpAbortRequest(g_list_req);
		sceHttpDeleteRequest(g_list_req);
		g_list_req = 0;
	}
	if (g_data_req != 0) {
		sceHttpAbortRequest(g_data_req);
		sceHttpDeleteRequest(g_data_req);
		g_data_req = 0;
	}
	g_aborted = 1;
	return 0;
}

static int sub_1ef0(void)
{
	uint32_t info[32];
	if (sceNetApctlGetInfo(13, info) < 0) {
		return 0;
	}
	return (info[0] == 1) ? 1 : 0;
}

static void sub_1f30(void)
{
	if (g_apctl_hid >= 0) {
		sceNetApctlDelHandler(g_apctl_hid);
	}
	g_apctl_hid = -1;
	g_apctl_err = 0;
	g_apctl_suberr = 0;
}

static int sub_1f7c(uint8_t region, int mode)
{
	g_region_idx = region;
	return (mode == 1) ? (int)0x805f000b : 0;
}

static void sub_1f98(void)
{
	if (g_custom_url != NULL) {
		g_active_list_url = g_custom_url;
		return;
	}

	const char *host_prefix;
	const char *region_dir;
	switch (g_region_idx) {
	case 1:
		host_prefix = "fu01";
		region_dir = "/us/";
		break;
	case 2:
		host_prefix = "fe01";
		region_dir = "/eu/";
		break;
	case 3:
		host_prefix = "fo01a";
		region_dir = "/oa/";
		break;
	case 4:
		host_prefix = "fo01b";
		region_dir = "/ob/";
		break;
	case 5:
		host_prefix = "fo01c";
		region_dir = "/oc/";
		break;
	case 6:
		host_prefix = "fo01d";
		region_dir = "/od/";
		break;
	case 7:
		host_prefix = "fo01e";
		region_dir = "/oe/";
		break;
	case 8:
		host_prefix = "fo01f";
		region_dir = "/of/";
		break;
	case 9:
		host_prefix = "fo01g";
		region_dir = "/og/";
		break;
	case 10:
		host_prefix = "fo01h";
		region_dir = "/oh/";
		break;
	case 11:
		host_prefix = "fo01i";
		region_dir = "/oi/";
		break;
	case 12:
		host_prefix = "fo01j";
		region_dir = "/oj/";
		break;
	case 13:
		host_prefix = "fo01k";
		region_dir = "/ok/";
		break;
	case 14:
		host_prefix = "fo01l";
		region_dir = "/ol/";
		break;
	case 15:
		host_prefix = "fo01m";
		region_dir = "/om/";
		break;
	case 0:
	default:
		host_prefix = "fj01";
		region_dir = "/jp/";
		break;
	}

	uint8_t code = 0;
	sub_077c(&code);
	sceNet_lib_80C9F02A(g_url_buf, "http://");
	sceNet_lib_5705F6F9(g_url_buf, host_prefix);
	sceNet_lib_5705F6F9(g_url_buf, ".psp.update.playstation.org");
	if (code & 0xe0) {
		sceNet_lib_5705F6F9(g_url_buf, "/update/psp/list3");
	} else if (code & 0x40) {
		sceNet_lib_5705F6F9(g_url_buf, "/update/psp/list2");
	} else {
		sceNet_lib_5705F6F9(g_url_buf, "/update");
	}
	sceNet_lib_5705F6F9(g_url_buf, region_dir);
	sceNet_lib_5705F6F9(g_url_buf, "psp-updatelist.txt");
	g_active_list_url = g_url_buf;
}

static int sub_21f0(uint64_t *out_len, int *out_err)
{
	int status = 0;
	int res = sceHttpCreateRequestWithURL(g_list_conn, 0, g_active_list_url, 0);
	if (res == (int)0x80431022) {
		res = 0x805f0001;
		goto cleanup;
	}
	if (res < 0) {
		*out_err = res;
		res = 0x805f0007;
		goto cleanup;
	}

	g_list_req = res;
	sceHttpDeleteHeader(g_list_req, "Accept-Encoding");
	sceHttpDeleteHeader(g_list_req, "Accept");
	sceHttpSetConnectTimeOut(g_list_conn, 30000000);
	res = sceHttpSendRequest(g_list_req, NULL, 0);
	if ((((uint32_t)res >> 16) & 0xfff) == 0x41 && (((uint32_t)res >> 20) & 0xff) == 4) {
		*out_err = res;
		res = 0x805f0005;
		goto cleanup;
	}
	if (res == (int)0x80431022) {
		res = 0x805f0001;
		goto cleanup;
	}
	if (res == (int)0x8043106d) {
		res = 0x805f0009;
		goto cleanup;
	}
	if (res < 0) {
		*out_err = res;
		res = 0x805f0007;
		goto cleanup;
	}

	res = sceHttpGetStatusCode(g_list_req, &status);
	if (res < 0) {
		*out_err = res;
		res = 0x805f0007;
		goto cleanup;
	}
	if (status != 200) {
		*out_err = 0x20000001;
		res = (status == 404) ? (int)0x805f0008 : (int)0x805f0007;
		goto cleanup;
	}

	res = sceHttpGetContentLength(g_list_req, out_len);
	if (res >= 0) {
		return res;
	}
	*out_err = 0x20000002;
	res = 0x805f0007;

cleanup:
	if (g_list_req != 0) {
		sceHttpDeleteRequest(g_list_req);
		g_list_req = 0;
	}
	return res;
}

static int sub_23dc(uint64_t *out_len)
{
	int status = 0;
	int res = sceHttpCreateConnectionWithURL(g_http_tmpl, g_uri_list->url, 0);
	if (res < 0) {
		return -1;
	}
	g_data_conn = res;
	if (sceHttpSetConnectTimeOut(g_data_conn, g_uri_list->timeout * 1000000) < 0) {
		sceHttpDeleteConnection(g_data_conn);
		g_data_conn = 0;
		return -1;
	}
	res = sceHttpCreateRequestWithURL(g_data_conn, 2, g_uri_list->url, 0);
	if (res < 0) {
		sceHttpDeleteConnection(g_data_conn);
		g_data_conn = 0;
		return -1;
	}
	g_data_req = res;
	sceHttpDeleteHeader(g_data_req, "Accept-Encoding");
	sceHttpDeleteHeader(g_data_req, "Accept");
	sceHttpAddExtraHeader(g_data_req, "Range", "bytes=0-1", 0);
	if (sceHttpSendRequest(g_data_req, NULL, 0) >= 0 &&
	    sceHttpGetStatusCode(g_data_req, &status) >= 0 &&
	    status == 206 &&
	    sub_25d8(g_data_req, out_len) >= 0 &&
	    (*out_len >> 32) == 0) {
		return 0;
	}

	sceHttpDeleteRequest(g_data_req);
	sceHttpDeleteConnection(g_data_conn);
	g_data_req = 0;
	g_data_conn = 0;
	return -1;
}

static void sub_254c(int old_state, int new_state, int event, int error, void *arg)
{
	(void)new_state;
	(void)arg;
	if (old_state == 4 && event == 6) {
		if (error == (int)0x80410a07) {
			g_apctl_err = 0x805f000e;
		} else if (error == (int)0x80410a06) {
			g_apctl_err = 0x805f000f;
		} else {
			g_apctl_err = 0x805f0007;
			g_apctl_suberr = error;
		}
		sub_1e20();
	}
}

static int sub_25d8(int req, uint64_t *out_len)
{
	char *hdr = NULL;
	uint32_t hdr_len = 0;
	char *val = NULL;
	uint32_t val_len = 0;

	int res = sceHttpGetAllHeader(req, &hdr, &hdr_len);
	if (res < 0) {
		return res;
	}

	sceParseHttp_AD7BFDEF(hdr, hdr_len, "Content-Range", &val, &val_len);
	if (val_len + 1 >= 0x41) {
		return -1;
	}

	char buf[0x40];
	sceKernelMemcpy(buf, val, val_len);
	buf[val_len] = '\0';

	char *slash = sceNet_lib_BCBE14CF(buf, '/');
	if (slash == NULL) {
		return -1;
	}

	char *num_str = slash + 1;
	if (num_str[0] == '*') {
		return -1;
	}

	sceNet_lib_D8722983(num_str);
	uint64_t acc = 0;
	if (num_str != NULL && num_str[0] != '\0') {
		for (const char *p = num_str; *p != '\0'; p++) {
			acc = acc * 10 + (int64_t)((int8_t)*p - '0');
		}
	}

	*out_len = acc;
	return (*out_len != 0) ? 0 : -1;
}

static uint32_t *sub_272c(uint32_t count)
{
	if (count >= 11) {
		return NULL;
	}
	for (uint32_t i = 0; i < count; i++) {
		g_perm[i] = i;
	}
	uint32_t rem = count;
	for (uint32_t i = 1; i < count; i++) {
		sub_0000(&g_mt);
		uint32_t divisor = (0xffffffffU / rem) + 1;
		uint32_t rnd = g_mt.state[g_mt.index];
		uint32_t pick = rnd / divisor;
		uint32_t tmp = g_perm[rem - 1];
		g_perm[rem - 1] = g_perm[pick];
		g_perm[pick] = tmp;
		rem--;
	}
	return g_perm;
}

static void sub_2848(uint32_t seed)
{
	sub_2dd8(&g_mt, seed);
	sceKernelMemset(g_perm, 0, sizeof(g_perm));
}

static void *sub_2880(uint32_t size)
{
	void *ptr = NULL;
	if (g_initialized == 1) {
		if (g_alloc.use_custom == 1) {
			ptr = g_alloc.alloc_fn(size);
		} else {
			if (sceKernelTryAllocateVpl(g_alloc.vpl_uid, size, &ptr) < 0) {
				ptr = NULL;
			}
		}
	}
	g_alloc_count++;
	return ptr;
}

static void sub_2904(void *ptr)
{
	if (g_initialized == 1) {
		if (g_alloc.use_custom == 1) {
			g_alloc.free_fn(ptr);
		} else {
			sceKernelFreeVpl(g_alloc.vpl_uid, ptr);
		}
		g_alloc_count--;
	}
}

int sceLibUpdateDL_F6690A9A(void *(*alloc_fn)(uint32_t), void (*free_fn)(void *))
{
	if (g_initialized == 1) {
		return 0x805f000d;
	}
	if (alloc_fn == NULL || free_fn == NULL) {
		return 0x805f000b;
	}
	g_alloc.alloc_fn = alloc_fn;
	g_alloc.free_fn = free_fn;
	g_alloc.use_custom = 1;
	sub_2848(sub_2cc4());
	sub_136c();
	g_initialized = 1;
	return 0;
}

int sceLibUpdateDL_FC1AB540(void)
{
	if (g_initialized == 1) {
		return 0x805f000d;
	}
	int uid = sceKernelCreateVpl("libupdown pool", 2, 0, 100000, NULL);
	if (uid == (int)0x80020190) {
		return 0x805f0001;
	}
	if (uid < 0) {
		return uid;
	}
	g_alloc.vpl_uid = uid;
	sub_2848(sub_2cc4());
	sub_136c();
	sub_07a0(0);
	g_initialized = 1;
	return 0;
}

int sceLibUpdateDL_D6A09757(void)
{
	if (g_initialized != 1) {
		return 0x805f0002;
	}
	sub_1444();
	if (g_alloc.use_custom == 1) {
		g_alloc.use_custom = 0;
	} else {
		sceKernelDeleteVpl(g_alloc.vpl_uid);
		g_alloc.vpl_uid = 0;
	}
	sub_1f30();
	g_initialized = 0;
	return 0;
}

int sceLibUpdateDL_FD675E8D(uint32_t *out_size, int *out_err)
{
	int res = 0x805f0002;
	if (g_initialized == 1) {
		if (out_err == NULL) {
			res = 0x805f000b;
		} else {
			res = sub_1574(out_err);
			if (res >= 0) {
				if (out_size != NULL) {
					res = sub_1850(out_size, out_err);
				}
				if (res >= 0) {
					g_started_flag = 1;
					return 0;
				}
			}
		}
	}

	if ((uint32_t)(res - 0x805f0007U) < 2) {
		if (sub_1ef0() != 1) {
			res = 0x805f0006;
		}
	}
	return res;
}

int sceLibUpdateDL_FA9AA797(void *buf, uint32_t size, int *out_err)
{
	int res = 0x805f0002;
	if (g_initialized == 1) {
		res = sub_1b84(buf, size, out_err);
	}
	if ((uint32_t)(res - 0x805f0007U) < 2) {
		if (sub_1ef0() != 1) {
			res = 0x805f0006;
		}
	}
	if (g_started_flag == 1 && res == (int)0x805f000f) {
		res = 0x805f000e;
	}
	g_started_flag = 0;
	return res;
}

static uint32_t sub_2cc4(void)
{
	return sceKernelGetSystemTimeLow();
}

int sceLibUpdateDL_88FF3935(uint8_t code)
{
	sub_0768(code);
	return 0;
}

int sceLibUpdateDL_B5DB018D(uint8_t region, int mode)
{
	return sub_1f7c(region, mode);
}

int sceLibUpdateDL_F7E66CB4(const char *url)
{
	return sub_13c0(url);
}

int sceLibUpdateDL_C1AF1076(uint32_t build)
{
	sub_0790(build);
	return 0;
}

int sceLibUpdateDL_36D8F34B(int ignore)
{
	sub_07a0(ignore);
	return 0;
}

int sceLibUpdateDL_34243B86(const char *ver_str)
{
	sub_07b0(ver_str);
	return 0;
}

int sceLibUpdateDL_4F49C9C1(void)
{
	if (g_initialized != 1) {
		return 0x805f0002;
	}
	return sub_1e20();
}

static int sub_2dd8(MtState *mt, uint32_t seed)
{
	mt->state[0] = seed;
	for (int i = 1; i < 624; i++) {
		uint32_t prev = mt->state[i - 1];
		mt->state[i] = (prev ^ (prev >> 30)) * 0x6c078965U + i;
	}
	mt->index = 0;
	for (int i = 622; i >= 0; i--) {
		sub_0000(mt);
	}
	return 0;
}
