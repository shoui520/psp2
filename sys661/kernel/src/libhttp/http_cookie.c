#include "libhttp_internal.h"

/* -------------------------------------------------------------------------
 * Cookie jar globals
 * ------------------------------------------------------------------------- */
s32               g_cookie_inited;
s32               g_cookie_loaded;
u32               g_cookie_max_bytes = 81920;
u32               g_cookie_cur_bytes;
u32               g_cookie_max_single = 4096;
u32               g_cookie_max_meta = 512;
u32               g_cookie_max_per_domain = 20;
u32               g_cookie_max_total = 300;
u32               g_cookie_cur_total;
HttpCookieDomain *g_cookie_lru_head;
HttpCookieDomain *g_cookie_lru_tail;
HttpCookieDomain *g_cookie_buckets[16];

/* -------------------------------------------------------------------------
 * Auth cache helpers & persistent storage
 * ------------------------------------------------------------------------- */
static void http_auth_promote_lru(HttpAuthEntry *entry)
{
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
    entry->prev = NULL;
    entry->next = g_auth_head;
    if (g_auth_head != NULL)
        g_auth_head->prev = entry;
    else
        g_auth_tail = entry;
    g_auth_head = entry;
}

HttpAuthEntry *http_auth_cache_find(HttpRequest *req, u32 auth_type, const char *realm)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    HttpAuthEntry *best = NULL;
    HttpAuthEntry *cur;
    u8 realm_digest[16];
    s32 intr = http_intr_suspend();

    for (cur = g_auth_head; cur != NULL; cur = cur->next) {
        if (cur->deleted == 1)
            continue;

        sceKernelMemset(realm_digest, 0, sizeof(realm_digest));
        if (realm != NULL && sceNet_lib_D8722983(realm) != 0)
            http_md5_digest(realm, sceNet_lib_D8722983(realm), realm_digest);

        if (sceNet_lib_E0A81C7C(cur->realm_md5, realm_digest, 16) != 0)
            continue;
        if ((u32)cur->auth_type != auth_type)
            continue;

        if (auth_type < HTTP_AUTH_PROXY_BASIC) {
            if (conn->scheme == cur->scheme &&
                conn->port == cur->port &&
                sceNet_lib_A0F16ABD(conn->host, cur->host) == 0 &&
                http_path_prefix_match(cur->path, req->path) == 1) {
                if (best == NULL ||
                    sceNet_lib_D8722983(cur->path) >= sceNet_lib_D8722983(best->path)) {
                    best = cur;
                }
            }
        } else {
            if (conn->cfg.proxy_port == cur->port &&
                conn->cfg.proxy_server != NULL &&
                sceNet_lib_A0F16ABD(conn->cfg.proxy_server, cur->host) == 0) {
                best = cur;
            }
        }
    }

    if (best != NULL) {
        best->hit_count++;
        best->ref_count++;
        http_auth_promote_lru(best);
    }

    http_intr_resume(intr);
    return best;
}

HttpAuthEntry *http_auth_cache_alloc(HttpRequest *req, s32 auth_type, const char *realm)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    HttpAuthEntry *slot = NULL;
    s32 intr = http_intr_suspend();

    while ((slot = g_auth_free_list) == NULL) {
        HttpAuthEntry *victim = g_auth_tail;
        while (victim != NULL && victim->ref_count != 0)
            victim = victim->prev;
        if (victim == NULL)
            break;
        http_auth_entry_mark_deleted(victim);
    }
    if (slot != NULL)
        g_auth_free_list = slot->free_next;
    http_intr_resume(intr);

    if (slot == NULL)
        return NULL;

    if (auth_type == HTTP_AUTH_PROXY_BASIC || auth_type == HTTP_AUTH_PROXY_DIGEST) {
        u32 host_len = sceNet_lib_D8722983(conn->host);
        slot->host = (char *)http_malloc(host_len + 1);
        if (slot->host == NULL)
            goto fail;
        sceNet_lib_80C9F02A(slot->host, conn->cfg.proxy_server);
        slot->port = conn->cfg.proxy_port;
    } else {
        const char *path_src;
        const char *slash;
        u32 host_len = sceNet_lib_D8722983(conn->host);

        slot->host = (char *)http_malloc(host_len + 1);
        if (slot->host == NULL)
            goto fail;
        sceNet_lib_80C9F02A(slot->host, conn->host);

        path_src = (req->path != NULL && sceNet_lib_D8722983(req->path) != 0) ? req->path : "/";
        slash = sceNet_lib_F48963C6(path_src, '/');
        if (slash == NULL)
            goto fail;
        {
            u32 dir_len = (u32)(slash - path_src) + 1;
            slot->path = (char *)http_malloc(dir_len + 1);
            if (slot->path == NULL)
                goto fail;
            sceKernelMemcpy(slot->path, path_src, dir_len);
            slot->path[dir_len] = '\0';
        }
        slot->port = conn->port;
    }

    if (realm != NULL && sceNet_lib_D8722983(realm) != 0)
        http_md5_digest(realm, sceNet_lib_D8722983(realm), slot->realm_md5);

    slot->auth_type = auth_type;
    slot->scheme = conn->scheme;
    slot->status = 0;
    return slot;

fail:
    if (slot->host != NULL)
        http_free(slot->host);
    if (slot->path != NULL)
        http_free(slot->path);
    sceKernelMemset(slot, 0, sizeof(*slot));
    slot->free_next = g_auth_free_list;
    g_auth_free_list = slot;
    return NULL;
}

void http_auth_cache_insert(HttpAuthEntry *entry)
{
    s32 intr = http_intr_suspend();

    entry->prev = NULL;
    entry->next = g_auth_head;
    if (g_auth_head != NULL)
        g_auth_head->prev = entry;
    else
        g_auth_tail = entry;
    g_auth_head = entry;
    http_intr_resume(intr);
}

