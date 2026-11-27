#include "libhttp_in.h"

/* -------------------------------------------------------------------------
 * HTTPS globals
 * ------------------------------------------------------------------------- */
u32    g_https_options = HTTPS_DEFAULT_OPTIONS;
s32    g_https_inited;
s32    g_https_ca_count;
void **g_https_ca_list;
void  *g_https_client_cert;
u32    g_https_client_cert_len;
char  *g_https_client_key;

/* -------------------------------------------------------------------------
 * Cookie port / domain / sort helpers (linked into this TU)
 * ------------------------------------------------------------------------- */
s32 http_parse_port_list_alloc(const char *str, u16 match_port, u16 **out_ports)
{
    s32 count;
    u16 *buf;

    *out_ports = NULL;
    count = http_parse_port_list(str, match_port, NULL);
    if (count > 0) {
        buf = (u16 *)http_malloc((u32)(2 * count));
        *out_ports = buf;
        if (buf == NULL)
            return SCE_HTTP_ERROR_OUT_OF_MEMORY;
        count = http_parse_port_list(str, match_port, buf);
        if (count > 0)
            return count;
        http_free(buf);
        *out_ports = NULL;
    }
    return count;
}

char *http_extract_dir_path(const char *path)
{
    const char *src = path;
    const char *slash = sceNet_lib_F48963C6(path, '/');
    u32 len;
    char *out;

    if (slash != NULL) {
        len = (u32)(slash - path) + 1;
    } else {
        src = "/";
        len = 1;
    }

    out = (char *)http_malloc(len + 1);
    if (out != NULL) {
        sceKernelMemcpy(out, src, len);
        out[len] = '\0';
    }
    return out;
}

u32 http_domain_min_labels(const char *tld, s32 tld_len, u32 prev_label_len)
{
    if (prev_label_len >= 3)
        return 2;
    if (tld_len == 3) {
        if (sceNet_lib_B9085A96(tld, "com", 3) == 0 ||
            sceNet_lib_B9085A96(tld, "edu", 3) == 0 ||
            sceNet_lib_B9085A96(tld, "net", 3) == 0 ||
            sceNet_lib_B9085A96(tld, "org", 3) == 0 ||
            sceNet_lib_B9085A96(tld, "gov", 3) == 0 ||
            sceNet_lib_B9085A96(tld, "mil", 3) == 0 ||
            sceNet_lib_B9085A96(tld, "int", 3) == 0)
            return 2;
    }
    return 3;
}

const char *http_domain_skip_tail_labels(const char *domain, s32 count)
{
    const char *ptr;
    u32 len;

    if (domain == NULL || (len = sceNet_lib_D8722983(domain)) == 0 || count == 0)
        return NULL;

    ptr = domain + len - 1;
    while (ptr != domain && count != 0) {
        ptr--;
        if (*ptr == '.')
            count--;
    }

    if (count == 0)
        return ptr + 1;
    if (count == 1 && *ptr != '.')
        return ptr;
    return NULL;
}

s32 http_domain_parse_tld(const char *domain, const char **tld_out, s32 *tld_len_out, u32 *prev_len_out)
{
    u32 len;
    const char *ptr;
    s32 labels;

    if (domain == NULL || tld_out == NULL || tld_len_out == NULL || prev_len_out == NULL)
        return -1;
    len = sceNet_lib_D8722983(domain);
    if (len == 0)
        return -1;

    *tld_len_out = 0;
    *prev_len_out = 0;
    ptr = domain + len - 1;

    if (*ptr == '.' && ptr != domain)
        ptr--;

    while (*ptr != '.' && ptr != domain) {
        (*tld_len_out)++;
        ptr--;
    }

    if (*ptr == '.') {
        *tld_out = ptr + 1;
    } else {
        *tld_out = ptr;
        (*tld_len_out)++;
    }

    labels = 1;
    if (ptr != domain) {
        while (ptr > domain) {
            ptr--;
            if (*ptr == '.')
                break;
            (*prev_len_out)++;
        }
        if (*ptr == '.') {
            labels = 2;
            while (ptr != domain) {
                ptr--;
                if (*ptr == '.')
                    labels++;
            }
        }
    }

    return labels + (domain[0] != '.');
}

HTTP_ALWAYS_INLINE s32 http_cookie_sort_cmp(const HttpCookie *lhs, const HttpCookie *rhs)
{
    s32 diff = (s32)sceNet_lib_D8722983(lhs->path) - (s32)sceNet_lib_D8722983(rhs->path);
    if (diff == 0)
        diff = (s32)sceNet_lib_D8722983(lhs->domain) - (s32)sceNet_lib_D8722983(rhs->domain);
    return diff;
}

void http_cookie_sort_insert(HttpCookieSortNode *root, HttpCookieSortNode *node)
{
    HttpCookieSortNode *cur = root;
    s32 cmp;

    if (root == node)
        return;

    cmp = http_cookie_sort_cmp(cur->cookie, node->cookie);
    if (cmp < 0) {
        while (cur->next != NULL && http_cookie_sort_cmp(cur->next->cookie, node->cookie) < 0)
            cur = cur->next;
        node->next = cur->next;
        cur->next = node;
        node->prev = cur;
        if (node->next != NULL)
            node->next->prev = node;
    } else if (cmp == 0) {
        node->prev = cur;
        node->next = cur->next;
        cur->next = node;
        if (node->next != NULL)
            node->next->prev = node;
    } else {
        while (cur->prev != NULL && http_cookie_sort_cmp(cur->prev->cookie, node->cookie) > 0)
            cur = cur->prev;
        node->prev = cur->prev;
        cur->prev = node;
        node->next = cur;
        if (node->prev != NULL)
            node->prev->next = node;
    }
}

s32 http_parse_port_list(const char *str, u16 match_port, u16 *out_ports)
{
    const char *ptr = str;
    s32 count = 0;
    s32 matched = 0;

    if (*ptr == '"') {
        ptr++;
        if (*ptr == '\0')
            return 0;
    }

    while (*ptr != '\0') {
        const char *num_start;
        u32 digits = 0;
        char num_buf[16];
        s32 port_val;

        while ((sceNet_lib_750F705D((s8)*ptr) & 8) != 0)
            ptr++;

        num_start = ptr;
        if ((sceNet_lib_750F705D((s8)*ptr) & 4) != 0) {
            while ((sceNet_lib_750F705D((s8)*ptr) & 4) != 0) {
                digits++;
                ptr++;
            }
            if (digits >= 6)
                return 0;
            sceKernelMemcpy(num_buf, num_start, digits);
            num_buf[digits] = '\0';
            port_val = sceNet_lib_1FB2FDDD(num_buf);
            if (port_val > 0xFFFF)
                return 0;
            if (port_val == (s32)match_port)
                matched = 1;
            if (out_ports != NULL)
                out_ports[count] = (u16)port_val;
            count++;
        }

        while (*ptr != '\0' && *ptr != ',' && *ptr != '"')
            ptr++;
        if (*ptr == '"')
            break;
        if (*ptr == ',')
            ptr++;
    }

    return matched ? count : 0;
}

/* -------------------------------------------------------------------------
 * SSL certificate verification & handshake
 * ------------------------------------------------------------------------- */
typedef struct HttpAsn1Item {
    u32 len;
    u8 *data;
    u8  hdr_len;
    u8  pad[3];
} HttpAsn1Item;

