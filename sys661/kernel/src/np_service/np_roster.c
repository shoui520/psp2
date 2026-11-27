#include "np_service_internal.h"

/* =========================================================================
 * Roster Request Context & Global State
 * ========================================================================= */

typedef struct NpRosterFriendEntry {
    u32 status;
    NpId npid;
} NpRosterFriendEntry; /* 40 bytes */

typedef struct NpRosterReq {
    struct NpRosterReq *next;
    int id;
    int conn_id;
    int req_id;
    SceUID mutex;
    int aborted;
    u8 packet_buf[3072];
} NpRosterReq; /* 3096 bytes */

static int g_roster_init = 0;
static NpRosterReq *g_roster_head = NULL;
static int g_roster_last_id = 0;
static char g_roster_url[64];

/* =========================================================================
 * Inline TLV Wire Primitives
 * ========================================================================= */

static inline __attribute__((always_inline)) void tlv_put_hdr(u8 *dst, u16 tag, u16 len)
{
    u16 be = bswap16(tag);
    sceKernelMemcpy(dst, &be, 2);
    be = bswap16(len);
    sceKernelMemcpy(dst + 2, &be, 2);
}

static inline __attribute__((always_inline)) void tlv_put_u16(u8 *dst, u16 val)
{
    u16 be = bswap16(val);
    sceKernelMemcpy(dst, &be, 2);
}

static inline __attribute__((always_inline)) void tlv_put_u8(u8 *dst, u8 val)
{
    u8 octet = val;
    sceKernelMemcpy(dst, &octet, 1);
}

static inline __attribute__((always_inline)) u16 tlv_get_u16(const u8 *src)
{
    u16 be;
    sceKernelMemcpy(&be, src, 2);
    return bswap16(be);
}

static inline __attribute__((always_inline)) u8 tlv_get_u8(const u8 *src)
{
    u8 octet;
    sceKernelMemcpy(&octet, src, 1);
    return octet;
}

static inline __attribute__((always_inline)) void tlv_write_common_hdr(u8 *dst)
{
    tlv_put_hdr(dst + 4, 0x9001, 2);
    tlv_put_u16(dst + 8, 1);
    tlv_put_hdr(dst + 10, 0x9000, 1);
    tlv_put_u8(dst + 14, 2);
}

static inline __attribute__((always_inline)) int tlv_encode_blob_inline(u8 *dst, u16 tag, const void *payload, u16 len)
{
    tlv_put_hdr(dst + 0, tag, (u16)(len + 4));
    tlv_put_hdr(dst + 4, 0, len);
    sceKernelMemcpy(dst + 8, payload, len);
    return (int)len + 8;
}

static inline __attribute__((always_inline)) int tlv_encode_npid_inline(u8 *dst, const NpId *npid)
{
    tlv_put_hdr(dst + 0, 0x1003, 28);
    tlv_put_hdr(dst + 4, 0, 24);
    sceKernelMemcpy(dst + 8, npid->handle, 16);
    sceKernelMemcpy(dst + 24, npid->opt_country, 2);
    sceKernelMemcpy(dst + 26, npid->opt_region, 2);
    sceKernelMemset(dst + 28, 0, 4);
    return 32;
}

/* =========================================================================
 * TLV Binary Packet Encoders
 * ========================================================================= */

static int __attribute__((noinline)) tlv_encode_u8_item(u8 *dst, u16 tag, u8 val)
{
    tlv_put_hdr(dst + 0, tag, 5);
    tlv_put_hdr(dst + 4, 0, 1);
    tlv_put_u8(dst + 8, val);
    return 9;
}

static int __attribute__((noinline)) tlv_encode_blob_item(u8 *dst, u16 tag, const void *payload, u16 len)
{
    return tlv_encode_blob_inline(dst, tag, payload, len);
}

static int __attribute__((noinline)) tlv_encode_npid_item(u8 *dst, const NpId *npid)
{
    return tlv_encode_npid_inline(dst, npid);
}

