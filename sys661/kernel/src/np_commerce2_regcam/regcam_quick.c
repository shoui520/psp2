#include "regcam_internal.h"

static __attribute__((noinline)) int regcam_quick_gen_oid(u32 (*rand_cb)(void),
                                                          const char *country, const char *lang,
                                                          char *oid_out, u32 oid_buf_size);
static __attribute__((noinline)) int regcam_quick_gen_password(u32 (*rand_cb)(void),
                                                               char *pass_out, u32 pass_buf_size);
static __attribute__((noinline)) int regcam_quick_extract_location(const u8 *resp_buf, u32 resp_size,
                                                                   int has_provinces,
                                                                   char *prov_out, u32 prov_size,
                                                                   char *city_out, u32 city_size,
                                                                   char *postal_out, u32 postal_size);
static __attribute__((noinline)) int regcam_quick_register_card(u32 ctx_id, const char *country,
                                                                const char *email, const char *password,
                                                                const RegCamBillingInfo *addr);

static inline char regcam_alnum_char(u32 idx)
{
    if (idx < 10)
        return (char)('0' + idx);
    if (idx < 36)
        return (char)('A' + (idx - 10));
    return (char)('a' + (idx - 36));
}

int sceNpCommerce2RegCam_D73CC389(u32 ctx_id, u32 (*rand_cb)(void), const char *country,
                                  const char *lang, char *password_out, u32 password_buf_size,
                                  u8 *resp_buf, u32 resp_buf_size, u32 *resp_size_out)
{
    char oid_buf[32];
    char prov_buf[64];
    char city_buf[64];
    char pass_buf[32];
    char email_buf[64];
    char env_buf[16];
    char postal_buf[16];
    char rrn_suffix[16];
    u8 reg_rules[112];
    u32 sess_cursor[12];
    u32 secq_root[12];
    u32 secq_item[12];
    u8 del_buf[2048];
    RegCamBillingInfo addr;
    u32 recv_size = 0;
    u32 del_size = 0;
    u32 sec_q_id = 0;
    int session_open = 0;
    int ret = REGCAM_ERR_INVALID_ARG;

    g_quick_abort_flag = 0;

    do {
        if (!rand_cb || !country || !lang || !password_out ||
            !resp_buf || resp_buf_size == 0 || !resp_size_out) {
            ret = REGCAM_ERR_INVALID_ARG;
            break;
        }
        if (password_buf_size < 31) {
            ret = REGCAM_ERR_INVALID_ARG;
            break;
        }
        if (sceNet_lib_D8722983(country) != 2 || sceNet_lib_D8722983(lang) != 2) {
            ret = REGCAM_ERR_INVALID_ARG;
            break;
        }

        ret = sceNpCommerce2_460A1D92(env_buf, 9);
        if (ret < 0)
            break;
        if (sceNet_lib_B9085A96("np", env_buf, 3) == 0) {
            ret = REGCAM_ERR_QUICK_ENV;
            break;
        }

        ret = regcam_api_set_svc_type(ctx_id, REGCAM_SVC_REG);
        if (ret < 0)
            break;

        /* Start registration session */
        ret = sceNpCommerce2RegCam_06B1824F(ctx_id, &g_quick_req_id);
        if (ret >= 0) {
            ret = sceNpCommerce2RegCam_7A315723(g_quick_req_id, country, lang, 1970, 1, 1);
            if (ret >= 0)
                ret = sceNpCommerce2RegCam_4B1A9E4E(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
            regcam_api_destroy_req(g_quick_req_id);
            g_quick_req_id = 0;
        }
        if (ret < 0)
            break;

        session_open = 1;
        ret = sceNpCommerce2RegCam_856C52E9(sess_cursor, resp_buf, recv_size);
        if (ret >= 0)
            ret = sceNpCommerce2RegCam_BD48E118(sess_cursor, (u32 *)reg_rules);
        if (ret < 0)
            break;
        if (g_quick_abort_flag == 1) {
            ret = REGCAM_ERR_QUICK_ABORTED;
            break;
        }

        /* Fetch security questions */
        ret = sceNpCommerce2RegCam_DB478D24(ctx_id, &g_quick_req_id);
        if (ret >= 0) {
            ret = sceNpCommerce2RegCam_5F45DF28(g_quick_req_id);
            if (ret >= 0)
                ret = sceNpCommerce2RegCam_6EA71511(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
            regcam_api_destroy_req(g_quick_req_id);
            g_quick_req_id = 0;
        }
        if (ret < 0)
            break;

        ret = sceNpCommerce2RegCam_EC918C7E(secq_root, resp_buf, recv_size);
        if (ret >= 0) {
            if (secq_root[10] == 0) {
                ret = REGCAM_ERR_QUICK_NO_SEC_Q;
                break;
            }
            ret = sceNpCommerce2RegCam_FAC30D4E(secq_root, 0, secq_item);
            if (ret >= 0)
                sec_q_id = secq_item[10];
        }
        if (ret < 0)
            break;
        if (g_quick_abort_flag == 1) {
            ret = REGCAM_ERR_QUICK_ABORTED;
            break;
        }

        /* Generate unique login / password / online ID, accept EULA */
        for (;;) {
            for (;;) {
                u32 pass_len;

                ret = regcam_quick_gen_oid(rand_cb, country, lang, oid_buf, 17);
                if (ret < 0)
                    break;
                if (sceNet_lib_D8722983(oid_buf) + 25U > sizeof(email_buf)) {
                    ret = REGCAM_ERR_QUICK_OVERFLOW;
                    break;
                }
                sceNet_lib_CF705E46(email_buf, "%s%s", oid_buf, "@d0.mail.playstation.net");

                ret = regcam_quick_gen_password(rand_cb, pass_buf, 31);
                if (ret < 0)
                    break;
                pass_len = sceNet_lib_D8722983(pass_buf);

                ret = sceNpCommerce2RegCam_B56A1200(ctx_id, &g_quick_req_id);
                if (ret >= 0) {
                    ret = sceNpCommerce2RegCam_13E0AD53(g_quick_req_id, email_buf, pass_buf,
                                                        sec_q_id, pass_buf, pass_len);
                    if (ret >= 0)
                        ret = sceNpCommerce2RegCam_F5E2A7B0(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
                    regcam_api_destroy_req(g_quick_req_id);
                    g_quick_req_id = 0;
                }
                if (ret != REGCAM_ERR_LOGIN_EXISTS)
                    break;
            }
            if (ret < 0)
                break;
            if (g_quick_abort_flag == 1) {
                ret = REGCAM_ERR_QUICK_ABORTED;
                break;
            }

            ret = sceNpCommerce2RegCam_69817297(ctx_id, &g_quick_req_id);
            if (ret >= 0) {
                ret = sceNpCommerce2RegCam_BCFD9CC5(g_quick_req_id);
                if (ret >= 0)
                    ret = sceNpCommerce2RegCam_E256E232(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
            }
            regcam_api_destroy_req(g_quick_req_id);
            g_quick_req_id = 0;
            if (ret < 0)
                break;
            if (g_quick_abort_flag == 1) {
                ret = REGCAM_ERR_QUICK_ABORTED;
                break;
            }

            ret = sceNpCommerce2RegCam_6F0783DA(ctx_id, &g_quick_req_id);
            if (ret >= 0) {
                ret = sceNpCommerce2RegCam_CD9F9F24(g_quick_req_id, oid_buf);
                if (ret >= 0)
                    ret = sceNpCommerce2RegCam_10E44562(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
            }
            regcam_api_destroy_req(g_quick_req_id);
            g_quick_req_id = 0;
            if (ret != REGCAM_ERR_OID_EXISTS)
                break;
        }
        if (ret < 0)
            break;
        if (g_quick_abort_flag == 1) {
            ret = REGCAM_ERR_QUICK_ABORTED;
            break;
        }

        /* Set default name */
        ret = sceNpCommerce2RegCam_B4CC8AE6(ctx_id, &g_quick_req_id);
        if (ret >= 0) {
            u32 first_len = sceNet_lib_D8722983("firstName");
            u32 last_len = sceNet_lib_D8722983("lastName");
            ret = sceNpCommerce2RegCam_EE818631(g_quick_req_id, "firstName", first_len,
                                                "lastName", last_len, NULL, NULL, 0);
            if (ret >= 0)
                ret = sceNpCommerce2RegCam_F6415A7A(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
        }
        regcam_api_destroy_req(g_quick_req_id);
        g_quick_req_id = 0;
        if (ret < 0)
            break;
        if (g_quick_abort_flag == 1) {
            ret = REGCAM_ERR_QUICK_ABORTED;
            break;
        }

        /* Query provinces and build default residential address */
        ret = regcam_api_create_get_prov_req(ctx_id, &g_quick_req_id);
        if (ret >= 0) {
            ret = regcam_api_start_get_prov(g_quick_req_id);
            if (ret >= 0)
                ret = regcam_api_recv_get_prov(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
        }
        regcam_api_destroy_req(g_quick_req_id);
        g_quick_req_id = 0;
        if (ret < 0 && ret != REGCAM_ERR_SERVER_NO_PROV)
            break;

        ret = regcam_quick_extract_location(resp_buf, recv_size, (ret == 0),
                                            prov_buf, sizeof(prov_buf),
                                            city_buf, sizeof(city_buf),
                                            postal_buf, sizeof(postal_buf));
        if (ret < 0)
            break;
        if (g_quick_abort_flag == 1) {
            ret = REGCAM_ERR_QUICK_ABORTED;
            break;
        }

        sceKernelMemset(&addr, 0, sizeof(addr));
        addr.street_a        = "StreetAddress1";
        addr.street_a_len    = sceNet_lib_D8722983("StreetAddress1");
        addr.province        = prov_buf;
        addr.province_len    = sceNet_lib_D8722983(prov_buf);
        addr.city            = city_buf;
        addr.city_len        = sceNet_lib_D8722983(city_buf);
        addr.postal_code     = postal_buf;
        addr.postal_code_len = sceNet_lib_D8722983(postal_buf);

        if (sceNet_lib_B9085A96("US", country, 3) == 0) {
            addr.province        = "CA";
            addr.province_len    = sceNet_lib_D8722983("CA");
            addr.postal_code     = "94404";
            addr.postal_code_len = sceNet_lib_D8722983("94404");
        } else if (sceNet_lib_B9085A96("CA", country, 3) == 0) {
            addr.province        = "ON";
            addr.province_len    = sceNet_lib_D8722983("ON");
            addr.postal_code     = "M2H 3R6";
            addr.postal_code_len = sceNet_lib_D8722983("M2H 3R6");
        }

        ret = sceNpCommerce2RegCam_3328C751(ctx_id, &g_quick_req_id);
        if (ret >= 0) {
            ret = sceNpCommerce2RegCam_461DD73D(g_quick_req_id, &addr);
            if (ret >= 0)
                ret = sceNpCommerce2RegCam_28541A7A(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
        }
        regcam_api_destroy_req(g_quick_req_id);
        g_quick_req_id = 0;
        if (ret < 0)
            break;
        if (g_quick_abort_flag == 1) {
            ret = REGCAM_ERR_QUICK_ABORTED;
            break;
        }

        /* Korea requires resident registration number */
        if (sceNet_lib_B9085A96("KR", country, 3) == 0) {
            sceNet_lib_CF705E46(rrn_suffix, "%07u", rand_cb() % 10000000U);
            ret = sceNpCommerce2RegCam_DF407CA3(ctx_id, &g_quick_req_id);
            if (ret >= 0) {
                ret = sceNpCommerce2RegCam_0EE4DB04(g_quick_req_id, "700101", rrn_suffix);
                if (ret >= 0)
                    ret = sceNpCommerce2RegCam_993C822E(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
            }
            regcam_api_destroy_req(g_quick_req_id);
            g_quick_req_id = 0;
            if (ret < 0)
                break;
        }
        if (g_quick_abort_flag == 1) {
            ret = REGCAM_ERR_QUICK_ABORTED;
            break;
        }

        /* Set opt-in flags and submit registration */
        ret = sceNpCommerce2RegCam_D99AEC05(ctx_id, &g_quick_req_id);
        if (ret >= 0) {
            ret = sceNpCommerce2RegCam_726FF626(g_quick_req_id, 1, 1);
            if (ret >= 0)
                ret = sceNpCommerce2RegCam_FA13A517(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
        }
        regcam_api_destroy_req(g_quick_req_id);
        g_quick_req_id = 0;
        if (ret < 0)
            break;
        if (g_quick_abort_flag == 1) {
            ret = REGCAM_ERR_QUICK_ABORTED;
            break;
        }

        ret = sceNpCommerce2RegCam_64D92E9C(ctx_id, &g_quick_req_id);
        if (ret >= 0) {
            ret = sceNpCommerce2RegCam_130756C8(g_quick_req_id);
            if (ret >= 0)
                ret = sceNpCommerce2RegCam_A2A91596(g_quick_req_id, resp_buf, resp_buf_size, &recv_size);
        }
        regcam_api_destroy_req(g_quick_req_id);
        g_quick_req_id = 0;
        if (ret < 0)
            break;
        if (g_quick_abort_flag == 1) {
            ret = REGCAM_ERR_QUICK_ABORTED;
            break;
        }

        *resp_size_out = recv_size;
        sceNet_lib_80C9F02A(password_out, pass_buf);

        /* Register test card if registration rules require payment verification */
        if (reg_rules[76] & 1) {
            ret = regcam_quick_register_card(ctx_id, country, email_buf, pass_buf, &addr);
            if (ret < 0)
                break;
        }
        if (g_quick_abort_flag == 1)
            ret = REGCAM_ERR_QUICK_ABORTED;
    } while (0);

    if (session_open && g_quick_abort_flag == 0) {
        if (regcam_api_create_del_sess_req(ctx_id, &g_quick_req_id) >= 0 &&
            regcam_api_start_del_sess(g_quick_req_id) >= 0) {
            regcam_api_recv_del_sess(g_quick_req_id, del_buf, sizeof(del_buf), &del_size);
        }
        regcam_api_destroy_req(g_quick_req_id);
        g_quick_req_id = 0;
    }
    return ret;
}

int sceNpCommerce2RegCam_107C4508(void)
{
    if (g_quick_req_id != 0)
        regcam_api_abort_req(g_quick_req_id);
    g_quick_abort_flag = 1;
    return 0;
}

static __attribute__((noinline)) int regcam_quick_gen_oid(u32 (*rand_cb)(void),
                                                          const char *country, const char *lang,
                                                          char *oid_out, u32 oid_buf_size)
{
    u32 flags;
    int ret;

    if (oid_buf_size < 17)
        return REGCAM_ERR_QUICK_OVERFLOW;

    do {
        char *cursor;
        u32 idx;

        if (g_quick_abort_flag == 1)
            return REGCAM_ERR_QUICK_ABORTED;

        sceNet_lib_80C9F02A(oid_out, "Q-");
        cursor = oid_out + sceNet_lib_D8722983("Q-");
        for (idx = 0; idx < 8; idx++)
            *cursor++ = regcam_alnum_char(rand_cb() % 62U);

        *cursor++ = '-';
        *cursor++ = (char)sceNet_lib_C13C9307((u8)country[0]);
        *cursor++ = (char)sceNet_lib_C13C9307((u8)country[1]);
        *cursor++ = '-';
        *cursor++ = (char)sceNet_lib_C13C9307((u8)lang[0]);
        *cursor++ = (char)sceNet_lib_C13C9307((u8)lang[1]);
        *cursor = '\0';

        flags = 0;
        ret = sceNpCommerce2RegCam_5F271C1B(16, oid_out, &flags);
    } while (ret < 0 || flags != 0);

    return ret;
}

static __attribute__((noinline)) int regcam_quick_gen_password(u32 (*rand_cb)(void),
                                                               char *pass_out, u32 pass_buf_size)
{
    u32 flags;
    int ret;

    if (pass_buf_size < 31)
        return REGCAM_ERR_QUICK_OVERFLOW;

    do {
        u32 idx;

        if (g_quick_abort_flag == 1)
            return REGCAM_ERR_QUICK_ABORTED;

        sceKernelMemset(pass_out, 0, pass_buf_size);
        for (idx = 0; idx < 30; idx++)
            pass_out[idx] = regcam_alnum_char(rand_cb() % 62U);

        flags = 0;
        ret = sceNpCommerce2RegCam_5F271C1B(40, pass_out, &flags);
    } while (ret < 0 || flags != 0);

    return ret;
}

static __attribute__((noinline)) int regcam_quick_extract_location(const u8 *resp_buf, u32 resp_size,
                                                                   int has_provinces,
                                                                   char *prov_out, u32 prov_size,
                                                                   char *city_out, u32 city_size,
                                                                   char *postal_out, u32 postal_size)
{
    u32 root_cursor[12];
    u32 prov_info[16];
    u32 city_info[12];
    int ret = 0;

    /* Sony swapped the fill/size arguments to sceKernelMemset here */
    sceKernelMemset(prov_out, (int)prov_size, 0);
    sceNet_lib_B5CE388A(prov_out, "Province", prov_size);
    sceKernelMemset(city_out, (int)city_size, 0);
    sceNet_lib_B5CE388A(city_out, "City", city_size);
    sceKernelMemset(postal_out, (int)postal_size, 0);
    sceNet_lib_B5CE388A(postal_out, "11111", postal_size);

    if (has_provinces) {
        ret = regcam_api_init_prov_result(root_cursor, resp_buf, resp_size);
        if (ret >= 0 && root_cursor[10] != 0) {
            ret = regcam_api_get_prov_info(root_cursor, 0, prov_info);
            if (ret >= 0) {
                const char *prov_name = (const char *)prov_info[11];
                sceNet_lib_B5CE388A(prov_out, prov_name, prov_size);
                if (prov_info[12] != 0) {
                    ret = regcam_api_get_city_info(prov_info, 0, city_info);
                    if (ret >= 0) {
                        const char *city_name = (const char *)city_info[11];
                        sceNet_lib_B5CE388A(city_out, city_name, city_size);
                    }
                }
            }
        }
    }
    return ret;
}

static __attribute__((noinline)) int regcam_quick_register_card(u32 ctx_id, const char *country,
                                                                const char *email, const char *password,
                                                                const RegCamBillingInfo *addr)
{
    RegCamBillingInfo card;
    u8 auth_buf[2048];
    u8 card_buf[2048];
    u32 auth_size;
    u32 card_size;
    int ret;

    ret = regcam_api_set_svc_type(ctx_id, REGCAM_SVC_CAM);
    if (ret < 0)
        return ret;
    if (g_quick_abort_flag == 1)
        return REGCAM_ERR_QUICK_ABORTED;

    ret = sceNpCommerce2RegCam_B140C5D7(ctx_id, &g_quick_req_id);
    if (ret >= 0) {
        ret = sceNpCommerce2RegCam_19679B00(g_quick_req_id, 1, email, password);
        if (ret >= 0)
            ret = sceNpCommerce2RegCam_9C9E0B84(g_quick_req_id, auth_buf, sizeof(auth_buf), &auth_size);
    }
    regcam_api_destroy_req(g_quick_req_id);
    g_quick_req_id = 0;
    if (ret < 0)
        return ret;
    if (g_quick_abort_flag == 1)
        return REGCAM_ERR_QUICK_ABORTED;

    sceKernelMemset(&card, 0, sizeof(card));
    card.payment_method_id     = "VISA";
    card.payment_method_id_len = sceNet_lib_D8722983("VISA");
    card.holder_name           = "firstName lastName";
    card.holder_name_len       = sceNet_lib_D8722983("firstName lastName");
    card.start_year            = 2008;
    card.start_month           = 12;
    card.expire_year           = 2022;
    card.expire_month          = 12;
    card.security_code         = "150";
    if (sceNet_lib_B9085A96("US", country, 3) != 0 &&
        sceNet_lib_B9085A96("CA", country, 3) != 0)
        card.card_number = "4111111111111111";
    else
        card.card_number = "4444444444444448";

    card.street_a        = addr->street_a;
    card.street_a_len    = addr->street_a_len;
    card.city            = addr->city;
    card.city_len        = addr->city_len;
    card.province        = addr->province;
    card.province_len    = addr->province_len;
    card.postal_code     = addr->postal_code;
    card.postal_code_len = addr->postal_code_len;

    ret = regcam_api_create_reg_cc_req(ctx_id, &g_quick_req_id);
    if (ret >= 0) {
        ret = regcam_api_start_reg_cc(g_quick_req_id, &card);
        if (ret >= 0)
            ret = regcam_api_recv_reg_cc(g_quick_req_id, card_buf, sizeof(card_buf), &card_size);
    }
    regcam_api_destroy_req(g_quick_req_id);
    g_quick_req_id = 0;
    return ret;
}
