#include "np_service_internal.h"

/* =========================================================================
 * Endpoint Table (25 entries)
 * ========================================================================= */

typedef struct NpEndpointDef {
    int require_arg;
    const char *scheme;
    int method;
    const char *host_prefix;
    const char *path1;
    const char *path2;
} NpEndpointDef;

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
static int g_trans_head[23]; /* dummy head at 0x119E4 (92 bytes, next at w10) */
static u8 g_trans_lwmtx[32];

static int g_user_mgr_init = 0;
static int g_user_head[26];  /* dummy head at 0x11A64 (104 bytes, next at w24) */
static u8 g_user_lwmtx[32];

static int np_mem_init(void)
{
    if (g_mem_init == 1)
        return (int)0x80550701;
    g_mem_init = 1;
    return 0;
}

static int np_mem_term(void)
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

void np_free(void *ptr)
{
    sceNpService_61AB2062(ptr);
}

/* --- User context management --- */

static int np_user_mgr_init(void)
{
    int ret;
    if (g_user_mgr_init != 0)
        return (int)0x80550701;
    sceKernelMemset(g_user_head, 0, sizeof(g_user_head));
    g_user_head[0] = 0x10000000;
    ret = sceKernelCreateLwMutex(g_user_lwmtx, "SceNetHttpXml", 0, 0, NULL);
    if (ret < 0)
        return ret;
    g_user_mgr_init = 1;
    return 0;
}

static int np_user_mgr_term(void)
{
    int ret;
    int *cur;

    if (g_user_mgr_init != 1)
        return (int)0x80550702;
    ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
    if (ret < 0)
        return ret;

    cur = (int *)g_user_head[24];
    while (cur != NULL) {
        int *next = (int *)cur[24];
        int *prev = (int *)cur[25];
        if (prev != NULL)
            prev[24] = (int)next;
        if (next != NULL)
            next[25] = (int)prev;
        {
            int *alloc_node = (int *)cur[23];
            while (alloc_node != NULL) {
                int *anext = (int *)alloc_node[1];
                np_free((void *)alloc_node[0]);
                np_free(alloc_node);
                alloc_node = anext;
            }
        }
        sceKernelDeleteLwMutex(cur + 8);
        np_free(cur);
        cur = next;
    }

    sceKernelUnlockLwMutex(g_user_lwmtx, 1);
    sceKernelDeleteLwMutex(g_user_lwmtx);
    g_user_mgr_init = 0;
    return 0;
}

static int np_user_acquire(const void *online_id, int **out_user)
{
    int ret;
    int *cur;

    *out_user = NULL;
    if (online_id == NULL)
        return (int)0x80550704;

    ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
    if (ret < 0)
        return ret;

    cur = (int *)g_user_head[24];
    while (cur != NULL) {
        if (sceNet_lib_B9085A96((const char *)(cur + 3), (const char *)online_id, 16) == 0 &&
            cur[2] == 0) {
            cur[1]++;
            *out_user = cur;
            ret = 0;
            break;
        }
        cur = (int *)cur[24];
    }

    sceKernelUnlockLwMutex(g_user_lwmtx, 1);
    if (cur == NULL)
        return (int)0x8055070F;
    return ret;
}

static void np_user_release(int *user)
{
    if (user == NULL || sceKernelLockLwMutex(g_user_lwmtx, 1, NULL) < 0)
        return;

    user[1]--;
    if (user[2] == 1 && user[1] == 0) {
        int *prev = (int *)user[25];
        int *next = (int *)user[24];
        int *alloc_node;
        if (prev != NULL)
            prev[24] = (int)next;
        if (next != NULL)
            next[25] = (int)prev;
        alloc_node = (int *)user[23];
        while (alloc_node != NULL) {
            int *anext = (int *)alloc_node[1];
            np_free((void *)alloc_node[0]);
            np_free(alloc_node);
            alloc_node = anext;
        }
        sceKernelDeleteLwMutex(user + 8);
        np_free(user);
    }
    sceKernelUnlockLwMutex(g_user_lwmtx, 1);
}

static int np_user_track_alloc(int *user, void *ptr)
{
    int *node;
    int ret;

    if (ptr == NULL)
        return 0;

    node = (int *)np_alloc(8);
    if (node == NULL)
        return (int)0x80550703;

    ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
    if (ret < 0) {
        np_free(node);
        return ret;
    }

    node[0] = (int)ptr;
    node[1] = user[23];
    user[23] = (int)node;
    sceKernelUnlockLwMutex(g_user_lwmtx, 1);
    return 0;
}

static int np_user_insert(int *user)
{
    int ret;
    int *cur;
    int *prev;
    int new_id;

    ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
    if (ret < 0)
        return ret;

    cur = (int *)g_user_head[24];
    while (cur != NULL) {
        if (sceNet_lib_B9085A96((const char *)(cur + 3), (const char *)(user + 3), 16) == 0 &&
            cur[2] == 0) {
            sceKernelUnlockLwMutex(g_user_lwmtx, 1);
            return (int)0x80550710;
        }
        cur = (int *)cur[24];
    }

    prev = g_user_head;
    cur = (int *)g_user_head[24];
    while (cur != NULL && cur[0] == prev[0] + 1) {
        prev = cur;
        cur = (int *)cur[24];
    }

    if (prev[0] > 0x20000000) {
        sceKernelUnlockLwMutex(g_user_lwmtx, 1);
        return (int)0x80550706;
    }

    new_id = prev[0] + 1;
    user[0] = new_id;
    user[24] = prev[24];
    user[25] = (int)prev;
    if (prev[24] != 0)
        ((int *)prev[24])[25] = (int)user;
    prev[24] = (int)user;
    sceKernelUnlockLwMutex(g_user_lwmtx, 1);
    return new_id;
}

static int np_user_create(const void *online_id)
{
    int *user = (int *)np_alloc(104);
    int ret = (int)0x80550703;
    if (user != NULL) {
        sceKernelMemset(user, 0, 104);
        sceKernelMemcpy(user + 3, online_id, 20);
        sceKernelCreateLwMutex(user + 8, "SceNetHttpXml", 0, 0, NULL);
        ret = np_user_insert(user);
        if (ret >= 0)
            return user[0];
        np_free(user);
    }
    return ret;
}

static int np_user_destroy(const void *online_id)
{
    int *user = NULL;
    int ret;

    ret = np_user_acquire(online_id, &user);
    if (ret >= 0) {
        ret = sceKernelLockLwMutex(g_user_lwmtx, 1, NULL);
        if (ret >= 0) {
            user[2] = 1;
            sceKernelUnlockLwMutex(g_user_lwmtx, 1);
            np_user_release(user);
            return 0;
        }
    }
    np_user_release(user);
    return ret;
}

/* --- Transaction context management --- */

static int np_trans_free(int *trans)
{
    int *node = (int *)trans[8];
    trans[8] = 0;
    while (node != NULL) {
        int *next = (int *)node[1];
        np_free((void *)node[0]);
        np_free(node);
        node = next;
    }
    sceKernelDeleteLwMutex(trans + 15);
    np_free(trans);
    return 0;
}

static int np_trans_insert(int *trans)
{
    int ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
    if (ret >= 0) {
        int *tail = g_trans_head;
        int *cur = (int *)g_trans_head[10];
        while (cur != NULL) {
            tail = cur;
            cur = (int *)cur[10];
        }
        g_trans_count++;
        g_next_trans_id++;
        if (tail[0] > 0x2FFFFFFF)
            g_next_trans_id = 0x20000001;
        ret = g_next_trans_id;
        trans[10] = 0;
        tail[10] = (int)trans;
        trans[0] = ret;
        trans[11] = (int)tail;
        sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
    }
    return ret;
}