static int __attribute__((noinline)) tlv_encode_npid_list(u8 *dst, u8 count, u16 item_tag, const NpId *npids)
{
    u8 *cursor = dst + 15;
    u16 total;

    tlv_put_hdr(dst + 4, 0x9002, 1);
    tlv_put_u8(dst + 8, count);
    tlv_put_hdr(dst + 9, 0x9006, 2);
    tlv_put_u16(dst + 13, item_tag);

    if (item_tag == 0x1003 && count != 0) {
        int i;
        for (i = 0; i < count; i++) {
            tlv_put_hdr(cursor + 0, item_tag, 28);
            tlv_put_hdr(cursor + 4, 0, 24);
            sceKernelMemcpy(cursor + 8, npids[i].handle, 16);
            sceKernelMemcpy(cursor + 24, npids[i].opt_country, 2);
            sceKernelMemcpy(cursor + 26, npids[i].opt_region, 2);
            sceKernelMemset(cursor + 28, 0, 4);
            cursor += 32;
        }
    }

    total = (u16)(cursor - dst);
    tlv_put_hdr(dst + 0, 0x1005, (u16)(total - 4));
    return total;
}

static int __attribute__((noinline)) tlv_encode_add_friend(u8 *dst, const void *ticket, u16 tlen,
                                                           const NpId *npid, const char *msg)
{
    u8 *cursor;
    u16 total;

    tlv_write_common_hdr(dst);
    cursor = dst + 15;
    cursor += tlv_encode_blob_inline(cursor, 0x1002, ticket, tlen);
    cursor += tlv_encode_npid_inline(cursor, npid);
    if (msg != NULL) {
        u16 mlen = (u16)sceNet_lib_D8722983(msg);
        cursor += tlv_encode_blob_inline(cursor, 0x1004, msg, mlen);
    }
    total = (u16)(cursor - dst);
    tlv_put_hdr(dst + 0, 0x1000, (u16)(total - 4));
    return total;
}

static int __attribute__((noinline)) tlv_encode_get_friends(u8 *dst, const void *ticket, u16 tlen,
                                                            u8 start, u8 count)
{
    u8 *cursor;
    u16 total;

    tlv_write_common_hdr(dst);
    cursor = dst + 15;
    cursor += tlv_encode_blob_item(cursor, 0x1002, ticket, tlen);
    cursor += tlv_encode_u8_item(cursor, 0x1006, start);
    cursor += tlv_encode_u8_item(cursor, 0x1008, count);
    total = (u16)(cursor - dst);
    tlv_put_hdr(dst + 0, 0x1000, (u16)(total - 4));
    return total;
}

static int __attribute__((noinline)) tlv_encode_del_friend(u8 *dst, const void *ticket, u16 tlen,
                                                           const NpId *npid)
{
    u8 *cursor;
    u16 total;

    tlv_write_common_hdr(dst);
    cursor = dst + 15;
    cursor += tlv_encode_blob_item(cursor, 0x1002, ticket, tlen);
    cursor += tlv_encode_npid_item(cursor, npid);
    total = (u16)(cursor - dst);
    tlv_put_hdr(dst + 0, 0x1000, (u16)(total - 4));
    return total;
}

static int __attribute__((noinline)) tlv_encode_accept_friend(u8 *dst, const void *ticket, u16 tlen,
                                                              const NpId *npid)
{
    u8 *cursor;
    u16 total;

    tlv_write_common_hdr(dst);
    cursor = dst + 15;
    cursor += tlv_encode_blob_item(cursor, 0x1002, ticket, tlen);
    cursor += tlv_encode_npid_item(cursor, npid);
    total = (u16)(cursor - dst);
    tlv_put_hdr(dst + 0, 0x1000, (u16)(total - 4));
    return total;
}