s32 sceHttp_77EE5319(void)
{
    HttpAuthEntry *entry = NULL;
    u32 rem_size = 0;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 2000)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_storage_lock();
    if (ret < 0)
        return ret;

    if (http_storage_verify(0, &rem_size) == 1 &&
        http_storage_open_body(0, 1, 0) >= 0 && rem_size != 0) {
        while (rem_size != 0) {
            u8 tag;
            u16 field_len;

            if (entry == NULL) {
                while ((entry = g_auth_free_list) == NULL) {
                    HttpAuthEntry *victim = g_auth_tail;
                    while (victim != NULL && victim->ref_count != 0)
                        victim = victim->prev;
                    if (victim == NULL)
                        goto finish;
                    http_auth_entry_mark_deleted(victim);
                }
                g_auth_free_list = entry->free_next;
            }

            if (rem_size < 3)
                break;
            if (http_storage_read_tlv_hdr(0, &tag, &field_len) < 0)
                break;
            rem_size -= 3;
            if (rem_size < field_len)
                break;

            switch (tag) {
            case 1: {
                s32 auth_type;
                if (field_len != 4 || http_storage_read_exact(0, &auth_type, 4) < 0)
                    goto abort_entry;
                entry->auth_type = auth_type;
                break;
            }
            case 2:
                if (field_len != 4 || http_storage_read_exact(0, &entry->scheme, 4) < 0)
                    goto abort_entry;
                break;
            case 3:
                if (field_len != 2 || http_storage_read_exact(0, &entry->port, 2) < 0)
                    goto abort_entry;
                break;
            case 4:
                if (entry->host != NULL)
                    http_free(entry->host);
                entry->host = (char *)http_malloc(field_len);
                if (entry->host == NULL || http_storage_read_exact(0, entry->host, field_len) < 0)
                    goto abort_entry;
                break;
            case 5:
                if (entry->path != NULL)
                    http_free(entry->path);
                entry->path = (char *)http_malloc(field_len);
                if (entry->path == NULL || http_storage_read_exact(0, entry->path, field_len) < 0)
                    goto abort_entry;
                break;
            case 6:
                if (field_len != 16 || http_storage_read_exact(0, entry->realm_md5, 16) < 0)
                    goto abort_entry;
                break;
            case 7:
                if (field_len != 256 || http_storage_read_exact(0, entry->username, 256) < 0)
                    goto abort_entry;
                break;
            case 8:
                if (field_len != 256 || http_storage_read_exact(0, entry->password, 256) < 0)
                    goto abort_entry;
                break;
            case 255:
                entry->prev = NULL;
                entry->status = 1;
                entry->save_flag = 1;
                entry->next = g_auth_head;
                if (g_auth_head != NULL)
                    g_auth_head->prev = entry;
                else
                    g_auth_tail = entry;
                g_auth_head = entry;
                entry = NULL;
                break;
            default:
                http_storage_seek(0, field_len, field_len, 0, 1);
                break;
            }

            rem_size -= field_len;
        }
    }

abort_entry:
    if (entry != NULL) {
        if (entry->host != NULL)
            http_free(entry->host);
        if (entry->path != NULL)
            http_free(entry->path);
        sceKernelMemset(entry, 0, sizeof(*entry));
        entry->free_next = g_auth_free_list;
        g_auth_free_list = entry;
    }

finish:
    http_storage_close(0);
    http_storage_unlock();
    return 0;
}

static s32 http_storage_write_tlv(s32 id, u8 tag, const void *data, u16 len, u32 *written)
{
    if (http_storage_write_tlv_hdr(id, tag, len) < 0)
        return -1;
    *written += 3;
    if (len != 0) {
        if (http_storage_write_exact(id, data, len) < 0)
            return -1;
        *written += len;
    }
    return 0;
}

s32 sceHttp_DD6E7857(void)
{
    s32 retry = 1;
    u32 total_written = 0;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 2000)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    ret = http_storage_lock();
    if (ret < 0)
        return ret;

    for (;;) {
        s32 ok = 1;
        if (http_storage_open_body(0, 2, 0) >= 0) {
            HttpAuthEntry *cur;
            for (cur = g_auth_tail; cur != NULL; cur = cur->prev) {
                if (cur->save_flag == 1 && cur->deleted != 1) {
                    u16 host_len = (u16)(sceNet_lib_D8722983(cur->host) + 1);
                    u16 path_len = (u16)(sceNet_lib_D8722983(cur->path) + 1);

                    if (http_storage_write_tlv(0, 1, &cur->auth_type, 4, &total_written) < 0 ||
                        http_storage_write_tlv(0, 2, &cur->scheme, 4, &total_written) < 0 ||
                        http_storage_write_tlv(0, 3, &cur->port, 2, &total_written) < 0 ||
                        http_storage_write_tlv(0, 4, cur->host, host_len, &total_written) < 0 ||
                        http_storage_write_tlv(0, 5, cur->path, path_len, &total_written) < 0 ||
                        http_storage_write_tlv(0, 6, cur->realm_md5, 16, &total_written) < 0 ||
                        http_storage_write_tlv(0, 7, cur->username, 256, &total_written) < 0 ||
                        http_storage_write_tlv(0, 8, cur->password, 256, &total_written) < 0 ||
                        http_storage_write_tlv(0, 255, NULL, 0, &total_written) < 0) {
                        ok = 0;
                        break;
                    }
                }
            }
            if (ok && http_storage_close(0) >= 0 && http_storage_commit_checksum(0, total_written) >= 0)
                break;
        }
        http_storage_close(0);
        if (!retry)
            break;
        retry = 0;
        if (http_storage_reset_file(0) != 0)
            break;
    }

    http_storage_unlock();
    return 0;
}

void http_digest_params_free(HttpDigestParams *params)
{
    if (params->buf != NULL) {
        http_free(params->buf);
        params->buf = NULL;
    }
}

void http_auth_entry_release_locked(HttpAuthEntry *entry)
{
    s32 intr = http_intr_suspend();
    http_auth_entry_release(entry);
    http_intr_resume(intr);
}