int np_trans_acquire(int trans_id, int **out_trans)
{
    int ret = (int)0x8055070E;
    int *cur = g_trans_head;

    *out_trans = NULL;
    if (trans_id == 0)
        return ret;

    ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
    if (ret < 0)
        return ret;

    while (cur != NULL) {
        if (cur[0] == trans_id) {
            cur[1]++;
            *out_trans = cur;
            ret = 0;
            break;
        }
        if (trans_id < cur[0]) {
            cur = NULL;
            break;
        }
        cur = (int *)cur[10];
    }

    sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
    if (cur == NULL)
        return (int)0x8055070E;
    return ret;
}

int np_trans_release(void *trans_ptr)
{
    int *trans = (int *)trans_ptr;
    int ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
    if (ret >= 0) {
        trans[1]--;
        if (trans[2] == 1 && trans[1] == 0) {
            if (trans[4] > 0)
                sceHttpDeleteRequest(trans[4]);
            trans[4] = 0;
            if (trans[3] > 0)
                sceHttpDeleteConnection(trans[3]);
            trans[3] = 0;
            np_user_release((int *)trans[9]);
            trans[9] = 0;
            np_trans_free(trans);
            g_trans_count--;
        }
        return sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
    }
    return ret;
}

int np_trans_track_alloc(void *trans_ptr, void *ptr)
{
    int *trans = (int *)trans_ptr;
    int *node;
    int ret;

    if (ptr == NULL)
        return 0;

    node = (int *)np_alloc(8);
    if (node == NULL)
        return (int)0x80550703;

    ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
    if (ret < 0) {
        np_free(node);
        return ret;
    }

    node[0] = (int)ptr;
    node[1] = trans[8];
    trans[8] = (int)node;
    sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
    return 0;
}

static int np_trans_create_obj(int *user, int type, int owner, void *arg)
{
    int *trans = (int *)np_alloc(92);
    int ret = (int)0x80550703;
    if (trans != NULL) {
        sceKernelMemset(trans, 0, 92);
        sceKernelCreateLwMutex(trans + 15, "SceNetHttpXml", 0, 0, NULL);
        trans[9] = (int)user;
        trans[13] = type;
        trans[5] = owner;
        trans[14] = (int)arg;
        if (arg == NULL || (ret = np_trans_track_alloc(trans, arg)) >= 0) {
            ret = np_trans_insert(trans);
            if (ret >= 0)
                return trans[0];
        }
        np_trans_free(trans);
    }
    return ret;
}

static int np_trans_destroy_by_owner(int trans_id, int owner)
{
    int *trans = NULL;
    int ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0) {
        if (trans[5] != owner) {
            np_trans_release(trans);
            return (int)0x8055070E;
        }
        sceNpService_43E635D2(trans_id);
        ret = sceKernelLockLwMutex(trans + 15, 1, NULL);
        if (ret >= 0) {
            ret = sceKernelLockLwMutex(g_trans_lwmtx, 1, NULL);
            if (ret >= 0) {
                int *prev = (int *)trans[11];
                int *next = (int *)trans[10];
                if (prev != NULL)
                    prev[10] = (int)next;
                if (next != NULL)
                    next[11] = (int)prev;
                trans[2] = 1;
                sceKernelUnlockLwMutex(g_trans_lwmtx, 1);
            }
            sceKernelUnlockLwMutex(trans + 15, 1);
            if (ret >= 0) {
                np_trans_release(trans);
                return 0;
            }
        }
    }
    if (trans != NULL)
        np_trans_release(trans);
    return ret;
}