static int __attribute__((noinline)) tlv_encode_get_msg(u8 *dst, const void *ticket, u16 tlen,
                                                        u8 count, const NpId *npids)
{
    u8 *cursor;
    u16 total;

    tlv_write_common_hdr(dst);
    cursor = dst + 15;
    cursor += tlv_encode_blob_item(cursor, 0x1002, ticket, tlen);
    cursor += tlv_encode_npid_list(cursor, count, 0x1003, npids);
    total = (u16)(cursor - dst);
    tlv_put_hdr(dst + 0, 0x1000, (u16)(total - 4));
    return total;
}

static int __attribute__((noinline)) tlv_encode_get_blocks(u8 *dst, const void *ticket, u16 tlen,
                                                           u8 start, u8 count)
{
    u8 *cursor;
    u16 total;

    tlv_write_common_hdr(dst);
    cursor = dst + 15;
    cursor += tlv_encode_blob_item(cursor, 0x1002, ticket, tlen);
    cursor += tlv_encode_u8_item(cursor, 0x1006, start);
    cursor += tlv_encode_u8_item(cursor, 0x1008, count);
    total = (u16)(cursor - dst);
    tlv_put_hdr(dst + 0, 0x1001, (u16)(total - 4));
    return total;
}

static int __attribute__((noinline)) tlv_encode_add_block(u8 *dst, const void *ticket, u16 tlen,
                                                          const NpId *npid)
{
    u8 *cursor;
    u16 total;

    tlv_write_common_hdr(dst);
    cursor = dst + 15;
    cursor += tlv_encode_blob_item(cursor, 0x1002, ticket, tlen);
    cursor += tlv_encode_npid_item(cursor, npid);
    total = (u16)(cursor - dst);
    tlv_put_hdr(dst + 0, 0x1001, (u16)(total - 4));
    return total;
}

static int __attribute__((noinline)) tlv_encode_del_block(u8 *dst, const void *ticket, u16 tlen,
                                                          const NpId *npid)
{
    u8 *cursor;
    u16 total;

    tlv_write_common_hdr(dst);
    cursor = dst + 15;
    cursor += tlv_encode_blob_item(cursor, 0x1002, ticket, tlen);
    cursor += tlv_encode_npid_item(cursor, npid);
    total = (u16)(cursor - dst);
    tlv_put_hdr(dst + 0, 0x1001, (u16)(total - 4));
    return total;
}

/* =========================================================================
 * TLV Binary Packet Decoders
 * ========================================================================= */

static int __attribute__((noinline)) tlv_decode_raw(const u8 *src, u16 expected_tag, void *out, const u16 *expected_len)
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

static int __attribute__((noinline)) tlv_decode_header(const u8 *src, u16 expected_tag, u16 *body_len_out)
{
    u16 tag = tlv_get_u16(src);
    u16 len = tlv_get_u16(src + 2);
    const u8 *cursor;
    u8 err_code;

    if (tag != expected_tag)
        return (int)0x80550A81;
    if (body_len_out != NULL)
        *body_len_out = len;

    cursor = src + 4;
    if (tlv_get_u16(cursor) != 0x9001) return (int)0x80550A81;
    if (tlv_get_u16(cursor + 2) != 2)  return (int)0x80550A83;
    if (tlv_get_u16(cursor + 4) != 1)  return (int)0x80550A84;

    cursor += 6;
    if (tlv_get_u16(cursor) != 0x9000) return (int)0x80550A81;
    if (tlv_get_u16(cursor + 2) != 1)  return (int)0x80550A83;
    if (tlv_get_u8(cursor + 4) != 2)   return (int)0x80550A85;

    cursor += 5;
    if (tlv_get_u16(cursor) != 0x9004) return (int)0x80550A81;
    if (tlv_get_u16(cursor + 2) != 1)  return (int)0x80550A83;
    err_code = tlv_get_u8(cursor + 4);
    if (err_code != 0) {
        u32 mapped = (u32)err_code | 0x80550B00u;
        if (mapped == 0x80550B4Cu)
            return (int)0x80550007;
        return (int)mapped;
    }
    return (int)((cursor + 5) - src);
}

