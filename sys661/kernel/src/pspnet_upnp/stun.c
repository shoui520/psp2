#include "upnp_int.h"

static s32 g_stun_initialized;
static StunCtx g_stun_ctx[4];

s32 sceNetStun_EBF3C24D(s32 stack_size, s32 prio, const char *host, u16 port, const StunConfig *cfg, StunCallback cb, s32 *out_id);
s32 sceNetStun_4403D9E7(s32 id);
s32 sceNetStun_A0CF3A47(s32 id, void *arg);
s32 sceNetStun_FBF60C6B(s32 id, u16 port, void *arg);
s32 sceNetStun_6CA54FC9(s32 id, s32 sock_fd, void *arg);
s32 sceNetStun_40971567(s32 id, void *arg);
static s32 stunCreateInternal(s32 stack_size, s32 prio, const char *host, u16 port, const StunConfig *cfg, StunCallback cb, s32 *out_id);
static s32 stunStopInternal(s32 id, void *arg);
static s32 stunInitLib(void);
static s32 stunTermLib(void);
static s32 stunDeleteInternal(s32 id);
static s32 stunStartNatCheckInternal(s32 id, void *arg);
static s32 stunStartPortCheckInternal(s32 id, u16 port, void *arg);
static s32 stunStartSockCheckInternal(s32 id, s32 sock_fd, void *arg);
static s32 stunWorkerThread(u32 args, void *argp);
static void stunDestroyContext(s32 idx);
static void stunExecPortCheck(StunCtx *ctx, u16 port, s32 sock_fd, void *arg);
static void stunExecNatCheck(StunCtx *ctx, void *arg);
static void stunExecExtNatCheck(StunCtx *ctx, void *arg);
static s32 stunResolveServer(StunCtx *ctx, const char *host, SockAddrIn *out_addr);
static s32 stunSendRecvRequest(s32 send_fd, s32 recv_fd, const SockAddrIn *dst_addr, const SockAddrIn *expect_addr, const StunReqOpts *req, StunRespData *resp, u8 *pkt_buf, u32 pkt_buf_size);
static s32 stunTestChangedAddrReachable(s32 sock_fd, const StunAddrAttr *dst, u32 *out_res);
static s32 stunBuildBindingRequest(u8 *buf, u32 bufsize, const u8 *tid, const StunReqOpts *req);
static s32 stunParseBindingResponse(u8 *buf, u32 bufsize, const u8 *tid, StunRespData *resp);
static s32 stunWriteHeader(u8 *buf, u32 total_len, const u8 *tid, u16 msg_type);
static s32 stunVerifyHeader(u8 *buf, u32 bufsize, const u8 *tid, u16 expect_type);
static s32 stunWriteAddrAttr(u8 *buf, u32 bufsize, u16 attr_type, u16 port, u32 addr);
static s32 stunReadAddrAttr(const u8 *buf, u32 bufsize, s32 *out_skip, StunAddrAttr *out_addr);
static s32 stunWriteChangeReqAttr(u8 *buf, u32 bufsize, u32 flags);
static s32 stunWriteBytesAttr(u8 *buf, u32 bufsize, u16 attr_type, const void *data, u32 data_len);
static s32 stunWriteMessageIntegrity(u8 *pkt_start, u32 bufsize, u8 *attr_pos, const u8 *key, u32 key_len);
static s32 stunVerifyMessageIntegrity(u8 *pkt_start, u32 bufsize, u8 *attr_pos, const u8 *key, u32 key_len);
static s32 stunVerifyExtAttr(const u8 *attr_pos, u32 rem, s32 check_ext, const void *ext_data, u32 ext_len);

/* =========================================================================
 * sceNetStun init/term & exports (0x5e48..0x6013)
 * ========================================================================= */

s32 stunInitGlobal(void)
{
	s32 ret;
	if (g_stun_initialized > 0)
		return (s32)0x80411701;
	ret = stunInitLib();
	if (ret < 0)
		return ret;
	g_stun_initialized = 1;
	return 0;
}

s32 stunTermGlobal(void)
{
	s32 ret;
	if (!g_stun_initialized)
		return 0;
	ret = stunTermLib();
	if (ret < 0)
		return ret;
	g_stun_initialized = 0;
	return 0;
}

s32 sceNetStun_EBF3C24D(s32 stack_size, s32 prio, const char *host, u16 port,
                        const StunConfig *cfg, StunCallback cb, s32 *out_id)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunCreateInternal(stack_size, prio, host, port, cfg, cb, out_id);
}

s32 sceNetStun_4403D9E7(s32 id)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunDeleteInternal(id);
}

s32 sceNetStun_A0CF3A47(s32 id, void *arg)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunStartNatCheckInternal(id, arg);
}

s32 sceNetStun_FBF60C6B(s32 id, u16 port, void *arg)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunStartPortCheckInternal(id, port, arg);
}

s32 sceNetStun_6CA54FC9(s32 id, s32 sock_fd, void *arg)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunStartSockCheckInternal(id, sock_fd, arg);
}

s32 sceNetStun_40971567(s32 id, void *arg)
{
	if (!g_stun_initialized)
		return (s32)0x80411702;
	return stunStopInternal(id, arg);
}

/* =========================================================================
 * STUN client implementation (0x6014..0x85ef)
 * ========================================================================= */