s32 http_ssl_verify_callback(void *ssl, void **certs, s32 cert_count, HttpConnection *conn, u32 *err_out)
{
    u32 opts = conn->cfg.ssl_flags != 0 ? conn->cfg.ssl_flags : g_https_options;
    u32 cert_flags = 0;
    s32 host_ok = 0;
    s32 cb_ret = 0;
    void *loaded_ca = NULL;
    s32 chain_idx;
    s32 idx;

    s32 failed = 0;

    (void)ssl;

    if (cert_count == 0 && (opts & 1) != 0)
        return 0;

    do {
        if ((opts & 1) == 0) {
            host_ok = 1;
            *err_out = 0;
            break;
        }

        for (idx = 0; idx < cert_count; idx++) {
            u8 tag[4];
            char *field_ptr = NULL;
            s32 field_len = 0;
            u8 hash_buf[32];
            s32 hash_len = 0;
            void *issuer;

            if (sceSsl_lib_9299C6CE(certs[idx], tag, &field_ptr, &field_len) != 0) {
                if (field_ptr != NULL) {
                    sceSsl_lib_5E5C873A(field_ptr);
                    field_ptr = NULL;
                }
            }

            issuer = sceSsl_lib_4BE9030A(certs[idx]);
            if (issuer != NULL) {
                http_ssl_scan_x509_name(issuer);
            } else {
                *err_out = 2;
                cert_flags |= 2;
            }

            if ((opts & 0x10) != 0 &&
                sceSsl_lib_341843AB(certs[idx], tag, &field_ptr, &field_len) != 0) {
                char time_buf[256];
                void *now_t;
                void *not_before_t;

                sceKernelMemset(time_buf, 0, sizeof(time_buf));
                sceKernelMemcpy(time_buf, field_ptr, (u32)field_len);
                sceSsl_lib_5E5C873A(field_ptr);
                field_ptr = NULL;

                now_t = sceSsl_lib_0861D934(12);
                not_before_t = sceSsl_lib_0861D934(12);
                if (now_t == NULL || not_before_t == NULL) {
                    if (now_t != NULL) sceSsl_lib_5E5C873A(now_t);
                    if (not_before_t != NULL) sceSsl_lib_5E5C873A(not_before_t);
                    *err_out = 17;
                    conn->ssl_err = SCE_HTTPS_ERROR_INTERNAL;
                    failed = 1;
                    break;
                }
                sceSsl_lib_E7BFC0EF(time_buf, field_len, 1, not_before_t);
                sceSsl_lib_A7BED83B(now_t);
                if (sceSsl_lib_047AB6BB(now_t, not_before_t) == -1) {
                    cert_flags |= 0x10;
                    *err_out = 9;
                }
                sceSsl_lib_5E5C873A(now_t);
                sceSsl_lib_5E5C873A(not_before_t);
            }

            if ((opts & 0x08) != 0 &&
                sceSsl_lib_05E019A4(certs[idx], tag, &field_ptr, &field_len) != 0) {
                char time_buf[256];
                void *now_t;
                void *not_after_t;

                sceKernelMemset(time_buf, 0, sizeof(time_buf));
                sceKernelMemcpy(time_buf, field_ptr, (u32)field_len);
                sceSsl_lib_5E5C873A(field_ptr);
                field_ptr = NULL;

                now_t = sceSsl_lib_0861D934(12);
                not_after_t = sceSsl_lib_0861D934(12);
                if (now_t == NULL || not_after_t == NULL) {
                    if (now_t != NULL) sceSsl_lib_5E5C873A(now_t);
                    if (not_after_t != NULL) sceSsl_lib_5E5C873A(not_after_t);
                    *err_out = 17;
                    failed = 1;
                    break;
                }
                sceSsl_lib_E7BFC0EF(time_buf, field_len, 1, not_after_t);
                sceSsl_lib_A7BED83B(now_t);
                if (sceSsl_lib_047AB6BB(now_t, not_after_t) == 1) {
                    cert_flags |= 0x08;
                    *err_out = 10;
                }
                sceSsl_lib_5E5C873A(now_t);
                sceSsl_lib_5E5C873A(not_after_t);
            }

            sceSsl_lib_37C7B76C(certs[idx], hash_buf, &hash_len);
        }
        if (failed)
            break;

        if ((opts & 0x04) != 0) {
            u8 *san_data = NULL;
            s32 san_len = 0;

            cb_ret = sceSsl_lib_76A46EDA(certs[0], g_oid_subject_alt_name, 0, &san_data, &san_len);
            if (cb_ret == 1 && san_data != NULL) {
                HttpAsn1Item item;
                cb_ret = sceSsl_lib_C85F6FCE(&item, san_data, san_len);
                if (cb_ret == 0) {
                    s32 rem = san_len - item.hdr_len;
                    while (rem > 0) {
                        cb_ret = sceSsl_lib_C85F6FCE(&item, item.data, rem);
                        if (cb_ret != 0)
                            break;
                        cb_ret = http_ssl_match_hostname((const char *)item.data, item.len, conn->host);
                        if (cb_ret == 0) {
                            host_ok = 1;
                            break;
                        }
                        rem -= (s32)(item.hdr_len + item.len);
                        item.data += item.len;
                    }
                }
            }

            if (!host_ok) {
                void *subj = sceSsl_lib_12C7718F(certs[0]);
                if (subj != NULL) {
                    cb_ret = http_ssl_check_cn(subj, conn->host);
                    if (cb_ret == 0)
                        host_ok = 1;
                }
            }
        }

        {
            s32 chain_matched = 0;

            for (chain_idx = 0; chain_idx < cert_count; chain_idx++) {
                void *issuer = sceSsl_lib_4BE9030A(certs[chain_idx]);
                s32 found_anchor = 0;

                if ((opts & 0x180) == 0 && g_https_ca_count > 0) {
                    s32 ca_idx;
                    for (ca_idx = 0; ca_idx < g_https_ca_count; ca_idx++) {
                        void *ca_subj = sceSsl_lib_12C7718F(g_https_ca_list[ca_idx]);
                        if (issuer != NULL && ca_subj != NULL && sceSsl_lib_7D0E41F4(ca_subj, issuer) == 0) {
                            found_anchor = 1;
                            break;
                        }
                    }
                }

                if (!found_anchor && (opts & 0x40) != 0) {
                    HttpCertIter iter;
                    sceKernelMemset(&iter, 0, sizeof(iter));
                    for (;;) {
                        cb_ret = http_ssl_load_next_system_cert(&iter, &loaded_ca, opts);
                        if (cb_ret < 0) {
                            failed = 1;
                            break;
                        }
                        if (loaded_ca == NULL)
                            break;
                        {
                            void *ca_subj = sceSsl_lib_12C7718F(loaded_ca);
                            if (issuer != NULL && ca_subj != NULL && sceSsl_lib_7D0E41F4(ca_subj, issuer) == 0) {
                                void *pubkey = sceSsl_lib_B87FFE37(loaded_ca);
                                if (pubkey == NULL) {
                                    *err_out = 17;
                                    conn->ssl_err = SCE_HTTPS_ERROR_INTERNAL;
                                    failed = 1;
                                    break;
                                }
                                if (sceSsl_lib_0FF7BA68(certs[chain_idx], pubkey) != 0) {
                                    found_anchor = 1;
                                    break;
                                }
                            }
                        }
                        sceSsl_lib_C1A2E14B(loaded_ca);
                        loaded_ca = NULL;
                    }
                    if (failed)
                        break;
                }

                if (found_anchor) {
                    if (sceKernelGetCompiledSdkVersion() > 0x060600FF) {
                        s32 verify_idx = chain_idx;
                        while (verify_idx > 0) {
                            void *pubkey = sceSsl_lib_B87FFE37(certs[verify_idx]);
                            if (pubkey == NULL) {
                                *err_out = 17;
                                conn->ssl_err = SCE_HTTPS_ERROR_INTERNAL;
                                failed = 1;
                                break;
                            }
                            if (sceSsl_lib_0FF7BA68(certs[verify_idx - 1], pubkey) == 0) {
                                failed = 1;
                                break;
                            }
                            verify_idx--;
                        }
                        if (failed)
                            break;
                    }

                    {
                        s32 bc_idx = chain_idx;
                        while (bc_idx >= 0) {
                            u8 is_ca[4];
                            s32 path_len = 10;
                            s32 has_path_len = 0;

                            if (bc_idx > 0 && sceSsl_lib_CFA2F8EB(certs[bc_idx], is_ca, &path_len, &has_path_len) == 1) {
                                if (has_path_len != 0) {
                                    if ((u32)(path_len + 1) < (u32)bc_idx) {
                                        failed = 1;
                                        break;
                                    }
                                } else {
                                    cert_flags |= 2;
                                }
                            }
                            bc_idx--;
                        }
                        if (failed)
                            break;
                    }
                    chain_matched = 1;
                    break;
                }

                if (chain_idx + 1 < cert_count) {
                    void *next_subj = sceSsl_lib_12C7718F(certs[chain_idx + 1]);
                    if (issuer == NULL || next_subj == NULL || sceSsl_lib_7D0E41F4(issuer, next_subj) != 0) {
                        failed = 1;
                        break;
                    }
                }
            }

            if (!failed && !chain_matched && (opts & 0x20) != 0)
                cert_flags |= 0x20;
        }
    } while (0);

    if (!failed) {
        if ((opts & 0x04) != 0 && host_ok != 1)
            cert_flags |= 0x04;

        if (cert_flags != 0) {
            conn->ssl_cert_flags = cert_flags;
            conn->ssl_err = SCE_HTTPS_ERROR_CERT;
            cb_ret = -1;
        }

        if (conn->cfg.ssl_cb != NULL) {
            typedef s32 (*SslCb)(u32, void **, s32, void *);
            sceKernelCheckThreadStack();
            cb_ret = ((SslCb)conn->cfg.ssl_cb)(cert_flags, certs, cert_count, conn->cfg.ssl_cb_arg);
            if (cb_ret >= 0) {
                conn->ssl_err = 0;
                conn->ssl_cert_flags = 0;
            }
        }

        if (!(cb_ret < 0 && (g_https_ca_count != 0 || (opts & 0x40) != 0))) {
            conn->ssl_handshake_done = 1;
            if (loaded_ca != NULL)
                sceSsl_lib_C1A2E14B(loaded_ca);
            *err_out = 0;
            return 1;
        }
    }

    if (loaded_ca != NULL)
        sceSsl_lib_C1A2E14B(loaded_ca);
    conn->ssl_handshake_done = 1;
    return 0;
}