void http_auth_entry_free_unused(HttpAuthEntry *entry)
{
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

s32 http_digest_append_kv(char *dst, u32 dst_size, u32 *written_out,
                          const char *key, const char *val, s32 do_write)
{
    u32 needed = sceNet_lib_D8722983(key) + 1 + 1 + sceNet_lib_D8722983(val) + 1 + 1 + 1;

    if (written_out != NULL)
        *written_out = needed;

    if (do_write) {
        if (dst_size < needed + 1)
            return SCE_HTTP_ERROR_OUT_OF_SIZE;
        sceNet_lib_5705F6F9(dst, key);
        sceNet_lib_5705F6F9(dst, "=");
        sceNet_lib_5705F6F9(dst, "\"");
        sceNet_lib_5705F6F9(dst, val);
        sceNet_lib_5705F6F9(dst, "\"");
        sceNet_lib_5705F6F9(dst, ",");
        sceNet_lib_5705F6F9(dst, " ");
    }
    return 0;
}

void http_md5_to_hex(const u8 *digest, char *hex_out)
{
    u32 idx;

    for (idx = 0; idx < 16; idx++) {
        u8 hi = digest[idx] >> 4;
        u8 lo = digest[idx] & 0x0F;
        hex_out[2 * idx]     = (char)(hi >= 10 ? hi + 'a' - 10 : hi + '0');
        hex_out[2 * idx + 1] = (char)(lo >= 10 ? lo + 'a' - 10 : lo + '0');
    }
    hex_out[32] = '\0';
}

void http_auth_entry_release(HttpAuthEntry *entry)
{
    entry->ref_count--;
    if (entry->ref_count == 0 && entry->deleted == 1)
        http_auth_entry_mark_deleted(entry);
}

s32 http_path_prefix_match(const char *prefix, const char *path)
{
    u32 prefix_len;

    if (prefix == NULL || sceNet_lib_D8722983(prefix) == 0)
        return 1;
    if (path == NULL || sceNet_lib_D8722983(path) == 0)
        return 0;

    prefix_len = sceNet_lib_D8722983(prefix);
    if (sceNet_lib_D8722983(path) < prefix_len)
        return 0;
    return sceNet_lib_E0A81C7C(prefix, path, prefix_len) == 0;
}

/* -------------------------------------------------------------------------
 * Cookie public API & storage
 * ------------------------------------------------------------------------- */
s32 sceHttpSaveSystemCookie(void)
{
    return 0;
}

static s32 http_storage_read_cstr(s32 id, char **dst, u16 len)
{
    char *buf;
    s32 ret;

    if (*dst != NULL) {
        http_free(*dst);
        *dst = NULL;
    }
    buf = (char *)http_malloc((u32)len + 1);
    if (buf == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    ret = http_storage_read_exact(id, buf, len);
    if (ret < 0) {
        http_free(buf);
        return ret;
    }
    buf[len] = '\0';
    *dst = buf;
    return 0;
}

s32 sceHttpLoadSystemCookie(void)
{
    HttpCookie tmp;
    HttpCookie *created = NULL;
    u64 now_tick = 0;
    u32 rem_size = 0;
    s32 expired = 0;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 1872)
        return SCE_HTTP_ERROR_OUT_OF_STACK;

    sceKernelMemset(&tmp, 0, sizeof(tmp));
    if (http_is_inited() != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    if ((u32)http_pool_get_size() <= 0x207FF)
        return SCE_HTTP_ERROR_POOL_TOO_SMALL;

    g_cookie_loaded = 1;
    ret = http_rtc_get_tick(&now_tick);
    if (ret < 0)
        goto cleanup;

    http_cookie_jar_term();
    http_cookie_jar_init();

    ret = http_storage_lock();
    if (ret < 0)
        return ret;

    if (http_storage_verify(1, &rem_size) == 1) {
        ret = http_storage_open_body(1, 1, 0);
        if (ret >= 0) {
            while (rem_size != 0) {
                u8 tag;
                u16 field_len;

                if (rem_size < 3)
                    goto cleanup;
                ret = http_storage_read_tlv_hdr(1, &tag, &field_len);
                if (ret < 0)
                    goto cleanup;
                rem_size -= 3;
                if (rem_size < field_len)
                    goto cleanup;

                switch (tag) {
                case 1:
                    if (field_len != 8)
                        goto cleanup;
                    ret = http_storage_read_exact(1, &tmp.expires, 8);
                    if (ret < 0)
                        goto cleanup;
                    if (tmp.expires < now_tick)
                        expired = 1;
                    break;
                case 2:
                    ret = http_storage_read_cstr(1, &tmp.domain, field_len);
                    if (ret < 0)
                        goto cleanup;
                    break;
                case 3:
                    ret = http_storage_read_cstr(1, &tmp.name, field_len);
                    if (ret < 0)
                        goto cleanup;
                    break;
                case 4:
                    ret = http_storage_read_cstr(1, &tmp.value, field_len);
                    if (ret < 0)
                        goto cleanup;
                    break;
                case 5:
                    ret = http_storage_read_cstr(1, &tmp.path, field_len);
                    if (ret < 0)
                        goto cleanup;
                    break;
                case 6:
                    ret = http_storage_read_cstr(1, &tmp.comment, field_len);
                    if (ret < 0)
                        goto cleanup;
                    break;
                case 7:
                    ret = http_storage_read_cstr(1, &tmp.comment_url, field_len);
                    if (ret < 0)
                        goto cleanup;
                    break;
                case 8:
                    if (tmp.ports != NULL)
                        http_free(tmp.ports);
                    tmp.ports = (u16 *)http_malloc((u32)field_len + 1);
                    ret = http_storage_read_exact(1, tmp.ports, field_len);
                    if (ret < 0)
                        goto cleanup;
                    break;
                case 9:
                    if (field_len != 4)
                        goto cleanup;
                    ret = http_storage_read_exact(0, &tmp.port_count, 4);
                    if (ret < 0)
                        goto cleanup;
                    break;
                case 255:
                    if (!expired && tmp.domain != NULL && (tmp.name != NULL || tmp.value != NULL)) {
                        ret = http_cookie_alloc_entry(&created, tmp.name, tmp.value,
                                                      tmp.comment, tmp.comment_url,
                                                      tmp.domain, tmp.path,
                                                      tmp.ports, tmp.port_count,
                                                      &tmp.expires, tmp.version,
                                                      tmp.secure, expired, 0);
                        if (ret < 0)
                            goto cleanup;
                        ret = http_cookie_jar_insert(created, created->size_bytes, 1);
                        if (ret < 0)
                            http_cookie_free_fields(created);
                    }
                    http_cookie_free_fields(&tmp);
                    sceKernelMemset(&tmp, 0, sizeof(tmp));
                    expired = 0;
                    created = NULL;
                    break;
                default:
                    http_storage_seek(0, field_len, field_len, 0, 1);
                    break;
                }

                rem_size -= field_len;
            }
            ret = 0;
        }
    }

cleanup:
    http_cookie_free_fields(&tmp);
    http_storage_close(1);
    http_storage_unlock();
    return ret;
}

s32 sceHttpGetCookie(const char *url, u8 *cookie_buf, u32 *len_out, u32 buf_size, s32 is_secure)
{
    HttpCookieSortChunk *chunks = NULL;
    HttpCookieRefNode *refs = NULL;
    void *uri_work = NULL;
    SceUriElement uri;
    u32 uri_work_size = 0;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 1952)
        return SCE_HTTP_ERROR_OUT_OF_STACK;

    ret = sceUriParse(NULL, url, NULL, &uri_work_size, 0);
    if (ret < 0)
        goto cleanup;

    uri_work = http_malloc(uri_work_size);
    if (uri_work == NULL) {
        ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }

    ret = sceUriParse(&uri, url, uri_work, NULL, uri_work_size);
    if (ret >= 0) {
        if (http_cookie_jar_lookup(&chunks, uri.host, uri.path, is_secure == 1, &refs) < 0 || chunks == NULL)
            ret = SCE_HTTP_ERROR_NOT_FOUND;
        else
            ret = http_cookie_format_header(cookie_buf, chunks, len_out, buf_size);
    }

cleanup:
    if (chunks != NULL || refs != NULL)
        http_cookie_lookup_free(chunks, refs);
    if (uri_work != NULL)
        http_free(uri_work);
    return ret;
}

s32 sceHttpAddCookie(const char *url, const char *header_val, u32 header_len)
{
    SceUriElement uri;
    void *uri_work = NULL;
    u32 uri_work_size = 0;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 1952)
        return SCE_HTTP_ERROR_OUT_OF_STACK;

    ret = sceUriParse(NULL, url, NULL, &uri_work_size, 0);
    if (ret < 0)
        return ret;

    uri_work = http_malloc(uri_work_size);
    if (uri_work == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    ret = sceUriParse(&uri, url, uri_work, NULL, uri_work_size);
    if (ret >= 0) {
        ret = http_cookie_add_raw(header_val, header_len, uri.host, uri.path, uri.port, 0);
        if (ret >= 0 && (u32)sceKernelGetCompiledSdkVersion() <= 0x03030010)
            ret = 0;
    }

    http_free(uri_work);
    return ret;
}

/* -------------------------------------------------------------------------
 * Cookie jar internals
 * ------------------------------------------------------------------------- */
void http_kv_list_free(HttpHeaderNode *list)
{
    while (list != NULL) {
        HttpHeaderNode *next = list->next;
        if (list->name != NULL)
            http_free(list->name);
        if (list->value != NULL)
            http_free(list->value);
        http_free(list);
        list = next;
    }
}

u32 http_cookie_free_fields(HttpCookie *cookie)
{
    u32 bytes = cookie->size_bytes;

    if (cookie->comment != NULL) { http_free(cookie->comment); cookie->comment = NULL; }
    if (cookie->comment_url != NULL) { http_free(cookie->comment_url); cookie->comment_url = NULL; }
    if (cookie->domain != NULL) { http_free(cookie->domain); cookie->domain = NULL; }
    if (cookie->name != NULL) { http_free(cookie->name); cookie->name = NULL; }
    if (cookie->path != NULL) { http_free(cookie->path); cookie->path = NULL; }
    if (cookie->value != NULL) { http_free(cookie->value); cookie->value = NULL; }
    if (cookie->ports != NULL) { http_free(cookie->ports); cookie->ports = NULL; }
    return bytes;
}

static void http_cookie_lru_promote(HttpCookieDomain *dom)
{
    if (dom->lru_prev != NULL) {
        dom->lru_prev->lru_next = dom->lru_next;
        if (dom->lru_next != NULL)
            dom->lru_next->lru_prev = dom->lru_prev;
        else
            g_cookie_lru_tail = dom->lru_prev;
    } else {
        g_cookie_lru_head = dom->lru_next;
        if (dom->lru_next != NULL)
            dom->lru_next->lru_prev = NULL;
        else
            g_cookie_lru_tail = NULL;
    }

    dom->lru_prev = NULL;
    dom->lru_next = g_cookie_lru_head;
    if (g_cookie_lru_head != NULL)
        g_cookie_lru_head->lru_prev = dom;
    else
        g_cookie_lru_tail = dom;
    g_cookie_lru_head = dom;
}

s32 http_cookie_jar_insert(HttpCookie *cookies, u32 total_bytes, u32 count)
{
    s32 bucket;
    HttpCookieDomain *dom;
    HttpCookieDomain *prev = NULL;
    s32 found = 0;
    s32 intr;

    if (cookies == NULL)
        return 0;

    bucket = http_cookie_domain_hash(cookies->domain);
    intr = http_intr_suspend();
    dom = g_cookie_buckets[bucket];
    while (dom != NULL) {
        if (sceNet_lib_A0F16ABD(dom->domain, cookies->domain) == 0) {
            http_cookie_domain_acquire(dom);
            found = 1;
            break;
        }
        prev = dom;
        dom = dom->hash_next;
    }
    http_intr_resume(intr);

    if (!found) {
        dom = (HttpCookieDomain *)http_malloc(sizeof(*dom));
        if (dom == NULL)
            return SCE_HTTP_ERROR_OUT_OF_MEMORY;
        sceKernelMemset(dom, 0, sizeof(*dom));

        dom->domain = (char *)http_malloc(sceNet_lib_D8722983(cookies->domain) + 1);
        if (dom->domain == NULL)
            return SCE_HTTP_ERROR_OUT_OF_MEMORY;
        sceNet_lib_80C9F02A(dom->domain, cookies->domain);

        dom->cookies = cookies;
        dom->cookie_count = count;
        if (prev != NULL) {
            dom->hash_prev = prev;
            prev->hash_next = dom;
        } else {
            g_cookie_buckets[bucket] = dom;
        }

        dom->lru_prev = NULL;
        dom->lru_next = g_cookie_lru_head;
        if (g_cookie_lru_head != NULL)
            g_cookie_lru_head->lru_prev = dom;
        else
            g_cookie_lru_tail = dom;
        g_cookie_lru_head = dom;
    } else {
        s32 excess = (s32)(dom->cookie_count + count - g_cookie_max_per_domain);
        if (excess > 0) {
            HttpCookie *cur = dom->cookies;
            intr = http_intr_suspend();
            while (cur != NULL && excess > 0) {
                if (cur->deleted != 1) {
                    cur->deleted = 1;
                    excess--;
                    dom->cookie_count--;
                    g_cookie_cur_total--;
                }
                cur = cur->next;
            }
            http_intr_resume(intr);
        }

        dom->cookie_count += count;
        if (dom->cookies != NULL) {
            HttpCookie *existing = dom->cookies;
            HttpCookie *tail = NULL;
            while (existing != NULL) {
                HttpCookie *nc = cookies;
                while (nc != NULL) {
                    if (nc->deleted != 1 && sceNet_lib_A0F16ABD(nc->name, existing->name) == 0) {
                        existing->deleted = 1;
                        dom->cookie_count--;
                        g_cookie_cur_total--;
                        break;
                    }
                    nc = nc->next;
                }
                tail = existing;
                existing = existing->next;
            }
            tail->next = cookies;
        } else {
            dom->cookies = cookies;
        }

        http_cookie_lru_promote(dom);
        http_cookie_domain_release(dom);
    }

    g_cookie_cur_bytes += total_bytes;
    g_cookie_cur_total += count;
    return 0;
}

s32 sceHttp_457D221D(void)
{
    u32 idx;
    s32 intr;

    if (_sceKernelCheckThreadStack() < 2000)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_cookie_inited == 0)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    intr = http_intr_suspend();
    for (idx = 0; idx < 16; idx++) {
        HttpCookieDomain *dom = g_cookie_buckets[idx];
        while (dom != NULL) {
            HttpCookieDomain *next = dom->hash_next;
            if (http_cookie_domain_acquire(dom) >= 0) {
                dom->deleted = 1;
                g_cookie_cur_total -= dom->cookie_count;
                http_cookie_domain_release(dom);
            }
            dom = next;
        }
    }
    http_intr_resume(intr);
    return 0;
}

