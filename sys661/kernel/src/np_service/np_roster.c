#include "np_service_internal.h"

/* =========================================================================
 * Roster Request Context & Global State
 * ========================================================================= */

typedef struct NpRosterReq {
    struct NpRosterReq *next; /* 0x000 */
    int id;                   /* 0x004 */
    int conn_id;              /* 0x008 */
    int req_id;               /* 0x00C */
    SceUID mutex;             /* 0x010 */
    int aborted;              /* 0x014 */
    u8 buf[3072];             /* 0x018 .. 0xC17 */
} NpRosterReq; /* 3096 bytes */

static int g_roster_init = 0;
static NpRosterReq *g_roster_head = NULL;
static int g_roster_last_id = 0;
static char g_roster_url[64];

/* =========================================================================
 * TLV Binary Packet Encoders
 * ========================================================================= */

static void tlv_put_u16(u8 *dst, u16 val)
{
    u16 be = bswap16(val);
    sceKernelMemcpy(dst, &be, 2);
}

static u16 tlv_get_u16(const u8 *src)
{
    u16 be;
    sceKernelMemcpy(&be, src, 2);
    return bswap16(be);
}

static int tlv_encode_u8_item(u8 *dst, u16 tag, u8 val)
{
    tlv_put_u16(dst + 0, tag);
    tlv_put_u16(dst + 2, 5);
    tlv_put_u16(dst + 4, 0);
    tlv_put_u16(dst + 6, 1);
    dst[8] = val;
    return 9;
}

static int tlv_encode_blob_item(u8 *dst, u16 tag, const void *data, u16 len)
{
    tlv_put_u16(dst + 0, tag);
    tlv_put_u16(dst + 2, (u16)(len + 4));
    tlv_put_u16(dst + 4, 0);
    tlv_put_u16(dst + 6, len);
    sceKernelMemcpy(dst + 8, data, len);
    return (int)len + 8;
}

static int tlv_encode_npid_item(u8 *dst, const u8 *npid)
{
    tlv_put_u16(dst + 0, 0x1003);
    tlv_put_u16(dst + 2, 28);
    tlv_put_u16(dst + 4, 0);
    tlv_put_u16(dst + 6, 24);
    sceKernelMemcpy(dst + 8, npid, 16);
    sceKernelMemcpy(dst + 24, npid + 20, 2);
    sceKernelMemcpy(dst + 26, npid + 22, 2);
    sceKernelMemset(dst + 28, 0, 4);
    return 32;
}

static int tlv_encode_npid_list(u8 *dst, u8 count, u16 item_tag, const u8 *npids)
{
    u8 *p = dst + 15;
    u16 total;

    tlv_put_u16(dst + 4, 0x9002);
    tlv_put_u16(dst + 6, 1);
    dst[8] = count;
    tlv_put_u16(dst + 9, 0x9006);
    tlv_put_u16(dst + 11, 2);
    tlv_put_u16(dst + 13, item_tag);

    if (item_tag == 0x1003 && count != 0) {
        int i;
        for (i = 0; i < count; i++) {
            tlv_put_u16(p + 0, item_tag);
            tlv_put_u16(p + 2, 28);
            tlv_put_u16(p + 4, 0);
            tlv_put_u16(p + 6, 24);
            sceKernelMemcpy(p + 8, npids, 16);
            sceKernelMemcpy(p + 24, npids + 20, 2);
            sceKernelMemcpy(p + 26, npids + 22, 2);
            sceKernelMemset(p + 28, 0, 4);
            p += 32;
            npids += 36;
        }
    }

    total = (u16)(p - dst);
    tlv_put_u16(dst + 0, 0x1005);
    tlv_put_u16(dst + 2, (u16)(total - 4));
    return total;
}

static void tlv_write_common_hdr(u8 *dst)
{
    tlv_put_u16(dst + 4, 0x9001);
    tlv_put_u16(dst + 6, 2);
    tlv_put_u16(dst + 8, 1);
    tlv_put_u16(dst + 10, 0x9000);
    tlv_put_u16(dst + 12, 1);
    dst[14] = 2;
}

