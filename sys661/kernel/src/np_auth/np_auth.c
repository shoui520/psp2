/*
 * ePSP 6.61 sceNpAuth (np_auth.prx)
 * Reimplemented from 3.650.011-TOOL pcff/elf/kd/np_auth.prx
 */

#include <stdint.h>
#include <stddef.h>

typedef int32_t SceUID;
typedef uint32_t SceSize;

/* Imports */
extern int sceNpCore_0366DAB6(char *buf, uint32_t size);
extern int sceNpCore_04096629(int idx, char *buf, uint32_t size);
extern int sceNpCore_243690EE(char *buf, uint32_t size);
extern int sceNpCore_5145344F(int idx, char *name, uint32_t name_sz, char *val, uint32_t val_sz);
extern int sceNpCore_515B65E8(int idx, const char *val, uint32_t len);
extern int sceNpCore_8AFAB4A0(void *param, char *buf, uint32_t size);
extern int sceNpCore_9218ACF6(char *name, uint32_t name_sz, char *val, uint32_t val_sz);
extern int sceNpCore_B13D27CA(char *url, uint32_t size);
extern int sceNpCore_E7AED5A3(char *buf, uint32_t size);

extern int sceKernelQueryMemoryInfo(uint32_t addr, uint32_t *mem_id, uint32_t *block_id);
extern int sceNetFreeThreadinfo(SceUID thid);

extern int sceNet_lib_B5CE388A(char *dst, const char *src, uint32_t size);
extern int sceNet_lib_CF705E46(char *dst, const char *fmt, ...);
extern uint32_t sceNet_lib_D8722983(const char *str);
extern int sceNet_lib_E0A81C7C(const char *s1, const char *s2, uint32_t n);

extern int sceParseUri_49E950EC(char *dst, uint32_t *out_len, uint32_t max_len, const char *src);
extern int sceParseHttp_AD7BFDEF(const char *headers, uint32_t headers_len, const char *name, const char **out_val, uint32_t *out_len);

extern int sceHttpGetContentLength(int req_id, uint64_t *len);
extern int sceHttpSetRecvTimeOut(int tmpl_id, uint32_t timeout);
extern int sceHttpAddExtraHeader(int id, const char *name, const char *value, uint32_t mode);
extern int sceHttpGetStatusCode(int req_id, int *status_code);
extern int sceHttpDeleteConnection(int conn_id);
extern int sceHttpSetConnectTimeOut(int tmpl_id, uint32_t timeout);
extern int sceHttpSetSendTimeOut(int tmpl_id, uint32_t timeout);
extern int sceHttpCreateTemplate(const char *agent, int http_ver, int auto_proxy);
extern int sceHttpDeleteRequest(int req_id);
extern int sceHttp_AB1540D5(int req_id, int *err1, int *err2);
extern int sceHttpCreateRequestWithURL(int conn_id, int method, const char *url, uint64_t content_len);
extern int sceHttpSendRequest(int req_id, const void *data, uint32_t data_size);
extern int sceHttpAbortRequest(int req_id);
extern int sceHttpCreateConnectionWithURL(int tmpl_id, const char *url, int enable_keepalive);
extern int sceHttpGetAllHeader(int req_id, char **header, uint32_t *header_size);
extern int sceHttpReadData(int req_id, void *data, uint32_t size);
extern int sceHttpDeleteTemplate(int tmpl_id);
extern int sceHttp_3C478044(int tmpl_id, int val);
extern int sceHttp_87F1E666(int tmpl_id, int val);

extern int sceNp_B4063F7A(char *buf);
extern int sceNp_B819A0C8(char *user, char *pass, void *opt);
extern int sceNp_E24DA399(void);

extern int sceKernelCpuSuspendIntr(void);
extern void *sceKernelMemset(void *s, int c, SceSize n);
extern int _sceKernelCheckThreadStack(void);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize n);
extern void sceKernelCpuResumeIntr(int intr);

extern int sceKernelGetModuleGPByAddress(uint32_t addr, uint32_t *out_gp);

extern int sceKernelSendMbx(SceUID mbxid, void *msg);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceKernelWaitThreadEnd(SceUID thid, uint32_t *timeout);
extern int sceKernelReferVplStatus(SceUID uid, void *info);
extern SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *), int initPriority, int stackSize, uint32_t attr, void *option);
extern SceUID sceKernelCreateVpl(const char *name, int part, int attr, uint32_t size, void *opt);
extern SceUID sceKernelCreateMbx(const char *name, uint32_t attr, void *opt);
extern int sceKernelDeleteMbx(SceUID mbxid);
extern int sceKernelDeleteVpl(SceUID uid);
extern int sceKernelReceiveMbx(SceUID mbxid, void **pmsg, uint32_t *timeout);
extern int sceKernelDeleteThread(SceUID thid);
extern int sceKernelTryAllocateVpl(SceUID uid, uint32_t size, void **data);
extern int sceKernelFreeVpl(SceUID uid, void *data);

/* Data structures */
typedef struct SceNpAuthParamNode {
	struct SceNpAuthParamNode *next; /* 0x00 */
	uint32_t id;                     /* 0x04 */
	uint32_t len;                    /* 0x08 */
	char data[];                     /* 0x0c */
} SceNpAuthParamNode;

typedef struct SceNpAuthRequest {
	struct SceNpAuthRequest *next;   /* 0x00 */
	int req_id;                      /* 0x04 */
	uint8_t state;                   /* 0x08 */
	uint8_t has_entitlement;         /* 0x09 */
	uint8_t use_console_id;          /* 0x0a */
	uint8_t pad_0b;                  /* 0x0b */
	uint16_t ver_major;              /* 0x0c */
	uint16_t ver_minor;              /* 0x0e */
	SceNpAuthParamNode *params_head; /* 0x10 */
	SceNpAuthParamNode *params_tail; /* 0x14 */
	uint32_t param_mask;             /* 0x18 */
	int (*pre_cb)(int, int, void *); /* 0x1c */
	int (*done_cb)(int, int, void *);/* 0x20 */
	void *cb_arg;                    /* 0x24 */
	int http_req_id;                 /* 0x28 */
	uint32_t pad_2c;                 /* 0x2c */
	void *mbx_next;                  /* 0x30 */
	uint8_t mbx_prio;                /* 0x34 */
	uint8_t mbx_dummy[3];            /* 0x35 */
	int64_t msg_req_id;              /* 0x38 */
	struct SceNpAuthRequest *self;   /* 0x40 */
	uint32_t pad_44;                 /* 0x44 */
	uint32_t pre_gp;                 /* 0x48 */
	uint32_t done_gp;                /* 0x4c */
} SceNpAuthRequest;

typedef struct SceNpAuthTicketBuf {
	const uint8_t *buf;              /* 0x00 */
	uint16_t ver_major;              /* 0x04 */
	uint16_t ver_minor;              /* 0x06 */
	uint32_t size;                   /* 0x08 */
	uint32_t offset;                 /* 0x0c */
} SceNpAuthTicketBuf;

typedef struct SceNpAuthVersionHandler {
	uint16_t major;
	uint16_t minor;
	int (*handler)(SceNpAuthTicketBuf *, ...);
} SceNpAuthVersionHandler;

/* Forward declarations */
static int sub_2164(SceNpAuthTicketBuf *tbuf, int param_id, void *out);
static int sub_2220(SceNpAuthTicketBuf *tbuf, int param_id, void *out);
static int sub_22dc(SceNpAuthTicketBuf *tbuf, uint8_t *out_buf, uint32_t max_count);
static int sub_23a4(SceNpAuthTicketBuf *tbuf, uint8_t *out_buf, uint32_t max_count);
static int sub_25fc(SceNpAuthTicketBuf *tbuf, const char *ent_id, uint8_t *out_ent);
static int sub_2790(SceNpAuthTicketBuf *tbuf, const char *ent_id, uint8_t *out_ent);
static int sub_2a68(const void *buf, uint32_t len, const char *ent_id, void *out_ent);
static int sub_2b94(SceNpAuthTicketBuf *tbuf, uint16_t target_tag, int skip_root);
static int sub_2cb4(SceNpAuthTicketBuf *tbuf, void *out_buf);
static int sub_2f1c(void *ctx, const char *prefix, uint32_t prefix_len, uint8_t *hdr, char *out_id);
static int sub_3050(void *ctx, const uint8_t *hdr, void *out_ent);

