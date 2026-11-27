/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include "psheet_int.h"

/* 0x1f4c: data_encrypt */
int data_encrypt(void *buf, int size, void *vkey, void *headerKey)
{
    void *ctx = g_work.encryptCtx;
    int result = sceDrmBBCipherInit(ctx, 2, 1, headerKey, vkey, 0);
    if (result >= 0) {
        result = sceDrmBBCipherUpdate(ctx, buf, size);
        if (result >= 0) {
            result = sceDrmBBCipherFinal(ctx);
            if (result >= 0) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "encrypt_wrapper.c", "data_encrypt", result);
    return SCE_PSHEET_ERROR_CRYPTO_FAILED;
}

/* 0x2000: create_mac */
int create_mac(void *buf, int size, void *vkey, void *macOut)
{
    void *ctx = g_work.macCtx;
    int result = sceDrmBBMacInit(ctx, 2);
    if (result >= 0) {
        result = sceDrmBBMacUpdate(ctx, buf, size);
        if (result >= 0) {
            result = sceDrmBBMacFinal(ctx, macOut, vkey);
            if (result >= 0) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "encrypt_wrapper.c", "create_mac", result);
    return SCE_PSHEET_ERROR_CRYPTO_FAILED;
}

/* 0x20c8: encrypt_and_mac */
int encrypt_and_mac(void *buf, int size, void *vkey, void *headerKey, void *macOut)
{
    int res = data_encrypt(buf, size, vkey, headerKey);
    if (res >= 0) {
        res = create_mac(buf, size, vkey, macOut);
        if (res > 0) {
            res = 0;
        }
    }
    return res;
}
