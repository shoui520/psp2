/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include <pspkernel.h>
#include <pspkerneltypes.h>
#include <string.h>

#define SCE_PSHEET_ERROR_SEMAID 0x80510108
#define SCE_PSHEET_ERROR_ILLEGAL_ADDRESS 0x80510109
#define SCE_PSHEET_ERROR_P 0x80510111
#define SCE_PSHEET_ERROR_P2 0x80510110
#define SCE_PSHEET_ERROR_P3 0x80510102
#define SCE_PSHEET_ERROR_P4 0x80510300
#define SCE_PSHEET_ERROR_P5 0x80510101
#define SCE_PSHEET_ERROR_P6 0x80510110
#define SCE_PSHEET_ERROR_P7 0x80510113
#define SCE_PSHEET_ERROR_P8 0x80510112

#ifndef IOASSIGN_RDONLY
#define IOASSIGN_RDONLY PSP_O_RDONLY
#endif

static inline int _getK1(void) { int k1; __asm__ volatile ("move %0, $k1" : "=r"(k1)); return k1; }
static inline void _setK1(int k1) { __asm__ volatile ("move $k1, %0" : : "r"(k1)); }
#define pspSdkGetK1() _getK1()
#define pspSdkSetK1(k1) _setK1(k1)

extern void *sceKernelMemset(void *dst, int val, SceSize size);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize size);
extern int sceKernelIsToolMode(void);
extern int sceDrmBBCipherInit(void *ckey, int type, int mode, void *header_key, void *version_key, u32 seed);
extern int sceDrmBBCipherUpdate(void *ckey, void *buf, int size);
extern int sceDrmBBCipherFinal(void *ckey);
extern int sceDrmBBMacInit(void *mkey, int type);
extern int sceDrmBBMacUpdate(void *mkey, void *buf, int size);
extern int sceDrmBBMacFinal(void *mkey, void *buf, void *vkey);
extern int sceDrmBBMacFinal2(void *mkey, void *out, void *vkey);
extern int sceUtilsPrepareGetLoadModuleSLength(void *buf, int size, void *out);
extern int sceUtilsPrepareGetLoadModuleRLength(void *buf, int size, void *out);

SceUID semaID = 0;          // 00002a40
SceUID unk1 = 0;            // 00002a44
SceUInt semaTimeout = 0;    // 00002a48
int unk3 = 0;               // 00002a4c
int buf[4] = {0, 0, 0, 0};  // 00002a50 - 00002a5c
int something = 0;          // 00002a60

u8 g_unkP[0x4D0];           // 0x00002A80
u8 cipher[0x60];            // 0x00002F20
int cipher0;                // 0x00002AA0
int cipher1;                // 0x00002AA4
int p;                      // 0x00002E80
int p0;                     // 0x00002E84
int p1;                     // 0x00002E88
int p2;                     // 0x00002E8C
int p3;                     // 0x00002E90
int p4;                     // 0x00002EA0
int p5;                     // 0x00002EB0
int p6;                     // 0x00002EC0
int p7;                     // 0x00002EC8
int p8;                     // 0x00002ED0
int p9;                     // 0x00002ED4
int p10;                    // 0x00002ED8
int p11;                    // 0x00002EF0
int p12;                    // 0x00002F08
u8 p13[0x60];               // 0x00002F20
int p14;                    // 0x00002F48
int p15;                    // 0x00002F4C
int p16;                    // 0x00003284
int p17;                    // 0x000032DC
int p18;                    // 0x00003804
int p19;                    // 0x00004804

int getSemaIdStatus(SceUInt *sTimeout);
int getSemaIdStatusIncrement(SceUInt *sTimeout);
int getSemaIdStatusUsingTimeout(int timeout, int a1);
int getSemaIdStatusUsingBuf(void);
int sub_01410(int a0);
int sub_014A8(char *file);
int sub_015D8(SceUID a0, int a1);
int sub_72C(int a0, int a1, void *a2);
int sub_864(int a0, int a1, int a2, void *a3);
int sub_2134(void *a0, int a1, int a2, void *a3, void *t0);
int sub_21F0(SceUID fd, int offset, int a2, void *a3, void *t0);
int sub_17C0(int arg1, int arg2, int arg3, void *arg4, int arg5);
void sub_1998(int arg1, int arg2, void *arg3, int arg4);
void sub_1B3C(int arg1, int arg2, int arg3, void *arg4, int arg5);

