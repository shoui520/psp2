#include "np_service_internal.h"

static int np_lookup_tss_impl(int trans_id, int slot_id, void *buf, u32 buf_size,
                              u32 max_size, u32 *total_out, int *read_out, int opt)
{
    int *trans = NULL;
    char *url = NULL;
    int ret = 0;

    if (opt != 0)
        return 0;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret < 0)
        goto done;

    if (trans[13] != 26 && trans[13] != 28)
        goto done;

    if (trans[13] == 26) {
        const char *env = (const char *)trans[9] + 64;
        const void *title_id = (const void *)trans[14];
        char title_str[16];
        char *tmp;

        trans[13] = 28;
        tmp = (char *)np_alloc(256);
        if (tmp == NULL)
            goto done;
        sceKernelMemset(tmp, 0, 256);
        if (sceNp_E90DFBC4(title_id, title_str, 13) < 0) {
            np_free(tmp);
            goto done;
        }
        url = tmp;
        sceNet_lib_CF705E46(url, "%s://%s%s%s%s%s/%s/%s-%d%s",
                            "https", "a0.ww.", env, ".dl.playstation.net",
                            "/tss/", env, title_str, title_str, slot_id, ".tss");
        trans[14] = 0;
    }

    ret = np_trans_download_url(trans_id, url, max_size, total_out, buf_size, buf, 0);
    if (ret >= 0 && read_out != NULL)
        *read_out = ret;

done:
    if (trans != NULL)
        np_trans_release(trans);
    if (url != NULL)
        np_free(url);
    return 0;
}

int sceNpLookupNpId(int trans_id, const char *online_id, void *npid_out, int opt)
{
    int *trans = NULL;
    char *copy = NULL;
    int ret;

    if (_sceKernelCheckThreadStack() < 14288)
        return (int)0x80550004;

    if (online_id == NULL || npid_out == NULL) {
        ret = (int)0x8055070C;
        goto done;
    }
    if (opt != 0) {
        ret = (int)0x80550704;
        goto done;
    }

    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0) {
        if (trans[13] != 26) {
            ret = (int)0x80550711;
        } else {
            int len = sceNet_lib_D8722983(online_id) + 1;
            trans[13] = 4;
            copy = (char *)np_alloc((SceSize)len);
            if (copy == NULL) {
                ret = (int)0x80550703;
            } else {
                sceKernelMemcpy(copy, online_id, (SceSize)len);
                ret = np_trans_track_alloc(trans, copy);
                if (ret >= 0) {
                    trans[14] = (int)copy;
                    copy = NULL;
                    ret = sceNpService_00041295(trans_id, npid_out);
                }
            }
        }
    }

done:
    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    if (copy != NULL)
        np_free(copy);
    return ret;
}

int sceNpLookupUserProfile(int trans_id, const void *npid, void *profile_out,
                           void *about_me_out, void *langs_out, void *country_out, int opt)
{
    int *trans = NULL;
    u8 full_prof[304];
    int ret;

    if (_sceKernelCheckThreadStack() < 13984)
        return (int)0x80550004;

    if (npid == NULL)
        return (int)0x8055070C;
    if (opt != 0)
        return (int)0x80550704;

    ret = np_trans_acquire(trans_id, &trans);
    if (ret >= 0) {
        if (trans[13] != 26) {
            ret = (int)0x80550711;
        } else {
            trans[13] = 2;
            ret = sceNpService_E00CDEBB(trans_id, npid, 2, full_prof);
            if (ret >= 0) {
                if (profile_out != NULL)
                    sceKernelMemcpy(profile_out, full_prof, 216);
                if (about_me_out != NULL)
                    sceKernelMemcpy(about_me_out, full_prof + 240, 64);
                if (langs_out != NULL)
                    sceKernelMemcpy(langs_out, full_prof + 220, 16);
                if (country_out != NULL)
                    *(u32 *)country_out = *(u32 *)(full_prof + 216);
            }
        }
    }

    if (trans != NULL) {
        if (trans[6] == 1)
            ret = (int)0x80550707;
        np_trans_release(trans);
    }
    return ret;
}

int sceNpLookupCreateTransactionCtx(const void *title_id)
{
    u8 online_id[304];
    int ret;

    if (_sceKernelCheckThreadStack() < 14016)
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
    if (_sceKernelCheckThreadStack() < 14320)
        return (int)0x80550004;
    return np_trans_destroy_lookup(trans_id);
}

int sceNpLookupAbortTransaction(int trans_id)
{
    if (_sceKernelCheckThreadStack() < 14320)
        return (int)0x80550004;
    return sceNpService_43E635D2(trans_id);
}

int sceNpService_68DE4402(int trans_id, int slot_id, void *buf, u32 buf_size,
                          u32 *total_out, int *read_out, int opt)
{
    if (_sceKernelCheckThreadStack() < 14304)
        return (int)0x80550004;
    if (buf_size > 0x10000u)
        return (int)0x80550704;
    return np_lookup_tss_impl(trans_id, slot_id, buf, buf_size, 0x20000u, total_out, read_out, opt);
}

int sceNpLookupTitleSmallStorage(int trans_id, void *buf, u32 buf_size,
                                 u32 *total_out, int opt)
{
    if (_sceKernelCheckThreadStack() < 14304)
        return (int)0x80550004;
    if (buf_size > 0x10000u)
        return (int)0x80550704;
    return np_lookup_tss_impl(trans_id, 0, buf, buf_size, buf_size, total_out, NULL, opt);
}