static s32 stunCreateInternal(s32 stack_size, s32 prio, const char *host, u16 port,
                    const StunConfig *cfg, StunCallback cb, s32 *out_id)
{
	s32 idx, ret;
	StunCtx *ctx;

	for (idx = 0; idx < 4; idx++) {
		if (g_stun_ctx[idx].thread_id < 0)
			break;
	}
	if (idx == 4)
		return (s32)0x80411709;

	if (sceNetStrlen(host) >= 0x40)
		return (s32)0x8041170a;

	ctx = &g_stun_ctx[idx];
	sceKernelMemset(ctx->host, 0, 0x40);
	sceNetStrncpy(ctx->host, host, 0x3f);
	ctx->port = port;
	sceKernelMemcpy(&ctx->cfg, cfg, sizeof(StunConfig));
	ctx->cb = cb;
	ctx->sock_fd = -1;

	do {
		if (ctx->cfg.has_ext_data != 0) {
			ctx->ext_copy = sceNetMallocInternal(ctx->cfg.ext_data_len);
			if (ctx->ext_copy == NULL) {
				ret = (s32)0x8041170d;
				break;
			}
			sceKernelMemcpy(ctx->ext_copy, ctx->cfg.ext_data, ctx->cfg.ext_data_len);
			ctx->pkt_buf_size = ctx->cfg.ext_data_len + 0x400;
		} else {
			ctx->pkt_buf_size = 0x400;
		}

		ctx->pkt_buf = sceNetMallocInternal(ctx->pkt_buf_size);
		if (ctx->pkt_buf == NULL) {
			ret = (s32)0x8041170d;
			break;
		}

		ret = sceKernelCreateMbx("SceNetStun", 0, NULL);
		ctx->mbx_id = ret;
		if (ret < 0)
			break;

		ret = sceKernelCreateThread("SceNetStun", stunWorkerThread, prio, stack_size, 0, NULL);
		ctx->thread_id = ret;
		if (ret < 0)
			break;

		ret = sceKernelStartThread(ctx->thread_id, 4, &ctx);
		if (ret < 0)
			break;

		*out_id = ctx->id;
		return ret;
	} while (0);

	stunDestroyContext(idx);
	return ret;
}

static s32 stunStopInternal(s32 id, void *arg)
{
	StunCtx *ctx = &g_stun_ctx[id - 1];
	StunMsg *msg;

	if (ctx->thread_id < 0)
		return (s32)0x8041170a;

	ctx->flags |= 1;
	if (ctx->resolver_id > 0)
		sceNetResolverStop(ctx->resolver_id);
	if (ctx->sock_fd >= 0)
		sceNetInetSocketAbort(ctx->sock_fd);
	sceNetFreeThreadinfo(sceKernelGetThreadId());

	msg = (StunMsg *)sceNetMallocInternal(sizeof(StunMsg));
	if (msg == NULL)
		return (s32)0x8041170d;
	sceKernelMemset(msg, 0, sizeof(StunMsg));
	msg->cmd = 4;
	msg->user_arg = arg;
	return sceKernelSendMbx(ctx->mbx_id, msg);
}

static s32 stunInitLib(void)
{
	s32 i;
	sceKernelMemset(g_stun_ctx, 0, sizeof(g_stun_ctx));
	for (i = 0; i < 4; i++) {
		g_stun_ctx[i].id = i + 1;
		g_stun_ctx[i].thread_id = -1;
		g_stun_ctx[i].resolver_id = 0;
		g_stun_ctx[i].mbx_id = -1;
	}
	return 0;
}

static s32 stunTermLib(void)
{
	s32 i;
	for (i = 0; i < 4; i++) {
		StunCtx *ctx = &g_stun_ctx[i];
		if (ctx->thread_id >= 0) {
			ctx->flags |= 1;
			if (ctx->resolver_id != 0)
				sceNetResolverStop(ctx->resolver_id);
			if (ctx->sock_fd >= 0)
				sceNetInetSocketAbort(ctx->sock_fd);
			sceNetFreeThreadinfo(sceKernelGetThreadId());
			sceKernelMemset(&ctx->stop_msg, 0, sizeof(StunMsg));
			ctx->stop_msg.cmd = 5;
			if (sceKernelSendMbx(ctx->mbx_id, &ctx->stop_msg) >= 0)
				stunDestroyContext(i);
		}
	}
	return 0;
}

static s32 stunDeleteInternal(s32 id)
{
	s32 idx = id - 1;
	StunCtx *ctx = &g_stun_ctx[idx];

	if (ctx->thread_id < 0)
		return (s32)0x8041170a;

	ctx->flags |= 1;
	if (ctx->resolver_id != 0)
		sceNetResolverStop(ctx->resolver_id);
	if (ctx->sock_fd >= 0)
		sceNetInetSocketAbort(ctx->sock_fd);
	sceNetFreeThreadinfo(sceKernelGetThreadId());

	sceKernelMemset(&ctx->stop_msg, 0, sizeof(StunMsg));
	ctx->stop_msg.cmd = 5;
	if (sceKernelSendMbx(ctx->mbx_id, &ctx->stop_msg) >= 0)
		stunDestroyContext(idx);
	return 0;
}