static int __attribute__((noinline)) tlv_decode_u8_item(const u8 *src, u16 expected_tag, u8 *out)
{
    if (tlv_get_u16(src) != expected_tag) return (int)0x80550A81;
    if (tlv_get_u16(src + 2) != 5)        return (int)0x80550A83;
    if (tlv_get_u16(src + 4) != 0)        return (int)0x80550A81;
    if (tlv_get_u16(src + 6) != 1)        return (int)0x80550A83;
    sceKernelMemcpy(out, src + 8, 1);
    return 9;
}

static int __attribute__((noinline)) tlv_decode_npid_item(const u8 *src, u16 expected_tag, NpId *npid_out)
{
    if (tlv_get_u16(src) != expected_tag) return (int)0x80550A81;
    if (tlv_get_u16(src + 2) != 28)       return (int)0x80550A83;
    if (tlv_get_u16(src + 4) != 0)        return (int)0x80550A81;
    if (tlv_get_u16(src + 6) != 24)       return (int)0x80550A83;
    sceKernelMemcpy(npid_out->handle, src + 8, 16);
    sceKernelMemcpy(npid_out->opt_country, src + 24, 2);
    sceKernelMemcpy(npid_out->opt_region, src + 26, 2);
    sceKernelMemcpy(npid_out->platform, "psp", 4);
    sceKernelMemset(npid_out->opt_flags, 0, 8);
    npid_out->opt_flags[0] = 1;
    return 32;
}

static int __attribute__((noinline)) tlv_decode_friend_item(const u8 *src, NpRosterFriendEntry *out)
{
    u8 raw_npid[24];
    u16 npid_len = 24;
    int ret;

    if (tlv_get_u16(src) != 0x1009)     return (int)0x80550A81;
    if (tlv_get_u16(src + 2) != 33)     return (int)0x80550A83;
    if (tlv_get_u16(src + 4) != 0x9005) return (int)0x80550A81;
    if (tlv_get_u16(src + 6) != 1)      return (int)0x80550A83;

    out->status = tlv_get_u8(src + 8);
    ret = tlv_decode_raw(src + 9, 0, raw_npid, &npid_len);
    if (ret < 0)
        return ret;

    sceKernelMemcpy(out->npid.handle, raw_npid, 16);
    sceKernelMemcpy(out->npid.opt_country, raw_npid + 16, 2);
    sceKernelMemcpy(out->npid.opt_region, raw_npid + 18, 2);
    sceKernelMemcpy(out->npid.platform, "psp", 4);
    sceKernelMemset(out->npid.opt_flags, 0, 8);
    out->npid.opt_flags[0] = 1;
    return 41;
}

static int __attribute__((noinline)) tlv_decode_msg_item(const u8 *src, void *msg_out, NpId *npid_out)
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
        sceKernelMemcpy(npid_out->handle, src + 8, 16);
        sceKernelMemcpy(npid_out->opt_country, src + 24, 2);
        sceKernelMemcpy(npid_out->opt_region, src + 26, 2);
        sceKernelMemcpy(npid_out->platform, "psp", 4);
        sceKernelMemset(npid_out->opt_flags, 0, 8);
        npid_out->opt_flags[0] = 1;
    }

    ret = tlv_decode_raw(src + 32, 0, msg_out, &text_len);
    if (ret < 0)
        return ret;
    return (int)len + 4;
}

