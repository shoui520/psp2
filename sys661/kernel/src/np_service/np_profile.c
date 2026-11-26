#include "np_service_internal.h"

/* =========================================================================
 * Endpoint Table (25 entries)
 * ========================================================================= */

typedef struct NpEndpointDef {
    int require_arg;
    const char *scheme;
    int method;
    const char *host_prefix;
    const char *path_prefix;
    const char *path_suffix;
} NpEndpointDef;

typedef struct NpRankingHostPair {
    char view_host[128];
    char update_host[128];
} NpRankingHostPair;

typedef struct NpUrlDownloadState {
    u32 phase;
    u32 total_size;
    u32 remaining;
    int last_error;
} NpUrlDownloadState; /* 16 bytes */

typedef struct NpUploaderDlState {
    u32 phase;
    u32 reserved_align;
    u32 total_size_lo;
    u32 total_size_hi;
    u8 reserved_area[64];
    u64 remaining;
    s64 last_error;
} NpUploaderDlState; /* 96 bytes */

static const NpEndpointDef g_np_endpoints[25] = {
    { 0, "https", 1, "",                 "",                                        "" },
    { 0, "https", 1, "getprof.",         "/basic_view/sec/get_self_profile",        "" },
    { 0, "https", 1, "profile.",         "/basic_update/sec/register_self_profile", "" },
    { 1, "http",  1, "getprof.",         "/basic_view/func/get_profile",            "" },
    { 0, "http",  0, "static-resource.", "_searchjid.xml",                          "" },
    { 1, "http",  1, "",                 "/basic_view/func/search_jid",             "" },
    { 0, "http",  0, "static-resource.", "/lobby/",                                 "-lobby.xml" },
    { 1, "http",  0, "static-resource.", "/ranking/",                               "-ranking.xml" },
    { 1, "https", 1, "",                 "/ranking_update/sec/record_score",        "" },
    { 1, "https", 1, "",                 "/ranking_update/sec/record_data",         "" },
    { 1, "http",  1, "",                 "/ranking_view/func/get_game_data",        "" },
    { 1, "http",  1, "",                 "/ranking_view/func/get_player_rank",      "" },
    { 1, "http",  1, "",                 "/ranking_view/func/get_ranking",          "" },
    { 1, "http",  1, "",                 "/ranking_view/func/get_board_info",       "" },
    { 0, "https", 1, "uploader.",        "/uploader/sec/upload_data",               "" },
    { 1, "https", 1, "uploader.",        "/uploader/sec/download_data",             "" },
    { 0, "https", 1, "uploader.",        "/uploader/sec/get_data_list",             "" },
    { 0, "https", 1, "uploader.",        "/uploader/sec/get_quota",                 "" },
    { 0, "https", 1, "uploader.",        "/uploader/sec/delete_data",               "" },
    { 0, "https", 1, "updptl.",          "/basic_update/sec/update_ptl",            "" },
    { 0, "https", 1, "updptl.",          "/basic_update/sec/update_ci",             "" },
    { 0, "http",  0, "static-resource.", "/matching/",                              "-matching.xml" },
    { 0, "http",  1, "getprof.",         "/basic_view/func/get_avatar_category",    "" },
    { 0, "http",  1, "getprof.",         "/basic_view/func/get_avatar_list",        "" },
    { 0, "https", 1, "profile.",         "/basic_update/sec/update_self_profile",   "" },
};

/* =========================================================================
 * Allocator, User List, and Transaction List
 * ========================================================================= */

static int g_httpxml_init = 0;
static int g_mem_init = 0;
static int g_trans_mgr_init = 0;
static int g_trans_count = 0;
static int g_http_template = 0;
static int g_next_trans_id = 0x20000000;
static NpTransCtx g_trans_head;
static u8 g_trans_lwmtx[32];

static int g_user_mgr_init = 0;
static NpUserCtx g_user_head;
static u8 g_user_lwmtx[32];

static int __attribute__((noinline)) np_mem_init(void)
{
    if (g_mem_init == 1)
        return (int)0x80550701;
    g_mem_init = 1;
    return 0;
}

static int __attribute__((noinline)) np_mem_term(void)
{
    if (g_mem_init == 0)
        return (int)0x80550702;
    g_mem_init = 0;
    return 0;
}

void *np_alloc(SceSize size)
{
    return sceNpService_C5094BE6(size);
}

void np_free(void *block)
{
    sceNpService_61AB2062(block);
}

/* --- User context management --- */

static int __attribute__((noinline)) np_user_mgr_init(void)
{
    int ret;
    if (g_user_mgr_init != 0)
        return (int)0x80550701;
    sceKernelMemset(&g_user_head, 0, sizeof(g_user_head));
    g_user_head.user_id = 0x10000000;
    ret = sceKernelCreateLwMutex(g_user_lwmtx, "SceNetHttpXml", 0, 0, NULL);
    if (ret < 0)
        return ret;
    g_user_mgr_init = 1;
    return 0;
}

static int __attribute__((noinline)) np_user_mgr_term(void)
{
    int ret;
    NpUserCtx *cur;

    if (g_user_mgr_init != 1)
        return (int)0x80550702;
    ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
    if (ret < 0)
        return ret;

    cur = g_user_head.next;
    while (cur != NULL) {
        NpUserCtx *next = cur->next;
        NpUserCtx *prev = cur->prev;
        NpAllocNode *alloc_node;
        if (prev != NULL)
            prev->next = next;
        if (next != NULL)
            next->prev = prev;
        alloc_node = cur->alloc_list;
        while (alloc_node != NULL) {
            NpAllocNode *anext = alloc_node->next;
            np_free(alloc_node->block);
            np_free(alloc_node);
            alloc_node = anext;
        }
        sceKernelDeleteLwMutex(cur->lwmutex);
        np_free(cur);
        cur = next;
    }

    sceKernelUnlockLwMutex(g_user_lwmtx, 1);
    sceKernelDeleteLwMutex(g_user_lwmtx);
    g_user_mgr_init = 0;
    return 0;
}

static int __attribute__((noinline)) np_user_acquire(const void *online_id, NpUserCtx **out_user)
{
    int ret;
    NpUserCtx *cur;

    *out_user = NULL;
    if (online_id == NULL)
        return (int)0x80550704;

    ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
    if (ret < 0)
        return ret;

    cur = g_user_head.next;
    while (cur != NULL) {
        if (sceNet_lib_B9085A96((const char *)cur->online_id, (const char *)online_id, 16) == 0 &&
            cur->destroying == 0) {
            cur->ref_count++;
            *out_user = cur;
            ret = 0;
            break;
        }
        cur = cur->next;
    }

    sceKernelUnlockLwMutex(g_user_lwmtx, 1);
    if (cur == NULL)
        return (int)0x8055070F;
    return ret;
}

static inline __attribute__((always_inline)) void np_user_release_inline(NpUserCtx *user)
{
    if (user != NULL && sceKernelLockLwMutex(g_user_lwmtx, 1, NULL) >= 0) {
        user->ref_count--;
        if (user->destroying == 1 && user->ref_count == 0) {
            NpUserCtx *prev = user->prev;
            NpUserCtx *next = user->next;
            NpAllocNode *alloc_node;
            if (prev != NULL)
                prev->next = next;
            if (next != NULL)
                next->prev = prev;
            alloc_node = user->alloc_list;
            while (alloc_node != NULL) {
                NpAllocNode *anext = alloc_node->next;
                np_free(alloc_node->block);
                np_free(alloc_node);
                alloc_node = anext;
            }
            sceKernelDeleteLwMutex(user->lwmutex);
            np_free(user);
        }
        sceKernelUnlockLwMutex(g_user_lwmtx, 1);
    }
}

static void __attribute__((noinline)) np_user_release(NpUserCtx *user)
{
    np_user_release_inline(user);
}

static int __attribute__((noinline)) np_user_track_alloc(NpUserCtx *user, void *block)
{
    NpAllocNode *node;
    int ret;

    if (block == NULL)
        return 0;

    node = (NpAllocNode *)np_alloc(sizeof(NpAllocNode));
    if (node == NULL)
        return (int)0x80550703;

    ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
    if (ret < 0) {
        np_free(node);
        return ret;
    }

    node->block = block;
    node->next = user->alloc_list;
    user->alloc_list = node;
    sceKernelUnlockLwMutex(g_user_lwmtx, 1);
    return 0;
}

static int __attribute__((noinline)) np_user_insert(NpUserCtx *user)
{
    int ret;
    NpUserCtx *cur;
    NpUserCtx *prev;
    int new_id;

    ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
    if (ret < 0)
        return ret;

    cur = g_user_head.next;
    while (cur != NULL) {
        if (sceNet_lib_B9085A96((const char *)cur->online_id, (const char *)user->online_id, 16) == 0 &&
            cur->destroying == 0) {
            sceKernelUnlockLwMutex(g_user_lwmtx, 1);
            return (int)0x80550710;
        }
        cur = cur->next;
    }

    prev = &g_user_head;
    cur = g_user_head.next;
    while (cur != NULL && cur->user_id == prev->user_id + 1) {
        prev = cur;
        cur = cur->next;
    }

    if (prev->user_id > 0x20000000) {
        sceKernelUnlockLwMutex(g_user_lwmtx, 1);
        return (int)0x80550706;
    }

    new_id = prev->user_id + 1;
    user->user_id = new_id;
    user->next = prev->next;
    user->prev = prev;
    if (prev->next != NULL)
        prev->next->prev = user;
    prev->next = user;
    sceKernelUnlockLwMutex(g_user_lwmtx, 1);
    return new_id;
}