void http_cookie_jar_init(void)
{
    if (g_cookie_inited == 0) {
        g_cookie_inited = 1;
        g_cookie_max_bytes = 81920;
        g_cookie_cur_bytes = 0;
        g_cookie_max_single = 4096;
        g_cookie_max_meta = 512;
        g_cookie_max_per_domain = 20;
        g_cookie_max_total = 300;
        g_cookie_lru_head = NULL;
        g_cookie_lru_tail = NULL;
    }
}

void http_cookie_jar_term(void)
{
    u32 idx;

    if (g_cookie_inited != 1)
        return;

    for (idx = 0; idx < 16; idx++) {
        HttpCookieDomain *dom = g_cookie_buckets[idx];
        while (dom != NULL) {
            HttpCookieDomain *next_dom = dom->hash_next;
            HttpCookie *cookie = dom->cookies;
            u32 freed_bytes = 0;

            while (cookie != NULL) {
                HttpCookie *next_cookie = cookie->next;
                freed_bytes += http_cookie_free_fields(cookie);
                http_free(cookie);
                cookie = next_cookie;
            }
            g_cookie_cur_bytes -= freed_bytes;
            if (dom->domain != NULL)
                http_free(dom->domain);
            http_free(dom);
            dom = next_dom;
        }
        g_cookie_buckets[idx] = NULL;
    }

    g_cookie_inited = 0;
    g_cookie_cur_bytes = 0;
    g_cookie_cur_total = 0;
}

s32 http_cookie_domain_acquire(HttpCookieDomain *dom)
{
    if (dom == NULL || dom->deleted == 1)
        return -1;
    dom->ref_count++;
    return 0;
}