int sub_0b98(SceSize args, void *argp);
static int sub_0ca0(int *out_id, const void *ver, const char *service_id, int use_cid);
static int sub_0e74(int req_id);
static SceNpAuthRequest *sub_0f64(int req_id, SceNpAuthRequest **out_prev);
static int sub_0fa0(SceNpAuthRequest *req, char *buf, uint32_t buf_size);
static int sub_11dc(SceNpAuthRequest *req, const char *user, const char *pass);
static int sub_12ac(SceNpAuthRequest *req, const char *ent_id, uint32_t consumed);
static int sub_136c(SceNpAuthRequest *req, const char *cookie, uint32_t cookie_len);
static int sub_13d0(SceNpAuthRequest *req, uint32_t param_id, const void *data, uint32_t len);
static int sub_1528(SceNpAuthRequest *req, uint32_t param_id);
static int sub_15ec(SceNpAuthRequest *req);
static int sub_191c(SceNpAuthRequest *req, const void *post_data, uint32_t post_len);
static int sub_1ad4(SceNpAuthRequest *req, char *buf, uint32_t buf_size);
static int sub_1bf8(SceNpAuthRequest *req, void *buf, uint32_t size);
static int sub_1cc8(int *out_tmpl, int *out_conn, char *agent_buf, uint32_t agent_sz, char *url_buf, uint32_t url_sz);
static int sub_1e0c(int http_req_id, int err);

int sceNpAuth_61BB18B3(int req_id);
int sceNpAuth_72BB0467(int req_id);
int sceNpAuth_B714FBDD(int req_id, const char *user, const char *pass);
int sceNpAuth_CE85B3B8(int *out_id, const void *ver, const char *service_id, int use_cid);
int sceNpAuth_D99455DD(int req_id);
int sceNpAuth_DAD65284(int req_id, int (*pre_cb)(int, int, void *), int (*done_cb)(int, int, void *), void *arg);

/* .rodata tables */
static const struct {
	uint16_t type;
	uint16_t size;
} g_ticket_v2_schema[11] = {
	{ 0x0008, 0x0014 },
	{ 0x0001, 0x0004 },
	{ 0x0007, 0x0008 },
	{ 0x0007, 0x0008 },
	{ 0x0002, 0x0008 },
	{ 0x0004, 0x0020 },
	{ 0x0008, 0x0004 },
	{ 0x0004, 0x0004 },
	{ 0x0008, 0x0018 },
	{ 0x0001, 0x0004 },
	{ 0x0007, 0x0008 },
};

static const struct {
	uint32_t id;
	uint16_t type;
	uint16_t size;
} g_ticket_v3_schema[12] = {
	{ 0,  0x0008, 0x0014 },
	{ 1,  0x0001, 0x0004 },
	{ 2,  0x0007, 0x0008 },
	{ 3,  0x0007, 0x0008 },
	{ 4,  0x0002, 0x0008 },
	{ 5,  0x0004, 0x0020 },
	{ 6,  0x0008, 0x0004 },
	{ 7,  0x0004, 0x0004 },
	{ 8,  0x0008, 0x0018 },
	{ 11, 0x3011, 0x0004 },
	{ 9,  0x0001, 0x0004 },
	{ 10, 0x0007, 0x0008 },
};

/* .data */
static SceUID g_vpl_id = -1;               /* 0x3890 */
static uint32_t g_vpl_max_used = 0xffffffff; /* 0x3894 */
static char g_np_header_val[12] = "        "; /* 0x3898 */

static const uint16_t g_supported_vers[] = {
	2, 1,
	3, 0,
	0, 0
};

static SceNpAuthVersionHandler g_param_handlers[] = {
	{ 2, 1, (void *)sub_2164 },
	{ 3, 0, (void *)sub_2220 },
	{ 0, 0, NULL }
};

static SceNpAuthVersionHandler g_ent_by_id_handlers[] = {
	{ 2, 1, (void *)sub_25fc },
	{ 3, 0, (void *)sub_2790 },
	{ 0, 0, NULL }
};

static SceNpAuthVersionHandler g_ent_list_handlers[] = {
	{ 2, 1, (void *)sub_22dc },
	{ 3, 0, (void *)sub_23a4 },
	{ 0, 0, NULL }
};

/* .bss */
static SceUID g_mbx_id;                    /* 0x3900 */
static uint32_t g_stack_size;              /* 0x3904 */
static SceUID g_thread_id;                 /* 0x3908 */
static int g_initialized;                  /* 0x390c */
static SceNpAuthRequest *g_req_list;       /* 0x3910 */
static int g_last_req_id;                  /* 0x3914 */
static SceNpAuthVersionHandler *g_cur_param_h;     /* 0x3918 */
static SceNpAuthVersionHandler *g_cur_ent_id_h;    /* 0x391c */
static SceNpAuthVersionHandler *g_cur_ent_list_h;  /* 0x3920 */

/* === sub_0000 @ 0x0000 === */
static __attribute__((noinline)) int sub_0000(int part, uint32_t size, const char *name)
{
	SceUID vpl = sceKernelCreateVpl(name, part, 0, size, NULL);
	if (vpl >= 0) {
		g_vpl_id = vpl;
		g_vpl_max_used = 0;
	}
	return vpl;
}

/* === sub_0044 @ 0x0044 === */
static __attribute__((noinline)) void sub_0044(void)
{
	if (g_vpl_id > 0) {
		sceKernelDeleteVpl(g_vpl_id);
		g_vpl_id = -1;
	}
}

/* === sub_0080 @ 0x0080 === */
static __attribute__((noinline)) int sub_0080(uint32_t *out_stat)
{
	uint32_t info[16];
	if (out_stat == NULL) {
		return (int)0x80550101;
	}
	info[0] = 0x34;
	int res = sceKernelReferVplStatus(g_vpl_id, info);
	if (res >= 0) {
		sceKernelMemset(out_stat, 0, 12);
		out_stat[1] = g_vpl_max_used;
		out_stat[2] = info[11];
		out_stat[0] = info[10];
		res = 0;
	}
	return res;
}

/* === sub_0104 @ 0x0104 === */
static __attribute__((noinline)) void *sub_0104(uint32_t size)
{
	uint32_t info[16];
	void *ptr = NULL;
	if (sceKernelTryAllocateVpl(g_vpl_id, size, &ptr) < 0) {
		return NULL;
	}
	info[0] = 0x34;
	if (sceKernelReferVplStatus(g_vpl_id, info) == 0) {
		uint32_t used = info[10] - info[11];
		if (used > g_vpl_max_used) {
			g_vpl_max_used = used;
		}
	}
	return ptr;
}

/* === sub_0178 @ 0x0178 === */
static __attribute__((noinline)) void sub_0178(void *ptr)
{
	if (ptr != NULL) {
		sceKernelFreeVpl(g_vpl_id, ptr);
	}
}

/* === sceNpAuth_A1DE86F8 (sceNpAuthInit) @ 0x01a0 === */
int sceNpAuth_A1DE86F8(uint32_t pool_size, uint32_t stack_size, int prio)
{
	uint32_t opt[4];
	SceNpAuthRequest dummy_req;
	uint32_t mem_info[4];

	if (_sceKernelCheckThreadStack() < 0xfd0) {
		return (int)0x80550004;
	}
	if (stack_size < 0x3800) {
		return (int)0x8055030d;
	}
	if (g_initialized > 0) {
		int err = (int)0x80550301;
		goto cleanup;
	}

	int res = sceKernelCreateMbx("SceNpAuthWorker", 0, NULL);
	if (res < 0) {
		goto cleanup_vpl;
	}
	g_mbx_id = res;

	res = sceKernelQueryMemoryInfo((uint32_t)(uintptr_t)&g_initialized, &mem_info[0], NULL);
	if (res < 0) {
		goto cleanup_vpl;
	}

	res = sub_0000((int)mem_info[0], pool_size, "SceAuthMain");
	if (res < 0) {
		goto cleanup_vpl;
	}

	sceKernelMemset(opt, 0, 8);
	g_stack_size = stack_size;
	opt[0] = 8;
	opt[1] = mem_info[0];
	if (stack_size == 0x7fffffff) {
		sub_0b98(0, NULL);
	}
	res = sceKernelCreateThread("SceNpAuthWorker", sub_0b98, prio, (int)stack_size, 0, opt);
	if (res < 0) {
		goto cleanup_vpl;
	}
	g_thread_id = res;

	res = sceKernelStartThread(res, 0, NULL);
	if (res < 0) {
		goto cleanup_vpl;
	}

	g_initialized = 1;
	return 0;

cleanup_vpl:
	{
		int err = res;
cleanup:
		g_stack_size = 0;
		sub_0044();
		sceKernelMemset(&dummy_req, 0, sizeof(dummy_req));
		dummy_req.msg_req_id = 0;
		if (g_mbx_id > 0) {
			sceKernelSendMbx(g_mbx_id, &dummy_req.mbx_next);
		}
		if (g_thread_id > 0) {
			if (sceKernelWaitThreadEnd(g_thread_id, NULL) >= 0) {
				sceKernelDeleteThread(g_thread_id);
			}
			sceNetFreeThreadinfo(g_thread_id);
			g_thread_id = 0;
		}
		if (g_mbx_id > 0) {
			sceKernelDeleteMbx(g_mbx_id);
			g_mbx_id = 0;
		}
		return err;
	}
}