static int tlv_encode_add_friend(u8 *dst, const void *ticket, u16 tlen,
                                 const u8 *npid, const char *msg)
{
    u8 *p;
    u16 total;

    tlv_write_common_hdr(dst);
    p = dst + 15;
    p += tlv_encode_blob_item(p, 0x1002, ticket, tlen);
    p += tlv_encode_npid_item(p, npid);
    if (msg != NULL) {
        u16 mlen = (u16)sceNet_lib_D8722983(msg);
        p += tlv_encode_blob_item(p, 0x1004, msg, mlen);
    }
    total = (u16)(p - dst);
    tlv_put_u16(dst + 0, 0x1000);
    tlv_put_u16(dst + 2, (u16)(total - 4));
    return total;
}

static int tlv_encode_get_list(u8 *dst, u16 outer_tag, const void *ticket, u16 tlen,
                               u8 start, u8 count)
{
    u8 *p;
    u16 total;

    tlv_write_common_hdr(dst);
    p = dst + 15;
    p += tlv_encode_blob_item(p, 0x1002, ticket, tlen);
    p += tlv_encode_u8_item(p, 0x1006, start);
    p += tlv_encode_u8_item(p, 0x1008, count);
    total = (u16)(p - dst);
    tlv_put_u16(dst + 0, outer_tag);
    tlv_put_u16(dst + 2, (u16)(total - 4));
    return total;
}

static int tlv_encode_single_npid(u8 *dst, u16 outer_tag, const void *ticket, u16 tlen,
                                  const u8 *npid)
{
    u8 *p;
    u16 total;

    tlv_write_common_hdr(dst);
    p = dst + 15;
    p += tlv_encode_blob_item(p, 0x1002, ticket, tlen);
    p += tlv_encode_npid_item(p, npid);
    total = (u16)(p - dst);
    tlv_put_u16(dst + 0, outer_tag);
    tlv_put_u16(dst + 2, (u16)(total - 4));
    return total;
}

static int tlv_encode_get_msg(u8 *dst, const void *ticket, u16 tlen, u8 count, const u8 *npids)
{
    u8 *p;
    u16 total;

    tlv_write_common_hdr(dst);
    p = dst + 15;
    p += tlv_encode_blob_item(p, 0x1002, ticket, tlen);
    p += tlv_encode_npid_list(p, count, 0x1003, npids);
    total = (u16)(p - dst);
    tlv_put_u16(dst + 0, 0x1000);
    tlv_put_u16(dst + 2, (u16)(total - 4));
    return total;
}

/* =========================================================================
 * TLV Binary Packet Decoders
 * ========================================================================= */

static int tlv_decode_raw(const u8 *src, u16 expected_tag, void *out, const u16 *expected_len)
{
    u16 tag = tlv_get_u16(src);
    u16 len = tlv_get_u16(src + 2);

    if (tag != expected_tag)
        return (int)0x80550A81;
    if (expected_len != NULL && *expected_len != len)
        return (int)0x80550A83;
    if (out != NULL) {
        sceKernelMemcpy(out, src + 4, len);
        return (int)len + 4;
    }
    return len;
}

static int tlv_decode_header(const u8 *src, u16 expected_tag, u16 *body_len_out)
{
    u16 tag = tlv_get_u16(src);
    u16 len = tlv_get_u16(src + 2);
    const u8 *p;
    u8 err_code;

    if (tag != expected_tag)
        return (int)0x80550A81;
    if (body_len_out != NULL)
        *body_len_out = len;

    p = src + 4;
    if (tlv_get_u16(p) != 0x9001) return (int)0x80550A81;
    if (tlv_get_u16(p + 2) != 2)  return (int)0x80550A83;
    if (tlv_get_u16(p + 4) != 1)  return (int)0x80550A84;

    p += 6;
    if (tlv_get_u16(p) != 0x9000) return (int)0x80550A81;
    if (tlv_get_u16(p + 2) != 1)  return (int)0x80550A83;
    if (p[4] != 2)                return (int)0x80550A85;

    p += 5;
    if (tlv_get_u16(p) != 0x9004) return (int)0x80550A81;
    if (tlv_get_u16(p + 2) != 1)  return (int)0x80550A83;
    err_code = p[4];
    if (err_code != 0) {
        u32 mapped = (u32)err_code | 0x80550B00u;
        if (mapped == 0x80550B4Cu)
            return (int)0x80550007;
        return (int)mapped;
    }
    return (int)((p + 5) - src);
}