// scePsheet_driver_302AB4B8 = sceDRMInstallInit
int sceDRMInstallInit(int address, int size)
{
    int k1 = pspSdkGetK1();
    pspSdkSetK1(k1 << 11);
    sceKernelMemset(g_unkP, 0, 0x4D0);

    if (((address | size | (address + size)) & k1) < 0) {
        pspSdkSetK1(k1);
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }

    getSemaIdStatusUsingTimeout(0, 1);
    sub_01410(0);
    buf[0] = address;
    buf[1] = size;

    if (((address < 1) | ((0x0001FFFF < size) ^ 1)) != 0) {
        buf[0] = 0;
        buf[1] = 0;
        pspSdkSetK1(k1);
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }

    pspSdkSetK1(k1);
    return 0;
}

// scePsheet_driver_15355B0E = sceDRMInstallGetPkgInfo
int sceDRMInstallGetPkgInfo(int address, int size, int a2)
{
    int crntAddress = address;
    SceUID s3;
    int k1 = pspSdkGetK1();

    if (buf[0] != 0) {
        if (((address < 1) | (size < 1)) != 0)
            return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        if (a2 == 0)
            return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        pspSdkSetK1(k1 << 11);
        if (!(((k1 << 11) & address) < 0)) {
            if (!((((size + 16) | size) & (k1 << 11)) < 0)) {
                if ((((a2 + 16) | a2) & (k1 << 11)) < 0) {
                    crntAddress = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
                    goto end;
                }
                sub_01410(0);
                s3 = sub_014A8((char *)address);
                if (s3 < 0) {
                    crntAddress = s3;
                    goto end;
                }
                crntAddress = sub_72C(s3, size, (void *)a2);
                sceIoClose(s3);
                goto end;
            }
            crntAddress = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
            goto end;
        }
        crntAddress = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
end:
        sceKernelMemset(g_unkP, 0, 0x4D0);
        pspSdkSetK1(k1);
        return crntAddress;
    }
    return SCE_PSHEET_ERROR_SEMAID;
}

// scePsheet_driver_34E68A41 = sceDRMInstallGetFileInfo
int sceDRMInstallGetFileInfo(char *file, int size, int a2, int a3)
{
    SceUID s0;
    int k1;
    SceUID s4;

    if (buf[0] != 0) {
        if (file == NULL || size == 0 || a2 == 0)
            return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
        k1 = pspSdkGetK1();
        if (!(a3 < 0)) {
            if (!(((k1 << 11) & (int)file) < 0)) {
                if (!((((size + 16) | size) & size) < 0)) {
                    if ((((a3 + 264) | a3) & (k1 << 11)) >= 0) {
                        pspSdkSetK1(k1);
                        s4 = sceIoOpen(file, IOASSIGN_RDONLY, 0);
                        s0 = k1 << 11;
                        pspSdkSetK1(s0);
                        s0 = s4;
                        if (!(s4 < 0)) {
                            sub_01410(0);
                            s0 = sub_864(s4, size, a2, (void *)a3);
                            sceIoClose(s4);
                            goto end;
                        }
                        goto end;
                    }
                    s0 = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
                    goto end;
                }
                s0 = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
                goto end;
            }
            pspSdkSetK1(k1 << 11);
            s0 = SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
end:
            sceKernelMemset(g_unkP, 0, 0x4D0);
            pspSdkSetK1(k1);
            return s0;
        }
        return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
    }
    return SCE_PSHEET_ERROR_SEMAID;
}

int sceDRMInstallInstall(char a0[256] __attribute__((unused)), int a1 __attribute__((unused)), int a2 __attribute__((unused)), int *a3 __attribute__((unused)))
{
    if (buf[0] == 0)
        return SCE_PSHEET_ERROR_SEMAID;
    return 0;
}