s32 http_ssl_client_cert_callback(void *ca_list, void **cert_out, void **key_out)
{
    const u8 *der_ptr = (const u8 *)g_https_client_cert;
    void *cert;
    void *bio = NULL;
    void *key = NULL;
    void *issuer;
    s32 ca_count;
    s32 idx;
    s32 matched = 0;
    s32 ok = 0;

    *cert_out = NULL;
    *key_out = NULL;

    ca_count = sceSsl_lib_92FA8B34(ca_list);
    if (g_https_client_cert == NULL)
        return 0;

    cert = sceSsl_lib_6038DEDE(NULL, &der_ptr, (s32)g_https_client_cert_len);
    if (cert == NULL)
        return 0;

    do {
        bio = sceSsl_lib_58352068();
        if (bio == NULL)
            break;

        sceSsl_lib_3454E622(bio, "%s", g_https_client_key);
        key = sceSsl_lib_0217E73C((void *)sceSsl_lib_A920B39E, "PRIVATE KEY", bio, NULL, NULL);
        if (key == NULL)
            break;

        issuer = sceSsl_lib_4BE9030A(cert);
        if (issuer == NULL)
            break;

        ok = 1;
        for (idx = 0; idx < ca_count; idx++) {
            void *ca_name = sceSsl_lib_7BDAF864(ca_list, idx);
            if (ca_name == NULL) {
                ok = 0;
                break;
            }
            if (sceSsl_lib_7D0E41F4(ca_name, issuer) == 0) {
                matched = 1;
                break;
            }
        }
    } while (0);

    if (ok) {
        if (matched) {
            *cert_out = cert;
            *key_out = key;
        }
        sceSsl_lib_3E040FD4(bio);
        return matched;
    }

    sceSsl_lib_C1A2E14B(cert);
    if (key != NULL)
        sceSsl_lib_7317C1BC(key);
    if (bio != NULL)
        sceSsl_lib_3E040FD4(bio);
    return 0;
}

s32 http_pem_to_der(const char *pem, u8 *der_out, u32 *der_len_out, u32 der_max)
{
    const char *dash;
    const char *body;
    const char *end_dash;
    u32 total = 0;

    dash = sceNet_lib_BCBE14CF(pem, '-');
    if (dash == NULL)
        return SCE_HTTP_ERROR_NOT_FOUND;
    body = sceNet_lib_BCBE14CF(dash, '\n');
    if (body == NULL)
        return SCE_HTTP_ERROR_NOT_FOUND;
    end_dash = sceNet_lib_BCBE14CF(body, '-');
    if (end_dash == NULL)
        return SCE_HTTP_ERROR_NOT_FOUND;

    body++;
    while (body < end_dash) {
        const char *lf = sceNet_lib_BCBE14CF(body, '\n');
        const char *cr;
        u32 line_len;
        s32 dec_len;

        if (lf == NULL)
            return SCE_HTTP_ERROR_NOT_FOUND;
        cr = sceNet_lib_BCBE14CF(body, '\r');
        line_len = (u32)(lf - body);
        if (cr != NULL && cr < lf)
            line_len--;

        if (der_out != NULL) {
            u32 est = 3 * (line_len >> 2);
            if (der_max < total + est)
                return SCE_HTTP_ERROR_OUT_OF_SIZE;
            dec_len = http_base64_decode(der_out + total, (const u8 *)body, (s32)line_len);
            if (dec_len < 0)
                return SCE_HTTP_ERROR_NOT_FOUND;
            total += (u32)dec_len;
        } else {
            total += 3 * (line_len >> 2);
        }
        body = lf + 1;
    }

    if (der_len_out != NULL)
        *der_len_out = (der_out == NULL) ? total + 1 : total;
    return 0;
}

/* -------------------------------------------------------------------------
 * Public HTTPS API
 * ------------------------------------------------------------------------- */
s32 sceHttpsInit(s32 ca_count, HttpCertSlot **ca_list, HttpCertSlot *client_cert, HttpCertSlot *client_key)
{
    char *pem_buf = NULL;
    u8 *der_buf = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 1968)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_https_inited != 0)
        return SCE_HTTP_ERROR_ALREADY_INITED;

    ret = sceSsl_lib_9266C0D5();
    if (ret == SCE_KERNEL_ERROR_NO_MOD)
        return ret;

    g_https_ca_count = 0;
    g_https_ca_list = NULL;

    ret = 0;
    do {
        if (ca_list != NULL && ca_count > 0) {
            s32 idx;
            g_https_ca_count = ca_count;
            g_https_ca_list = (void **)http_malloc((u32)(4 * ca_count));
            if (g_https_ca_list == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                break;
            }
            sceKernelMemset(g_https_ca_list, 0, (u32)(4 * ca_count));

            for (idx = 0; idx < ca_count; idx++) {
                u32 der_cap = 0;
                u32 der_len = 0;
                const u8 *der_ptr;
                void *cert;

                pem_buf = (char *)sceSsl_lib_0861D934(ca_list[idx]->size + 1);
                if (pem_buf == NULL) {
                    ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                    break;
                }
                sceKernelMemcpy(pem_buf, ca_list[idx]->data, ca_list[idx]->size);
                pem_buf[ca_list[idx]->size] = '\0';

                if (http_pem_to_der(pem_buf, NULL, &der_cap, 0) < 0) {
                    ret = SCE_HTTP_ERROR_SSL;
                    break;
                }
                der_buf = (u8 *)sceSsl_lib_0861D934(der_cap);
                if (http_pem_to_der(pem_buf, der_buf, &der_len, der_cap) < 0) {
                    ret = SCE_HTTP_ERROR_SSL;
                    break;
                }
                der_ptr = der_buf;
                cert = sceSsl_lib_6038DEDE(NULL, &der_ptr, (s32)der_len);
                if (cert == NULL) {
                    ret = SCE_HTTP_ERROR_SSL;
                    break;
                }
                sceSsl_lib_5E5C873A(pem_buf);
                sceSsl_lib_5E5C873A(der_buf);
                pem_buf = NULL;
                der_buf = NULL;
                g_https_ca_list[idx] = cert;
            }
            if (ret < 0)
                break;
        }

        if (client_cert != NULL) {
            u32 der_cap = 0;
            u32 der_len = 0;

            pem_buf = (char *)sceSsl_lib_0861D934(client_cert->size + 1);
            if (pem_buf == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                break;
            }
            sceKernelMemcpy(pem_buf, client_cert->data, client_cert->size);
            pem_buf[client_cert->size] = '\0';

            if (http_pem_to_der(pem_buf, NULL, &der_cap, 0) < 0) {
                ret = SCE_HTTP_ERROR_SSL;
                break;
            }
            der_buf = (u8 *)sceSsl_lib_0861D934(der_cap);
            if (http_pem_to_der(pem_buf, der_buf, &der_len, der_cap) < 0) {
                ret = SCE_HTTP_ERROR_SSL;
                break;
            }
            g_https_client_cert = der_buf;
            g_https_client_cert_len = der_len;
            der_buf = NULL;
            sceSsl_lib_5E5C873A(pem_buf);
            pem_buf = NULL;
        }

        if (client_key != NULL) {
            g_https_client_key = (char *)sceSsl_lib_0861D934(client_key->size + 1);
            if (g_https_client_key == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                break;
            }
            sceKernelMemcpy(g_https_client_key, client_key->data, client_key->size);
            g_https_client_key[client_key->size] = '\0';
        }

        g_https_inited = 1;
        return 0;
    } while (0);

    if (pem_buf != NULL)
        sceSsl_lib_5E5C873A(pem_buf);
    if (der_buf != NULL)
        sceSsl_lib_5E5C873A(der_buf);
    if (ret != SCE_HTTP_ERROR_ALREADY_INITED && ret != SCE_KERNEL_ERROR_NO_MOD)
        http_ssl_end_internal();
    return ret;
}