static int np_trans_mgr_init(void)
{
    int ret;
    int tid;

    if (g_trans_mgr_init != 0)
        return (int)0x80550701;

    sceKernelMemset(g_trans_head, 0, sizeof(g_trans_head));
    g_trans_head[0] = 0x20000000;
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

static int np_trans_mgr_term(void)
{
    int *cur;
    if (g_trans_mgr_init != 1)
        return (int)0x80550702;

    cur = (int *)g_trans_head[10];
    while (cur != NULL) {
        int *next = (int *)cur[10];
        np_trans_destroy_by_owner(cur[0], 1);
        cur = next;
    }
    sceKernelDeleteLwMutex(g_trans_lwmtx);
    if (g_http_template > 0)
        sceHttpDeleteTemplate(g_http_template);
    g_http_template = 0;
    g_trans_mgr_init = 0;
    return 0;
}

int np_httpxml_init(void)
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

int np_httpxml_term(void)
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

static int np_http_auth_cb(int req_id, int type, const char *realm,
                           char *user, char *pass, int need_entity,
                           u8 **entity_body, u32 *entity_size,
                           int *save, int mode)
{
    int i;

    (void)req_id; (void)need_entity; (void)entity_body; (void)entity_size;

    if (save != NULL)
        *save = 0;
    if (type != 1 || realm == NULL)
        return -1;

    if (mode == 1) {
        int rlen = sceNet_lib_D8722983("c7y-ranking");
        if (sceNet_lib_94DCA9F0(realm, "c7y-ranking", rlen + 1) != 0 || user == NULL || pass == NULL)
            return -1;
        sceNet_lib_B5CE388A(user, "c7y-ranking01", sceNet_lib_D8722983("c7y-ranking01") + 1);
        for (i = 0; i < 32; i++)
            pass[i] = (char)(g_ranking_auth_pw_xor[i] ^ 0x7Fu);
        pass[32] = '\0';
        return 0;
    }

    if (mode == 2) {
        int rlen = sceNet_lib_D8722983("c7y-basic");
        if (sceNet_lib_94DCA9F0(realm, "c7y-basic", rlen + 1) != 0 || user == NULL || pass == NULL)
            return -1;
        sceNet_lib_B5CE388A(user, "c7y-basic02", sceNet_lib_D8722983("c7y-basic02") + 1);
        for (i = 0; i < 32; i++)
            pass[i] = (char)(g_basic_auth_pw_xor[i] ^ 0x7Fu);
        pass[32] = '\0';
        return 0;
    }

    return -1;
}

static int np_http_open_conn(int *trans, const char *host, const char *scheme, u16 port)
{
    int cid = sceHttpCreateConnection(g_http_template, host, scheme, port, 0);
    if (cid >= 0) {
        int mode = 2;
        trans[3] = cid;
        if (sceNet_lib_B9085A96(host, "getprof", sceNet_lib_D8722983("getprof")) != 0 &&
            sceNet_lib_B9085A96(host, "searchjid", sceNet_lib_D8722983("searchjid")) != 0) {
            mode = 1;
        }
        sceHttpSetAuthInfoCallback(cid, np_http_auth_cb, (void *)mode);
        return 0;
    }
    return cid;
}

static int np_http_open_url(int *trans, const char *url)
{
    int cid = sceHttpCreateConnectionWithURL(g_http_template, url, 0);
    if (cid >= 0) {
        trans[3] = cid;
        return 0;
    }
    return cid;
}

static int np_http_create_req(int *trans, int method, const char *path, u64 content_len)
{
    int rid = sceHttpCreateRequest(trans[3], method, path, content_len);
    if (rid >= 0) {
        trans[4] = rid;
        return 0;
    }
    return rid;
}

static int np_http_send_locked(int *trans, const void *body, u32 body_len)
{
    int ret = sceKernelLockLwMutex(trans + 15, 1, NULL);
    if (ret < 0)
        return ret;
    if (trans[6] == 1)
        ret = (int)0x80550707;
    else
        trans[7] = 1;
    sceKernelUnlockLwMutex(trans + 15, 1);
    if (ret >= 0) {
        ret = sceHttpSendRequest(trans[4], body, body_len);
        trans[7] = 0;
        if (ret < 0)
            ret = np_map_ssl_error(trans[4], ret);
    }
    return ret;
}

static int np_http_setup_endpoint(int *trans, u32 ep_idx, const void *arg, u32 content_len)
{
    const NpEndpointDef *ep = &g_np_endpoints[ep_idx];
    u8 *user = (u8 *)trans[9];
    const char *env = (const char *)(user + 64);
    const char *country = (const char *)(user + 79);
    char *buf;
    int ret;
    u16 port;
    const char *path;

    buf = (char *)np_alloc(256);
    if (buf == NULL)
        return (int)0x80550703;
    sceKernelMemset(buf, 0, 256);

    if (ep->require_arg == 1 && arg == NULL) {
        ret = (int)0x8055070C;
        goto done;
    }

    switch (ep_idx) {
    case 3:
    case 15: {
        u32 total = (u32)(sceNet_lib_D8722983(ep->host_prefix) +
                          sceNet_lib_D8722983(env) +
                          sceNet_lib_D8722983(".community.playstation.net") + 1);
        if (arg != NULL)
            total += (u32)sceNet_lib_D8722983((const char *)arg) + 1;
        if (total >= 257u) { ret = (int)0x80550781; goto done; }
        sceNet_lib_CF705E46(buf, "%s%s.%s%s", ep->host_prefix, (const char *)arg, env, ".community.playstation.net");
        break;
    }
    case 4:
    case 6:
    case 7:
    case 21: {
        u32 total = (u32)(sceNet_lib_D8722983(ep->host_prefix) +
                          sceNet_lib_D8722983(env) +
                          sceNet_lib_D8722983(".community.playstation.net") + 1);
        if (total >= 257u) { ret = (int)0x80550781; goto done; }
        sceNet_lib_CF705E46(buf, "%s%s%s", ep->host_prefix, env, ".community.playstation.net");
        break;
    }
    case 5:
    case 10:
    case 11:
    case 12:
    case 13:
        sceNet_lib_B5CE388A(buf, (const char *)arg, 255);
        break;
    case 8:
    case 9:
        sceNet_lib_B5CE388A(buf, (const char *)arg + 128, 255);
        break;
    default: {
        u32 total = (u32)(sceNet_lib_D8722983(ep->host_prefix) +
                          sceNet_lib_D8722983(env) +
                          sceNet_lib_D8722983(".community.playstation.net") + 1);
        if (country != NULL)
            total += (u32)sceNet_lib_D8722983(country) + 1;
        if (total >= 257u) { ret = (int)0x80550781; goto done; }
        sceNet_lib_CF705E46(buf, "%s%s.%s%s", ep->host_prefix, country, env, ".community.playstation.net");
        break;
    }
    }

    port = (sceNet_lib_A0F16ABD(ep->scheme, "http") == 0) ? 80 : 443;
    ret = np_http_open_conn(trans, buf, ep->scheme, port);
    if (ret < 0)
        goto done;

    if (ep_idx == 4) {
        u32 plen = (u32)(sceNet_lib_D8722983("/np/resource/basic/") +
                         sceNet_lib_D8722983(country) +
                         sceNet_lib_D8722983(ep->path1) + 1);
        if (plen >= 257u) { ret = (int)0x80550781; goto done; }
        sceNet_lib_CF705E46(buf, "%s%s%s", "/np/resource/basic/", country, ep->path1);
        path = buf;
    } else if (ep_idx == 6 || ep_idx == 7 || ep_idx == 21) {
        char title_str[16];
        sceKernelMemset(title_str, 0, 13);
        ret = sceNp_E90DFBC4(arg, title_str, 13);
        if (ret < 0)
            goto done;
        {
            u32 plen = (u32)(sceNet_lib_D8722983("/np/resource/psp-title/") +
                             2 * sceNet_lib_D8722983(title_str) +
                             sceNet_lib_D8722983(ep->path1) +
                             sceNet_lib_D8722983(ep->path2) + 1);
            if (plen >= 257u) { ret = (int)0x80550781; goto done; }
            sceNet_lib_CF705E46(buf, "%s%s%s%s%s", "/np/resource/psp-title/", title_str, ep->path1, title_str, ep->path2);
            path = buf;
        }
    } else {
        path = ep->path1;
    }

    ret = np_http_create_req(trans, ep->method, path, (u64)content_len);

done:
    np_free(buf);
    return ret;
}

static int np_http_check_resp(int *trans, int mode, u64 *extra_len_out, u32 *xml_len_out, void *unused, u8 *sig_out)
{
    int status = 0;
    u64 clen = 0;
    u32 info_len = 0;
    int ret;

    (void)unused;

    ret = np_http_parse_headers(trans, &status, &clen, &info_len, sig_out, mode);
    if (ret < 0 || clen == 0) {
        if (ret >= 0) ret = (int)0x80550708;
        np_trans_close_http(trans);
        return ret;
    }

    if (info_len != 0) {
        if (clen < (u64)info_len) {
            np_trans_close_http(trans);
            return (int)0x80550708;
        }
        if (extra_len_out != NULL)
            *extra_len_out = clen - (u64)info_len;
    } else {
        if (clen > 0x10000ULL) {
            np_trans_close_http(trans);
            return (int)0x80550709;
        }
        info_len = (u32)clen;
        if (extra_len_out != NULL)
            *extra_len_out = 0;
    }

    if (info_len > 0x10000u) {
        np_trans_close_http(trans);
        return (int)0x80550709;
    }
    *xml_len_out = info_len;
    return 0;
}

static int np_http_expect_bytes(int *trans, const void *expected, u32 len)
{
    void *buf = np_alloc(len);
    u32 got = 0;
    int ret = (int)0x80550703;

    if (buf != NULL) {
        if (trans[6] == 1) {
            ret = (int)0x80550707;
        } else {
            ret = np_http_read_exact(trans, buf, len, &got);
            if (ret >= 0) {
                if (got != len || sceNet_lib_E0A81C7C(buf, expected, (int)len) != 0)
                    ret = (int)0x80550708;
            }
        }
        np_free(buf);
    }
    return ret;
}

/* =========================================================================
 * XML Request Builders
 * ========================================================================= */

static const char *np_avatar_size_str(int size, int check_sdk)
{
    if (check_sdk && sceKernelGetCompiledSdkVersion() <= 0x04010010u)
        size = 0;
    if (size == 0) return "l";
    if (size == 1) return "m";
    if (size == 2) return "s";
    return NULL;
}

static int np_build_uploader_dl_xml(char **out_xml, const u8 *ticket, u32 ticket_len,
                                    const void *npid, const char *ref)
{
    u32 b64_size = 4u * ((ticket_len + 2u) / 3u) + 1u;
    char *b64 = NULL;
    char *xml = NULL;
    char *jid = NULL;
    char ver[16];
    int ret;
    char *p;

    b64 = (char *)np_alloc(b64_size);
    if (b64 == NULL)
        return (int)0x80550703;
    sceKernelMemset(b64, 0, b64_size);
    ret = np_base64_encode(b64, ticket, ticket_len);
    if (ret < 0)
        goto fail;

    xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(b64) + 256));
    if (xml == NULL) { ret = (int)0x80550703; goto fail; }

    p = xml;
    p += sceNet_lib_CF705E46(p, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" %s=\"%s\">", "uploader", "uploader", "psp", "sv", ver);
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "ticket", b64, "ticket");

    ret = np_npid_to_jid_str(&jid, npid);
    if (ret < 0)
        goto fail;
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "ticketjid", jid, "ticketjid");
    np_free(jid);
    jid = NULL;

    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "reference", ref, "reference");
    p += sceNet_lib_CF705E46(p, "</%s>", "uploader");
    *out_xml = xml;
    np_free(b64);
    return (int)(p - xml);

fail:
    if (jid != NULL) np_free(jid);
    if (b64 != NULL) np_free(b64);
    if (xml != NULL) np_free(xml);
    return ret;
}