static int __attribute__((noinline)) tlv_decode_list(const u8 *src, u16 expected_item_tag, void *out_buf, u8 max_count, u8 *count_out)
{
    u16 tag = tlv_get_u16(src);
    u16 body_len = tlv_get_u16(src + 2);
    u8 count;
    u16 item_tag_be;
    u16 tag_len = 2;
    const u8 *cursor;
    int ret;

    if (tag != 0x1005)                  return (int)0x80550A81;
    if (tlv_get_u16(src + 4) != 0x9002) return (int)0x80550A81;
    if (tlv_get_u16(src + 6) != 1)      return (int)0x80550A83;

    count = tlv_get_u8(src + 8);
    if (max_count != 0 && max_count < count)
        return (int)0x80550A86;
    if (count_out != NULL)
        *count_out = count;

    ret = tlv_decode_raw(src + 9, 0x9006, &item_tag_be, &tag_len);
    if (ret < 0)
        return ret;
    if (bswap16(item_tag_be) != expected_item_tag)
        return (int)0x80550A81;

    cursor = src + 15;
    if (max_count == 0 || out_buf == NULL)
        return (int)body_len + 4;

    if (expected_item_tag == 0x1003) {
        NpId *npids = (NpId *)out_buf;
        int i;
        for (i = 0; i < *count_out; i++) {
            ret = tlv_decode_npid_item(cursor, 0x1003, &npids[i]);
            if (ret < 0) return ret;
            cursor += 32;
        }
        return (int)body_len + 4;
    }
    if (expected_item_tag == 0x1009) {
        NpRosterFriendEntry *friends = (NpRosterFriendEntry *)out_buf;
        int i;
        for (i = 0; i < *count_out; i++) {
            ret = tlv_decode_friend_item(cursor, &friends[i]);
            if (ret < 0) return ret;
            cursor += 37;
        }
        return (int)body_len + 4;
    }
    if (expected_item_tag == 0x1004) {
        int i;
        int last_ret = ret;
        for (i = 0; i < *count_out; i++) {
            last_ret = tlv_decode_msg_item(cursor, out_buf, NULL);
            if (last_ret < 0) return last_ret;
            cursor += last_ret;
        }
        return last_ret;
    }
    return (int)0x80550A81;
}

static int __attribute__((noinline)) tlv_decode_get_friends_resp(const u8 *src, int max_count, int expected_start,
                                                                 int *total_out, int *got_out, void *entries_out)
{
    u16 body_len;
    u8 val;
    const u8 *cursor;
    int ret;

    ret = tlv_decode_header(src, 0x1000, &body_len);
    if (ret < 0) return ret;
    cursor = src + ret;

    ret = tlv_decode_u8_item(cursor, 0x1006, &val);
    if (ret < 0) return ret;
    if (expected_start != val)
        return (int)0x80550A88;
    cursor += ret;

    ret = tlv_decode_u8_item(cursor, 0x1007, &val);
    if (ret < 0) return ret;
    if (total_out != NULL)
        *total_out = val;
    cursor += ret;

    ret = tlv_decode_list(cursor, 0x1009, entries_out, (u8)max_count, &val);
    if (ret < 0) return ret;
    if (got_out != NULL)
        *got_out = val;
    return (int)((cursor + ret) - src);
}

static int __attribute__((noinline)) tlv_decode_get_blocks_resp(const u8 *src, int max_count, int expected_start,
                                                                int *total_out, int *got_out, void *entries_out)
{
    u16 body_len;
    u8 val;
    const u8 *cursor;
    int ret;

    ret = tlv_decode_header(src, 0x1001, &body_len);
    if (ret < 0) return ret;
    cursor = src + ret;

    ret = tlv_decode_u8_item(cursor, 0x1006, &val);
    if (ret < 0) return ret;
    if (expected_start != val)
        return (int)0x80550A88;
    cursor += ret;

    ret = tlv_decode_u8_item(cursor, 0x1007, &val);
    if (ret < 0) return ret;
    if (total_out != NULL)
        *total_out = val;
    cursor += ret;

    ret = tlv_decode_list(cursor, 0x1003, entries_out, (u8)max_count, &val);
    if (ret < 0) return ret;
    if (got_out != NULL)
        *got_out = val;
    return (int)((cursor + ret) - src);
}