s32 sceHttpsInitWithPath(s32 ca_count, s32 ca_list, s32 client_cert)
{
    s32 ret;

    if (sceKernelCheckThreadStack() < 1968)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_https_inited != 0)
        return SCE_HTTP_ERROR_ALREADY_INITED;

    ret = sceSsl_lib_9266C0D5();
    if (ret == SCE_KERNEL_ERROR_NO_MOD)
        return ret;
    if ((ca_count | ca_list | client_cert) != 0)
        return SCE_HTTP_ERROR_NOT_SUPPORTED;

    g_https_inited = 1;
    g_https_ca_count = 0;
    g_https_ca_list = NULL;
    return 0;
}

s32 sceHttpsLoadDefaultCert(u32 group, u32 bit)
{
    void *work_buf = NULL;
    void **new_list = NULL;
    u8 *der_buf = NULL;
    HttpCertNode *cert_head = NULL;
    u32 work_size = 0;
    u32 out_size = 0;
    s32 ret;

    if (sceKernelCheckThreadStack() < 1984)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (http_is_inited() != 1) {
        http_ssl_end_internal();
        return SCE_HTTP_ERROR_BEFORE_INIT;
    }

    ret = sceCertLoader_DD629A24(group, bit, NULL, NULL, 0, &work_size);
    if (ret < 0) {
        http_ssl_end_internal();
        return ret;
    }

    work_buf = sceSsl_lib_0861D934(work_size);
    if (work_buf == NULL) {
        http_ssl_end_internal();
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    }

    ret = sceCertLoader_DD629A24(group, bit, &cert_head, work_buf, work_size, &out_size);
    if (ret >= 0 && cert_head != NULL && ret > 0) {
        s32 idx;
        s32 added = 0;
        HttpCertNode *cur = cert_head;

        new_list = (void **)http_malloc((u32)(4 * (g_https_ca_count + ret)));
        if (new_list == NULL) {
            ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
        } else {
            sceKernelMemset(new_list, 0, (u32)(4 * (g_https_ca_count + ret)));

            for (idx = 0; idx < g_https_ca_count; idx++)
                new_list[idx] = g_https_ca_list[idx];

            while (cur != NULL) {
                u32 der_cap = 0;
                u32 der_len = 0;
                const u8 *der_ptr;
                void *cert;

                if (http_pem_to_der(cur->pem, NULL, &der_cap, 0) < 0) {
                    ret = SCE_HTTP_ERROR_SSL;
                    break;
                }
                der_buf = (u8 *)http_malloc(der_cap);
                if (der_buf == NULL) {
                    ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                    break;
                }
                if (http_pem_to_der(cur->pem, der_buf, &der_len, der_cap) < 0) {
                    ret = SCE_HTTP_ERROR_SSL;
                    break;
                }
                der_ptr = der_buf;
                cert = sceSsl_lib_6038DEDE(NULL, &der_ptr, (s32)der_len);
                if (cert == NULL) {
                    ret = SCE_HTTP_ERROR_SSL;
                    break;
                }
                http_free(der_buf);
                der_buf = NULL;
                new_list[g_https_ca_count + added] = cert;
                added++;
                cur = cur->next;
            }

            if (ret >= 0) {
                if (g_https_ca_list != NULL)
                    http_free(g_https_ca_list);
                g_https_ca_list = new_list;
                g_https_ca_count += added;
                new_list = NULL;
            }
        }
    }

    if (ret >= 0) {
        sceSsl_lib_5E5C873A(work_buf);
        return sceKernelGetCompiledSdkVersion() > 0x02050010 ? ret : 0;
    }

    if (new_list != NULL)
        http_free(new_list);
    if (der_buf != NULL)
        http_free(der_buf);
    if (work_buf != NULL)
        http_free(work_buf);
    http_ssl_end_internal();
    return ret;
}

s32 http_ssl_end_internal(void)
{
    if (g_https_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;

    if (g_https_ca_list != NULL) {
        s32 idx;
        for (idx = 0; idx < g_https_ca_count; idx++) {
            if (g_https_ca_list[idx] != NULL)
                sceSsl_lib_C1A2E14B(g_https_ca_list[idx]);
        }
        http_free(g_https_ca_list);
        g_https_ca_list = NULL;
    }
    if (g_https_client_cert != NULL) {
        sceSsl_lib_5E5C873A(g_https_client_cert);
        g_https_client_cert = NULL;
        g_https_client_cert_len = 0;
    }
    if (g_https_client_key != NULL) {
        sceSsl_lib_5E5C873A(g_https_client_key);
        g_https_client_key = NULL;
    }
    g_https_inited = 0;
    return 0;
}

HTTP_ALWAYS_INLINE s32 http_ssl_modify_flags(s32 id, u32 flags, s32 enable)
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
            if (enable)
                tmpl->ssl_flags |= flags;
            else
                tmpl->ssl_flags &= ~flags;
            tmpl->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_CONNECTION) {
        HttpConnection *conn = NULL;
        http_lock_connection(id, &conn);
        if (conn != NULL) {
            if (enable)
                conn->cfg.ssl_flags |= flags;
            else
                conn->cfg.ssl_flags &= ~flags;
            conn->ref_count--;
            ret = 0;
        }
    }
    http_intr_resume(intr);
    return ret;
}

s32 sceHttp_87F1E666(s32 id, u32 flags)
{
    return http_ssl_modify_flags(id, flags, 1);
}

s32 sceHttp_3C478044(s32 id, u32 flags)
{
    return http_ssl_modify_flags(id, flags, 0);
}

s32 http_ssl_proxy_connect(HttpRequest *req, s32 sock_fd)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    char *hdr_buf = NULL;
    s32 saved_method = req->method;
    s32 ret;

    req->method = HTTP_METHOD_CONNECT;
    ret = http_build_request_header_buf(&hdr_buf, req);
    req->method = saved_method;

    if (ret >= 0) {
        u32 hdr_len = sceNet_lib_D8722983(hdr_buf);
        if (sceNetInetSend(sock_fd, hdr_buf, hdr_len, 0) < 0) {
            conn->net_errno = sceNetInetGetErrno();
            conn->net_psp_err = sceNetInetGetPspError();
            ret = SCE_HTTP_ERROR_NETWORK;
        } else {
            conn->state = HTTP_CONN_PROXY_TUNNEL;
            ret = http_recv_and_parse_headers(req, conn);
            if (ret < 0) {
                ret = SCE_HTTP_ERROR_BAD_RESPONSE;
            } else if (req->res_info.status_code == 200) {
                req->res_header_len = 0;
            } else {
                conn->ssl_err = SCE_HTTPS_ERROR_PROXY;
                ret = SCE_HTTP_ERROR_SSL;
                if (req->method != HTTP_METHOD_HEAD)
                    req->state = HTTP_REQ_PROXY_AUTH_FAIL;
            }
        }
    }

    if (hdr_buf != NULL)
        http_free(hdr_buf);
    return ret;
}