static int __attribute__((noinline)) np_user_create(const void *online_id)
{
    NpUserCtx *user = (NpUserCtx *)np_alloc(sizeof(NpUserCtx));
    int ret = (int)0x80550703;
    if (user != NULL) {
        sceKernelMemset(user, 0, sizeof(NpUserCtx));
        sceKernelMemcpy(user->online_id, online_id, 20);
        sceKernelCreateLwMutex(user->lwmutex, "SceNetHttpXml", 0, 0, NULL);
        ret = np_user_insert(user);
        if (ret >= 0)
            return user->user_id;
        np_free(user);
    }
    return ret;
}

static int __attribute__((noinline)) np_user_destroy(const void *online_id)
{
    NpUserCtx *user = NULL;
    int ret;

    ret = np_user_acquire(online_id, &user);
    if (ret >= 0) {
        ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
        if (ret >= 0) {
            user->destroying = 1;
            sceKernelUnlockLwMutex(g_user_lwmtx, 1);
            np_user_release_inline(user);
            return 0;
        }
    }
    np_user_release_inline(user);
    return ret;
}

/* --- Transaction context management --- */

static int __attribute__((noinline)) np_trans_free(NpTransCtx *trans)
{
    NpAllocNode *node = trans->alloc_list;
    trans->alloc_list = NULL;
    while (node != NULL) {
        NpAllocNode *next = node->next;
        np_free(node->block);
        np_free(node);
        node = next;
    }
    sceKernelDeleteLwMutex(trans->lwmutex);
    np_free(trans);
    return 0;
}

static int __attribute__((noinline)) np_trans_insert(NpTransCtx *trans)
{
    int ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
    if (ret >= 0) {
        NpTransCtx *tail = &g_trans_head;
        NpTransCtx *cur = g_trans_head.next;
        while (cur != NULL) {
            tail = cur;
            cur = cur->next;
        }
        g_trans_count++;
        g_next_trans_id++;
        if (tail->trans_id > 0x2FFFFFFF)
            g_next_trans_id = 0x20000001;
        ret = g_next_trans_id;
        trans->next = NULL;
        tail->next = trans;
        trans->trans_id = ret;
        trans->prev = tail;
        sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
    }
    return ret;
}

int np_trans_acquire(int trans_id, NpTransCtx **out_trans)
{
    int ret = (int)0x8055070E;
    NpTransCtx *cur = &g_trans_head;

    *out_trans = NULL;
    if (trans_id == 0)
        return ret;

    ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
    if (ret < 0)
        return ret;

    while (cur != NULL) {
        if (cur->trans_id == trans_id) {
            cur->ref_count++;
            *out_trans = cur;
            ret = 0;
            break;
        }
        if (trans_id < cur->trans_id) {
            cur = NULL;
            break;
        }
        cur = cur->next;
    }

    sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
    if (cur == NULL)
        return (int)0x8055070E;
    return ret;
}

int np_trans_release(NpTransCtx *trans)
{
    int ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
    if (ret >= 0) {
        trans->ref_count--;
        if (trans->destroying == 1 && trans->ref_count == 0) {
            if (trans->req_id > 0)
                sceHttpDeleteRequest(trans->req_id);
            trans->req_id = 0;
            if (trans->conn_id > 0)
                sceHttpDeleteConnection(trans->conn_id);
            trans->conn_id = 0;
            np_user_release(trans->user);
            trans->user = NULL;
            np_trans_free(trans);
            g_trans_count--;
        }
        return sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
    }
    return ret;
}

int np_trans_track_alloc(NpTransCtx *trans, void *block)
{
    NpAllocNode *node;
    int ret;

    if (block == NULL)
        return 0;

    node = (NpAllocNode *)np_alloc(sizeof(NpAllocNode));
    if (node == NULL)
        return (int)0x80550703;

    ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
    if (ret < 0) {
        np_free(node);
        return ret;
    }

    node->block = block;
    node->next = trans->alloc_list;
    trans->alloc_list = node;
    sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
    return 0;
}

static int __attribute__((noinline)) np_trans_create_obj(NpUserCtx *user, int trans_kind, int owner, void *extra_arg)
{
    NpTransCtx *trans = (NpTransCtx *)np_alloc(sizeof(NpTransCtx));
    int ret = (int)0x80550703;
    if (trans != NULL) {
        sceKernelMemset(trans, 0, sizeof(NpTransCtx));
        sceKernelCreateLwMutex(trans->lwmutex, "SceNetHttpXml", 0, 0, NULL);
        trans->user = user;
        trans->kind = trans_kind;
        trans->owner = owner;
        trans->extra = extra_arg;
        if (extra_arg == NULL || (ret = np_trans_track_alloc(trans, extra_arg)) >= 0) {
            ret = np_trans_insert(trans);
            if (ret >= 0)
                return trans->trans_id;
        }
        np_trans_free(trans);
    }
    return ret;
}

static int __attribute__((noinline)) np_trans_destroy_by_owner(int trans_id, int owner)
{
    NpTransCtx *trans = NULL;
    int ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0) {
        if (trans->owner != owner) {
            ret = (int)0x8055070E;
        } else {
            sceNpService_43E635D2(trans_id);
            ret = sceKernelLockLwMutex(trans->lwmutex, 1, NULL);
            if (ret >= 0) {
                ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
                if (ret >= 0) {
                    NpTransCtx *prev = trans->prev;
                    NpTransCtx *next = trans->next;
                    if (prev != NULL)
                        prev->next = next;
                    if (next != NULL)
                        next->prev = prev;
                    trans->destroying = 1;
                    sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
                }
                sceKernelUnlockLwMutex(trans->lwmutex, 1);
                if (ret >= 0) {
                    if (trans != NULL)
                        np_trans_release(trans);
                    return 0;
                }
            }
        }
    }
    if (trans != NULL)
        np_trans_release(trans);
    return ret;
}

static int __attribute__((noinline)) np_trans_mgr_init(void)
{
    int ret;
    int tid;

    if (g_trans_mgr_init != 0)
        return (int)0x80550701;

    sceKernelMemset(&g_trans_head, 0, sizeof(g_trans_head));
    g_trans_head.trans_id = 0x20000000;
    ret = sceKernelCreateLwMutex(g_trans_lwmtx, "SceNetHttpXml", 0, 0, NULL);
    if (ret >= 0) {
        tid = sceHttpCreateTemplate("PS3Community-agent/1.0.0 libhttp/1.0.0", 0, 0);
        ret = tid;
        if (tid >= 0) {
            g_http_template = tid;
            ret = sceHttp_87F1E666(tid, 357);
            if (ret >= 0) {
                ret = sceHttp_3C478044(g_http_template, 26);
                if (ret >= 0) {
                    sceHttpDeleteHeader(g_http_template, "Accept");
                    sceHttpDeleteHeader(g_http_template, "Accept-Encoding");
                    sceHttpDeleteHeader(g_http_template, "Accept-Charset");
                    sceHttpSetConnectTimeOut(g_http_template, 30000000u);
                    sceHttpSetRecvTimeOut(g_http_template, 120000000u);
                    sceHttpSetSendTimeOut(g_http_template, 120000000u);
                    g_trans_mgr_init = 1;
                    return 0;
                }
            }
        }
    }
    if (g_http_template > 0)
        sceHttpDeleteTemplate(g_http_template);
    return ret;
}

static int __attribute__((noinline)) np_trans_mgr_term(void)
{
    NpTransCtx *cur;
    if (g_trans_mgr_init != 1)
        return (int)0x80550702;

    cur = g_trans_head.next;
    while (cur != NULL) {
        NpTransCtx *next = cur->next;
        np_trans_destroy_by_owner(cur->trans_id, 1);
        cur = next;
    }
    sceKernelDeleteLwMutex(g_trans_lwmtx);
    if (g_http_template > 0)
        sceHttpDeleteTemplate(g_http_template);
    g_http_template = 0;
    g_trans_mgr_init = 0;
    return 0;
}

static int __attribute__((noinline)) np_httpxml_init_impl(void)
{
    int mem_ok = 0;
    int usr_ok = 0;
    int ret;

    if (g_httpxml_init == 1)
        return (int)0x80550701;

    ret = np_mem_init();
    if (ret >= 0) {
        mem_ok = 1;
        ret = np_user_mgr_init();
        if (ret >= 0) {
            usr_ok = 1;
            ret = np_trans_mgr_init();
            if (ret >= 0) {
                g_httpxml_init = 1;
                return 0;
            }
        }
    }
    if (usr_ok) np_user_mgr_term();
    if (mem_ok) np_mem_term();
    return ret;
}

static int __attribute__((noinline)) np_httpxml_term_impl(void)
{
    int tr_ok = 0;
    int usr_ok = 0;
    int ret;

    if (g_httpxml_init == 0)
        return (int)0x80550702;

    ret = np_trans_mgr_term();
    if (ret >= 0) {
        tr_ok = 1;
        ret = np_user_mgr_term();
        if (ret >= 0) {
            usr_ok = 1;
            ret = np_mem_term();
            if (ret >= 0) {
                g_httpxml_init = 0;
                return 0;
            }
        }
    }
    if (tr_ok) np_trans_mgr_init();
    if (usr_ok) np_user_mgr_init();
    return ret;
}

int np_httpxml_init(void)
{
    return np_httpxml_init_impl();
}

int np_httpxml_term(void)
{
    return np_httpxml_term_impl();
}

int np_httpxml_is_init(void)
{
    return g_httpxml_init;
}