static int __attribute__((noinline)) tlv_decode_add_friend_resp(const u8 *src)
{
    u16 body_len;
    int ret = tlv_decode_header(src, 0x1000, &body_len);
    if (ret >= 0)
        return (int)((src + ret) - src);
    return ret;
}

static int __attribute__((noinline)) tlv_decode_del_friend_resp(const u8 *src)
{
    u16 body_len;
    int ret = tlv_decode_header(src, 0x1000, &body_len);
    if (ret >= 0)
        return (int)((src + ret) - src);
    return ret;
}

static int __attribute__((noinline)) tlv_decode_accept_friend_resp(const u8 *src)
{
    u16 body_len;
    int ret = tlv_decode_header(src, 0x1000, &body_len);
    if (ret >= 0)
        return (int)((src + ret) - src);
    return ret;
}

static int __attribute__((noinline)) tlv_decode_get_msg_resp(const u8 *src, int max_count, int *got_out, void *msg_out)
{
    u16 body_len;
    u8 cnt;
    int hdr_ret;
    int list_ret;

    hdr_ret = tlv_decode_header(src, 0x1000, &body_len);
    if (hdr_ret < 0) return hdr_ret;
    cnt = *(const u8 *)got_out;
    list_ret = tlv_decode_list(src + hdr_ret, 0x1004, msg_out, (u8)max_count, &cnt);
    if (list_ret < 0) return list_ret;
    *got_out = cnt;
    return hdr_ret + list_ret;
}

static int __attribute__((noinline)) tlv_decode_add_block_resp(const u8 *src)
{
    u16 body_len;
    int ret = tlv_decode_header(src, 0x1001, &body_len);
    if (ret >= 0)
        return (int)((src + ret) - src);
    return ret;
}

static int __attribute__((noinline)) tlv_decode_del_block_resp(const u8 *src)
{
    u16 body_len;
    int ret = tlv_decode_header(src, 0x1001, &body_len);
    if (ret >= 0)
        return (int)((src + ret) - src);
    return ret;
}

/* =========================================================================
 * Roster HTTP Transport & Request Lifecycle
 * ========================================================================= */