s32 http_ssl_handshake(HttpRequest *req, s32 sock_fd)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    u32 opts;
    void *bio;
    s32 ret;
    s32 ssl_err;

    if (g_https_inited == 0) {
        http_ssl_free_conn(conn);
        return SCE_HTTP_ERROR_SSL_NOT_INITED;
    }

    opts = conn->cfg.ssl_flags != 0 ? conn->cfg.ssl_flags : (g_https_options & 0xFF);
    if (conn->ssl_handle != NULL) {
        if (sceSsl_lib_A8F0AD39(conn->ssl_handle) == 1)
            return 0;
        sceSsl_lib_3E3133D6(conn->ssl_handle);
        sceSsl_lib_54A7D8F3(conn->ssl_handle);
        sceSsl_lib_69F1B678(conn->ssl_handle, 0);
        sceSsl_lib_B9C8CCE6(conn->ssl_handle, NULL, NULL);
        sceSsl_lib_84833472(conn->ssl_handle);
        conn->ssl_handle = NULL;
    }

    if (conn->ssl_ctx == NULL) {
        void *method;
        if (conn->cfg.ssl_version == 1 || conn->cfg.ssl_version == 2)
            method = sceSsl_lib_42FFA860();
        else
            method = sceSsl_lib_B40D11EA();

        conn->ssl_ctx = sceSsl_lib_FB8273FE(method);
        if (conn->ssl_ctx == NULL) {
            conn->ssl_err = SCE_HTTPS_ERROR_INTERNAL;
            http_ssl_free_conn(conn);
            return SCE_HTTP_ERROR_SSL;
        }

        ret = sceSsl_lib_B4D78E98(conn->ssl_ctx, 32, 0x2000FFFF, NULL);
        if (ret < 0) {
            if (((u32)ret >> 16 & 0xFFF) != 2)
                ret = SCE_HTTP_ERROR_SSL;
            http_ssl_free_conn(conn);
            return ret;
        }

        sceSsl_lib_AEBF278B(conn->ssl_ctx, 5, NULL);
        sceSsl_lib_529A9477(conn->ssl_ctx, (void *)http_ssl_verify_callback, conn);
        if ((opts & 2) != 0)
            sceSsl_lib_BF55C31C(conn->ssl_ctx, (void *)http_ssl_client_cert_callback);
        sceSsl_lib_D5ABC603(conn->ssl_ctx, 1);
    }

    conn->ssl_handle = sceSsl_lib_EBFB0E3C(conn->ssl_ctx);
    if (conn->ssl_handle == NULL || (bio = sceSsl_lib_28B4DE33(sock_fd, 0)) == NULL) {
        conn->ssl_err = SCE_HTTPS_ERROR_INTERNAL;
        http_ssl_free_conn(conn);
        return SCE_HTTP_ERROR_SSL;
    }

    sceSsl_lib_B9C8CCE6(conn->ssl_handle, bio, bio);
    sceSsl_lib_80608663(conn->ssl_handle);
    ret = sceSsl_lib_D53ACF30(conn->ssl_handle);
    ssl_err = sceSsl_lib_B3B04C58(conn->ssl_handle, ret);

    if (ssl_err == 1) {
        if (conn->ssl_err == 0)
            conn->ssl_err = SCE_HTTPS_ERROR_HANDSHAKE;
        ret = SCE_HTTP_ERROR_SSL;
    } else if (ssl_err == 5) {
        if (conn->ssl_err == 0)
            conn->ssl_err = SCE_HTTPS_ERROR_IO;
        ret = SCE_HTTP_ERROR_SSL;
    }

    if (ret < 0) {
        http_ssl_free_conn(conn);
        return ret;
    }
    return 0;
}

s32 http_ssl_recv(HttpConnection *conn, void *buf, s32 len)
{
    s32 ret;
    s32 ssl_err;

    if (len == 0)
        return sceSsl_lib_A8F0AD39(conn->ssl_handle) == 1 ? 0 : SCE_HTTP_ERROR_SSL;

    ret = sceSsl_lib_E7C29542(conn->ssl_handle, buf, len);
    ssl_err = sceSsl_lib_B3B04C58(conn->ssl_handle, ret);
    switch (ssl_err) {
    case 1:
        if (conn->ssl_err == 0)
            conn->ssl_err = SCE_HTTPS_ERROR_HANDSHAKE;
        return SCE_HTTP_ERROR_SSL;
    case 5:
        if (conn->ssl_err == 0)
            conn->ssl_err = SCE_HTTPS_ERROR_IO;
        /* fall through */
    case 6:
        conn->peer_closed = 1;
        break;
    default:
        break;
    }
    return ret;
}

s32 http_ssl_send(HttpConnection *conn, const void *buf, s32 len)
{
    s32 ret = sceSsl_lib_B7CA8717(conn->ssl_handle, buf, len);
    s32 ssl_err = sceSsl_lib_B3B04C58(conn->ssl_handle, ret);

    if (ssl_err == 1) {
        if (conn->ssl_err == 0)
            conn->ssl_err = SCE_HTTPS_ERROR_HANDSHAKE;
        return SCE_HTTP_ERROR_SSL;
    }
    if (ssl_err == 5) {
        if (conn->ssl_err == 0)
            conn->ssl_err = SCE_HTTPS_ERROR_IO;
        return SCE_HTTP_ERROR_SSL;
    }
    return ret;
}

s32 sceHttpsGetSslError(s32 req_id, s32 *ssl_err_out, u32 *cert_flags_out)
{
    HttpRequest *req = NULL;
    s32 ret;

    if (sceKernelCheckThreadStack() < 2016)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_https_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    if (ssl_err_out == NULL || cert_flags_out == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    ret = http_lock_request(req_id, &req);
    if (ret >= 0) {
        *ssl_err_out = req->ssl_err;
        *cert_flags_out = req->ssl_cert_flags;
    }

    if (req != NULL) {
        s32 intr = http_intr_suspend();
        req->ref_count--;
        http_intr_resume(intr);
    }
    return ret;
}

s32 sceHttpsSetSslCallback(s32 id, void *cb, void *arg)
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
            tmpl->ssl_cb = cb;
            tmpl->ssl_cb_arg = arg;
            tmpl->ref_count--;
            ret = 0;
        }
    } else if (type == HTTP_HANDLE_CONNECTION) {
        HttpConnection *conn = NULL;
        http_lock_connection(id, &conn);
        if (conn != NULL) {
            conn->cfg.ssl_cb = cb;
            conn->cfg.ssl_cb_arg = arg;
            conn->ref_count--;
            ret = 0;
        }
    }
    http_intr_resume(intr);
    return ret;
}

s32 sceHttpsGetCaList(void ***list_out, s32 *count_out)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    if (g_https_inited != 1)
        return SCE_HTTP_ERROR_BEFORE_INIT;
    if (list_out == NULL || count_out == NULL)
        return SCE_HTTP_ERROR_INVALID_VALUE;

    *list_out = g_https_ca_list;
    *count_out = g_https_ca_count;
    return 0;
}

s32 http_ssl_is_inited(void)
{
    return g_https_inited;
}

s32 sceHttpsEnd(void)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    return http_ssl_end_internal();
}

s32 sceHttpsEnableOption(u32 flags)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    g_https_options |= flags;
    return 0;
}

s32 sceHttpsDisableOption(u32 flags)
{
    if (sceKernelCheckThreadStack() < 2032)
        return SCE_HTTP_ERROR_OUT_OF_STACK;
    g_https_options &= ~flags;
    return 0;
}

void http_ssl_close_conn(HttpConnection *conn)
{
    http_ssl_free_conn(conn);
}