/* =========================================================================
 * HTTP Basic Auth Callback & Connection/Request Setup
 * ========================================================================= */

static const u8 g_basic_auth_pw_xor[32] = {
    0x35, 0x03, 0x0F, 0x19, 0x40, 0x16, 0x49, 0x04,
    0x1C, 0x35, 0x03, 0x1E, 0x21, 0x48, 0x2D, 0x4E,
    0x07, 0x1C, 0x5A, 0x36, 0x0E, 0x3F, 0x0C, 0x18,
    0x49, 0x15, 0x4E, 0x21, 0x14, 0x36, 0x1D, 0x16,
};

static const u8 g_ranking_auth_pw_xor[32] = {
    0x21, 0x2D, 0x18, 0x1B, 0x1D, 0x0E, 0x2A, 0x23,
    0x04, 0x4C, 0x4B, 0x19, 0x4F, 0x25, 0x26, 0x3F,
    0x4B, 0x4D, 0x4C, 0x44, 0x58, 0x3C, 0x31, 0x4C,
    0x15, 0x4C, 0x5C, 0x41, 0x32, 0x38, 0x1E, 0x08,
};

static int np_http_auth_cb(int req_id, int auth_kind, const char *realm,
                           char *user, char *pass, int need_entity,
                           u8 **entity_body, u32 *entity_size,
                           int *save, int auth_realm_kind)
{
    int i;

    (void)req_id; (void)need_entity; (void)entity_body; (void)entity_size;

    if (save != NULL)
        *save = 0;
    if (auth_kind != 1 || realm == NULL)
        return -1;

    if (auth_realm_kind == 1) {
        int rlen = sceNet_lib_D8722983("c7y-ranking");
        if (sceNet_lib_94DCA9F0(realm, "c7y-ranking", rlen + 1) != 0 || user == NULL)
            return -1;
        sceNet_lib_B5CE388A(user, "c7y-ranking01", sceNet_lib_D8722983("c7y-ranking01") + 1);
        if (pass == NULL)
            return -1;
        for (i = 0; i < 32; i++)
            pass[i] = (char)(g_ranking_auth_pw_xor[i] ^ 0x7Fu);
        pass[32] = '\0';
        return 0;
    }

    if (auth_realm_kind == 2) {
        int rlen = sceNet_lib_D8722983("c7y-basic");
        if (sceNet_lib_94DCA9F0(realm, "c7y-basic", rlen + 1) != 0 || user == NULL)
            return -1;
        sceNet_lib_B5CE388A(user, "c7y-basic02", sceNet_lib_D8722983("c7y-basic02") + 1);
        if (pass == NULL)
            return -1;
        for (i = 0; i < 32; i++)
            pass[i] = (char)(g_basic_auth_pw_xor[i] ^ 0x7Fu);
        pass[32] = '\0';
        return 0;
    }

    return -1;
}

static int __attribute__((noinline)) np_http_open_conn(NpTransCtx *trans, const char *host, const char *scheme, u16 port)
{
    int cid = sceHttpCreateConnection(g_http_template, host, scheme, port, 0);
    if (cid >= 0) {
        int realm_kind = 2;
        trans->conn_id = cid;
        if (sceNet_lib_B9085A96(host, "getprof", sceNet_lib_D8722983("getprof")) != 0 &&
            sceNet_lib_B9085A96(host, "searchjid", sceNet_lib_D8722983("searchjid")) != 0) {
            realm_kind = 1;
        }
        sceHttpSetAuthInfoCallback(trans->conn_id, np_http_auth_cb, (void *)realm_kind);
        return 0;
    }
    return cid;
}

static int __attribute__((noinline)) np_http_open_url(NpTransCtx *trans, const char *url)
{
    int cid = sceHttpCreateConnectionWithURL(g_http_template, url, 0);
    if (cid >= 0) {
        trans->conn_id = cid;
        return 0;
    }
    return cid;
}

static int __attribute__((noinline)) np_http_create_req(NpTransCtx *trans, int method, const char *path, u64 content_len)
{
    int rid = sceHttpCreateRequest(trans->conn_id, method, path, content_len);
    if (rid >= 0) {
        trans->req_id = rid;
        return 0;
    }
    return rid;
}

static inline __attribute__((always_inline)) int np_http_send_locked(NpTransCtx *trans, const void *body, u32 body_len)
{
    int ret = sceKernelLockLwMutex(trans->lwmutex, 1, NULL);
    if (ret >= 0) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        else
            trans->sending = 1;
        sceKernelUnlockLwMutex(trans->lwmutex, 1);
        if (ret >= 0) {
            ret = sceHttpSendRequest(trans->req_id, body, body_len);
            trans->sending = 0;
            if (ret < 0)
                ret = np_map_ssl_error(trans->req_id, ret);
        }
    }
    return ret;
}

static int __attribute__((noinline)) np_http_setup_endpoint(NpTransCtx *trans, u32 ep_index, const void *ep_arg, u32 content_len)
{
    const NpEndpointDef *ep = &g_np_endpoints[ep_index];
    NpUserCtx *user = trans->user;
    const char *env = user->env;
    const char *country = user->country;
    char *url_buf;
    int ret = 0;
    u16 port;
    const char *path = NULL;

    url_buf = (char *)np_alloc(256);
    if (url_buf == NULL)
        return (int)0x80550703;
    sceKernelMemset(url_buf, 0, 256);

    if (ep->require_arg == 1 && ep_arg == NULL) {
        ret = (int)0x8055070C;
    } else {
        switch (ep_index) {
        case 3:
        case 15: {
            u32 total = (u32)(sceNet_lib_D8722983(ep->host_prefix) +
                              sceNet_lib_D8722983(env) +
                              sceNet_lib_D8722983(".community.playstation.net") + 1);
            if (ep_arg != NULL)
                total += (u32)sceNet_lib_D8722983((const char *)ep_arg) + 1;
            if (total >= 257u)
                ret = (int)0x80550781;
            else
                sceNet_lib_CF705E46(url_buf, "%s%s.%s%s", ep->host_prefix, (const char *)ep_arg, env, ".community.playstation.net");
            break;
        }
        case 4:
        case 6:
        case 7:
        case 21: {
            u32 total = (u32)(sceNet_lib_D8722983(ep->host_prefix) +
                              sceNet_lib_D8722983(env) +
                              sceNet_lib_D8722983(".community.playstation.net") + 1);
            if (total >= 257u)
                ret = (int)0x80550781;
            else
                sceNet_lib_CF705E46(url_buf, "%s%s%s", ep->host_prefix, env, ".community.playstation.net");
            break;
        }
        case 5:
        case 10:
        case 11:
        case 12:
        case 13:
            sceNet_lib_B5CE388A(url_buf, (const char *)ep_arg, 255);
            break;
        case 8:
        case 9:
            sceNet_lib_B5CE388A(url_buf, ((const NpRankingHostPair *)ep_arg)->update_host, 255);
            break;
        default: {
            u32 total = (u32)(sceNet_lib_D8722983(ep->host_prefix) +
                              sceNet_lib_D8722983(env) +
                              sceNet_lib_D8722983(".community.playstation.net") + 1);
            if (country != NULL)
                total += (u32)sceNet_lib_D8722983(country) + 1;
            if (total >= 257u)
                ret = (int)0x80550781;
            else
                sceNet_lib_CF705E46(url_buf, "%s%s.%s%s", ep->host_prefix, country, env, ".community.playstation.net");
            break;
        }
        }
    }

    if (ret >= 0) {
        port = (sceNet_lib_A0F16ABD(ep->scheme, "http") == 0) ? 80 : 443;
        ret = np_http_open_conn(trans, url_buf, ep->scheme, port);
    }

    if (ret >= 0) {
        if (ep_index == 4) {
            u32 plen = (u32)(sceNet_lib_D8722983("/np/resource/basic/") +
                             sceNet_lib_D8722983(country) +
                             sceNet_lib_D8722983(ep->path_prefix) + 1);
            if (plen >= 257u) {
                ret = (int)0x80550781;
            } else {
                sceNet_lib_CF705E46(url_buf, "%s%s%s", "/np/resource/basic/", country, ep->path_prefix);
                path = url_buf;
            }
        } else if (ep_index == 6 || ep_index == 7 || ep_index == 21) {
            char title_str[16];
            sceKernelMemset(title_str, 0, 13);
            ret = sceNp_E90DFBC4(ep_arg, title_str, 13);
            if (ret >= 0) {
                u32 plen = (u32)(sceNet_lib_D8722983("/np/resource/psp-title/") +
                                 2 * sceNet_lib_D8722983(title_str) +
                                 sceNet_lib_D8722983(ep->path_prefix) +
                                 sceNet_lib_D8722983(ep->path_suffix) + 1);
                if (plen >= 257u) {
                    ret = (int)0x80550781;
                } else {
                    sceNet_lib_CF705E46(url_buf, "%s%s%s%s%s", "/np/resource/psp-title/", title_str,
                                        ep->path_prefix, title_str, ep->path_suffix);
                    path = url_buf;
                }
            }
        } else {
            path = ep->path_prefix;
        }
    }

    if (ret >= 0)
        ret = np_http_create_req(trans, ep->method, path, (u64)content_len);

    np_free(url_buf);
    return ret;
}