static int __attribute__((noinline)) roster_map_ssl_error(int req_id, int err)
{
    if (err == (int)0x80431075) {
        int ssl_err = 0;
        u32 detail = 0;
        int ret = sceHttpsGetSslError(req_id, &ssl_err, &detail);
        if (ret < 0)
            return ret;
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

static int __attribute__((noinline)) roster_build_url(char *dst, u32 size)
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

static int __attribute__((noinline)) roster_open_conn(NpRosterReq *req)
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

static void __attribute__((noinline)) roster_close_conn(NpRosterReq *req)
{
    if (req->conn_id > 0) {
        sceHttpDeleteConnection(req->conn_id);
        req->conn_id = 0;
    }
}

static int __attribute__((noinline)) roster_http_post(NpRosterReq *req, const char *path, void *packet_buf,
                                                      u64 req_len, u64 max_resp_len, u64 *resp_len_out)
{
    int rid;
    int ret;
    int status = 0;

    if (req == NULL || path == NULL || packet_buf == NULL || resp_len_out == NULL)
        return (int)0x80550A04;

    rid = sceHttpCreateRequest(req->conn_id, 1, path, req_len);
    ret = rid;
    if (rid >= 0) {
        req->req_id = rid;
        if (req->aborted > 0) {
            ret = (int)0x80550A05;
        } else {
            ret = sceHttpSendRequest(rid, packet_buf, (u32)req_len);
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
                            if (*resp_len_out > max_resp_len) {
                                ret = (int)0x80550A89;
                            } else if (*resp_len_out == 0) {
                                ret = (int)0x80550A08;
                            } else if (req->aborted > 0) {
                                ret = (int)0x80550A05;
                            } else {
                                ret = sceHttpReadData(req->req_id, packet_buf, (u32)max_resp_len);
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
        int del_id = req->req_id;
        req->req_id = 0;
        sceHttpDeleteRequest(del_id);
    }
    req->aborted = 0;
    return ret;
}

static int __attribute__((noinline)) roster_init_list(void)
{
    g_roster_head = NULL;
    return 0;
}

static NpRosterReq * __attribute__((noinline)) roster_find_req(int id)
{
    int intr = sceKernelCpuSuspendIntr();
    NpRosterReq *cur = g_roster_head;
    while (cur != NULL && cur->id != id)
        cur = cur->next;
    sceKernelCpuResumeIntr(intr);
    return cur;
}

static void __attribute__((noinline)) roster_destroy_req_obj(NpRosterReq *req)
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

static int __attribute__((noinline)) roster_create_req_obj(void)
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

static int __attribute__((noinline)) roster_term_all(void)
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

static int __attribute__((noinline)) roster_op_get_friends(NpRosterReq *req, int count, int start,
                                                           int *total_out, int *got_out, void *entries)
{
    u16 tlen;
    void *ticket;
    u64 resp_len;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_get_friends(req->packet_buf, ticket, tlen, (u8)start, (u8)count);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/get_friend_list",
                                   req->packet_buf, (u64)(u32)plen, 3072ULL, &resp_len);
            if (ret >= 0)
                return tlv_decode_get_friends_resp(req->packet_buf, count, start, total_out, got_out, entries);
        }
    }
    return ret;
}

static int __attribute__((noinline)) roster_op_add_friend(NpRosterReq *req, const NpId *npid, const char *msg)
{
    u16 tlen;
    void *ticket;
    u64 resp_len;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_add_friend(req->packet_buf, ticket, tlen, npid, msg);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/add_friend",
                                   req->packet_buf, (u64)(u32)plen, 3072ULL, &resp_len);
            if (ret >= 0)
                return tlv_decode_add_friend_resp(req->packet_buf);
        }
    }
    return ret;
}

static int __attribute__((noinline)) roster_op_del_friend(NpRosterReq *req, const NpId *npid)
{
    u16 tlen;
    void *ticket;
    u64 resp_len;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_del_friend(req->packet_buf, ticket, tlen, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/remove_friend",
                                   req->packet_buf, (u64)(u32)plen, 3072ULL, &resp_len);
            if (ret >= 0)
                return tlv_decode_del_friend_resp(req->packet_buf);
        }
    }
    return ret;
}

static int __attribute__((noinline)) roster_op_accept_friend(NpRosterReq *req, const NpId *npid)
{
    u16 tlen;
    void *ticket;
    u64 resp_len;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_accept_friend(req->packet_buf, ticket, tlen, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/accept_friend",
                                   req->packet_buf, (u64)(u32)plen, 3072ULL, &resp_len);
            if (ret >= 0)
                return tlv_decode_accept_friend_resp(req->packet_buf);
        }
    }
    return ret;
}

static int __attribute__((noinline)) roster_op_get_msg(NpRosterReq *req, int count, int *got_out, const NpId *npid, void *msg_out)
{
    u16 tlen;
    void *ticket;
    u64 resp_len;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_get_msg(req->packet_buf, ticket, tlen, (u8)count, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/friend/get_friend_message_list",
                                   req->packet_buf, (u64)(u32)plen, 3072ULL, &resp_len);
            if (ret >= 0)
                return tlv_decode_get_msg_resp(req->packet_buf, count, got_out, msg_out);
        }
    }
    return ret;
}