void http_ssl_scan_x509_name(void *x509_name)
{
    s32 count = sceSsl_lib_4C75B8E9(x509_name);
    s32 idx;

    for (idx = 0; idx < count; idx++) {
        void *entry = sceSsl_lib_5040E836(x509_name, idx);
        void *oid = NULL;
        s32 oid_len = 0;
        u8 tag[4];
        char *val = NULL;
        u32 val_len = 0;

        if (sceSsl_lib_8D484361(entry, 0, &oid, &oid_len, tag, &val, &val_len) != 0)
            sceSsl_lib_2B7BD166(oid, oid_len, 0);
    }
}

s32 http_ssl_match_hostname(const char *pattern, u32 pat_len, const char *host)
{
    u32 host_len = sceNet_lib_D8722983(host);
    u32 pat_pos = 0;

    if (pat_len > 0 && pattern[pat_len - 1] == '.')
        pat_len--;
    if (host_len > 0 && host[host_len - 1] == '.')
        host_len--;

    for (;;) {
        const char *star = http_strnchr(pattern + pat_pos, '*', pat_len - pat_pos);
        u32 lit_len;
        const char *next_star;
        u32 seg_len;

        if (star == NULL)
            break;

        lit_len = (u32)(star - (pattern + pat_pos));
        if (lit_len != 0) {
            s32 cmp = sceNet_lib_B9085A96(pattern + pat_pos, host, lit_len);
            if (cmp != 0)
                return cmp;
            host += lit_len;
            host_len -= lit_len;
            pat_pos += lit_len;
        }

        pat_pos++;
        next_star = http_strnchr(pattern + pat_pos, '*', pat_len - pat_pos);
        if (next_star == NULL) {
            u32 tail_len = pat_len - pat_pos;
            const char *dot;
            u32 skip;

            if (host_len < tail_len)
                return -1;
            skip = host_len - tail_len;
            dot = http_strnchr(host, '.', host_len);
            if (dot != NULL && (u32)(dot - host) < skip)
                return -1;
            host += skip;
            host_len -= skip;
        } else {
            seg_len = (u32)(next_star - (pattern + pat_pos));
            if (seg_len != 0) {
                s32 found = -1;
                if (host_len < seg_len)
                    return -1;
                while (host_len >= seg_len) {
                    found = sceNet_lib_B9085A96(pattern + pat_pos, host, seg_len);
                    if (found == 0) {
                        host += seg_len;
                        host_len -= seg_len;
                        pat_pos += seg_len;
                        break;
                    }
                    host++;
                    host_len--;
                }
                if (found != 0)
                    return found;
            }
        }

        if (pat_pos > pat_len)
            return -1;
    }

    if (pat_len - pat_pos == host_len)
        return sceNet_lib_B9085A96(pattern + pat_pos, host, host_len);
    return -1;
}

const char *http_strnchr(const char *str, s32 ch, u32 max_len)
{
    u32 idx;

    if (str == NULL)
        return NULL;
    for (idx = 0; idx < max_len; idx++) {
        if (str[idx] == (char)ch)
            return &str[idx];
        if (str[idx] == '\0')
            return NULL;
    }
    return NULL;
}

void http_ssl_free_conn(HttpConnection *conn)
{
    if (conn->ssl_handle != NULL) {
        if (conn->ssl_handshake_done == 1)
            sceSsl_lib_3E3133D6(conn->ssl_handle);
        sceSsl_lib_54A7D8F3(conn->ssl_handle);
        sceSsl_lib_69F1B678(conn->ssl_handle, 0);
        sceSsl_lib_B9C8CCE6(conn->ssl_handle, NULL, NULL);
        sceSsl_lib_84833472(conn->ssl_handle);
        conn->ssl_handle = NULL;
        conn->ssl_handshake_done = 0;
    }
    if (conn->ssl_ctx != NULL) {
        sceSsl_lib_588F2FE8(conn->ssl_ctx);
        conn->ssl_ctx = NULL;
    }
}

s32 http_ssl_check_cn(void *x509_name, const char *host)
{
    s32 count = sceSsl_lib_4C75B8E9(x509_name);
    s32 matched = 0;
    s32 idx;

    for (idx = 0; idx < count; idx++) {
        void *entry = sceSsl_lib_5040E836(x509_name, idx);
        void *oid = NULL;
        s32 oid_len = 0;
        u8 tag[4];
        char *val = NULL;
        u32 val_len = 0;

        if (sceSsl_lib_8D484361(entry, 0, &oid, &oid_len, tag, &val, &val_len) != 0) {
            const char *name = sceSsl_lib_2B7BD166(oid, oid_len, 0);
            if (name == NULL)
                name = "<UNKNOWN>";
            if (sceNet_lib_A0F16ABD(name, "commonName") == 0 &&
                http_ssl_match_hostname(val, val_len, host) == 0) {
                matched = 1;
            }
        }
    }

    return matched ? 0 : -1;
}