static s32 stunStartNatCheckInternal(s32 id, void *arg)
{
	StunCtx *ctx = &g_stun_ctx[id - 1];
	StunMsg *msg;

	ctx->flags = 0;
	msg = (StunMsg *)sceNetMallocInternal(sizeof(StunMsg));
	if (msg == NULL)
		return (s32)0x8041170d;
	sceKernelMemset(msg, 0, sizeof(StunMsg));
	msg->cmd = 1;
	msg->user_arg = arg;
	return sceKernelSendMbx(ctx->mbx_id, msg);
}

static s32 stunStartPortCheckInternal(s32 id, u16 port, void *arg)
{
	StunCtx *ctx = &g_stun_ctx[id - 1];
	StunMsg *msg;

	ctx->flags = 0;
	msg = (StunMsg *)sceNetMallocInternal(sizeof(StunMsg));
	if (msg == NULL)
		return (s32)0x8041170d;
	sceKernelMemset(msg, 0, sizeof(StunMsg));
	msg->cmd = 2;
	msg->user_arg = arg;
	msg->bind_port = port;
	return sceKernelSendMbx(ctx->mbx_id, msg);
}

static s32 stunStartSockCheckInternal(s32 id, s32 sock_fd, void *arg)
{
	StunCtx *ctx = &g_stun_ctx[id - 1];
	StunMsg *msg;

	ctx->flags = 0;
	msg = (StunMsg *)sceNetMallocInternal(sizeof(StunMsg));
	if (msg == NULL)
		return (s32)0x8041170d;
	sceKernelMemset(msg, 0, sizeof(StunMsg));
	msg->cmd = 6;
	msg->user_arg = arg;
	msg->sock_fd = sock_fd;
	return sceKernelSendMbx(ctx->mbx_id, msg);
}

static s32 stunWorkerThread(u32 args, void *argp)
{
	StunCtx *ctx = *(StunCtx **)argp;
	StunMsg *msg = NULL;

	for (;;) {
		void *raw_msg = NULL;
		if (sceKernelReceiveMbx(ctx->mbx_id, &raw_msg, NULL) < 0)
			break;
		msg = (StunMsg *)raw_msg;
		switch (msg->cmd) {
		case 1:
			stunExecNatCheck(ctx, msg->user_arg);
			break;
		case 2:
			stunExecPortCheck(ctx, msg->bind_port, -1, msg->user_arg);
			break;
		case 4:
			if (ctx->flags & 1) {
				ctx->cb(ctx->id, msg->user_arg, 4, NULL);
				sceNetFreeThreadinfo(sceKernelGetThreadId());
				ctx->flags = 0;
			}
			break;
		case 5:
			sceNetFreeThreadinfo(sceKernelGetThreadId());
			return 0;
		case 6:
			stunExecPortCheck(ctx, 0, msg->sock_fd, msg->user_arg);
			break;
		case 7:
			stunExecExtNatCheck(ctx, msg->user_arg);
			break;
		default:
			break;
		}
		sceNetFreeInternal(msg);
		msg = NULL;
	}

	sceNetFreeThreadinfo(sceKernelGetThreadId());
	return 0;
}

static void stunDestroyContext(s32 idx)
{
	StunCtx *ctx = &g_stun_ctx[idx];

	if (ctx->thread_id >= 0) {
		if (sceKernelWaitThreadEnd(ctx->thread_id, NULL) >= 0) {
			sceKernelDeleteThread(ctx->thread_id);
			sceNetFreeThreadinfo(ctx->thread_id);
		}
		ctx->thread_id = -1;
	}
	if (ctx->mbx_id >= 0) {
		sceKernelDeleteMbx(ctx->mbx_id);
		ctx->mbx_id = -1;
	}
	if (ctx->pkt_buf != NULL) {
		sceNetFreeInternal(ctx->pkt_buf);
		ctx->pkt_buf = NULL;
	}
	if (ctx->ext_copy != NULL) {
		sceNetFreeInternal(ctx->ext_copy);
		ctx->ext_copy = NULL;
	}
}

