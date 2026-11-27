/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include "psheet_int.h"

/* 0x2134: drmbb_decrypt */
int drmbb_decrypt(void *buf, int size, int seed, void *vkey, void *headerKey)
{
    void *ctx = g_work.decryptCtx;
    int result = sceDrmBBCipherInit(ctx, 1, 2, headerKey, vkey, seed);
    if (result >= 0) {
        result = sceDrmBBCipherUpdate(ctx, buf, size);
        if (result >= 0) {
            result = sceDrmBBCipherFinal(ctx);
            if (result >= 0) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "decrypt_wrapper.c", "drmbb_decrypt", result);
    return SCE_PSHEET_ERROR_CRYPTO_FAILED;
}

/* 0x21f0: verify_file_mac */
int verify_file_mac(SceUID pkgFd, int offset, int size, void *vkey, void *mac)
{
    void *mkey = g_work.macCtx;
    int res;
    do {
        res = sceIoLseek32(pkgFd, offset, PSP_SEEK_SET);
        if (res < offset) {
            break;
        }
        res = sceDrmBBMacInit(mkey, 1);
        if (res < 0) {
            break;
        }
        u32 done = 0;
        int failed = 0;
        while (done < (u32)size) {
            u32 chunk = (u32)size - done;
            if (chunk > g_state.workBufSize) {
                chunk = g_state.workBufSize;
            }
            res = sceIoRead(pkgFd, g_state.workBuf, chunk);
            if (res < (int)chunk) {
                failed = 1;
                break;
            }
            res = sceDrmBBMacUpdate(mkey, g_state.workBuf, chunk);
            if (res < 0) {
                failed = 1;
                break;
            }
            res = get_state();
            if (res == 1) {
                res = SCE_PSHEET_ERROR_ABORTED;
            }
            if (res < 0) {
                failed = 1;
                break;
            }
            done += g_state.workBufSize;
        }
        if (failed) {
            break;
        }
        if (mac == NULL) {
            res = sceIoRead(pkgFd, g_state.workBuf, 0x10);
            if (res < 0x10) {
                break;
            }
            mac = g_state.workBuf;
        }
        res = sceDrmBBMacFinal2(mkey, mac, vkey);
        if (res >= 0) {
            return 0;
        }
        res = SCE_PSHEET_ERROR_MAC_MISMATCH;
    } while (0);

    Kprintf("%s: error %08x\n", "decrypt_wrapper.c", res);
    if (res == (int)SCE_PSHEET_ERROR_AMCTRL_BASE) {
        return SCE_PSHEET_ERROR_MAC_MISMATCH;
    }
    if (((u32)res & ~0xFFU) == SCE_PSHEET_ERROR_AMCTRL_BASE) {
        return SCE_PSHEET_ERROR_CRYPTO_FAILED;
    }
    return (res < 0) ? res : (int)SCE_PSHEET_ERROR_MAC_MISMATCH;
}