static int __attribute__((noinline)) np_http_check_resp(NpTransCtx *trans, int sig_kind, u64 *extra_len_out,
                                                        u32 *xml_len_out, void *unused, u8 *sig_out)
{
    int status = 0;
    u64 clen = 0;
    u32 hdr_info_len = 0;
    int ret;

    (void)unused;

    ret = np_http_parse_headers(trans, &status, &clen, &hdr_info_len, sig_out, sig_kind);
    if (ret < 0 || clen == 0) {
        if (ret >= 0)
            ret = (int)0x80550708;
        np_trans_close_http(trans);
        return ret;
    }

    if (hdr_info_len != 0) {
        if (clen < (u64)hdr_info_len) {
            np_trans_close_http(trans);
            return (int)0x80550708;
        }
        if (extra_len_out != NULL)
            *extra_len_out = clen - (u64)hdr_info_len;
    } else {
        if (clen > 0x10000ULL) {
            np_trans_close_http(trans);
            return (int)0x80550709;
        }
        hdr_info_len = (u32)clen;
        if (extra_len_out != NULL)
            *extra_len_out = 0;
    }

    if (hdr_info_len > 0x10000u) {
        np_trans_close_http(trans);
        return (int)0x80550709;
    }
    *xml_len_out = hdr_info_len;
    return 0;
}

static int __attribute__((noinline)) np_http_expect_bytes(NpTransCtx *trans, const void *expected, u32 len)
{
    void *recv_buf = np_alloc(len);
    u32 got = 0;
    int ret = (int)0x80550703;

    if (recv_buf != NULL) {
        if (trans->aborted == 1) {
            ret = (int)0x80550707;
        } else {
            ret = np_http_read_exact(trans, recv_buf, len, &got);
            if (ret >= 0) {
                if (got != len || sceNet_lib_E0A81C7C(recv_buf, expected, (int)len) != 0)
                    ret = (int)0x80550708;
            }
        }
        np_free(recv_buf);
    }
    return ret;
}

/* =========================================================================
 * XML Request Builders
 * ========================================================================= */

static inline __attribute__((always_inline)) const char *np_avatar_size_str(int size, int check_sdk)
{
    if (check_sdk && sceKernelGetCompiledSdkVersion() <= 0x04010010u)
        size = 0;
    if (size == 0) return "l";
    if (size == 1) return "m";
    if (size == 2) return "s";
    return NULL;
}

static int __attribute__((noinline)) np_build_uploader_dl_xml(char **out_xml, const u8 *ticket, u32 ticket_len,
                                                              const NpId *npid, const char *ref)
{
    u32 encoded_size = 4u * ((ticket_len + 2u) / 3u) + 1u;
    char *encoded_ticket = NULL;
    char *xml = NULL;
    char *jid = NULL;
    char ver[16];
    int ret;
    char *cursor;

    encoded_ticket = (char *)np_alloc(encoded_size);
    if (encoded_ticket == NULL)
        return (int)0x80550703;
    sceKernelMemset(encoded_ticket, 0, encoded_size);
    ret = np_base64_encode(encoded_ticket, ticket, ticket_len);
    if (ret >= 0) {
        xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(encoded_ticket) + 256));
        if (xml == NULL) {
            ret = (int)0x80550703;
        } else {
            cursor = xml;
            cursor += sceNet_lib_CF705E46(cursor, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
            sceKernelMemset(ver, 0, 8);
            sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
            cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" %s=\"%s\">", "uploader", "uploader", "psp", "sv", ver);
            cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "ticket", encoded_ticket, "ticket");

            ret = np_npid_to_jid_str(&jid, npid);
            if (ret >= 0) {
                cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "ticketjid", jid, "ticketjid");
                np_free(jid);
                jid = NULL;
                cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "reference", ref, "reference");
                cursor += sceNet_lib_CF705E46(cursor, "</%s>", "uploader");
                *out_xml = xml;
                np_free(encoded_ticket);
                return (int)(cursor - xml);
            }
        }
    }

    if (jid != NULL) np_free(jid);
    if (encoded_ticket != NULL) np_free(encoded_ticket);
    if (xml != NULL) np_free(xml);
    return ret;
}

static int __attribute__((noinline)) np_build_self_profile_xml(char **out_xml, const u8 *ticket, u32 ticket_len,
                                                               const char *env, int avatar_size)
{
    const char *sz = np_avatar_size_str(avatar_size, 1);
    u32 encoded_size;
    char *encoded_ticket = NULL;
    char *xml = NULL;
    char ver[16];
    int ret;
    char *cursor;

    if (sz == NULL)
        return (int)0x80550704;

    encoded_size = 4u * ((ticket_len + 2u) / 3u) + 1u;
    encoded_ticket = (char *)np_alloc(encoded_size);
    if (encoded_ticket == NULL)
        return (int)0x80550703;
    sceKernelMemset(encoded_ticket, 0, encoded_size);
    ret = np_base64_encode(encoded_ticket, ticket, ticket_len);
    if (ret >= 0) {
        xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(encoded_ticket) + 256));
        if (xml == NULL) {
            ret = (int)0x80550703;
        } else {
            cursor = xml;
            cursor += sceNet_lib_CF705E46(cursor, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
            sceKernelMemset(ver, 0, 8);
            sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
            cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
            cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "ticket", encoded_ticket, "ticket");
            cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "env", env, "env");
            cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" />", "avatar", "size", sz);
            cursor += sceNet_lib_CF705E46(cursor, "</%s>", "profile");
            *out_xml = xml;
            np_free(encoded_ticket);
            return (int)(cursor - xml);
        }
    }

    if (encoded_ticket != NULL) np_free(encoded_ticket);
    if (xml != NULL) np_free(xml);
    return ret;
}

static int __attribute__((noinline)) np_build_update_profile_xml(char **out_xml, const u8 *ticket, u32 ticket_len,
                                                                 const char *env, const char *online_name, const int *langs,
                                                                 const char *about_me, const int *avatar_id, int avatar_size)
{
    const char *sz = np_avatar_size_str(avatar_size, 1);
    u32 encoded_size;
    char *encoded_ticket = NULL;
    char *esc_name = NULL;
    char *esc_about = NULL;
    char *xml = NULL;
    char ver[16];
    int total_len;
    int ret;
    char *cursor;

    if (sz == NULL)
        return (int)0x80550704;

    encoded_size = 4u * ((ticket_len + 2u) / 3u) + 1u;
    encoded_ticket = (char *)np_alloc(encoded_size);
    if (encoded_ticket == NULL)
        return (int)0x80550703;
    sceKernelMemset(encoded_ticket, 0, encoded_size);
    ret = np_base64_encode(encoded_ticket, ticket, ticket_len);
    if (ret >= 0) {
        total_len = sceNet_lib_D8722983(encoded_ticket) + 256;
        if (online_name != NULL) {
            ret = np_xml_escape_str(&esc_name, online_name);
            if (ret >= 0)
                total_len += sceNet_lib_D8722983(esc_name);
        }
        if (ret >= 0 && about_me != NULL) {
            ret = np_xml_escape_str(&esc_about, about_me);
            if (ret >= 0)
                total_len += sceNet_lib_D8722983(esc_about);
        }
        if (ret >= 0) {
            xml = (char *)np_alloc((SceSize)total_len);
            if (xml == NULL) {
                ret = (int)0x80550703;
            } else {
                cursor = xml;
                cursor += sceNet_lib_CF705E46(cursor, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
                sceKernelMemset(ver, 0, 8);
                sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
                cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
                cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "ticket", encoded_ticket, "ticket");
                cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "env", env, "env");
                if (esc_name != NULL)
                    cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "onlinename", esc_name, "onlinename");
                if (langs != NULL) {
                    int lang_first = langs[0];
                    int lang_second = langs[1];
                    int lang_third = langs[2];
                    if (lang_second >= 0 && lang_first < 0) {
                        lang_first = lang_second;
                        lang_second = -1;
                    }
                    if (lang_third >= 0) {
                        if (lang_first < 0) {
                            lang_first = lang_third;
                            lang_third = -1;
                        } else if (lang_second < 0) {
                            lang_second = lang_third;
                            lang_third = -1;
                        }
                    }
                    if (lang_first < 0) {
                        ret = (int)0x80550704;
                    } else {
                        cursor += sceNet_lib_CF705E46(cursor, "<%s>%d</%s>", "language1", lang_first, "language1");
                        if (lang_second < 0)
                            cursor += sceNet_lib_CF705E46(cursor, "<%s />", "language2");
                        else
                            cursor += sceNet_lib_CF705E46(cursor, "<%s>%d</%s>", "language2", lang_second, "language2");
                        if (lang_third < 0)
                            cursor += sceNet_lib_CF705E46(cursor, "<%s />", "language3");
                        else
                            cursor += sceNet_lib_CF705E46(cursor, "<%s>%d</%s>", "language3", lang_third, "language3");
                    }
                }
                if (ret >= 0) {
                    if (esc_about != NULL)
                        cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "aboutme", esc_about, "aboutme");
                    if (avatar_id != NULL)
                        cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%d\" %s=\"%s\"/>", "avatar", "id", *avatar_id, "size", sz);
                    else
                        cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\"/>", "avatar", "size", sz);
                    cursor += sceNet_lib_CF705E46(cursor, "</%s>", "profile");
                    *out_xml = xml;
                    ret = (int)(cursor - xml);
                    xml = NULL;
                }
            }
        }
    }

    if (encoded_ticket != NULL) np_free(encoded_ticket);
    if (esc_name != NULL) np_free(esc_name);
    if (esc_about != NULL) np_free(esc_about);
    if (xml != NULL) np_free(xml);
    return ret;
}