int module_start(SceSize argc __attribute__((unused)), void *argp __attribute__((unused)))
{
    SceUID s0 = sceKernelCreateSema("ScePsheet1", 0, 1, 1, 0);
    if (!(s0 < 0)) {
        semaID = s0;
        sceDRMInstallInit(0, 0);
        return 0;
    }
    if (unk1 > 0) {
        sceKernelDeleteSema(unk1);
    }
    sceKernelMemset(&semaID, 0, 64);
    sceKernelMemset(g_unkP, 0, 0x4D0);
    return s0 + 1;
}

int module_reboot_before(SceSize args __attribute__((unused)), void *argp __attribute__((unused)))
{
    if (!(semaID < 0)) {
        sceKernelDeleteSema(semaID);
    }
    if (!(unk1 <= 0)) {
        sceKernelDeleteSema(unk1);
    }
    sceKernelMemset(&semaID, 0, 64);
    sceKernelMemset(g_unkP, 0, 0x4D0);
    return 0;
}

// scePsheet_driver_3CEC4078 = sceDRMInstallEnd
int sceDRMInstallEnd(void)
{
    sceDRMInstallInit(0, 0);
    return 0;
}

// scePsheet_driver_3BA93CFA / scePsheet_3BA93CFA
int scePsheet_3BA93CFA(int a0)
{
    int k1 = pspSdkGetK1();
    if ((((a0 + 8) | a0) & (k1 << 11)) >= 0) {
        pspSdkSetK1(k1 << 11);
        int result = getSemaIdStatus((SceUInt *)a0);
        pspSdkSetK1(k1);
        return result;
    }
    pspSdkSetK1(k1);
    return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
}

// scePsheet_driver_226D9099 = sceDRMInstallAbort
void sceDRMInstallAbort(void)
{
    int k1 = pspSdkGetK1();
    pspSdkSetK1(k1 << 11);
    sub_01410(1);
    pspSdkSetK1(k1);
}

int sub_72C(int a0, int a1, void *a2)
{
    int lbuf[4];
    u32 *s1 = (u32 *)a2;
    u8 *s4 = g_unkP;
    sceKernelMemset(s4, 0, 0x4D0);
    sceKernelMemset(s1, 0, sizeof(lbuf));
    lbuf[0] = sub_015D8(a0, a1);
    if (!(lbuf[0] < 0)) {
        sceKernelMemset(lbuf, 0, sizeof(lbuf[0]));
        lbuf[0] = 0x01010100;
        lbuf[2] = 0x01050200;
        sub_1998(a0, a1, lbuf, 2);
        s1[0] = *(u32 *)(s4 + 0x448);
        s1[1] = a1;
        sub_1B3C(a0, a1, lbuf[1], (u8 *)s1 + 8, 7);
        return 0;
    }
    if (!(*(u32 *)s4 != 0)) {
        lbuf[0] = SCE_PSHEET_ERROR_P;
    }
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_package_info", lbuf[0]);
    if (lbuf[0] < 0)
        return 1;
    return lbuf[0];
}

int sub_864(int a0, int a1, int a2, void *a3)
{
    int lbuf[4];
    char *s3 = (char *)a3;
    char *s5;
    u8 *s6 = g_unkP;
    sceKernelMemset(s6, 0, 0x4D0);
    sceKernelMemset(s3, 0, 0x108);
    lbuf[0] = sub_015D8(a0, a1);
    if (!(lbuf[0] < 0)) {
        s5 = s3 + 8;
        sceKernelMemset(lbuf, 0, 8);
        lbuf[0] = 0x01010100;
        sub_1998(a0, a1, lbuf, 1);
        lbuf[0] = 0x80510109;
        if (!(a2 < lbuf[1]))
            goto Loc;
        lbuf[0] = sub_17C0(a0, a1, a2, s5, 0xFF);
        if (lbuf[0] < 0)
            goto Loc;
        lbuf[0] = strcmp(s5, "EBOOT.PBP");
        u32 t5 = *(u32 *)(s6 + 1032);
        if (lbuf[0] != 0) {
            u32 t2 = (t5 + 15) & ~0xFU;
            u32 t4 = (t2 + 1023) & ~0x3FFU;
            u32 t3 = t4 >> 6;
            *(u32 *)(s3 + 4) = t2 + t3 + 144;
            return 0;
        }
        *(u32 *)(s3 + 4) = t5;
        return 0;
    }
Loc:
    Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_file_info", lbuf[0]);
    if (lbuf[0] < 0)
        return 1;
    return SCE_PSHEET_ERROR_P;
}

