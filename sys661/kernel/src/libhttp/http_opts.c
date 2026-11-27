#include "libhttp_in.h"

/* -------------------------------------------------------------------------
 * Memory allocator & library init state
 * ------------------------------------------------------------------------- */
static void *(*g_http_alloc_fn)(u32);
static void  (*g_http_free_fn)(void *);
static void *(*g_http_realloc_fn)(void *, u32);
static s32     g_http_vpl_id;
static s32     g_http_vpl_size;
static s32     g_http_alloc_count;
static s32     g_http_inited;

/* -------------------------------------------------------------------------
 * 301 Redirect cache state (16 static slots)
 * ------------------------------------------------------------------------- */
static s32             g_redir_inited;
static HttpRedirEntry  g_redir_pool[16];
static HttpRedirEntry *g_redir_free_list;
static HttpRedirEntry *g_redir_head;
static HttpRedirEntry *g_redir_tail;

/* -------------------------------------------------------------------------
 * Shared handle-option helpers
 * ------------------------------------------------------------------------- */
static inline void http_unlock_template(HttpTemplate *tmpl)
{
    s32 intr = http_intr_suspend();
    tmpl->ref_count--;
    http_intr_resume(intr);
}

static inline void http_unlock_connection(HttpConnection *conn)
{
    s32 intr = http_intr_suspend();
    conn->ref_count--;
    http_intr_resume(intr);
}

static inline void http_unlock_request(HttpRequest *req)
{
    s32 intr = http_intr_suspend();
    req->ref_count--;
    http_intr_resume(intr);
}

/* -------------------------------------------------------------------------
 * Path resolution & memory pool
 * ------------------------------------------------------------------------- */
char *http_resolve_relative_path(const char *base_path, const char *rel_path)
{
    char *combined;
    char *swept;

    if (base_path == NULL || rel_path == NULL)
        return NULL;

    if (sceNet_lib_D8722983(rel_path) != 0 && rel_path[0] == '/') {
        u32 len = sceNet_lib_D8722983(rel_path) + 1;
        combined = (char *)http_malloc(len);
        if (combined == NULL)
            return NULL;
        sceKernelMemcpy(combined, rel_path, len);
    } else {
        const char *slash = sceNet_lib_F48963C6(base_path, '/');
        if (slash != NULL) {
            u32 dir_len = (u32)(slash - base_path) + 1;
            u32 rel_len = sceNet_lib_D8722983(rel_path);
            combined = (char *)http_malloc(dir_len + rel_len + 1);
            if (combined == NULL)
                return NULL;
            sceKernelMemcpy(combined, base_path, dir_len);
            combined[dir_len] = '\0';
        } else {
            u32 base_len = sceNet_lib_D8722983(base_path);
            u32 rel_len = sceNet_lib_D8722983(rel_path);
            combined = (char *)http_malloc(base_len + rel_len + 1);
            if (combined == NULL)
                return NULL;
            sceNet_lib_80C9F02A(combined, base_path);
        }
        sceNet_lib_5705F6F9(combined, rel_path);
    }

    {
        u32 comb_len = sceNet_lib_D8722983(combined);
        swept = (char *)http_malloc(comb_len + 1);
        if (swept != NULL && sceUriSweepPath(swept, combined, comb_len + 1) >= 0) {
            http_free(combined);
            return swept;
        }
    }

    http_free(combined);
    if (swept != NULL)
        http_free(swept);
    return NULL;
}

void *http_malloc(u32 size)
{
    void *ptr = NULL;

    if (g_http_alloc_fn != NULL) {
        ptr = g_http_alloc_fn(size);
    } else if (sceKernelTryAllocateVpl(g_http_vpl_id, size, &ptr) < 0) {
        ptr = NULL;
    }

    if (ptr != NULL)
        g_http_alloc_count++;
    return ptr;
}

void http_free(void *ptr)
{
    if (ptr != NULL)
        g_http_alloc_count--;
    if (g_http_free_fn != NULL)
        g_http_free_fn(ptr);
    else
        sceKernelFreeVpl(g_http_vpl_id, ptr);
}