static int __attribute__((noinline)) np_build_get_profile_xml(char **out_xml, const NpId *npid, int avatar_size)
{
    char *jid = NULL;
    char *xml = NULL;
    const char *sz;
    char ver[16];
    int ret;
    char *cursor;

    ret = np_npid_to_jid_str(&jid, npid);
    if (ret >= 0) {
        if (jid == NULL) {
            ret = (int)0x80550703;
        } else {
            sz = np_avatar_size_str(avatar_size, 1);
            if (sz == NULL) {
                ret = (int)0x80550704;
            } else {
                xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(jid) + 256));
                if (xml == NULL) {
                    ret = (int)0x80550703;
                } else {
                    cursor = xml;
                    cursor += sceNet_lib_CF705E46(cursor, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
                    sceKernelMemset(ver, 0, 8);
                    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
                    cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
                    cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "jid", jid, "jid");
                    cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" />", "avatar", "size", sz);
                    cursor += sceNet_lib_CF705E46(cursor, "</%s>", "profile");
                    *out_xml = xml;
                    ret = (int)(cursor - xml);
                    xml = NULL;
                }
            }
        }
    }

    if (jid != NULL) np_free(jid);
    if (xml != NULL) np_free(xml);
    return ret;
}

static int __attribute__((noinline)) np_build_searchjid_xml(char **out_xml, const char *online_id)
{
    char *xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(online_id) + 256));
    char ver[16];
    char *cursor;

    if (xml == NULL)
        return (int)0x80550703;
    cursor = xml;
    cursor += sceNet_lib_CF705E46(cursor, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" %s=\"%s\">", "searchjid", "platform", "psp", "sv", ver);
    cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "online-id", online_id, "online-id");
    cursor += sceNet_lib_CF705E46(cursor, "</%s>", "searchjid");
    *out_xml = xml;
    return (int)(cursor - xml);
}

static int __attribute__((noinline)) np_build_avatar_cat_xml(char **out_xml, int lang, int start, int max_num)
{
    char *xml = (char *)np_alloc(256);
    char ver[16];
    char *cursor;

    if (xml == NULL)
        return (int)0x80550703;
    cursor = xml;
    cursor += sceNet_lib_CF705E46(cursor, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
    cursor += sceNet_lib_CF705E46(cursor, "<%s>%d</%s>", "start", start, "start");
    cursor += sceNet_lib_CF705E46(cursor, "<%s>%d</%s>", "max", max_num, "max");
    cursor += sceNet_lib_CF705E46(cursor, "<%s>%d</%s>", "lang", lang, "lang");
    cursor += sceNet_lib_CF705E46(cursor, "</%s>", "profile");
    *out_xml = xml;
    return (int)(cursor - xml);
}

static int __attribute__((noinline)) np_build_avatar_list_xml(char **out_xml, const int *cat_id, int size, int start, int max_num)
{
    const char *sz = np_avatar_size_str(size, 0);
    char *xml;
    char ver[16];
    char *cursor;

    if (sz == NULL)
        return (int)0x80550704;
    xml = (char *)np_alloc(256);
    if (xml == NULL)
        return (int)0x80550703;

    cursor = xml;
    cursor += sceNet_lib_CF705E46(cursor, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
    cursor += sceNet_lib_CF705E46(cursor, "<%s>%d</%s>", "start", start, "start");
    cursor += sceNet_lib_CF705E46(cursor, "<%s>%d</%s>", "max", max_num, "max");
    if (cat_id != NULL)
        cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" %s=\"%d\" />", "avatar", "size", sz, "category", *cat_id);
    else
        cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" />", "avatar", "size", sz);
    cursor += sceNet_lib_CF705E46(cursor, "</%s>", "profile");
    *out_xml = xml;
    return (int)(cursor - xml);
}

static int __attribute__((noinline)) np_build_uploader_list_xml(char **out_xml, const u8 *ticket, u32 ticket_len,
                                                                const NpId *npid, int quota, u32 start, u32 max_num)
{
    u32 encoded_size = 4u * ((ticket_len + 2u) / 3u) + 1u;
    char *encoded_ticket = NULL;
    char *xml = NULL;
    char *jid = NULL;
    const char *qstr = NULL;
    char ver[16];
    int ret;
    char *cursor;

    encoded_ticket = (char *)np_alloc(encoded_size);
    if (encoded_ticket == NULL)
        return (int)0x80550703;
    sceKernelMemset(encoded_ticket, 0, encoded_size);
    ret = np_base64_encode(encoded_ticket, ticket, ticket_len);
    if (ret >= 0) {
        xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(encoded_ticket) + 384));
        if (xml == NULL) {
            ret = (int)0x80550703;
        } else {
            cursor = xml;
            cursor += sceNet_lib_CF705E46(cursor, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
            sceKernelMemset(ver, 0, 8);
            sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
            cursor += sceNet_lib_CF705E46(cursor, "<%s %s=\"%s\" %s=\"%s\">", "uploader", "uploader", "psp", "sv", ver);
            cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "ticket", encoded_ticket, "ticket");

            ret = np_npid_to_jid_str(&jid, npid);
            if (ret >= 0) {
                cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "ticketjid", jid, "ticketjid");
                np_free(jid);
                jid = NULL;

                if (quota == 1)      qstr = "sys";
                else if (quota == 2) qstr = "game";
                else if (quota == 3) qstr = "premo";
                else                 ret = (int)0x80550713;

                if (ret >= 0) {
                    cursor += sceNet_lib_CF705E46(cursor, "<%s>%s</%s>", "quota", qstr, "quota");
                    cursor += sceNet_lib_CF705E46(cursor, "<%s>%u</%s>", "start", start, "start");
                    cursor += sceNet_lib_CF705E46(cursor, "<%s>%u</%s>", "list-max", max_num, "list-max");
                    cursor += sceNet_lib_CF705E46(cursor, "</%s>", "uploader");
                    *out_xml = xml;
                    ret = (int)(cursor - xml);
                    xml = NULL;
                }
            }
        }
    }

    if (jid != NULL) np_free(jid);
    if (encoded_ticket != NULL) np_free(encoded_ticket);
    if (xml != NULL) np_free(xml);
    return ret;
}

/* =========================================================================
 * Streaming XML Response Readers
 * ========================================================================= */

static inline __attribute__((always_inline)) int np_read_and_parse_xml(NpTransCtx *trans, void *parser, void *chunk_buf, u32 total_len)
{
    int ret = 0;
    u32 got = 0;

    while (total_len != 0) {
        u32 chunk = (total_len > 256u) ? 256u : total_len;
        ret = np_http_read_exact(trans, chunk_buf, chunk, &got);
        if (ret < 0)
            break;
        if (got == 0) {
            ret = (int)0x80550708;
            break;
        }
        ret = np_xml_feed(parser, chunk_buf, got);
        if (ret < 0)
            break;
        total_len -= got;
    }
    return ret;
}

static int __attribute__((noinline)) np_parse_searchjid_resp(NpTransCtx *trans, NpId *npid_out, u32 total_len)
{
    NpXmlSearchJidCtx ctx;
    u8 parser[848];
    void *chunk_buf;
    int ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.npid_out = npid_out;
    ret = np_xml_searchjid_init(parser, &ctx);
    if (ret < 0)
        return ret;
    chunk_buf = np_alloc(256);
    if (chunk_buf == NULL)
        return (int)0x80550703;
    ret = np_read_and_parse_xml(trans, parser, chunk_buf, total_len);
    if (ret >= 0)
        ret = np_xml_searchjid_check(&ctx);
    np_free(chunk_buf);
    return ret;
}

static int __attribute__((noinline)) np_parse_profile_resp(NpTransCtx *trans, NpProfileRecord *prof_out,
                                                           int *ptlp_out, int *upd_out, u64 *date_out, u32 total_len)
{
    NpXmlProfileCtx ctx;
    u8 parser[848];
    void *chunk_buf;
    int parse_kind;
    int ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.profile = prof_out;
    if (trans->kind == 2)
        parse_kind = 2;
    else if (trans->kind == 1 || trans->kind == 25)
        parse_kind = 1;
    else
        return (int)0x80550711;

    ret = np_xml_profile_init(parser, parse_kind, &ctx);
    if (ret < 0)
        return ret;
    chunk_buf = np_alloc(256);
    if (chunk_buf == NULL)
        return (int)0x80550703;
    ret = np_read_and_parse_xml(trans, parser, chunk_buf, total_len);
    if (ret >= 0) {
        ret = np_xml_profile_check(&ctx);
        if (ret >= 0) {
            if (ptlp_out != NULL) *ptlp_out = ctx.ptlp;
            if (upd_out != NULL)  *upd_out  = ctx.update_flag;
            if (date_out != NULL) *date_out = ctx.updated_date;
        }
    }
    np_free(chunk_buf);
    return ret;
}

static int __attribute__((noinline)) np_parse_avatar_cat_resp(NpTransCtx *trans, int *total_out,
                                                              NpAvatarCategory *entries, int max_num, u32 total_len)
{
    NpXmlAvatarCatCtx ctx;
    u8 parser[848];
    void *chunk_buf;
    int ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.entries = entries;
    ctx.max_count = (u32)max_num;
    ret = np_xml_avatar_cat_init(parser, &ctx);
    if (ret < 0)
        return ret;
    chunk_buf = np_alloc(256);
    if (chunk_buf == NULL)
        return (int)0x80550703;
    ret = np_read_and_parse_xml(trans, parser, chunk_buf, total_len);
    if (ret >= 0) {
        ret = np_xml_avatar_cat_check(&ctx);
        if (ret >= 0) {
            if (total_out != NULL)
                *total_out = (int)ctx.total_count;
            ret = (int)ctx.parsed_count;
        }
    }
    np_free(chunk_buf);
    return ret;
}