static int np_build_self_profile_xml(char **out_xml, const u8 *ticket, u32 ticket_len,
                                     const char *env, int avatar_size)
{
    const char *sz = np_avatar_size_str(avatar_size, 1);
    u32 b64_size;
    char *b64 = NULL;
    char *xml = NULL;
    char ver[16];
    int ret;
    char *p;

    if (sz == NULL)
        return (int)0x80550704;

    b64_size = 4u * ((ticket_len + 2u) / 3u) + 1u;
    b64 = (char *)np_alloc(b64_size);
    if (b64 == NULL)
        return (int)0x80550703;
    sceKernelMemset(b64, 0, b64_size);
    ret = np_base64_encode(b64, ticket, ticket_len);
    if (ret < 0)
        goto fail;

    xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(b64) + 256));
    if (xml == NULL) { ret = (int)0x80550703; goto fail; }

    p = xml;
    p += sceNet_lib_CF705E46(p, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "ticket", b64, "ticket");
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "env", env, "env");
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" />", "avatar", "size", sz);
    p += sceNet_lib_CF705E46(p, "</%s>", "profile");
    *out_xml = xml;
    np_free(b64);
    return (int)(p - xml);

fail:
    if (b64 != NULL) np_free(b64);
    if (xml != NULL) np_free(xml);
    return ret;
}

static int np_build_update_profile_xml(char **out_xml, const u8 *ticket, u32 ticket_len,
                                       const char *env, const char *online_name, const int *langs,
                                       const char *about_me, const int *avatar_id, int avatar_size)
{
    const char *sz = np_avatar_size_str(avatar_size, 1);
    u32 b64_size;
    char *b64 = NULL;
    char *esc_name = NULL;
    char *esc_about = NULL;
    char *xml = NULL;
    char ver[16];
    int total_len;
    int ret;
    char *p;

    if (sz == NULL)
        return (int)0x80550704;

    b64_size = 4u * ((ticket_len + 2u) / 3u) + 1u;
    b64 = (char *)np_alloc(b64_size);
    if (b64 == NULL)
        return (int)0x80550703;
    sceKernelMemset(b64, 0, b64_size);
    ret = np_base64_encode(b64, ticket, ticket_len);
    if (ret < 0)
        goto fail;

    total_len = sceNet_lib_D8722983(b64) + 256;
    if (online_name != NULL) {
        ret = np_xml_escape_str(&esc_name, online_name);
        if (ret < 0) goto fail;
        total_len += sceNet_lib_D8722983(esc_name);
    }
    if (about_me != NULL) {
        ret = np_xml_escape_str(&esc_about, about_me);
        if (ret < 0) goto fail;
        total_len += sceNet_lib_D8722983(esc_about);
    }

    xml = (char *)np_alloc((SceSize)total_len);
    if (xml == NULL) { ret = (int)0x80550703; goto fail; }

    p = xml;
    p += sceNet_lib_CF705E46(p, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "ticket", b64, "ticket");
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "env", env, "env");
    if (esc_name != NULL)
        p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "onlinename", esc_name, "onlinename");
    if (langs != NULL) {
        int l0 = langs[0], l1 = langs[1], l2 = langs[2];
        if (l1 >= 0 && l0 < 0) { l0 = l1; l1 = -1; }
        if (l2 >= 0) {
            if (l0 < 0) { l0 = l2; l2 = -1; }
            else if (l1 < 0) { l1 = l2; l2 = -1; }
        }
        if (l0 < 0) { ret = (int)0x80550704; goto fail; }
        p += sceNet_lib_CF705E46(p, "<%s>%d</%s>", "language1", l0, "language1");
        if (l1 < 0) p += sceNet_lib_CF705E46(p, "<%s />", "language2");
        else        p += sceNet_lib_CF705E46(p, "<%s>%d</%s>", "language2", l1, "language2");
        if (l2 < 0) p += sceNet_lib_CF705E46(p, "<%s />", "language3");
        else        p += sceNet_lib_CF705E46(p, "<%s>%d</%s>", "language3", l2, "language3");
    }
    if (esc_about != NULL)
        p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "aboutme", esc_about, "aboutme");
    if (avatar_id != NULL)
        p += sceNet_lib_CF705E46(p, "<%s %s=\"%d\" %s=\"%s\"/>", "avatar", "id", *avatar_id, "size", sz);
    else
        p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\"/>", "avatar", "size", sz);
    p += sceNet_lib_CF705E46(p, "</%s>", "profile");

    *out_xml = xml;
    ret = (int)(p - xml);
    xml = NULL;

fail:
    if (b64 != NULL) np_free(b64);
    if (esc_name != NULL) np_free(esc_name);
    if (esc_about != NULL) np_free(esc_about);
    if (xml != NULL) np_free(xml);
    return ret;
}

static int np_build_get_profile_xml(char **out_xml, const void *npid, int avatar_size)
{
    char *jid = NULL;
    char *xml = NULL;
    const char *sz;
    char ver[16];
    int ret;
    char *p;

    ret = np_npid_to_jid_str(&jid, npid);
    if (ret < 0)
        goto fail;
    if (jid == NULL) { ret = (int)0x80550703; goto fail; }

    sz = np_avatar_size_str(avatar_size, 1);
    if (sz == NULL) { ret = (int)0x80550704; goto fail; }

    xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(jid) + 256));
    if (xml == NULL) { ret = (int)0x80550703; goto fail; }

    p = xml;
    p += sceNet_lib_CF705E46(p, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "jid", jid, "jid");
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" />", "avatar", "size", sz);
    p += sceNet_lib_CF705E46(p, "</%s>", "profile");
    *out_xml = xml;
    ret = (int)(p - xml);
    xml = NULL;

fail:
    if (jid != NULL) np_free(jid);
    if (xml != NULL) np_free(xml);
    return ret;
}

static int np_build_searchjid_xml(char **out_xml, const char *online_id)
{
    char *xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(online_id) + 256));
    char ver[16];
    char *p;

    if (xml == NULL)
        return (int)0x80550703;
    p = xml;
    p += sceNet_lib_CF705E46(p, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" %s=\"%s\">", "searchjid", "platform", "psp", "sv", ver);
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "online-id", online_id, "online-id");
    p += sceNet_lib_CF705E46(p, "</%s>", "searchjid");
    *out_xml = xml;
    return (int)(p - xml);
}

static int np_build_avatar_cat_xml(char **out_xml, int lang, int start, int max_num)
{
    char *xml = (char *)np_alloc(256);
    char ver[16];
    char *p;

    if (xml == NULL)
        return (int)0x80550703;
    p = xml;
    p += sceNet_lib_CF705E46(p, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
    p += sceNet_lib_CF705E46(p, "<%s>%d</%s>", "start", start, "start");
    p += sceNet_lib_CF705E46(p, "<%s>%d</%s>", "max", max_num, "max");
    p += sceNet_lib_CF705E46(p, "<%s>%d</%s>", "lang", lang, "lang");
    p += sceNet_lib_CF705E46(p, "</%s>", "profile");
    *out_xml = xml;
    return (int)(p - xml);
}

static int np_build_avatar_list_xml(char **out_xml, const int *cat_id, int size, int start, int max_num)
{
    const char *sz = np_avatar_size_str(size, 0);
    char *xml;
    char ver[16];
    char *p;

    if (sz == NULL)
        return (int)0x80550704;
    xml = (char *)np_alloc(256);
    if (xml == NULL)
        return (int)0x80550703;

    p = xml;
    p += sceNet_lib_CF705E46(p, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp", "sv", ver);
    p += sceNet_lib_CF705E46(p, "<%s>%d</%s>", "start", start, "start");
    p += sceNet_lib_CF705E46(p, "<%s>%d</%s>", "max", max_num, "max");
    if (cat_id != NULL)
        p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" %s=\"%d\" />", "avatar", "size", sz, "category", *cat_id);
    else
        p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" />", "avatar", "size", sz);
    p += sceNet_lib_CF705E46(p, "</%s>", "profile");
    *out_xml = xml;
    return (int)(p - xml);
}