void http_cookie_domain_release(HttpCookieDomain *dom)
{
    s32 intr;

    if (dom == NULL)
        return;

    intr = http_intr_suspend();
    dom->ref_count--;
    if (dom->ref_count != 0) {
        http_intr_resume(intr);
        return;
    }

    if (dom->deleted != 1) {
        HttpCookie *prev = NULL;
        HttpCookie *cur = dom->cookies;
        u32 freed_bytes = 0;

        while (cur != NULL) {
            HttpCookie *next = cur->next;
            if (cur->deleted == 1) {
                if (prev != NULL)
                    prev->next = next;
                else
                    dom->cookies = next;
                freed_bytes += http_cookie_free_fields(cur);
                http_free(cur);
            } else {
                prev = cur;
            }
            cur = next;
        }

        if (dom->cookies == NULL) {
            if (dom->domain != NULL)
                http_free(dom->domain);
            http_free(dom);
        }
        g_cookie_cur_bytes -= freed_bytes;
    } else {
        HttpCookie *cur;
        u32 freed_bytes = 0;

        if (dom->lru_prev != NULL) {
            dom->lru_prev->lru_next = dom->lru_next;
            if (dom->lru_next != NULL)
                dom->lru_next->lru_prev = dom->lru_prev;
            else
                g_cookie_lru_tail = dom->lru_prev;
        } else {
            g_cookie_lru_head = dom->lru_next;
            if (dom->lru_next != NULL)
                dom->lru_next->lru_prev = NULL;
            else
                g_cookie_lru_tail = NULL;
        }

        if (dom->hash_next != NULL)
            dom->hash_next->hash_prev = dom->hash_prev;
        if (dom->hash_prev != NULL)
            dom->hash_prev->hash_next = dom->hash_next;
        else
            g_cookie_buckets[http_cookie_domain_hash(dom->domain)] = dom->hash_next;

        dom->hash_prev = NULL;
        dom->hash_next = NULL;

        cur = dom->cookies;
        while (cur != NULL) {
            HttpCookie *next = cur->next;
            freed_bytes += http_cookie_free_fields(cur);
            http_free(cur);
            cur = next;
        }
        g_cookie_cur_bytes -= freed_bytes;
        if (dom->domain != NULL)
            http_free(dom->domain);
        http_free(dom);
    }

    http_intr_resume(intr);
}

