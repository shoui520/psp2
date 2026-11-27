#include "libhttp_in.h"

/* -------------------------------------------------------------------------
 * Handle list sentinels, cache state & storage lock
 * ------------------------------------------------------------------------- */
static HttpRequest    g_req_list_head;
static HttpConnection g_conn_list_head;
static HttpTemplate   g_tmpl_list_head;

HttpCacheEntry *g_cache_mem_buckets[16];
HttpCacheEntry *g_cache_pool_buckets[16];

s32             g_cache_mem_inited;
u32             g_cache_mem_max_size;
u32             g_cache_mem_cur_size;
HttpCacheEntry *g_cache_mem_head;
HttpCacheEntry *g_cache_mem_tail;

s32             g_cache_pool_inited;
u32             g_cache_pool_base;
u32             g_cache_pool_size;
u32             g_cache_pool_used_blocks;
HttpCacheBlock *g_cache_pool_free_list;
HttpCacheEntry *g_cache_pool_head;
HttpCacheEntry *g_cache_pool_tail;

static s32 g_storage_locked;
static u8  g_storage_md5_buf[1024] __attribute__((aligned(4)));

/* -------------------------------------------------------------------------
 * Socket connection & buffered I/O
 * ------------------------------------------------------------------------- */
s32 http_ensure_connected(HttpRequest *req)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    s32 ret = 0;
    s32 fail_stage = 0; /* 1 = net_err, 2 = fail_reset, 3 = fail_close */

    do {
        if (conn->state == HTTP_CONN_SENDING ||
            (conn->state == HTTP_CONN_SENDING_BODY && req->sending_body != 1)) {
            if (http_is_tcp_established(conn) == 1) {
                ret = http_apply_socket_options(conn->sock_fd, &req->cfg);
                if (ret < 0) {
                    fail_stage = 1;
                    break;
                }
            } else {
                http_close_connection(conn);
                conn->state = HTTP_CONN_CONNECTING;
            }
        }

        while (conn->state == HTTP_CONN_CONNECTING) {
            s32 sock;
            u32 nonblock;

            if (conn->resolved != 1) {
                ret = http_resolve_connection_host(conn);
                if (ret < 0) {
                    fail_stage = 2;
                    break;
                }
            }

            sock = sceNetInetSocket(2, 1, 0);
            if (sock < 0) {
                fail_stage = 1;
                break;
            }

            conn->sock_fd = sock;
            conn->state = HTTP_CONN_CONNECTING;

            if (http_apply_socket_options(sock, &req->cfg) < 0) {
                fail_stage = 1;
                break;
            }

            nonblock = 1;
            if (sceNetInetSetsockopt(sock, 0xFFFF, 4105, &nonblock, 4) < 0) {
                fail_stage = 1;
                break;
            }

            ret = http_connect_with_timeout(sock, &conn->addr, req->cfg.connect_timeout,
                                            &conn->net_errno, &conn->net_psp_err);
            if (ret < 0) {
                fail_stage = 2;
                break;
            }

            nonblock = 0;
            ret = sceNetInetSetsockopt(sock, 0xFFFF, 4105, &nonblock, 4);
            if (ret < 0) {
                fail_stage = 1;
                break;
            }

            if (conn->scheme == HTTP_SCHEME_HTTPS) {
                if (conn->cfg.use_proxy == 1) {
                    http_apply_cached_proxy_auth(req, conn);
                    ret = http_ssl_proxy_connect(req, sock);
                    if (ret == SCE_HTTP_ERROR_SSL) {
                        http_copy_ssl_errors(conn, req);
                        if (conn->ssl_err == SCE_HTTPS_ERROR_PROXY && req->res_info.status_code == 407) {
                            if (http_handle_auth_response(req) > 0) {
                                if (http_is_tcp_established(conn) == 1) {
                                    ret = http_apply_socket_options(conn->sock_fd, &req->cfg);
                                    if (ret < 0) {
                                        fail_stage = 1;
                                        break;
                                    }
                                } else {
                                    http_close_connection(conn);
                                    conn->state = HTTP_CONN_CONNECTING;
                                }
                                continue;
                            }
                            if (req->method != HTTP_METHOD_HEAD)
                                req->state = HTTP_REQ_PROXY_AUTH_FAIL;
                        }
                        fail_stage = 3;
                        break;
                    }
                }

                ret = http_ssl_handshake(req, sock);
                if (ret < 0) {
                    if (ret == SCE_HTTP_ERROR_SSL) {
                        if (conn->ssl_err == SCE_HTTPS_ERROR_HANDSHAKE && conn->cfg.ssl_version == 1) {
                            http_close_connection(conn);
                            conn->state = HTTP_CONN_CONNECTING;
                            conn->ssl_err = 0;
                            conn->cfg.ssl_version = 0;
                            continue;
                        }
                        http_copy_ssl_errors(conn, req);
                    }
                    fail_stage = 3;
                    break;
                }
            }

            if (req->method == HTTP_METHOD_POST || req->method == HTTP_METHOD_PUT)
                conn->state = HTTP_CONN_SENDING_BODY;
            else
                conn->state = HTTP_CONN_SENDING;
            return ret;
        }
    } while (0);

    if (fail_stage == 0)
        return ret;

    if (fail_stage == 1) {
        conn->net_errno = sceNetInetGetErrno();
        conn->net_psp_err = sceNetInetGetPspError();
        ret = SCE_HTTP_ERROR_NETWORK;
    }
    if (fail_stage <= 2)
        conn->state = HTTP_CONN_IDLE;
    if (conn->sock_fd > 0 && conn->state != HTTP_CONN_PROXY_TUNNEL) {
        sceNetInetClose(conn->sock_fd);
        conn->sock_fd = 0;
        conn->state = HTTP_CONN_IDLE;
    }
    return ret;
}

typedef struct HttpTcpCbStat {
    struct HttpTcpCbStat *next;
    u8                    reserved[16];
    u16                   bound_port;
    u16                   remote_port;
    u16                   tcp_state;
    u16                   pad;
} HttpTcpCbStat;

s32 http_is_tcp_established(HttpConnection *conn)
{
    HttpSockAddrIn bound_addr;
    s32 addr_len = sizeof(bound_addr);
    s32 buf_len = 0;
    HttpTcpCbStat *stat_buf;
    HttpTcpCbStat *cur;
    s32 established = 0;

    if (conn->resolved == 0)
        return 0;
    if (sceNetInetGetsockname(conn->sock_fd, &bound_addr, &addr_len) < 0)
        return 0;
    if (sceNetInetGetTcpcbstat(&buf_len, NULL) < 0 || buf_len == 0)
        return 0;

    stat_buf = (HttpTcpCbStat *)http_malloc((u32)buf_len);
    if (stat_buf == NULL)
        return 0;

    if (sceNetInetGetTcpcbstat(&buf_len, stat_buf) >= 0) {
        for (cur = stat_buf; cur != NULL; cur = cur->next) {
            if (cur->bound_port == bound_addr.port) {
                if (cur->tcp_state == 4)
                    established = 1;
                break;
            }
        }
    }

    http_free(stat_buf);
    return established;
}

s32 http_skip_chunk_trailers(HttpRequest *req, s32 flags)
{
    s32 ret;

    do {
        ret = http_skip_crlf_line(req, flags);
        if (ret < 0)
            return ret;
    } while (ret != 2);
    return 0;
}

s32 http_refresh_apctl_proxy(HttpTemplate *cfg)
{
    u32 use_proxy = 0;
    u32 port_val = 0;
    char host_buf[128];
    s32 ret;
    s32 intr;

    ret = sceNetApctlGetInfo(13, &use_proxy);
    if (ret < 0)
        return ret;
    if (use_proxy != 1) {
        cfg->use_proxy = 0;
        return ret;
    }

    cfg->use_proxy = 1;
    ret = sceNetApctlGetInfo(15, &port_val);
    if (ret < 0)
        return ret;
    cfg->proxy_port = (u16)port_val;

    sceKernelMemset(host_buf, 0, sizeof(host_buf));
    ret = sceNetApctlGetInfo(14, host_buf);
    if (ret < 0)
        return ret;
    http_strip_proxy_host(host_buf);

    intr = http_intr_suspend();
    if (cfg->proxy_server != NULL && sceNet_lib_A0F16ABD(cfg->proxy_server, host_buf) != 0) {
        http_free(cfg->proxy_server);
        cfg->proxy_server = NULL;
    }
    if (cfg->proxy_server == NULL) {
        cfg->proxy_server = (char *)http_malloc(sceNet_lib_D8722983(host_buf) + 1);
        if (cfg->proxy_server != NULL)
            sceNet_lib_80C9F02A(cfg->proxy_server, host_buf);
        else
            ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
    }
    http_intr_resume(intr);
    return ret;
}

s32 http_peek_raw(HttpRequest *req, void *dst, u32 len, s32 flags)
{
    return http_buffered_recv(req, (u8 *)dst, len, flags, 1);
}

s32 http_transport_send(HttpConnection *conn, const void *buf, u32 len, s32 flags)
{
    if (conn->scheme == HTTP_SCHEME_HTTPS)
        return http_ssl_send(conn, buf, (s32)len);
    return sceNetInetSend(conn->sock_fd, buf, len, flags);
}