static int tlv_decode_u8_item(const u8 *src, u16 expected_tag, u8 *out)
{
    if (tlv_get_u16(src) != expected_tag) return (int)0x80550A81;
    if (tlv_get_u16(src + 2) != 5)        return (int)0x80550A83;
    if (tlv_get_u16(src + 4) != 0)        return (int)0x80550A81;
    if (tlv_get_u16(src + 6) != 1)        return (int)0x80550A83;
    *out = src[8];
    return 9;
}

static int tlv_decode_npid_item(const u8 *src, u16 expected_tag, u8 *npid_out)
{
    if (tlv_get_u16(src) != expected_tag) return (int)0x80550A81;
    if (tlv_get_u16(src + 2) != 28)       return (int)0x80550A83;
    if (tlv_get_u16(src + 4) != 0)        return (int)0x80550A81;
    if (tlv_get_u16(src + 6) != 24)       return (int)0x80550A83;
    sceKernelMemcpy(npid_out, src + 8, 16);
    sceKernelMemcpy(npid_out + 20, src + 24, 2);
    sceKernelMemcpy(npid_out + 22, src + 26, 2);
    sceKernelMemcpy(npid_out + 24, "psp", 4);
    sceKernelMemset(npid_out + 28, 0, 8);
    npid_out[28] = 1;
    return 32;
}

static int tlv_decode_friend_item(const u8 *src, u8 *out)
{
    u8 raw_npid[24];
    u16 npid_len = 24;
    int ret;

    if (tlv_get_u16(src) != 0x1009)     return (int)0x80550A81;
    if (tlv_get_u16(src + 2) != 33)     return (int)0x80550A83;
    if (tlv_get_u16(src + 4) != 0x9005) return (int)0x80550A81;
    if (tlv_get_u16(src + 6) != 1)      return (int)0x80550A83;

    *(u32 *)out = src[8];
    ret = tlv_decode_raw(src + 9, 0, raw_npid, &npid_len);
    if (ret < 0)
        return ret;

    sceKernelMemcpy(out + 4, raw_npid, 16);
    sceKernelMemcpy(out + 24, raw_npid + 16, 2);
    sceKernelMemcpy(out + 26, raw_npid + 18, 2);
    sceKernelMemcpy(out + 28, "psp", 4);
    sceKernelMemset(out + 32, 0, 8);
    out[32] = 1;
    return 41;
}

static int tlv_decode_msg_item(const u8 *src, void *msg_out, u8 *npid_out)
{
    u16 tag = tlv_get_u16(src);
    u16 len = tlv_get_u16(src + 2);
    u16 text_len;
    int ret;

    if (tag != 0x1004)
        return (int)0x80550A81;

    text_len = (u16)(len - 32);
    if (npid_out != NULL) {
        if (tlv_get_u16(src + 4) != 0x9003) return (int)0x80550A81;
        if (tlv_get_u16(src + 6) != 24)     return (int)0x80550A83;
        sceKernelMemcpy(npid_out, src + 8, 16);
        sceKernelMemcpy(npid_out + 20, src + 24, 2);
        sceKernelMemcpy(npid_out + 22, src + 26, 2);
        sceKernelMemcpy(npid_out + 24, "psp", 4);
        sceKernelMemset(npid_out + 28, 0, 8);
        npid_out[28] = 1;
    }

    ret = tlv_decode_raw(src + 32, 0, msg_out, &text_len);
    if (ret < 0)
        return ret;
    return (int)len + 4;
}