static int np_build_uploader_list_xml(char **out_xml, const u8 *ticket, u32 ticket_len,
                                      const void *npid, int quota, u32 start, u32 max_num)
{
    u32 b64_size = 4u * ((ticket_len + 2u) / 3u) + 1u;
    char *b64 = NULL;
    char *xml = NULL;
    char *jid = NULL;
    const char *qstr;
    char ver[16];
    int ret;
    char *p;

    b64 = (char *)np_alloc(b64_size);
    if (b64 == NULL)
        return (int)0x80550703;
    sceKernelMemset(b64, 0, b64_size);
    ret = np_base64_encode(b64, ticket, ticket_len);
    if (ret < 0)
        goto fail;

    xml = (char *)np_alloc((SceSize)(sceNet_lib_D8722983(b64) + 384));
    if (xml == NULL) { ret = (int)0x80550703; goto fail; }

    p = xml;
    p += sceNet_lib_CF705E46(p, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    sceKernelMemset(ver, 0, 8);
    sceNet_lib_CF705E46(ver, "%d.%02d", 6, 60);
    p += sceNet_lib_CF705E46(p, "<%s %s=\"%s\" %s=\"%s\">", "uploader", "uploader", "psp", "sv", ver);
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "ticket", b64, "ticket");

    ret = np_npid_to_jid_str(&jid, npid);
    if (ret < 0)
        goto fail;
    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "ticketjid", jid, "ticketjid");
    np_free(jid);
    jid = NULL;

    if (quota == 1)      qstr = "sys";
    else if (quota == 2) qstr = "game";
    else if (quota == 3) qstr = "premo";
    else { ret = (int)0x80550713; goto fail; }

    p += sceNet_lib_CF705E46(p, "<%s>%s</%s>", "quota", qstr, "quota");
    p += sceNet_lib_CF705E46(p, "<%s>%u</%s>", "start", start, "start");
    p += sceNet_lib_CF705E46(p, "<%s>%u</%s>", "list-max", max_num, "list-max");
    p += sceNet_lib_CF705E46(p, "</%s>", "uploader");
    *out_xml = xml;
    ret = (int)(p - xml);
    xml = NULL;

fail:
    if (jid != NULL) np_free(jid);
    if (b64 != NULL) np_free(b64);
    if (xml != NULL) np_free(xml);
    return ret;
}

/* =========================================================================
 * Streaming XML Response Readers
 * ========================================================================= */

static int np_read_and_parse_xml(int *trans, void *parser, u32 total_len)
{
    void *buf = np_alloc(256);
    int ret = (int)0x80550703;
    u32 got = 0;

    if (buf == NULL)
        return ret;

    ret = 0;
    while (total_len != 0) {
        u32 chunk = (total_len > 256u) ? 256u : total_len;
        ret = np_http_read_exact(trans, buf, chunk, &got);
        if (ret < 0)
            break;
        if (got == 0) {
            ret = (int)0x80550708;
            break;
        }
        ret = np_xml_feed(parser, buf, got);
        if (ret < 0)
            break;
        total_len -= got;
    }

    np_free(buf);
    return ret;
}

static int np_parse_searchjid_resp(int *trans, void *npid_out, u32 total_len)
{
    u32 ctx[4];
    u8 parser[848];
    int ret;

    sceKernelMemset(ctx, 0, 8);
    ctx[0] = (u32)npid_out;
    ret = np_xml_searchjid_init(parser, ctx);
    if (ret < 0)
        return ret;
    ret = np_read_and_parse_xml(trans, parser, total_len);
    if (ret >= 0)
        ret = np_xml_searchjid_check(ctx);
    return ret;
}

static int np_parse_profile_resp(int *trans, void *prof_out, int *ptlp_out,
                                 int *upd_out, u64 *date_out, u32 total_len)
{
    u32 ctx[8];
    u8 parser[848];
    int mode;
    int ret;

    sceKernelMemset(ctx, 0, sizeof(ctx));
    ctx[1] = (u32)prof_out;
    if (trans[13] == 2)
        mode = 2;
    else if (trans[13] == 1 || trans[13] == 25)
        mode = 1;
    else
        return (int)0x80550711;

    ret = np_xml_profile_init(parser, mode, ctx);
    if (ret < 0)
        return ret;
    ret = np_read_and_parse_xml(trans, parser, total_len);
    if (ret >= 0) {
        ret = np_xml_profile_check(ctx);
        if (ret >= 0) {
            if (ptlp_out != NULL) *ptlp_out = (int)ctx[3];
            if (upd_out != NULL)  *upd_out  = (int)ctx[2];
            if (date_out != NULL) {
                ((u32 *)date_out)[0] = ctx[6];
                ((u32 *)date_out)[1] = ctx[7];
            }
        }
    }
    return ret;
}

static int np_parse_avatar_cat_resp(int *trans, int *total_out, void *entries, int max_num, u32 total_len)
{
    u32 ctx[8];
    u8 parser[848];
    int ret;

    sceKernelMemset(ctx, 0, 20);
    ctx[0] = (u32)entries;
    ctx[3] = (u32)max_num;
    ret = np_xml_avatar_cat_init(parser, ctx);
    if (ret < 0)
        return ret;
    ret = np_read_and_parse_xml(trans, parser, total_len);
    if (ret >= 0) {
        ret = np_xml_avatar_cat_check(ctx);
        if (ret >= 0) {
            if (total_out != NULL)
                *total_out = (int)ctx[1];
            ret = (int)ctx[2];
        }
    }
    return ret;
}

static int np_parse_avatar_list_resp(int *trans, int *total_out, int *ids, void *urls, int max_num, u32 total_len)
{
    u32 ctx[8];
    u8 parser[848];
    int ret;

    sceKernelMemset(ctx, 0, 24);
    ctx[0] = (u32)urls;
    ctx[1] = (u32)ids;
    ctx[4] = (u32)max_num;
    ret = np_xml_avatar_list_init(parser, ctx);
    if (ret < 0)
        return ret;
    ret = np_read_and_parse_xml(trans, parser, total_len);
    if (ret >= 0) {
        ret = np_xml_avatar_list_check(ctx);
        if (ret >= 0) {
            if (total_out != NULL)
                *total_out = (int)ctx[2];
            ret = (int)ctx[3];
        }
    }
    return ret;
}

static int np_parse_uploader_list_resp(int *trans, void **list_out, u32 total_len)
{
    u32 ctx[8];
    u8 parser[848];
    int ret;

    sceKernelMemset(ctx, 0, 24);
    ctx[0] = (u32)list_out;
    ret = np_xml_uploader_list_init(parser, ctx);
    if (ret < 0)
        return ret;
    ret = np_read_and_parse_xml(trans, parser, total_len);
    if (ret >= 0)
        ret = np_xml_uploader_list_check(ctx);
    return ret;
}

static int np_parse_uploader_dl_resp(int *trans, void *info_out, u32 total_len)
{
    u32 ctx[4];
    u8 parser[848];
    int ret;

    sceKernelMemset(ctx, 0, 8);
    ctx[0] = (u32)info_out;
    ret = np_xml_uploader_dl_init(parser, ctx);
    if (ret < 0)
        return ret;
    ret = np_read_and_parse_xml(trans, parser, total_len);
    if (ret >= 0)
        ret = np_xml_uploader_dl_check(ctx);
    return ret;
}

