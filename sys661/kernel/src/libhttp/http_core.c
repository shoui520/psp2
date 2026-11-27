#include "libhttp_in.h"

/* -------------------------------------------------------------------------
 * Read-only tables & auth cache globals
 * ------------------------------------------------------------------------- */
const u8 g_oid_subject_alt_name[4] __attribute__((aligned(4))) = {
    0x55, 0x1D, 0x11, 0x00,
};

const u8 g_b64_decode_tab[256] __attribute__((aligned(4))) = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, 0xFF, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF,
    0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

const u8 g_md5_empty_digest[16] __attribute__((aligned(4))) = {
    0xD4, 0x1D, 0x8C, 0xD9, 0x8F, 0x00, 0xB2, 0x04,
    0xE9, 0x80, 0x09, 0x98, 0xEC, 0xF8, 0x42, 0x7E,
};

static const char g_b64_encode_tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

s32            g_auth_inited;
HttpAuthEntry  g_auth_pool[16];
HttpAuthEntry *g_auth_free_list;
HttpAuthEntry *g_auth_head;
HttpAuthEntry *g_auth_tail;

/* -------------------------------------------------------------------------
 * Base64 codec
 * ------------------------------------------------------------------------- */
s32 http_base64_decode(u8 *dst, const u8 *src, s32 src_len)
{
    s32 out_len = 0;

    while (src_len > 0) {
        u8 ch_c = src[2];
        u8 ch_d = src[3];
        s8 dec_a = (s8)g_b64_decode_tab[src[0]];
        s8 dec_b = (s8)g_b64_decode_tab[src[1]];
        s8 dec_c = (s8)g_b64_decode_tab[ch_c];
        s8 dec_d = (s8)g_b64_decode_tab[ch_d];
        u32 bits;

        if (ch_c == '=')
            out_len--;
        if (ch_d == '=')
            out_len--;

        if (dec_a < 0 || dec_b < 0 || dec_c < 0 || dec_d < 0)
            return -1;

        bits = ((u32)dec_a << 18) | ((u32)dec_b << 12) | ((u32)dec_c << 6) | (u32)dec_d;
        dst[0] = (u8)(bits >> 16);
        dst[1] = (u8)(bits >> 8);
        dst[2] = (u8)bits;

        src += 4;
        dst += 3;
        src_len -= 4;
        out_len += 3;
    }

    *dst = '\0';
    return out_len;
}

s32 http_base64_encode(char *dst, const u8 *src, u32 src_len)
{
    s32 out_len = 0;

    while ((s32)src_len > 0) {
        u32 octet_a = src[0];
        u32 octet_b = src_len >= 2 ? src[1] : 0;
        u32 octet_c = src_len >= 3 ? src[2] : 0;
        u32 bits = (octet_a << 16) | (octet_b << 8) | octet_c;

        dst[0] = g_b64_encode_tab[(bits >> 18) & 0x3F];
        dst[1] = g_b64_encode_tab[(bits >> 12) & 0x3F];
        dst[2] = src_len >= 2 ? g_b64_encode_tab[(bits >> 6) & 0x3F] : '=';
        dst[3] = src_len >= 3 ? g_b64_encode_tab[bits & 0x3F] : '=';

        src += 3;
        dst += 4;
        src_len -= 3;
        out_len += 4;
    }

    *dst = '\0';
    return out_len;
}

/* -------------------------------------------------------------------------
 * Request sending / reading / aborting
 * ------------------------------------------------------------------------- */