static int tlv_decode_list(const u8 *src, u16 expected_item_tag, u8 *out_buf, u8 max_count, u8 *count_out)
{
    u16 tag = tlv_get_u16(src);
    u16 body_len = tlv_get_u16(src + 2);
    u8 count;
    u16 item_tag_be;
    u16 tag_len = 2;
    const u8 *p;
    int ret;

    if (tag != 0x1005)                  return (int)0x80550A81;
    if (tlv_get_u16(src + 4) != 0x9002) return (int)0x80550A81;
    if (tlv_get_u16(src + 6) != 1)      return (int)0x80550A83;

    count = src[8];
    if (max_count != 0 && max_count < count)
        return (int)0x80550A86;
    if (count_out != NULL)
        *count_out = count;

    ret = tlv_decode_raw(src + 9, 0x9006, &item_tag_be, &tag_len);
    if (ret < 0)
        return ret;
    if (bswap16(item_tag_be) != expected_item_tag)
        return (int)0x80550A81;

    p = src + 15;
    if (max_count == 0 || out_buf == NULL)
        return (int)body_len + 4;

    if (expected_item_tag == 0x1003) {
        int i;
        for (i = 0; i < count; i++) {
            ret = tlv_decode_npid_item(p, 0x1003, out_buf + 36 * i);
            if (ret < 0) return ret;
            p += 32;
        }
        return (int)body_len + 4;
    }
    if (expected_item_tag == 0x1009) {
        int i;
        for (i = 0; i < count; i++) {
            ret = tlv_decode_friend_item(p, out_buf + 40 * i);
            if (ret < 0) return ret;
            p += 37;
        }
        return (int)body_len + 4;
    }
    if (expected_item_tag == 0x1004) {
        int i;
        int last_ret = ret;
        for (i = 0; i < count; i++) {
            last_ret = tlv_decode_msg_item(p, out_buf, NULL);
            if (last_ret < 0) return last_ret;
            p += last_ret;
        }
        return last_ret;
    }
    return (int)0x80550A81;
}

static int tlv_decode_list_resp(const u8 *src, u16 outer_tag, u16 item_tag,
                                u8 max_count, int expected_start,
                                int *total_out, int *got_out, void *entries_out)
{
    u16 body_len = 0;
    u8 val = 0;
    const u8 *p;
    int r;

    r = tlv_decode_header(src, outer_tag, &body_len);
    if (r < 0) return r;
    p = src + r;

    r = tlv_decode_u8_item(p, 0x1006, &val);
    if (r < 0) return r;
    if (expected_start != val)
        return (int)0x80550A88;
    p += r;

    r = tlv_decode_u8_item(p, 0x1007, &val);
    if (r < 0) return r;
    if (total_out != NULL)
        *total_out = val;
    p += r;

    r = tlv_decode_list(p, item_tag, (u8 *)entries_out, max_count, &val);
    if (r < 0) return r;
    if (got_out != NULL)
        *got_out = val;
    return (int)((p + r) - src);
}

static int tlv_decode_simple_resp(const u8 *src, u16 outer_tag)
{
    u16 body_len = 0;
    return tlv_decode_header(src, outer_tag, &body_len);
}

static int tlv_decode_msg_resp(const u8 *src, u8 max_count, int *got_out, void *msg_out)
{
    u16 body_len = 0;
    u8 cnt = (u8)(*got_out);
    int r;
    int r2;

    r = tlv_decode_header(src, 0x1000, &body_len);
    if (r < 0) return r;
    r2 = tlv_decode_list(src + r, 0x1004, (u8 *)msg_out, max_count, &cnt);
    if (r2 < 0) return r2;
    *got_out = cnt;
    return r + r2;
}

/* =========================================================================
 * Roster HTTP Transport & Request Lifecycle
 * ========================================================================= */

static int roster_map_ssl_error(int req_id, int err)
{
    if (err == (int)0x80431075) {
        int ssl_err = 0;
        u32 detail = 0;
        int r = sceHttpsGetSslError(req_id, &ssl_err, &detail);
        if (r < 0)
            return r;
        if (ssl_err == (int)0x80435060) {
            if ((detail & 4u) != 0)    return (int)0x80550A8B;
            if ((detail & 0x20u) != 0) return (int)0x80550A8C;
            if ((detail & 2u) != 0)    return (int)0x80550A8D;
            if ((detail & 1u) != 0)    return (int)0x80550A8E;
            return (int)0x80550A8F;
        }
        return ssl_err;
    }
    return err;
}