s32 http_cookie_jar_lookup(HttpCookieSortChunk **out_chunks, const char *host,
                           const char *path, s32 is_secure, HttpCookieRefNode **out_refs)
{
    HttpCookieSortChunk *chunk = NULL;
    HttpCookieDomain **bucket;
    HttpCookieDomain *dom;
    s32 first_iter = 1;
    s32 ret;

    if (out_chunks == NULL || out_refs == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    *out_chunks = NULL;
    *out_refs = NULL;

    bucket = &g_cookie_buckets[http_cookie_domain_hash(host)];
    if (*bucket == NULL)
        return SCE_HTTP_ERROR_NOT_FOUND;

    chunk = (HttpCookieSortChunk *)http_malloc(sizeof(*chunk));
    if (chunk == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceKernelMemset(chunk, 0, sizeof(*chunk));

    dom = *bucket;
    for (;;) {
        s32 matched = 0;
        s32 intr = http_intr_suspend();

        if (first_iter) {
            dom = *bucket;
            first_iter = 0;
        }
        while (dom != NULL) {
            if (http_cookie_domain_match(dom->domain, host) == 0 &&
                http_cookie_domain_acquire(dom) >= 0) {
                matched = 1;
                break;
            }
            dom = dom->hash_next;
        }
        http_intr_resume(intr);

        if (matched) {
            ret = http_cookie_ref_push(out_refs, dom);
            if (ret < 0) {
                http_cookie_domain_release(dom);
                goto fail;
            }
            http_cookie_lru_promote(dom);
            http_cookie_collect_matching(chunk, dom->cookies, path, is_secure);
        }

        if (dom == NULL || (dom = dom->hash_next) == NULL) {
            if (chunk->nodes[0].cookie != NULL) {
                *out_chunks = chunk;
                return 0;
            }
            ret = SCE_HTTP_ERROR_NOT_FOUND;
            goto fail;
        }
    }

fail:
    if (chunk != NULL || *out_refs != NULL) {
        http_cookie_lookup_free(chunk, *out_refs);
        *out_refs = NULL;
    }
    return ret;
}

void http_cookie_lookup_free(HttpCookieSortChunk *chunks, HttpCookieRefNode *refs)
{
    while (chunks != NULL) {
        HttpCookieSortChunk *next = chunks->next;
        http_free(chunks);
        chunks = next;
    }
    while (refs != NULL) {
        HttpCookieRefNode *next = refs->next;
        http_cookie_domain_release(refs->domain);
        http_free(refs);
        refs = next;
    }
}

s32 http_cookie_format_header(u8 *dst, HttpCookieSortChunk *chunk, u32 *len_out, u32 dst_size)
{
    u32 total_len = 1;
    s32 count = 0;
    s32 do_write = (dst != NULL);
    u64 now_tick = 0;
    HttpCookieSortNode *node;
    s32 ret;

    if (do_write)
        dst[0] = '\0';
    else if (len_out == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    ret = http_rtc_get_tick(&now_tick);
    if (ret < 0)
        return ret;

    node = &chunk->nodes[0];
    while (node->next != NULL)
        node = node->next;

    while (node != NULL) {
        HttpCookie *cookie = node->cookie;
        if (cookie->expires == 0 || cookie->expires >= now_tick) {
            u32 item_len = 0;
            if (cookie->name != NULL && cookie->value != NULL)
                item_len = sceNet_lib_D8722983(cookie->name) + 1 + sceNet_lib_D8722983(cookie->value);
            else if (cookie->name != NULL)
                item_len = sceNet_lib_D8722983(cookie->name);
            else if (cookie->value != NULL)
                item_len = sceNet_lib_D8722983(cookie->value);

            if (cookie->name != NULL || cookie->value != NULL) {
                total_len += item_len + 2;
                if (do_write) {
                    if (dst_size < total_len)
                        return SCE_HTTP_ERROR_OUT_OF_SIZE;
                    if (cookie->name != NULL && cookie->value != NULL) {
                        sceNet_lib_5705F6F9((char *)dst, cookie->name);
                        sceNet_lib_5705F6F9((char *)dst, "=");
                        sceNet_lib_5705F6F9((char *)dst, cookie->value);
                    } else if (cookie->name != NULL) {
                        sceNet_lib_5705F6F9((char *)dst, cookie->name);
                    } else {
                        sceNet_lib_5705F6F9((char *)dst, cookie->value);
                    }
                    sceNet_lib_5705F6F9((char *)dst, "; ");
                }
            }
            count++;
        }
        node = node->prev;
    }

    if (do_write) {
        u32 cur_len = sceNet_lib_D8722983((const char *)dst);
        if (cur_len >= 3)
            dst[cur_len - 2] = '\0';
    }
    if (len_out != NULL)
        *len_out = total_len;
    return count == 0 ? SCE_HTTP_ERROR_NOT_FOUND : 0;
}

s32 http_cookie_process_response(const char *hdr, u32 hdr_len, const char *host,
                                 const char *path, u16 port, HttpRequest *req)
{
    typedef s32 (*CookieRecvCb)(s32, const char *, const char *, u32, void *);
    u32 max_cookies = g_cookie_max_per_domain;
    u32 count = 0;
    u32 off = 0;
    s32 cb_ret = 0;
    s32 ret = 0;

    if (hdr_len != 0 && max_cookies != 0 && req->cfg.cookie_enabled == 1) {
        while (off < hdr_len && count < max_cookies && req->cfg.cookie_enabled == 1) {
            char *val = NULL;
            u32 val_len = 0;
            s32 adv = sceParseHttpResponseHeader(hdr + off, hdr_len, "Set-Cookie", &val, &val_len);
            if (adv < 0)
                break;
            off += (u32)adv;

            if (req->cfg.cookie_recv_cb != NULL) {
                char *url = http_build_request_url(req);
                _sceKernelCheckThreadStack();
                cb_ret = ((CookieRecvCb)req->cfg.cookie_recv_cb)(req->id, url, val, val_len,
                                                                 req->cfg.cookie_recv_cb_arg);
                if (url != NULL)
                    http_free(url);
            }
            if (cb_ret >= 0) {
                count++;
                http_cookie_add_raw(val, val_len, host, path, port, 0);
            }
        }
    }

    if (hdr_len != 0 && count >= max_cookies && req->cfg.cookie_enabled == 1) {
        off = 0;
        while (off < hdr_len && count >= max_cookies && req->cfg.cookie_enabled == 1) {
            char *val = NULL;
            u32 val_len = 0;
            s32 adv = sceParseHttpResponseHeader(hdr + off, hdr_len, "Set-Cookie2", &val, &val_len);
            if (adv < 0) {
                ret = SCE_HTTP_ERROR_NOT_FOUND;
                break;
            }
            off += (u32)adv;

            if (req->cfg.cookie_recv_cb != NULL) {
                char *url = http_build_request_url(req);
                _sceKernelCheckThreadStack();
                cb_ret = ((CookieRecvCb)req->cfg.cookie_recv_cb)(req->id, url, val, val_len,
                                                                 req->cfg.cookie_recv_cb_arg);
                if (url != NULL)
                    http_free(url);
            }
            if (cb_ret >= 0) {
                count++;
                http_cookie_add_raw(val, val_len, host, path, port, 2);
            }
        }
    }

    return http_min_s32(ret, 0);
}

s32 http_cookie_add_raw(const char *header_val, u32 header_len, const char *host,
                        const char *path, u16 port, s32 is_v2)
{
    HttpHeaderNode *kv_list = NULL;
    HttpCookie *cookies = NULL;
    u32 secure = 0;
    u32 discard = 0;
    u32 bytes = 0;
    u32 count = 0;
    s32 ret;

    ret = http_cookie_parse_kv((const u8 *)header_val, header_len, &kv_list, &secure, &discard, is_v2);
    if (ret < 0)
        goto fail;

    ret = http_cookie_build_from_kv(&cookies, &bytes, &count, kv_list, host, path, port,
                                    (s32)secure, (s32)discard, is_v2);
    if (ret < 0)
        goto fail;

    if (cookies == NULL) {
        if (kv_list != NULL)
            http_kv_list_free(kv_list);
        return 0;
    }

    http_cookie_evict_if_needed(bytes, count);
    if (g_cookie_max_bytes - g_cookie_cur_bytes >= bytes) {
        ret = http_cookie_jar_insert(cookies, bytes, count);
        if (ret >= 0) {
            if (kv_list != NULL)
                http_kv_list_free(kv_list);
            return 0;
        }
    }

fail:
    if (kv_list != NULL)
        http_kv_list_free(kv_list);
    while (cookies != NULL) {
        HttpCookie *next = cookies->next;
        http_cookie_free_fields(cookies);
        http_free(cookies);
        cookies = next;
    }
    return ret;
}

s32 http_cookie_ref_push(HttpCookieRefNode **head, HttpCookieDomain *dom)
{
    HttpCookieRefNode *node = (HttpCookieRefNode *)http_malloc(sizeof(*node));

    if (node == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    node->domain = dom;
    node->next = *head;
    *head = node;
    return 0;
}

s32 http_cookie_build_from_kv(HttpCookie **out_list, u32 *out_bytes, u32 *out_count,
                              HttpHeaderNode *kv_list, const char *host, const char *path,
                              u16 port, s32 secure, s32 discard, s32 is_v2)
{
    HttpHeaderNode *pairs = NULL;
    HttpHeaderNode *cur = kv_list;
    HttpCookie *result_list = NULL;
    char *default_path = NULL;
    char *dot_domain = NULL;
    u16 *port_list = NULL;
    const char *domain_val = NULL;
    const char *path_val = NULL;
    const char *comment_val = NULL;
    const char *comment_url_val = NULL;
    u32 port_count = 0;
    s32 version = 0;
    u64 expires = 0;
    u64 max_age = 0;
    u8 ip_tmp[16];
    s32 ret = 0;

    if (out_list == NULL || out_bytes == NULL || out_count == NULL || kv_list == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    *out_bytes = 0;
    *out_count = 0;

    default_path = http_extract_dir_path(path);
    if (default_path == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    while (cur != NULL) {
        if (cur->next == NULL) {
            HttpHeaderNode *pair = (HttpHeaderNode *)http_malloc(sizeof(*pair));
            if (pair == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                goto fail;
            }
            sceKernelMemset(pair, 0, sizeof(*pair));
            pair->next = pairs;
            pairs = pair;

            if (cur->name != NULL) {
                pair->name = (char *)http_malloc(sceNet_lib_D8722983(cur->name) + 1);
                if (pair->name == NULL) {
                    ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                    goto fail;
                }
                sceNet_lib_80C9F02A(pair->name, cur->name);
            }
            if (cur->value != NULL) {
                pair->value = (char *)http_malloc(sceNet_lib_D8722983(cur->value) + 1);
                if (pair->value == NULL) {
                    ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                    goto fail;
                }
                sceNet_lib_80C9F02A(pair->value, cur->value);
            }
        } else if (cur->name != NULL) {
            if (is_v2 == 1 || is_v2 == 2) {
                if (sceNet_lib_9CFBC7E3(cur->name, "Comment") == 0) {
                    comment_val = cur->value;
                    cur = cur->next;
                    continue;
                }
                if (sceNet_lib_9CFBC7E3(cur->name, "Max-Age") == 0) {
                    if (cur->value != NULL) {
                        max_age = http_parse_u64(cur->value);
                        ret = http_rtc_get_tick(&expires);
                        if (ret < 0)
                            goto fail;
                        ret = http_rtc_add_seconds(&expires, &expires, (s64)max_age);
                        if (ret < 0)
                            goto fail;
                    }
                    cur = cur->next;
                    continue;
                }
                if (sceNet_lib_9CFBC7E3(cur->name, "Version") == 0) {
                    if (cur->value != NULL)
                        version = sceNet_lib_1FB2FDDD(cur->value);
                    cur = cur->next;
                    continue;
                }
            }

            if (is_v2 == 2 && sceNet_lib_9CFBC7E3(cur->name, "CommentURL") == 0) {
                comment_url_val = cur->value;
            } else if (is_v2 == 2 && sceNet_lib_9CFBC7E3(cur->name, "Port") == 0) {
                if (cur->value != NULL) {
                    s32 pc = http_parse_port_list_alloc(cur->value, port, &port_list);
                    if (pc <= 0) {
                        ret = SCE_HTTP_ERROR_NOT_FOUND;
                        goto fail;
                    }
                    port_count = (u32)pc;
                }
            } else if (is_v2 == 0 && sceNet_lib_9CFBC7E3(cur->name, "expires") == 0) {
                if (cur->value != NULL && max_age == 0) {
                    if (http_rtc_parse_datetime(&expires, cur->value) < 0)
                        expires = 0;
                }
            } else if (sceNet_lib_9CFBC7E3(cur->name, "Domain") == 0) {
                if (cur->value != NULL) {
                    if (http_cookie_normalize_domain(cur->value) == NULL)
                        goto fail;
                    domain_val = cur->value;
                }
            } else if (sceNet_lib_9CFBC7E3(cur->name, "Path") == 0) {
                if (cur->value != NULL)
                    path_val = cur->value;
            }
        }
        cur = cur->next;
    }

    if (path_val == NULL)
        path_val = default_path;

    if (domain_val == NULL)
        domain_val = host;

    if (domain_val[0] != '.' && http_inet_pton(2, domain_val, ip_tmp) != 1) {
        dot_domain = (char *)http_malloc(sceNet_lib_D8722983(domain_val) + 2);
        if (dot_domain == NULL) {
            ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
            goto fail;
        }
        dot_domain[0] = '.';
        sceNet_lib_80C9F02A(dot_domain + 1, domain_val);
        domain_val = dot_domain;
    }

    if (http_cookie_domain_match(domain_val, host) != 0)
        goto fail;

    {
        HttpHeaderNode *pair = pairs;
        while (pair != NULL) {
            HttpCookie *item = NULL;
            ret = http_cookie_alloc_entry(&item, pair->name, pair->value,
                                          comment_val, comment_url_val,
                                          domain_val, path_val,
                                          port_list, port_count,
                                          &expires, version, secure, discard, is_v2);
            if (ret < 0)
                goto fail;
            (*out_count)++;
            *out_bytes += item->size_bytes;
            item->next = result_list;
            result_list = item;
            pair = pair->next;
        }
    }

    *out_list = result_list;
    if (pairs != NULL)
        http_kv_list_free(pairs);
    if (port_list != NULL)
        http_free(port_list);
    http_free(default_path);
    if (dot_domain != NULL)
        http_free(dot_domain);
    return (s32)*out_bytes;

fail:
    if (pairs != NULL)
        http_kv_list_free(pairs);
    while (result_list != NULL) {
        HttpCookie *next = result_list->next;
        http_cookie_free_fields(result_list);
        http_free(result_list);
        result_list = next;
    }
    if (port_list != NULL)
        http_free(port_list);
    if (default_path != NULL)
        http_free(default_path);
    if (dot_domain != NULL)
        http_free(dot_domain);
    return ret;
}

static s32 http_cookie_dup_field(char **dst, const char *src, u32 *size_acc, u32 max_limit)
{
    u32 len = sceNet_lib_D8722983(src) + 1;
    char *copy;

    *size_acc += len;
    if (max_limit < *size_acc)
        return SCE_HTTP_ERROR_OUT_OF_SIZE;
    copy = (char *)http_malloc(len);
    if (copy == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceNet_lib_80C9F02A(copy, src);
    *dst = copy;
    return 0;
}

s32 http_cookie_alloc_entry(HttpCookie **out_cookie, const char *name, const char *value,
                            const char *comment, const char *comment_url,
                            const char *domain, const char *path,
                            const u16 *ports, u32 port_count,
                            const u64 *expires, s32 version, s32 secure,
                            s32 discard, s32 is_v2)
{
    HttpCookie *cookie;
    u32 size_acc = 0;
    u32 meta_limit = g_cookie_max_single + g_cookie_max_meta;
    s32 ret;

    cookie = (HttpCookie *)http_malloc(sizeof(*cookie));
    if (cookie == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceKernelMemset(cookie, 0, sizeof(*cookie));

    if (name != NULL) {
        ret = http_cookie_dup_field(&cookie->name, name, &size_acc, g_cookie_max_single);
        if (ret < 0) goto fail;
    }
    if (value != NULL) {
        ret = http_cookie_dup_field(&cookie->value, value, &size_acc, g_cookie_max_single);
        if (ret < 0) goto fail;
    }
    if (comment != NULL) {
        ret = http_cookie_dup_field(&cookie->comment, comment, &size_acc, meta_limit);
        if (ret < 0) goto fail;
    }
    if (comment_url != NULL) {
        ret = http_cookie_dup_field(&cookie->comment_url, comment_url, &size_acc, meta_limit);
        if (ret < 0) goto fail;
    }
    if (domain != NULL) {
        ret = http_cookie_dup_field(&cookie->domain, domain, &size_acc, meta_limit);
        if (ret < 0) goto fail;
    }
    if (path != NULL) {
        u32 plen = sceNet_lib_D8722983(path) + 1;
        size_acc += plen;
        if (meta_limit < size_acc) {
            ret = SCE_HTTP_ERROR_OUT_OF_SIZE;
            goto fail;
        }
        cookie->path = (char *)http_malloc(plen);
        if (cookie->path == NULL) {
            ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
            goto fail;
        }
        ret = sceUriSweepPath(cookie->path, path, plen);
        if (ret < 0)
            goto fail;
    }
    if (ports != NULL) {
        size_acc += 2 * port_count;
        if (meta_limit < size_acc) {
            ret = SCE_HTTP_ERROR_OUT_OF_SIZE;
            goto fail;
        }
        cookie->ports = (u16 *)http_malloc(2 * port_count);
        if (cookie->ports == NULL) {
            ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
            goto fail;
        }
        sceKernelMemcpy(cookie->ports, ports, 2 * port_count);
        cookie->port_count = port_count;
    }

    cookie->expires = *expires;
    cookie->version = version;
    cookie->secure = secure;
    cookie->discard = discard;
    cookie->is_v2 = is_v2;
    cookie->size_bytes = size_acc;
    *out_cookie = cookie;
    return 0;

fail:
    http_cookie_free_fields(cookie);
    http_free(cookie);
    return ret;
}

static void http_trim_trailing_ws(char *str)
{
    u32 len;

    if (str == NULL)
        return;
    len = sceNet_lib_D8722983(str);
    while (len > 0 && (sceNet_lib_750F705D((s8)str[len - 1]) & 8) != 0)
        str[--len] = '\0';
}

s32 http_cookie_parse_kv(const u8 *src, u32 len, HttpHeaderNode **out_list,
                         u32 *out_secure, u32 *out_discard, s32 is_v2)
{
    HttpHeaderNode *head = NULL;
    u32 pos = 0;

    *out_secure = 0;
    *out_discard = 0;

    while (pos < len) {
        u32 name_len = 0;
        u32 val_len = 0;
        u32 cur;
        s32 no_equals = 0;
        HttpHeaderNode *node;

        if (http_cookie_match_keyword((const char *)(src + pos), len - pos, "secure")) {
            *out_secure = 1;
            while (pos < len && src[pos] != ';')
                pos++;
            if (pos < len)
                pos++;
            continue;
        }

        if (is_v2 == 2 && http_cookie_match_keyword((const char *)(src + pos), len - pos, "discard")) {
            *out_discard = 1;
            while (pos < len && src[pos] != ';')
                pos++;
            if (pos < len)
                pos++;
            continue;
        }

        while (pos < len && (sceNet_lib_750F705D((s8)src[pos]) & 8) != 0)
            pos++;

        cur = pos;
        while (cur < len && src[cur] != '=' && src[cur] != ';') {
            cur++;
            name_len++;
        }

        if (cur == len || src[cur] == ';') {
            no_equals = 1;
            if (head != NULL) {
                pos = (cur < len) ? cur + 1 : len;
                while (pos < len && (sceNet_lib_750F705D((s8)src[pos]) & 8) != 0)
                    pos++;
                continue;
            }
        }

        node = (HttpHeaderNode *)http_malloc(sizeof(*node));
        if (node == NULL) {
            if (head != NULL)
                http_kv_list_free(head);
            return SCE_HTTP_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(node, 0, sizeof(*node));
        node->next = head;

        if (name_len != 0) {
            node->name = (char *)http_malloc(name_len + 1);
            if (node->name == NULL) {
                http_kv_list_free(node);
                return SCE_HTTP_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(node->name, src + pos, name_len);
            node->name[name_len] = '\0';
        }

        if (!no_equals) {
            if (cur < len)
                cur++;
            while (cur < len && (sceNet_lib_750F705D((s8)src[cur]) & 8) != 0)
                cur++;
            while (cur + val_len < len && src[cur + val_len] != ';')
                val_len++;
        }

        pos = cur + val_len;
        if (val_len != 0 || (name_len != 0 && !no_equals)) {
            node->value = (char *)http_malloc(val_len + 1);
            if (node->value == NULL) {
                http_kv_list_free(node);
                return SCE_HTTP_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(node->value, src + cur, val_len);
            node->value[val_len] = '\0';
        }

        if (pos < len && src[pos] == ';')
            pos++;

        http_trim_trailing_ws(node->name);
        http_trim_trailing_ws(node->value);
        head = node;

        while (pos < len && (sceNet_lib_750F705D((s8)src[pos]) & 8) != 0)
            pos++;
    }

    *out_list = head;
    return 0;
}

const char *http_cookie_normalize_domain(const char *domain)
{
    const char *tld;
    s32 tld_len;
    u32 prev_len;
    s32 labels = http_domain_parse_tld(domain, &tld, &tld_len, &prev_len);
    u32 min_labels;

    if (labels < 0)
        return NULL;
    min_labels = http_domain_min_labels(tld, tld_len, prev_len);
    if ((u32)labels < min_labels)
        return NULL;
    return http_domain_skip_tail_labels(domain, (s32)min_labels);
}

s32 http_cookie_domain_hash(const char *domain)
{
    const char *norm = http_cookie_normalize_domain(domain);
    u32 len;
    u8 digest[16];

    if (norm == NULL)
        norm = domain;
    len = sceNet_lib_D8722983(norm);
    if (len != 0) {
        if (norm[len - 1] == '.')
            len--;
        if (len != 0 && norm[0] == '.')
            norm++;
    }
    http_md5_digest(norm, len, digest);
    return (s32)(digest[0] >> 4);
}

s32 http_cookie_match_keyword(const char *str, u32 rem, const char *kw)
{
    u32 kw_len = sceNet_lib_D8722983(kw);

    if (rem < kw_len)
        return 0;
    if (sceNet_lib_B9085A96(str, kw, kw_len) != 0)
        return 0;
    if (kw_len == rem)
        return 1;
    if (str[kw_len] == ';')
        return 1;
    return (sceNet_lib_750F705D((s8)str[kw_len]) & 8) != 0;
}

void http_cookie_evict_if_needed(u32 need_bytes, u32 need_count)
{
    if (g_cookie_max_bytes - g_cookie_cur_bytes < need_bytes ||
        g_cookie_max_total - g_cookie_cur_total < need_count) {
        u32 limit = need_count >= 11 ? need_count : 10;
        u32 evicted = 0;
        s32 intr = http_intr_suspend();
        HttpCookieDomain *dom = g_cookie_lru_tail;

        while (dom != NULL && evicted < limit) {
            HttpCookieDomain *prev = dom->lru_prev;
            if (http_cookie_domain_acquire(dom) >= 0) {
                dom->deleted = 1;
                g_cookie_cur_total -= dom->cookie_count;
                http_cookie_domain_release(dom);
            }
            dom = prev;
            evicted++;
        }
        http_intr_resume(intr);
    }
}

s32 http_cookie_collect_matching(HttpCookieSortChunk *head, HttpCookie *list,
                                 const char *path, s32 is_secure)
{
    HttpCookieSortChunk *chunk = head;

    while (chunk->next != NULL)
        chunk = chunk->next;

    while (list != NULL) {
        if (chunk->count >= 10) {
            HttpCookieSortChunk *next_chunk = (HttpCookieSortChunk *)http_malloc(sizeof(*next_chunk));
            if (next_chunk == NULL)
                return SCE_HTTP_ERROR_OUT_OF_MEMORY;
            sceKernelMemset(next_chunk, 0, sizeof(*next_chunk));
            chunk->next = next_chunk;
            chunk = next_chunk;
        }

        if (list->deleted != 1) {
            const char *req_path = (path != NULL && sceNet_lib_D8722983(path) != 0) ? path : "/";
            u32 cpath_len = sceNet_lib_D8722983(list->path);

            if (sceNet_lib_B9085A96(list->path, req_path, cpath_len) == 0 &&
                (list->secure != 1 || is_secure != 0)) {
                HttpCookieSortNode *slot = &chunk->nodes[chunk->count];
                slot->cookie = list;
                http_cookie_sort_insert(&head->nodes[0], slot);
                chunk->count++;
            }
        }
        list = list->next;
    }

    return 0;
}

s32 http_cookie_domain_match(const char *cookie_domain, const char *host)
{
    u32 dom_len = sceNet_lib_D8722983(cookie_domain);
    u32 host_len = sceNet_lib_D8722983(host);
    u32 ip_tmp;

    if (dom_len == 0 || host_len == 0)
        return -1;

    if (cookie_domain[dom_len - 1] == '.')
        dom_len--;
    if (host[host_len - 1] == '.')
        host_len--;

    if (http_inet_pton(2, host, &ip_tmp) != 1 && cookie_domain[0] == '.') {
        if (dom_len - 1 == host_len)
            return sceNet_lib_B9085A96(cookie_domain + 1, host, host_len);
        if (host_len < dom_len)
            return -1;
        return sceNet_lib_B9085A96(cookie_domain, host + (host_len - dom_len), dom_len);
    }

    if (dom_len != host_len)
        return -1;
    return sceNet_lib_B9085A96(cookie_domain, host, dom_len);
}