s32 http_send_request_core(HttpRequest *req)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    char *hdr_buf = NULL;
    char date_buf[48];
    s32 owns_conn = 0;
    s32 ret = 0;
    s32 intr;

    if (conn->scheme == HTTP_SCHEME_HTTPS && http_ssl_is_inited() == 0)
        return SCE_HTTP_ERROR_SSL_NOT_INITED;

    if (conn->state == HTTP_CONN_PROXY_TUNNEL)
        http_close_connection(conn);

    intr = http_intr_suspend();
    if (conn->state == HTTP_CONN_CLOSING || conn->state == HTTP_CONN_CONNECTING ||
        conn->state == HTTP_CONN_SENDING || conn->state == HTTP_CONN_RESOLVING ||
        conn->close_on_complete == 1) {
        ret = SCE_HTTP_ERROR_BUSY;
    } else {
        switch (conn->state) {
        case HTTP_CONN_IDLE:
            owns_conn = 1;
            conn->state = HTTP_CONN_CONNECTING;
            break;
        case HTTP_CONN_PROXY_TUNNEL:
            owns_conn = 1;
            break;
        case HTTP_CONN_ESTABLISHED:
            if (conn->cfg.keep_alive == 1) {
                owns_conn = 1;
                if (req->method == HTTP_METHOD_POST || req->method == HTTP_METHOD_PUT)
                    conn->state = HTTP_CONN_SENDING_BODY;
                else
                    conn->state = HTTP_CONN_SENDING;
            } else {
                ret = SCE_HTTP_ERROR_BUSY;
            }
            break;
        case HTTP_CONN_SENDING_BODY:
            if (conn->active_post_req_id == req->id)
                req->sending_body = 1;
            else if (conn->active_post_req_id != 0)
                ret = SCE_HTTP_ERROR_BUSY;
            break;
        default:
            break;
        }
    }
    http_intr_resume(intr);

    do {
        if (ret < 0)
            break;

        if (req->cfg.cache_mode != HTTP_CACHE_NONE && req->method == HTTP_METHOD_GET &&
            http_header_list_find(req->cfg.headers, "Range") == NULL &&
            http_header_list_find(req->cfg.headers, "Content-Range") == NULL) {
            HttpCacheEntry *cached = http_cache_lookup_req(req);
            req->cache_read_entry = cached;
            if (cached != NULL) {
                ret = http_rtc_format_rfc2822(date_buf, &cached->data->timestamp, 0);
                if (ret > 0)
                    break;
                ret = http_add_header_internal(req->id, "If-Modified-Since", date_buf, 0);
                if (ret < 0)
                    break;
            }
        }

        if (req->sending_body != 1) {
            if (conn->cfg.use_default_proxy == 1)
                http_refresh_apctl_proxy(&conn->cfg);
            http_apply_cached_proxy_auth(req, conn);
            http_apply_cached_auth(req);
            if (req->cfg.cookie_enabled != 0)
                http_attach_request_cookies(req);
            ret = http_build_request_header_buf(&hdr_buf, req);
            if (ret < 0)
                break;
        }

        ret = http_ensure_connected(req);
        if (ret == SCE_HTTP_ERROR_NETWORK) {
            req->net_errno = conn->net_errno;
            req->net_psp_err = conn->net_psp_err;
        }
        if (ret < 0)
            break;

        if (req->state == HTTP_REQ_CREATED) {
            u32 hdr_len = sceNet_lib_D8722983(hdr_buf);
            ret = http_transport_send(conn, hdr_buf, hdr_len, 0);
            if (ret == SCE_HTTP_ERROR_SSL) {
                http_copy_ssl_errors(conn, req);
                break;
            }
            if (ret < 0) {
                req->net_errno = sceNetInetGetErrno();
                req->net_psp_err = sceNetInetGetPspError();
                conn->net_errno = req->net_errno;
                conn->net_psp_err = req->net_psp_err;
                ret = SCE_HTTP_ERROR_NETWORK;
                break;
            }
            if (req->method == HTTP_METHOD_POST || req->method == HTTP_METHOD_PUT)
                req->state = HTTP_REQ_SENDING_BODY;
            else
                req->state = HTTP_REQ_SENT;
        }

        if (req->state == HTTP_REQ_SENDING_BODY && req->method == HTTP_METHOD_POST &&
            req->post_data != NULL && req->post_chunk_len != 0) {
            u64 remaining = req->req_content_len - req->req_sent_bytes;
            if ((u64)req->post_chunk_len < remaining) {
                req->req_sent_bytes += req->post_chunk_len;
                conn->active_post_req_id = req->id;
            } else {
                req->post_chunk_len = (u32)remaining;
                req->req_sent_bytes = req->req_content_len;
                req->state = HTTP_REQ_SENT;
                conn->active_post_req_id = 0;
            }

            ret = http_transport_send(conn, req->post_data, req->post_chunk_len, 0);
            if (ret == SCE_HTTP_ERROR_SSL) {
                http_copy_ssl_errors(conn, req);
                req->state = HTTP_REQ_EOF;
                http_close_connection(conn);
                break;
            }
            if (ret < 0) {
                req->net_errno = sceNetInetGetErrno();
                req->net_psp_err = sceNetInetGetPspError();
                conn->net_errno = req->net_errno;
                conn->net_psp_err = req->net_psp_err;
                req->state = HTTP_REQ_ERROR;
                http_close_connection(conn);
                ret = SCE_HTTP_ERROR_NETWORK;
                break;
            }
        } else if (req->req_content_len == 0 && conn->state == HTTP_CONN_SENDING_BODY) {
            req->state = HTTP_REQ_SENT;
        }

        if (req->state == HTTP_REQ_SENT)
            conn->state = HTTP_CONN_ESTABLISHED;

        {
            s32 pipelined = 0;
            intr = http_intr_suspend();
            if (conn->active_post_req_id == 0)
                conn->pipelined_count++;

            if (conn->pipelined_count < 2) {
                conn->tail_req_id = req->id;
                conn->current_req_id = req->id;
            } else {
                HttpRequest *tail_req = NULL;
                pipelined = 1;
                http_lock_request(conn->tail_req_id, &tail_req);
                req->state = HTTP_REQ_SENT;
                tail_req->next_pipelined_id = req->id;
                tail_req->ref_count--;
                req->prev_pipelined_id = tail_req->id;
                conn->tail_req_id = req->id;
            }
            http_intr_resume(intr);

            if (req->state == HTTP_REQ_SENT && !pipelined)
                ret = http_process_response(req);
        }
    } while (0);

    if (owns_conn && ret < 0 && conn->state != HTTP_CONN_PROXY_TUNNEL)
        http_close_connection(conn);
    if (hdr_buf != NULL)
        http_free(hdr_buf);
    if (ret == SCE_HTTP_ERROR_NETWORK && req->net_errno == 11)
        ret = SCE_HTTP_ERROR_TIMEOUT;
    return http_min_s32(ret, 0);
}

s32 sceHttpIsRequestInCache(s32 req_id)
{
    HttpRequest *req = NULL;
    HttpRedirEntry *redir = NULL;
    s32 in_cache = 0;

    if (sceKernelCheckThreadStack() < 2000)
        return SCE_HTTP_ERROR_OUT_OF_STACK;

    if (http_is_inited() == 1 && http_lock_request(req_id, &req) >= 0) {
        if (req->state == HTTP_REQ_CREATED || req->state == HTTP_REQ_SENDING_BODY) {
            if (req->cfg.auto_redirect == 1)
                redir = http_follow_cached_redirects(req);

            if (req->cfg.cache_mode != HTTP_CACHE_NONE && req->method == HTTP_METHOD_GET &&
                http_header_list_find(req->cfg.headers, "Range") == NULL &&
                http_header_list_find(req->cfg.headers, "Content-Range") == NULL) {
                HttpCacheEntry *entry = NULL;
                if (redir != NULL) {
                    entry = http_cache_lookup(req->cfg.cache_mode, redir->dst_url, req->method, NULL);
                } else {
                    char *url = http_build_request_url(req);
                    if (url != NULL) {
                        entry = http_cache_lookup(req->cfg.cache_mode, url, req->method, NULL);
                        http_free(url);
                    }
                }
                if (entry != NULL) {
                    in_cache = 1;
                    http_cache_entry_release_locked(entry);
                }
            }

            if (redir != NULL) {
                req->redirect_count = 0;
                http_redir_entry_release_locked(redir);
            }
        }
    }

    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return in_cache;
}