static int np_parse_searchjid_fqdn_resp(int *trans, void **list_out, u32 total_len)
{
    u32 ctx[8];
    u8 parser[848];
    int ret;

    sceKernelMemset(ctx, 0, 28);
    ctx[2] = (u32)list_out;
    ret = np_xml_fqdn_init(parser, 1, NULL, ctx);
    if (ret < 0)
        return ret;
    ret = np_read_and_parse_xml(trans, parser, total_len);
    if (ret >= 0)
        ret = np_xml_fqdn_check(ctx);
    return ret;
}

static int np_fetch_searchjid_fqdn(int *trans)
{
    void *list = NULL;
    u32 xml_len = 0;
    int ret;
    u8 *cur;
    int *user;

    ret = np_http_setup_endpoint(trans, 4, NULL, 0);
    if (ret < 0) goto fail;
    ret = np_http_send_locked(trans, NULL, 0);
    if (ret < 0) goto fail;
    ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret < 0) goto fail;
    ret = np_parse_searchjid_fqdn_resp(trans, &list, xml_len);
    if (ret < 0) goto fail;

    np_trans_close_http(trans);
    cur = (u8 *)list;
    while (cur != NULL) {
        ret = np_user_track_alloc((int *)trans[9], cur);
        if (ret < 0) goto fail;
        cur = *(u8 **)(cur + 12);
    }

    user = (int *)trans[9];
    ret = sceKernelLockLwMutex(user + 8, 1, NULL);
    if (ret < 0) goto fail;
    if (user[22] == 0)
        user[22] = (int)list;
    sceKernelUnlockLwMutex(user + 8, 1);
    return 0;

fail:
    np_free_fqdn_list(list);
    return ret;
}

/* =========================================================================
 * Transaction Creation & Exported Service Functions
 * ========================================================================= */

static int np_create_trans_by_type(int *user, int type, int owner, const void *arg)
{
    void *copy = NULL;
    int ret;

    switch (type) {
    case 1:
        if (sceNet_lib_D8722983((const char *)user + 79) == 0)
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
        if (arg == NULL)
            return (int)0x8055070C;
        len = sceNet_lib_D8722983((const char *)arg) + 1;
        copy = np_alloc((SceSize)len);
        if (copy == NULL)
            return (int)0x80550703;
        sceKernelMemcpy(copy, arg, (SceSize)len);
        break;
    }
    case 20:
    case 26:
        if (arg == NULL)
            return (int)0x8055070C;
        copy = np_alloc(12);
        if (copy == NULL)
            return (int)0x80550703;
        sceKernelMemcpy(copy, arg, 12);
        break;
    default:
        return (int)0x8055070D;
    }

    ret = np_trans_create_obj(user, type, owner, copy);
    if (ret < 0 && copy != NULL)
        np_free(copy);
    return ret;
}

int sceNpService_7EF4312E(const void *online_id, int type, const void *arg)
{
    int *user = NULL;
    int ret = (int)0x80550702;

    if (np_httpxml_is_init() == 1) {
        ret = np_user_acquire(online_id, &user);
        if (ret >= 0) {
            ret = np_create_trans_by_type(user, type, 1, arg);
            if (ret >= 0)
                return ret;
        }
    }
    if (user != NULL)
        np_user_release(user);
    return ret;
}