/* === sceNpAuth_CD86A656 (sceNpAuthCreateStartRequest) @ 0x03a8 === */
int sceNpAuth_CD86A656(const uint32_t *param)
{
	char user[80];
	char pass[48];
	int req_id = 0;

	if (_sceKernelCheckThreadStack() < 0xf70) {
		return (int)0x80550004;
	}
	if (param == NULL || param[0] != 0x24) {
		return (int)0x80550303;
	}
	if (g_initialized == 0) {
		return (int)0x80550302;
	}

	int res = sceNpAuth_CE85B3B8(&req_id, &param[1], (const char *)(uintptr_t)param[2], 0);
	if (res < 0) {
		goto error;
	}

	res = sceNp_B819A0C8(user, pass, NULL);
	if (res < 0) {
		goto error;
	}

	res = sceNpAuth_B714FBDD(req_id, user, pass);
	if (res < 0) {
		goto error;
	}

	if (param[3] != 0) {
		if (g_initialized == 0) {
			res = (int)0x80550302;
			goto error;
		}
		SceNpAuthRequest *req = sub_0f64(req_id, NULL);
		if (req == NULL) {
			res = (int)0x80550305;
			goto error;
		}
		res = sub_136c(req, (const char *)(uintptr_t)param[3], param[4]);
		if (res < 0) {
			goto error;
		}
	}

	if (param[5] != 0) {
		if (g_initialized == 0) {
			res = (int)0x80550302;
			goto error;
		}
		SceNpAuthRequest *req = sub_0f64(req_id, NULL);
		if (req == NULL) {
			res = (int)0x80550305;
			goto error;
		}
		res = sub_12ac(req, (const char *)(uintptr_t)param[5], param[6]);
		if (res < 0) {
			goto error;
		}
	}

	res = sceNpAuth_DAD65284(req_id, NULL, (void *)(uintptr_t)param[7], (void *)(uintptr_t)param[8]);
	if (res < 0) {
		goto error;
	}
	return req_id;

error:
	if (req_id != 0) {
		sceNpAuth_72BB0467(req_id);
	}
	return res;
}

/* === sceNpAuth_72BB0467 (sceNpAuthDestroyRequest) @ 0x0568 === */
int sceNpAuth_72BB0467(int req_id)
{
	if (_sceKernelCheckThreadStack() < 0xfe0) {
		return (int)0x80550004;
	}
	if (g_initialized == 0) {
		return (int)0x80550302;
	}
	return sub_0e74(req_id);
}

/* === sceNpAuth_D99455DD (sceNpAuthAbortRequest) @ 0x05c4 === */
int sceNpAuth_D99455DD(int req_id)
{
	if (_sceKernelCheckThreadStack() < 0xff0) {
		return (int)0x80550004;
	}
	if (g_initialized == 0) {
		return (int)0x80550302;
	}
	if (req_id == 0) {
		return (int)0x80550303;
	}
	SceNpAuthRequest *req = sub_0f64(req_id, NULL);
	if (req == NULL) {
		return (int)0x80550305;
	}
	int state = sceNpAuth_61BB18B3(req_id);
	if (state >= 0) {
		if (state < 2) {
			int intr = sceKernelCpuSuspendIntr();
			if (req->state <= 3) {
				req->state = 3;
			}
			sceKernelCpuResumeIntr(intr);
			return 0;
		}
		if (state == 2) {
			int intr = sceKernelCpuSuspendIntr();
			if (req->state <= 3) {
				req->state = 3;
			}
			sceKernelCpuResumeIntr(intr);
			if (req->http_req_id != 0) {
				return sceHttpAbortRequest(req->http_req_id);
			}
			return 0;
		}
	}
	return (int)0x80550307;
}

/* === sceNpAuth_CE85B3B8 @ 0x06f0 === */
int sceNpAuth_CE85B3B8(int *out_id, const void *ver, const char *service_id, int use_cid)
{
	if (g_initialized == 0) {
		return (int)0x80550302;
	}
	if (out_id == NULL) {
		return (int)0x80550303;
	}
	return sub_0ca0(out_id, ver, service_id, use_cid);
}

/* === sceNpAuth_B714FBDD @ 0x072c === */
int sceNpAuth_B714FBDD(int req_id, const char *user, const char *pass)
{
	if (g_initialized == 0) {
		return (int)0x80550302;
	}
	SceNpAuthRequest *req = sub_0f64(req_id, NULL);
	if (req == NULL) {
		return (int)0x80550305;
	}
	return sub_11dc(req, user, pass);
}

/* === sceNpAuth_DAD65284 @ 0x07a0 === */
int sceNpAuth_DAD65284(int req_id, int (*pre_cb)(int, int, void *), int (*done_cb)(int, int, void *), void *arg)
{
	if (g_initialized == 0) {
		return (int)0x80550302;
	}
	if (req_id == 0 || done_cb == NULL) {
		return (int)0x80550303;
	}
	SceNpAuthRequest *req = sub_0f64(req_id, NULL);
	if (req == NULL) {
		return (int)0x80550305;
	}
	int intr = sceKernelCpuSuspendIntr();
	req->pre_cb = pre_cb;
	if (sceKernelGetModuleGPByAddress((uint32_t)(uintptr_t)pre_cb, &req->pre_gp) < 0) {
		register uint32_t gp_reg asm("gp");
		req->pre_gp = gp_reg;
	}
	req->done_cb = done_cb;
	if (sceKernelGetModuleGPByAddress((uint32_t)(uintptr_t)done_cb, &req->done_gp) < 0) {
		register uint32_t gp_reg asm("gp");
		req->done_gp = gp_reg;
	}
	req->msg_req_id = (int64_t)req_id;
	req->cb_arg = arg;
	req->self = req;
	sceKernelCpuResumeIntr(intr);

	int res = sceKernelSendMbx(g_mbx_id, &req->mbx_next);
	if (res >= 0) {
		intr = sceKernelCpuSuspendIntr();
		if (req->state <= 1) {
			req->state = 1;
		}
		sceKernelCpuResumeIntr(intr);
	}
	return res;
}

/* === sceNpAuth_61BB18B3 @ 0x08f0 === */
int sceNpAuth_61BB18B3(int req_id)
{
	if (g_initialized == 0) {
		return (int)0x80550302;
	}
	if (req_id == 0) {
		return (int)0x80550303;
	}
	SceNpAuthRequest *req = sub_0f64(req_id, NULL);
	if (req == NULL) {
		return (int)0x80550305;
	}
	int intr = sceKernelCpuSuspendIntr();
	int state = req->state;
	sceKernelCpuResumeIntr(intr);
	return state;
}

/* === sub_0960 @ 0x0960 === */
static __attribute__((noinline)) uint32_t sub_0960(void)
{
	return g_stack_size;
}

/* === sceNpAuth_4EC1F667 (sceNpAuthTerm) @ 0x096c === */
int sceNpAuth_4EC1F667(void)
{
	SceNpAuthRequest dummy_req;

	if (_sceKernelCheckThreadStack() < 0xff0) {
		return (int)0x80550004;
	}
	if (g_initialized == 0) {
		return 0;
	}
	while (g_req_list != NULL) {
		sceNpAuth_D99455DD(g_req_list->req_id);
		sub_0e74(g_req_list->req_id);
	}
	g_stack_size = 0;
	sub_0044();
	sceKernelMemset(&dummy_req, 0, sizeof(dummy_req));
	dummy_req.msg_req_id = 0;
	if (g_mbx_id > 0) {
		sceKernelSendMbx(g_mbx_id, &dummy_req.mbx_next);
	}
	if (g_thread_id > 0) {
		if (sceKernelWaitThreadEnd(g_thread_id, NULL) >= 0) {
			sceKernelDeleteThread(g_thread_id);
		}
		sceNetFreeThreadinfo(g_thread_id);
		g_thread_id = 0;
	}
	if (g_mbx_id > 0) {
		sceKernelDeleteMbx(g_mbx_id);
		g_mbx_id = 0;
	}
	g_initialized = 0;
	return 0;
}