s32 http_buffered_recv(HttpRequest *req, u8 *dst, u32 len, s32 flags, s32 peek)
{
    HttpConnection *conn;
    u32 copied = 0;
    u32 peek_base = 0;
    s32 short_read = 0;

    (void)flags;

    if (req == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;
    if (req->state == HTTP_REQ_EOF)
        return 0;
    if (req->state == HTTP_REQ_ERROR)
        return SCE_HTTP_ERROR_NETWORK;

    conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    if (conn == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;
    if (peek == 1 && conn->recv_buf_size < len)
        return SCE_HTTP_ERROR_OUT_OF_SIZE;

    while (copied < len) {
        if (short_read || req->state == HTTP_REQ_EOF)
            return (s32)copied;

        if (conn->recv_pos == conn->recv_end) {
            u8 *fill_ptr;
            u32 fill_cap;
            s32 n;

            if (conn->state != HTTP_CONN_PROXY_TUNNEL && conn->state != HTTP_CONN_ESTABLISHED)
                return SCE_HTTP_ERROR_NOT_CONNECTED;

            if (peek == 1 && copied != 0) {
                sceKernelMemcpy(conn->recv_buf, dst, copied);
                fill_ptr = conn->recv_buf + copied;
                fill_cap = conn->recv_buf_size - copied;
                peek_base = copied;
            } else {
                fill_ptr = conn->recv_buf;
                fill_cap = conn->recv_buf_size;
            }

            n = http_transport_recv(conn, fill_ptr, fill_cap, flags);
            if (n == SCE_HTTP_ERROR_SSL) {
                http_copy_ssl_errors(conn, req);
                http_close_connection(conn);
                req->state = HTTP_REQ_EOF;
                return 0;
            }
            if (n < 0) {
                req->net_errno = sceNetInetGetErrno();
                req->net_psp_err = sceNetInetGetPspError();
                conn->net_errno = req->net_errno;
                conn->net_psp_err = req->net_psp_err;
                http_close_connection(conn);
                req->state = HTTP_REQ_ERROR;
                return req->net_errno == 11 ? SCE_HTTP_ERROR_TIMEOUT : SCE_HTTP_ERROR_NETWORK;
            }
            if (n == 0) {
                conn->peer_closed = 1;
                http_close_connection(conn);
                if (req->state == HTTP_REQ_SENT || req->state == HTTP_REQ_RECEIVING)
                    req->state = HTTP_REQ_EOF;
                continue;
            }

            conn->recv_pos = peek_base;
            conn->recv_end = peek_base + (u32)n;
            if (conn->recv_end != conn->recv_buf_size)
                short_read = 1;
        }

        {
            u32 avail = conn->recv_end - conn->recv_pos;
            u32 want = len - copied;
            u32 chunk = avail < want ? avail : want;

            sceKernelMemcpy(dst + copied, conn->recv_buf + conn->recv_pos, chunk);
            copied += chunk;
            conn->recv_pos += chunk;
        }
    }

    if (peek == 1)
        conn->recv_pos -= copied;
    return (s32)copied;
}

s32 http_apply_socket_options(s32 sock_fd, const HttpTemplate *cfg)
{
    s32 ret = sceNetInetSetsockopt(sock_fd, 0xFFFF, 4102, &cfg->recv_timeout, 4);
    if (ret >= 0) {
        ret = sceNetInetSetsockopt(sock_fd, 0xFFFF, 4101, &cfg->send_timeout, 4);
        if (ret >= 0)
            ret = sceNetInetSetsockopt(sock_fd, 6, 1, &cfg->tcp_nodelay, 4);
    }
    return ret;
}

s32 http_connect_with_timeout(s32 sock_fd, const HttpSockAddrIn *addr, u32 timeout_usec,
                              s32 *errno_out, s32 *psp_err_out)
{
    u32 rfds[8];
    u32 wfds[8];
    u32 tv[2];
    s32 ret;
    u32 bit;
    u32 mask_idx;

    ret = sceNetInetConnect(sock_fd, addr, sizeof(*addr));
    if (ret < 0) {
        *errno_out = sceNetInetGetErrno();
        *psp_err_out = sceNetInetGetPspError();
        if (*errno_out != 119)
            return SCE_HTTP_ERROR_NETWORK;
        *errno_out = 0;
        *psp_err_out = 0;
    }
    if (ret == 0)
        return 0;

    sceKernelMemset(rfds, 0, sizeof(rfds));
    mask_idx = (u32)sock_fd >> 5;
    bit = 1U << ((u32)sock_fd & 31);
    rfds[mask_idx] |= bit;
    sceKernelMemcpy(wfds, rfds, sizeof(wfds));

    tv[0] = timeout_usec / 1000000U;
    tv[1] = timeout_usec % 1000000U;

    if (sceNetInetSelect(sock_fd + 1, rfds, wfds, NULL, timeout_usec != 0 ? tv : NULL) == 0)
        return SCE_HTTP_ERROR_TIMEOUT;

    if ((rfds[mask_idx] & bit) != 0 || (wfds[mask_idx] & bit) != 0) {
        s32 opt_len = 4;
        s32 gs_ret = sceNetInetGetsockopt(sock_fd, 0xFFFF, 4103, psp_err_out, &opt_len);
        if (gs_ret < 0 || *psp_err_out != 0) {
            if (*psp_err_out != 0) {
                *errno_out = (u16)*psp_err_out;
            } else {
                *errno_out = sceNetInetGetErrno();
                *psp_err_out = sceNetInetGetPspError();
            }
            return (*errno_out == 0x74) ? SCE_HTTP_ERROR_TIMEOUT : SCE_HTTP_ERROR_NETWORK;
        }
        return 0;
    }

    *errno_out = sceNetInetGetErrno();
    *psp_err_out = sceNetInetGetPspError();
    return SCE_HTTP_ERROR_NETWORK;
}

/* -------------------------------------------------------------------------
 * Request object management
 * ------------------------------------------------------------------------- */
s32 http_create_request_with_url_internal(s32 conn_id, s32 method, const char *url, u64 content_len)
{
    SceUriElement uri;
    void *uri_work;
    u32 work_size = 0;
    s32 ret;

    sceKernelMemset(&uri, 0, sizeof(uri));
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = sceUriParse(NULL, url, NULL, &work_size, 0);
    if (ret < 0)
        return ret;
    uri_work = http_malloc(work_size);
    if (uri_work == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    sceUriParse(&uri, url, uri_work, NULL, work_size);
    ret = http_create_request_core(conn_id, method, uri.path, uri.query, uri.fragment,
                                   content_len, uri.userinfo, uri.password, uri.host);
    http_free(uri_work);
    return ret;
}

s32 http_delete_request_internal(s32 req_id)
{
    HttpRequest *req = NULL;
    HttpConnection *conn = NULL;
    s32 need_close = 0;
    s32 ret;
    s32 intr;

    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    if (req_id == HTTP_HANDLE_REQUEST)
        return SCE_HTTP_ERROR_INVALID_ID;

    intr = http_intr_suspend();
    ret = http_lock_request(req_id, &req);
    if (req == NULL) {
        ret = SCE_HTTP_ERROR_INVALID_ID;
    } else if (req->ref_count != 1) {
        req->ref_count--;
        ret = SCE_HTTP_ERROR_STILL_IN_USE;
    } else {
        if (req->prev != NULL)
            req->prev->next = req->next;
        if (req->next != NULL)
            req->next->prev = req->prev;

        conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
        if (conn->pipelined_count != 0) {
            if (conn->current_req_id == req->id) {
                s32 next_id = req->next_pipelined_id;
                conn->pipelined_count = 0;
                conn->current_req_id = 0;
                conn->tail_req_id = 0;
                need_close = 1;
                while (next_id != 0) {
                    HttpRequest *p = NULL;
                    http_lock_request(next_id, &p);
                    if (p == NULL)
                        break;
                    next_id = p->next_pipelined_id;
                    p->prev_pipelined_id = 0;
                    p->next_pipelined_id = 0;
                    p->state = HTTP_REQ_ABORTED;
                    p->ref_count--;
                }
            } else {
                if (req->prev_pipelined_id != 0) {
                    HttpRequest *prev_req = NULL;
                    conn->pipelined_count--;
                    conn->tail_req_id = req->prev_pipelined_id;
                    http_lock_request(req->prev_pipelined_id, &prev_req);
                    if (prev_req != NULL) {
                        prev_req->force_close = 1;
                        prev_req->next_pipelined_id = 0;
                        prev_req->ref_count--;
                    }
                }
                {
                    s32 next_id = req->next_pipelined_id;
                    while (next_id != 0) {
                        HttpRequest *p = NULL;
                        http_lock_request(next_id, &p);
                        if (p == NULL)
                            break;
                        next_id = p->next_pipelined_id;
                        conn->pipelined_count--;
                        p->prev_pipelined_id = 0;
                        p->next_pipelined_id = 0;
                        p->state = HTTP_REQ_ABORTED;
                        p->ref_count--;
                    }
                }
            }
        } else if (req->state != HTTP_REQ_CREATED && req->state != HTTP_REQ_DONE && req->state != HTTP_REQ_ABORTED) {
            need_close = 1;
        }
    }
    http_intr_resume(intr);

    if (need_close && conn != NULL)
        http_close_connection(conn);
    if (ret < 0)
        return ret;

    if (req->path != NULL)          http_free(req->path);
    if (req->query != NULL)         http_free(req->query);
    if (req->fragment != NULL)      http_free(req->fragment);
    if (req->res_header_buf != NULL) http_free(req->res_header_buf);
    if (req->all_header_buf != NULL) http_free(req->all_header_buf);
    if (req->url_full != NULL)      http_free(req->url_full);
    if (req->cache_read_entry != NULL)  http_cache_entry_release_locked(req->cache_read_entry);
    if (req->cache_write_entry != NULL) http_cache_abort_write(req->cache_write_entry);
    if (req->auth_entry != NULL) {
        if (req->auth_entry->status != 0)
            http_auth_entry_release_locked(req->auth_entry);
        else
            http_auth_entry_free_unused(req->auth_entry);
    }

    http_template_free_fields(&req->cfg);

    intr = http_intr_suspend();
    req->conn->ref_count--;
    http_intr_resume(intr);

    if (req->redir_conn != NULL) {
        intr = http_intr_suspend();
        req->redir_conn->ref_count--;
        http_intr_resume(intr);
    }

    http_free(req);
    return ret;
}

s32 http_lock_request(s32 req_id, HttpRequest **out_req)
{
    HttpRequest *cur = &g_req_list_head;
    s32 ret = SCE_HTTP_ERROR_INVALID_ID;
    s32 intr;

    *out_req = NULL;
    if (req_id == 0)
        return SCE_HTTP_ERROR_INVALID_ID;

    intr = http_intr_suspend();
    while (cur != NULL) {
        if (cur->id == req_id) {
            cur->ref_count++;
            *out_req = cur;
            ret = 0;
            break;
        }
        if (req_id < cur->id)
            break;
        cur = cur->next;
    }
    http_intr_resume(intr);
    return ret;
}

s32 http_register_request(HttpRequest *req)
{
    HttpRequest *prev = &g_req_list_head;
    HttpRequest *cur;
    s32 new_id;
    s32 intr = http_intr_suspend();

    cur = prev->next;
    while (cur != NULL && cur->id == prev->id + 1) {
        prev = cur;
        cur = cur->next;
    }

    if (prev->id > 0x02FFFFFF) {
        http_intr_resume(intr);
        return SCE_HTTP_ERROR_TOO_MANY_HANDLES;
    }

    new_id = prev->id + 1;
    req->id = new_id;
    req->next = prev->next;
    req->prev = prev;
    if (prev->next != NULL)
        prev->next->prev = req;
    prev->next = req;
    http_intr_resume(intr);
    return new_id;
}

char *http_build_request_url(HttpRequest *req)
{
    SceUriElement uri;
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    u32 req_len = 0;
    char *out;

    sceKernelMemset(&uri, 0, sizeof(uri));
    uri.scheme = (conn->scheme != HTTP_SCHEME_HTTP) ? "https" : "http";
    uri.host = conn->host;
    uri.port = conn->port;
    uri.path = req->path;
    uri.query = req->query;
    uri.fragment = req->fragment;

    if (sceUriBuild(NULL, &req_len, 0, &uri, 0xFFFF) < 0)
        return NULL;
    out = (char *)http_malloc(req_len);
    if (out == NULL)
        return NULL;
    if (sceUriBuild(out, NULL, req_len, &uri, 0xFFFF) < 0) {
        http_free(out);
        return NULL;
    }
    return out;
}

void http_request_list_init(void)
{
    sceKernelMemset(&g_req_list_head, 0, sizeof(g_req_list_head));
    g_req_list_head.id = HTTP_HANDLE_REQUEST;
}

s32 http_create_request_internal(s32 conn_id, s32 method, const char *path, u64 content_len)
{
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    return http_create_request_core(conn_id, method, path, NULL, NULL, content_len, NULL, NULL, NULL);
}

s32 http_create_request_core(s32 conn_id, s32 method, const char *path,
                             const char *query, const char *fragment, u64 content_len,
                             const char *user, const char *pass, const char *host_override)
{
    char *swept_path = NULL;
    char *query_copy = NULL;
    char *frag_copy = NULL;
    char *user_copy = NULL;
    char *pass_copy = NULL;
    char *host_hdr = NULL;
    HttpHeaderNode *host_node = NULL;
    HttpConnection *conn = NULL;
    HttpRequest *req = NULL;
    u32 path_len;
    s32 ret;
    s32 intr;

    path_len = sceNet_lib_D8722983(path);
    swept_path = (char *)http_malloc(path_len + 1);
    if (swept_path == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    do {
        ret = sceUriSweepPath(swept_path, path, path_len + 1);
        if (ret < 0)
            break;

        if (query != NULL) {
            query_copy = (char *)http_malloc(sceNet_lib_D8722983(query) + 1);
            if (query_copy == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(query_copy, query);
        }
        if (fragment != NULL) {
            frag_copy = (char *)http_malloc(sceNet_lib_D8722983(fragment) + 1);
            if (frag_copy == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(frag_copy, fragment);
        }
        if (user != NULL) {
            user_copy = (char *)http_malloc(sceNet_lib_D8722983(user) + 1);
            if (user_copy == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(user_copy, user);
        }
        if (pass != NULL) {
            pass_copy = (char *)http_malloc(sceNet_lib_D8722983(pass) + 1);
            if (pass_copy == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(pass_copy, pass);
        }

        req = (HttpRequest *)http_malloc(sizeof(*req));
        if (req == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
        sceKernelMemset(req, 0, sizeof(*req));

        intr = http_intr_suspend();
        ret = http_lock_connection(conn_id, &conn);
        if (conn != NULL) {
            req->conn = conn;
            ret = http_template_copy(&conn->cfg, &req->cfg);
        }
        http_intr_resume(intr);
        if (ret < 0)
            break;

        {
            const char *hdr_host = (host_override != NULL && sceNet_lib_A0F16ABD(conn->host, host_override) != 0)
                                   ? host_override : conn->host;
            ret = http_format_host_header(&host_hdr, conn->scheme, hdr_host, conn->port);
            if (ret < 0)
                break;
            ret = http_header_node_alloc(&host_node, "Host", host_hdr);
            if (ret < 0)
                break;
            http_free(host_hdr);
            host_hdr = NULL;
        }

        if (req->cfg.headers == NULL) {
            req->cfg.headers = host_node;
        } else {
            HttpHeaderNode *tail = req->cfg.headers;
            while (tail->next != NULL)
                tail = tail->next;
            tail->next = host_node;
        }

        req->state = HTTP_REQ_CREATED;
        req->method = method;
        req->path = swept_path;   swept_path = NULL;
        req->query = query_copy;  query_copy = NULL;
        req->fragment = frag_copy; frag_copy = NULL;

        if ((user != NULL && sceNet_lib_D8722983(user) == 0) ||
            (pass != NULL && sceNet_lib_D8722983(pass) == 0)) {
            if (req->cfg.auth_user != NULL) http_free(req->cfg.auth_user);
            if (req->cfg.auth_pass != NULL) http_free(req->cfg.auth_pass);
            req->cfg.auth_user = user_copy; user_copy = NULL;
            req->cfg.auth_pass = pass_copy; pass_copy = NULL;
        }

        req->req_content_len = content_len;
        req->res_header_buf = (char *)http_malloc(1500);
        if (req->res_header_buf == NULL) {
            intr = http_intr_suspend();
            conn->ref_count--;
            http_intr_resume(intr);
            ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
            break;
        }
        sceKernelMemset(req->res_header_buf, 0, 1500);

        ret = http_register_request(req);
        if (ret >= 0) {
            req->url_full = http_build_request_url(req);
            return ret;
        }
    } while (0);

    if (req != NULL) {
        if (req->res_header_buf != NULL)
            http_free(req->res_header_buf);
        http_header_list_free(req->cfg.headers);
        http_free(req);
    }
    if (swept_path != NULL) http_free(swept_path);
    if (query_copy != NULL) http_free(query_copy);
    if (frag_copy != NULL)  http_free(frag_copy);
    if (user_copy != NULL)  http_free(user_copy);
    if (pass_copy != NULL)  http_free(pass_copy);
    if (host_hdr != NULL)   http_free(host_hdr);
    return ret;
}

/* -------------------------------------------------------------------------
 * Connection object management
 * ------------------------------------------------------------------------- */
s32 http_create_connection_internal(s32 tmpl_id, const char *host, const char *scheme,
                                    const char *user, const char *pass, u16 port,
                                    s32 keep_alive, s32 redir_mode, HttpRequest *owner_req)
{
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    return http_create_connection_core(tmpl_id, host, scheme, user, pass, port,
                                       keep_alive, redir_mode, owner_req);
}

s32 http_create_connection_with_url_internal(s32 tmpl_id, const char *url, s32 keep_alive,
                                             s32 redir_mode, HttpRequest *owner_req)
{
    SceUriElement uri;
    void *uri_work;
    u32 work_size = 0;
    s32 ret;

    sceKernelMemset(&uri, 0, sizeof(uri));
    ret = sceUriParse(NULL, url, NULL, &work_size, 0);
    if (ret < 0)
        return ret;

    uri_work = http_malloc(work_size);
    if (uri_work == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    sceUriParse(&uri, url, uri_work, NULL, work_size);
    ret = http_create_connection_core(tmpl_id, uri.host, uri.scheme, uri.userinfo, uri.password,
                                      uri.port, keep_alive, redir_mode, owner_req);
    http_free(uri_work);
    return ret;
}

s32 http_create_connection_core(s32 tmpl_id, const char *host, const char *scheme,
                                const char *user, const char *pass, u16 port,
                                s32 keep_alive, s32 redir_mode, HttpRequest *owner_req)
{
    HttpConnection *conn = NULL;
    char *host_copy = NULL;
    char *user_copy = NULL;
    char *pass_copy = NULL;
    s32 scheme_id;
    s32 ret;
    s32 intr;

    scheme_id = http_parse_scheme(scheme);
    if (scheme_id < 0)
        return scheme_id;

    if (redir_mode == 1 || redir_mode == 2) {
        if (http_find_redir_connection(owner_req, host, scheme_id, port) >= 0)
            return 0;
    }

    conn = (HttpConnection *)http_malloc(sizeof(*conn));
    if (conn == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceKernelMemset(conn, 0, sizeof(*conn));

    do {
        if (host == NULL) {
            ret = SCE_HTTP_ERROR_INVALID_VALUE;
            break;
        }

        {
            u32 host_len = sceNet_lib_D8722983(host) + 1;
            host_copy = (char *)http_malloc(host_len);
            if (host_copy == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceKernelMemcpy(host_copy, host, host_len);
            host_copy[host_len - 1] = '\0';
        }

        conn->port = port;
        conn->scheme = scheme_id;
        conn->host = host_copy;
        conn->state = HTTP_CONN_IDLE;

        if (redir_mode == 1 || redir_mode == 2) {
            ret = http_template_copy(&owner_req->conn->cfg, &conn->cfg);
        } else {
            HttpTemplate *tmpl = NULL;
            if (user != NULL) {
                u32 len = sceNet_lib_D8722983(user) + 1;
                user_copy = (char *)http_malloc(len);
                if (user_copy == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
                sceKernelMemcpy(user_copy, user, len);
            }
            if (pass != NULL) {
                u32 len = sceNet_lib_D8722983(pass) + 1;
                pass_copy = (char *)http_malloc(len);
                if (pass_copy == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
                sceKernelMemcpy(pass_copy, pass, len);
            }
            intr = http_intr_suspend();
            ret = http_lock_template(tmpl_id, &tmpl);
            if (tmpl != NULL) {
                ret = http_template_copy(tmpl, &conn->cfg);
                tmpl->ref_count--;
            }
            http_intr_resume(intr);
        }
        if (ret < 0)
            break;

        conn->recv_buf = (u8 *)http_malloc(conn->cfg.recv_block_size);
        if (conn->recv_buf == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
        conn->recv_buf_size = conn->cfg.recv_block_size;
        sceKernelMemset(conn->recv_buf, 0, conn->recv_buf_size);

        if (user_copy != NULL || pass_copy != NULL) {
            if (conn->cfg.auth_user != NULL) http_free(conn->cfg.auth_user);
            if (conn->cfg.auth_pass != NULL) http_free(conn->cfg.auth_pass);
            conn->cfg.auth_user = user_copy; user_copy = NULL;
            conn->cfg.auth_pass = pass_copy; pass_copy = NULL;
        }
        conn->cfg.keep_alive = keep_alive;

        if (redir_mode != 1 && redir_mode != 2) {
            ret = http_register_connection(conn);
            if (ret < 0)
                break;
            return ret;
        }

        intr = http_intr_suspend();
        if (owner_req->conn->redir_chain_next != NULL)
            conn->redir_chain_next = owner_req->conn->redir_chain_next;
        owner_req->conn->redir_chain_next = conn;
        conn->ref_count++;
        owner_req->redir_conn = conn;
        http_intr_resume(intr);
        return 0;
    } while (0);

    if (conn != NULL) {
        if (conn->recv_buf != NULL)
            http_free(conn->recv_buf);
        http_free(conn);
    }
    if (host_copy != NULL) http_free(host_copy);
    if (user_copy != NULL) http_free(user_copy);
    if (pass_copy != NULL) http_free(pass_copy);
    return ret;
}

s32 http_delete_connection_internal(s32 conn_id)
{
    HttpConnection *conn = NULL;
    HttpConnection *cur;
    s32 ret;
    s32 intr;

    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    if (conn_id == HTTP_HANDLE_CONNECTION)
        return SCE_HTTP_ERROR_INVALID_ID;

    intr = http_intr_suspend();
    ret = http_lock_connection(conn_id, &conn);
    if (conn == NULL) {
        ret = SCE_HTTP_ERROR_INVALID_ID;
    } else if (conn->ref_count != 1) {
        conn->ref_count--;
        ret = SCE_HTTP_ERROR_STILL_IN_USE;
    } else {
        if (conn->prev != NULL)
            conn->prev->next = conn->next;
        if (conn->next != NULL)
            conn->next->prev = conn->prev;
    }
    http_intr_resume(intr);

    if (ret < 0)
        return ret;

    cur = conn;
    while (cur != NULL) {
        HttpConnection *next_redir = cur->redir_chain_next;
        if (cur->host != NULL) {
            http_free(cur->host);
            cur->host = NULL;
        }
        http_template_free_fields(&cur->cfg);
        http_close_connection(cur);
        http_free(cur->recv_buf);
        http_free(cur);
        cur = next_redir;
    }
    return ret;
}

s32 http_register_connection(HttpConnection *conn)
{
    HttpConnection *prev = &g_conn_list_head;
    HttpConnection *cur;
    s32 new_id;
    s32 intr = http_intr_suspend();

    cur = prev->next;
    while (cur != NULL && cur->id == prev->id + 1) {
        prev = cur;
        cur = cur->next;
    }

    if (prev->id > 0x01FFFFFF) {
        http_intr_resume(intr);
        return SCE_HTTP_ERROR_TOO_MANY_HANDLES;
    }

    new_id = prev->id + 1;
    conn->id = new_id;
    conn->next = prev->next;
    conn->prev = prev;
    if (prev->next != NULL)
        prev->next->prev = conn;
    prev->next = conn;
    http_intr_resume(intr);
    return new_id;
}

s32 http_lock_connection(s32 conn_id, HttpConnection **out_conn)
{
    HttpConnection *cur = &g_conn_list_head;
    s32 ret = SCE_HTTP_ERROR_INVALID_ID;
    s32 intr;

    *out_conn = NULL;
    if (conn_id == 0)
        return SCE_HTTP_ERROR_INVALID_ID;

    intr = http_intr_suspend();
    while (cur != NULL) {
        if (cur->id == conn_id) {
            cur->ref_count++;
            *out_conn = cur;
            ret = 0;
            break;
        }
        if (conn_id < cur->id)
            break;
        cur = cur->next;
    }
    http_intr_resume(intr);
    return ret;
}

void http_close_connection(HttpConnection *conn)
{
    s32 intr = http_intr_suspend();

    conn->state = HTTP_CONN_CLOSING;
    if (conn->pipelined_count >= 2) {
        s32 next_id = conn->current_req_id;
        while (next_id != 0) {
            HttpRequest *req = NULL;
            http_lock_request(next_id, &req);
            if (req == NULL)
                break;
            next_id = req->next_pipelined_id;
            req->prev_pipelined_id = 0;
            req->state = HTTP_REQ_ABORTED;
            req->ref_count--;
        }
    }
    http_intr_resume(intr);

    if (conn->scheme == HTTP_SCHEME_HTTPS)
        http_ssl_close_conn(conn);
    if (conn->sock_fd > 0)
        sceNetInetClose(conn->sock_fd);

    conn->sock_fd = 0;
    sceKernelMemset(conn->recv_buf, 0, conn->recv_buf_size);
    conn->state = HTTP_CONN_IDLE;
    conn->recv_pos = 0;
    conn->recv_end = 0;
    conn->peer_closed = 0;
    conn->pipelined_count = 0;
    conn->close_on_complete = 0;
}

void http_connection_list_init(void)
{
    sceKernelMemset(&g_conn_list_head, 0, sizeof(g_conn_list_head));
    g_conn_list_head.id = HTTP_HANDLE_CONNECTION;
}

s32 http_find_redir_connection(HttpRequest *req, const char *host, s32 scheme, u16 port)
{
    HttpConnection *cur;
    s32 found = 0;
    s32 intr;

    if (req->conn == NULL)
        return SCE_HTTP_ERROR_NOT_FOUND;

    intr = http_intr_suspend();
    cur = req->conn->redir_chain_next;
    while (cur != NULL && host != NULL && cur->host != NULL) {
        if (sceNet_lib_9CFBC7E3(host, cur->host) == 0 &&
            cur->scheme == scheme &&
            cur->port == port) {
            req->redir_conn = cur;
            cur->ref_count++;
            found = 1;
            break;
        }
        cur = cur->redir_chain_next;
    }
    http_intr_resume(intr);
    return found ? 0 : SCE_HTTP_ERROR_NOT_FOUND;
}

/* -------------------------------------------------------------------------
 * Template object management
 * ------------------------------------------------------------------------- */
HTTP_ALWAYS_INLINE void http_append_header_node(HttpHeaderNode **head, HttpHeaderNode *node)
{
    if (*head == NULL) {
        *head = node;
    } else {
        HttpHeaderNode *tail = *head;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = node;
    }
}

s32 http_create_template_internal(const char *user_agent, u32 http_ver, s32 auto_proxy)
{
    HttpTemplate *tmpl;
    HttpHeaderNode *node = NULL;
    s32 sdk_ver;
    s32 ret;

    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    tmpl = (HttpTemplate *)http_malloc(sizeof(*tmpl));
    if (tmpl == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceKernelMemset(tmpl, 0, sizeof(*tmpl));

    sdk_ver = sceKernelGetCompiledSdkVersion();

    do {
        if (user_agent != NULL) {
            ret = http_header_node_alloc(&node, "User-Agent", user_agent);
            if (ret < 0) break;
            http_append_header_node(&tmpl->headers, node);
        }

        ret = http_header_node_alloc(&node, "Accept", "*/*;q=0.01");
        if (ret < 0) break;
        http_append_header_node(&tmpl->headers, node);

        ret = http_header_node_alloc(&node, "Accept-Encoding", "");
        if (ret < 0) break;
        http_append_header_node(&tmpl->headers, node);

        ret = http_header_node_alloc(&node, "Accept-Charset", "iso-8859-1;q=0.01");
        if (ret < 0) break;
        http_append_header_node(&tmpl->headers, node);

        if (http_ver >= 2) {
            ret = SCE_HTTP_ERROR_UNKNOWN_VERSION;
            break;
        }

        tmpl->http_version = (s32)http_ver;
        tmpl->use_default_proxy = (u16)auto_proxy;
        tmpl->resolve_timeout = 3000000;
        tmpl->resolve_retry = 10;
        tmpl->res_header_max_size = 5000;
        tmpl->recv_block_size = 1500;
        tmpl->max_redirects = 6;
        tmpl->max_auth_retries = 6;
        tmpl->auto_redirect = 1;
        tmpl->cache_mode = HTTP_CACHE_NONE;
        tmpl->cookie_enabled = 1;
        tmpl->auth_enabled = 1;
        tmpl->ssl_version = (sdk_ver > 0x060600FF) ? 2 : 1;

        return http_register_template(tmpl);
    } while (0);

    http_header_list_free(tmpl->headers);
    http_free(tmpl);
    return ret;
}

s32 http_clone_template_internal(s32 tmpl_id)
{
    HttpTemplate *src = NULL;
    HttpTemplate *dst = NULL;
    s32 ret;

    ret = http_lock_template(tmpl_id, &src);
    if (ret >= 0) {
        dst = (HttpTemplate *)http_malloc(sizeof(*dst));
        if (dst == NULL) {
            ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
        } else {
            sceKernelMemset(dst, 0, sizeof(*dst));
            ret = http_template_copy(src, dst);
            if (ret >= 0)
                ret = http_register_template(dst);
        }
    }

    if (src != NULL) {
        s32 intr = http_intr_suspend();
        src->ref_count--;
        http_intr_resume(intr);
    }

    if (ret < 0 && dst != NULL)
        http_free(dst);
    return (ret >= 0) ? dst->id : ret;
}

s32 http_delete_template_internal(s32 tmpl_id)
{
    HttpTemplate *tmpl = NULL;
    s32 ret;
    s32 intr;

    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    if (tmpl_id == HTTP_HANDLE_TEMPLATE)
        return SCE_HTTP_ERROR_INVALID_ID;

    intr = http_intr_suspend();
    ret = http_lock_template(tmpl_id, &tmpl);
    if (tmpl == NULL) {
        ret = SCE_HTTP_ERROR_INVALID_ID;
    } else if (tmpl->ref_count != 1) {
        tmpl->ref_count--;
        ret = SCE_HTTP_ERROR_STILL_IN_USE;
    } else {
        if (tmpl->prev != NULL)
            tmpl->prev->next = tmpl->next;
        if (tmpl->next != NULL)
            tmpl->next->prev = tmpl->prev;
    }
    http_intr_resume(intr);

    if (ret >= 0) {
        http_template_free_fields(tmpl);
        http_free(tmpl);
    }
    return ret;
}

void http_template_free_fields(HttpTemplate *cfg)
{
    if (cfg->headers != NULL) {
        http_header_list_free(cfg->headers);
        cfg->headers = NULL;
    }
    if (cfg->proxy_server != NULL) {
        http_free(cfg->proxy_server);
        cfg->proxy_server = NULL;
    }
    if (cfg->auth_user != NULL) {
        http_free(cfg->auth_user);
        cfg->auth_user = NULL;
    }
    if (cfg->auth_pass != NULL) {
        http_free(cfg->auth_pass);
        cfg->auth_pass = NULL;
    }
}

s32 http_lock_template(s32 tmpl_id, HttpTemplate **out_tmpl)
{
    HttpTemplate *cur = &g_tmpl_list_head;
    s32 ret = SCE_HTTP_ERROR_INVALID_ID;
    s32 intr;

    *out_tmpl = NULL;
    if (tmpl_id == 0)
        return SCE_HTTP_ERROR_INVALID_ID;

    intr = http_intr_suspend();
    while (cur != NULL) {
        if (cur->id == tmpl_id) {
            cur->ref_count++;
            *out_tmpl = cur;
            ret = 0;
            break;
        }
        if (tmpl_id < cur->id)
            break;
        cur = cur->next;
    }
    http_intr_resume(intr);
    return ret;
}

s32 http_register_template(HttpTemplate *tmpl)
{
    HttpTemplate *prev = &g_tmpl_list_head;
    HttpTemplate *cur;
    s32 new_id;
    s32 intr = http_intr_suspend();

    cur = prev->next;
    while (cur != NULL && cur->id == prev->id + 1) {
        prev = cur;
        cur = cur->next;
    }

    if (prev->id > 0x00FFFFFF) {
        http_intr_resume(intr);
        return SCE_HTTP_ERROR_TOO_MANY_HANDLES;
    }

    new_id = prev->id + 1;
    tmpl->id = new_id;
    tmpl->next = prev->next;
    tmpl->prev = prev;
    if (prev->next != NULL)
        prev->next->prev = tmpl;
    prev->next = tmpl;
    http_intr_resume(intr);
    return new_id;
}

s32 http_template_copy(const HttpTemplate *src, HttpTemplate *dst)
{
    s32 ret;

    if (src == NULL || dst == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    sceKernelMemset(dst, 0, sizeof(*dst));
    dst->id = src->id;
    dst->http_version = src->http_version;
    dst->use_default_proxy = src->use_default_proxy;
    dst->resolve_timeout = src->resolve_timeout;
    dst->resolve_retry = src->resolve_retry;
    dst->keep_alive = src->keep_alive;
    dst->auto_redirect = src->auto_redirect;
    dst->cache_mode = src->cache_mode;
    dst->cookie_enabled = src->cookie_enabled;
    dst->auth_enabled = src->auth_enabled;
    dst->proxy_auth_enabled = src->proxy_auth_enabled;
    dst->allow_http09 = src->allow_http09;
    dst->ssl_flags = src->ssl_flags;
    dst->ssl_version = src->ssl_version;
    dst->connect_timeout = src->connect_timeout;
    dst->recv_timeout = src->recv_timeout;
    dst->send_timeout = src->send_timeout;
    dst->res_header_max_size = src->res_header_max_size;
    dst->recv_block_size = src->recv_block_size;
    dst->cache_content_max_size = src->cache_content_max_size;
    dst->max_redirects = src->max_redirects;
    dst->max_auth_retries = src->max_auth_retries;
    dst->use_proxy = src->use_proxy;
    dst->proxy_port = src->proxy_port;
    dst->tcp_nodelay = src->tcp_nodelay;
    dst->auth_cb_legacy = src->auth_cb_legacy;
    dst->auth_cb = src->auth_cb;
    dst->auth_cb_arg = src->auth_cb_arg;
    dst->redirect_cb = src->redirect_cb;
    dst->redirect_cb_arg = src->redirect_cb_arg;
    dst->cookie_send_cb = src->cookie_send_cb;
    dst->cookie_send_cb_arg = src->cookie_send_cb_arg;
    dst->cookie_recv_cb = src->cookie_recv_cb;
    dst->cookie_recv_cb_arg = src->cookie_recv_cb_arg;
    dst->ssl_cb = src->ssl_cb;
    dst->ssl_cb_arg = src->ssl_cb_arg;

    do {
        if (src->proxy_server != NULL) {
            dst->proxy_server = (char *)http_malloc(sceNet_lib_D8722983(src->proxy_server) + 1);
            if (dst->proxy_server == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(dst->proxy_server, src->proxy_server);
        }
        if (src->auth_user != NULL && sceNet_lib_D8722983(src->auth_user) != 0) {
            dst->auth_user = (char *)http_malloc(sceNet_lib_D8722983(src->auth_user) + 1);
            if (dst->auth_user == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(dst->auth_user, src->auth_user);
        }
        if (src->auth_pass != NULL && sceNet_lib_D8722983(src->auth_pass) != 0) {
            dst->auth_pass = (char *)http_malloc(sceNet_lib_D8722983(src->auth_pass) + 1);
            if (dst->auth_pass == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(dst->auth_pass, src->auth_pass);
        }

        ret = http_header_list_clone(&dst->headers, src->headers);
        if (ret >= 0)
            return 0;
    } while (0);

    http_template_free_fields(dst);
    return ret;
}

void http_template_list_init(void)
{
    sceKernelMemset(&g_tmpl_list_head, 0, sizeof(g_tmpl_list_head));
    g_tmpl_list_head.id = HTTP_HANDLE_TEMPLATE;
}

/* -------------------------------------------------------------------------
 * Utility helpers (URI, line reading, u64 conversion, handle classification)
 * ------------------------------------------------------------------------- */
void http_strip_proxy_host(char *host_buf)
{
    SceUriElement uri;
    u32 work_size;
    void *uri_work;

    if (host_buf == NULL || sceUriParse(NULL, host_buf, NULL, &work_size, 0) < 0)
        return;

    uri_work = http_malloc(work_size);
    if (uri_work == NULL)
        return;

    if (sceUriParse(&uri, host_buf, uri_work, NULL, work_size) >= 0) {
        uri.opaque = 1;
        if (sceUriBuild(NULL, &work_size, 0, &uri, 2) >= 0 &&
            sceNet_lib_D8722983(host_buf) + 1 >= work_size) {
            sceUriBuild(host_buf, NULL, work_size, &uri, 2);
        }
    }
    http_free(uri_work);
}

s32 http_skip_crlf_line(HttpRequest *req, s32 flags)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    u8 ch;
    s32 count = 0;
    s32 saw_cr = 0;

    for (;;) {
        s32 n = http_recv_raw(req, &ch, 1, flags);
        if (n < 0)
            return n;
        if (n == 0) {
            http_close_connection(conn);
            return SCE_HTTP_ERROR_BAD_RESPONSE;
        }
        count++;
        if (saw_cr && ch == '\n')
            return count;
        saw_cr = (ch == '\r');
    }
}

s32 http_parse_scheme(const char *scheme)
{
    if (scheme == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;
    if (sceNet_lib_9CFBC7E3(scheme, "HTTP") == 0)
        return HTTP_SCHEME_HTTP;
    if (sceNet_lib_9CFBC7E3(scheme, "HTTPS") == 0)
        return HTTP_SCHEME_HTTPS;
    return SCE_HTTP_ERROR_UNKNOWN_SCHEME;
}

s32 http_drain_response_body(HttpRequest *req)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    u8 discard[128];

    if (req->method != HTTP_METHOD_HEAD) {
        if (req->res_info.has_content_length == 1) {
            u64 rem = req->res_info.content_length - req->res_read_bytes;
            while (rem != 0) {
                u32 chunk = rem < sizeof(discard) ? (u32)rem : sizeof(discard);
                s32 n = http_recv_raw(req, discard, chunk, 0);
                if (n < 0) {
                    req->chunk_remaining = 0;
                    return 0;
                }
                if (n == 0)
                    break;
                rem -= (u32)n;
            }
        } else {
            for (;;) {
                s32 n = (req->res_info.is_chunked == 1)
                        ? http_recv_chunked(req, discard, sizeof(discard), 0)
                        : http_recv_raw(req, discard, sizeof(discard), 0);
                if (n < 0) {
                    req->chunk_remaining = 0;
                    return 0;
                }
                if (n == 0 || req->state == HTTP_REQ_DONE)
                    break;
            }
        }
    }

    if (req->res_info.conn_close == 1) {
        http_close_connection(conn);
    } else {
        conn->pipelined_count--;
        conn->current_req_id = req->next_pipelined_id;
    }
    req->chunk_remaining = 0;
    return 0;
}

s32 http_recv_response_header(HttpRequest *req)
{
    char *buf = req->res_header_buf;
    u32 max_len = req->cfg.res_header_max_size;
    u32 pos = 0;
    s32 state = 0; /* 0: text, 1: \r, 2: \n or \r\n, 3: \r\n\r, 4: done */

    if (req->cfg.allow_http09 == 1) {
        s32 peek_n = http_peek_raw(req, buf, 5, 0);
        if (peek_n != SCE_HTTP_ERROR_OUT_OF_SIZE) {
            if (peek_n <= 0)
                return SCE_HTTP_ERROR_BAD_RESPONSE;
            if (sceNet_lib_B9085A96(buf, "HTTP/", (u32)peek_n) != 0) {
                sceNet_lib_80C9F02A(buf, "HTTP/0.9 200 OK\r\n\r\n");
                req->res_header_len = sceNet_lib_D8722983("HTTP/0.9 200 OK\r\n\r\n") + 1;
                return 0;
            }
        }
    }

    while (pos < max_len) {
        s32 n = http_recv_raw(req, &buf[pos], 1, 0);
        char ch;

        if (n == 0) {
            buf[pos] = '\0';
            return SCE_HTTP_ERROR_BAD_RESPONSE;
        }
        if (n < 0)
            return n;

        ch = buf[pos];
        if (state == 0) {
            if (ch == '\r')      state = 1;
            else if (ch == '\n') state = 2;
        } else if (state == 1) {
            state = (ch == '\n') ? 2 : 0;
        } else if (state == 2) {
            if (ch == '\r')      state = 3;
            else if (ch == '\n') state = 4;
            else                 state = 0;
        } else if (state == 3) {
            state = (ch == '\n') ? 4 : 0;
        }

        if (state == 4) {
            req->res_header_len = pos + 1;
            return 0;
        }

        if (pos == 1499) {
            if (max_len >= 1501) {
                char *bigger = (char *)http_malloc(max_len);
                if (bigger == NULL)
                    return SCE_HTTP_ERROR_OUT_OF_MEMORY;
                sceKernelMemcpy(bigger, buf, 1498);
                http_free(buf);
                buf = bigger;
                req->res_header_buf = bigger;
                pos++;
            } else {
                pos = 1500;
            }
        } else {
            pos++;
        }
    }

    return SCE_HTTP_ERROR_BAD_RESPONSE;
}

u64 http_parse_u64(const char *str)
{
    u64 val = 0;
    u32 idx = 0;

    if (str == NULL)
        return 0;
    while (str[idx] != '\0') {
        if ((sceNet_lib_750F705D((s8)str[idx]) & 8) != 0)
            break;
        if (idx >= 21)
            break;
        val = val * 10ULL + (s8)str[idx] - '0';
        idx++;
    }
    return val;
}

static void http_format_u64_rec(char **dst, u64 val)
{
    if (val != 0) {
        http_format_u64_rec(dst, val / 10ULL);
        **dst = (char)((val % 10ULL) + '0');
        (*dst)++;
    }
}

void http_format_u64(char *dst, u64 val)
{
    char *ptr = dst;

    if (val == 0) {
        sceNet_lib_80C9F02A(dst, "0");
        return;
    }
    http_format_u64_rec(&ptr, val / 10ULL);
    *ptr++ = (char)((val % 10ULL) + '0');
}

void http_copy_ssl_errors(HttpConnection *conn, HttpRequest *req)
{
    if (conn->ssl_err != 0) {
        req->ssl_err = conn->ssl_err;
        conn->ssl_err = 0;
    }
    if (conn->ssl_cert_flags != 0) {
        req->ssl_cert_flags = conn->ssl_cert_flags;
        conn->ssl_cert_flags = 0;
    }
}

s32 http_handle_type(s32 id)
{
    u32 kind = (u32)id & HTTP_HANDLE_MASK;

    if (kind == HTTP_HANDLE_TEMPLATE ||
        kind == HTTP_HANDLE_CONNECTION ||
        kind == HTTP_HANDLE_REQUEST)
        return (s32)kind;
    return SCE_HTTP_ERROR_INVALID_ID;
}

s32 http_storage_read_tlv_hdr(s32 id, u8 *tag_out, u16 *len_out)
{
    s32 ret = http_storage_read_exact(id, tag_out, 1);
    if (ret >= 0)
        ret = http_storage_read_exact(id, len_out, 2);
    return ret;
}

s32 http_storage_write_tlv_hdr(s32 id, u8 tag, u16 len)
{
    s32 ret = http_storage_write_exact(id, &tag, 1);
    if (ret >= 0)
        ret = http_storage_write_exact(id, &len, 2);
    return ret;
}

/* -------------------------------------------------------------------------
 * HTTP response cache (memory & fixed-pool modes)
 * ------------------------------------------------------------------------- */
HTTP_ALWAYS_INLINE s32 http_is_pool_ptr(const void *ptr)
{
    u32 addr = (u32)ptr;
    return g_cache_pool_inited == 1 &&
           addr >= g_cache_pool_base &&
           addr < g_cache_pool_base + g_cache_pool_size;
}

s32 sceHttpInitCache(u32 max_size)
{
    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_cache_mem_inited == 1)
        return SCE_HTTP_ERROR_ALREADY_INITED;

    sceKernelMemset(g_cache_mem_buckets, 0, sizeof(g_cache_mem_buckets));
    g_cache_mem_inited = 1;
    g_cache_mem_max_size = max_size;
    g_cache_mem_cur_size = 0;
    g_cache_mem_head = NULL;
    g_cache_mem_tail = NULL;
    return 0;
}

s32 sceHttpFlushCache(void)
{
    HttpCacheEntry *cur;
    s32 intr;

    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_cache_mem_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    intr = http_intr_suspend();
    cur = g_cache_mem_head;
    while (cur != NULL) {
        HttpCacheEntry *next = cur->lru_next;
        http_cache_mem_entry_mark_deleted(cur);
        cur = next;
    }
    http_intr_resume(intr);
    return 0;
}

s32 sceHttpEndCache(void)
{
    HttpCacheEntry *cur;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_cache_mem_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    cur = g_cache_mem_head;
    while (cur != NULL) {
        HttpCacheEntry *next = cur->lru_next;
        http_cache_data_free(cur->data);
        http_free(cur->url);
        http_free(cur);
        cur = next;
    }

    g_cache_mem_inited = 0;
    g_cache_mem_max_size = 0;
    g_cache_mem_cur_size = 0;
    g_cache_mem_head = NULL;
    g_cache_mem_tail = NULL;
    sceKernelMemset(g_cache_mem_buckets, 0, sizeof(g_cache_mem_buckets));
    return 0;
}

s32 sceHttp_739C2D79(void *pool_mem, u32 pool_size)
{
    u32 off;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_cache_pool_inited == 1)
        return SCE_HTTP_ERROR_ALREADY_INITED;

    sceKernelMemset(g_cache_pool_buckets, 0, sizeof(g_cache_pool_buckets));
    g_cache_pool_inited = 1;
    g_cache_pool_base = (u32)pool_mem;
    g_cache_pool_size = pool_size;
    g_cache_pool_used_blocks = 0;
    g_cache_pool_free_list = NULL;
    g_cache_pool_head = NULL;
    g_cache_pool_tail = NULL;

    sceKernelMemset(pool_mem, 0, pool_size);
    for (off = 0; off + 512 <= pool_size; off += 512) {
        HttpCacheBlock *blk = (HttpCacheBlock *)((u8 *)pool_mem + off);
        blk->next = g_cache_pool_free_list;
        g_cache_pool_free_list = blk;
    }
    return 0;
}

s32 sceHttp_B0257723(void)
{
    HttpCacheEntry *cur;
    s32 intr;

    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_cache_pool_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    intr = http_intr_suspend();
    cur = g_cache_pool_head;
    while (cur != NULL) {
        HttpCacheEntry *next = cur->lru_next;
        http_cache_pool_entry_mark_deleted(cur);
        cur = next;
    }
    http_intr_resume(intr);
    return 0;
}

s32 sceHttp_A461A167(void)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_cache_pool_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    g_cache_pool_inited = 0;
    g_cache_pool_base = 0;
    g_cache_pool_size = 0;
    g_cache_pool_used_blocks = 0;
    g_cache_pool_free_list = NULL;
    g_cache_pool_head = NULL;
    g_cache_pool_tail = NULL;
    sceKernelMemset(g_cache_pool_buckets, 0, sizeof(g_cache_pool_buckets));
    return 0;
}

void http_cache_entry_addref(HttpCacheEntry *entry)
{
    entry->hit_count++;
    entry->ref_count++;
}

HttpCacheEntry *http_cache_lookup_req(HttpRequest *req)
{
    HttpCacheEntry *entry = NULL;
    char *url = http_build_request_url(req);

    if (url != NULL) {
        s32 intr = http_intr_suspend();
        if (req->cfg.cache_mode == HTTP_CACHE_MEM)
            entry = http_cache_hash_find(g_cache_mem_buckets, url, req->method, NULL);
        else if (req->cfg.cache_mode == HTTP_CACHE_POOL)
            entry = http_cache_hash_find(g_cache_pool_buckets, url, req->method, NULL);
        http_intr_resume(intr);
        http_free(url);
    }
    return entry;
}

HttpCacheEntry *http_cache_lookup(s32 mode, const char *url, s32 method, const u8 *extra_md5)
{
    HttpCacheEntry *entry = NULL;
    s32 intr = http_intr_suspend();

    if (mode == HTTP_CACHE_MEM)
        entry = http_cache_hash_find(g_cache_mem_buckets, url, method, extra_md5);
    else if (mode == HTTP_CACHE_POOL)
        entry = http_cache_hash_find(g_cache_pool_buckets, url, method, extra_md5);
    http_intr_resume(intr);
    return entry;
}

HttpCacheEntry *http_cache_begin_write(HttpRequest *req)
{
    char *val = NULL;
    u32 val_len = 0;

    if (req->res_info.has_content_length == 1) {
        if ((req->res_info.content_length >> 32) != 0)
            return NULL;
        if (req->cfg.cache_content_max_size < (u32)req->res_info.content_length)
            return NULL;
    }

    if (sceParseHttpResponseHeader(req->res_header_buf, req->res_header_len, "Pragma", &val, &val_len) > 0)
        return NULL;
    if (sceParseHttpResponseHeader(req->res_header_buf, req->res_header_len, "Cache-Control", &val, &val_len) > 0)
        return NULL;

    if (req->cfg.cache_mode == HTTP_CACHE_MEM) {
        if (req->res_info.has_content_length == 1 && req->res_info.content_length != 0)
            return http_cache_mem_alloc_entry(req);
        return NULL;
    }
    if (req->cfg.cache_mode == HTTP_CACHE_POOL)
        return http_cache_pool_alloc_entry(req);
    return NULL;
}

void http_cache_commit_write(HttpRequest *req)
{
    char *url;
    s32 is_pool;
    HttpCacheEntry *existing;
    s32 intr;

    if (req->cache_write_entry == NULL)
        return;

    url = http_build_request_url(req);
    if (url != NULL) {
        intr = http_intr_suspend();
        is_pool = http_is_pool_ptr(req->cache_write_entry);
        existing = http_cache_hash_find(is_pool ? g_cache_pool_buckets : g_cache_mem_buckets,
                                        url, req->method, NULL);
        if (existing != NULL) {
            if (http_is_pool_ptr(existing))
                http_cache_pool_entry_release(existing);
            else
                http_cache_mem_entry_release(existing);
        } else if (is_pool) {
            HttpCacheEntry *entry = req->cache_write_entry;
            http_cache_hash_insert(g_cache_pool_buckets, entry);
            entry->lru_prev = NULL;
            entry->lru_next = g_cache_pool_head;
            if (g_cache_pool_head != NULL)
                g_cache_pool_head->lru_prev = entry;
            else
                g_cache_pool_tail = entry;
            g_cache_pool_head = entry;
            req->cache_write_entry = NULL;
        } else {
            HttpCacheEntry *entry = req->cache_write_entry;
            http_cache_hash_insert(g_cache_mem_buckets, entry);
            entry->lru_prev = NULL;
            entry->lru_next = g_cache_mem_head;
            if (g_cache_mem_head != NULL)
                g_cache_mem_head->lru_prev = entry;
            else
                g_cache_mem_tail = entry;
            g_cache_mem_head = entry;
            req->cache_write_entry = NULL;
        }
        http_intr_resume(intr);
        http_free(url);
    }

    if (req->cache_write_entry != NULL) {
        http_cache_abort_write(req->cache_write_entry);
        req->cache_write_entry = NULL;
    }
}

void http_cache_abort_write(HttpCacheEntry *entry)
{
    if (http_is_pool_ptr(entry)) {
        http_cache_pool_free_data(entry->data);
        *(void **)entry = NULL;
        http_cache_pool_free_chain((HttpCacheBlock *)entry);
    } else {
        http_cache_mem_account_free(entry->data);
        http_cache_data_free(entry->data);
        http_free(entry->url);
        http_free(entry);
    }
}

u32 http_cache_read_bytes(void *dst, void **src_ptr, u32 offset, u32 len)
{
    HttpCacheBlock *blk;
    u32 total = 0;

    if (!http_is_pool_ptr(src_ptr)) {
        sceKernelMemcpy(dst, (u8 *)(*src_ptr) + offset, len);
        return len;
    }

    blk = (HttpCacheBlock *)(*src_ptr);
    while (blk != NULL && offset >= blk->used) {
        offset -= blk->used;
        blk = blk->next;
    }

    while (blk != NULL && len != 0) {
        u32 avail = blk->used - offset;
        u32 chunk = avail < len ? avail : len;
        sceKernelMemcpy((u8 *)dst + total, blk->data + offset, chunk);
        total += chunk;
        len -= chunk;
        offset = 0;
        blk = blk->next;
    }
    return total;
}

u32 http_cache_write_bytes(void **dst_ptr, u32 offset, const void *src, u32 len)
{
    HttpCacheBlock *blk;
    u32 total = 0;

    if (!http_is_pool_ptr(dst_ptr)) {
        sceKernelMemcpy((u8 *)(*dst_ptr) + offset, src, len);
        return len;
    }

    blk = (HttpCacheBlock *)(*dst_ptr);
    if (blk == NULL) {
        blk = http_cache_pool_alloc_block();
    } else {
        while (blk->next != NULL)
            blk = blk->next;
    }

    while (blk != NULL && len != 0) {
        u32 space = sizeof(blk->data) - blk->used;
        u32 chunk = space < len ? space : len;
        sceKernelMemcpy(blk->data + blk->used, (const u8 *)src + total, chunk);
        blk->used += chunk;
        total += chunk;
        len -= chunk;
        if (len != 0) {
            blk->next = http_cache_pool_alloc_block();
            blk = blk->next;
        }
    }
    return total;
}

void http_cache_data_free(HttpCacheData *data)
{
    if (data->extra_buf != NULL)  http_free(data->extra_buf);
    if (data->header_buf != NULL) http_free(data->header_buf);
    if (data->body_buf != NULL)   http_free(data->body_buf);
    http_free(data);
}

void http_cache_mem_account_free(const HttpCacheData *data)
{
    g_cache_mem_cur_size -= data->header_size + data->body_size;
}

HttpCacheEntry *http_cache_mem_alloc_entry(HttpRequest *req)
{
    u32 reserve_size = req->res_header_len + (u32)req->res_info.content_length;
    char *url = NULL;
    HttpCacheEntry *entry = NULL;
    HttpCacheData *data = NULL;
    s32 reserved;
    s32 intr;

    intr = http_intr_suspend();
    reserved = http_cache_mem_reserve(reserve_size);
    http_intr_resume(intr);
    if (!reserved)
        return NULL;

    do {
        url = http_build_request_url(req);
        if (url == NULL)
            break;

        entry = (HttpCacheEntry *)http_malloc(sizeof(*entry));
        if (entry == NULL)
            break;

        data = (HttpCacheData *)http_malloc(sizeof(*data));
        if (data == NULL)
            break;
        sceKernelMemset(data, 0, sizeof(*data));

        if (req->res_header_len != 0) {
            data->header_size = req->res_header_len;
            data->header_buf = http_malloc(req->res_header_len);
            if (data->header_buf == NULL)
                break;
        }

        if (req->res_info.content_length != 0) {
            if ((req->res_info.content_length >> 32) != 0)
                break;
            data->body_size = (u32)req->res_info.content_length;
            data->body_buf = http_malloc(data->body_size);
            if (data->body_buf == NULL)
                break;
        }

        sceKernelMemset(entry, 0, sizeof(*entry));
        http_md5_digest(url, sceNet_lib_D8722983(url), entry->url_md5);
        entry->url = url;
        entry->method = req->method;
        entry->data = data;
        return entry;
    } while (0);

    if (reserved)
        g_cache_mem_cur_size -= reserve_size;
    if (url != NULL)
        http_free(url);
    if (entry != NULL)
        http_free(entry);
    if (data != NULL)
        http_cache_data_free(data);
    return NULL;
}

HttpCacheBlock *http_cache_pool_alloc_block(void)
{
    HttpCacheBlock *blk;
    s32 intr = http_intr_suspend();

    while ((blk = g_cache_pool_free_list) == NULL) {
        HttpCacheEntry *victim = g_cache_pool_tail;
        while (victim != NULL && victim->ref_count != 0)
            victim = victim->lru_prev;
        if (victim == NULL)
            break;
        http_cache_pool_entry_mark_deleted(victim);
    }

    if (blk != NULL) {
        g_cache_pool_free_list = blk->next;
        blk->next = NULL;
    }
    http_intr_resume(intr);

    if (blk != NULL) {
        sceKernelMemset(blk, 0, sizeof(*blk));
        g_cache_pool_used_blocks++;
    }
    return blk;
}

void http_cache_pool_free_chain(HttpCacheBlock *head)
{
    HttpCacheBlock *tail;
    s32 intr;

    if (head == NULL)
        return;

    tail = head;
    g_cache_pool_used_blocks--;
    while (tail->next != NULL) {
        tail = tail->next;
        g_cache_pool_used_blocks--;
    }

    intr = http_intr_suspend();
    tail->next = g_cache_pool_free_list;
    g_cache_pool_free_list = head;
    http_intr_resume(intr);
}

void http_cache_pool_free_data(HttpCacheData *data)
{
    if (data->extra_buf != NULL)
        http_cache_pool_free_chain((HttpCacheBlock *)data->extra_buf);
    if (data->header_buf != NULL)
        http_cache_pool_free_chain((HttpCacheBlock *)data->header_buf);
    if (data->body_buf != NULL)
        http_cache_pool_free_chain((HttpCacheBlock *)data->body_buf);
}

HTTP_ALWAYS_INLINE void http_cache_pool_unlink_and_free(HttpCacheEntry *entry)
{
    http_cache_hash_remove(g_cache_pool_buckets, entry);
    if (entry->lru_prev != NULL) {
        entry->lru_prev->lru_next = entry->lru_next;
        if (entry->lru_next != NULL)
            entry->lru_next->lru_prev = entry->lru_prev;
        else
            g_cache_pool_tail = entry->lru_prev;
    } else {
        g_cache_pool_head = entry->lru_next;
        if (entry->lru_next != NULL)
            entry->lru_next->lru_prev = NULL;
        else
            g_cache_pool_tail = NULL;
    }

    http_cache_pool_free_data(entry->data);
    *(void **)entry = NULL;
    http_cache_pool_free_chain((HttpCacheBlock *)entry);
}

void http_cache_pool_entry_release(HttpCacheEntry *entry)
{
    entry->ref_count--;
    if (entry->ref_count == 0 && entry->deleted == 1)
        http_cache_pool_unlink_and_free(entry);
}

void http_cache_pool_entry_mark_deleted(HttpCacheEntry *entry)
{
    entry->deleted = 1;
    if (entry->ref_count == 0)
        http_cache_pool_unlink_and_free(entry);
}

HttpCacheEntry *http_cache_pool_alloc_entry(HttpRequest *req)
{
    char *url = http_build_request_url(req);
    HttpCacheBlock *blk;
    HttpCacheEntry *entry;
    HttpCacheData *data;

    if (url == NULL)
        return NULL;

    if (sceNet_lib_D8722983(url) + 1 >= 401) {
        http_free(url);
        return NULL;
    }

    blk = http_cache_pool_alloc_block();
    if (blk == NULL) {
        http_free(url);
        return NULL;
    }

    entry = (HttpCacheEntry *)blk;
    data = (HttpCacheData *)((u8 *)blk + 72);
    entry->url = (char *)((u8 *)blk + 112);

    do {
        if (req->res_header_len != 0) {
            data->header_size = req->res_header_len;
            data->header_buf = http_cache_pool_alloc_block();
            if (data->header_buf == NULL)
                break;
        }

        if (!(req->res_info.has_content_length == 1 && req->res_info.content_length == 0)) {
            data->body_size = (u32)req->res_info.content_length;
            data->body_buf = http_cache_pool_alloc_block();
            if (data->body_buf == NULL)
                break;
        }

        http_md5_digest(url, sceNet_lib_D8722983(url), entry->url_md5);
        sceNet_lib_80C9F02A(entry->url, url);
        entry->data = data;
        entry->method = req->method;
        http_free(url);
        return entry;
    } while (0);

    http_cache_pool_free_data(data);
    *(void **)entry = NULL;
    http_cache_pool_free_chain(blk);
    http_free(url);
    return NULL;
}

void http_cache_entry_release_locked(HttpCacheEntry *entry)
{
    s32 intr = http_intr_suspend();
    if (http_is_pool_ptr(entry))
        http_cache_pool_entry_release(entry);
    else
        http_cache_mem_entry_release(entry);
    http_intr_resume(intr);
}

void http_cache_entry_invalidate_locked(HttpCacheEntry *entry)
{
    s32 intr = http_intr_suspend();
    if (http_is_pool_ptr(entry)) {
        http_cache_pool_entry_mark_deleted(entry);
        http_cache_pool_entry_release(entry);
    } else {
        http_cache_mem_entry_mark_deleted(entry);
        http_cache_mem_entry_release(entry);
    }
    http_intr_resume(intr);
}

s32 http_cache_mem_is_inited(void)     { return g_cache_mem_inited; }
u32 http_cache_mem_get_max_size(void)  { return g_cache_mem_max_size; }
s32 http_cache_pool_is_inited(void)    { return g_cache_pool_inited; }
u32 http_cache_pool_get_max_size(void) { return g_cache_pool_size; }

void http_cache_hash_insert(HttpCacheEntry **buckets, HttpCacheEntry *entry)
{
    u32 idx = entry->url_md5[0] >> 4;
    HttpCacheEntry *prev = NULL;
    HttpCacheEntry *cur = buckets[idx];

    while (cur != NULL) {
        if (sceNet_lib_E0A81C7C(cur->url_md5, entry->url_md5, 16) >= 0)
            break;
        prev = cur;
        cur = cur->hash_next;
    }

    if (prev != NULL) {
        entry->hash_next = prev->hash_next;
        prev->hash_next = entry;
    } else {
        entry->hash_next = buckets[idx];
        buckets[idx] = entry;
    }
}

void http_cache_hash_remove(HttpCacheEntry **buckets, HttpCacheEntry *entry)
{
    u32 idx = entry->url_md5[0] >> 4;
    HttpCacheEntry *prev = NULL;
    HttpCacheEntry *cur = buckets[idx];

    while (cur != NULL) {
        if (cur == entry) {
            if (prev != NULL)
                prev->hash_next = cur->hash_next;
            else
                buckets[idx] = cur->hash_next;
            cur->hash_next = NULL;
            return;
        }
        prev = cur;
        cur = cur->hash_next;
    }
}

HTTP_ALWAYS_INLINE void http_cache_lru_promote(HttpCacheEntry *entry, HttpCacheEntry **head, HttpCacheEntry **tail)
{
    if (entry->lru_prev != NULL) {
        entry->lru_prev->lru_next = entry->lru_next;
        if (entry->lru_next != NULL)
            entry->lru_next->lru_prev = entry->lru_prev;
        else
            *tail = entry->lru_prev;
    } else {
        *head = entry->lru_next;
        if (entry->lru_next != NULL)
            entry->lru_next->lru_prev = NULL;
        else
            *tail = NULL;
    }

    entry->lru_prev = NULL;
    entry->lru_next = *head;
    if (*head != NULL)
        (*head)->lru_prev = entry;
    else
        *tail = entry;
    *head = entry;
}

HttpCacheEntry *http_cache_hash_find(HttpCacheEntry **buckets, const char *url, s32 method, const u8 *extra_md5)
{
    u8 digest[16];
    HttpCacheEntry *cur;

    http_md5_digest(url, sceNet_lib_D8722983(url), digest);
    cur = buckets[digest[0] >> 4];
    while (cur != NULL) {
        s32 cmp = sceNet_lib_E0A81C7C(cur->url_md5, digest, 16);
        if (cmp > 0)
            return NULL;
        if (cmp == 0 &&
            sceNet_lib_A0F16ABD(cur->url, url) == 0 &&
            cur->method == method &&
            (extra_md5 == NULL || sceNet_lib_E0A81C7C(cur->extra_md5, extra_md5, 16) == 0) &&
            cur->deleted != 1) {
            http_cache_entry_addref(cur);
            if (http_is_pool_ptr(cur))
                http_cache_lru_promote(cur, &g_cache_pool_head, &g_cache_pool_tail);
            else
                http_cache_lru_promote(cur, &g_cache_mem_head, &g_cache_mem_tail);
            return cur;
        }
        cur = cur->hash_next;
    }
    return NULL;
}

s32 http_cache_mem_reserve(u32 needed)
{
    while (g_cache_mem_max_size < g_cache_mem_cur_size + needed) {
        HttpCacheEntry *victim = g_cache_mem_tail;
        while (victim != NULL && victim->ref_count != 0)
            victim = victim->lru_prev;
        if (victim == NULL)
            return 0;
        http_cache_mem_entry_mark_deleted(victim);
    }
    g_cache_mem_cur_size += needed;
    return 1;
}

/* -------------------------------------------------------------------------
 * Request query APIs
 * ------------------------------------------------------------------------- */
HTTP_ALWAYS_INLINE s32 http_ensure_response_ready(HttpRequest *req)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;

    if (req->state == HTTP_REQ_CREATED)
        return SCE_HTTP_ERROR_BEFORE_SEND;
    if (req->state == HTTP_REQ_ERROR)
        return SCE_HTTP_ERROR_NETWORK;
    if (req->state == HTTP_REQ_ABORTED)
        return SCE_HTTP_ERROR_ABORTED;
    if (req->state == HTTP_REQ_SENT) {
        if (conn->current_req_id != req->id)
            return SCE_HTTP_ERROR_NOT_CURRENT_REQ;
        return http_process_response(req);
    }
    return 0;
}

s32 sceHttpGetContentLength(s32 req_id, u64 *len_out)
{
    HttpRequest *req = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 10208)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_lock_request(req_id, &req);
    if (ret >= 0) {
        ret = http_ensure_response_ready(req);
        if (ret >= 0) {
            if (req->res_info.has_content_length != 0) {
                *len_out = req->res_info.content_length;
                ret = 0;
            } else {
                ret = (req->res_info.is_chunked == 1)
                      ? SCE_HTTP_ERROR_CHUNK_ENC : SCE_HTTP_ERROR_NO_CONTENT_LENGTH;
            }
        }
    }

    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return ret;
}

s32 sceHttpGetStatusCode(s32 req_id, s32 *code_out)
{
    HttpRequest *req = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 10208)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_lock_request(req_id, &req);
    if (ret >= 0) {
        ret = http_ensure_response_ready(req);
        if (ret >= 0) {
            *code_out = req->res_info.status_code;
            ret = 0;
        }
    }

    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return ret;
}

s32 sceHttpGetNetworkErrno(s32 req_id, s32 *errno_out)
{
    HttpRequest *req = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_lock_request(req_id, &req);
    if (ret >= 0) {
        *errno_out = req->net_errno;
        ret = 0;
    }

    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return ret;
}

s32 sceHttpGetNetworkPspError(s32 req_id, s32 *err_out)
{
    HttpRequest *req = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_lock_request(req_id, &req);
    if (ret >= 0) {
        *err_out = req->net_psp_err;
        ret = 0;
    }

    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return ret;
}

s32 sceHttpGetAllHeader(s32 req_id, char **hdr_out, u32 *len_out)
{
    HttpRequest *req = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 10208)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_lock_request(req_id, &req);
    if (ret >= 0) {
        HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
        switch (req->state) {
        case HTTP_REQ_CREATED:
            ret = SCE_HTTP_ERROR_BEFORE_SEND;
            break;
        case HTTP_REQ_ERROR:
            ret = SCE_HTTP_ERROR_NETWORK;
            break;
        case HTTP_REQ_ABORTED:
            ret = SCE_HTTP_ERROR_ABORTED;
            break;
        case HTTP_REQ_SENT:
            if (conn->current_req_id != req->id)
                ret = SCE_HTTP_ERROR_NOT_CURRENT_REQ;
            else
                ret = http_process_response(req);
            break;
        default:
            break;
        }
        if (ret >= 0) {
            if (req->all_header_buf == NULL)
                ret = http_copy_all_response_headers(req);
            if (ret >= 0) {
                *hdr_out = req->all_header_buf;
                *len_out = req->all_header_len;
            }
        }
    }

    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return ret;
}

/* -------------------------------------------------------------------------
 * Request & response header serialization / parsing
 * ------------------------------------------------------------------------- */
s32 http_load_cached_response_header(HttpRequest *req)
{
    if (req->cache_read_entry->data->header_size >= 1500) {
        char *buf = (char *)http_malloc(req->cache_read_entry->data->header_size + 1);
        if (buf == NULL)
            return SCE_HTTP_ERROR_OUT_OF_MEMORY;
        http_free(req->res_header_buf);
        req->res_header_buf = buf;
    }

    http_cache_read_bytes(req->res_header_buf, &req->cache_read_entry->data->header_buf,
                          0, req->cache_read_entry->data->header_size);
    req->res_header_buf[req->cache_read_entry->data->header_size] = '\0';
    req->res_header_len = req->cache_read_entry->data->header_size;
    http_parse_response_fields(req->res_header_buf, req->res_header_len, &req->res_info);
    req->res_info.has_content_length = 1;
    req->res_info.content_length = req->cache_read_entry->data->body_size;
    return 0;
}

s32 http_attach_request_cookies(HttpRequest *req)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    HttpCookieSortChunk *chunks = NULL;
    HttpCookieRefNode *refs = NULL;
    u8 *cookie_buf = NULL;
    char *url_str = NULL;
    u32 cookie_len = 0;
    s32 ret;

    ret = http_cookie_jar_lookup(&chunks, conn->host, req->path, conn->scheme, &refs);
    if (ret >= 0) {
        ret = http_cookie_format_header(NULL, chunks, &cookie_len, 0);
        if (ret >= 0) {
            cookie_buf = (u8 *)http_malloc(cookie_len);
            if (cookie_buf == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
            } else {
                ret = http_cookie_format_header(cookie_buf, chunks, NULL, cookie_len);
                if (ret >= 0) {
                    if (req->cfg.cookie_send_cb != NULL) {
                        typedef s32 (*CookieSendCb)(s32, const char *, const char *, void *);
                        url_str = http_build_request_url(req);
                        sceKernelCheckThreadStack();
                        ret = ((CookieSendCb)req->cfg.cookie_send_cb)(req->id, url_str,
                                                                      (const char *)cookie_buf,
                                                                      req->cfg.cookie_send_cb_arg);
                    }
                    if (ret >= 0)
                        http_add_header_internal(req->id, "Cookie", (const char *)cookie_buf, 0);
                }
            }
        }
    }

    if (chunks != NULL || refs != NULL)
        http_cookie_lookup_free(chunks, refs);
    if (url_str != NULL)
        http_free(url_str);
    if (cookie_buf != NULL)
        http_free(cookie_buf);
    return ret;
}

s32 http_attach_basic_auth_header(HttpRequest *req, const char *user, const char *pass, s32 is_proxy)
{
    u32 req_len = 0;
    u8 *buf;
    s32 ret = http_build_basic_auth(NULL, &req_len, 0, user, pass);

    if (ret < 0)
        return ret;
    buf = (u8 *)http_malloc(req_len);
    if (buf == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    ret = http_build_basic_auth(buf, NULL, req_len, user, pass);
    if (ret >= 0) {
        const char *hdr = is_proxy ? "Proxy-Authorization" : "Authorization";
        ret = http_add_header_internal(req->id, hdr, (const char *)buf, 0);
    }
    http_free(buf);
    return ret;
}

s32 http_attach_digest_auth_header(HttpRequest *req, const char *user, const char *pass,
                                   const void *entity, u32 entity_len,
                                   const HttpDigestParams *params, s32 is_proxy)
{
    u32 req_len = 0;
    char *buf;
    s32 ret = http_build_digest_auth(NULL, &req_len, 0, user, pass, req->path,
                                     req->method, entity, entity_len, params);

    if (ret < 0)
        return ret;
    buf = (char *)http_malloc(req_len);
    if (buf == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    ret = http_build_digest_auth(buf, NULL, req_len, user, pass, req->path,
                                 req->method, entity, entity_len, params);
    if (ret >= 0) {
        const char *hdr = is_proxy ? "Proxy-Authorization" : "Authorization";
        ret = http_add_header_internal(req->id, hdr, buf, 0);
    }
    http_free(buf);
    return ret;
}

s32 http_header_node_alloc(HttpHeaderNode **out_node, const char *name, const char *value)
{
    HttpHeaderNode *node;
    u32 name_len;
    u32 val_len;

    if (out_node == NULL || name == NULL || value == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    *out_node = NULL;
    node = (HttpHeaderNode *)http_malloc(sizeof(*node));
    if (node == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceKernelMemset(node, 0, sizeof(*node));

    name_len = sceNet_lib_D8722983(name);
    node->name = (char *)http_malloc(name_len + 1);
    if (node->name != NULL) {
        sceKernelMemcpy(node->name, name, name_len);
        node->name[name_len] = '\0';

        val_len = sceNet_lib_D8722983(value);
        node->value = (char *)http_malloc(val_len + 1);
        if (node->value != NULL) {
            sceKernelMemcpy(node->value, value, val_len);
            node->value[val_len] = '\0';

            *out_node = node;
            return 0;
        }
    }

    if (node->name != NULL)  http_free(node->name);
    if (node->value != NULL) http_free(node->value);
    http_free(node);
    return SCE_HTTP_ERROR_OUT_OF_MEMORY;
}

void http_header_list_free(HttpHeaderNode *list)
{
    while (list != NULL) {
        HttpHeaderNode *next = list->next;
        if (list->name != NULL)  http_free(list->name);
        if (list->value != NULL) http_free(list->value);
        http_free(list);
        list = next;
    }
}

u32 http_serialize_request_headers(HttpRequest *req, char *dst, s32 dry_run)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    SceUriElement uri;
    const char *method_str;
    const char *conn_hdr;
    HttpHeaderNode *hdr;
    u32 uri_flags;
    u32 uri_len = 0;
    u32 total = 0;

    sceKernelMemset(&uri, 0, sizeof(uri));
    switch (req->method) {
    case HTTP_METHOD_GET:     method_str = "GET"; break;
    case HTTP_METHOD_POST:    method_str = "POST"; break;
    case HTTP_METHOD_HEAD:    method_str = "HEAD"; break;
    case HTTP_METHOD_OPTIONS: method_str = "OPTIONS"; break;
    case HTTP_METHOD_PUT:     method_str = "PUT"; break;
    case HTTP_METHOD_DELETE:  method_str = "DELETE"; break;
    case HTTP_METHOD_TRACE:   method_str = "TRACE"; break;
    case HTTP_METHOD_CONNECT: method_str = "CONNECT"; break;
    default:                  return 0;
    }

    total += sceNet_lib_D8722983(method_str) + 1;
    if (!dry_run) {
        sceNet_lib_5705F6F9(dst, method_str);
        sceNet_lib_5705F6F9(dst, " ");
    }

    uri.host = conn->host;
    uri.port = conn->port;
    if (req->method != HTTP_METHOD_CONNECT) {
        uri.path = req->path;
        uri.query = req->query;
        uri.fragment = req->fragment;
    }
    if (uri.path == NULL || sceNet_lib_D8722983(uri.path) == 0 ||
        (sceNet_lib_D8722983(uri.path) == 1 && uri.path[0] == ' ')) {
        uri.path = "/";
    }

    if (req->method == HTTP_METHOD_CONNECT) {
        uri.opaque = 1;
        uri_flags = 6;
    } else if (conn->cfg.use_proxy == 1 && conn->cfg.proxy_server != NULL &&
               conn->scheme == HTTP_SCHEME_HTTP) {
        uri.scheme = "http";
        uri_flags = 207;
    } else {
        uri.opaque = 1;
        uri_flags = 200;
    }

    sceUriBuild(NULL, &uri_len, 0, &uri, uri_flags);
    total += uri_len;
    if (!dry_run)
        sceUriBuild(dst + sceNet_lib_D8722983(dst), NULL, uri_len, &uri, uri_flags);

    total += 1 + 8 + 2; /* " HTTP/1.x\r\n" */
    if (!dry_run) {
        sceNet_lib_5705F6F9(dst, " ");
        sceNet_lib_5705F6F9(dst, req->cfg.http_version != 0 ? "HTTP/1.1" : "HTTP/1.0");
        sceNet_lib_5705F6F9(dst, "\r\n");
    }

    for (hdr = req->cfg.headers; hdr != NULL; hdr = hdr->next) {
        if (req->method == HTTP_METHOD_CONNECT) {
            if (sceNet_lib_9CFBC7E3(hdr->name, "Host") != 0 &&
                sceNet_lib_9CFBC7E3(hdr->name, "Proxy-Authorization") != 0)
                continue;
        } else if (conn->scheme == HTTP_SCHEME_HTTPS &&
                   sceNet_lib_9CFBC7E3(hdr->name, "Proxy-Authorization") == 0) {
            continue;
        }

        total += sceNet_lib_D8722983(hdr->name) + 2;
        if (!dry_run) {
            sceNet_lib_5705F6F9(dst, hdr->name);
            sceNet_lib_5705F6F9(dst, ": ");
        }
        if (hdr->value != NULL) {
            total += sceNet_lib_D8722983(hdr->value);
            if (!dry_run)
                sceNet_lib_5705F6F9(dst, hdr->value);
        }
        total += 2;
        if (!dry_run)
            sceNet_lib_5705F6F9(dst, "\r\n");
    }

    {
        s32 use_proxy_conn = (conn->cfg.use_proxy == 1 &&
                              (conn->scheme == HTTP_SCHEME_HTTP || req->method == HTTP_METHOD_CONNECT));
        if (conn->cfg.keep_alive != 0)
            conn_hdr = use_proxy_conn ? "Proxy-Connection: Keep-Alive" : "Connection: Keep-Alive";
        else
            conn_hdr = use_proxy_conn ? "Proxy-Connection: close" : "Connection: close";
    }

    total += sceNet_lib_D8722983(conn_hdr) + 2;
    if (!dry_run) {
        sceNet_lib_5705F6F9(dst, conn_hdr);
        sceNet_lib_5705F6F9(dst, "\r\n");
    }

    if (req->method == HTTP_METHOD_POST || req->method == HTTP_METHOD_PUT) {
        char len_buf[32];
        sceKernelMemset(len_buf, 0, 21);
        http_format_u64(len_buf, req->req_content_len);
        total += 16 + sceNet_lib_D8722983(len_buf) + 2;
        if (!dry_run) {
            sceNet_lib_5705F6F9(dst, "Content-Length: ");
            sceNet_lib_5705F6F9(dst, len_buf);
            sceNet_lib_5705F6F9(dst, "\r\n");
        }
    }

    total += 3; /* "\r\n\0" */
    if (!dry_run)
        sceNet_lib_5705F6F9(dst, "\r\n");
    return total;
}

s32 http_header_list_clone(HttpHeaderNode **dst_head, const HttpHeaderNode *src)
{
    HttpHeaderNode *head;
    HttpHeaderNode *cur;
    s32 ret = 0;
    s32 intr;

    if (src == NULL) {
        *dst_head = NULL;
        return 0;
    }

    head = (HttpHeaderNode *)http_malloc(sizeof(*head));
    if (head == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceKernelMemset(head, 0, sizeof(*head));

    cur = head;
    intr = http_intr_suspend();
    while (src != NULL) {
        if (src->name != NULL) {
            cur->name = (char *)http_malloc(sceNet_lib_D8722983(src->name) + 1);
            if (cur->name == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(cur->name, src->name);
        }
        if (src->value != NULL) {
            cur->value = (char *)http_malloc(sceNet_lib_D8722983(src->value) + 1);
            if (cur->value == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(cur->value, src->value);
        }
        if (src->next != NULL) {
            cur->next = (HttpHeaderNode *)http_malloc(sizeof(*cur->next));
            if (cur->next == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceKernelMemset(cur->next, 0, sizeof(*cur->next));
        }
        src = src->next;
        cur = cur->next;
    }
    http_intr_resume(intr);

    if (ret < 0) {
        http_header_list_free(head);
        return ret;
    }
    *dst_head = head;
    return 0;
}

s32 http_parse_response_fields(const char *hdr, u32 hdr_len, HttpResponseInfo *info)
{
    s32 ret = sceParseHttpStatusLine(hdr, hdr_len,
                                     &info->http_major, &info->http_minor,
                                     &info->status_code,
                                     &info->reason_ptr, &info->reason_len);
    if (ret < 0)
        return ret;

    sceParseHttpResponseHeader(hdr, hdr_len, "Date", &info->date_ptr, &info->date_len);
    sceParseHttpResponseHeader(hdr, hdr_len, "Server", &info->server_ptr, &info->server_len);
    sceParseHttpResponseHeader(hdr, hdr_len, "ETag", &info->etag_ptr, &info->etag_len);
    sceParseHttpResponseHeader(hdr, hdr_len, "Last-Modifiled", &info->last_mod_ptr, &info->last_mod_len);
    sceParseHttpResponseHeader(hdr, hdr_len, "Location", &info->location_ptr, &info->location_len);
    sceParseHttpResponseHeader(hdr, hdr_len, "WWW-Authenticate", &info->www_auth_ptr, &info->www_auth_len);
    return 0;
}

HTTP_ALWAYS_INLINE void http_finalize_auth_slot(HttpAuthEntry **slot, s32 rejected)
{
    HttpAuthEntry *entry = *slot;
    if (entry == NULL)
        return;

    if (entry->status == 0) {
        if (rejected) {
            http_auth_entry_free_unused(entry);
        } else {
            entry->status = 2;
            http_auth_cache_insert(entry);
        }
    } else {
        entry->status = rejected ? 1 : 2;
        http_auth_entry_release_locked(entry);
    }
    *slot = NULL;
}

s32 http_recv_and_parse_headers(HttpRequest *req, HttpConnection *conn)
{
    s32 ret;

    do {
        char *val = NULL;
        u32 val_len = 0;
        const char *conn_field;

        ret = http_recv_response_header(req);
        if (ret < 0)
            return ret;
        ret = http_parse_response_fields(req->res_header_buf, req->res_header_len, &req->res_info);
        if (ret < 0)
            return ret;

        if (sceParseHttpResponseHeader(req->res_header_buf, req->res_header_len,
                                       "Content-Length", &val, &val_len) > 0) {
            char len_buf[32];
            if (val_len >= 22)
                val_len = 21;
            sceKernelMemcpy(len_buf, val, val_len);
            len_buf[val_len] = '\0';
            req->res_info.has_content_length = 1;
            req->res_info.is_chunked = 0;
            req->res_info.content_length = http_parse_u64(len_buf);
        } else {
            req->res_info.has_content_length = 0;
        }

        if (sceParseHttpResponseHeader(req->res_header_buf, req->res_header_len,
                                       "Transfer-Encoding", &val, &val_len) > 0 &&
            sceNet_lib_B9085A96(val, "chunked", 7) == 0) {
            req->res_info.is_chunked = 1;
            req->res_info.content_length = 0;
            req->res_info.has_content_length = 0;
        } else {
            req->res_info.is_chunked = 0;
        }

        req->res_info.conn_close = 0;
        conn->close_on_complete = 0;
        if (conn->cfg.use_proxy == 1 &&
            (conn->scheme == HTTP_SCHEME_HTTP || req->method == HTTP_METHOD_CONNECT))
            conn_field = "Proxy-Connection";
        else
            conn_field = "Connection";

        if (sceParseHttpResponseHeader(req->res_header_buf, req->res_header_len,
                                       conn_field, &req->res_info.conn_hdr_ptr,
                                       &req->res_info.conn_hdr_len) >= 0) {
            if (sceNet_lib_B9085A96(req->res_info.conn_hdr_ptr, "close",
                                    req->res_info.conn_hdr_len) == 0) {
                req->res_info.conn_close = 1;
                conn->close_on_complete = 1;
            }
        } else if ((conn->cfg.use_proxy != 0 && conn->scheme != HTTP_SCHEME_HTTPS) ||
                   req->res_info.http_major != 1 || req->res_info.http_minor != 1) {
            req->res_info.conn_close = 1;
            conn->close_on_complete = 1;
        }
    } while (req->res_info.status_code >= 100 && req->res_info.status_code < 103);

    if (req->cfg.auth_enabled != 0)
        http_finalize_auth_slot(&req->auth_entry, req->res_info.status_code == 401);
    if (req->cfg.proxy_auth_enabled != 0)
        http_finalize_auth_slot(&req->proxy_auth_entry, req->res_info.status_code == 407);
    return 0;
}

s32 http_process_response(HttpRequest *req)
{
    HttpConnection *conn;
    s32 ret;

    if (req == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    ret = http_recv_and_parse_headers(req, conn);
    if (ret < 0)
        return ret;

    req->state = HTTP_REQ_RECEIVING;
    if (req->method == HTTP_METHOD_HEAD ||
        (req->res_info.has_content_length != 0 && req->res_info.content_length == 0)) {
        conn->pipelined_count--;
        conn->current_req_id = req->next_pipelined_id;
        if (req->res_info.conn_close == 1)
            http_close_connection(conn);
        req->state = HTTP_REQ_DONE;
    }

    if (req->cfg.cookie_enabled != 0)
        http_cookie_process_response(req->res_header_buf, req->res_header_len,
                                     conn->host, req->path, conn->port, req);

    ret = http_handle_redirect_and_auth(req);
    if (ret < 0)
        return ret;

    if (req->state == HTTP_REQ_SENT || req->cfg.cache_mode == HTTP_CACHE_NONE || req->cache_hit_state != 0)
        return (s32)req->res_header_len;

    if (req->cache_read_entry != NULL) {
        if (req->res_info.status_code == 304) {
            ret = http_load_cached_response_header(req);
            if (ret < 0)
                return ret;
            req->cache_hit_state = 2;
            req->cache_read_pos = 0;
            return (s32)req->res_header_len;
        }
        http_cache_entry_invalidate_locked(req->cache_read_entry);
        req->cache_read_entry = NULL;
    }

    if (req->res_info.status_code == 200 && req->method == HTTP_METHOD_GET &&
        http_header_list_find(req->cfg.headers, "Range") == NULL &&
        http_header_list_find(req->cfg.headers, "Content-Range") == NULL) {
        HttpCacheEntry *wentry = http_cache_begin_write(req);
        req->cache_write_entry = wentry;
        if (wentry != NULL) {
            if (http_cache_write_bytes(&wentry->data->header_buf, 0,
                                       req->res_header_buf, req->res_header_len) == req->res_header_len) {
                http_rtc_get_tick(&wentry->data->timestamp);
            } else {
                http_cache_abort_write(wentry);
                req->cache_write_entry = NULL;
            }
        }
    }

    req->cache_hit_state = 1;
    return (s32)req->res_header_len;
}

const char *http_header_list_find(const HttpHeaderNode *list, const char *name)
{
    if (list == NULL || name == NULL)
        return NULL;
    while (list != NULL) {
        if (sceNet_lib_9CFBC7E3(list->name, name) == 0)
            return list->value;
        list = list->next;
    }
    return NULL;
}

s32 http_format_host_header(char **out_str, s32 scheme, const char *host, u16 port)
{
    SceUriElement uri;
    char *host_copy = NULL;
    char *built = NULL;
    u32 req_len;
    s32 ret;

    sceKernelMemset(&uri, 0, sizeof(uri));
    req_len = sceNet_lib_D8722983(host) + 1;
    host_copy = (char *)http_malloc(req_len);
    if (host_copy == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceNet_lib_80C9F02A(host_copy, host);

    uri.opaque = 1;
    uri.scheme = (scheme == HTTP_SCHEME_HTTPS) ? "https" : "http";
    uri.host = host_copy;
    uri.port = port;

    ret = sceUriBuild(NULL, &req_len, 0, &uri, 6);
    if (ret >= 0) {
        built = (char *)http_malloc(req_len);
        if (built != NULL) {
            ret = sceUriBuild(built, NULL, req_len, &uri, 6);
            if (ret >= 0) {
                http_free(host_copy);
                *out_str = built;
                return 0;
            }
        }
    }

    http_free(host_copy);
    if (built != NULL)
        http_free(built);
    return ret;
}

s32 http_build_request_header_buf(char **out_buf, HttpRequest *req)
{
    s32 intr = http_intr_suspend();
    u32 needed = http_serialize_request_headers(req, NULL, 1);
    char *buf = (char *)http_malloc(needed);
    s32 ret = 0;

    if (buf != NULL) {
        sceKernelMemset(buf, 0, needed);
        http_serialize_request_headers(req, buf, 0);
    } else {
        ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
    }
    http_intr_resume(intr);

    if (ret >= 0) {
        *out_buf = buf;
        return 0;
    }
    if (buf != NULL)
        http_free(buf);
    return ret;
}

s32 http_copy_all_response_headers(HttpRequest *req)
{
    char *buf = (char *)http_malloc(req->res_header_len + 1);
    req->all_header_buf = buf;
    if (buf == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    sceKernelMemcpy(buf, req->res_header_buf, req->res_header_len);
    req->all_header_buf[req->res_header_len] = '\0';
    req->all_header_len = req->res_header_len + 1;
    return 0;
}

/* -------------------------------------------------------------------------
 * Thin syscall / library wrappers & persistent storage
 * ------------------------------------------------------------------------- */
s32  http_intr_suspend(void)                                    { return sceKernelCpuSuspendIntr(); }
void http_intr_resume(s32 intr)                                 { sceKernelCpuResumeIntr(intr); }
s32  http_md5_digest(const void *data, u32 len, u8 *out)        { return sceKernelUtilsMd5Digest(data, len, out); }
s32  http_md5_init(void *ctx)                                   { return sceKernelUtilsMd5BlockInit(ctx); }
s32  http_md5_update(void *ctx, const void *data, u32 len)      { return sceKernelUtilsMd5BlockUpdate(ctx, data, len); }
s32  http_md5_result(void *ctx, u8 *out)                        { return sceKernelUtilsMd5BlockResult(ctx, out); }
s32  http_inet_pton(s32 af, const char *src, void *dst)         { return sceNetInetInetPton(af, src, dst); }
s32  http_rtc_get_tick(u64 *tick)                               { return sceRtcGetCurrentTick(tick); }
s32  http_rtc_add_seconds(u64 *dst, const u64 *src, s64 secs)   { return sceRtcTickAddSeconds(dst, src, secs); }
s32  http_rtc_parse_datetime(u64 *tick, const char *str)        { return sceRtcParseDateTime(tick, str); }
s32  http_rtc_format_rfc2822(char *dst, const u64 *tick, s32 tz){ return sceRtcFormatRFC2822(dst, tick, tz); }

s32 http_storage_reset_file(s32 id)
{
    u32 zero_len = 0;
    s32 ret = sceHttpStorage_driver_700AAD44(id, 0x02000602, 0644);
    if (ret < 0 ||
        (ret = sceHttpStorage_driver_24AA94F4(id, &zero_len, 4)) < 0 ||
        (ret = sceHttpStorage_driver_24AA94F4(id, g_md5_empty_digest, 16)) < 0) {
        sceHttpStorage_driver_CDA3D8F6(id);
        return ret;
    }
    sceHttpStorage_driver_CDA3D8F6(id);
    return 0;
}

s32 http_storage_open_body(s32 id, s32 flags, s32 mode)
{
    s32 ret = sceHttpStorage_driver_700AAD44(id, flags, mode);
    if (ret >= 0)
        ret = sceHttpStorage_driver_B33389CE(id, 20, 0);
    if (ret < 0)
        sceHttpStorage_driver_CDA3D8F6(id);
    return ret;
}

typedef struct HttpStorageStat {
    u32 mode;
    u32 attr;
    s64 size;
    u8  rest[48];
} HttpStorageStat;

s32 http_storage_verify(s32 id, u32 *size_out)
{
    HttpStorageStat st;
    u32 body_size = 0;
    u8 expected_md5[16];
    u8 actual_md5[16];
    u8 md5_ctx[96];
    u32 rem;

    do {
        s32 failed = 0;

        if (sceHttpStorage_driver_2D8DAE58(id, &st) < 0)
            break;
        if (sceHttpStorage_driver_700AAD44(id, 1, 0) < 0)
            break;
        if (sceHttpStorage_driver_CDDF1103(id, &body_size, 4) < 0)
            break;
        if (st.size < 20 || (u64)(st.size - 20) < (u64)body_size)
            break;
        if (sceHttpStorage_driver_CDDF1103(id, expected_md5, 16) < 0)
            break;
        if (sceHttpStorage_driver_B33389CE(id, 20, 0) < 0)
            break;
        if (http_md5_init(md5_ctx) < 0)
            break;

        rem = body_size;
        while (rem != 0) {
            u32 chunk = rem < sizeof(g_storage_md5_buf) ? rem : sizeof(g_storage_md5_buf);
            if (sceHttpStorage_driver_CDDF1103(id, g_storage_md5_buf, (s32)chunk) < 0 ||
                http_md5_update(md5_ctx, g_storage_md5_buf, chunk) < 0) {
                failed = 1;
                break;
            }
            rem -= chunk;
        }
        if (failed)
            break;

        if (http_md5_result(md5_ctx, actual_md5) >= 0 &&
            sceNet_lib_E0A81C7C(expected_md5, actual_md5, 16) == 0) {
            *size_out = body_size;
            sceHttpStorage_driver_CDA3D8F6(id);
            return 1;
        }
    } while (0);

    sceHttpStorage_driver_CDA3D8F6(id);
    return 0;
}

s32 http_storage_commit_checksum(s32 id, u32 body_size)
{
    HttpStorageStat st;
    u8 digest[16];
    u8 md5_ctx[96];
    u32 rem;
    s32 ret;

    do {
        ret = sceHttpStorage_driver_2D8DAE58(id, &st);
        if (ret < 0)
            break;
        if (st.size < 20 || (u64)(st.size - 20) < (u64)body_size) {
            ret = -1;
            break;
        }

        ret = sceHttpStorage_driver_700AAD44(id, 3, 0);
        if (ret < 0) break;
        ret = sceHttpStorage_driver_B33389CE(id, 20, 0);
        if (ret < 0) break;
        ret = http_md5_init(md5_ctx);
        if (ret < 0) break;

        rem = body_size;
        while (rem != 0) {
            u32 chunk = rem < sizeof(g_storage_md5_buf) ? rem : sizeof(g_storage_md5_buf);
            ret = sceHttpStorage_driver_CDDF1103(id, g_storage_md5_buf, (s32)chunk);
            if (ret < 0) break;
            ret = http_md5_update(md5_ctx, g_storage_md5_buf, chunk);
            if (ret < 0) break;
            rem -= chunk;
        }
        if (ret < 0) break;

        ret = http_md5_result(md5_ctx, digest);
        if (ret < 0) break;
        ret = sceHttpStorage_driver_B33389CE(id, 0, 0);
        if (ret < 0) break;
        ret = sceHttpStorage_driver_24AA94F4(id, &body_size, 4);
        if (ret < 0) break;
        ret = sceHttpStorage_driver_24AA94F4(id, digest, 16);
    } while (0);

    sceHttpStorage_driver_CDA3D8F6(id);
    return ret;
}

s32 http_storage_lock(void)
{
    s32 ret = 0;
    s32 intr = http_intr_suspend();

    if (g_storage_locked <= 0)
        g_storage_locked = 1;
    else
        ret = SCE_HTTP_ERROR_BUSY;
    http_intr_resume(intr);
    return ret;
}

void http_storage_unlock(void)
{
    s32 intr = http_intr_suspend();
    g_storage_locked = 0;
    http_intr_resume(intr);
}

s32 http_storage_close(s32 id)
{
    return sceHttpStorage_driver_CDA3D8F6(id);
}

s32 http_storage_read_exact(s32 id, void *buf, s32 len)
{
    s32 ret = sceHttpStorage_driver_CDDF1103(id, buf, len);
    if (ret >= 0 && ret != len)
        return -1;
    return ret;
}

s32 http_storage_write_exact(s32 id, const void *buf, s32 len)
{
    s32 ret = sceHttpStorage_driver_24AA94F4(id, buf, len);
    if (ret >= 0 && ret != len)
        return -1;
    return ret;
}

s32 http_storage_seek(s32 id, s64 offset, s32 whence)
{
    return sceHttpStorage_driver_B33389CE(id, offset, whence);
}