// sub_0122C
int getSemaIdStatus(SceUInt *sTimeout)
{
    int result = sceKernelWaitSema(semaID, 1, 0);
    if (result < 0)
        return result;
    if (sTimeout) {
        sTimeout[0] = unk3;
        sTimeout[1] = semaTimeout;
    }
    result = sceKernelSignalSema(semaID, 1);
    if (result < 0)
        return result;
    return 0;
}

// sub_012A0
int getSemaIdStatusIncrement(SceUInt *sTimeout)
{
    int result = sceKernelWaitSema(semaID, 1, 0);
    if (result < 0)
        return result;
    semaTimeout += (SceUInt)sTimeout;
    result = sceKernelSignalSema(semaID, 1);
    if (result < 0)
        return result;
    return 0;
}

// sub_01310
int getSemaIdStatusUsingTimeout(int timeout, int a1)
{
    int result = sceKernelWaitSema(semaID, 1, 0);
    if (result < 0)
        return result;
    unk3 = a1;
    semaTimeout = timeout;
    result = sceKernelSignalSema(semaID, 1);
    if (result < 0)
        return result;
    return 0;
}

// sub_01388
int getSemaIdStatusUsingBuf(void)
{
    int result = 0, lckdresult;
    lckdresult = sceKernelWaitSema(semaID, 1, 0);
    if (lckdresult < 0) {
Loc:
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "get_state", lckdresult);
        return lckdresult;
    }
    lckdresult = sceKernelSignalSema(semaID, 1);
    result = buf[3];
    if (lckdresult < 0)
        goto Loc;
    return result;
}

// sub_01410
int sub_01410(int a0)
{
    int result = 0, lckdresult;
    lckdresult = sceKernelWaitSema(semaID, 1, 0);
    if (lckdresult < 0) {
Loc:
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_api_raw.c", "set_state", lckdresult);
        return lckdresult;
    }
    result = buf[3];
    buf[3] = a0;
    lckdresult = sceKernelSignalSema(semaID, 1);
    if (lckdresult < 0)
        goto Loc;
    return result;
}

// sub_014A8
int sub_014A8(char *file)
{
    SceUID fd;
    int result = strncmp(file, "ms0:/", 5);
    if (result == 0) {
        fd = sceIoOpen(file, IOASSIGN_RDONLY, 0);
        if (!(fd < 0)) {
            result = sceIoLseek32(fd, 0, 2);
            if (result < 56)
                goto Loc;
            result = sub_21F0(fd, 0, result - 0x16, &something, 0);
            if (result < 0)
                goto Loc;
            sceIoLseek32(fd, 0, 0);
            return fd;
        }
Loc:
        if (!(fd < 0)) {
            sceIoClose(fd);
        }
        result = (fd >= 0 || fd == (int)0x80510101) ? (int)SCE_PSHEET_ERROR_P2 : fd;
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "open_and_mac_check", result);
        return result;
    }
    return SCE_PSHEET_ERROR_ILLEGAL_ADDRESS;
}