/* === sceNpAuth_3F1C1F70 @ 0x0a84 === */
int sceNpAuth_3F1C1F70(int req_id, void *buf, uint32_t size)
{
	if (_sceKernelCheckThreadStack() < 0xff0) {
		return (int)0x80550004;
	}
	if (g_initialized == 0) {
		return (int)0x80550302;
	}
	if (req_id == 0 || buf == NULL || size == 0) {
		return (int)0x80550303;
	}
	SceNpAuthRequest *req = sub_0f64(req_id, NULL);
	if (req == NULL) {
		return (int)0x80550305;
	}
	return sub_1bf8(req, buf, size);
}

/* === sceNpAuth_F4531ADC @ 0x0b3c === */
int sceNpAuth_F4531ADC(uint32_t *out_stat)
{
	if (_sceKernelCheckThreadStack() < 0xff0) {
		return (int)0x80550004;
	}
	if (g_initialized == 0) {
		return (int)0x80550302;
	}
	return sub_0080(out_stat);
}

/* === sub_0b98 @ 0x0b98 === */
__attribute__((noinline)) int sub_0b98(SceSize args, void *argp)
{
	void *msg = NULL;
	while (sceKernelReceiveMbx(g_mbx_id, &msg, NULL) >= 0) {
		uint32_t *m = (uint32_t *)msg;
		if ((m[2] | m[3]) == 0) {
			break;
		}
		SceNpAuthRequest *req = (SceNpAuthRequest *)(uintptr_t)m[4];
		if (req != NULL) {
			int intr = sceKernelCpuSuspendIntr();
			if (req->state <= 2) {
				req->state = 2;
			}
			sceKernelCpuResumeIntr(intr);
		}
		int res = sub_15ec((SceNpAuthRequest *)(uintptr_t)m[4]);
		req = (SceNpAuthRequest *)(uintptr_t)m[4];
		if (req != NULL) {
			int intr = sceKernelCpuSuspendIntr();
			if (req->state <= 4) {
				req->state = 4;
			}
			sceKernelCpuResumeIntr(intr);
		}
		if (res == 1) {
			req = (SceNpAuthRequest *)(uintptr_t)m[4];
			sub_0e74(req->req_id);
		}
	}
	return 0;
}

/* === sub_0ca0 @ 0x0ca0 === */
static __attribute__((noinline)) int sub_0ca0(int *out_id, const void *ver, const char *service_id, int use_cid)
{
	uint32_t svc_len = 0;
	if (service_id != NULL) {
		svc_len = sceNet_lib_D8722983(service_id);
	}
	if (svc_len - 1 >= 0x18) {
		return (int)0x80550308;
	}
	SceNpAuthRequest *req = (SceNpAuthRequest *)sub_0104(sizeof(SceNpAuthRequest));
	if (req == NULL) {
		return (int)0x80550304;
	}
	sceKernelMemset(req, 0, sizeof(SceNpAuthRequest));
	sceKernelMemcpy(&req->ver_major, ver, 4);
	req->use_console_id = (uint8_t)use_cid;
	req->state = 0;
	req->has_entitlement = 0;

	int res = sub_13d0(req, 2, service_id, svc_len);
	if (res < 0) {
		goto error;
	}

	int intr = sceKernelCpuSuspendIntr();
	int next_id = (g_last_req_id == 16) ? 1 : (g_last_req_id + 1);
	int found_id = (int)0x80550381;
	while (next_id != g_last_req_id) {
		if (sub_0f64(next_id, NULL) == NULL) {
			found_id = next_id;
			g_last_req_id = next_id;
			break;
		}
		next_id = (next_id == 16) ? 1 : (next_id + 1);
	}
	if (found_id < 0) {
		sceKernelCpuResumeIntr(intr);
		res = found_id;
		goto error;
	}

	*out_id = found_id;
	req->next = g_req_list;
	req->req_id = found_id;
	g_req_list = req;
	sceKernelCpuResumeIntr(intr);
	return res;

error:
	if (req != NULL) {
		while (req->params_head != NULL) {
			SceNpAuthParamNode *n = req->params_head->next;
			sub_0178(req->params_head);
			req->params_head = n;
		}
		sub_0178(req);
	}
	return res;
}

/* === sub_0e74 @ 0x0e74 === */
static __attribute__((noinline)) int sub_0e74(int req_id)
{
	SceNpAuthRequest *prev = NULL;
	int intr = sceKernelCpuSuspendIntr();
	SceNpAuthRequest *req = sub_0f64(req_id, &prev);
	if (req == NULL) {
		sceKernelCpuResumeIntr(intr);
		return (int)0x80550305;
	}
	int state = sceNpAuth_61BB18B3(req_id);
	if ((uint32_t)(state - 1) < 3) {
		sceKernelCpuResumeIntr(intr);
		return (int)0x80550306;
	}
	if (prev != NULL) {
		prev->next = req->next;
	} else {
		g_req_list = req->next;
	}
	sceKernelCpuResumeIntr(intr);

	while (req->params_head != NULL) {
		SceNpAuthParamNode *n = req->params_head->next;
		sub_0178(req->params_head);
		req->params_head = n;
	}
	sub_0178(req);
	return 0;
}

/* === sub_0f64 @ 0x0f64 === */
static __attribute__((noinline)) SceNpAuthRequest *sub_0f64(int req_id, SceNpAuthRequest **out_prev)
{
	SceNpAuthRequest *prev = NULL;
	SceNpAuthRequest *cur = g_req_list;
	while (cur != NULL) {
		if (cur->req_id == req_id) {
			break;
		}
		prev = cur;
		cur = cur->next;
	}
	if (out_prev != NULL) {
		*out_prev = prev;
	}
	return cur;
}

/* === sub_0fa0 @ 0x0fa0 === */
static __attribute__((noinline)) int sub_0fa0(SceNpAuthRequest *req, char *buf, uint32_t buf_size)
{
	char prefix[32];
	char num_str[16];
	uint32_t enc_len = 0;

	if (req == NULL) {
		sceKernelMemset(prefix, 0, 32);
		return (int)0x80550303;
	}

	int res;
	if ((req->param_mask & 1) == 0) {
		sceNet_lib_CF705E46(num_str, "%u", req->has_entitlement);
		uint32_t len = sceNet_lib_D8722983(num_str);
		res = sub_13d0(req, 0, num_str, len);
		if (res != 0) {
			goto out_err;
		}
	}

	if (req->use_console_id != 0) {
		res = sceNpCore_243690EE(buf, buf_size);
		if (res < 0) {
			goto out_err;
		}
		res = sub_13d0(req, 1, buf, (uint32_t)res);
		if (res < 0 && res != (int)0x80550382) {
			goto out_err;
		}
		req->use_console_id = 0;
	} else {
		res = sub_1528(req, 1);
		if (res < 0 && res != (int)0x80550305) {
			goto out_err;
		}
	}

	if ((req->param_mask & 0x100) == 0) {
		res = sceNpCore_E7AED5A3(buf, buf_size);
		if (res < 0) {
			goto out_err;
		}
		res = sub_13d0(req, 8, buf, 0x40);
		if (res < 0) {
			goto out_err;
		}
	}

	uint32_t total = 0;
	for (SceNpAuthParamNode *node = req->params_head; node != NULL; node = node->next) {
		res = sceNpCore_8AFAB4A0(node, prefix, 32);
		if (res < 0) {
			goto out_err;
		}
		if ((uint32_t)res >= buf_size) {
			res = (int)0x80550304;
			goto out_err;
		}
		enc_len = (uint32_t)res;
		sceNet_lib_B5CE388A(buf, prefix, (uint32_t)res);
		buf += enc_len;
		buf_size -= enc_len;
		total += enc_len;

		res = sceParseUri_49E950EC(buf, &enc_len, buf_size, node->data);
		uint32_t added = enc_len - 1;
		total += added;
		buf += added;
		buf_size -= added;
		if (res < 0) {
			goto out_err;
		}
	}

	sceKernelMemset(prefix, 0, 32);
	return (int)total;

out_err:
	sceKernelMemset(prefix, 0, 32);
	return res;
}

/* === sub_11dc @ 0x11dc === */
static __attribute__((noinline)) int sub_11dc(SceNpAuthRequest *req, const char *user, const char *pass)
{
	uint32_t ulen = (user != NULL) ? sceNet_lib_D8722983(user) : 0;
	uint32_t plen = (pass != NULL) ? sceNet_lib_D8722983(pass) : 0;
	if (req == NULL) {
		return (int)0x80550303;
	}
	if (ulen == 0 || plen == 0) {
		return (int)0x80550309;
	}
	int res = sub_13d0(req, 3, user, ulen);
	if (res >= 0) {
		res = sub_13d0(req, 4, pass, plen);
	}
	return res;
}