s32 sceHttpSendRequestInCacheFirstMode(s32 req_id, const void *data, u32 size)
{
    HttpRequest *req = NULL;
    HttpRedirEntry *redir = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 10192)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_lock_request(req_id, &req);
    if (ret < 0)
        return ret;

    if (req->state == HTTP_REQ_PROXY_AUTH_FAIL)
        req->state = HTTP_REQ_CREATED;
    if (req->state != HTTP_REQ_CREATED && req->state != HTTP_REQ_SENDING_BODY && req->state != HTTP_REQ_RECEIVING)
        return SCE_HTTP_ERROR_AFTER_SEND;

    if (req->cfg.auto_redirect == 1)
        redir = http_follow_cached_redirects(req);

    do {
        if (req->cfg.cache_mode != HTTP_CACHE_NONE && req->method == HTTP_METHOD_GET &&
            http_header_list_find(req->cfg.headers, "Range") == NULL &&
            http_header_list_find(req->cfg.headers, "Content-Range") == NULL) {
            if (redir != NULL) {
                req->cache_read_entry = http_cache_lookup(req->cfg.cache_mode, redir->dst_url, req->method, NULL);
            } else {
                char *url = http_build_request_url(req);
                if (url == NULL)
                    break;
                req->cache_read_entry = http_cache_lookup(req->cfg.cache_mode, url, req->method, NULL);
                http_free(url);
            }

            if (req->cache_read_entry != NULL) {
                req->state = HTTP_REQ_RECEIVING;
                http_load_cached_response_header(req);
                ret = (s32)req->res_header_len;
                req->cache_hit_state = 2;
                req->cache_read_pos = 0;
                break;
            }
        }

        if (redir != NULL)
            req->redirect_count = 0;
        ret = http_send_request_internal(req_id, data, size);
    } while (0);

    if (redir != NULL)
        http_redir_entry_release_locked(redir);
    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return ret;
}

s32 sceHttpAbortRequest(s32 req_id)
{
    HttpRequest *req = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 10208)
        return SCE_HTTP_ERROR_OUT_OF_STACK;

    if (http_is_inited() != 1) {
        ret = SCE_HTTP_ERROR_BEFORE_INIT;
    } else {
        ret = http_lock_request(req_id, &req);
        if (ret >= 0) {
            HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
            if (conn->state == HTTP_CONN_IDLE) {
                ret = SCE_HTTP_ERROR_NOT_CONNECTED;
            } else if (conn->state == HTTP_CONN_RESOLVING) {
                ret = sceNetResolverStop(conn->resolver_id);
                if (ret < 0)
                    sceNetInetSocketAbort(conn->sock_fd);
            } else {
                ret = sceNetInetSocketAbort(conn->sock_fd);
                if (ret < 0) {
                    req->net_errno = sceNetInetGetErrno();
                    req->net_psp_err = sceNetInetGetPspError();
                    conn->net_errno = req->net_errno;
                    conn->net_psp_err = req->net_psp_err;
                    ret = SCE_HTTP_ERROR_NETWORK;
                }
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

s32 sceHttpReadData(s32 req_id, void *data, u32 size)
{
    HttpRequest *req = NULL;
    HttpConnection *conn;
    u32 total_read = 0;
    s32 hit_eof = 0;
    s32 ret;

    if (sceKernelCheckThreadStack() < 10176)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    do {
        s32 read_err = 0;

        ret = http_lock_request(req_id, &req);
        if (ret < 0)
            break;

        conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
        if (req->state == HTTP_REQ_CREATED) {
            ret = SCE_HTTP_ERROR_BEFORE_SEND;
            break;
        }
        if (req->state == HTTP_REQ_ERROR) {
            ret = SCE_HTTP_ERROR_NETWORK;
            break;
        }
        if (req->state == HTTP_REQ_ABORTED) {
            ret = SCE_HTTP_ERROR_ABORTED;
            break;
        }
        if (req->state == HTTP_REQ_SENT) {
            if (conn->current_req_id != req->id) {
                ret = SCE_HTTP_ERROR_NOT_CURRENT_REQ;
                break;
            }
            ret = http_process_response(req);
            if (ret < 0)
                break;
        }

        if (req->method == HTTP_METHOD_HEAD) {
            ret = SCE_HTTP_ERROR_HEAD_NO_BODY;
            break;
        }
        if (req->state == HTTP_REQ_PROXY_AUTH_FAIL)
            req->state = HTTP_REQ_RECEIVING;

        if (req->cache_hit_state == 2) {
            HttpCacheData *cdata = req->cache_read_entry->data;
            u32 avail = cdata->body_size - req->cache_read_pos;
            if (avail < size)
                size = avail;
            http_cache_read_bytes(data, &cdata->body_buf, req->cache_read_pos, size);
            req->cache_read_pos += size;
            ret = (s32)size;
            break;
        }

        if (req->state == HTTP_REQ_DONE) {
            ret = 0;
            break;
        }
        if (req->res_info.has_content_length == 1 && req->res_read_bytes == req->res_info.content_length) {
            ret = 0;
            break;
        }
        if (req->res_info.has_content_length == 0 && req->state == HTTP_REQ_EOF) {
            ret = 0;
            break;
        }

        while (total_read < size && req->state != HTTP_REQ_DONE) {
            u32 to_read = size - total_read;
            s32 n;

            if (req->res_info.has_content_length != 0) {
                if (req->res_read_bytes >= req->res_info.content_length)
                    break;
                {
                    u64 rem = req->res_info.content_length - req->res_read_bytes;
                    if (rem < (u64)to_read)
                        to_read = (u32)rem;
                }
            }

            if (req->res_info.is_chunked == 1)
                n = http_recv_chunked(req, (u8 *)data + total_read, to_read, 0);
            else
                n = http_recv_raw(req, (u8 *)data + total_read, to_read, 0);

            if (n < 0) {
                ret = n;
                read_err = 1;
                break;
            }
            if (n == 0) {
                hit_eof = 1;
                break;
            }
            total_read += (u32)n;
            req->res_read_bytes += (u32)n;
        }
        if (read_err)
            break;

        if (req->cache_write_entry != NULL) {
            HttpCacheData *wdata = req->cache_write_entry->data;
            s32 overflow = 0;

            if (req->res_info.has_content_length == 1) {
                if (wdata->body_size < wdata->written_size + total_read)
                    overflow = 1;
            } else {
                if (req->cfg.cache_content_max_size < wdata->body_size + total_read)
                    overflow = 1;
            }

            if (overflow || http_cache_write_bytes(&wdata->body_buf, wdata->written_size, data, total_read) != total_read) {
                http_cache_abort_write(req->cache_write_entry);
                req->cache_write_entry = NULL;
            } else {
                wdata->written_size += total_read;
                if (req->res_info.has_content_length != 1)
                    wdata->body_size += total_read;
            }
        }

        if ((req->res_info.has_content_length == 1 && req->res_read_bytes == req->res_info.content_length) ||
            req->state == HTTP_REQ_DONE) {
            s32 intr = http_intr_suspend();
            conn->pipelined_count--;
            conn->current_req_id = req->next_pipelined_id;
            http_intr_resume(intr);

            if (req->cache_write_entry != NULL)
                http_cache_commit_write(req);
            if (req->res_info.conn_close == 1)
                http_close_connection(conn);
            req->state = HTTP_REQ_DONE;
        }

        if ((req->res_info.has_content_length == 1 && req->res_read_bytes == req->res_info.content_length &&
             conn->cfg.keep_alive == 0) || hit_eof == 1) {
            http_close_connection(conn);
        }

        if (req->res_info.has_content_length == 1 && req->res_read_bytes == req->res_info.content_length &&
            req->force_close == 1) {
            http_close_connection(conn);
        }

        ret = (s32)total_read;
    } while (0);

    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return ret;
}

s32 sceHttpSendRequest(s32 req_id, const void *data, u32 size)
{
    if (sceKernelCheckThreadStack() < 10224)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_send_request_internal(req_id, data, size);
}

s32 sceHttpCreateTemplate(const char *user_agent, s32 http_ver, s32 auto_proxy)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_create_template_internal(user_agent, (u32)http_ver, auto_proxy);
}

s32 sceHttpDeleteTemplate(s32 tmpl_id)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_delete_template_internal(tmpl_id);
}

s32 sceHttpCreateConnection(s32 tmpl_id, const char *host, const char *scheme, u16 port, s32 keep_alive)
{
    if (sceKernelCheckThreadStack() < 2000)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_create_connection_internal(tmpl_id, host, scheme, NULL, NULL, port, keep_alive, 0, NULL);
}

s32 sceHttpCreateConnectionWithURL(s32 tmpl_id, const char *url, s32 keep_alive)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_create_connection_with_url_internal(tmpl_id, url, keep_alive, 0, NULL);
}