static int roster_build_url(char *dst, u32 size)
{
    char env[16];
    char region[16];
    char country[16];
    char lang[16];
    int ret;

    if (size < 45u)
        return (int)0x80550A04;

    sceKernelMemset(lang, 0, 3);
    sceKernelMemset(country, 0, 3);
    ret = sceNpCore_52440ABF(env);
    if (ret < 0) return ret;
    ret = sceNp_F2B95034(NULL, region, NULL, lang, NULL);
    if (ret < 0) return ret;

    sceKernelMemcpy(country, region, 2);
    if (lang[0] == '\0' || country[0] == '\0')
        return (int)0x80550A07;

    {
        u32 rem = size;
        sceNp_4B09907A(dst, "https://", rem); rem -= (u32)sceNet_lib_D8722983("https://");
        sceNp_4B09907A(dst, "imgw", rem);     rem -= (u32)sceNet_lib_D8722983("imgw");
        sceNp_4B09907A(dst, ".", rem);        rem -= (u32)sceNet_lib_D8722983(".");
        sceNp_4B09907A(dst, country, rem);    rem -= (u32)sceNet_lib_D8722983(country);
        sceNp_4B09907A(dst, ".", rem);        rem -= (u32)sceNet_lib_D8722983(".");
        sceNp_4B09907A(dst, env, rem);        rem -= (u32)sceNet_lib_D8722983(env);
        sceNp_4B09907A(dst, ".", rem);        rem -= (u32)sceNet_lib_D8722983(".");
        sceNp_4B09907A(dst, "communication.playstation.net", rem);
    }
    return ret;
}

static int roster_open_conn(NpRosterReq *req)
{
    int tid = sceHttpCreateTemplate("PSP NP/1.0", 0, 0);
    int ret = tid;
    if (tid >= 0) {
        ret = sceHttp_87F1E666(tid, 357);
        if (ret >= 0) {
            ret = sceHttp_3C478044(tid, 26);
            if (ret >= 0) {
                sceHttpSetConnectTimeOut(tid, 30000000u);
                sceHttpSetRecvTimeOut(tid, 120000000u);
                sceHttpSetSendTimeOut(tid, 120000000u);
                sceHttpDeleteHeader(tid, "Accept");
                sceHttpDeleteHeader(tid, "Accept-Encoding");
                sceHttpDeleteHeader(tid, "Accept-Charset");
                sceKernelMemset(g_roster_url, 0, sizeof(g_roster_url));
                ret = roster_build_url(g_roster_url, sizeof(g_roster_url));
                if (ret >= 0) {
                    int cid = sceHttpCreateConnectionWithURL(tid, g_roster_url, 0);
                    ret = cid;
                    if (cid >= 0)
                        req->conn_id = cid;
                }
            }
        }
        if (tid > 0)
            sceHttpDeleteTemplate(tid);
    }
    return ret;
}

static void roster_close_conn(NpRosterReq *req)
{
    if (req->conn_id > 0) {
        sceHttpDeleteConnection(req->conn_id);
        req->conn_id = 0;
    }
}

static int roster_http_post(NpRosterReq *req, const char *path, void *buf,
                            u32 req_len, u32 max_resp_len, u64 *resp_len_out)
{
    int rid;
    int ret;
    int status = 0;

    if (req == NULL || path == NULL || buf == NULL || resp_len_out == NULL)
        return (int)0x80550A04;

    rid = sceHttpCreateRequest(req->conn_id, 1, path, (u64)req_len);
    ret = rid;
    if (rid >= 0) {
        req->req_id = rid;
        if (req->aborted > 0) {
            ret = (int)0x80550A05;
        } else {
            ret = sceHttpSendRequest(rid, buf, req_len);
            if (ret < 0) {
                ret = roster_map_ssl_error(req->req_id, ret);
            } else {
                ret = sceHttpGetStatusCode(req->req_id, &status);
                if (ret >= 0) {
                    if (status != 200) {
                        ret = (int)0x80550A89;
                    } else {
                        ret = sceHttpGetContentLength(req->req_id, resp_len_out);
                        if (ret >= 0) {
                            if (*resp_len_out > (u64)max_resp_len) {
                                ret = (int)0x80550A89;
                            } else if (*resp_len_out == 0) {
                                ret = (int)0x80550A08;
                            } else if (req->aborted > 0) {
                                ret = (int)0x80550A05;
                            } else {
                                ret = sceHttpReadData(req->req_id, buf, max_resp_len);
                                if (ret < 0)
                                    ret = roster_map_ssl_error(req->req_id, ret);
                            }
                        }
                    }
                }
            }
        }
    }

    if (req->req_id > 0) {
        sceHttpDeleteRequest(req->req_id);
        req->req_id = 0;
    }
    req->aborted = 0;
    return ret;
}