/* === sub_12ac @ 0x12ac === */
static __attribute__((noinline)) int sub_12ac(SceNpAuthRequest *req, const char *ent_id, uint32_t consumed)
{
	char num_str[16];
	uint32_t len = (ent_id != NULL) ? sceNet_lib_D8722983(ent_id) : 0;
	if (req == NULL) {
		return (int)0x80550303;
	}
	if (len - 1 >= 0x20) {
		return (int)0x8055030a;
	}
	req->has_entitlement = 1;
	int res = sub_13d0(req, 5, ent_id, len);
	if (res >= 0) {
		sceNet_lib_CF705E46(num_str, "%u", consumed);
		uint32_t nlen = sceNet_lib_D8722983(num_str);
		res = sub_13d0(req, 6, num_str, nlen);
	}
	return res;
}

/* === sub_136c @ 0x136c === */
static __attribute__((noinline)) int sub_136c(SceNpAuthRequest *req, const char *cookie, uint32_t cookie_len)
{
	if (req == NULL) {
		return (int)0x80550303;
	}
	if (cookie == NULL || cookie_len == 0 || cookie_len > 0x400) {
		return (int)0x8055030b;
	}
	return sub_13d0(req, 7, cookie, cookie_len);
}

/* === sub_13d0 @ 0x13d0 === */
static __attribute__((noinline)) int sub_13d0(SceNpAuthRequest *req, uint32_t param_id, const void *data, uint32_t len)
{
	char prefix[32];
	int res;

	if (req == NULL || data == NULL || param_id >= 9) {
		res = (int)0x80550303;
		goto out;
	}
	if ((req->param_mask & (1u << param_id)) != 0) {
		res = (int)0x80550382;
		goto out;
	}

	SceNpAuthParamNode *node = (SceNpAuthParamNode *)sub_0104(len + 13);
	if (node == NULL) {
		res = (int)0x80550304;
		goto out;
	}
	node->next = NULL;
	node->id = param_id;
	node->len = len;
	sceKernelMemcpy(node->data, data, len);
	node->data[len] = '\0';

	res = sceNpCore_8AFAB4A0(node, prefix, 32);
	if (res < 0) {
		goto out;
	}
	if (prefix[0] == '&') {
		if (req->params_head != NULL) {
			req->params_tail->next = node;
		} else {
			req->params_head = node;
		}
		req->params_tail = node;
	} else {
		if (req->params_tail == NULL) {
			req->params_tail = node;
		}
		node->next = req->params_head;
		req->params_head = node;
	}
	req->param_mask |= (1u << param_id);
	res = 0;

out:
	sceKernelMemset(prefix, 0, 32);
	return res;
}

/* === sub_1528 @ 0x1528 === */
static __attribute__((noinline)) int sub_1528(SceNpAuthRequest *req, uint32_t param_id)
{
	if (req == NULL || param_id >= 9) {
		return (int)0x80550303;
	}
	SceNpAuthParamNode *prev = NULL;
	SceNpAuthParamNode *cur = req->params_head;
	uint32_t mask = ~(1u << param_id);
	while (cur != NULL) {
		if (cur->id == param_id) {
			if (prev == NULL) {
				req->params_head = cur->next;
			} else {
				prev->next = cur->next;
			}
			if (cur->next == NULL) {
				req->params_tail = prev;
			}
			sub_0178(cur);
			req->param_mask &= mask;
			return 0;
		}
		prev = cur;
		cur = cur->next;
	}
	return (int)0x80550305;
}

/* === sub_15ec @ 0x15ec === */
static __attribute__((noinline)) int sub_15ec(SceNpAuthRequest *req)
{
	int tmpl_id = 0;
	int conn_id = 0;
	int cb_res = 0;

	if (req == NULL) {
		return (int)0x80550303;
	}

	int ver_ok = 0;
	for (int i = 0; g_supported_vers[i] != 0; i += 2) {
		if (g_supported_vers[i] == req->ver_major && g_supported_vers[i + 1] == req->ver_minor) {
			ver_ok = 1;
			break;
		}
	}

	int res;
	char *work_buf = NULL;
	if (!ver_ok) {
		res = (int)0x8055030c;
		goto done;
	}

	work_buf = (char *)sub_0104(0x832);
	if (work_buf == NULL) {
		res = (int)0x80550304;
		goto done;
	}
	sceKernelMemset(work_buf, 0, 0x832);
	char *body_buf = work_buf + 0x32;

	res = sub_1cc8(&tmpl_id, &conn_id, body_buf, 0x800, work_buf, 0x32);
	if (res < 0) {
		goto done;
	}

	if (req->pre_cb != NULL) {
		register uint32_t gp_reg asm("gp");
		uint32_t saved_gp = gp_reg;
		gp_reg = req->pre_gp;
		int stk = _sceKernelCheckThreadStack();
		int max_stk = (int)sub_0960();
		if (stk - max_stk > 0) {
			uint32_t pad = ((uint32_t)(stk - (int)sub_0960()) + 15) & ~15u;
			void * volatile dummy = __builtin_alloca(pad);
			(void)dummy;
			req->pre_cb(req->req_id, conn_id, req->cb_arg);
		} else {
			req->pre_cb(req->req_id, conn_id, req->cb_arg);
		}
		gp_reg = saved_gp;
	}

	res = sub_0fa0(req, body_buf, 0x800);
	if (res < 0) {
		goto done;
	}
	int post_len = res;

	if (sceNpAuth_61BB18B3(req->req_id) == 3) {
		res = (int)0x80550307;
		goto done;
	}

	res = sceHttpCreateRequestWithURL(conn_id, 1, work_buf, (uint64_t)(uint32_t)post_len);
	if (res < 0) {
		goto done;
	}
	req->http_req_id = res;

	res = sub_191c(req, body_buf, (uint32_t)post_len);
	if (res == 0) {
		res = sub_1ad4(req, body_buf, 0x800);
	}
	if (sceNpAuth_61BB18B3(req->req_id) == 3) {
		res = (int)0x80550307;
	}

done:
	if (req->done_cb != NULL) {
		int stk = _sceKernelCheckThreadStack();
		register uint32_t gp_reg asm("gp");
		uint32_t saved_gp = gp_reg;
		gp_reg = req->done_gp;
		int max_stk = (int)sub_0960();
		if (stk - max_stk > 0) {
			uint32_t pad = ((uint32_t)(stk - (int)sub_0960()) + 15) & ~15u;
			void * volatile dummy = __builtin_alloca(pad);
			(void)dummy;
			cb_res = req->done_cb(req->req_id, res, req->cb_arg);
		} else {
			cb_res = req->done_cb(req->req_id, res, req->cb_arg);
		}
		gp_reg = saved_gp;
	}

	if (req->http_req_id > 0) {
		sceHttpDeleteRequest(req->http_req_id);
		req->http_req_id = 0;
	}
	if (tmpl_id > 0) {
		sceHttpDeleteTemplate(tmpl_id);
	}
	if (conn_id > 0) {
		sceHttpDeleteConnection(conn_id);
	}
	if (work_buf != NULL) {
		sub_0178(work_buf);
	}
	return cb_res;
}

/* === sub_191c @ 0x191c === */
static __attribute__((noinline)) int sub_191c(SceNpAuthRequest *req, const void *post_data, uint32_t post_len)
{
	char hdr_name[32];
	char hdr_val[48];
	char ver_str[32];

	sceNet_lib_CF705E46(ver_str, "%d.%d", req->ver_major, req->ver_minor);
	int idx = 0;
	int done = 0;
	do {
		int res = sceNpCore_5145344F(idx, hdr_name, 32, hdr_val, 48);
		if (res < 0) {
			return res;
		}
		if (res > 0) {
			done = 1;
		}
		if (idx == 0) {
			sceNet_lib_B5CE388A(hdr_val, ver_str, 48);
		}
		res = sceHttpAddExtraHeader(req->http_req_id, hdr_name, hdr_val, 0);
		idx++;
		if (res < 0) {
			return res;
		}
	} while (!done);

	int res = sceNpCore_9218ACF6(hdr_name, 32, g_np_header_val, 4);
	if (res < 0) {
		return res;
	}
	res = sceNp_B4063F7A(g_np_header_val + 4);
	if (res < 0) {
		return res;
	}
	res = sceHttpAddExtraHeader(req->http_req_id, hdr_name, g_np_header_val, 0);
	if (res < 0) {
		return res;
	}
	res = sceHttpSendRequest(req->http_req_id, post_data, post_len);
	if (res < 0) {
		return sub_1e0c(req->http_req_id, res);
	}
	sceKernelMemset(hdr_name, 0, 32);
	sceKernelMemset(hdr_val, 0, 48);
	sceKernelMemset(g_np_header_val, 0x20, 4);
	sceKernelMemset(g_np_header_val + 4, 0x20, 4);
	return 0;
}