static void stunExecPortCheck(StunCtx *ctx, u16 port, s32 sock_fd, void *arg)
{
	SockAddrIn bind_addr, srv_addr;
	u8 mapped_out[8];
	StunReqOpts req;
	StunRespData resp;
	FdSet rfds;
	TimeVal tv;
	s32 ret;

	sceKernelMemset(mapped_out, 0, sizeof(mapped_out));
	do {
		ret = stunResolveServer(ctx, ctx->host, &srv_addr);
		if (ret < 0)
			break;
		srv_addr.sin_port = bswap16(ctx->port);

		if (sock_fd == -1) {
			ret = sceNetInetSocket(2, 2, 0);
			if (ret < 0)
				break;
			ctx->sock_fd = ret;

			sceKernelMemset(&bind_addr, 0, sizeof(bind_addr));
			bind_addr.sin_family = 2;
			bind_addr.sin_port = bswap16(port);
			bind_addr.sin_addr = 0;
			ret = sceNetInetBind(ctx->sock_fd, &bind_addr, sizeof(bind_addr));
			if (ret < 0)
				break;
		} else {
			for (;;) {
				sceKernelMemset(&rfds, 0, sizeof(rfds));
				rfds.bits[sock_fd >> 5] |= (1u << (sock_fd & 31));
				tv.tv_sec = 0;
				tv.tv_usec = 0;
				ret = sceNetInetSelect(sock_fd + 1, &rfds, NULL, NULL, &tv);
				if (ret <= 0)
					break;
				ret = sceNetInetRecvfrom(sock_fd, ctx->pkt_buf, ctx->pkt_buf_size, 0, NULL, NULL);
				if (ret < 0)
					break;
			}
			if (ret < 0)
				break;
			ctx->sock_fd = sock_fd;
		}

		sceKernelMemset(&req, 0, sizeof(req));
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;
		if (ctx->cfg.has_ext_data != 0) {
			req.has_ext = 1;
			req.ext_data = ctx->ext_copy;
			req.ext_data_len = ctx->cfg.ext_data_len;
		}

		sceKernelMemset(&resp, 0, sizeof(resp));
		if (ctx->cfg.username != 0) {
			resp.hmac_key = ctx->cfg.key;
			resp.hmac_key_len = 20;
		}
		if (ctx->cfg.has_ext_data != 0) {
			resp.ext_data = ctx->ext_copy;
			resp.ext_data_len = ctx->cfg.ext_data_len;
		}

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &srv_addr, &srv_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret >= 0) {
			u16 mport = bswap16(resp.mapped.port);
			sceKernelMemcpy(mapped_out, &resp.mapped.addr, 4);
			sceKernelMemcpy(mapped_out + 4, &mport, 2);
		}
	} while (0);

	if (ctx->sock_fd >= 0) {
		if (sock_fd == -1)
			sceNetInetClose(ctx->sock_fd);
		ctx->sock_fd = -1;
	}

	if (ctx->flags & 1) {
		ctx->cb(ctx->id, arg, 4, NULL);
	} else if (ret < 0) {
		ctx->cb(ctx->id, arg, ret, NULL);
	} else {
		ctx->cb(ctx->id, arg, 2, mapped_out);
	}
	sceNetFreeThreadinfo(sceKernelGetThreadId());
	ctx->flags = 0;
}

static void stunExecNatCheck(StunCtx *ctx, void *arg)
{
	u32 result[4];
	u8 mapped_ev[8];
	StunReqOpts req;
	StunRespData resp;
	SockAddrIn bind_addr, alt_addr, srv_addr;
	s32 namelen;
	s32 prev_delta_alt_port = 0, prev_delta_alt_ip = 0, retry_pass = 0;
	s32 ret;

	for (;;) {
		sceKernelMemset(result, 0, sizeof(result));
		if (!retry_pass) {
			ret = stunResolveServer(ctx, ctx->host, &srv_addr);
			if (ret < 0)
				break;
			srv_addr.sin_port = bswap16(ctx->port);
		}

		ret = sceNetInetSocket(2, 2, 0);
		if (ret < 0)
			break;
		ctx->sock_fd = ret;

		sceKernelMemset(&bind_addr, 0, sizeof(bind_addr));
		bind_addr.sin_family = 2;
		bind_addr.sin_port = 0;
		bind_addr.sin_addr = 0;
		ret = sceNetInetBind(ctx->sock_fd, &bind_addr, sizeof(bind_addr));
		if (ret < 0)
			break;

		namelen = sizeof(bind_addr);
		ret = sceNetInetGetsockname(ctx->sock_fd, &bind_addr, &namelen);
		if (ret < 0)
			break;

		/* Step 1: Binding request to primary server */
		sceKernelMemset(&req, 0, sizeof(req));
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;

		sceKernelMemset(&resp, 0, sizeof(resp));
		if (ctx->cfg.username != 0) {
			resp.hmac_key = ctx->cfg.key;
			resp.hmac_key_len = 20;
		}

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &srv_addr, &srv_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret < 0)
			break;

		if (!retry_pass) {
			u16 mport = bswap16(resp.mapped.port);
			sceKernelMemcpy(mapped_ev, &resp.mapped.addr, 4);
			sceKernelMemcpy(mapped_ev + 4, &mport, 2);
			ctx->cb(ctx->id, arg, 5, mapped_ev);
		}

		u16 mapped_port_base = resp.mapped.port;
		result[0] = resp.mapped.addr;
		u32 src_ip = resp.source.addr;
		u32 chg_ip = resp.changed.addr;
		u16 chg_port = resp.changed.port;
		result[2] = (bind_addr.sin_port == mapped_port_base) ? 1 : 0;

		/* Step 2: Binding request to same IP, changed port */
		sceKernelMemset(&alt_addr, 0, sizeof(alt_addr));
		alt_addr.sin_family = 2;
		alt_addr.sin_port = chg_port;
		alt_addr.sin_addr = src_ip;

		sceKernelMemset(&req, 0, sizeof(req));
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;

		sceKernelMemset(&resp, 0, sizeof(resp));
		if (ctx->cfg.username != 0) {
			resp.hmac_key = ctx->cfg.key;
			resp.hmac_key_len = 20;
		}

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &alt_addr, &alt_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret < 0)
			break;

		u16 mapped_port_alt_port = resp.mapped.port;
		u32 delta_alt_port = (u32)(u16)(bswap16(mapped_port_alt_port) - bswap16(mapped_port_base));
		result[3] = delta_alt_port;
		if (mapped_port_alt_port != mapped_port_base) {
			result[1] = 2;
			break;
		}

		/* Step 3: Binding request to changed IP, changed port */
		alt_addr.sin_addr = chg_ip;
		sceKernelMemset(&req, 0, sizeof(req));
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;

		sceKernelMemset(&resp, 0, sizeof(resp));
		if (ctx->cfg.username != 0) {
			resp.hmac_key = ctx->cfg.key;
			resp.hmac_key_len = 20;
		}

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &alt_addr, &alt_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret < 0)
			break;

		u16 mapped_port_alt_ip = resp.mapped.port;
		result[1] = (mapped_port_alt_ip == mapped_port_alt_port) ? 0 : 1;

		u32 delta_alt_ip = (u32)(u16)(bswap16(mapped_port_alt_ip) - bswap16(mapped_port_alt_port));
		if (delta_alt_ip == result[3])
			break;

		if (!retry_pass) {
			if (delta_alt_ip == 1 || result[3] == 1) {
				result[3] = 1;
				break;
			}
			prev_delta_alt_port = (s32)(u16)result[3];
			prev_delta_alt_ip = (s32)delta_alt_ip;
			retry_pass = 1;
			continue;
		} else {
			if (delta_alt_ip == 1 || result[3] == 1) {
				result[3] = 1;
				break;
			}
			if (delta_alt_ip == 2 || (result[3] == 2 && delta_alt_ip == result[3])) {
				if (result[3] == 2 && prev_delta_alt_port == 2 && (u32)prev_delta_alt_ip == delta_alt_ip) {
					result[3] = 2;
					break;
				}
			}
			result[3] = 1;
			break;
		}
	}

	if (ctx->sock_fd >= 0) {
		sceNetInetClose(ctx->sock_fd);
		ctx->sock_fd = -1;
	}

	if (ctx->flags & 1) {
		ctx->cb(ctx->id, arg, 4, NULL);
	} else if (ret < 0) {
		ctx->cb(ctx->id, arg, ret, NULL);
	} else {
		ctx->cb(ctx->id, arg, 1, result);
	}
	sceNetFreeThreadinfo(sceKernelGetThreadId());
	ctx->flags = 0;
}