static int __attribute__((noinline)) roster_op_get_blocks(NpRosterReq *req, int count, int start,
                                                          int *total_out, int *got_out, void *entries)
{
    u16 tlen;
    void *ticket;
    u64 resp_len;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_get_blocks(req->packet_buf, ticket, tlen, (u8)start, (u8)count);
            ret = roster_http_post(req, "/imgw/sec/psp/block/get_block_list",
                                   req->packet_buf, (u64)(u32)plen, 3072ULL, &resp_len);
            if (ret >= 0)
                return tlv_decode_get_blocks_resp(req->packet_buf, count, start, total_out, got_out, entries);
        }
    }
    return ret;
}

static int __attribute__((noinline)) roster_op_add_block(NpRosterReq *req, const NpId *npid)
{
    u16 tlen;
    void *ticket;
    u64 resp_len;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_add_block(req->packet_buf, ticket, tlen, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/block/add_block_list",
                                   req->packet_buf, (u64)(u32)plen, 3072ULL, &resp_len);
            if (ret >= 0)
                return tlv_decode_add_block_resp(req->packet_buf);
        }
    }
    return ret;
}

static int __attribute__((noinline)) roster_op_del_block(NpRosterReq *req, const NpId *npid)
{
    u16 tlen;
    void *ticket;
    u64 resp_len;
    int ret;

    ret = sceNp_67BCF9E3(0, &tlen);
    if (ret >= 0) {
        ret = sceNp_9B87B19B(&ticket);
        if (ret >= 0) {
            int plen = tlv_encode_del_block(req->packet_buf, ticket, tlen, npid);
            ret = roster_http_post(req, "/imgw/sec/psp/block/remove_block_list",
                                   req->packet_buf, (u64)(u32)plen, 3072ULL, &resp_len);
            if (ret >= 0)
                return tlv_decode_del_block_resp(req->packet_buf);
        }
    }
    return ret;
}

/* =========================================================================
 * Exported sceNpRoster* Functions
 * ========================================================================= */

int np_roster_init(void)
{
    if (sceKernelCheckThreadStack() < 4080)
        return (int)0x80550004;
    if (g_roster_init > 0)
        return (int)0x80550A01;
    roster_init_list();
    g_roster_init = 1;
    return 0;
}

int np_roster_term(void)
{
    int ret = sceKernelCheckThreadStack();
    if (ret >= 4080 && g_roster_init > 0) {
        ret = roster_term_all();
        g_roster_init = 0;
    }
    return ret;
}

int sceNpRosterCreateRequest(void)
{
    if (sceKernelCheckThreadStack() < 4080)
        return (int)0x80550004;
    return roster_create_req_obj();
}

int sceNpRosterDeleteRequest(int req_id)
{
    NpRosterReq *req;
    if (sceKernelCheckThreadStack() < 4080)
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
    if (sceKernelCheckThreadStack() < 4080)
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

    if (sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
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

    if (sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
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

int sceNpRosterAddFriendListEntry(int req_id, const NpId *npid, const char *msg, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
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

int sceNpRosterDeleteFriendListEntry(int req_id, const NpId *npid, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (sceKernelCheckThreadStack() < 14320) return (int)0x80550004;
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

int sceNpRosterAcceptFriendListEntry(int req_id, const NpId *npid, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (sceKernelCheckThreadStack() < 14320) return (int)0x80550004;
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

int sceNpRosterGetFriendListMessage(int req_id, const NpId *npid, void *msg_out, const u32 *opt)
{
    NpRosterReq *req;
    int got;
    int ret;

    if (sceKernelCheckThreadStack() < 14288) return (int)0x80550004;
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

    if (sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
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

    if (sceKernelCheckThreadStack() < 14304) return (int)0x80550004;
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

int sceNpRosterAddBlockListEntry(int req_id, const NpId *npid, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (sceKernelCheckThreadStack() < 14320) return (int)0x80550004;
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

int sceNpRosterDeleteBlockListEntry(int req_id, const NpId *npid, const u32 *opt)
{
    NpRosterReq *req;
    int ret;

    if (sceKernelCheckThreadStack() < 14320) return (int)0x80550004;
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