/* === sub_1ad4 @ 0x1ad4 === */
static __attribute__((noinline)) int sub_1ad4(SceNpAuthRequest *req, char *buf, uint32_t buf_size)
{
	char hdr_name[32];
	int status_code = 0;
	char *all_hdr = NULL;
	uint32_t all_hdr_len = 0;
	const char *val_ptr = NULL;
	uint32_t val_len = 0;
	uint64_t content_len = 0;

	int res = sceHttpGetStatusCode(req->http_req_id, &status_code);
	if (res < 0) {
		return res;
	}
	res = sceHttpGetAllHeader(req->http_req_id, &all_hdr, &all_hdr_len);
	if (res < 0) {
		return res;
	}

	int idx = 0;
	int done = 0;
	do {
		res = sceNpCore_04096629(idx, hdr_name, 32);
		if (res < 0) {
			return res;
		}
		if (res > 0) {
			done = 1;
		}
		res = sceParseHttp_AD7BFDEF(all_hdr, all_hdr_len, hdr_name, &val_ptr, &val_len);
		if (res < 0) {
			return res;
		}
		res = sceNpCore_515B65E8(idx, val_ptr, val_len);
		idx++;
		if (res < 0) {
			return res;
		}
	} while (!done);

	res = sceHttpGetContentLength(req->http_req_id, &content_len);
	if (res < 0) {
		return res;
	}
	if (content_len == 0 || content_len > 0x10000) {
		return (int)0x8055030b;
	}
	return (int)content_len;
}

/* === sub_1bf8 @ 0x1bf8 === */
static __attribute__((noinline)) int sub_1bf8(SceNpAuthRequest *req, void *buf, uint32_t size)
{
	if (req == NULL || req->http_req_id == 0 || buf == NULL || size == 0) {
		return (int)0x80550303;
	}
	uint32_t total = 0;
	uint8_t *dst = (uint8_t *)buf;
	while (1) {
		int n = sceHttpReadData(req->http_req_id, dst, size);
		if (n < 0) {
			return sub_1e0c(req->http_req_id, n);
		}
		size -= (uint32_t)n;
		dst += n;
		total += (uint32_t)n;
		if (n <= 0 || size == 0) {
			break;
		}
	}
	if (size != 0) {
		return (int)0x80550384;
	}
	return (int)total;
}

/* === sub_1cc8 @ 0x1cc8 === */
static __attribute__((noinline)) int sub_1cc8(int *out_tmpl, int *out_conn, char *agent_buf, uint32_t agent_sz, char *url_buf, uint32_t url_sz)
{
	int res = sceNpCore_0366DAB6(agent_buf, agent_sz);
	if (res < 0) {
		return res;
	}
	res = sceHttpCreateTemplate(agent_buf, 0, 0);
	if (res < 0) {
		return res;
	}
	*out_tmpl = res;

	res = sceHttpSetConnectTimeOut(*out_tmpl, 20000000);
	if (res < 0) {
		return res;
	}
	res = sceHttpSetSendTimeOut(*out_tmpl, 20000000);
	if (res < 0) {
		return res;
	}
	res = sceHttpSetRecvTimeOut(*out_tmpl, 20000000);
	if (res < 0) {
		return res;
	}

	if (sceNp_E24DA399() > 0) {
		res = sceHttp_87F1E666(*out_tmpl, 0x40);
		if (res < 0) {
			return res;
		}
		res = sceHttp_3C478044(*out_tmpl, 1);
	} else {
		res = sceHttp_87F1E666(*out_tmpl, 0xe5);
		if (res < 0) {
			return res;
		}
		res = sceHttp_3C478044(*out_tmpl, 0x1a);
	}
	if (res < 0) {
		return res;
	}

	res = sceNpCore_B13D27CA(url_buf, url_sz);
	if (res < 0) {
		return res;
	}
	res = sceHttpCreateConnectionWithURL(*out_tmpl, url_buf, 1);
	if (res < 0) {
		return res;
	}
	*out_conn = res;
	return 0;
}

/* === sub_1e0c @ 0x1e0c === */
static __attribute__((noinline)) int sub_1e0c(int http_req_id, int err)
{
	int err1 = 0, err2 = 0;
	if (err != (int)0x80431075) {
		return err;
	}
	int res = sceHttp_AB1540D5(http_req_id, &err1, &err2);
	if (res < 0) {
		return res;
	}
	if (err1 != (int)0x80435060) {
		return err1;
	}
	if (err2 & 4) {
		return (int)0x80550385;
	}
	if (err2 & 0x20) {
		return (int)0x80550386;
	}
	if (err2 & 2) {
		return (int)0x80550387;
	}
	if (err2 & 1) {
		return (int)0x80550388;
	}
	return (int)0x80550389;
}

/* === sceNpAuth_5A3CB57A (sceNpAuthGetTicketParam) @ 0x1eb4 === */
int sceNpAuth_5A3CB57A(const void *buf, uint32_t len, int param_id, void *out_param)
{
	SceNpAuthTicketBuf tbuf;
	sceKernelMemset(out_param, 0, 0x100);
	if (buf == NULL || out_param == NULL) {
		return (int)0x80550503;
	}
	if (len - 4 > 0xfffc) {
		return (int)0x80550505;
	}
	if ((uint32_t)param_id >= 12) {
		return (int)0x80550503;
	}
	tbuf.buf = (const uint8_t *)buf;
	tbuf.size = len;
	tbuf.offset = 4;
	sceKernelMemcpy(&tbuf.ver_major, buf, 4);
	uint8_t maj = (uint8_t)(tbuf.ver_major & 0xff) >> 4;
	uint8_t min = (uint8_t)((tbuf.ver_major >> 8) & 0xff);
	tbuf.ver_major = (uint16_t)(maj | ((uint16_t)min << 8));

	g_cur_param_h = g_param_handlers;
	while (g_cur_param_h->major != 0) {
		if (g_cur_param_h->major == maj && g_cur_param_h->minor == min) {
			break;
		}
		g_cur_param_h++;
	}
	if (g_cur_param_h->major == 0) {
		return (int)0x80550310;
	}
	return ((int (*)(SceNpAuthTicketBuf *, int, void *))g_cur_param_h->handler)(&tbuf, param_id, out_param);
}

/* === sceNpAuth_75FB0AE3 (sceNpAuthGetEntitlementIdList) @ 0x2000 === */
int sceNpAuth_75FB0AE3(const void *buf, uint32_t len, uint8_t *out_buf, uint32_t max_count)
{
	SceNpAuthTicketBuf tbuf;
	if (buf == NULL) {
		return (int)0x80550503;
	}
	if (len - 4 > 0xfffc) {
		return (int)0x80550505;
	}
	tbuf.buf = (const uint8_t *)buf;
	tbuf.size = len;
	tbuf.offset = 4;
	sceKernelMemcpy(&tbuf.ver_major, buf, 4);
	uint8_t maj = (uint8_t)(tbuf.ver_major & 0xff) >> 4;
	uint8_t min = (uint8_t)((tbuf.ver_major >> 8) & 0xff);
	tbuf.ver_major = (uint16_t)(maj | ((uint16_t)min << 8));

	g_cur_ent_list_h = g_ent_list_handlers;
	while (g_cur_ent_list_h->major != 0) {
		if (g_cur_ent_list_h->major == maj && g_cur_ent_list_h->minor == min) {
			break;
		}
		g_cur_ent_list_h++;
	}
	if (g_cur_ent_list_h->major == 0) {
		return (int)0x80550310;
	}
	return ((int (*)(SceNpAuthTicketBuf *, uint8_t *, uint32_t))g_cur_ent_list_h->handler)(&tbuf, out_buf, max_count);
}

/* === sceNpAuth_6900F084 (sceNpAuthGetEntitlementById) @ 0x2114 === */
int sceNpAuth_6900F084(const void *buf, uint32_t len, const char *ent_id, void *out_ent)
{
	uint8_t tmp[64];
	int res = sub_2a68(buf, len, ent_id, tmp);
	if (out_ent != NULL) {
		sceKernelMemcpy(out_ent, tmp, 64);
	}
	return res;
}

/* === sub_2164 @ 0x2164 === */
static __attribute__((noinline)) int sub_2164(SceNpAuthTicketBuf *tbuf, int param_id, void *out)
{
	if (param_id >= 11) {
		return (int)0x8055030f;
	}
	int res = sub_2b94(tbuf, 0x3000, 1);
	if (res != 0) {
		return res;
	}
	for (int i = 0; i <= param_id; i++) {
		int t = sub_2cb4(tbuf, out);
		if (t != g_ticket_v2_schema[i].type) {
			return (int)0x80550603;
		}
	}
	return 0;
}