static void stunExecExtNatCheck(StunCtx *ctx, void *arg)
{
	u32 out_res[2];
	StunReqOpts req;
	StunRespData resp;
	SockAddrIn bind_addr, src_addr, expect_addr, srv_addr;
	s32 namelen;
	u32 chg_ip;
	s32 ret;

	sceKernelMemset(out_res, 0, sizeof(out_res));
	do {
		ret = stunResolveServer(ctx, ctx->host, &srv_addr);
		if (ret < 0)
			break;
		srv_addr.sin_port = bswap16(ctx->port);

		ret = sceNetInetSocket(2, 2, 0);
		if (ret < 0)
			break;
		ctx->sock_fd = ret;

		sceKernelMemset(&bind_addr, 0, sizeof(bind_addr));
		bind_addr.sin_family = 2;
		bind_addr.sin_port = 0;
		bind_addr.sin_addr = 0;
		ret = sceNetInetBind(ctx->sock_fd, &bind_addr, sizeof(bind_addr));
		if (ret < 0)
			break;

		namelen = sizeof(bind_addr);
		ret = sceNetInetGetsockname(ctx->sock_fd, &bind_addr, &namelen);
		if (ret < 0)
			break;

		/* Request 1: basic binding */
		sceKernelMemset(&req, 0, sizeof(req));
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;

		sceKernelMemset(&resp, 0, sizeof(resp));
		if (ctx->cfg.username != 0) {
			resp.hmac_key = ctx->cfg.key;
			resp.hmac_key_len = 20;
		}

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &srv_addr, &srv_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret < 0)
			break;

		chg_ip = resp.changed.addr;
		sceKernelMemset(&src_addr, 0, sizeof(src_addr));
		src_addr.sin_family = 2;
		src_addr.sin_port = resp.source.port;
		src_addr.sin_addr = resp.source.addr;

		sceKernelMemset(&expect_addr, 0, sizeof(expect_addr));
		expect_addr.sin_family = 2;
		expect_addr.sin_port = resp.changed.port;
		expect_addr.sin_addr = resp.source.addr;

		/* Request 2: CHANGE_PORT (2) */
		sceKernelMemset(&req, 0, sizeof(req));
		req.change_req = 2;
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &src_addr, &expect_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret == (s32)0x80411703) {
			out_res[0] = 2;
			ret = stunTestChangedAddrReachable(ctx->sock_fd, &resp.mapped, out_res);
			break;
		}
		if (ret < 0)
			break;

		/* Request 3: CHANGE_IP | CHANGE_PORT (6) */
		expect_addr.sin_addr = chg_ip;
		sceKernelMemset(&req, 0, sizeof(req));
		req.change_req = 6;
		if (ctx->cfg.username != 0) {
			req.username = &ctx->cfg.username;
			req.username_len = 4;
			req.hmac_key = ctx->cfg.key;
			req.hmac_key_len = 20;
		}
		if (ctx->cfg.use_xor != 0)
			req.use_xor = 1;

		ret = stunSendRecvRequest(ctx->sock_fd, ctx->sock_fd, &src_addr, &expect_addr, &req, &resp,
			(u8 *)ctx->pkt_buf, ctx->pkt_buf_size);
		if (ret == (s32)0x80411703) {
			out_res[0] = 1;
		} else if (ret < 0) {
			break;
		} else {
			out_res[0] = 0;
		}

		ret = stunTestChangedAddrReachable(ctx->sock_fd, &resp.mapped, out_res);
	} while (0);

	if (ctx->sock_fd >= 0) {
		sceNetInetClose(ctx->sock_fd);
		ctx->sock_fd = -1;
	}

	if (ctx->flags & 1) {
		ctx->cb(ctx->id, arg, 4, NULL);
	} else if (ret < 0) {
		ctx->cb(ctx->id, arg, ret, NULL);
	} else {
		ctx->cb(ctx->id, arg, 6, out_res);
	}
	sceNetFreeThreadinfo(sceKernelGetThreadId());
	ctx->flags = 0;
}

