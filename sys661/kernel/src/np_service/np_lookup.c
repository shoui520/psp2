#include "np_service_internal.h"

static int np_lookup_tss_impl(int trans_id, int slot_id, void *out_buf, u32 buf_size,
                              u32 max_size, u32 *total_out, int *read_out, int opt)
{
    NpTransCtx *trans = NULL;
    char *url = NULL;
    int ret = 0;

    if (opt == 0) {
        ret = np_trans_acquire(trans_id, &trans);
        if (ret >= 0 && (trans->kind == 26 || trans->kind == 28)) {
            int url_ok = 1;
            if (trans->kind == 26) {
                const char *env = trans->user->env;
                const void *title_id = trans->extra;
                char title_str[16];
                char *allocated;

                trans->kind = 28;
                url_ok = 0;
                allocated = (char *)np_alloc(256);
                if (allocated != NULL) {
                    sceKernelMemset(allocated, 0, 256);
                    if (sceNp_E90DFBC4(title_id, title_str, 13) >= 0) {
                        url = allocated;
                        allocated = NULL;
                        sceNet_lib_CF705E46(url, "%s://%s%s%s%s%s/%s/%s-%d%s",
                                            "https", "a0.ww.", env, ".dl.playstation.net",
                                            "/tss/", env, title_str, title_str, slot_id, ".tss");
                        url_ok = 1;
                    }
                    if (allocated != NULL)
                        np_free(allocated);
                }
                if (url_ok)
                    trans->extra = NULL;
            }
            if (url_ok) {
                ret = np_trans_download_url(trans_id, url, max_size, total_out, buf_size, out_buf, 0);
                if (ret >= 0 && read_out != NULL)
                    *read_out = ret;
            }
        }
    }

    if (trans != NULL)
        np_trans_release(trans);
    if (url != NULL)
        np_free(url);
    return 0;
}

int sceNpLookupNpId(int trans_id, const char *online_id, NpId *npid_out, int opt)
{
    NpTransCtx *trans = NULL;
    char *copy = NULL;
    int ret;

    if (sceKernelCheckThreadStack() < 14288)
        return (int)0x80550004;

    if (online_id == NULL || npid_out == NULL) {
        ret = (int)0x8055070C;
    } else if (opt != 0) {
        ret = (int)0x80550704;
    } else {
        ret = np_trans_acquire(trans_id, &trans);
        if (ret >= 0) {
            if (trans->kind != 26) {
                ret = (int)0x80550711;
            } else {
                int len = sceNet_lib_D8722983(online_id) + 1;
                trans->kind = 4;
                copy = (char *)np_alloc((SceSize)len);
                if (copy == NULL) {
                    ret = (int)0x80550703;
                } else {
                    sceKernelMemcpy(copy, online_id, (SceSize)len);
                    ret = np_trans_track_alloc(trans, copy);
                    if (ret >= 0) {
                        trans->extra = copy;
                        copy = NULL;
                        ret = sceNpService_00041295(trans_id, npid_out);
                    }
                }
            }
        }
    }

    if (trans != NULL) {
        if (trans->aborted == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    if (copy != NULL)
        np_free(copy);
    return ret;
}

int sceNpLookupUserProfile(int trans_id, const NpId *npid, NpProfileBase *profile_out,
                           NpAboutMe *about_me_out, NpLangArray *langs_out,
                           u32 *country_out, int opt)
{
    NpTransCtx *trans = NULL;
    NpProfileRecord full_prof;
    int ret;

    if (sceKernelCheckThreadStack() < 13984)
        return (int)0x80550004;

    if (npid == NULL) {
        ret = (int)0x8055070C;
    } else if (opt != 0) {
        ret = (int)0x80550704;
    } else {
        ret = np_trans_acquire(trans_id, &trans);
        if (ret >= 0) {
            if (trans->kind != 26) {
                ret = (int)0x80550711;
            } else {
                trans->kind = 2;
                ret = sceNpService_E00CDEBB(trans_id, npid, 2, &full_prof);
                if (ret >= 0) {
                    if (profile_out != NULL)
                        *profile_out = full_prof.base;
                    if (about_me_out != NULL)
                        *about_me_out = full_prof.about_me;
                    if (langs_out != NULL)
                        *langs_out = full_prof.languages;
                    if (country_out != NULL)
                        *country_out = full_prof.country_code;
                }
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

int sceNpLookupCreateTransactionCtx(const void *title_id)
{
    u8 online_id[304];
    int ret;

    if (sceKernelCheckThreadStack() < 14016)
        return (int)0x80550004;

    ret = sceNp_F2B95034(online_id, NULL, NULL, NULL, NULL);
    if (ret >= 0) {
        ret = np_trans_create_lookup(online_id, 26, title_id);
        if (ret == (int)0x8055070F)
            return (int)0x80550705;
    }
    return ret;
}

int sceNpLookupDestroyTransactionCtx(int trans_id)
{
    if (sceKernelCheckThreadStack() < 14320)
        return (int)0x80550004;
    return np_trans_destroy_lookup(trans_id);
}

int sceNpLookupAbortTransaction(int trans_id)
{
    if (sceKernelCheckThreadStack() < 14320)
        return (int)0x80550004;
    return sceNpService_43E635D2(trans_id);
}

int sceNpService_68DE4402(int trans_id, int slot_id, void *out_buf, u32 buf_size,
                          u32 *total_out, int *read_out, int opt)
{
    if (sceKernelCheckThreadStack() < 14304)
        return (int)0x80550004;
    if (buf_size > 0x10000u)
        return (int)0x80550704;
    return np_lookup_tss_impl(trans_id, slot_id, out_buf, buf_size, 0x20000u, total_out, read_out, opt);
}

int sceNpLookupTitleSmallStorage(int trans_id, void *out_buf, u32 buf_size,
                                 u32 *total_out, int opt)
{
    if (sceKernelCheckThreadStack() < 14304)
        return (int)0x80550004;
    if (buf_size > 0x10000u)
        return (int)0x80550704;
    return np_lookup_tss_impl(trans_id, 0, out_buf, buf_size, buf_size, total_out, NULL, opt);
}