/* === sub_2220 @ 0x2220 === */
static __attribute__((noinline)) int sub_2220(SceNpAuthTicketBuf *tbuf, int param_id, void *out)
{
	if (param_id >= 12) {
		return (int)0x8055030f;
	}
	int res = sub_2b94(tbuf, 0x3000, 1);
	if (res != 0) {
		return res;
	}
	for (int i = 0; i < 12; i++) {
		int t = sub_2cb4(tbuf, out);
		if (t != g_ticket_v3_schema[i].type) {
			return (int)0x80550603;
		}
		if ((int)g_ticket_v3_schema[i].id == param_id) {
			break;
		}
	}
	return 0;
}

/* === sub_22dc @ 0x22dc === */
static __attribute__((noinline)) int sub_22dc(SceNpAuthTicketBuf *tbuf, uint8_t *out_buf, uint32_t max_count)
{
	uint8_t tmp[256];
	uint32_t count = 0;
	while (sub_2b94(tbuf, 0x3001, 1) == 0) {
		if (sub_2cb4(tbuf, tmp) != 8) {
			return (int)0x80550603;
		}
		if (out_buf != NULL && count < max_count) {
			sceKernelMemcpy(out_buf, tmp, 0x20);
		}
		count++;
		out_buf += 0x20;
	}
	return (int)count;
}

/* === sub_23a4 @ 0x23a4 === */
static __attribute__((noinline)) int sub_23a4(SceNpAuthTicketBuf *tbuf, uint8_t *out_buf, uint32_t max_count)
{
	struct {
		uint16_t tag;
		uint16_t len;
		const uint8_t *ptr;
		uint32_t pos;
	} sub_ctx;
	uint8_t hdr[16];
	char prefix[256];
	uint8_t byte_tmp;

	tbuf->offset = 4;
	int res = sub_2220(tbuf, 8, prefix);
	if (res < 0) {
		return res;
	}
	uint32_t prefix_len = sceNet_lib_D8722983(prefix);
	tbuf->offset = 4;
	if (sub_2b94(tbuf, 0x3010, 0) != 0) {
		return (int)0x80550603;
	}
	if (tbuf->offset + 4 >= tbuf->size) {
		return (int)0x80550603;
	}
	sceKernelMemcpy(&sub_ctx, tbuf->buf + tbuf->offset, 4);
	sub_ctx.tag = (uint16_t)((sub_ctx.tag >> 8) | (sub_ctx.tag << 8));
	sub_ctx.len = (uint16_t)((sub_ctx.len >> 8) | (sub_ctx.len << 8));
	if (tbuf->offset + sub_ctx.len + 4 > tbuf->size) {
		return (int)0x80550603;
	}
	sub_ctx.ptr = tbuf->buf + tbuf->offset + 4;
	sub_ctx.pos = 0;

	uint32_t count = 0;
	while (sub_ctx.pos < sub_ctx.len) {
		if (sub_ctx.pos + 1 > sub_ctx.len) {
			return (int)0x80550603;
		}
		sceKernelMemcpy(&byte_tmp, sub_ctx.ptr + sub_ctx.pos, 1);
		sub_ctx.pos++;
		hdr[0] = (uint8_t)(((byte_tmp >> 6) & 3) | (((byte_tmp >> 5) & 1) << 2));
		uint8_t id_len = byte_tmp & 0x1f;
		hdr[1] = (id_len != 0) ? id_len : 0x20;

		char *dst_id = (out_buf != NULL && count < max_count) ? (char *)(out_buf + (count << 5)) : NULL;
		res = sub_2f1c(&sub_ctx, prefix, prefix_len, hdr, dst_id);
		if (res < 0) {
			return res;
		}
		uint32_t skip = (hdr[0] & 4) ? 16 : 8;
		if ((hdr[0] & 3) == 1) {
			skip += 8;
		}
		if (sub_ctx.pos + skip > sub_ctx.len) {
			return (int)0x80550603;
		}
		sub_ctx.pos += skip;
		count++;
	}
	return (int)count;
}

/* === sub_25fc @ 0x25fc === */
static __attribute__((noinline)) int sub_25fc(SceNpAuthTicketBuf *tbuf, const char *ent_id, uint8_t *out_ent)
{
	uint8_t tmp[256];
	while (sub_2b94(tbuf, 0x3001, 1) == 0) {
		if (sub_2cb4(tbuf, tmp) != 8) {
			return (int)0x80550603;
		}
		uint32_t len = sceNet_lib_D8722983(ent_id) + 1;
		if (sceNet_lib_E0A81C7C(ent_id, (const char *)tmp, len) == 0) {
			sceKernelMemset(out_ent, 0, 0x40);
			sceKernelMemcpy(out_ent, tmp, 0x20);
			if (sub_2cb4(tbuf, tmp) != 7) {
				return (int)0x80550603;
			}
			sceKernelMemcpy(out_ent + 0x20, tmp, 8);
			if (sub_2cb4(tbuf, tmp) != 7) {
				return (int)0x80550603;
			}
			sceKernelMemcpy(out_ent + 0x28, tmp, 8);
			if (sub_2cb4(tbuf, tmp) != 1) {
				return (int)0x80550603;
			}
			sceKernelMemcpy(out_ent + 0x30, tmp, 4);
			uint32_t etype = *(uint32_t *)(out_ent + 0x30);
			if (etype != 1) {
				return 0;
			}
			if (sub_2cb4(tbuf, tmp) != 1) {
				return (int)0x80550603;
			}
			sceKernelMemcpy(out_ent + 0x34, tmp, 4);
			if (sub_2cb4(tbuf, tmp) != 1) {
				return (int)0x80550603;
			}
			sceKernelMemcpy(out_ent + 0x38, tmp, 4);
			return 0;
		}
	}
	return (int)0x80550305;
}

/* === sub_2790 @ 0x2790 === */
static __attribute__((noinline)) int sub_2790(SceNpAuthTicketBuf *tbuf, const char *ent_id, uint8_t *out_ent)
{
	struct {
		uint16_t tag;
		uint16_t len;
		const uint8_t *ptr;
		uint32_t pos;
	} sub_ctx;
	uint8_t hdr[16];
	char prefix[256];
	char cur_id[32];
	uint8_t byte_tmp;

	uint32_t eid_len = sceNet_lib_D8722983(ent_id) + 1;
	if (eid_len > 0x20) {
		return (int)0x80550003;
	}
	tbuf->offset = 4;
	int res = sub_2220(tbuf, 8, prefix);
	if (res < 0) {
		return res;
	}
	uint32_t prefix_len = sceNet_lib_D8722983(prefix);
	tbuf->offset = 4;
	if (sub_2b94(tbuf, 0x3010, 0) != 0) {
		return (int)0x80550603;
	}
	if (sub_2b94(tbuf, 0x3010, 0) != 0) {
		return (int)0x80550603;
	}
	if (tbuf->offset + 4 >= tbuf->size) {
		return (int)0x80550603;
	}
	sceKernelMemcpy(&sub_ctx, tbuf->buf + tbuf->offset, 4);
	sub_ctx.tag = (uint16_t)((sub_ctx.tag >> 8) | (sub_ctx.tag << 8));
	sub_ctx.len = (uint16_t)((sub_ctx.len >> 8) | (sub_ctx.len << 8));
	if (tbuf->offset + sub_ctx.len + 4 > tbuf->size) {
		return (int)0x80550603;
	}
	sub_ctx.ptr = tbuf->buf + tbuf->offset + 4;
	sub_ctx.pos = 0;

	while (sub_ctx.pos < sub_ctx.len) {
		if (sub_ctx.pos + 1 > sub_ctx.len) {
			return (int)0x80550603;
		}
		sceKernelMemcpy(&byte_tmp, sub_ctx.ptr + sub_ctx.pos, 1);
		sub_ctx.pos++;
		hdr[0] = (uint8_t)(((byte_tmp >> 6) & 3) | (((byte_tmp >> 5) & 1) << 2));
		uint8_t id_len = byte_tmp & 0x1f;
		hdr[1] = (id_len != 0) ? id_len : 0x20;

		res = sub_2f1c(&sub_ctx, prefix, prefix_len, hdr, cur_id);
		if (res < 0) {
			return res;
		}
		if (sceNet_lib_E0A81C7C(ent_id, cur_id, eid_len) == 0) {
			sceKernelMemset(out_ent, 0, 0x40);
			res = sub_3050(&sub_ctx, hdr, out_ent);
			if (res < 0) {
				return res;
			}
			sceKernelMemcpy(out_ent, ent_id, eid_len);
			return 0;
		}
		uint32_t skip = (hdr[0] & 4) ? 16 : 8;
		if ((hdr[0] & 3) == 1) {
			skip += 8;
		}
		if (sub_ctx.pos + skip > sub_ctx.len) {
			return (int)0x80550603;
		}
		sub_ctx.pos += skip;
	}
	return (int)0x80550305;
}