static s32 stunResolveServer(StunCtx *ctx, const char *host, SockAddrIn *out_addr)
{
	void *res_buf = NULL;
	s32 ret = 0;

	sceKernelMemset(out_addr, 0, sizeof(SockAddrIn));
	out_addr->sin_family = 2;

	u32 ip = sceNetInetInetAddr(host);
	if (ip != 0xffffffffu) {
		out_addr->sin_addr = ip;
	} else {
		res_buf = sceNetMallocInternal(0x300);
		if (res_buf == NULL) {
			ret = (s32)0x8041170d;
		} else {
			ret = sceNetResolverCreate(&ctx->resolver_id, res_buf, 0x300);
			if (ret >= 0) {
				ret = sceNetResolverStartNtoA(ctx->resolver_id, host, &out_addr->sin_addr, 1000000, 5);
				if (ret >= 0)
					ret = 0;
			}
		}
	}

	if (ctx->resolver_id > 0)
		sceNetResolverDelete(ctx->resolver_id);
	ctx->resolver_id = 0;
	if (res_buf != NULL)
		sceNetFreeInternal(res_buf);
	return ret;
}

static s32 stunSendRecvRequest(s32 send_fd, s32 recv_fd, const SockAddrIn *dst_addr, const SockAddrIn *expect_addr,
                    const StunReqOpts *req, StunRespData *resp, u8 *pkt_buf, u32 pkt_buf_size)
{
	u32 tid[4];
	SockAddrIn from;
	TimeVal tv;
	FdSet rfds;
	s32 fromlen;
	u32 rto = 100000;
	s32 attempt = 0;
	s32 i, ret;

	for (i = 0; i < 4; i++)
		tid[i] = sceNetRand();

	for (;;) {
		if (attempt > 0) {
			if (attempt >= 9)
				return (s32)0x80411703;
			if (rto <= 1600000u - 1u)
				rto <<= 1;
		}

		ret = stunBuildBindingRequest(pkt_buf, pkt_buf_size, (const u8 *)tid, req);
		if (ret < 0)
			return ret;

		ret = sceNetInetSendto(send_fd, pkt_buf, (u32)ret, 0, dst_addr, sizeof(SockAddrIn));
		if (ret < 0)
			return ret;

		u32 deadline = sceKernelGetSystemTimeLow() + rto;
		for (;;) {
			u32 now = sceKernelGetSystemTimeLow();
			if (now > deadline)
				break;
			u32 rem = deadline - now;
			tv.tv_sec = (s32)(rem / 1000000u);
			tv.tv_usec = (s32)(rem % 1000000u);

			sceKernelMemset(&rfds, 0, sizeof(rfds));
			rfds.bits[recv_fd >> 5] |= (1u << (recv_fd & 31));
			ret = sceNetInetSelect(recv_fd + 1, &rfds, NULL, NULL, &tv);
			if (ret < 0)
				return ret;
			if (ret == 0)
				break;

			fromlen = sizeof(from);
			ret = sceNetInetRecvfrom(recv_fd, pkt_buf, pkt_buf_size, 0, &from, &fromlen);
			if (ret < 0)
				continue;
			if (from.sin_family != expect_addr->sin_family)
				continue;
			if (from.sin_addr != expect_addr->sin_addr)
				continue;

			ret = stunParseBindingResponse(pkt_buf, pkt_buf_size, (const u8 *)tid, resp);
			if (ret != (s32)0x80411704)
				return ret;
		}
		attempt++;
	}
}

static s32 stunTestChangedAddrReachable(s32 sock_fd, const StunAddrAttr *dst, u32 *out_res)
{
	SockAddrIn addr;
	TimeVal tv;
	u8 probe[0x40] __attribute__((aligned(16)));
	FdSet rfds;
	s32 i, ret;

	sceKernelMemset(&addr, 0, sizeof(addr));
	addr.sin_family = 2;
	addr.sin_port = dst->port;
	addr.sin_addr = dst->addr;

	for (i = 0; i < 5; i++) {
		ret = sceNetInetSendto(sock_fd, probe, sizeof(probe), 0, &addr, sizeof(addr));
		if (ret < 0)
			return ret;

		sceKernelMemset(&rfds, 0, sizeof(rfds));
		rfds.bits[sock_fd >> 5] |= (1u << (sock_fd & 31));
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		ret = sceNetInetSelect(sock_fd + 1, &rfds, NULL, NULL, &tv);
		if (ret < 0)
			return ret;
		if (ret > 0)
			break;
	}

	out_res[1] = (i != 5) ? 1u : 0u;
	return ret;
}