static int __attribute__((noinline)) np_parse_avatar_list_resp(NpTransCtx *trans, int *total_out,
                                                               int *ids, char (*urls)[128], int max_num, u32 total_len)
{
    NpXmlAvatarListCtx ctx;
    u8 parser[848];
    void *chunk_buf;
    int ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.urls = urls;
    ctx.avatar_ids = ids;
    ctx.max_count = (u32)max_num;
    ret = np_xml_avatar_list_init(parser, &ctx);
    if (ret < 0)
        return ret;
    chunk_buf = np_alloc(256);
    if (chunk_buf == NULL)
        return (int)0x80550703;
    ret = np_read_and_parse_xml(trans, parser, chunk_buf, total_len);
    if (ret >= 0) {
        ret = np_xml_avatar_list_check(&ctx);
        if (ret >= 0) {
            if (total_out != NULL)
                *total_out = (int)ctx.total_count;
            ret = (int)ctx.parsed_count;
        }
    }
    np_free(chunk_buf);
    return ret;
}

static int __attribute__((noinline)) np_parse_uploader_list_resp(NpTransCtx *trans, NpUploaderEntry **list_out, u32 total_len)
{
    NpXmlUploaderListCtx ctx;
    u8 parser[848];
    void *chunk_buf;
    int ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.list_head = list_out;
    ret = np_xml_uploader_list_init(parser, &ctx);
    if (ret < 0)
        return ret;
    chunk_buf = np_alloc(256);
    if (chunk_buf == NULL)
        return (int)0x80550703;
    ret = np_read_and_parse_xml(trans, parser, chunk_buf, total_len);
    if (ret >= 0)
        ret = np_xml_uploader_list_check(&ctx);
    np_free(chunk_buf);
    return ret;
}

static int __attribute__((noinline)) np_parse_uploader_dl_resp(NpTransCtx *trans, NpUploaderDlInfo *dl_info_out, u32 total_len)
{
    NpXmlUploaderDlCtx ctx;
    u8 parser[848];
    void *chunk_buf;
    int ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.dl_info = dl_info_out;
    ret = np_xml_uploader_dl_init(parser, &ctx);
    if (ret < 0)
        return ret;
    chunk_buf = np_alloc(256);
    if (chunk_buf == NULL)
        return (int)0x80550703;
    ret = np_read_and_parse_xml(trans, parser, chunk_buf, total_len);
    if (ret >= 0)
        ret = np_xml_uploader_dl_check(&ctx);
    np_free(chunk_buf);
    return ret;
}

static int __attribute__((noinline)) np_parse_searchjid_fqdn_resp(NpTransCtx *trans, NpFqdnEntry **list_out, u32 total_len)
{
    NpXmlFqdnCtx ctx;
    u8 parser[848];
    void *chunk_buf;
    int ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.list_head = list_out;
    ret = np_xml_fqdn_init(parser, 1, NULL, &ctx);
    if (ret < 0)
        return ret;
    chunk_buf = np_alloc(256);
    if (chunk_buf == NULL)
        return (int)0x80550703;
    ret = np_read_and_parse_xml(trans, parser, chunk_buf, total_len);
    if (ret >= 0)
        ret = np_xml_fqdn_check(&ctx);
    np_free(chunk_buf);
    return ret;
}

static int __attribute__((noinline)) np_fetch_searchjid_fqdn(NpTransCtx *trans)
{
    NpFqdnEntry *list = NULL;
    u32 xml_len = 0;
    int ret;

    ret = np_http_setup_endpoint(trans, 4, NULL, 0);
    if (ret >= 0)
        ret = np_http_send_locked(trans, NULL, 0);
    if (ret >= 0)
        ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret >= 0)
        ret = np_parse_searchjid_fqdn_resp(trans, &list, xml_len);
    if (ret >= 0) {
        NpFqdnEntry *cur = list;
        np_trans_close_http(trans);
        while (cur != NULL) {
            ret = np_user_track_alloc(trans->user, cur);
            if (ret < 0)
                break;
            cur = cur->next;
        }
    }
    if (ret >= 0) {
        NpUserCtx *user = trans->user;
        ret = sceKernelLockLwMutex(user->lwmutex, 1, NULL);
        if (ret >= 0) {
            if (user->searchjid_fqdn == NULL)
                user->searchjid_fqdn = list;
            sceKernelUnlockLwMutex(user->lwmutex, 1);
            return 0;
        }
    }

    np_free_fqdn_list(list);
    return ret;
}

/* =========================================================================
 * Transaction Creation & Exported Service Functions
 * ========================================================================= */

static int __attribute__((noinline)) np_create_trans_by_type(NpUserCtx *user, int trans_kind, int owner, const void *extra_arg)
{
    void *copy = NULL;
    int ret;

    switch (trans_kind) {
    case 1:
        if (sceNet_lib_D8722983(user->country) == 0)
            return (int)0x80550705;
        /* fallthrough */
    case 2:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
        break;
    case 4: {
        int len;
        if (extra_arg == NULL)
            return (int)0x8055070C;
        len = sceNet_lib_D8722983((const char *)extra_arg) + 1;
        copy = np_alloc((SceSize)len);
        if (copy == NULL)
            return (int)0x80550703;
        sceKernelMemcpy(copy, extra_arg, (SceSize)len);
        break;
    }
    case 20:
    case 26:
        if (extra_arg == NULL)
            return (int)0x8055070C;
        copy = np_alloc(12);
        if (copy == NULL)
            return (int)0x80550703;
        sceKernelMemcpy(copy, extra_arg, 12);
        break;
    default:
        return (int)0x8055070D;
    }

    ret = np_trans_create_obj(user, trans_kind, owner, copy);
    if (ret < 0 && copy != NULL)
        np_free(copy);
    return ret;
}

int sceNpService_7EF4312E(const void *online_id, int trans_kind, const void *extra_arg)
{
    NpUserCtx *user = NULL;
    int ret = (int)0x80550702;

    if (np_httpxml_is_init() == 1) {
        ret = np_user_acquire(online_id, &user);
        if (ret >= 0) {
            ret = np_create_trans_by_type(user, trans_kind, 1, extra_arg);
            if (ret >= 0)
                return ret;
        }
    }
    if (user != NULL)
        np_user_release(user);
    return ret;
}

int np_trans_create_lookup(const void *online_id, int trans_kind, const void *extra_arg)
{
    NpUserCtx *user = NULL;
    int ret = (int)0x80550702;

    if (np_httpxml_is_init() == 1) {
        ret = np_user_acquire(online_id, &user);
        if (ret >= 0) {
            ret = np_create_trans_by_type(user, trans_kind, 2, extra_arg);
            if (ret >= 0)
                return ret;
        }
    }
    if (user != NULL)
        np_user_release(user);
    return ret;
}

int np_user_set_env_country(const void *online_id, const char *env, const char *country)
{
    NpUserCtx *user = NULL;
    int ret = (int)0x80550702;

    if (np_httpxml_is_init() == 1) {
        ret = (int)0x80550704;
        if (env != NULL && country != NULL) {
            ret = np_user_acquire(online_id, &user);
            if (ret >= 0) {
                sceNet_lib_B5CE388A(user->env, env, 14);
                sceNet_lib_B5CE388A(user->country, country, 2);
                ret = 0;
            }
        }
    }
    if (user != NULL)
        np_user_release(user);
    return ret;
}