/* === sub_2a68 @ 0x2a68 === */
static __attribute__((noinline)) int sub_2a68(const void *buf, uint32_t len, const char *ent_id, void *out_ent)
{
	SceNpAuthTicketBuf tbuf;
	if (buf == NULL || ent_id == NULL || out_ent == NULL) {
		return (int)0x80550003;
	}
	if (len - 4 > 0xfffc) {
		return (int)0x80550505;
	}
	tbuf.buf = (const uint8_t *)buf;
	tbuf.size = len;
	tbuf.offset = 4;
	sceKernelMemcpy(&tbuf.ver_major, buf, 4);
	uint8_t maj = (uint8_t)(tbuf.ver_major & 0xff) >> 4;
	uint8_t min = (uint8_t)((tbuf.ver_major >> 8) & 0xff);
	tbuf.ver_major = (uint16_t)(maj | ((uint16_t)min << 8));

	g_cur_ent_id_h = g_ent_by_id_handlers;
	while (g_cur_ent_id_h->major != 0) {
		if (g_cur_ent_id_h->major == maj && g_cur_ent_id_h->minor == min) {
			break;
		}
		g_cur_ent_id_h++;
	}
	if (g_cur_ent_id_h->major == 0) {
		return (int)0x80550310;
	}
	return ((int (*)(SceNpAuthTicketBuf *, const char *, void *))g_cur_ent_id_h->handler)(&tbuf, ent_id, out_ent);
}

/* === sub_2b94 @ 0x2b94 === */
static __attribute__((noinline)) int sub_2b94(SceNpAuthTicketBuf *tbuf, uint16_t target_tag, int skip_root)
{
	uint16_t hdr[2];
	uint32_t off = tbuf->offset;
	if (off + 4 >= tbuf->size) {
		return (int)0x80550603;
	}
	while (1) {
		sceKernelMemcpy(hdr, tbuf->buf + off, 4);
		uint16_t tag = (uint16_t)((hdr[0] >> 8) | (hdr[0] << 8));
		uint16_t len = (uint16_t)((hdr[1] >> 8) | (hdr[1] << 8));
		hdr[0] = tag;
		hdr[1] = len;
		if (tag == target_tag && (skip_root & 1) == 0) {
			tbuf->offset = off;
			return 0;
		}
		off += 4;
		if ((tag & 0xf000) == 0) {
			if (tag != 0) {
				if (off + len > tbuf->size) {
					return (int)0x80550603;
				}
				off += len;
			}
		} else if ((tag & 0xf010) == 0x3010) {
			if (off + len > tbuf->size) {
				return (int)0x80550603;
			}
			off += len;
		}
		if (hdr[0] == target_tag) {
			tbuf->offset = off;
			return 0;
		}
		if (off + 4 >= tbuf->size) {
			break;
		}
	}
	return (int)0x80550603;
}

/* === sub_2cb4 @ 0x2cb4 === */
static __attribute__((noinline)) int sub_2cb4(SceNpAuthTicketBuf *tbuf, void *out_buf)
{
	uint16_t hdr[2];
	uint32_t off = tbuf->offset;
	if (off + 4 >= tbuf->size) {
		return (int)0x80550603;
	}
	sceKernelMemcpy(hdr, tbuf->buf + off, 4);
	uint16_t tag = (uint16_t)((hdr[0] >> 8) | (hdr[0] << 8));
	uint16_t len = (uint16_t)((hdr[1] >> 8) | (hdr[1] << 8));
	hdr[0] = tag;
	hdr[1] = len;
	if (off + len + 4 > tbuf->size || tag == 0 || (tag & 0xf010) == 0x3000) {
		return (int)0x80550603;
	}
	sceKernelMemset(out_buf, 0, 0x100);
	uint32_t data_off = off + 4;

	if (tag == 1) {
		uint32_t v = 0;
		sceKernelMemcpy(&v, tbuf->buf + data_off, 4);
		*(uint32_t *)out_buf = __builtin_bswap32(v);
	} else if (tag == 2 || tag == 7) {
		uint8_t raw[8];
		sceKernelMemcpy(raw, tbuf->buf + data_off, 8);
		uint64_t val = 0;
		for (int i = 0; i < 8; i++) {
			val = (val << 8) | raw[i];
		}
		*(uint64_t *)out_buf = val;
	} else if (tag == 4 || tag == 8) {
		if (len < 0x100) {
			sceKernelMemcpy(out_buf, tbuf->buf + data_off, len);
			((uint8_t *)out_buf)[len] = 0;
		} else {
			sceKernelMemcpy(out_buf, tbuf->buf + data_off, 0xff);
			((uint8_t *)out_buf)[0xff] = 0;
		}
	} else if (tag == 0x3011) {
		uint32_t v = 0;
		sceKernelMemcpy(&v, tbuf->buf + data_off, 4);
		uint32_t bs = __builtin_bswap32(v);
		*(uint16_t *)out_buf = (uint16_t)(bs >> 16);
		((uint8_t *)out_buf)[2] = (uint8_t)(bs >> 8);
		((uint8_t *)out_buf)[3] = (uint8_t)bs;
	}
	tbuf->offset = data_off + hdr[1];
	return hdr[0];
}

/* === sub_2f1c @ 0x2f1c === */
static __attribute__((noinline)) int sub_2f1c(void *ctx, const char *prefix, uint32_t prefix_len, uint8_t *hdr, char *out_id)
{
	struct {
		uint16_t tag;
		uint16_t len;
		const uint8_t *ptr;
		uint32_t pos;
	} *c = ctx;
	uint8_t tmp[32];
	uint8_t id_len = hdr[1];

	if (c->pos + id_len > c->len) {
		return (int)0x80550603;
	}
	sceKernelMemcpy(tmp, c->ptr + c->pos, id_len);
	c->pos += id_len;

	if (out_id != NULL) {
		if (hdr[1] < 9) {
			if (prefix_len + hdr[1] + 2 > 0x20) {
				return (int)0x80550603;
			}
			sceKernelMemcpy(out_id, prefix, prefix_len);
			out_id[prefix_len] = '-';
			sceKernelMemcpy(out_id + prefix_len + 1, tmp, hdr[1]);
			out_id[prefix_len + 1 + hdr[1]] = '\0';
		} else {
			sceKernelMemset(out_id, 0, 0x20);
			sceKernelMemcpy(out_id, tmp, hdr[1]);
		}
	}
	return 0;
}

/* === sub_3050 @ 0x3050 === */
static __attribute__((noinline)) int sub_3050(void *ctx, const uint8_t *hdr, void *out_ent)
{
	struct {
		uint16_t tag;
		uint16_t len;
		const uint8_t *ptr;
		uint32_t pos;
	} *c = ctx;
	uint8_t *out = (uint8_t *)out_ent;
	uint8_t raw[8];

	*(uint32_t *)(out + 0x30) = hdr[0] & 3;
	if (c->pos + 8 > c->len) {
		return (int)0x80550603;
	}
	sceKernelMemcpy(raw, c->ptr + c->pos, 8);
	c->pos += 8;
	uint64_t v1 = 0;
	for (int i = 0; i < 8; i++) {
		v1 = (v1 << 8) | raw[i];
	}
	*(uint64_t *)(out + 0x20) = v1;

	if (hdr[0] & 4) {
		if (c->pos + 8 > c->len) {
			return (int)0x80550603;
		}
		sceKernelMemcpy(raw, c->ptr + c->pos, 8);
		c->pos += 8;
		uint64_t v2 = 0;
		for (int i = 0; i < 8; i++) {
			v2 = (v2 << 8) | raw[i];
		}
		*(uint64_t *)(out + 0x28) = v2;
	} else {
		*(uint64_t *)(out + 0x28) = 0;
	}

	if ((hdr[0] & 3) == 1) {
		if (c->pos + 4 > c->len) {
			return (int)0x80550603;
		}
		uint32_t val = 0;
		sceKernelMemcpy(&val, c->ptr + c->pos, 4);
		c->pos += 4;
		*(uint32_t *)(out + 0x34) = __builtin_bswap32(val);

		if (c->pos + 4 > c->len) {
			return (int)0x80550603;
		}
		sceKernelMemcpy(&val, c->ptr + c->pos, 4);
		c->pos += 4;
		*(uint32_t *)(out + 0x38) = __builtin_bswap32(val);
	} else {
		*(uint32_t *)(out + 0x38) = 0;
		*(uint32_t *)(out + 0x34) = 0;
	}
	return 0;
}