static s32 stunBuildBindingRequest(u8 *buf, u32 bufsize, const u8 *tid, const StunReqOpts *req)
{
	u8 *p = buf + 0x14;
	u32 rem = bufsize - 0x14;
	s32 ret;

	if (req->has_resp_addr == 1) {
		ret = stunWriteAddrAttr(p, rem, 2, req->resp_port, req->resp_addr);
		if (ret < 0)
			return ret;
		p += ret;
		rem -= (u32)ret;
	}

	if (req->change_req != 0) {
		ret = stunWriteChangeReqAttr(p, rem, req->change_req);
		if (ret < 0)
			return ret;
		p += ret;
		rem -= (u32)ret;
	}

	if (req->use_xor == 1) {
		ret = stunWriteBytesAttr(p, rem, 0x0021, NULL, 0);
		if (ret < 0)
			return ret;
		p += ret;
		rem -= (u32)ret;
	}

	if (req->username != NULL) {
		ret = stunWriteBytesAttr(p, rem, 0x0006, req->username, req->username_len);
		if (ret < 0)
			return ret;
		p += ret;
		rem -= (u32)ret;
	}

	if (req->has_ext == 1) {
		ret = stunWriteBytesAttr(p, rem, 0x8099, req->ext_data, req->ext_data_len);
		if (ret < 0)
			return ret;
		p += ret;
	}

	if (req->hmac_key != NULL) {
		ret = stunWriteHeader(buf, (u32)(p - buf) + 0x18, tid, 1);
		if (ret < 0)
			return ret;
		ret = stunWriteMessageIntegrity(buf, bufsize, p, (const u8 *)req->hmac_key, req->hmac_key_len);
		if (ret < 0)
			return ret;
		return (s32)((p + ret) - buf);
	}

	ret = stunWriteHeader(buf, (u32)(p - buf), tid, 1);
	if (ret < 0)
		return ret;
	return (s32)(p - buf);
}

static s32 stunParseBindingResponse(u8 *buf, u32 bufsize, const u8 *tid, StunRespData *resp)
{
	StunAttrHdr hdr;
	s32 skip;
	s32 got_mapped = 0, got_source = 0, got_changed = 0;
	s32 rem, ret;
	u8 *p;

	rem = stunVerifyHeader(buf, bufsize, tid, 0x0101);
	if (rem < 0)
		return rem;

	p = buf + 0x14;
	while ((u32)rem >= 4u) {
		sceKernelMemcpy(&hdr, p, 4);
		u16 attr_len = bswap16(hdr.length);
		if ((u32)rem < (u32)attr_len + 4u)
			break;
		hdr.length = attr_len;
		u16 attr_type = bswap16(hdr.type);

		if (attr_type == 0x0001 || attr_type == 0x8020) {
			ret = stunReadAddrAttr(p, (u32)rem, &skip, &resp->mapped);
			if (ret < 0)
				return ret;
			if (skip != 1) {
				got_mapped = 1;
				if (attr_type == 0x8020) {
					u16 xport = (u16)((u32)tid[0] | ((u32)tid[1] << 8));
					u32 xaddr = (u32)tid[0] | ((u32)tid[1] << 8) |
					            ((u32)tid[2] << 16) | ((u32)tid[3] << 24);
					resp->mapped.port ^= xport;
					resp->mapped.addr ^= xaddr;
				}
			}
		} else if (attr_type == 0x0004) {
			ret = stunReadAddrAttr(p, (u32)rem, &skip, &resp->source);
			if (ret < 0)
				return ret;
			if (skip != 1)
				got_source = 1;
		} else if (attr_type == 0x0005) {
			ret = stunReadAddrAttr(p, (u32)rem, &skip, &resp->changed);
			if (ret < 0)
				return ret;
			if (skip != 1)
				got_changed = 1;
		} else if (attr_type == 0x0008) {
			ret = stunVerifyMessageIntegrity(buf, bufsize, p, (const u8 *)resp->hmac_key, resp->hmac_key_len);
			if (ret < 0)
				return ret;
		} else if (attr_type == 0x8099) {
			ret = stunVerifyExtAttr(p, (u32)rem, resp->check_ext, resp->ext_data, resp->ext_data_len);
			if (ret < 0)
				return ret;
		}

		hdr.length = bswap16(hdr.length);
		sceKernelMemcpy(p, &hdr, 4);
		p += attr_len + 4;
		rem -= (s32)attr_len + 4;
	}

	if (!got_mapped || !got_source || !got_changed)
		return (s32)0x80411705;
	return 0;
}

static s32 stunWriteHeader(u8 *buf, u32 total_len, const u8 *tid, u16 msg_type)
{
	StunHdr hdr;

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(msg_type);
	hdr.length = bswap16((u16)(total_len - 0x14));
	sceKernelMemcpy(hdr.tid, tid, 16);
	sceKernelMemcpy(buf, &hdr, sizeof(hdr));
	return (s32)total_len;
}

static s32 stunVerifyHeader(u8 *buf, u32 bufsize, const u8 *tid, u16 expect_type)
{
	StunHdr hdr;

	if (bufsize < 0x14)
		return (s32)0x80411704;

	sceKernelMemcpy(&hdr, buf, sizeof(hdr));
	if (sceNetMemcmp(tid, hdr.tid, 16) != 0)
		return (s32)0x80411704;

	if (hdr.type != expect_type)
		return (s32)0x80411704;

	u16 body_len = bswap16(hdr.length);
	if (bufsize < (u32)body_len + 0x14)
		return (s32)0x80411704;

	hdr.length = bswap16(body_len);
	sceKernelMemcpy(buf, &hdr, sizeof(hdr));
	return (s32)body_len;
}