int sceNpService_76867C01(int trans_id, NpFqdnEntry **list_out)
{
    NpTransCtx *trans = NULL;
    void *body = NULL;
    u32 xml_len = 0;
    u32 got = 0;
    u8 sig[20];
    NpXmlFqdnCtx ctx;
    u8 parser[848];
    int ret;

    *list_out = NULL;
    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0) {
        if (trans->kind != 20)
            ret = (int)0x80550711;
    }
    if (ret >= 0)
        ret = np_http_setup_endpoint(trans, 21, trans->extra, 0);
    if (ret >= 0)
        ret = np_http_send_locked(trans, NULL, 0);
    if (ret >= 0)
        ret = np_http_check_resp(trans, 3, NULL, &xml_len, NULL, sig);
    if (ret >= 0) {
        body = np_alloc(xml_len);
        if (body == NULL)
            ret = (int)0x80550703;
    }
    if (ret >= 0) {
        ret = np_http_read_exact(trans, body, xml_len, &got);
        if (ret >= 0 && got != xml_len)
            ret = (int)0x80550708;
    }
    if (ret >= 0)
        ret = np_verify_hmac_sig(body, (int)got, sig, 3);
    if (ret >= 0) {
        np_trans_close_http(trans);
        sceKernelMemset(&ctx, 0, sizeof(ctx));
        ctx.list_head = list_out;
        ret = np_xml_fqdn_init(parser, 2, trans->extra, &ctx);
        if (ret >= 0) {
            ret = np_xml_feed(parser, body, got);
            if (ret >= 0)
                ret = np_xml_fqdn_check(&ctx);
        }
    }
    if (ret >= 0) {
        NpFqdnEntry *cur;
        np_free(body);
        body = NULL;
        cur = *list_out;
        while (cur != NULL) {
            ret = np_trans_track_alloc(trans, cur);
            if (ret < 0)
                break;
            cur = cur->next;
        }
        if (ret >= 0) {
            np_trans_release(trans);
            return 0;
        }
    }

    np_free_fqdn_list(*list_out);
    *list_out = NULL;
    if (body != NULL)
        np_free(body);
    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_00041295(int trans_id, NpId *npid_out)
{
    NpTransCtx *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int ret;
    const char *online_id = NULL;
    NpFqdnEntry *fqdn;
    int req_len = 0;

    if (npid_out == NULL)
        return (int)0x80550704;

    sceKernelMemset(npid_out, 0, sizeof(NpId));
    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0) {
        if (trans->kind != 4) {
            ret = (int)0x80550711;
        } else {
            online_id = (const char *)trans->extra;
            if (online_id == NULL)
                ret = (int)0x8055070C;
            else if (!np_validate_online_id(online_id))
                ret = (int)0x8055081B;
        }
    }
    if (ret >= 0 && trans->user->searchjid_fqdn == NULL)
        ret = np_fetch_searchjid_fqdn(trans);
    if (ret >= 0) {
        req_len = np_build_searchjid_xml(&req_xml, online_id);
        if (req_len < 0)
            ret = req_len;
        else if (req_xml == NULL)
            ret = (int)0x80550703;
    }
    if (ret >= 0) {
        fqdn = trans->user->searchjid_fqdn;
        while (fqdn != NULL) {
            ret = np_http_setup_endpoint(trans, 5, fqdn->host, (u32)req_len);
            if (ret < 0)
                break;
            ret = np_http_send_locked(trans, req_xml, (u32)req_len);
            if (ret < 0)
                break;
            if (req_xml != NULL) {
                np_free(req_xml);
                req_xml = NULL;
            }
            ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
            if (ret < 0)
                break;
            ret = np_parse_searchjid_resp(trans, npid_out, xml_len);
            if (ret == (int)0x8055081B) {
                ret = np_trans_close_http(trans);
                fqdn = fqdn->next;
                if (fqdn != NULL)
                    continue;
            }
            break;
        }
    }

    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_E00CDEBB(int trans_id, const NpId *npid, int avatar_size, NpProfileRecord *profile_out)
{
    NpTransCtx *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    char country[4];
    int req_len = 0;
    int ret;

    if (profile_out == NULL)
        return (int)0x80550704;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0 && trans->kind != 2)
        ret = (int)0x80550711;
    if (ret >= 0) {
        sceKernelMemset(profile_out, 0, 4);
        sceKernelMemcpy(country, npid->opt_region, 2);
        country[2] = '\0';

        req_len = np_build_get_profile_xml(&req_xml, npid, avatar_size);
        if (req_len < 0)
            ret = req_len;
        else if (req_xml == NULL)
            ret = (int)0x80550703;
    }
    if (ret >= 0)
        ret = np_http_setup_endpoint(trans, 3, country, (u32)req_len);
    if (ret >= 0)
        ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret >= 0) {
        np_free(req_xml);
        req_xml = NULL;
        ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    }
    if (ret >= 0)
        ret = np_parse_profile_resp(trans, profile_out, NULL, NULL, NULL, xml_len);
    if (ret >= 0) {
        ret = np_trans_close_http(trans);
        profile_out->base.npid = *npid;
    }

    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int np_get_self_profile(int trans_id, NpProfileRecord *profile_out, int *reserved_out, int *ptlp_out,
                        int *upd_out, u64 *date_out, const u8 *ticket, u32 ticket_len,
                        int avatar_size)
{
    NpTransCtx *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len = 0;
    int ret;

    if (profile_out == NULL || reserved_out == NULL || ptlp_out == NULL || ticket == NULL)
        return (int)0x80550704;

    sceKernelMemset(profile_out, 0, sizeof(NpProfileRecord));
    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0 && trans->kind != 1)
        ret = (int)0x80550711;
    if (ret >= 0) {
        req_len = np_build_self_profile_xml(&req_xml, ticket, ticket_len,
                                            trans->user->env, avatar_size);
        if (req_len < 0)
            ret = req_len;
        else if (req_xml == NULL)
            ret = (int)0x80550703;
    }
    if (ret >= 0)
        ret = np_http_setup_endpoint(trans, 1, NULL, (u32)req_len);
    if (ret >= 0)
        ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret >= 0)
        ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret >= 0) {
        ret = np_parse_profile_resp(trans, profile_out, ptlp_out, upd_out, date_out, xml_len);
        if (ret == (int)0x8055080D) {
            np_trans_close_http(trans);
            ret = np_http_setup_endpoint(trans, 2, NULL, (u32)req_len);
            if (ret >= 0)
                ret = np_http_send_locked(trans, req_xml, (u32)req_len);
            if (ret >= 0)
                ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
            if (ret >= 0)
                ret = np_parse_profile_resp(trans, profile_out, ptlp_out, upd_out, date_out, xml_len);
        }
    }

    if (ret >= 0) {
        if (req_xml != NULL) {
            np_free(req_xml);
            req_xml = NULL;
        }
        np_trans_close_http(trans);
        if (trans != NULL)
            np_trans_release(trans);
        return 0;
    }

    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_66B3E216(int trans_id, const char *online_name, const int *langs,
                          const char *about_me, const int *avatar_id, int avatar_size,
                          NpProfileRecord *profile_out, int *ptlp_out, int *upd_out, u64 *date_out,
                          const u8 *ticket, u32 ticket_len)
{
    NpTransCtx *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len = 0;
    int ret;

    if (ticket == NULL || profile_out == NULL ||
        (online_name == NULL && langs == NULL && about_me == NULL && avatar_id == NULL))
        return (int)0x8055070C;

    sceKernelMemset(profile_out, 0, sizeof(NpProfileRecord));
    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0 && trans->kind != 25)
        ret = (int)0x80550711;
    if (ret >= 0) {
        req_len = np_build_update_profile_xml(&req_xml, ticket, ticket_len,
                                              trans->user->env,
                                              online_name, langs, about_me, avatar_id, avatar_size);
        if (req_len < 0)
            ret = req_len;
        else if (req_xml == NULL)
            ret = (int)0x80550703;
    }
    if (ret >= 0)
        ret = np_http_setup_endpoint(trans, 24, NULL, (u32)req_len);
    if (ret >= 0)
        ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret >= 0)
        ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret >= 0)
        ret = np_parse_profile_resp(trans, profile_out, ptlp_out, upd_out, date_out, xml_len);

    if (ret >= 0) {
        if (req_xml != NULL) {
            np_free(req_xml);
            req_xml = NULL;
        }
        np_trans_close_http(trans);
        if (trans != NULL)
            np_trans_release(trans);
        return 0;
    }

    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_9CD89895(int trans_id, int lang, int start, NpAvatarCategory *entries, int max_num, int *total_out)
{
    NpTransCtx *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len = 0;
    int ret;

    if (entries == NULL || max_num == 0)
        return (int)0x80550704;

    sceKernelMemset(entries, 0, sizeof(NpAvatarCategory) * (SceSize)max_num);
    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0 && trans->kind != 23)
        ret = (int)0x80550711;
    if (ret >= 0) {
        req_len = np_build_avatar_cat_xml(&req_xml, lang, start, max_num);
        if (req_len < 0)
            ret = req_len;
        else if (req_xml == NULL)
            ret = (int)0x80550703;
    }
    if (ret >= 0)
        ret = np_http_setup_endpoint(trans, 22, NULL, (u32)req_len);
    if (ret >= 0)
        ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret >= 0)
        ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret >= 0)
        ret = np_parse_avatar_cat_resp(trans, total_out, entries, max_num, xml_len);

    if (ret >= 0) {
        if (req_xml != NULL) {
            np_free(req_xml);
            req_xml = NULL;
        }
        np_trans_close_http(trans);
        if (trans != NULL)
            np_trans_release(trans);
        return ret;
    }

    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_570DB15B(int trans_id, const int *cat_id, int size, int start,
                          int *ids, char (*urls)[128], int max_num, int *total_out)
{
    NpTransCtx *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len = 0;
    int ret;

    if (ids == NULL || urls == NULL)
        return (int)0x8055070C;
    if (max_num == 0)
        return (int)0x80550704;

    sceKernelMemset(ids, 0, sizeof(int) * (SceSize)max_num);
    sceKernelMemset(urls, 0, 128u * (SceSize)max_num);
    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0 && trans->kind != 24)
        ret = (int)0x80550711;
    if (ret >= 0) {
        req_len = np_build_avatar_list_xml(&req_xml, cat_id, size, start, max_num);
        if (req_len < 0)
            ret = req_len;
        else if (req_xml == NULL)
            ret = (int)0x80550703;
    }
    if (ret >= 0)
        ret = np_http_setup_endpoint(trans, 23, NULL, (u32)req_len);
    if (ret >= 0)
        ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret >= 0)
        ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret >= 0)
        ret = np_parse_avatar_list_resp(trans, total_out, ids, urls, max_num, xml_len);

    if (ret >= 0) {
        if (req_xml != NULL) {
            np_free(req_xml);
            req_xml = NULL;
        }
        np_trans_close_http(trans);
        if (trans != NULL)
            np_trans_release(trans);
        return ret;
    }

    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int np_trans_download_url(int trans_id, const char *url, u32 max_size, u32 *total_out,
                          u32 chunk_size, void *out_buf, int opt)
{
    NpTransCtx *trans = NULL;
    NpUrlDownloadState *dl_state = NULL;
    u32 got = 0;
    int ret;

    if (opt != 0)
        return (int)0x80550704;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0) {
        if (trans->extra == NULL) {
            if ((u32)(trans->kind - 27) >= 2u) {
                ret = (int)0x80550711;
            } else {
                dl_state = (NpUrlDownloadState *)np_alloc(sizeof(NpUrlDownloadState));
                if (dl_state == NULL) {
                    ret = (int)0x80550703;
                } else {
                    sceKernelMemset(dl_state, 0, sizeof(NpUrlDownloadState));
                    dl_state->phase = 0;
                    ret = np_trans_track_alloc(trans, dl_state);
                    if (ret < 0) {
                        np_free(dl_state);
                        dl_state = NULL;
                    } else {
                        trans->extra = dl_state;
                    }
                }
            }
        } else {
            dl_state = (NpUrlDownloadState *)trans->extra;
        }
    }

    if (ret >= 0) {
        if (dl_state->phase == 3u) {
            ret = dl_state->last_error;
        } else if (dl_state->phase == 2u) {
            if (total_out != NULL)
                *total_out = dl_state->total_size;
            ret = 0;
        } else {
            if (dl_state->phase == 0u) {
                int status = 0;
                u64 clen = 0;
                int rid;

                if (url == NULL) {
                    ret = (int)0x8055070C;
                } else {
                    ret = np_http_open_url(trans, url);
                }
                if (ret >= 0) {
                    rid = sceHttpCreateRequestWithURL(trans->conn_id, 0, url, 0);
                    if (rid < 0)
                        ret = rid;
                    else
                        trans->req_id = rid;
                }
                if (ret >= 0)
                    ret = np_http_send_locked(trans, NULL, 0);
                if (ret >= 0)
                    ret = sceHttpGetStatusCode(trans->req_id, &status);
                if (ret >= 0) {
                    if (status == 200) {
                        ret = sceHttpGetContentLength(trans->req_id, &clen);
                    } else if (trans->kind == 28 && status == 404) {
                        chunk_size = 0;
                        clen = 0;
                    } else {
                        ret = (int)0x8055070A;
                    }
                }
                if (ret >= 0) {
                    if (clen > (u64)max_size) {
                        ret = (int)0x80550709;
                    } else {
                        dl_state->remaining = (u32)clen;
                        dl_state->total_size = (u32)clen;
                        dl_state->phase = 1u;
                    }
                }
            }

            if (ret >= 0 && dl_state->phase == 1u && chunk_size != 0) {
                if (dl_state->remaining < chunk_size)
                    chunk_size = dl_state->remaining;
                if (out_buf == NULL) {
                    ret = (int)0x8055070C;
                } else {
                    ret = np_http_read_exact(trans, out_buf, chunk_size, &got);
                    if (ret >= 0) {
                        if (got != chunk_size)
                            ret = (int)0x8055070A;
                        else
                            dl_state->remaining -= chunk_size;
                    }
                }
            }

            if (ret >= 0) {
                if (dl_state->remaining == 0) {
                    np_trans_close_http(trans);
                    dl_state->phase = 2u;
                }
                if (total_out != NULL)
                    *total_out = dl_state->total_size;
                ret = (int)got;
            }
        }
    }

    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_43E635D2(int trans_id)
{
    NpTransCtx *trans = NULL;
    int ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0) {
        trans->aborted = 1;
        for (;;) {
            ret = sceKernelLockLwMutex(trans->lwmutex, 1, NULL);
            if (ret < 0)
                break;
            ret = 0;
            if (trans->req_id > 0)
                ret = sceHttpAbortRequest(trans->req_id);
            sceKernelUnlockLwMutex(trans->lwmutex, 1);
            if (ret >= 0 || trans->sending != 1)
                break;
            sceKernelDelayThread(500000);
        }
    }
    if (trans != NULL)
        np_trans_release(trans);
    return ret;
}