s32 http_ssl_load_next_system_cert(HttpCertIter *iter, void **cert_out, u32 flags)
{
    s32 only_group3 = (flags & 0x100) != 0;
    u32 work_size = 0;
    u32 out_size = 0;
    void *work_buf = NULL;
    u8 *der_buf = NULL;
    HttpCertNode *cert_node = NULL;
    s32 ret;

    for (;;) {
        if (iter->group == 0 || iter->advance_group == 1) {
            iter->group = only_group3 ? 3 : iter->group + 1;
            iter->bit = 1;
        } else {
            iter->bit *= 2;
        }

        ret = sceCertLoader_DD629A24(iter->group, iter->bit, NULL, NULL, 0, &work_size);
        if (ret == SCE_CERTLOADER_ERROR_END) {
            if (iter->advance_group == 1 || only_group3) {
                *cert_out = NULL;
                return 0;
            }
            iter->advance_group = 1;
            continue;
        }
        if (ret < 0)
            return ret;
        iter->advance_group = 0;
        break;
    }

    work_buf = sceSsl_lib_0861D934(work_size + 8000);
    if (work_buf == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;

    ret = sceCertLoader_DD629A24(iter->group, iter->bit, &cert_node, work_buf, work_size, &out_size);
    if (ret >= 0) {
        if (ret == 1 && http_pem_to_der(cert_node->pem, NULL, &work_size, 0) >= 0) {
            der_buf = (u8 *)sceSsl_lib_0861D934(work_size);
            if (der_buf == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
            } else if (http_pem_to_der(cert_node->pem, der_buf, &out_size, work_size) >= 0) {
                const u8 *der_ptr = der_buf;
                void *cert;
                sceSsl_lib_5E5C873A(work_buf);
                work_buf = NULL;
                cert = sceSsl_lib_6038DEDE(NULL, &der_ptr, (s32)out_size);
                *cert_out = cert;
                ret = (cert != NULL) ? 0 : SCE_HTTP_ERROR_SSL;
            } else {
                ret = SCE_HTTP_ERROR_SSL;
            }
        } else {
            ret = SCE_HTTP_ERROR_SSL;
        }
    }

    if (work_buf != NULL)
        sceSsl_lib_5E5C873A(work_buf);
    if (der_buf != NULL)
        sceSsl_lib_5E5C873A(der_buf);
    return ret;
}

/* -------------------------------------------------------------------------
 * 401/407 Auth & 3xx Redirect response handling
 * ------------------------------------------------------------------------- */
s32 http_handle_auth_response(HttpRequest *req)
{
    HttpAuthChallenge chal;
    HttpDigestParams dparams;
    HttpAuthEntry *entry = NULL;
    char *realm_buf = NULL;
    char *user_ptr;
    char *pass_ptr;
    const void *entity = NULL;
    u32 entity_len = 0;
    s32 save_flag = 0;
    s32 ret = 0;

    if (req->state == HTTP_REQ_SENT)
        return 0;

    if (!((req->cfg.auth_enabled == 1 && req->res_info.status_code == 401) ||
          (req->cfg.proxy_auth_enabled == 1 && req->res_info.status_code == 407)))
        return 0;

    if (req->sending_body == 1 || req->cfg.max_auth_retries < req->auth_retry_count)
        return 0;

    ret = http_parse_auth_header(&chal, req->res_header_buf, req->res_header_len, req->res_info.status_code);
    if (ret == SCE_HTTP_ERROR_OUT_OF_MEMORY)
        return ret;
    if (ret < 0)
        return 0;

    if ((req->cfg.auth_user == NULL || sceNet_lib_D8722983(req->cfg.auth_user) == 0) &&
        (req->cfg.auth_pass == NULL || sceNet_lib_D8722983(req->cfg.auth_pass) == 0) &&
        req->cfg.auth_cb_legacy == NULL && req->cfg.auth_cb == NULL)
        return 0;

    do {
        if (req->cfg.auth_user != NULL && sceNet_lib_D8722983(req->cfg.auth_user) != 0 &&
            req->cfg.auth_pass != NULL && sceNet_lib_D8722983(req->cfg.auth_pass) != 0) {
            user_ptr = req->cfg.auth_user;
            pass_ptr = req->cfg.auth_pass;
        } else {
            realm_buf = (char *)http_malloc(chal.realm_len + 1);
            if (realm_buf == NULL)
                return SCE_HTTP_ERROR_OUT_OF_MEMORY;
            sceKernelMemcpy(realm_buf, chal.realm, chal.realm_len);
            realm_buf[chal.realm_len] = '\0';

            user_ptr = req->auth_user_buf;
            pass_ptr = req->auth_pass_buf;

            entry = http_auth_cache_find(req, (u32)chal.type, realm_buf);
            if (entry != NULL) {
                sceKernelMemcpy(user_ptr, entry->username, 256);
                sceKernelMemcpy(pass_ptr, entry->password, 256);
                save_flag = entry->save_flag;
            } else if (chal.type == HTTP_AUTH_PROXY_BASIC || chal.type == HTTP_AUTH_PROXY_DIGEST) {
                sceKernelMemset(user_ptr, 0, 256);
                sceKernelMemset(pass_ptr, 0, 256);
            }

            if (entry != NULL && chal.stale == 1)
                entry->status = 2;

            if (entry == NULL || entry->status != 2) {
                s32 cb_ret;
                if (req->cfg.auth_cb != NULL) {
                    typedef s32 (*AuthCb)(s32, s32, const char *, char *, char *, s32,
                                          const void **, u32 *, s32 *, void *);
                    sceKernelCheckThreadStack();
                    cb_ret = ((AuthCb)req->cfg.auth_cb)(req->id, chal.type, realm_buf,
                                                        user_ptr, pass_ptr, 0,
                                                        &entity, &entity_len, &save_flag,
                                                        req->cfg.auth_cb_arg);
                } else {
                    typedef s32 (*LegacyAuthCb)(s32, s32, const char *, char *, char *, s32,
                                                const void **, u32 *, s32 *);
                    sceKernelCheckThreadStack();
                    cb_ret = ((LegacyAuthCb)req->cfg.auth_cb_legacy)(req->id, chal.type, realm_buf,
                                                                     user_ptr, pass_ptr, 0,
                                                                     &entity, &entity_len, &save_flag);
                }
                if (cb_ret < 0) {
                    if (entry != NULL)
                        http_auth_entry_release_locked(entry);
                    ret = 0;
                    break;
                }
            }

            if (entry == NULL)
                entry = http_auth_cache_alloc(req, chal.type, realm_buf);
            if (entry != NULL) {
                sceKernelMemcpy(entry->username, user_ptr, 256);
                sceKernelMemcpy(entry->password, pass_ptr, 256);
                entry->save_flag = save_flag;
                if (chal.type >= HTTP_AUTH_PROXY_BASIC)
                    req->proxy_auth_entry = entry;
                else
                    req->auth_entry = entry;
            }
        }

        ret = http_drain_and_reset_request(req);
        if (ret < 0)
            break;

        req->auth_retry_count++;
        if (chal.type == HTTP_AUTH_BASIC || chal.type == HTTP_AUTH_PROXY_BASIC) {
            s32 is_proxy = (chal.type == HTTP_AUTH_PROXY_BASIC);
            ret = http_attach_basic_auth_header(req, user_ptr, pass_ptr, is_proxy);
            if (is_proxy) {
                sceKernelMemset(user_ptr, 0, 256);
                sceKernelMemset(pass_ptr, 0, 256);
            }
            if (ret >= 0) {
                ret = 1;
                break;
            }
        } else if (chal.type == HTTP_AUTH_DIGEST || chal.type == HTTP_AUTH_PROXY_DIGEST) {
            ret = http_init_digest_params(&dparams, &chal);
            if (ret >= 0) {
                s32 is_proxy = (chal.type == HTTP_AUTH_PROXY_DIGEST);
                ret = http_attach_digest_auth_header(req, user_ptr, pass_ptr,
                                                     entity, entity_len, &dparams, is_proxy);
                if (is_proxy) {
                    sceKernelMemset(user_ptr, 0, 256);
                    sceKernelMemset(pass_ptr, 0, 256);
                }
                if (ret >= 0) {
                    if (entry != NULL) {
                        http_digest_params_free(&entry->digest);
                        sceKernelMemcpy(&entry->digest, &dparams, sizeof(dparams));
                    } else {
                        http_digest_params_free(&dparams);
                    }
                    ret = 1;
                    break;
                }
            }
        } else {
            ret = 1;
            break;
        }

        if (ret != SCE_HTTP_ERROR_OUT_OF_MEMORY)
            ret = 0;
    } while (0);

    if (realm_buf != NULL)
        http_free(realm_buf);
    return ret;
}

s32 http_handle_redirect_and_auth(HttpRequest *req)
{
    s32 ret = http_handle_redirect(req);
    if (ret >= 0)
        return http_retry_with_auth(req);
    return ret;
}

s32 http_handle_redirect(HttpRequest *req)
{
    HttpConnection *conn;
    SceUriElement uri;
    char *location_str = NULL;
    void *uri_work = NULL;
    char *new_path = NULL;
    char *new_query = NULL;
    char *new_frag = NULL;
    char *proxy_host = NULL;
    char *base_path = NULL;
    char *rel_path = NULL;
    char *host_hdr = NULL;
    char *old_url = NULL;
    char *new_url = NULL;
    char *tmp_url = NULL;
    u32 work_size = 0;
    s32 status;
    s32 ret;

    if (req->state == HTTP_REQ_SENT || req->cfg.auto_redirect != 1)
        return 0;

    status = req->res_info.status_code;
    if (!((status >= 300 && status <= 303) || status == 305 || status == 307))
        return 0;
    if (req->sending_body == 1 || req->cfg.max_redirects < req->redirect_count)
        return 0;

    if (status == 303 && req->method != HTTP_METHOD_HEAD)
        req->method = HTTP_METHOD_GET;

    conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    if (req->res_info.location_ptr == NULL)
        return SCE_HTTP_ERROR_BAD_RESPONSE;

    location_str = (char *)http_malloc(req->res_info.location_len + 1);
    if (location_str == NULL)
        return SCE_HTTP_ERROR_OUT_OF_MEMORY;
    sceKernelMemcpy(location_str, req->res_info.location_ptr, req->res_info.location_len);
    location_str[req->res_info.location_len] = '\0';

    do {
        if (req->cfg.redirect_cb != NULL) {
            typedef s32 (*RedirCb)(s32, s32, s32 *, const char *, void *);
            sceKernelCheckThreadStack();
            if (((RedirCb)req->cfg.redirect_cb)(req->id, status, &req->method,
                                                location_str, req->cfg.redirect_cb_arg) < 0) {
                ret = 0;
                break;
            }
        }

        ret = sceUriParse(NULL, location_str, NULL, &work_size, 0);
        if (ret < 0)
            break;
        uri_work = http_malloc(work_size);
        if (uri_work == NULL) {
            ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
            break;
        }
        ret = sceUriParse(&uri, location_str, uri_work, NULL, work_size);
        if (ret < 0)
            break;

        if (uri.query != NULL && sceNet_lib_D8722983(uri.query) != 0) {
            work_size = sceNet_lib_D8722983(uri.query) + 1;
            new_query = (char *)http_malloc(work_size);
            if (new_query == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                break;
            }
            sceKernelMemcpy(new_query, uri.query, work_size);
        }
        if (uri.fragment != NULL && sceNet_lib_D8722983(uri.fragment) != 0) {
            work_size = sceNet_lib_D8722983(uri.fragment) + 1;
            new_frag = (char *)http_malloc(work_size);
            if (new_frag == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                break;
            }
            sceKernelMemcpy(new_frag, uri.fragment, work_size);
        }

        ret = http_drain_and_reset_request(req);
        if (ret < 0)
            break;

        req->redirect_count++;
        if (req->res_info.status_code == 301) {
            old_url = http_build_request_url(req);
            if (old_url == NULL) {
                ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
                break;
            }
        }

        if (req->res_info.status_code != 305) {
            if (uri.opaque != 1 && req->path != NULL) {
                http_free(req->path);
                req->path = NULL;
            }
            if (req->query != NULL) {
                http_free(req->query);
                req->query = NULL;
            }
            if (req->fragment != NULL) {
                http_free(req->fragment);
                req->fragment = NULL;
            }
        }

        if (uri.opaque == 1) {
            ret = sceUriBuild(NULL, &work_size, 0, &uri, 10);
            if (ret < 0) break;
            rel_path = (char *)http_malloc(work_size);
            if (rel_path == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            ret = sceUriBuild(rel_path, NULL, work_size, &uri, 10);
            if (ret < 0) break;

            http_free(uri_work);
            uri_work = NULL;
            sceKernelMemset(&uri, 0, sizeof(uri));

            ret = sceUriParse(NULL, req->path, NULL, &work_size, 0);
            if (ret < 0) break;
            uri_work = http_malloc(work_size);
            if (uri_work == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            ret = sceUriParse(&uri, req->path, uri_work, NULL, work_size);
            if (ret < 0) break;

            if (req->path != NULL) {
                http_free(req->path);
                req->path = NULL;
            }

            ret = sceUriBuild(NULL, &work_size, 0, &uri, 10);
            if (ret < 0) break;
            base_path = (char *)http_malloc(work_size);
            if (base_path == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            ret = sceUriBuild(base_path, NULL, work_size, &uri, 10);
            if (ret < 0) break;

            new_path = http_resolve_relative_path(base_path, rel_path);
            if (new_path == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }

            http_free(base_path); base_path = NULL;
            http_free(rel_path);  rel_path = NULL;

            req->path = new_path;     new_path = NULL;
            req->query = new_query;   new_query = NULL;
            req->fragment = new_frag; new_frag = NULL;

            if (req->next_pipelined_id != 0) {
                const char *scheme_str = (conn->scheme == HTTP_SCHEME_HTTPS) ? "https" : "http";
                ret = http_create_connection_internal(0, conn->host, scheme_str, NULL, NULL,
                                                      conn->port, conn->cfg.keep_alive, 2, req);
                if (ret < 0)
                    break;
            }
        } else if (sceNet_lib_A0F16ABD(uri.host, conn->host) == 0 &&
                   req->next_pipelined_id == 0 &&
                   http_parse_scheme(uri.scheme) == conn->scheme &&
                   conn->port == uri.port) {
            work_size = sceNet_lib_D8722983(uri.path) + 1;
            new_path = (char *)http_malloc(work_size);
            if (new_path == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            sceNet_lib_80C9F02A(new_path, uri.path);
            http_free(uri_work); uri_work = NULL;

            req->path = new_path;     new_path = NULL;
            req->query = new_query;   new_query = NULL;
            req->fragment = new_frag; new_frag = NULL;
        } else {
            s32 cur_status = req->res_info.status_code;
            if (req->method == HTTP_METHOD_POST && cur_status != 305 && cur_status != 307 &&
                conn->scheme == HTTP_SCHEME_HTTPS && http_parse_scheme(uri.scheme) == HTTP_SCHEME_HTTP) {
                req->method = HTTP_METHOD_GET;
            }

            if (cur_status == 305) {
                proxy_host = (char *)http_malloc(sceNet_lib_D8722983(uri.host) + 1);
                if (proxy_host == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
                sceNet_lib_80C9F02A(proxy_host, uri.host);

                tmp_url = http_build_request_url(req);
                if (tmp_url == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }

                ret = http_create_connection_with_url_internal(0, tmp_url, req->cfg.keep_alive, 1, req);
                if (ret < 0)
                    break;
                http_free(tmp_url);
                tmp_url = NULL;

                req->redir_conn->cfg.use_proxy = 1;
                req->redir_conn->cfg.proxy_port = uri.port;
                req->redir_conn->cfg.use_default_proxy = 0;
                req->redir_conn->cfg.proxy_server = proxy_host;
                proxy_host = NULL;
            } else {
                s32 new_scheme;
                work_size = sceNet_lib_D8722983(uri.path) + 1;
                new_path = (char *)http_malloc(work_size);
                if (new_path == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
                sceNet_lib_80C9F02A(new_path, uri.path);

                req->path = new_path;     new_path = NULL;
                req->query = new_query;   new_query = NULL;
                req->fragment = new_frag; new_frag = NULL;

                new_scheme = http_parse_scheme(uri.scheme);
                ret = http_format_host_header(&host_hdr, new_scheme, uri.host, uri.port);
                if (ret < 0)
                    break;
                http_add_header_internal(req->id, "Host", host_hdr, 0);
                http_free(host_hdr);
                host_hdr = NULL;

                ret = http_create_connection_with_url_internal(0, location_str, req->cfg.keep_alive, 1, req);
                if (ret < 0)
                    break;
            }
        }

        if (req->res_info.status_code == 301) {
            new_url = http_build_request_url(req);
            if (new_url == NULL) { ret = SCE_HTTP_ERROR_OUT_OF_MEMORY; break; }
            http_redir_cache_add(old_url, req->method, NULL, new_url);
        }

        if (uri_work != NULL)     { http_free(uri_work);     uri_work = NULL; }
        if (location_str != NULL) { http_free(location_str); location_str = NULL; }
        if (old_url != NULL)      { http_free(old_url);      old_url = NULL; }
        if (new_url != NULL)      { http_free(new_url);      new_url = NULL; }

        ret = http_send_request_core(req);
        if (ret >= 0)
            return ret;
    } while (0);

    if (location_str != NULL) http_free(location_str);
    if (uri_work != NULL)   http_free(uri_work);
    if (new_path != NULL)   http_free(new_path);
    if (new_query != NULL)  http_free(new_query);
    if (new_frag != NULL)   http_free(new_frag);
    if (proxy_host != NULL) http_free(proxy_host);
    if (base_path != NULL)  http_free(base_path);
    if (rel_path != NULL)   http_free(rel_path);
    if (host_hdr != NULL)   http_free(host_hdr);
    if (old_url != NULL)    http_free(old_url);
    if (new_url != NULL)    http_free(new_url);
    if (tmp_url != NULL)    http_free(tmp_url);
    return ret;
}

s32 http_retry_with_auth(HttpRequest *req)
{
    HttpConnection *conn = req->redir_conn != NULL ? req->redir_conn : req->conn;
    s32 ret = http_handle_auth_response(req);

    if (ret > 0) {
        if (req->next_pipelined_id != 0) {
            const char *scheme_str = (conn->scheme == HTTP_SCHEME_HTTPS) ? "https" : "http";
            ret = http_create_connection_internal(0, conn->host, scheme_str, NULL, NULL,
                                                  conn->port, conn->cfg.keep_alive, 2, req);
            if (ret < 0)
                return ret;
        }
        return http_send_request_core(req);
    }
    return ret;
}

s32 http_drain_and_reset_request(HttpRequest *req)
{
    s32 ret = http_drain_response_body(req);

    if (ret >= 0) {
        req->req_sent_bytes = 0;
        req->res_read_bytes = 0;
        req->state = HTTP_REQ_CREATED;
        req->chunk_remaining = 0;
        http_header_list_remove(&req->cfg.headers, "Proxy-Authorization");
        http_header_list_remove(&req->cfg.headers, "Authorization");
        http_header_list_remove(&req->cfg.headers, "Cookie");
        if (req->cache_read_entry != NULL) {
            http_header_list_remove(&req->cfg.headers, "If-Modified-Since");
            http_cache_entry_invalidate_locked(req->cache_read_entry);
            req->cache_read_entry = NULL;
        }
    }
    return ret;
}