static s32 stunWriteAddrAttr(u8 *buf, u32 bufsize, u16 attr_type, u16 port, u32 addr)
{
	StunAttrHdr hdr;
	StunAddrAttr val;

	if (bufsize < 12)
		return (s32)0x80411706;

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(attr_type);
	hdr.length = bswap16(8);
	sceKernelMemcpy(buf, &hdr, sizeof(hdr));

	sceKernelMemset(&val, 0, sizeof(val));
	val.family = 1;
	val.port = port;
	val.addr = addr;
	sceKernelMemcpy(buf + 4, &val, sizeof(val));
	return 12;
}

static s32 stunReadAddrAttr(const u8 *buf, u32 bufsize, s32 *out_skip, StunAddrAttr *out_addr)
{
	StunAttrHdr hdr;
	StunAddrAttr val;

	if (bufsize < 12)
		return (s32)0x80411706;

	sceKernelMemcpy(&hdr, buf, 4);
	hdr.length = bswap16(hdr.length);
	if (hdr.length != 8) {
		*out_skip = 1;
		return (s32)hdr.length + 4;
	}

	sceKernelMemcpy(&val, buf + 4, 8);
	if (val.family != 1) {
		*out_skip = 1;
		return (s32)hdr.length + 4;
	}

	sceKernelMemset(out_addr, 0, sizeof(StunAddrAttr));
	out_addr->family = val.family;
	out_addr->port = val.port;
	out_addr->addr = val.addr;
	*out_skip = 0;
	return 12;
}

static s32 stunWriteChangeReqAttr(u8 *buf, u32 bufsize, u32 flags)
{
	StunAttrHdr hdr;
	u32 val;

	if (bufsize < 8)
		return (s32)0x80411706;

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(3);
	hdr.length = bswap16(4);
	sceKernelMemcpy(buf, &hdr, 4);

	val = bswap32(flags & 6u);
	sceKernelMemcpy(buf + 4, &val, 4);
	return 8;
}

static s32 stunWriteBytesAttr(u8 *buf, u32 bufsize, u16 attr_type, const void *data, u32 data_len)
{
	StunAttrHdr hdr;
	u32 total = data_len + 4;

	if (bufsize < total)
		return (s32)0x80411706;

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(attr_type);
	hdr.length = bswap16((u16)data_len);
	sceKernelMemcpy(buf, &hdr, 4);

	if (data_len != 0)
		sceKernelMemcpy(buf + 4, data, data_len);
	return (s32)total;
}

static s32 stunWriteMessageIntegrity(u8 *pkt_start, u32 bufsize, u8 *attr_pos, const u8 *key, u32 key_len)
{
	StunAttrHdr hdr;
	u8 digest[20];
	s32 msg_len = (s32)(attr_pos - pkt_start);
	u32 padded_len = (msg_len <= 0) ? 0x40u : (((u32)(msg_len - 1) | 0x3fu) + 1u);

	if (bufsize < padded_len || bufsize < (u32)msg_len + 0x18u)
		return (s32)0x80411706;

	sceKernelMemset(attr_pos, 0, padded_len - (u32)msg_len);
	hmacSha1Compute(pkt_start, (s32)padded_len, key, (s32)key_len, digest);

	sceKernelMemset(&hdr, 0, sizeof(hdr));
	hdr.type = bswap16(8);
	hdr.length = bswap16(20);
	sceKernelMemcpy(attr_pos, &hdr, 4);
	sceKernelMemcpy(attr_pos + 4, digest, 20);
	return 24;
}

static s32 stunVerifyMessageIntegrity(u8 *pkt_start, u32 bufsize, u8 *attr_pos, const u8 *key, u32 key_len)
{
	u8 expected[20];
	u8 computed[20];
	s32 msg_len = (s32)(attr_pos - pkt_start);
	u32 padded_len = (msg_len <= 0) ? 0x40u : (((u32)(msg_len - 1) | 0x3fu) + 1u);

	if (bufsize < padded_len)
		return (s32)0x80411706;

	sceKernelMemcpy(expected, attr_pos + 4, 20);
	sceKernelMemset(attr_pos, 0, padded_len - (u32)msg_len);
	hmacSha1Compute(pkt_start, (s32)padded_len, key, (s32)key_len, computed);

	if (sceNetMemcmp(expected, computed, 20) != 0)
		return (s32)0x80411707;
	return 0;
}

static s32 stunVerifyExtAttr(const u8 *attr_pos, u32 rem, s32 check_ext, const void *ext_data, u32 ext_len)
{
	StunAttrHdr hdr;

	if (!check_ext)
		return 0;

	sceKernelMemcpy(&hdr, attr_pos, 4);
	hdr.length = bswap16(hdr.length);
	if (hdr.length != ext_len)
		return (s32)0x8041170b;

	if (sceNetMemcmp(attr_pos + 4, ext_data, ext_len) != 0)
		return (s32)0x8041170c;
	return 0;
}