static NpRosterReq *roster_find_req(int id)
{
    int intr = sceKernelCpuSuspendIntr();
    NpRosterReq *cur = g_roster_head;
    while (cur != NULL && cur->id != id)
        cur = cur->next;
    sceKernelCpuResumeIntr(intr);
    return cur;
}

static void roster_destroy_req_obj(NpRosterReq *req)
{
    int intr = sceKernelCpuSuspendIntr();
    NpRosterReq *cur = g_roster_head;
    NpRosterReq *prev = NULL;

    while (cur != NULL) {
        if (cur == req) {
            if (prev != NULL)
                prev->next = req->next;
            else
                g_roster_head = req->next;
            req->next = NULL;
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    sceKernelCpuResumeIntr(intr);

    if (req != NULL) {
        roster_close_conn(req);
        if (req->mutex > 0) {
            sceKernelDeleteMutex(req->mutex);
            req->mutex = 0;
        }
        sceNpService_61AB2062(req);
    }
}

static int roster_create_req_obj(void)
{
    NpRosterReq *req = (NpRosterReq *)sceNpService_C5094BE6(sizeof(NpRosterReq));
    int intr;
    int cand;
    int assigned;
    SceUID mtx;
    int ret;

    if (req == NULL)
        return (int)0x80550A03;

    sceKernelMemset(req, 0, sizeof(NpRosterReq));
    intr = sceKernelCpuSuspendIntr();
    cand = (g_roster_last_id == 4) ? 1 : (g_roster_last_id + 1);
    assigned = (int)0x80550A06;
    if (cand != g_roster_last_id) {
        for (;;) {
            if (roster_find_req(cand) == NULL) {
                assigned = cand;
                g_roster_last_id = cand;
                break;
            }
            cand = (cand == 4) ? 1 : (cand + 1);
            if (cand == g_roster_last_id)
                break;
        }
    }
    if (assigned < 0) {
        sceKernelCpuResumeIntr(intr);
        sceNpService_61AB2062(req);
        return assigned;
    }

    req->id = assigned;
    req->next = g_roster_head;
    g_roster_head = req;
    sceKernelCpuResumeIntr(intr);

    mtx = sceKernelCreateMutex("SceNpRoster", 0x200, 0, NULL);
    ret = mtx;
    if (mtx >= 0) {
        req->mutex = mtx;
        ret = roster_open_conn(req);
        if (ret >= 0)
            return assigned;
    }

    roster_close_conn(req);
    if (req->mutex > 0) {
        sceKernelDeleteMutex(req->mutex);
        req->mutex = 0;
    }
    roster_destroy_req_obj(req);
    return ret;
}

static int roster_term_all(void)
{
    NpRosterReq *cur = g_roster_head;
    while (cur != NULL) {
        NpRosterReq *next = cur->next;
        roster_close_conn(cur);
        if (cur->mutex > 0) {
            sceKernelDeleteMutex(cur->mutex);
            cur->mutex = 0;
        }
        sceNpService_61AB2062(cur);
        cur = next;
    }
    return 0;
}

/* --- Internal Roster Operations --- */

static int roster_op_get_friends(NpRosterReq *req, u8 count, int start,
                                 int *total_out, int *got_out, void *entries)
{
    u16 tlen = 0;
    void *ticket = NULL;
    u64 resp_len = 0;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_get_list(req->buf, 0x1000, ticket, tlen, (u8)start, count);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/get_friend_list",
                                   req->buf, (u32)plen, 3072u, &resp_len);
            if (ret >= 0)
                return tlv_decode_list_resp(req->buf, 0x1000, 0x1009, count, start, total_out, got_out, entries);
        }
    }
    return ret;
}