s32 sceHttpSetMallocFunction(void *(*alloc_fn)(u32), void (*free_fn)(void *), void *(*realloc_fn)(void *, u32))
{
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (alloc_fn == NULL || free_fn == NULL || realloc_fn == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;
    if (g_http_alloc_count > 0)
        return SCE_HTTP_ERROR_BUSY;

    if (g_http_vpl_id != 0) {
        ret = sceKernelDeleteVpl(g_http_vpl_id);
        if (ret < 0)
            return ret;
        g_http_vpl_id = 0;
    }

    g_http_alloc_fn = alloc_fn;
    g_http_free_fn = free_fn;
    g_http_realloc_fn = realloc_fn;
    return 0;
}

s32 http_pool_init(u32 size)
{
    u32 part_id;
    s32 ret;

    if (size == 0)
        return 0;

    ret = sceKernelQueryMemoryInfo(&g_http_vpl_id, &part_id, NULL);
    if (ret >= 0) {
        ret = sceKernelCreateVpl("SceHttp", (s32)part_id, 0, size, NULL);
        if (ret >= 0) {
            g_http_vpl_id = ret;
            g_http_vpl_size = (s32)size;
            return 0;
        }
    }

    if (g_http_vpl_id != 0) {
        sceKernelDeleteVpl(g_http_vpl_id);
        g_http_vpl_id = 0;
    }
    return ret;
}

void http_pool_term(void)
{
    if (g_http_vpl_id != 0) {
        sceKernelDeleteVpl(g_http_vpl_id);
        g_http_vpl_id = 0;
        g_http_vpl_size = 0;
    }
}

s32 http_pool_get_size(void)
{
    if (g_http_alloc_fn != NULL)
        return -1;
    return g_http_vpl_size;
}

/* -------------------------------------------------------------------------
 * Template / Connection / Request option setters
 * ------------------------------------------------------------------------- */
s32 sceHttpSetResolveRetry(s32 id, s32 retry)
{
    HttpTemplate *tmpl = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_lock_template(id, &tmpl);
    if (ret >= 0) {
        tmpl->resolve_retry = retry;
        http_unlock_template(tmpl);
    }
    return ret;
}

s32 sceHttpSetResolveTimeOut(s32 id, u32 usec)
{
    HttpTemplate *tmpl = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_lock_template(id, &tmpl);
    if (ret >= 0) {
        tmpl->resolve_timeout = usec;
        http_unlock_template(tmpl);
    }
    return ret;
}

HTTP_ALWAYS_INLINE s32 http_set_tmpl_or_conn_u32(s32 id, u32 val, u32 tmpl_off)
{
    s32 type;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    type = http_handle_type(id);
    if (type < 0)
        return type;

    if (type == HTTP_HANDLE_TEMPLATE) {
        HttpTemplate *tmpl = NULL;
        ret = http_lock_template(id, &tmpl);
        if (ret >= 0) {
            *(u32 *)((u8 *)tmpl + tmpl_off) = val;
            http_unlock_template(tmpl);
        }
        return ret;
    }
    if (type == HTTP_HANDLE_CONNECTION) {
        HttpConnection *conn = NULL;
        ret = http_lock_connection(id, &conn);
        if (ret >= 0) {
            *(u32 *)((u8 *)&conn->cfg + tmpl_off) = val;
            http_unlock_connection(conn);
        }
        return ret;
    }
    return SCE_HTTP_ERROR_INVALID_ID;
}

s32 sceHttpSetConnectTimeOut(s32 id, u32 usec)
{
    return http_set_tmpl_or_conn_u32(id, usec, offsetof(HttpTemplate, connect_timeout));
}

s32 sceHttpSetSendTimeOut(s32 id, u32 usec)
{
    return http_set_tmpl_or_conn_u32(id, usec, offsetof(HttpTemplate, send_timeout));
}

s32 sceHttpSetRecvTimeOut(s32 id, u32 usec)
{
    return http_set_tmpl_or_conn_u32(id, usec, offsetof(HttpTemplate, recv_timeout));
}

s32 sceHttp_CC920C12(s32 id)
{
    return http_set_tmpl_or_conn_u32(id, 1, offsetof(HttpTemplate, tcp_nodelay));
}

s32 sceHttp_D29163DA(s32 id)
{
    return http_set_tmpl_or_conn_u32(id, 0, offsetof(HttpTemplate, tcp_nodelay));
}

s32 sceHttpSetResHeaderMaxSize(s32 id, u32 size)
{
    return http_set_tmpl_or_conn_u32(id, size, offsetof(HttpTemplate, res_header_max_size));
}

s32 sceHttpSetRecvBlockSize(s32 id, u32 size)
{
    s32 type;
    s32 ret;
    HttpTemplate *tmpl = NULL;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    type = http_handle_type(id);
    if (type < 0)
        return type;
    if (type != HTTP_HANDLE_TEMPLATE)
        return SCE_HTTP_ERROR_INVALID_ID;

    ret = http_lock_template(id, &tmpl);
    if (ret >= 0) {
        tmpl->recv_block_size = size;
        http_unlock_template(tmpl);
    }
    return ret;
}

s32 sceHttpSetCacheContentLengthMaxSize(s32 id, u32 size)
{
    s32 type;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    type = http_handle_type(id);
    if (type < 0)
        return type;

    if (type == HTTP_HANDLE_TEMPLATE) {
        HttpTemplate *tmpl = NULL;
        ret = http_lock_template(id, &tmpl);
        if (ret >= 0) {
            tmpl->cache_content_max_size = size;
            http_unlock_template(tmpl);
        }
        return ret;
    }
    if (type == HTTP_HANDLE_CONNECTION) {
        HttpConnection *conn = NULL;
        ret = http_lock_connection(id, &conn);
        if (ret >= 0) {
            conn->cfg.cache_content_max_size = size;
            http_unlock_connection(conn);
        }
        return ret;
    }
    if (type == HTTP_HANDLE_REQUEST) {
        HttpRequest *req = NULL;
        ret = http_lock_request(id, &req);
        if (ret >= 0) {
            req->cfg.cache_content_max_size = size;
            http_unlock_request(req);
        }
        return ret;
    }
    return SCE_HTTP_ERROR_INVALID_ID;
}

s32 sceHttpEnableKeepAlive(s32 id)
{
    return http_set_tmpl_or_conn_u32(id, 1, offsetof(HttpTemplate, keep_alive));
}

s32 sceHttpDisableKeepAlive(s32 id)
{
    return http_set_tmpl_or_conn_u32(id, 0, offsetof(HttpTemplate, keep_alive));
}

HTTP_ALWAYS_INLINE s32 http_set_any_u32_locked(s32 id, u32 val, u32 tmpl_off)
{
    s32 type;
    s32 intr;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    type = http_handle_type(id);
    if (type < 0)
        return type;

    intr = http_intr_suspend();
    if (type == HTTP_HANDLE_TEMPLATE) {
        HttpTemplate *tmpl = NULL;
        ret = http_lock_template(id, &tmpl);
        if (tmpl != NULL) {
            *(u32 *)((u8 *)tmpl + tmpl_off) = val;
            tmpl->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_CONNECTION) {
        HttpConnection *conn = NULL;
        ret = http_lock_connection(id, &conn);
        if (conn != NULL) {
            *(u32 *)((u8 *)&conn->cfg + tmpl_off) = val;
            conn->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_REQUEST) {
        HttpRequest *req = NULL;
        ret = http_lock_request(id, &req);
        if (req != NULL) {
            *(u32 *)((u8 *)&req->cfg + tmpl_off) = val;
            req->ref_count--;
        }
    } else {
        ret = SCE_HTTP_ERROR_INVALID_ID;
    }
    http_intr_resume(intr);
    return ret;
}

HTTP_ALWAYS_INLINE s32 http_set_any_u16_locked(s32 id, u16 val, u32 tmpl_off)
{
    s32 type;
    s32 intr;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    type = http_handle_type(id);
    if (type < 0)
        return type;

    intr = http_intr_suspend();
    if (type == HTTP_HANDLE_TEMPLATE) {
        HttpTemplate *tmpl = NULL;
        ret = http_lock_template(id, &tmpl);
        if (tmpl != NULL) {
            *(u16 *)((u8 *)tmpl + tmpl_off) = val;
            tmpl->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_CONNECTION) {
        HttpConnection *conn = NULL;
        ret = http_lock_connection(id, &conn);
        if (conn != NULL) {
            *(u16 *)((u8 *)&conn->cfg + tmpl_off) = val;
            conn->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_REQUEST) {
        HttpRequest *req = NULL;
        ret = http_lock_request(id, &req);
        if (req != NULL) {
            *(u16 *)((u8 *)&req->cfg + tmpl_off) = val;
            req->ref_count--;
        }
    } else {
        ret = SCE_HTTP_ERROR_INVALID_ID;
    }
    http_intr_resume(intr);
    return ret;
}

s32 sceHttpEnableCookie(s32 id)
{
    return http_set_any_u32_locked(id, 1, offsetof(HttpTemplate, cookie_enabled));
}

s32 sceHttpDisableCookie(s32 id)
{
    return http_set_any_u32_locked(id, 0, offsetof(HttpTemplate, cookie_enabled));
}

s32 sceHttpEnableAuth(s32 id)
{
    return http_set_any_u32_locked(id, 1, offsetof(HttpTemplate, auth_enabled));
}

s32 sceHttpDisableAuth(s32 id)
{
    return http_set_any_u32_locked(id, 0, offsetof(HttpTemplate, auth_enabled));
}

s32 sceHttp_CDB0DC58(s32 id)
{
    return http_set_any_u16_locked(id, 1, offsetof(HttpTemplate, proxy_auth_enabled));
}

s32 sceHttp_951D310E(s32 id)
{
    return http_set_any_u16_locked(id, 0, offsetof(HttpTemplate, proxy_auth_enabled));
}

s32 sceHttp_D80BE761(s32 id)
{
    return http_set_any_u16_locked(id, 1, offsetof(HttpTemplate, allow_http09));
}

s32 sceHttp_C0E69162(s32 id)
{
    return http_set_any_u16_locked(id, 0, offsetof(HttpTemplate, allow_http09));
}

s32 sceHttpEnableRedirect(s32 id)
{
    return http_set_tmpl_or_conn_u32(id, 1, offsetof(HttpTemplate, auto_redirect));
}

s32 sceHttpDisableRedirect(s32 id)
{
    return http_set_tmpl_or_conn_u32(id, 0, offsetof(HttpTemplate, auto_redirect));
}

s32 sceHttpChangeHttpVersion(s32 id, s32 version)
{
    s32 type;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    type = http_handle_type(id);
    if (type < 0)
        return type;

    if (type == HTTP_HANDLE_TEMPLATE) {
        HttpTemplate *tmpl = NULL;
        ret = http_lock_template(id, &tmpl);
        if (ret >= 0) {
            tmpl->http_version = version;
            http_unlock_template(tmpl);
        }
        return ret;
    }
    if (type == HTTP_HANDLE_CONNECTION) {
        HttpConnection *conn = NULL;
        ret = http_lock_connection(id, &conn);
        if (ret >= 0) {
            conn->cfg.auto_redirect = 0;
            http_unlock_connection(conn);
        }
        return ret;
    }
    if (type == HTTP_HANDLE_REQUEST) {
        HttpRequest *req = NULL;
        ret = http_lock_request(id, &req);
        if (ret >= 0) {
            req->cfg.http_version = version;
            http_unlock_request(req);
        }
        return ret;
    }
    return SCE_HTTP_ERROR_INVALID_ID;
}

s32 sceHttpDisableCache(s32 id)
{
    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1 || http_cache_mem_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    return http_set_any_u32_locked(id, HTTP_CACHE_NONE, offsetof(HttpTemplate, cache_mode));
}

HTTP_ALWAYS_INLINE s32 http_set_callback_pair(s32 id, void *cb, void *arg, u32 cb_off, u32 arg_off, s32 has_arg)
{
    s32 type;
    s32 intr;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2000)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    type = http_handle_type(id);
    if (type < 0)
        return type;

    intr = http_intr_suspend();
    ret = SCE_HTTP_ERROR_INVALID_ID;
    if (type == HTTP_HANDLE_TEMPLATE) {
        HttpTemplate *tmpl = NULL;
        http_lock_template(id, &tmpl);
        if (tmpl != NULL) {
            *(void **)((u8 *)tmpl + cb_off) = cb;
            if (has_arg)
                *(void **)((u8 *)tmpl + arg_off) = arg;
            tmpl->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_CONNECTION) {
        HttpConnection *conn = NULL;
        http_lock_connection(id, &conn);
        if (conn != NULL) {
            *(void **)((u8 *)&conn->cfg + cb_off) = cb;
            if (has_arg)
                *(void **)((u8 *)&conn->cfg + arg_off) = arg;
            conn->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_REQUEST) {
        HttpRequest *req = NULL;
        ret = http_lock_request(id, &req);
        if (req != NULL) {
            *(void **)((u8 *)&req->cfg + cb_off) = cb;
            if (has_arg)
                *(void **)((u8 *)&req->cfg + arg_off) = arg;
            req->ref_count--;
        }
    }
    http_intr_resume(intr);
    return ret;
}

s32 sceHttpSetAuthInfoCB(s32 id, void *cb)
{
    return http_set_callback_pair(id, cb, NULL, offsetof(HttpTemplate, auth_cb_legacy), 0, 0);
}

s32 sceHttpSetAuthInfoCallback(s32 id, void *cb, void *arg)
{
    return http_set_callback_pair(id, cb, arg,
                                  offsetof(HttpTemplate, auth_cb),
                                  offsetof(HttpTemplate, auth_cb_arg), 1);
}

s32 sceHttpSetRedirectCallback(s32 id, void *cb, void *arg)
{
    return http_set_callback_pair(id, cb, arg,
                                  offsetof(HttpTemplate, redirect_cb),
                                  offsetof(HttpTemplate, redirect_cb_arg), 1);
}

s32 sceHttpSetCookieSendCallback(s32 id, void *cb, void *arg)
{
    return http_set_callback_pair(id, cb, arg,
                                  offsetof(HttpTemplate, cookie_send_cb),
                                  offsetof(HttpTemplate, cookie_send_cb_arg), 1);
}

s32 sceHttpSetCookieRecvCallback(s32 id, void *cb, void *arg)
{
    return http_set_callback_pair(id, cb, arg,
                                  offsetof(HttpTemplate, cookie_recv_cb),
                                  offsetof(HttpTemplate, cookie_recv_cb_arg), 1);
}

/* -------------------------------------------------------------------------
 * Extra headers & proxy configuration
 * ------------------------------------------------------------------------- */
s32 http_add_header_internal(s32 id, const char *name, const char *value, s32 mode)
{
    HttpHeaderNode *node = NULL;
    HttpTemplate *cfg = NULL;
    HttpConnection *conn = NULL;
    HttpRequest *req = NULL;
    s32 type;
    s32 ret;
    s32 intr;

    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    if (mode == 0)
        http_delete_header_internal(id, name);

    do {
        ret = http_header_node_alloc(&node, name, value);
        if (ret < 0)
            break;

        type = http_handle_type(id);
        if (type < 0) {
            ret = type;
            break;
        }

        if (type == HTTP_HANDLE_TEMPLATE) {
            ret = http_lock_template(id, &cfg);
            if (ret < 0)
                break;
        } else if (type == HTTP_HANDLE_CONNECTION) {
            ret = http_lock_connection(id, &conn);
            if (ret < 0)
                break;
            cfg = &conn->cfg;
        } else if (type == HTTP_HANDLE_REQUEST) {
            ret = http_lock_request(id, &req);
            if (ret < 0)
                break;
            cfg = &req->cfg;
        } else {
            ret = SCE_HTTP_ERROR_INVALID_ID;
            break;
        }

        intr = http_intr_suspend();
        if (cfg->headers == NULL) {
            cfg->headers = node;
        } else {
            HttpHeaderNode *tail = cfg->headers;
            while (tail->next != NULL)
                tail = tail->next;
            tail->next = node;
        }

        if (req != NULL)
            req->ref_count--;
        else if (conn != NULL)
            conn->ref_count--;
        else if (cfg != NULL)
            cfg->ref_count--;

        http_intr_resume(intr);
        return 0;
    } while (0);

    http_header_list_free(node);
    return ret;
}

s32 http_header_list_remove(HttpHeaderNode **head, const char *name)
{
    HttpHeaderNode *prev = NULL;
    HttpHeaderNode *cur;

    if (head == NULL || name == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    cur = *head;
    while (cur != NULL) {
        if (sceNet_lib_9CFBC7E3(cur->name, name) == 0) {
            if (prev != NULL)
                prev->next = cur->next;
            else
                *head = cur->next;
            http_free(cur->name);
            http_free(cur->value);
            http_free(cur);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    return SCE_HTTP_ERROR_NOT_FOUND;
}

s32 http_delete_header_internal(s32 id, const char *name)
{
    HttpTemplate *cfg = NULL;
    HttpConnection *conn = NULL;
    HttpRequest *req = NULL;
    s32 type;
    s32 ret;
    s32 removed = 0;
    s32 intr;

    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    if (name == NULL)
        return SCE_HTTP_ERROR_NOT_FOUND;

    type = http_handle_type(id);
    if (type < 0)
        return type;

    if (type == HTTP_HANDLE_TEMPLATE) {
        ret = http_lock_template(id, &cfg);
        if (ret < 0)
            return ret;
    } else if (type == HTTP_HANDLE_CONNECTION) {
        ret = http_lock_connection(id, &conn);
        if (ret < 0)
            return ret;
        cfg = &conn->cfg;
    } else if (type == HTTP_HANDLE_REQUEST) {
        ret = http_lock_request(id, &req);
        if (ret < 0)
            return ret;
        cfg = &req->cfg;
    } else {
        return SCE_HTTP_ERROR_INVALID_ID;
    }

    intr = http_intr_suspend();
    while (http_header_list_remove(&cfg->headers, name) >= 0)
        removed = 1;

    if (req != NULL)
        req->ref_count--;
    else if (conn != NULL)
        conn->ref_count--;
    else if (cfg != NULL)
        cfg->ref_count--;

    http_intr_resume(intr);
    return removed ? 0 : SCE_HTTP_ERROR_NOT_FOUND;
}

s32 sceHttpSetProxy(s32 id, s32 use_proxy, s32 proxy_mode, const char *server, u16 port)
{
    char *server_copy = NULL;
    HttpTemplate *cfg = NULL;
    HttpConnection *conn = NULL;
    s32 type;
    s32 ret;
    s32 intr;

    if (sceKernelCheckThreadStack() < 10192)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    if (proxy_mode == 1) {
        u32 len;
        if (server == NULL)
            return SCE_HTTP_ERROR_INVALID_VALUE;
        len = sceNet_lib_D8722983(server);
        if (len + 1 >= 129)
            return SCE_HTTP_ERROR_OUT_OF_SIZE;
        server_copy = (char *)http_malloc(len + 1);
        if (server_copy == NULL)
            return SCE_HTTP_ERROR_OUT_OF_MEMORY;
        sceNet_lib_80C9F02A(server_copy, server);
        http_strip_proxy_host(server_copy);
    }

    do {
        type = http_handle_type(id);
        if (type < 0) {
            ret = type;
            break;
        }

        if (type == HTTP_HANDLE_TEMPLATE) {
            ret = http_lock_template(id, &cfg);
            if (ret < 0)
                break;
        } else if (type == HTTP_HANDLE_CONNECTION) {
            ret = http_lock_connection(id, &conn);
            if (ret < 0)
                break;
            cfg = &conn->cfg;
            conn->resolved = 0;
        } else {
            ret = SCE_HTTP_ERROR_INVALID_ID;
            break;
        }

        ret = http_refresh_apctl_proxy(cfg);
        if (ret < 0)
            break;

        intr = http_intr_suspend();
        if (use_proxy != 0) {
            if (proxy_mode == 0) {
                cfg->use_default_proxy = 1;
            } else if (proxy_mode == 1) {
                if (cfg->proxy_server != NULL)
                    http_free(cfg->proxy_server);
                cfg->proxy_server = server_copy;
                server_copy = NULL;
                cfg->proxy_port = port;
                cfg->use_proxy = 1;
                cfg->use_default_proxy = 0;
            }
        } else {
            cfg->use_proxy = 0;
            cfg->use_default_proxy = 0;
        }

        if (conn != NULL)
            conn->ref_count--;
        else if (cfg != NULL)
            cfg->ref_count--;
        http_intr_resume(intr);
        return 0;
    } while (0);

    if (conn != NULL)
        http_unlock_connection(conn);
    else if (cfg != NULL)
        http_unlock_template(cfg);
    if (server_copy != NULL)
        http_free(server_copy);
    return ret;
}

s32 sceHttpGetProxy(s32 id, s32 *use_proxy, s32 *proxy_mode, char *server, u32 server_len, u16 *port)
{
    HttpTemplate *cfg = NULL;
    HttpConnection *conn = NULL;
    HttpRequest *req = NULL;
    s32 type;
    s32 ret;
    s32 intr;

    if (sceKernelCheckThreadStack() < 2000)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    if (use_proxy == NULL || proxy_mode == NULL || server == NULL || port == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;
    if (server_len < 128)
        return SCE_HTTP_ERROR_OUT_OF_SIZE;

    type = http_handle_type(id);
    if (type < 0)
        return type;

    if (type == HTTP_HANDLE_TEMPLATE) {
        ret = http_lock_template(id, &cfg);
        if (ret < 0)
            return ret;
    } else if (type == HTTP_HANDLE_CONNECTION) {
        ret = http_lock_connection(id, &conn);
        if (ret < 0)
            return ret;
        cfg = &conn->cfg;
    } else if (type == HTTP_HANDLE_REQUEST) {
        ret = http_lock_request(id, &req);
        if (ret < 0)
            return ret;
        cfg = &req->conn->cfg;
    } else {
        return SCE_HTTP_ERROR_INVALID_ID;
    }

    intr = http_intr_suspend();
    if (cfg->use_proxy == 1) {
        *use_proxy = 1;
        if (cfg->proxy_server != NULL) {
            sceNet_lib_B5CE388A(server, cfg->proxy_server, 128);
            *port = cfg->proxy_port;
        }
    } else {
        *use_proxy = 0;
    }
    *proxy_mode = (cfg->use_default_proxy != 1);

    if (req != NULL)
        req->ref_count--;
    else if (conn != NULL)
        conn->ref_count--;
    else if (cfg != NULL)
        cfg->ref_count--;
    http_intr_resume(intr);
    return 0;
}

s32 sceHttpEnableCache(s32 id)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1 || http_cache_mem_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    return http_set_cache_mode(id, HTTP_CACHE_MEM);
}

s32 sceHttp_8046E250(s32 id)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1 || http_cache_pool_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    return http_set_cache_mode(id, HTTP_CACHE_POOL);
}

s32 sceHttpAddExtraHeader(s32 id, const char *name, const char *value, s32 mode)
{
    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_add_header_internal(id, name, value, mode);
}

s32 sceHttpDeleteHeader(s32 id, const char *name)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_delete_header_internal(id, name);
}

s32 http_set_cache_mode(s32 id, s32 mode)
{
    s32 type;
    s32 intr;
    s32 ret;
    HttpTemplate *cfg = NULL;
    HttpConnection *conn = NULL;
    HttpRequest *req = NULL;

    type = http_handle_type(id);
    if (type < 0)
        return type;

    intr = http_intr_suspend();
    if (type == HTTP_HANDLE_TEMPLATE) {
        ret = http_lock_template(id, &cfg);
        if (cfg != NULL) {
            cfg->cache_mode = mode;
            if (cfg->cache_content_max_size == 0) {
                if (mode == HTTP_CACHE_MEM)
                    cfg->cache_content_max_size = http_cache_mem_get_max_size();
                else if (mode == HTTP_CACHE_POOL)
                    cfg->cache_content_max_size = http_cache_pool_get_max_size();
            }
            cfg->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_CONNECTION) {
        ret = http_lock_connection(id, &conn);
        if (conn != NULL) {
            conn->cfg.cache_mode = mode;
            if (conn->cfg.cache_content_max_size == 0) {
                if (mode == HTTP_CACHE_MEM)
                    conn->cfg.cache_content_max_size = http_cache_mem_get_max_size();
                else if (mode == HTTP_CACHE_POOL)
                    conn->cfg.cache_content_max_size = http_cache_pool_get_max_size();
            }
            conn->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_REQUEST) {
        ret = http_lock_request(id, &req);
        if (req != NULL) {
            req->cfg.cache_mode = mode;
            if (req->cfg.cache_content_max_size == 0) {
                if (mode == HTTP_CACHE_MEM)
                    req->cfg.cache_content_max_size = http_cache_mem_get_max_size();
                else if (mode == HTTP_CACHE_POOL)
                    req->cfg.cache_content_max_size = http_cache_pool_get_max_size();
            }
            req->ref_count--;
        }
    } else {
        ret = SCE_HTTP_ERROR_INVALID_ID;
    }
    http_intr_resume(intr);
    return ret;
}

/* -------------------------------------------------------------------------
 * 301 Redirect cache
 * ------------------------------------------------------------------------- */
HTTP_ALWAYS_INLINE void http_redir_unlink_and_free(HttpRedirEntry *entry)
{
    if (entry->prev != NULL) {
        entry->prev->next = entry->next;
        if (entry->next != NULL)
            entry->next->prev = entry->prev;
        else
            g_redir_tail = entry->prev;
    } else {
        g_redir_head = entry->next;
        if (entry->next != NULL)
            entry->next->prev = NULL;
        else
            g_redir_tail = NULL;
    }

    http_free(entry->src_url);
    http_free(entry->dst_url);
    sceKernelMemset(entry, 0, sizeof(*entry));
    entry->free_next = g_redir_free_list;
    g_redir_free_list = entry;
}

HTTP_ALWAYS_INLINE void http_cache_mem_unlink_and_free(HttpCacheEntry *entry)
{
    http_cache_hash_remove(g_cache_mem_buckets, entry);
    if (entry->lru_prev != NULL) {
        entry->lru_prev->lru_next = entry->lru_next;
        if (entry->lru_next != NULL)
            entry->lru_next->lru_prev = entry->lru_prev;
        else
            g_cache_mem_tail = entry->lru_prev;
    } else {
        g_cache_mem_head = entry->lru_next;
        if (entry->lru_next != NULL)
            entry->lru_next->lru_prev = NULL;
        else
            g_cache_mem_tail = NULL;
    }

    http_cache_mem_account_free(entry->data);
    http_cache_data_free(entry->data);
    http_free(entry->url);
    http_free(entry);
}

s32 http_redir_cache_init(void)
{
    u32 idx;

    if (g_redir_inited == 1)
        return SCE_HTTP_ERROR_ALREADY_INITED;

    g_redir_inited = 1;
    sceKernelMemset(g_redir_pool, 0, sizeof(g_redir_pool));
    g_redir_free_list = &g_redir_pool[0];
    for (idx = 0; idx < 15; idx++)
        g_redir_pool[idx].free_next = &g_redir_pool[idx + 1];
    g_redir_pool[15].free_next = NULL;
    g_redir_head = NULL;
    g_redir_tail = NULL;
    return 0;
}

s32 http_redir_cache_term(void)
{
    HttpRedirEntry *entry;

    if (g_redir_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    for (entry = g_redir_head; entry != NULL; entry = entry->next) {
        http_free(entry->dst_url);
        http_free(entry->src_url);
    }

    g_redir_inited = 0;
    sceKernelMemset(g_redir_pool, 0, sizeof(g_redir_pool));
    g_redir_free_list = NULL;
    g_redir_head = NULL;
    g_redir_tail = NULL;
    return 0;
}

HttpRedirEntry *http_redir_cache_lookup_req(HttpRequest *req)
{
    HttpRedirEntry *entry = NULL;
    char *url = http_build_request_url(req);

    if (url != NULL) {
        s32 intr = http_intr_suspend();
        entry = http_redir_cache_find_unlocked(url, req->method, NULL);
        http_intr_resume(intr);
        http_free(url);
    }
    return entry;
}

void http_redir_cache_add(const char *src_url, s32 method, const u8 *extra_md5, const char *dst_url)
{
    s32 intr = http_intr_suspend();
    HttpRedirEntry *existing = http_redir_cache_find_unlocked(src_url, method, extra_md5);
    HttpRedirEntry *slot;

    if (existing != NULL) {
        if (sceNet_lib_A0F16ABD(existing->dst_url, dst_url) == 0) {
            http_redir_entry_release(existing);
            http_intr_resume(intr);
            return;
        }
        http_redir_entry_mark_deleted(existing);
        http_redir_entry_release(existing);
    }

    while ((slot = g_redir_free_list) == NULL) {
        HttpRedirEntry *victim = g_redir_tail;
        while (victim != NULL && victim->ref_count != 0)
            victim = victim->prev;
        if (victim == NULL) {
            http_intr_resume(intr);
            return;
        }
        http_redir_entry_mark_deleted(victim);
    }

    g_redir_free_list = slot->free_next;
    slot->src_url = (char *)http_malloc(sceNet_lib_D8722983(src_url) + 1);
    if (slot->src_url != NULL) {
        sceNet_lib_80C9F02A(slot->src_url, src_url);
        slot->dst_url = (char *)http_malloc(sceNet_lib_D8722983(dst_url) + 1);
        if (slot->dst_url != NULL) {
            sceNet_lib_80C9F02A(slot->dst_url, dst_url);
            http_md5_digest(src_url, sceNet_lib_D8722983(src_url), slot->url_md5);
            slot->method = method;
            if (extra_md5 != NULL)
                sceKernelMemcpy(slot->extra_md5, extra_md5, 16);
            slot->prev = NULL;
            slot->next = g_redir_head;
            if (g_redir_head != NULL)
                g_redir_head->prev = slot;
            else
                g_redir_tail = slot;
            g_redir_head = slot;
            http_intr_resume(intr);
            return;
        }
        http_free(slot->src_url);
    }

    sceKernelMemset(slot, 0, sizeof(*slot));
    slot->free_next = g_redir_free_list;
    g_redir_free_list = slot;
    http_intr_resume(intr);
}

HttpRedirEntry *http_redir_cache_find(const char *url, s32 method, const u8 *extra_md5)
{
    s32 intr = http_intr_suspend();
    HttpRedirEntry *entry = http_redir_cache_find_unlocked(url, method, extra_md5);
    http_intr_resume(intr);
    return entry;
}

void http_redir_entry_release_locked(HttpRedirEntry *entry)
{
    s32 intr = http_intr_suspend();
    http_redir_entry_release(entry);
    http_intr_resume(intr);
}

void http_redir_entry_invalidate_locked(HttpRedirEntry *entry)
{
    s32 intr = http_intr_suspend();
    http_redir_entry_mark_deleted(entry);
    http_redir_entry_release(entry);
    http_intr_resume(intr);
}

void http_redir_entry_release(HttpRedirEntry *entry)
{
    entry->ref_count--;
    if (entry->ref_count == 0 && entry->deleted == 1)
        http_redir_unlink_and_free(entry);
}

void http_cache_mem_entry_release(HttpCacheEntry *entry)
{
    entry->ref_count--;
    if (entry->ref_count == 0 && entry->deleted == 1)
        http_cache_mem_unlink_and_free(entry);
}

void http_redir_entry_mark_deleted(HttpRedirEntry *entry)
{
    entry->deleted = 1;
    if (entry->ref_count == 0)
        http_redir_unlink_and_free(entry);
}

void http_cache_mem_entry_mark_deleted(HttpCacheEntry *entry)
{
    entry->deleted = 1;
    if (entry->ref_count == 0)
        http_cache_mem_unlink_and_free(entry);
}

HttpRedirEntry *http_redir_cache_find_unlocked(const char *url, s32 method, const u8 *extra_md5)
{
    u8 digest[16];
    HttpRedirEntry *entry;

    http_md5_digest(url, sceNet_lib_D8722983(url), digest);
    for (entry = g_redir_head; entry != NULL; entry = entry->next) {
        if (sceNet_lib_E0A81C7C(entry->url_md5, digest, 16) != 0)
            continue;
        if (sceNet_lib_A0F16ABD(entry->src_url, url) != 0)
            continue;
        if (entry->method != method)
            continue;
        if (extra_md5 != NULL && sceNet_lib_E0A81C7C(entry->extra_md5, extra_md5, 16) != 0)
            continue;
        if (entry->deleted == 1)
            continue;

        entry->hit_count++;
        entry->ref_count++;

        if (entry->prev != NULL) {
            entry->prev->next = entry->next;
            if (entry->next != NULL)
                entry->next->prev = entry->prev;
            else
                g_redir_tail = entry->prev;
        } else {
            g_redir_head = entry->next;
            if (entry->next != NULL)
                entry->next->prev = NULL;
            else
                g_redir_tail = NULL;
        }
        entry->prev = NULL;
        entry->next = g_redir_head;
        if (g_redir_head != NULL)
            g_redir_head->prev = entry;
        else
            g_redir_tail = entry;
        g_redir_head = entry;
        return entry;
    }
    return NULL;
}

/* -------------------------------------------------------------------------
 * Library lifecycle
 * ------------------------------------------------------------------------- */
s32 sceHttpInit(u32 pool_size)
{
    s32 ret;

    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_http_inited == 1)
        return SCE_HTTP_ERROR_ALREADY_INITED;

    http_template_list_init();
    http_connection_list_init();
    http_request_list_init();
    http_redir_cache_init();
    http_auth_cache_init();
    http_cookie_jar_init();

    ret = http_pool_init(pool_size);
    if (ret < 0)
        return ret;

    g_http_inited = 1;
    return 0;
}

s32 sceHttpEnd(void)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_http_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    http_cookie_jar_term();
    http_redir_cache_term();
    http_auth_cache_term();
    http_pool_term();
    g_http_inited = 0;
    return 0;
}

s32 http_is_inited(void)
{
    return g_http_inited;
}

s32 module_reboot_before(void)
{
    while (http_storage_lock() < 0)
        sceKernelDelayThread(100000);
    return 0;
}

/* -------------------------------------------------------------------------
 * Chunked transfer-encoding & DNS resolution
 * ------------------------------------------------------------------------- */
s32 http_recv_chunked(HttpRequest *req, u8 *dst, u32 len, s32 flags)
{
    u32 remaining = len;
    u32 total = 0;
    HttpConnection *conn;
    s32 ret;

    if (req->state != HTTP_REQ_RECEIVING)
        return 0;

    conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    if (len == 0)
        return 0;

    while (remaining != 0 && req->state == HTTP_REQ_RECEIVING) {
        u32 chunk_avail = req->chunk_remaining;

        if (chunk_avail == 0) {
            ret = http_read_chunk_size(req, &req->chunk_remaining, flags);
            if (ret < 0) {
                req->chunk_remaining = 0;
                req->res_info.is_chunked = 0;
                return ret;
            }
            chunk_avail = req->chunk_remaining;
            if (chunk_avail == 0) {
                ret = http_skip_chunk_trailers(req, flags);
                if (ret < 0) {
                    req->chunk_remaining = 0;
                    req->res_info.is_chunked = 0;
                    return ret;
                }
                if (req->res_info.conn_close == 1)
                    http_close_connection(conn);
                req->res_info.is_chunked = 0;
                req->state = HTTP_REQ_DONE;
                break;
            }
        }

        {
            u32 to_read = remaining < chunk_avail ? remaining : chunk_avail;
            ret = http_recv_raw(req, dst + total, to_read, flags);
            if (ret < 0) {
                req->chunk_remaining = 0;
                req->res_info.is_chunked = 0;
                return ret;
            }
            total += (u32)ret;
            remaining -= (u32)ret;
            req->chunk_remaining -= (u32)ret;
            if (req->chunk_remaining == 0) {
                ret = http_skip_crlf_line(req, flags);
                if (ret < 0) {
                    req->chunk_remaining = 0;
                    req->res_info.is_chunked = 0;
                    return ret;
                }
            }
        }
    }

    return (s32)total;
}

s32 http_recv_raw(HttpRequest *req, void *dst, u32 len, s32 flags)
{
    return http_buffered_recv(req, (u8 *)dst, len, flags, 0);
}

s32 http_read_chunk_size(HttpRequest *req, u32 *size_out, s32 flags)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    char buf[16];
    char lf;
    s32 idx = 0;
    s32 found_delim = 0;
    s32 ret;

    while (idx < 11) {
        ret = http_recv_raw(req, &buf[idx], 1, flags);
        if (ret < 0)
            return ret;
        if (ret == 0) {
            http_close_connection(conn);
            return SCE_HTTP_ERROR_BAD_RESPONSE;
        }
        if (buf[idx] == ' ' || buf[idx] == '\r') {
            found_delim = 1;
            break;
        }
        idx++;
    }

    if (!found_delim)
        return 0;

    if (buf[idx] == ' ') {
        ret = http_skip_crlf_line(req, flags);
        if (ret < 0)
            return ret;
    } else if (buf[idx] == '\r') {
        ret = http_recv_raw(req, &lf, 1, flags);
        if (ret < 0)
            return ret;
        if (ret == 0 || lf != '\n') {
            http_close_connection(conn);
            return SCE_HTTP_ERROR_BAD_RESPONSE;
        }
    }

    buf[idx] = '\0';
    *size_out = sceNet_lib_2A73ADDC(buf, NULL, 16);
    return 0;
}

s32 http_transport_recv(HttpConnection *conn, void *buf, u32 len, s32 flags)
{
    if (conn->scheme != HTTP_SCHEME_HTTPS || conn->state == HTTP_CONN_PROXY_TUNNEL)
        return sceNetInetRecv(conn->sock_fd, buf, len, flags);
    return http_ssl_recv(conn, buf, (s32)len);
}

s32 http_resolve_connection_host(HttpConnection *conn)
{
    char proxy_buf[128];
    const char *target_host;
    void *res_buf = NULL;
    s32 rid = 0;
    s32 ret;
    s32 intr;

    if (conn->cfg.use_default_proxy == 1) {
        ret = http_refresh_apctl_proxy(&conn->cfg);
        if (ret < 0)
            return ret;
    }

    intr = http_intr_suspend();
    if (conn->cfg.use_proxy == 1) {
        sceNet_lib_B5CE388A(proxy_buf, conn->cfg.proxy_server, sizeof(proxy_buf));
        target_host = proxy_buf;
    } else {
        target_host = conn->host;
    }
    http_intr_resume(intr);

    ret = http_inet_pton(2, target_host, &conn->addr.addr);
    if (ret != 1) {
        res_buf = http_malloc(768);
        if (res_buf == NULL)
            return SCE_HTTP_ERROR_OUT_OF_MEMORY;

        ret = sceNetResolverCreate(&rid, res_buf, 768);
        if (ret >= 0) {
            conn->state = HTTP_CONN_RESOLVING;
            conn->resolver_id = rid;
            ret = sceNetResolverStartNtoA(rid, target_host, &conn->addr.addr,
                                          conn->cfg.resolve_timeout, conn->cfg.resolve_retry);
            conn->state = HTTP_CONN_CONNECTING;
            if (ret < 0) {
                sceNetResolverDelete(rid);
                http_free(res_buf);
                return ret;
            }
            ret = sceNetResolverDelete(rid);
            if (ret < 0) {
                http_free(res_buf);
                return ret;
            }
        } else {
            http_free(res_buf);
            return ret;
        }
    }

    conn->addr.family = 2;
    conn->resolved = 1;
    conn->addr.port = http_bswap16(conn->cfg.use_proxy == 1 ? conn->cfg.proxy_port : conn->port);

    if (res_buf != NULL)
        http_free(res_buf);
    return 0;
}