s32 sceHttpDeleteConnection(s32 conn_id)
{
    if (sceKernelCheckThreadStack() < 10224)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_delete_connection_internal(conn_id);
}

s32 sceHttpCreateRequest(s32 conn_id, s32 method, const char *path, u64 content_len)
{
    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_create_request_internal(conn_id, method, path, content_len);
}

s32 sceHttpCreateRequestWithURL(s32 conn_id, s32 method, const char *url, u64 content_len)
{
    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_create_request_with_url_internal(conn_id, method, url, content_len);
}

s32 sceHttpDeleteRequest(s32 req_id)
{
    if (sceKernelCheckThreadStack() < 10224)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_delete_request_internal(req_id);
}

s32 sceHttp_4E4A284A(s32 tmpl_id)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_clone_template_internal(tmpl_id);
}

s32 sceHttp_A909F2AE(void)
{
    return SCE_HTTP_ERROR_INVALID_VALUE;
}

/* -------------------------------------------------------------------------
 * Cached 301 redirect chain & send wrapper
 * ------------------------------------------------------------------------- */
HttpRedirEntry *http_follow_cached_redirects(HttpRequest *req)
{
    HttpRedirEntry *last = NULL;
    HttpRedirEntry *cur = http_redir_cache_lookup_req(req);

    while (cur != NULL) {
        if (req->cfg.max_redirects < req->redirect_count + 1) {
            http_redir_entry_release_locked(cur);
            return last;
        }
        if (req->cfg.redirect_cb != NULL) {
            typedef s32 (*RedirCb)(s32, s32, s32 *, const char *, void *);
            sceKernelCheckThreadStack();
            if (((RedirCb)req->cfg.redirect_cb)(req->id, 301, &req->method,
                                                cur->dst_url, req->cfg.redirect_cb_arg) < 0) {
                req->cfg.auto_redirect = 0;
                http_redir_entry_release_locked(cur);
                return last;
            }
        }
        req->redirect_count++;
        if (last != NULL)
            http_redir_entry_release_locked(last);
        last = cur;
        cur = http_redir_cache_find(cur->dst_url, req->method, NULL);
    }

    return last;
}