// sub_015D8
int sub_015D8(SceUID a0, int a1)
{
    int result, s1;
    u8 *s3 = g_unkP;
    int temp;
    result = sceIoLseek32(a0, 0, 0);
    if (result < 0) {
exit:
        Kprintf("%s: %s(): error 0x%08X\n", "bbox_access.c", "get_core_header", result);
        if (result < 0)
            a1 = 1;
        if (a1 != 0)
            return result;
        return SCE_PSHEET_ERROR_P6;
    }

    result = sceIoRead(a0, s3, 40);
    if (result < 40)
        goto exit;
    s1 = *(int *)(s3 + 32);
    if (*(int *)(s3 + 36) < (s1 + 96))
        goto exit;
    *(int *)(s3 + 1228) = s1;
    if (sub_21F0(a0, s1, 80, (void *)a1, 0) < 0) {
        result = SCE_PSHEET_ERROR_P7;
        goto exit;
    }
    result = sceIoLseek32(a0, s1, 0);
    if (s1 < result)
        goto exit;
    result = sceIoRead(a0, s3 + 1040, 96);
    if (result < 0)
        goto exit;

    result = sub_2134(s3 + 1056, 64, 0, &something, s3 + 1040);
    if (result < 0)
        goto exit;
    if (*(int *)(s3 + 1088) != 1)
        goto exit;
    s1 = *(int *)(s3 + 1096);
    result = SCE_PSHEET_ERROR_P8;
    if (*(int *)(s3 + 1092) == 0)
        return 0;
    result = sceKernelIsToolMode();
    temp = 2;
    if (result < 0) {
        temp = s1;
        goto exit;
    }
    result = *(int *)(s3 + 1092) & temp;
    if (result == 0) {
        Kprintf("target arch: 0x%08x\n", *(int *)(s3 + 1092));
        result = SCE_PSHEET_ERROR_P8;
        goto exit;
    }
    if (*(int *)(s3 + 1092) != 2) {
        return 0;
    }
    Kprintf("Package for PSP Development TOOL\n");
    return 0;
}

int sub_17C0(int arg1 __attribute__((unused)), int arg2 __attribute__((unused)), int arg3 __attribute__((unused)), void *arg4 __attribute__((unused)), int arg5 __attribute__((unused)))
{
    return 0;
}

void sub_1998(int arg1 __attribute__((unused)), int arg2 __attribute__((unused)), void *arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
}

void sub_1B3C(int arg1 __attribute__((unused)), int arg2 __attribute__((unused)), int arg3 __attribute__((unused)), void *arg4 __attribute__((unused)), int arg5 __attribute__((unused)))
{
}

int sub_01F4C(void *a0, int a1, void *a2)
{
    void *s2 = &p12;
    int result = sceDrmBBCipherInit(s2, 2, 1, a2, a2, 0);
    if (!(result < 0)) {
        result = sceDrmBBCipherUpdate(s2, a0, a1);
        if (!(result < 0)) {
            result = sceDrmBBCipherFinal(s2);
            if (!(result < 0)) {
                return 0;
            }
        }
    }
    Kprintf("%s: %s(): error 0x%08X\n\n", "encrypt_wrapper.c", "buf_encrypt", result);
    return result;
}

int sub_2000(void *a0, int a1, void *a2, void *a3)
{
    void *s2 = p13;
    int result = sceDrmBBMacInit(s2, 2);
    if (!(result < 0)) {
        result = sceDrmBBMacUpdate(s2, a0, a1);
        if (result < 0)
            goto exit;
        result = sceDrmBBMacFinal(s2, a3, a2);
        if (result < 0)
            goto exit;
        return 0;
    }
exit:
    Kprintf("%s: %s(): error 0x%08X\n\n", "encrypt_wrapper.c", "create_mac", result);
    return SCE_PSHEET_ERROR_P3;
}

int sub_2134(void *a0, int a1, int a2, void *a3, void *t0)
{
    void *s2 = cipher;
    int result = sceDrmBBCipherInit(s2, 1, 2, t0, a3, a2);
    if (!(result < 0)) {
        result = sceDrmBBCipherUpdate(s2, a0, a1);
        if (result < 0)
            goto exit;
        result = sceDrmBBCipherFinal(s2);
        if (result < 0)
            goto exit;
        return 0;
    }
exit:
    Kprintf("%s: %s(): error 0x%08X\n\n", "decrypt_wrapper.c", "drmbb_decrypt", result);
    return SCE_PSHEET_ERROR_P3;
}

int sub_21F0(SceUID fd __attribute__((unused)), int offset __attribute__((unused)), int a2 __attribute__((unused)), void *a3 __attribute__((unused)), void *t0 __attribute__((unused)))
{
    return 0;
}