int np_trans_create_lookup(const void *online_id, int type, const void *arg)
{
    int *user = NULL;
    int ret = (int)0x80550702;

    if (np_httpxml_is_init() == 1) {
        ret = np_user_acquire(online_id, &user);
        if (ret >= 0) {
            ret = np_create_trans_by_type(user, type, 2, arg);
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
    int *user = NULL;
    int ret = (int)0x80550702;

    if (np_httpxml_is_init() == 1) {
        ret = (int)0x80550704;
        if (env != NULL && country != NULL) {
            ret = np_user_acquire(online_id, &user);
            if (ret >= 0) {
                sceNet_lib_B5CE388A((char *)user + 64, env, 14);
                sceNet_lib_B5CE388A((char *)user + 79, country, 2);
                ret = 0;
            }
        }
    }
    if (user != NULL)
        np_user_release(user);
    return ret;
}

int sceNpService_76867C01(int trans_id, void **list_out)
{
    int *trans = NULL;
    void *body = NULL;
    u32 xml_len = 0;
    u32 got = 0;
    u8 sig[20];
    u32 ctx[8];
    u8 parser[848];
    int ret;

    *list_out = NULL;
    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto fail;
    if (trans[13] != 20) { ret = (int)0x80550711; goto fail; }

    ret = np_http_setup_endpoint(trans, 21, (const void *)trans[14], 0);
    if (ret < 0) goto fail;
    ret = np_http_send_locked(trans, NULL, 0);
    if (ret < 0) goto fail;
    ret = np_http_check_resp(trans, 3, NULL, &xml_len, NULL, sig);
    if (ret < 0) goto fail;

    body = np_alloc(xml_len);
    if (body == NULL) { ret = (int)0x80550703; goto fail; }
    ret = np_http_read_exact(trans, body, xml_len, &got);
    if (ret < 0) goto fail;
    if (got != xml_len) { ret = (int)0x80550708; goto fail; }
    ret = np_verify_hmac_sig(body, (int)got, sig, 3);
    if (ret < 0) goto fail;

    np_trans_close_http(trans);
    sceKernelMemset(ctx, 0, 28);
    ctx[2] = (u32)list_out;
    ret = np_xml_fqdn_init(parser, 2, (void *)trans[14], ctx);
    if (ret >= 0) {
        ret = np_xml_feed(parser, body, got);
        if (ret < 0) goto fail;
        ret = np_xml_fqdn_check(ctx);
    }
    if (ret < 0) goto fail;

    np_free(body);
    body = NULL;

    {
        u8 *cur = (u8 *)*list_out;
        while (cur != NULL) {
            ret = np_trans_track_alloc(trans, cur);
            if (ret < 0) goto fail;
            cur = *(u8 **)(cur + 12);
        }
    }

    np_trans_release(trans);
    return 0;

fail:
    np_free_fqdn_list(*list_out);
    *list_out = NULL;
    if (body != NULL)
        np_free(body);
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_00041295(int trans_id, void *npid_out)
{
    int *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int ret;
    const char *online_id;
    int *fqdn;
    int req_len;

    if (npid_out == NULL)
        return (int)0x80550704;

    sceKernelMemset(npid_out, 0, 36);
    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto fail;
    if (trans[13] != 4) { ret = (int)0x80550711; goto fail; }

    online_id = (const char *)trans[14];
    if (online_id == NULL) { ret = (int)0x8055070C; goto fail; }
    if (!np_validate_online_id(online_id)) { ret = (int)0x8055081B; goto fail; }

    if (((int *)trans[9])[22] == 0) {
        ret = np_fetch_searchjid_fqdn(trans);
        if (ret < 0) goto fail;
    }

    fqdn = (int *)((int *)trans[9])[22];
    req_len = np_build_searchjid_xml(&req_xml, online_id);
    if (req_len < 0) { ret = req_len; goto fail; }
    if (req_xml == NULL) { ret = (int)0x80550703; goto fail; }

    while (fqdn != NULL) {
        ret = np_http_setup_endpoint(trans, 5, (const void *)fqdn[0], (u32)req_len);
        if (ret < 0) goto fail;
        ret = np_http_send_locked(trans, req_xml, (u32)req_len);
        if (ret < 0) goto fail;
        if (req_xml != NULL) {
            np_free(req_xml);
            req_xml = NULL;
        }
        ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
        if (ret < 0) goto fail;
        ret = np_parse_searchjid_resp(trans, npid_out, xml_len);
        if (ret == (int)0x8055081B) {
            ret = np_trans_close_http(trans);
            fqdn = (int *)fqdn[3];
            if (fqdn != NULL)
                continue;
        }
        break;
    }

fail:
    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_E00CDEBB(int trans_id, const void *npid, int avatar_size, void *profile_out)
{
    int *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    char country[4];
    int req_len;
    int ret;

    if (profile_out == NULL)
        return (int)0x80550704;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto fail;
    if (trans[13] != 2) { ret = (int)0x80550711; goto fail; }

    sceKernelMemset(profile_out, 0, 4);
    sceKernelMemcpy(country, (const u8 *)npid + 22, 2);
    country[2] = '\0';

    req_len = np_build_get_profile_xml(&req_xml, npid, avatar_size);
    if (req_len < 0) { ret = req_len; goto fail; }
    if (req_xml == NULL) { ret = (int)0x80550703; goto fail; }

    ret = np_http_setup_endpoint(trans, 3, country, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret < 0) goto fail;

    np_free(req_xml);
    req_xml = NULL;

    ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret < 0) goto fail;
    ret = np_parse_profile_resp(trans, profile_out, NULL, NULL, NULL, xml_len);
    if (ret < 0) goto fail;

    ret = np_trans_close_http(trans);
    sceKernelMemcpy(profile_out, npid, 36);

fail:
    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int np_get_self_profile(int trans_id, void *profile_out, int *a3, int *ptlp_out,
                        int *upd_out, u64 *date_out, const u8 *ticket, u32 ticket_len,
                        int avatar_size)
{
    int *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len;
    int ret;

    if (profile_out == NULL || a3 == NULL || ptlp_out == NULL || ticket == NULL)
        return (int)0x80550704;

    sceKernelMemset(profile_out, 0, 304);
    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto fail;
    if (trans[13] != 1) { ret = (int)0x80550711; goto fail; }

    req_len = np_build_self_profile_xml(&req_xml, ticket, ticket_len,
                                        (const char *)trans[9] + 64, avatar_size);
    if (req_len < 0) { ret = req_len; goto fail; }
    if (req_xml == NULL) { ret = (int)0x80550703; goto fail; }

    ret = np_http_setup_endpoint(trans, 1, NULL, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret < 0) goto fail;
    ret = np_parse_profile_resp(trans, profile_out, ptlp_out, upd_out, date_out, xml_len);
    if (ret == (int)0x8055080D) {
        np_trans_close_http(trans);
        ret = np_http_setup_endpoint(trans, 2, NULL, (u32)req_len);
        if (ret < 0) goto fail;
        ret = np_http_send_locked(trans, req_xml, (u32)req_len);
        if (ret < 0) goto fail;
        ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
        if (ret < 0) goto fail;
        ret = np_parse_profile_resp(trans, profile_out, ptlp_out, upd_out, date_out, xml_len);
    }
    if (ret < 0) goto fail;

    if (req_xml != NULL) {
        np_free(req_xml);
        req_xml = NULL;
    }
    np_trans_close_http(trans);
    np_trans_release(trans);
    return 0;

fail:
    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_66B3E216(int trans_id, const char *online_name, const int *langs,
                          const char *about_me, const int *avatar_id, int avatar_size,
                          void *profile_out, int *ptlp_out, int *upd_out, u64 *date_out,
                          const u8 *ticket, u32 ticket_len)
{
    int *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len;
    int ret;

    if (ticket == NULL || profile_out == NULL ||
        (online_name == NULL && langs == NULL && about_me == NULL && avatar_id == NULL))
        return (int)0x8055070C;

    sceKernelMemset(profile_out, 0, 304);
    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto fail;
    if (trans[13] != 25) { ret = (int)0x80550711; goto fail; }

    req_len = np_build_update_profile_xml(&req_xml, ticket, ticket_len,
                                          (const char *)trans[9] + 64,
                                          online_name, langs, about_me, avatar_id, avatar_size);
    if (req_len < 0) { ret = req_len; goto fail; }
    if (req_xml == NULL) { ret = (int)0x80550703; goto fail; }

    ret = np_http_setup_endpoint(trans, 24, NULL, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret < 0) goto fail;
    ret = np_parse_profile_resp(trans, profile_out, ptlp_out, upd_out, date_out, xml_len);
    if (ret < 0) goto fail;

    if (req_xml != NULL) {
        np_free(req_xml);
        req_xml = NULL;
    }
    np_trans_close_http(trans);
    np_trans_release(trans);
    return 0;

fail:
    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_9CD89895(int trans_id, int lang, int start, void *entries, int max_num, int *total_out)
{
    int *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len;
    int ret;

    if (entries == NULL || max_num == 0)
        return (int)0x80550704;

    sceKernelMemset(entries, 0, (SceSize)(408 * max_num));
    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto fail;
    if (trans[13] != 23) { ret = (int)0x80550711; goto fail; }

    req_len = np_build_avatar_cat_xml(&req_xml, lang, start, max_num);
    if (req_len < 0) { ret = req_len; goto fail; }
    if (req_xml == NULL) { ret = (int)0x80550703; goto fail; }

    ret = np_http_setup_endpoint(trans, 22, NULL, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret < 0) goto fail;
    ret = np_parse_avatar_cat_resp(trans, total_out, entries, max_num, xml_len);
    if (ret < 0) goto fail;

    if (req_xml != NULL) {
        np_free(req_xml);
        req_xml = NULL;
    }
    np_trans_close_http(trans);
    np_trans_release(trans);
    return ret;

fail:
    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_570DB15B(int trans_id, const int *cat_id, int size, int start,
                          int *ids, void *urls, int max_num, int *total_out)
{
    int *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len;
    int ret;

    if (ids == NULL || urls == NULL)
        return (int)0x8055070C;
    if (max_num == 0)
        return (int)0x80550704;

    sceKernelMemset(ids, 0, (SceSize)(4 * max_num));
    sceKernelMemset(urls, 0, (SceSize)(max_num << 7));
    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto fail;
    if (trans[13] != 24) { ret = (int)0x80550711; goto fail; }

    req_len = np_build_avatar_list_xml(&req_xml, cat_id, size, start, max_num);
    if (req_len < 0) { ret = req_len; goto fail; }
    if (req_xml == NULL) { ret = (int)0x80550703; goto fail; }

    ret = np_http_setup_endpoint(trans, 23, NULL, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret < 0) goto fail;
    ret = np_parse_avatar_list_resp(trans, total_out, ids, urls, max_num, xml_len);
    if (ret < 0) goto fail;

    if (req_xml != NULL) {
        np_free(req_xml);
        req_xml = NULL;
    }
    np_trans_close_http(trans);
    np_trans_release(trans);
    return ret;

fail:
    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int np_trans_download_url(int trans_id, const char *url, u32 max_size, u32 *total_out,
                          u32 chunk_size, void *buf, int opt)
{
    int *trans = NULL;
    u32 *dl_state;
    u32 got = 0;
    int ret;

    if (opt != 0)
        return (int)0x80550704;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto done;

    if (trans[14] == 0) {
        if ((u32)(trans[13] - 27) >= 2u) { ret = (int)0x80550711; goto done; }
        dl_state = (u32 *)np_alloc(16);
        if (dl_state == NULL) { ret = (int)0x80550703; goto done; }
        sceKernelMemset(dl_state, 0, 16);
        dl_state[0] = 0;
        ret = np_trans_track_alloc(trans, dl_state);
        if (ret < 0) {
            np_free(dl_state);
            goto done;
        }
        trans[14] = (int)dl_state;
    } else {
        dl_state = (u32 *)trans[14];
    }

    if (dl_state[0] == 3u) {
        ret = (int)dl_state[3];
        goto done;
    }
    if (dl_state[0] == 2u) {
        if (total_out != NULL)
            *total_out = dl_state[1];
        ret = 0;
        goto done;
    }

    if (dl_state[0] == 0u) {
        int status = 0;
        u64 clen = 0;
        int rid;

        if (url == NULL) { ret = (int)0x8055070C; goto done; }
        ret = np_http_open_url(trans, url);
        if (ret < 0) goto done;
        rid = sceHttpCreateRequestWithURL(trans[3], 0, url, 0);
        if (rid < 0) { ret = rid; goto done; }
        trans[4] = rid;
        ret = np_http_send_locked(trans, NULL, 0);
        if (ret < 0) goto done;
        ret = sceHttpGetStatusCode(trans[4], &status);
        if (ret < 0) goto done;
        if (status == 200) {
            ret = sceHttpGetContentLength(trans[4], &clen);
            if (ret < 0) goto done;
        } else if (trans[13] == 28 && status == 404) {
            chunk_size = 0;
            clen = 0;
        } else {
            ret = (int)0x8055070A;
            goto done;
        }
        if (clen > (u64)max_size) { ret = (int)0x80550709; goto done; }
        dl_state[2] = (u32)clen;
        dl_state[1] = (u32)clen;
        dl_state[0] = 1u;
    }

    if (dl_state[0] == 1u && chunk_size != 0) {
        if (dl_state[2] < chunk_size)
            chunk_size = dl_state[2];
        if (buf == NULL) { ret = (int)0x8055070C; goto done; }
        ret = np_http_read_exact(trans, buf, chunk_size, &got);
        if (ret < 0) goto done;
        if (got != chunk_size) { ret = (int)0x8055070A; goto done; }
        dl_state[2] -= chunk_size;
    }

    if (dl_state[2] == 0) {
        np_trans_close_http(trans);
        dl_state[0] = 2u;
    }
    if (total_out != NULL)
        *total_out = dl_state[1];
    ret = (int)got;

done:
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_43E635D2(int trans_id)
{
    int *trans = NULL;
    int ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0) {
        trans[6] = 1;
        for (;;) {
            ret = sceKernelLockLwMutex(trans + 15, 1, NULL);
            if (ret < 0)
                break;
            ret = 0;
            if (trans[4] > 0)
                ret = sceHttpAbortRequest(trans[4]);
            sceKernelUnlockLwMutex(trans + 15, 1);
            if (ret >= 0 || trans[7] != 1)
                break;
            sceKernelDelayThread(500000);
        }
    }
    if (trans != NULL)
        np_trans_release(trans);
    return ret;
}

int sceNpService_D7A0CD2D(int trans_id, const u8 *ticket, u32 ticket_len,
                          const void *npid, int quota, void **list_out,
                          u32 start, u32 max_num)
{
    int *trans = NULL;
    char *req_xml = NULL;
    u32 xml_len = 0;
    int req_len;
    int ret;

    if (ticket == NULL || npid == NULL || list_out == NULL)
        return (int)0x80550704;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto fail;
    if (trans[13] != 22) { ret = (int)0x80550711; goto fail; }

    req_len = np_build_uploader_list_xml(&req_xml, ticket, ticket_len, npid, quota, start, max_num);
    if (req_len < 0) { ret = req_len; goto fail; }
    if (req_xml == NULL) { ret = (int)0x80550703; goto fail; }

    ret = np_http_setup_endpoint(trans, 16, NULL, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_send_locked(trans, req_xml, (u32)req_len);
    if (ret < 0) goto fail;
    ret = np_http_check_resp(trans, 1, NULL, &xml_len, NULL, NULL);
    if (ret < 0) goto fail;
    ret = np_parse_uploader_list_resp(trans, list_out, xml_len);
    if (ret < 0) goto fail;

    {
        u8 *cur = (u8 *)*list_out;
        while (cur != NULL) {
            ret = np_trans_track_alloc(trans, cur);
            if (ret < 0) goto fail;
            cur = *(u8 **)(cur + 144);
        }
    }

    if (req_xml != NULL) {
        np_free(req_xml);
        req_xml = NULL;
    }
    np_trans_close_http(trans);
    np_trans_release(trans);
    return 0;

fail:
    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpService_AD2C0674(int trans_id, const u8 *ticket, u32 ticket_len,
                          const void *npid, const char *ref, void *buf,
                          u64 req_size, void *info_out, int opt)
{
    int *trans = NULL;
    u8 *dl_state = NULL;
    char *req_xml = NULL;
    u64 extra_len = 0;
    u32 xml_len = 0;
    u32 got = 0;
    int read_total = 0;
    int ret;

    if (ticket == NULL || npid == NULL || info_out == NULL || ref == NULL)
        return (int)0x8055070C;
    if (req_size == 0 && buf == NULL)
        return (int)0x8055070C;
    if (opt != 0)
        return (int)0x80550704;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0) goto done;
    if (trans[13] != 22) { ret = (int)0x80550711; goto done; }

    if (trans[14] == 0) {
        dl_state = (u8 *)np_alloc(96);
        if (dl_state == NULL) { ret = (int)0x80550703; goto done; }
        sceKernelMemset(dl_state, 0, 96);
        *(u32 *)dl_state = 0;
        ret = np_trans_track_alloc(trans, dl_state);
        if (ret < 0) {
            np_free(dl_state);
            dl_state = NULL;
            goto done;
        }
        trans[14] = (int)dl_state;
    } else {
        dl_state = (u8 *)trans[14];
    }

    if (*(u32 *)dl_state == 3u) {
        ret = *(int *)(dl_state + 88);
        goto done;
    }
    if (*(u32 *)dl_state == 2u) {
        ret = (int)0x80550712;
        goto done;
    }

    if (*(u32 *)dl_state == 0u) {
        char country[8];
        int req_len;
        u32 sep_len;

        sceKernelMemset(country, 0, sizeof(country));
        sceKernelMemcpy(country, ref, 2);
        req_len = np_build_uploader_dl_xml(&req_xml, ticket, ticket_len, npid, ref);
        if (req_len < 0) { ret = req_len; goto done; }
        if (req_xml == NULL) { ret = (int)0x80550703; goto done; }

        ret = np_http_setup_endpoint(trans, 15, country, (u32)req_len);
        if (ret < 0) goto done;
        ret = np_http_send_locked(trans, req_xml, (u32)req_len);
        if (ret < 0) goto done;
        np_free(req_xml);
        req_xml = NULL;

        ret = np_http_check_resp(trans, 1, &extra_len, &xml_len, NULL, NULL);
        if (ret < 0) goto done;
        ret = np_parse_uploader_dl_resp(trans, info_out, xml_len);
        if (ret < 0) goto done;

        sep_len = (u32)sceNet_lib_D8722983("\r\n\r\n");
        ret = np_http_expect_bytes(trans, "\r\n\r\n", sep_len);
        if (ret < 0) goto done;

        extra_len -= sep_len;
        *(u32 *)dl_state = 1u;
        *(u64 *)(dl_state + 80) = (u32)extra_len;
        *(u32 *)(dl_state + 8) = (u32)extra_len;
        *(u32 *)(dl_state + 12) = 0;
    }

    if (req_size != 0 && *(u32 *)dl_state == 1u) {
        u64 rem = *(u64 *)(dl_state + 80);
        if (rem < req_size)
            req_size = rem;
        ret = np_http_read_exact(trans, buf, (u32)req_size, &got);
        if (ret < 0) goto done;
        if (got != (u32)req_size || (req_size >> 32) != 0) {
            ret = (int)0x8055070A;
            goto done;
        }
        *(u64 *)(dl_state + 80) = rem - got;
        read_total = (int)got;
    }

    if (*(u64 *)(dl_state + 80) == 0) {
        np_trans_close_http(trans);
        *(u32 *)dl_state = 2u;
    }
    ret = read_total;

done:
    if (req_xml != NULL)
        np_free(req_xml);
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        if (ret < 0 && dl_state != NULL) {
            *(u32 *)dl_state = 3u;
            *(s64 *)(dl_state + 88) = ret;
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