HTTP_ALWAYS_INLINE s32 http_replace_str_copy(char **field, const char *src)
{
    u32 len;
    char *copy;

    if (*field != NULL) {
        http_free(*field);
        *field = NULL;
    }
    len = sceNet_lib_D8722983(src);
    copy = (char *)http_malloc(len + 1);
    if (copy == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceNet_lib_80C9F02A(copy, src);
    *field = copy;
    return 0;
}

s32 http_send_request_internal(s32 req_id, const void *post_data, u32 post_size)
{
    HttpRequest *req = NULL;
    HttpRedirEntry *redir = NULL;
    void *uri_work = NULL;
    SceUriElement uri;
    u32 uri_work_size;
    s32 ret;

    sceKernelMemset(&uri, 0, sizeof(uri));
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    do {
        ret = http_lock_request(req_id, &req);
        if (ret < 0)
            break;

        if (req->state != HTTP_REQ_CREATED && req->state != HTTP_REQ_SENDING_BODY) {
            ret = SCE_HTTP_ERROR_AFTER_SEND;
            break;
        }

        if (req->method == HTTP_METHOD_POST) {
            req->post_data = post_data;
            req->post_chunk_len = post_size;
        }

        if (req->cfg.auto_redirect == 1) {
            redir = http_follow_cached_redirects(req);
            if (redir != NULL) {
                if (http_create_connection_with_url_internal(0, redir->dst_url, req->cfg.keep_alive, 1, req) < 0) {
                    http_redir_entry_invalidate_locked(redir);
                    redir = NULL;
                    req->redirect_count = 0;
                } else {
                    ret = sceUriParse(NULL, redir->dst_url, NULL, &uri_work_size, 0);
                    if (ret < 0)
                        break;
                    uri_work = http_malloc(uri_work_size);
                    if (uri_work == NULL) {
                        ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                        break;
                    }
                    sceUriParse(&uri, redir->dst_url, uri_work, NULL, uri_work_size);

                    if (uri.path != NULL) {
                        ret = http_replace_str_copy(&req->path, uri.path);
                        if (ret < 0)
                            break;
                    }
                    if (uri.query != NULL) {
                        ret = http_replace_str_copy(&req->query, uri.query);
                        if (ret < 0)
                            break;
                    }
                    if (uri.fragment != NULL) {
                        ret = http_replace_str_copy(&req->fragment, uri.fragment);
                        if (ret < 0)
                            break;
                    }
                    if (uri.host != NULL) {
                        char *host_hdr = NULL;
                        s32 scheme = http_parse_scheme(uri.scheme);
                        ret = http_format_host_header(&host_hdr, scheme, uri.host, uri.port);
                        if (ret < 0)
                            break;
                        http_add_header_internal(req->id, "Host", host_hdr, 0);
                        http_free(host_hdr);
                    }

                    http_free(uri_work);
                    uri_work = NULL;
                    http_redir_entry_release_locked(redir);
                    redir = NULL;
                }
            }
        }

        ret = http_send_request_core(req);
    } while (0);

    if (redir != NULL)
        http_redir_entry_release_locked(redir);
    if (uri_work != NULL)
        http_free(uri_work);
    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return ret;
}

/* -------------------------------------------------------------------------
 * HTTP Basic & Digest authentication
 * ------------------------------------------------------------------------- */
s32 http_parse_auth_header(HttpAuthChallenge *out, const char *hdr, u32 hdr_len, s32 status_code)
{
    const char *field;
    s32 is_proxy;
    char *val = NULL;
    u32 val_len;

    if (status_code == 401) {
        field = "WWW-Authenticate";
        is_proxy = 0;
    } else if (status_code == 407) {
        field = "Proxy-Authenticate";
        is_proxy = 1;
    } else {
        return SCE_HTTP_ERROR_NOT_FOUND;
    }

    if (sceParseHttpResponseHeader(hdr, hdr_len, field, &val, &val_len) < 0)
        return SCE_HTTP_ERROR_NOT_FOUND;

    return http_parse_auth_challenge(out, (const u8 *)val, val_len, is_proxy);
}

s32 http_parse_auth_challenge(HttpAuthChallenge *out, const u8 *val, u32 len, s32 is_proxy)
{
    s32 ret = 0;
    u32 pos;

    if (out == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    sceKernelMemset(out, 0, sizeof(*out));
    if (val == NULL || *val == '\0')
        ret = SCE_HTTP_ERROR_BAD_RESPONSE;

    if (sceNet_lib_B9085A96((const char *)val, "Basic", 5) == 0) {
        out->type = is_proxy ? HTTP_AUTH_PROXY_BASIC : HTTP_AUTH_BASIC;
        pos = 5;
    } else if (sceNet_lib_B9085A96((const char *)val, "Digest", 6) == 0) {
        out->type = is_proxy ? HTTP_AUTH_PROXY_DIGEST : HTTP_AUTH_DIGEST;
        pos = 6;
    } else {
        return SCE_HTTP_ERROR_UNKNOWN_AUTH_TYPE;
    }

    while (pos < len) {
        const u8 *cur = val + pos;
        u32 rem = len - pos;

        if (sceNet_lib_B9085A96((const char *)cur, "realm", 5) == 0) {
            pos += http_parse_auth_param(cur, rem, &out->realm, &out->realm_len);
        } else if (sceNet_lib_B9085A96((const char *)cur, "nonce", 5) == 0) {
            pos += http_parse_auth_param(cur, rem, &out->nonce, &out->nonce_len);
        } else if (sceNet_lib_B9085A96((const char *)cur, "opaque", 6) == 0) {
            pos += http_parse_auth_param(cur, rem, &out->opaque, &out->opaque_len);
        } else if (sceNet_lib_B9085A96((const char *)cur, "stale", 5) == 0) {
            char *stale_val = NULL;
            u32 stale_len = 0;
            pos += http_parse_auth_param(cur, rem, &stale_val, &stale_len);
            if (sceNet_lib_B9085A96(stale_val, "true", 4) == 0)
                out->stale = 1;
            else if (sceNet_lib_B9085A96(stale_val, "false", 5) == 0)
                out->stale = 0;
        } else if (sceNet_lib_B9085A96((const char *)cur, "algorithm", 9) == 0) {
            pos += http_parse_auth_param(cur, rem, &out->algorithm, &out->algorithm_len);
        } else if (sceNet_lib_B9085A96((const char *)cur, "qop", 3) == 0) {
            pos += http_parse_auth_param(cur, rem, &out->qop, &out->qop_len);
            if (out->qop != NULL && out->qop_len > 4 &&
                sceNet_lib_B9085A96(out->qop, "auth", 4) == 0) {
                out->qop_len = 4;
            }
        } else if (sceNet_lib_B9085A96((const char *)cur, "domain", 6) == 0) {
            pos += http_parse_auth_param(cur, rem, &out->domain, &out->domain_len);
        } else {
            pos++;
        }
    }

    return ret;
}

u32 http_parse_auth_param(const u8 *src, u32 len, char **val_out, u32 *val_len_out)
{
    u32 idx = 0;

    *val_out = NULL;
    *val_len_out = 0;

    while (idx < len && src[idx] != '=')
        idx++;
    if (idx < len)
        idx++;

    if (idx >= len)
        return idx;

    if (src[idx] == '"') {
        idx++;
        if (idx <= len) {
            *val_out = (char *)(src + idx);
            while (idx < len && src[idx] != '"') {
                idx++;
                (*val_len_out)++;
            }
            while (idx < len && src[idx] != ',' && src[idx] != '\n')
                idx++;
        }
    } else {
        *val_out = (char *)(src + idx);
        while (idx < len && src[idx] != ',' && src[idx] != '\n') {
            idx++;
            (*val_len_out)++;
        }
    }

    return idx;
}

s32 http_build_basic_auth(u8 *dst, u32 *req_len, u32 dst_size, const char *user, const char *pass)
{
    u32 raw_len;
    u32 out_len;
    char *raw_buf;

    if (user == NULL || pass == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    if (dst != NULL)
        dst[0] = '\0';

    raw_len = sceNet_lib_D8722983(user) + 1 + sceNet_lib_D8722983(pass) + 1;
    out_len = 6 + 4 * ((raw_len + 1) / 3) + 1;

    if (dst == NULL) {
        if (req_len != NULL)
            *req_len = out_len;
        return 0;
    }

    if (dst_size < out_len)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    raw_buf = (char *)http_malloc(raw_len);
    if (raw_buf == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    sceKernelMemset(raw_buf, 0, raw_len);
    sceNet_lib_80C9F02A(raw_buf, user);
    sceNet_lib_5705F6F9(raw_buf, ":");
    sceNet_lib_5705F6F9(raw_buf, pass);

    {
        char *out = (char *)dst;
        sceNet_lib_80C9F02A(out, "Basic ");
        u32 enc_len = (u32)http_base64_encode(out + 6, (const u8 *)raw_buf,
                                              sceNet_lib_D8722983(raw_buf));
        out[6 + enc_len] = '\0';
    }

    http_free(raw_buf);
    return 0;
}

HTTP_ALWAYS_INLINE const char *http_method_name(s32 method)
{
    switch (method) {
    case HTTP_METHOD_GET:     return "GET";
    case HTTP_METHOD_POST:    return "POST";
    case HTTP_METHOD_HEAD:    return "HEAD";
    case HTTP_METHOD_OPTIONS: return "OPTIONS";
    case HTTP_METHOD_PUT:     return "PUT";
    case HTTP_METHOD_DELETE:  return "DELETE";
    case HTTP_METHOD_TRACE:   return "TRACE";
    case HTTP_METHOD_CONNECT: return "CONNECT";
    default:                  return NULL;
    }
}

s32 http_build_digest_auth(char *dst, u32 *req_len, u32 dst_size,
                           const char *user, const char *pass, const char *uri,
                           s32 method, const void *entity, u32 entity_len,
                           const HttpDigestParams *params)
{
    const char *method_str;
    char nc_hex[16];
    char ha1_hex[40];
    char entity_hex[40];
    char resp_hex[40];
    char cnonce_hex[40];
    u32 pos;
    u32 added = 0;
    s32 do_write = (dst != NULL);
    s32 ret;

    if (user == NULL || pass == NULL || params->realm == NULL ||
        params->qop == NULL || params->nonce == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    method_str = http_method_name(method);
    if (method_str == NULL)
        return SCE_HTTP_ERROR_UNKNOWN_METHOD;

    if (entity != NULL)
        http_digest_hash_entity(entity, entity_len, entity_hex);

    http_digest_make_cnonce(sceKernelGetSystemTimeLow(), cnonce_hex);
    http_digest_calc_ha1(params->algorithm, user, params->realm, pass,
                         params->nonce, cnonce_hex, ha1_hex);
    sceNet_lib_CF705E46(nc_hex, "%08x", params->nc);
    nc_hex[8] = '\0';
    http_digest_calc_response(ha1_hex, params->nonce, nc_hex, cnonce_hex,
                              params->qop, method_str, uri, entity_hex, resp_hex);

    pos = 7; /* "Digest " */
    if (do_write)
        sceNet_lib_B5CE388A(dst, "Digest ", dst_size);

    ret = http_digest_append_kv(dst + pos, dst_size - pos, &added, "username", user, do_write);
    if (ret < 0) return ret;
    pos += added;

    ret = http_digest_append_kv(dst + pos, dst_size - pos, &added, "realm", params->realm, do_write);
    if (ret < 0) return ret;
    pos += added;

    ret = http_digest_append_kv(dst + pos, dst_size - pos, &added, "nonce", params->nonce, do_write);
    if (ret < 0) return ret;
    pos += added;

    ret = http_digest_append_kv(dst + pos, dst_size - pos, &added, "uri", uri, do_write);
    if (ret < 0) return ret;
    pos += added;

    ret = http_digest_append_kv(dst + pos, dst_size - pos, &added, "qop", params->qop, do_write);
    if (ret < 0) return ret;
    pos += added;

    ret = http_digest_append_kv(dst + pos, dst_size - pos, &added, "nc", nc_hex, do_write);
    if (ret < 0) return ret;
    pos += added;

    ret = http_digest_append_kv(dst + pos, dst_size - pos, &added, "cnonce", cnonce_hex, do_write);
    if (ret < 0) return ret;
    pos += added;

    ret = http_digest_append_kv(dst + pos, dst_size - pos, &added, "response", resp_hex, do_write);
    if (ret < 0) return ret;
    pos += added;

    if (params->opaque != NULL && sceNet_lib_D8722983(params->opaque) != 0) {
        ret = http_digest_append_kv(dst + pos, dst_size - pos, &added, "opaque", params->opaque, do_write);
        if (ret < 0)
            return ret;
        pos += added;
    }

    if (do_write)
        dst[pos] = '\0';
    if (req_len != NULL)
        *req_len = pos + 1;
    return 0;
}

s32 http_init_digest_params(HttpDigestParams *dst, const HttpAuthChallenge *src)
{
    s32 is_md5_sess = 0;
    s32 has_auth = 0;
    s32 has_auth_int = 0;
    char *alg_tmp = NULL;
    char *qop_tmp = NULL;
    const char *alg_str;
    const char *qop_str = NULL;
    u32 total;
    char *buf;
    u32 off = 0;
    s32 ret = 0;

    do {
        if (src->algorithm != NULL) {
            alg_tmp = (char *)http_malloc(src->algorithm_len + 1);
            if (alg_tmp == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                break;
            }
            sceKernelMemcpy(alg_tmp, src->algorithm, src->algorithm_len);
            alg_tmp[src->algorithm_len] = '\0';
            if (sceNet_lib_B9085A96(alg_tmp, "md5-sess", 8) == 0)
                is_md5_sess = 1;
        }

        if (src->qop != NULL) {
            u32 pos = 0;
            qop_tmp = (char *)http_malloc(src->qop_len + 1);
            if (qop_tmp == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                break;
            }
            sceKernelMemcpy(qop_tmp, src->qop, src->qop_len);
            qop_tmp[src->qop_len] = '\0';
            while (pos < src->qop_len) {
                if (sceNet_lib_B9085A96(qop_tmp + pos, "auth-int", 8) == 0)
                    has_auth_int = 1;
                else if (sceNet_lib_B9085A96(qop_tmp + pos, "auth", 4) == 0)
                    has_auth = 1;
                while (pos < src->qop_len && qop_tmp[pos] != ',')
                    pos++;
                if (pos < src->qop_len && qop_tmp[pos] == ',')
                    pos++;
            }
        }

        alg_str = is_md5_sess ? "md5-sess" : "md5";
        total = sceNet_lib_D8722983(alg_str) + 1;
        if (has_auth_int) {
            qop_str = "auth-int";
            total += sceNet_lib_D8722983(qop_str) + 1;
        } else if (has_auth) {
            qop_str = "auth";
            total += sceNet_lib_D8722983(qop_str) + 1;
        }

        total += src->algorithm_len + src->domain_len + src->nonce_len +
                 src->opaque_len + src->qop_len + src->realm_len + 6;

        buf = (char *)http_malloc(total);
        if (buf == NULL) {
            ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
            break;
        }
        sceKernelMemset(buf, 0, total);

        dst->algorithm = buf + off;
        sceNet_lib_80C9F02A(dst->algorithm, alg_str);
        off += sceNet_lib_D8722983(alg_str) + 1;

        if (qop_str != NULL) {
            dst->qop = buf + off;
            sceNet_lib_80C9F02A(dst->qop, qop_str);
            off += sceNet_lib_D8722983(qop_str) + 1;
        } else {
            dst->qop = NULL;
        }

        dst->algorithm = buf + off;
        sceKernelMemcpy(dst->algorithm, src->algorithm, src->algorithm_len);
        off += src->algorithm_len + 1;

        dst->domain = buf + off;
        sceKernelMemcpy(dst->domain, src->domain, src->domain_len);
        off += src->domain_len + 1;

        dst->nonce = buf + off;
        sceKernelMemcpy(dst->nonce, src->nonce, src->nonce_len);
        off += src->nonce_len + 1;

        dst->opaque = buf + off;
        sceKernelMemcpy(dst->opaque, src->opaque, src->opaque_len);
        off += src->opaque_len + 1;

        dst->qop = buf + off;
        sceKernelMemcpy(dst->qop, src->qop, src->qop_len);
        off += src->qop_len + 1;

        dst->realm = buf + off;
        sceKernelMemcpy(dst->realm, src->realm, src->realm_len);

        dst->buf = buf;
        dst->nc = 1;
    } while (0);

    if (alg_tmp != NULL)
        http_free(alg_tmp);
    if (qop_tmp != NULL)
        http_free(qop_tmp);
    return ret;
}

void http_digest_calc_ha1(const char *alg, const char *user, const char *realm,
                          const char *pass, const char *nonce, const char *cnonce,
                          char *ha1_hex)
{
    u8 ctx[96];
    u8 digest[16];

    http_md5_init(ctx);
    http_md5_update(ctx, user, sceNet_lib_D8722983(user));
    http_md5_update(ctx, ":", 1);
    http_md5_update(ctx, realm, sceNet_lib_D8722983(realm));
    http_md5_update(ctx, ":", 1);
    http_md5_update(ctx, pass, sceNet_lib_D8722983(pass));
    http_md5_result(ctx, digest);

    if (sceNet_lib_9CFBC7E3(alg, "md5-sess") == 0) {
        http_md5_init(ctx);
        http_md5_update(ctx, digest, 16);
        http_md5_update(ctx, ":", 1);
        http_md5_update(ctx, nonce, sceNet_lib_D8722983(nonce));
        http_md5_update(ctx, ":", 1);
        http_md5_update(ctx, cnonce, sceNet_lib_D8722983(cnonce));
        http_md5_result(ctx, digest);
    }

    http_md5_to_hex(digest, ha1_hex);
}

void http_digest_calc_response(const char *ha1_hex, const char *nonce, const char *nc_hex,
                               const char *cnonce, const char *qop, const char *method,
                               const char *uri, const char *entity_hex, char *resp_hex)
{
    u8 ctx[96];
    u8 ha2_bin[16];
    u8 resp_bin[16];
    char ha2_hex[40];

    http_md5_init(ctx);
    http_md5_update(ctx, method, sceNet_lib_D8722983(method));
    http_md5_update(ctx, ":", 1);
    http_md5_update(ctx, uri, sceNet_lib_D8722983(uri));
    if (sceNet_lib_9CFBC7E3(qop, "auth-int") == 0) {
        http_md5_update(ctx, ":", 1);
        http_md5_update(ctx, entity_hex, 32);
    }
    http_md5_result(ctx, ha2_bin);
    http_md5_to_hex(ha2_bin, ha2_hex);

    http_md5_init(ctx);
    http_md5_update(ctx, ha1_hex, 32);
    http_md5_update(ctx, ":", 1);
    http_md5_update(ctx, nonce, sceNet_lib_D8722983(nonce));
    http_md5_update(ctx, ":", 1);
    if (*qop != '\0') {
        http_md5_update(ctx, nc_hex, sceNet_lib_D8722983(nc_hex));
        http_md5_update(ctx, ":", 1);
        http_md5_update(ctx, cnonce, sceNet_lib_D8722983(cnonce));
        http_md5_update(ctx, ":", 1);
        http_md5_update(ctx, qop, sceNet_lib_D8722983(qop));
        http_md5_update(ctx, ":", 1);
        http_md5_update(ctx, ha2_hex, 32);
        http_md5_result(ctx, resp_bin);
    }

    http_md5_to_hex(resp_bin, resp_hex);
}

void http_digest_make_cnonce(u32 seed, char *cnonce_hex)
{
    u8 ctx[96];
    u8 digest[16];

    http_md5_init(ctx);
    http_md5_update(ctx, &seed, 4);
    http_md5_result(ctx, digest);
    http_md5_to_hex(digest, cnonce_hex);
}

s32 http_digest_hash_entity(const void *data, u32 len, char *out_hex)
{
    u8 ctx[96];
    u8 digest[16];

    if (data == NULL || out_hex == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    http_md5_init(ctx);
    http_md5_update(ctx, data, len);
    http_md5_result(ctx, digest);
    http_md5_to_hex(digest, out_hex);
    return 0;
}

/* -------------------------------------------------------------------------
 * Auth credential cache
 * ------------------------------------------------------------------------- */
s32 sceHttpFlushAuthList(void)
{
    s32 intr;
    HttpAuthEntry *cur;

    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_auth_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    intr = http_intr_suspend();
    cur = g_auth_head;
    while (cur != NULL) {
        HttpAuthEntry *next = cur->next;
        http_auth_entry_mark_deleted(cur);
        cur = next;
    }
    http_intr_resume(intr);
    return 0;
}

s32 http_auth_cache_init(void)
{
    u32 idx;

    if (g_auth_inited == 1)
        return SCE_HTTP_ERROR_ALREADY_INITED;

    g_auth_inited = 1;
    sceKernelMemset(g_auth_pool, 0, sizeof(g_auth_pool));
    g_auth_free_list = &g_auth_pool[0];
    for (idx = 0; idx < 15; idx++)
        g_auth_pool[idx].free_next = &g_auth_pool[idx + 1];
    g_auth_pool[15].free_next = NULL;
    g_auth_head = NULL;
    g_auth_tail = NULL;
    return 0;
}

s32 http_auth_cache_term(void)
{
    HttpAuthEntry *cur;

    if (g_auth_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    for (cur = g_auth_head; cur != NULL; cur = cur->next) {
        http_free(cur->host);
        http_free(cur->path);
        if (cur->digest.buf != NULL)
            http_free(cur->digest.buf);
    }

    g_auth_inited = 0;
    sceKernelMemset(g_auth_pool, 0, sizeof(g_auth_pool));
    g_auth_free_list = NULL;
    g_auth_head = NULL;
    g_auth_tail = NULL;
    return 0;
}

void http_auth_entry_mark_deleted(HttpAuthEntry *entry)
{
    entry->deleted = 1;
    if (entry->ref_count != 0)
        return;

    if (entry->prev != NULL) {
        entry->prev->next = entry->next;
        if (entry->next != NULL)
            entry->next->prev = entry->prev;
        else
            g_auth_tail = entry->prev;
    } else {
        g_auth_head = entry->next;
        if (entry->next != NULL)
            entry->next->prev = NULL;
        else
            g_auth_tail = NULL;
    }

    http_free(entry->host);
    http_free(entry->path);
    if (entry->digest.buf != NULL) {
        http_free(entry->digest.buf);
        entry->digest.buf = NULL;
    }
    sceKernelMemset(entry, 0, sizeof(*entry));
    entry->free_next = g_auth_free_list;
    g_auth_free_list = entry;
}

void http_apply_cached_auth(HttpRequest *req)
{
    HttpAuthEntry *entry;

    if (req->cfg.auth_enabled == 0 || req->auth_entry != NULL)
        return;

    entry = http_auth_cache_find_ready(req, 0);
    req->auth_entry = entry;
    if (entry == NULL)
        return;

    if (entry->auth_type == HTTP_AUTH_BASIC) {
        http_attach_basic_auth_header(req, entry->username, entry->password, 0);
    } else if (entry->auth_type == HTTP_AUTH_DIGEST) {
        entry->digest.nc++;
        http_attach_digest_auth_header(req, entry->username, entry->password,
                                       NULL, 0, &entry->digest, 0);
    }
}

void http_apply_cached_proxy_auth(HttpRequest *req, HttpConnection *conn)
{
    HttpAuthEntry *entry;

    if (conn->cfg.proxy_server == NULL || req->cfg.proxy_auth_enabled == 0 ||
        req->proxy_auth_entry != NULL)
        return;

    entry = http_auth_cache_find_ready(req, 1);
    req->proxy_auth_entry = entry;
    if (entry == NULL)
        return;

    if (entry->auth_type == HTTP_AUTH_PROXY_BASIC) {
        http_attach_basic_auth_header(req, entry->username, entry->password, 1);
    } else if (entry->auth_type == HTTP_AUTH_PROXY_DIGEST) {
        http_attach_digest_auth_header(req, entry->username, entry->password,
                                       NULL, 0, &entry->digest, 1);
    }
}

HttpAuthEntry *http_auth_cache_find_ready(HttpRequest *req, s32 is_proxy)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    HttpAuthEntry *best = NULL;
    HttpAuthEntry *cur;
    s32 intr = http_intr_suspend();

    for (cur = g_auth_head; cur != NULL; cur = cur->next) {
        if (cur->deleted == 1 || cur->status != 2)
            continue;

        if (is_proxy == 1) {
            if ((cur->auth_type == HTTP_AUTH_PROXY_BASIC || cur->auth_type == HTTP_AUTH_PROXY_DIGEST) &&
                conn->cfg.proxy_port == cur->port &&
                sceNet_lib_A0F16ABD(conn->cfg.proxy_server, cur->host) == 0) {
                best = cur;
            }
        } else {
            if ((cur->auth_type == HTTP_AUTH_BASIC || cur->auth_type == HTTP_AUTH_DIGEST) &&
                http_path_prefix_match(cur->path, req->path) == 1 &&
                conn->scheme == cur->scheme &&
                conn->port == cur->port &&
                sceNet_lib_A0F16ABD(conn->host, cur->host) == 0) {
                if (best == NULL ||
                    sceNet_lib_D8722983(cur->path) >= sceNet_lib_D8722983(best->path)) {
                    best = cur;
                }
            }
        }
    }

    if (best != NULL) {
        best->hit_count++;
        best->ref_count++;

        if (best->prev != NULL) {
            best->prev->next = best->next;
            if (best->next != NULL)
                best->next->prev = best->prev;
            else
                g_auth_tail = best->prev;
        } else {
            g_auth_head = best->next;
            if (best->next != NULL)
                best->next->prev = NULL;
            else
                g_auth_tail = NULL;
        }
        best->prev = NULL;
        best->next = g_auth_head;
        if (g_auth_head != NULL)
            g_auth_head->prev = best;
        else
            g_auth_tail = best;
        g_auth_head = best;
    }

    http_intr_resume(intr);
    return best;
}