static int roster_op_add_friend(NpRosterReq *req, const u8 *npid, const char *msg)
{
    u16 tlen = 0;
    void *ticket = NULL;
    u64 resp_len = 0;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_add_friend(req->buf, ticket, tlen, npid, msg);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/add_friend",
                                   req->buf, (u32)plen, 3072u, &resp_len);
            if (ret >= 0)
                return tlv_decode_simple_resp(req->buf, 0x1000);
        }
    }
    return ret;
}

static int roster_op_del_friend(NpRosterReq *req, const u8 *npid)
{
    u16 tlen = 0;
    void *ticket = NULL;
    u64 resp_len = 0;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_single_npid(req->buf, 0x1000, ticket, tlen, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/remove_friend",
                                   req->buf, (u32)plen, 3072u, &resp_len);
            if (ret >= 0)
                return tlv_decode_simple_resp(req->buf, 0x1000);
        }
    }
    return ret;
}

static int roster_op_accept_friend(NpRosterReq *req, const u8 *npid)
{
    u16 tlen = 0;
    void *ticket = NULL;
    u64 resp_len = 0;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_single_npid(req->buf, 0x1000, ticket, tlen, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/accept_friend",
                                   req->buf, (u32)plen, 3072u, &resp_len);
            if (ret >= 0)
                return tlv_decode_simple_resp(req->buf, 0x1000);
        }
    }
    return ret;
}

static int roster_op_get_msg(NpRosterReq *req, u8 count, int *got_out, const u8 *npid, void *msg_out)
{
    u16 tlen = 0;
    void *ticket = NULL;
    u64 resp_len = 0;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_get_msg(req->buf, ticket, tlen, count, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/get_friend_message_list",
                                   req->buf, (u32)plen, 3072u, &resp_len);
            if (ret >= 0)
                return tlv_decode_msg_resp(req->buf, count, got_out, msg_out);
        }
    }
    return ret;
}

static int roster_op_get_blocks(NpRosterReq *req, u8 count, int start,
                                int *total_out, int *got_out, void *entries)
{
    u16 tlen = 0;
    void *ticket = NULL;
    u64 resp_len = 0;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_get_list(req->buf, 0x1001, ticket, tlen, (u8)start, count);
            ret = roster_http_post(req, "/imgw/sec/psp/block/get_block_list",
                                   req->buf, (u32)plen, 3072u, &resp_len);
            if (ret >= 0)
                return tlv_decode_list_resp(req->buf, 0x1001, 0x1003, count, start, total_out, got_out, entries);
        }
    }
    return ret;
}

static int roster_op_add_block(NpRosterReq *req, const u8 *npid)
{
    u16 tlen = 0;
    void *ticket = NULL;
    u64 resp_len = 0;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_single_npid(req->buf, 0x1001, ticket, tlen, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/block/add_block_list",
                                   req->buf, (u32)plen, 3072u, &resp_len);
            if (ret >= 0)
                return tlv_decode_simple_resp(req->buf, 0x1001);
        }
    }
    return ret;
}

static int roster_op_del_block(NpRosterReq *req, const u8 *npid)
{
    u16 tlen = 0;
    void *ticket = NULL;
    u64 resp_len = 0;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_single_npid(req->buf, 0x1001, ticket, tlen, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/block/remove_block_list",
                                   req->buf, (u32)plen, 3072u, &resp_len);
            if (ret >= 0)
                return tlv_decode_simple_resp(req->buf, 0x1001);
        }
    }
    return ret;
}

/* =========================================================================
 * Exported sceNpRoster* Functions
 * ========================================================================= */

int np_roster_init(void)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return (int)0x80550004;
    if (g_roster_init > 0)
        return (int)0x80550A01;
    g_roster_head = NULL;
    g_roster_init = 1;
    return 0;
}