int sceNpService_D7A0CD2D(int trans_id, const u8 *ticket, u32 ticket_len,
                          const NpId *npid, int quota, NpUploaderEntry **list_out,
                          u32 start, u32 max_num)
{
    NpTransCtx *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len = 0;
    int ret;

    if (ticket == NULL || npid == NULL || list_out == NULL)
        return (int)0x80550704;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0 && trans->kind != 22)
        ret = (int)0x80550711;
    if (ret >= 0) {
        req_len = np_build_uploader_list_xml(&req_xml, ticket, ticket_len, npid, quota, start, max_num);
        if (req_len < 0)
            ret = req_len;
        else if (req_xml == NULL)
            ret = (int)0x80550703;
    }
    if (ret >= 0)
        ret = np_http_setup_endpoint(trans, 16, NULL, (u32)req_len);
    if (ret >= 0)
        ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret >= 0)
        ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret >= 0)
        ret = np_parse_uploader_list_resp(trans, list_out, xml_len);
    if (ret >= 0) {
        NpUploaderEntry *cur = *list_out;
        while (cur != NULL) {
            ret = np_trans_track_alloc(trans, cur);
            if (ret < 0)
                break;
            cur = cur->next;
        }
    }

    if (ret >= 0) {
        if (req_xml != NULL) {
            np_free(req_xml);
            req_xml = NULL;
        }
        np_trans_close_http(trans);
        if (trans != NULL)
            np_trans_release(trans);
        return 0;
    }

    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_AD2C0674(int trans_id, const u8 *ticket, u32 ticket_len,
                          const NpId *npid, const char *ref, void *out_buf,
                          u64 req_size, NpUploaderDlInfo *dl_info_out, int opt)
{
    NpTransCtx *trans = NULL;
    NpUploaderDlState *dl_state = NULL;
    char *req_xml = NULL;
    u64 extra_len = 0;
    u32 xml_len = 0;
    u32 got = 0;
    int read_total = 0;
    int ret;

    if (ticket == NULL || npid == NULL || dl_info_out == NULL || ref == NULL)
        return (int)0x8055070C;
    if (req_size == 0 && out_buf == NULL)
        return (int)0x8055070C;
    if (opt != 0)
        return (int)0x80550704;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0 && trans->kind != 22)
        ret = (int)0x80550711;
    if (ret >= 0) {
        if (trans->extra == NULL) {
            dl_state = (NpUploaderDlState *)np_alloc(sizeof(NpUploaderDlState));
            if (dl_state == NULL) {
                ret = (int)0x80550703;
            } else {
                sceKernelMemset(dl_state, 0, sizeof(NpUploaderDlState));
                dl_state->phase = 0;
                ret = np_trans_track_alloc(trans, dl_state);
                if (ret < 0) {
                    np_free(dl_state);
                    dl_state = NULL;
                } else {
                    trans->extra = dl_state;
                }
            }
        } else {
            dl_state = (NpUploaderDlState *)trans->extra;
        }
    }

    if (ret >= 0) {
        if (dl_state->phase == 3u) {
            ret = (int)dl_state->last_error;
        } else if (dl_state->phase == 2u) {
            ret = (int)0x80550712;
        } else {
            if (dl_state->phase == 0u) {
                char country[8];
                int req_len;
                u32 sep_len;

                sceKernelMemset(country, 0, sizeof(country));
                sceKernelMemcpy(country, ref, 2);
                req_len = np_build_uploader_dl_xml(&req_xml, ticket, ticket_len, npid, ref);
                if (req_len < 0)
                    ret = req_len;
                else if (req_xml == NULL)
                    ret = (int)0x80550703;
                if (ret >= 0)
                    ret = np_http_setup_endpoint(trans, 15, country, (u32)req_len);
                if (ret >= 0)
                    ret = np_http_send_locked(trans, req_xml, (u32)req_len);
                if (ret >= 0) {
                    np_free(req_xml);
                    req_xml = NULL;
                    ret = np_http_check_resp(trans, 1, &extra_len, &xml_len, NULL, NULL);
                }
                if (ret >= 0)
                    ret = np_parse_uploader_dl_resp(trans, dl_info_out, xml_len);
                if (ret >= 0) {
                    sep_len = (u32)sceNet_lib_D8722983("\r\n\r\n");
                    ret = np_http_expect_bytes(trans, "\r\n\r\n", sep_len);
                    if (ret >= 0) {
                        extra_len -= sep_len;
                        dl_state->phase = 1u;
                        dl_state->remaining = (u32)extra_len;
                        dl_state->total_size_lo = (u32)extra_len;
                        dl_state->total_size_hi = 0;
                    }
                }
            }

            if (ret >= 0 && req_size != 0 && dl_state->phase == 1u) {
                u64 rem = dl_state->remaining;
                if (rem < req_size)
                    req_size = rem;
                ret = np_http_read_exact(trans, out_buf, (u32)req_size, &got);
                if (ret >= 0) {
                    if (got != (u32)req_size || (req_size >> 32) != 0) {
                        ret = (int)0x8055070A;
                    } else {
                        dl_state->remaining = rem - got;
                        read_total = (int)got;
                    }
                }
            }

            if (ret >= 0) {
                if (dl_state->remaining == 0) {
                    np_trans_close_http(trans);
                    dl_state->phase = 2u;
                }
                ret = read_total;
            }
        }
    }

    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        if (ret < 0 && dl_state != NULL) {
            dl_state->phase = 3u;
            dl_state->last_error = ret;
        }
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_D97B509F(const void *online_id)
{
    if (np_httpxml_is_init() == 1)
        return np_user_create(online_id);
    return (int)0x80550702;
}

int sceNpService_5B74DE06(const void *online_id)
{
    return np_user_destroy(online_id);
}

int sceNpService_DD8B5E53(int trans_id)
{
    return np_trans_destroy_by_owner(trans_id, 1);
}

int np_trans_destroy_lookup(int trans_id)
{
    return np_trans_destroy_by_owner(trans_id, 2);
}