int np_roster_term(void)
{
    int ret = _sceKernelCheckThreadStack();
    if (ret >= 4080 && g_roster_init > 0) {
        ret = roster_term_all();
        g_roster_init = 0;
    }
    return ret;
}

int sceNpRosterCreateRequest(void)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return (int)0x80550004;
    return roster_create_req_obj();
}

int sceNpRosterDeleteRequest(int req_id)
{
    NpRosterReq *req;
    if (_sceKernelCheckThreadStack() < 4080)
        return (int)0x80550004;
    req = roster_find_req(req_id);
    if (req == NULL)
        return (int)0x80550A04;
    roster_destroy_req_obj(req);
    return 0;
}

int sceNpRosterAbort(int req_id)
{
    NpRosterReq *req;
    if (_sceKernelCheckThreadStack() < 4080)
        return (int)0x80550004;
    if (g_roster_init <= 0)
        return (int)0x80550A02;
    req = roster_find_req(req_id);
    if (req == NULL)
        return (int)0x80550A04;
    req->aborted = 1;
    if (req->req_id != 0)
        return sceHttpAbortRequest(req->req_id);
    return 0;
}

int sceNpRosterGetFriendListEntryCount(int req_id, const u32 *opt)
{
    NpRosterReq *req;
    int total = 0;
    int ret;

    if (_sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_get_friends(req, 0, 0, &total, NULL, NULL);
        if (ret >= 0)
            ret = total;
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}

int sceNpRosterGetFriendListEntry(int req_id, u8 count, int start,
                                  int *total_out, int *got_out, void *entries,
                                  const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (_sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_get_friends(req, count, start, total_out, got_out, entries);
        if (ret >= 0)
            ret = *total_out;
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}

int sceNpRosterAddFriendListEntry(int req_id, const u8 *npid, const char *msg, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (_sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_add_friend(req, npid, msg);
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}

int sceNpRosterDeleteFriendListEntry(int req_id, const u8 *npid, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (_sceKernelCheckThreadStack() < 14320) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_del_friend(req, npid);
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}

int sceNpRosterAcceptFriendListEntry(int req_id, const u8 *npid, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (_sceKernelCheckThreadStack() < 14320) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_accept_friend(req, npid);
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}

int sceNpRosterGetFriendListMessage(int req_id, const u8 *npid, void *msg_out, const u32 *opt)
{
    NpRosterReq *req;
    int got = 0;
    int ret;

    if (_sceKernelCheckThreadStack() < 14288) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    if (npid == NULL || msg_out == NULL) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    sceKernelMemset(msg_out, 0, 1540);
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_get_msg(req, 1, &got, npid, msg_out);
        if (ret >= 0)
            ret = got;
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}

int sceNpRosterGetBlockListEntryCount(int req_id, const u32 *opt)
{
    NpRosterReq *req;
    int total = 0;
    int ret;

    if (_sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_get_blocks(req, 0, 0, &total, NULL, NULL);
        if (ret >= 0)
            ret = total;
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}

int sceNpRosterGetBlockListEntry(int req_id, u8 count, int start,
                                 int *total_out, int *got_out, void *entries,
                                 const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (_sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_get_blocks(req, count, start, total_out, got_out, entries);
        if (ret >= 0)
            ret = *total_out;
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}

int sceNpRosterAddBlockListEntry(int req_id, const u8 *npid, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (_sceKernelCheckThreadStack() < 14320) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_add_block(req, npid);
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}

int sceNpRosterDeleteBlockListEntry(int req_id, const u8 *npid, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (_sceKernelCheckThreadStack() < 14320) return (int)0x80550004;
    if (g_roster_init <= 0) return (int)0x80550A02;
    if (opt != NULL && *opt != 4u) return (int)0x80550A04;
    req = roster_find_req(req_id);
    if (req == NULL) return (int)0x80550A04;
    ret = sceKernelLockMutex(req->mutex, 1, NULL);
    if (ret >= 0) {
        ret = roster_op_del_block(req, npid);
        sceKernelUnlockMutex(req->mutex, 1);
    }
    return ret;
}
