/*
 * ePSP 6.61 memlmd_01g.prx (sceMemlmd v1.37)
 */

#include <common_imp.h>
#include <interruptman.h>
#include <sysmem_suspend_kernel.h>
#include <sysmem_sysevent.h>
#include <sysmem_utils_kernel.h>
#include <threadman_kernel.h>

#define KIRK_SET_INCOMPLETE_PROCESSING_ERROR(p) (*(volatile u32 *)0xBDE00008 = (p))
#define KIRK_GET_INCOMPLETE_PROCESSING_ERROR()  (*(volatile u32 *)0xBDE00008)
#define KIRK_SET_PROCESSING_PHASE(p)            (*(volatile u32 *)0xBDE0000C = (p))
#define KIRK_SET_COMMAND(m)                     (*(volatile u32 *)0xBDE00010 = (m))
#define KIRK_GET_PROCESSING_RESULT()            (*(volatile u32 *)0xBDE00014)
#define KIRK_GET_PROCESSING_STATUS_PATTERN()    (*(volatile u32 *)0xBDE0001C)
#define KIRK_SET_INIT_ASYNC_PROCESSING_PATTERN(p) (*(volatile u32 *)0xBDE00020 = (p))
#define KIRK_SET_END_ASYNC_PROCESSING_PATTERN(p)  (*(volatile u32 *)0xBDE00024 = (p))
#define KIRK_SET_END_PROCESSING_PATTERN(p)      (*(volatile u32 *)0xBDE00028 = (p))
#define KIRK_SET_SOURCE_ADDR(a)                 (*(volatile u32 *)0xBDE0002C = (a))
#define KIRK_SET_DEST_ADDR(a)                   (*(volatile u32 *)0xBDE00030 = (a))

#define SYSCONF_GET_BUS_CLOCK_MASK()            (*(volatile u32 *)0xBC100050)
#define SYSCONF_SET_BUS_CLOCK_MASK(mask)        (*(volatile u32 *)0xBC100050 = (mask))

#define KIRK_BUS_CLOCK (1 << 7)
#define KIRK_INTERRUPT 0x18

#define BUS_ENABLE  1
#define BUS_DISABLE 0

#define OP_IDLE       0
#define OP_ASYNC_SEMA 1
#define OP_SYNC_SEMA  2

extern int UtilsForKernel_66463BEC(int arg);

int sceUtilsBufferCopy(u8 *outbuff, u8 *inbuff, int cmd);
int sceUtilsBufferCopyByPolling(u8 *outbuff, u8 *inbuff, int cmd);
int sceUtilsBufferCopyWithRange(u8 *outbuff, int outsize, u8 *inbuff, int insize, int cmd);
int sceUtilsBufferCopyByPollingWithRange(u8 *outbuff, int outsize, u8 *inbuff, int insize, int cmd);
static int CallSysEventHandler(int ev_id, char *ev_name, void *param, int *result);

typedef struct {
    int operation;      /* 0x00 */
    SceUID utils_mutex; /* 0x04 */
    SceUID utils_flag;  /* 0x08 */
    int result;         /* 0x0C */
    int status_pattern; /* 0x10 */
    int kirk_enabled;   /* 0x14 */
} GlobalSysParam;

/* 0x000022A4 */
static const u8 g_kirk11_header[0x28] = {
    0x77, 0x3F, 0x4B, 0xE1, 0x4C, 0x0A, 0xB4, 0x52,
    0x67, 0x2B, 0x67, 0x56, 0x82, 0x4C, 0xCF, 0x42,
    0xAA, 0x37, 0xFF, 0xC0, 0x89, 0x41, 0xE5, 0x63,
    0x5E, 0x84, 0xE9, 0xFB, 0x53, 0xDA, 0x94, 0x9E,
    0x9B, 0xB7, 0xC2, 0xA4, 0x22, 0x9F, 0xDF, 0x1F
};

/* 0x000022CC */
static const u32 g_tags[4] = {
    0x00000000, /* 0x000022CC */
    0x01000000, /* 0x000022D0 */
    0x4C9490F0, /* 0x000022D4 */
    0x4C94A1F0  /* 0x000022D8 */
};

/* 0x000022DC */
static const u8 g_key_22DC[16] = {
    0x71, 0xF6, 0xA8, 0x31, 0x1E, 0xE0, 0xFF, 0x1E,
    0x50, 0xBA, 0x6C, 0xD2, 0x98, 0x2D, 0xD6, 0x2D
};

/* 0x000022EC */
static const u8 g_key_22EC[16] = {
    0xAA, 0x85, 0x4D, 0xB0, 0xFF, 0xCA, 0x47, 0xEB,
    0x38, 0x7F, 0xD7, 0xE4, 0x3D, 0x62, 0xB0, 0x10
};

/* 0x000022FC */
static const u8 g_key_22FC[0x90] = {
    0x6A, 0x19, 0x71, 0xF3, 0x18, 0xDE, 0xD3, 0xA2, 0x6D, 0x3B, 0xDE, 0xC7, 0xBE, 0x98, 0xE2, 0x4C,
    0xE3, 0xDC, 0xDF, 0x42, 0x7B, 0x5B, 0x12, 0x28, 0x7D, 0xC0, 0x7A, 0x59, 0x86, 0xF0, 0xF5, 0xB5,
    0x58, 0xD8, 0x64, 0x18, 0x84, 0x24, 0x7F, 0xE9, 0x57, 0xAB, 0x4F, 0xC6, 0x92, 0x6D, 0x70, 0x29,
    0xD3, 0x61, 0x87, 0x87, 0xD0, 0xAE, 0x2C, 0xE7, 0x37, 0x77, 0xC7, 0x3C, 0x96, 0x7E, 0x21, 0x1F,
    0x65, 0x95, 0xC0, 0x61, 0x57, 0xAC, 0x64, 0xD8, 0x5A, 0x6D, 0x14, 0xD2, 0x9C, 0x54, 0xC6, 0x68,
    0x5D, 0xF5, 0xC3, 0xF0, 0x50, 0xDA, 0xEA, 0x19, 0x43, 0xA7, 0xAD, 0xC3, 0x2A, 0x14, 0xCA, 0xC8,
    0x4C, 0x83, 0x86, 0x18, 0xAE, 0x86, 0x49, 0xFB, 0x4F, 0x45, 0x75, 0xD2, 0xC3, 0xD6, 0xE1, 0x13,
    0x69, 0x37, 0xC6, 0x90, 0xCF, 0xF9, 0x79, 0xA1, 0x77, 0x3A, 0x3E, 0xBB, 0xBB, 0xD5, 0x3B, 0x84,
    0x1B, 0x9A, 0xB8, 0x79, 0xF0, 0xD3, 0x5F, 0x6F, 0x4C, 0xC0, 0x28, 0x87, 0xBC, 0xAE, 0xDA, 0x00
};

/* 0x0000238C */
static const u8 g_key_238C[0x90] = {
    0x50, 0xCC, 0x03, 0xAC, 0x3F, 0x53, 0x1A, 0xFA, 0x0A, 0xA4, 0x34, 0x23, 0x86, 0x61, 0x7F, 0x97,
    0x84, 0x1C, 0x1A, 0x1D, 0x08, 0xD4, 0x50, 0xB6, 0xD9, 0x73, 0x27, 0x80, 0xD1, 0xDE, 0xEE, 0xCA,
    0x49, 0x8B, 0x84, 0x37, 0xDB, 0xF0, 0x70, 0xA2, 0xA6, 0x2B, 0x09, 0x4D, 0x3B, 0x29, 0xDE, 0x0B,
    0xE1, 0x6F, 0x04, 0x7A, 0xC4, 0x18, 0x7A, 0x69, 0x73, 0xBF, 0x02, 0xD8, 0xA1, 0xD0, 0x58, 0x7E,
    0x69, 0xCE, 0xAC, 0x5E, 0x1B, 0x0A, 0xF8, 0x19, 0xE6, 0x9A, 0xC0, 0xDE, 0xA0, 0xB2, 0xCE, 0x04,
    0x43, 0xC0, 0x9D, 0x50, 0x5D, 0x0A, 0xD7, 0xFD, 0xC6, 0x53, 0xAA, 0x13, 0xDD, 0x2C, 0x3B, 0x2B,
    0xBF, 0xAB, 0x7C, 0xF5, 0xA0, 0x4A, 0x79, 0xE3, 0xF1, 0x7B, 0x2E, 0xB2, 0xA3, 0xAC, 0x8E, 0x0A,
    0x38, 0x9B, 0x9E, 0xAA, 0xEC, 0x2B, 0xA3, 0x75, 0x13, 0x75, 0x77, 0x98, 0x6A, 0x66, 0x92, 0x65,
    0xBC, 0x97, 0x80, 0x0E, 0x32, 0x88, 0x9F, 0x64, 0xBA, 0x99, 0x8A, 0x72, 0x96, 0x9F, 0xE1, 0xE0
};

/* 0x0000241C */
static const u8 g_seed_241C[16] = {
    0x09, 0xEE, 0x7E, 0x6D, 0x7A, 0xE9, 0xA6, 0xDE,
    0x79, 0xC0, 0xBA, 0x24, 0xA3, 0x88, 0x1D, 0x14
};

/* 0x0000242C */
static const u8 g_sha1_whitelist[0x104] = {
    0x7B, 0xBC, 0x67, 0x1A, 0x0B, 0x98, 0xA8, 0xDC, 0x00, 0x72, 0x74, 0xB0, 0x32, 0x0D, 0x60, 0xF1, 0x09, 0x2E, 0x34, 0x05,
    0x93, 0x56, 0x47, 0x21, 0xB8, 0xE3, 0x89, 0x11, 0x04, 0x11, 0xC5, 0x1A, 0x94, 0xF5, 0x0A, 0x40, 0xF2, 0xD5, 0x85, 0xBE,
    0x95, 0xC1, 0x44, 0x0B, 0xAC, 0x02, 0xAE, 0x8D, 0x87, 0x6F, 0x18, 0x40, 0x92, 0xC4, 0xDA, 0xE7, 0xD7, 0xF1, 0x91, 0xF1,
    0x5B, 0x71, 0x04, 0xD4, 0xC1, 0xCD, 0x64, 0x33, 0xB4, 0x08, 0x78, 0x86, 0x5B, 0x11, 0x33, 0x63, 0x8F, 0x4F, 0xA8, 0x3E,
    0x83, 0xA1, 0x86, 0x2F, 0x24, 0x78, 0x28, 0x65, 0x95, 0xC1, 0xF5, 0xF8, 0x99, 0x9A, 0x4C, 0x64, 0xA1, 0x7A, 0x66, 0xC2,
    0x62, 0x61, 0xFB, 0x3B, 0xE3, 0x76, 0x60, 0xF1, 0xA7, 0xC7, 0x22, 0x27, 0x7A, 0x9A, 0x1D, 0x70, 0xE8, 0xA1, 0xA5, 0x9F,
    0x8E, 0x5D, 0x4D, 0x03, 0x06, 0xEF, 0x3B, 0xA7, 0xBB, 0x7A, 0x20, 0x6A, 0x26, 0xD6, 0x31, 0x76, 0xC8, 0xB5, 0xAA, 0x87,
    0x15, 0x72, 0xBE, 0x6E, 0xC3, 0x5C, 0x1C, 0xB1, 0x28, 0x31, 0x40, 0x1D, 0xF8, 0xF8, 0x18, 0xBB, 0x92, 0xCF, 0x26, 0x7D,
    0xCE, 0x9D, 0x02, 0xE8, 0xD0, 0x26, 0x76, 0x80, 0x4D, 0x6E, 0x69, 0xC0, 0xA1, 0x75, 0x84, 0x01, 0x21, 0x25, 0x70, 0x46,
    0x81, 0xDF, 0x67, 0x18, 0xAC, 0xF1, 0xF7, 0x62, 0xCC, 0x70, 0x90, 0xE4, 0xC6, 0x01, 0xD3, 0x5A, 0xB4, 0x98, 0xB5, 0xE2,
    0x66, 0xEE, 0x26, 0x79, 0xCB, 0x51, 0x51, 0xDA, 0x48, 0x06, 0x35, 0x8C, 0x66, 0x61, 0x54, 0xAC, 0x0E, 0xAF, 0x56, 0x86,
    0x5F, 0x6F, 0xF4, 0x01, 0xD9, 0x53, 0x0A, 0x53, 0xFB, 0xD9, 0xB5, 0xF1, 0x6F, 0x84, 0x5F, 0x71, 0x91, 0xF8, 0x6A, 0xA6,
    0xFE, 0x55, 0x0A, 0x9F, 0x24, 0xA8, 0x14, 0x13, 0x38, 0xA2, 0xE2, 0x96, 0xB6, 0xCD, 0xAC, 0x2B, 0x49, 0x2D, 0x6B, 0x2B
};

/* .data @ 0x00002540 */
static volatile u8 *g_key_dest   = (volatile u8 *)0xBFC00220; /* 0x2540 */
static volatile u8 *g_hwbuff_280 = (volatile u8 *)0xBFC00280; /* 0x2544 */
static volatile u8 *g_hwbuff_A00 = (volatile u8 *)0xBFC00A00; /* 0x2548 */
static volatile u8 *g_hwbuff_340 = (volatile u8 *)0xBFC00340; /* 0x254C */

static SceSysEventHandler g_sysevent_handler = {
    sizeof(SceSysEventHandler),
    "SceUtils1024",
    0x00FFFF00,
    CallSysEventHandler,
    0,
    0,
    NULL,
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static const u8 g_key_zero_16[16] = { 0 };

/* .bss */
static u8 *g_prx_buff;              /* 0x25C0 */
static u8 g_swbuff_25C4[0x150];     /* 0x25C4 */
static u8 g_swbuff_2740[0x28];      /* 0x2740 */
static u8 g_selftest_buffer[0x1C];  /* 0x2780 */
static GlobalSysParam g_sys;        /* 0x279C */

/* 0x00000000 */
static int kirk7(u8 *prx, u32 size, u32 scramble_code, u32 sync)
{
    g_prx_buff = prx;
    ((u32 *)prx)[0] = 5;
    ((u32 *)prx)[3] = scramble_code;
    ((u32 *)prx)[1] = 0;
    ((u32 *)prx)[2] = 0;
    ((u32 *)prx)[4] = size;
    if (sync != 0)
        return sceUtilsBufferCopyByPollingWithRange(prx, size + 20, prx, size + 20, 7);
    return sceUtilsBufferCopyWithRange(prx, size + 20, prx, size + 20, 7);
}

/* 0x0000006C */
static int memlmd_memcmp(const u8 *a, const u8 *b, u32 size)
{
    u32 i;
    for (i = 0; i < size; i++) {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
    }
    return 0;
}

/* 0x000000A8 */
static int check_whitelist(u8 *prx, u32 prx_size, const u8 *whitelist, u32 whitelist_size, u32 sync)
{
    u32 i;
    int ret;

    for (i = 0; i < 4; i++)
        g_hwbuff_340[i] = prx[i];

    *(u32 *)prx = prx_size - 4;

    if (sync != 0)
        ret = sceUtilsBufferCopyByPollingWithRange((u8 *)g_hwbuff_A00, 0x14, prx, prx_size, 0xB);
    else
        ret = sceUtilsBufferCopyWithRange((u8 *)g_hwbuff_A00, 0x14, prx, prx_size, 0xB);

    if (ret != 0)
        return 0;

    for (i = 0; i < 4; i++)
        prx[i] = g_hwbuff_340[i];

    u32 num_hashes = whitelist_size / 0x14;
    for (i = 0; i < num_hashes; i++) {
        u32 j;
        int diff = 0;
        const u8 *entry = whitelist + i * 0x14;
        for (j = 0; j < 0x14; j++) {
            if ((s8)entry[j] != (s8)g_hwbuff_A00[j]) {
                diff = (s8)entry[j] - (s8)g_hwbuff_A00[j];
                break;
            }
        }
        if (diff == 0)
            return 1;
    }
    return 0;
}

/* 0x0000020C */
static int sub_0000020C(u8 *prx, u32 size, u32 *newsize, u32 sync)
{
    int ret;
    u32 i;
    const volatile u8 *key_ptr;
    u32 scramble_code;
    u32 mode_type;
    u8 stack_save[0x28];
    u8 stack_hash[0x14];

    if (prx == NULL || newsize == NULL) {
        ret = -201;
        goto cleanup;
    }
    if (size < 0x160) {
        ret = -202;
        goto cleanup;
    }
    if (((u32)prx & 0x3F) != 0) {
        ret = -203;
        goto cleanup;
    }
    if (((0x00220202 >> (((u32)prx >> 27) & 0x1F)) & 1) == 0) {
        ret = -204;
        goto cleanup;
    }

    for (i = 0; i < 0x150; i++)
        g_swbuff_25C4[i] = prx[i];

    if (memlmd_memcmp((const u8 *)&g_tags[3], g_swbuff_25C4 + 0xD0, 4) == 0) {
        key_ptr = g_key_dest;
        scramble_code = 0x43;
        mode_type = 4;
    } else if (memlmd_memcmp((const u8 *)&g_tags[2], g_swbuff_25C4 + 0xD0, 4) == 0) {
        key_ptr = g_key_zero_16;
        scramble_code = 0x43;
        mode_type = 4;
    } else if (memlmd_memcmp((const u8 *)&g_tags[0], g_swbuff_25C4 + 0xD0, 4) == 0) {
        key_ptr = g_key_22FC;
        scramble_code = 0x42;
        mode_type = 0;
    } else if (memlmd_memcmp((const u8 *)&g_tags[1], g_swbuff_25C4 + 0xD0, 4) == 0) {
        key_ptr = g_key_238C;
        scramble_code = 0x43;
        mode_type = 0;
    } else {
        ret = -301;
        goto cleanup;
    }

    if (mode_type != 0) {
        for (i = 0; i < 0x30; i++) {
            if (g_swbuff_25C4[0xD4 + i] != 0) {
                ret = -302;
                goto cleanup;
            }
        }
    } else {
        if (check_whitelist(prx, size, g_sha1_whitelist, 0x104, sync) != 1) {
            ret = -305;
            goto cleanup;
        }
    }

    u32 out_sz = ((u32)g_swbuff_25C4[0xB3] << 24) |
                 ((u32)g_swbuff_25C4[0xB2] << 16) |
                 ((u32)g_swbuff_25C4[0xB1] << 8)  |
                 (u32)g_swbuff_25C4[0xB0];
    *newsize = out_sz;
    if (size - 0x150 < out_sz) {
        ret = -206;
        goto cleanup;
    }

    if (mode_type == 0) {
        for (i = 0; i < 0x90; i++)
            g_hwbuff_A00[0x14 + i] = key_ptr[i];
    } else {
        u32 blk;
        for (blk = 0; blk < 9; blk++) {
            for (i = 0; i < 0x10; i++)
                g_hwbuff_A00[0x14 + (blk << 4) + i] = key_ptr[i];
            g_hwbuff_A00[0x14 + (blk << 4)] = (u8)blk;
        }
        for (i = 0; i < 0x28; i++)
            g_swbuff_2740[i] = prx[0x104 + i];
        for (i = 0; i < 0x28; i++)
            prx[0x104 + i] = 0;
        for (i = 0; i < 0x28; i++)
            stack_save[i] = g_swbuff_2740[i];

        g_prx_buff = prx;
        *(u32 *)prx = size - 4;
        if (sync != 0)
            ret = sceUtilsBufferCopyByPollingWithRange(prx, size, prx, size, 0xB);
        else
            ret = sceUtilsBufferCopyWithRange(prx, size, prx, size, 0xB);

        if (ret != 0) {
            if (ret < 0)
                ret = -102;
            else if (ret == 12)
                ret = -107;
            else
                ret = -105;
            goto restore_and_cleanup;
        }

        for (i = 0; i < 0x14; i++)
            stack_hash[i] = prx[i];
        for (i = 0; i < 0x20; i++)
            prx[i] = g_swbuff_25C4[i];
        for (i = 0; i < 0x28; i++)
            g_hwbuff_340[i] = g_kirk11_header[i];
        for (i = 0; i < 0x14; i++)
            g_hwbuff_340[0x28 + i] = stack_hash[i];
        for (i = 0; i < 0x28; i++)
            g_hwbuff_340[0x3C + i] = stack_save[i];

        if (sync != 0)
            ret = sceUtilsBufferCopyByPollingWithRange(NULL, 0, (u8 *)g_hwbuff_340, 0x64, 0x11);
        else
            ret = sceUtilsBufferCopyWithRange(NULL, 0, (u8 *)g_hwbuff_340, 0x64, 0x11);

        if (ret != 0) {
            ret = -306;
            goto restore_and_cleanup;
        }

        for (i = 0; i < 0x10; i++)
            g_key_dest[i] = 0;
    }

    ret = kirk7((u8 *)g_hwbuff_A00, 0x90, scramble_code, sync);
    if (ret != 0) {
        if (ret < 0)
            ret = -101;
        else if (ret == 12)
            ret = -107;
        else
            ret = -104;
        goto restore_and_cleanup;
    }

    for (i = 0; i < 0x90; i++)
        g_hwbuff_280[i] = g_hwbuff_A00[i];

    if (mode_type == 0) {
        for (i = 0; i < 0x80; i++)
            g_hwbuff_A00[i] = g_swbuff_25C4[0xD0 + i];
        for (i = 0; i < 0x50; i++)
            g_hwbuff_A00[0x80 + i] = g_swbuff_25C4[0x80 + i];
        for (i = 0; i < 0x80; i++)
            g_hwbuff_A00[0xD0 + i] = g_swbuff_25C4[i];
    } else {
        for (i = 0; i < 0x5C; i++)
            g_hwbuff_A00[i] = g_swbuff_25C4[0xD0 + i];
        for (i = 0; i < 0x10; i++)
            g_hwbuff_A00[0x5C + i] = g_swbuff_25C4[0x140 + i];
        for (i = 0; i < 0x14; i++)
            g_hwbuff_A00[0x6C + i] = g_swbuff_25C4[0x12C + i];
        for (i = 0; i < 0x30; i++)
            g_hwbuff_A00[0x80 + i] = g_swbuff_25C4[0x80 + i];
        for (i = 0; i < 0x10; i++)
            g_hwbuff_A00[0xB0 + i] = g_swbuff_25C4[0xC0 + i];
        for (i = 0; i < 0x10; i++)
            g_hwbuff_A00[0xC0 + i] = g_swbuff_25C4[0xB0 + i];
        for (i = 0; i < 0x80; i++)
            g_hwbuff_A00[0xD0 + i] = g_swbuff_25C4[i];
        for (i = 0; i < 0x28; i++)
            g_hwbuff_A00[0x34 + i] = 0;
    }

    if (mode_type != 0) {
        for (i = 0; i < 0x60; i++)
            g_hwbuff_340[0x14 + i] = g_hwbuff_A00[0x5C + i];
        ret = kirk7((u8 *)g_hwbuff_340, 0x60, scramble_code, sync);
        if (ret != 0) {
            if (ret < 0)
                ret = -103;
            else if (ret == 12)
                ret = -107;
            else
                ret = -106;
            goto restore_and_cleanup;
        }
        for (i = 0; i < 0x60; i++)
            g_hwbuff_A00[0x5C + i] = g_hwbuff_340[i];
    }

    if (mode_type == 0) {
        for (i = 0; i < 0x14; i++)
            g_hwbuff_340[i] = g_hwbuff_A00[4 + i];
        *(volatile u32 *)g_hwbuff_A00 = 0x14C;
        g_prx_buff = (u8 *)g_hwbuff_A00;
        for (i = 0; i < 0x14; i++)
            g_hwbuff_A00[4 + i] = g_hwbuff_280[i];
    } else {
        for (i = 0; i < 0x14; i++)
            g_hwbuff_340[i] = g_hwbuff_A00[0x6C + i];
        for (i = 0; i < 0x10; i++)
            g_hwbuff_A00[0x70 + i] = g_hwbuff_A00[0x5C + i];
        for (i = 0; i < 0x58; i++)
            g_hwbuff_A00[0x18 + i] = 0;
        g_prx_buff = (u8 *)g_hwbuff_A00;
        for (i = 0; i < 4; i++)
            g_hwbuff_A00[4 + i] = g_hwbuff_A00[i];
        *(volatile u32 *)g_prx_buff = 0x14C;
        for (i = 0; i < 0x10; i++)
            g_hwbuff_A00[8 + i] = g_hwbuff_280[i];
        for (i = 0; i < 0x10; i++)
            g_hwbuff_280[i] = 0;
    }

    if (sync != 0)
        ret = sceUtilsBufferCopyByPollingWithRange((u8 *)g_hwbuff_A00, 0x150, (u8 *)g_hwbuff_A00, 0x150, 0xB);
    else
        ret = sceUtilsBufferCopyWithRange((u8 *)g_hwbuff_A00, 0x150, (u8 *)g_hwbuff_A00, 0x150, 0xB);

    if (ret != 0) {
        if (ret < 0)
            ret = -102;
        else if (ret == 12)
            ret = -107;
        else
            ret = -105;
        goto restore_and_cleanup;
    }

    for (i = 0; i < 0x14; i++) {
        if ((s8)g_hwbuff_A00[i] != (s8)g_hwbuff_340[i]) {
            ret = -302;
            goto restore_and_cleanup;
        }
    }

    if (mode_type == 0) {
        for (i = 0; i < 0x70; i++)
            g_hwbuff_A00[0x40 + i] ^= g_hwbuff_280[0x14 + i];
        ret = kirk7((u8 *)(g_hwbuff_A00 + 0x2C), 0x70, scramble_code, sync);
        if (ret != 0) {
            if (ret < 0)
                ret = -103;
            else if (ret == 12)
                ret = -107;
            else
                ret = -106;
            goto restore_and_cleanup;
        }
        for (i = 0; i < 0x70; i++)
            prx[0x40 + i] = g_hwbuff_A00[0x2C + i] ^ g_hwbuff_280[0x20 + i];
        for (i = 0; i < 0xA0; i++)
            prx[0xB0 + i] = g_hwbuff_A00[0xB0 + i];
        if (prx[0xA4] != 1) {
            ret = -303;
            goto restore_and_cleanup;
        }
    } else {
        for (i = 0; i < 0x40; i++)
            g_hwbuff_A00[0x80 + i] ^= g_hwbuff_280[0x10 + i];
        for (i = 0; i < 0x40; i++)
            g_hwbuff_280[0x10 + i] = 0;
        ret = kirk7((u8 *)(g_hwbuff_A00 + 0x6C), 0x40, scramble_code, sync);
        if (ret != 0) {
            if (ret < 0)
                ret = -103;
            else if (ret == 12)
                ret = -107;
            else
                ret = -106;
            goto restore_and_cleanup;
        }
        for (i = 0; i < 0x40; i++)
            prx[0x40 + i] = g_hwbuff_A00[0x6C + i] ^ g_hwbuff_280[0x50 + i];
        for (i = 0; i < 0x40; i++)
            g_hwbuff_280[0x50 + i] = 0;
        for (i = 0; i < 0x30; i++)
            prx[0x80 + i] = 0;
        prx[0xA0] = 1;
        for (i = 0; i < 0x10; i++)
            prx[0xB0 + i] = g_hwbuff_A00[0xC0 + i];
        for (i = 0; i < 0x10; i++)
            prx[0xC0 + i] = 0;
        for (i = 0; i < 0x80; i++)
            prx[0xD0 + i] = g_hwbuff_A00[0xD0 + i];
    }

    if (sync != 0)
        ret = sceUtilsBufferCopyByPollingWithRange(prx, size, prx + 0x40, size - 0x40, 1);
    else
        ret = sceUtilsBufferCopyWithRange(prx, size, prx + 0x40, size - 0x40, 1);

    if (ret != 0) {
        if (ret < 0)
            ret = -303;
        else if (ret == 12)
            ret = -107;
        else
            ret = -304;
        goto restore_and_cleanup;
    }

    if (*newsize < 0x150) {
        for (i = 0; i < 0x150 - *newsize; i++)
            prx[*newsize + i] = 0;
    }
    ret = 0;
    goto cleanup;

restore_and_cleanup:
    for (i = 0; i < 0x150; i++)
        prx[i] = g_swbuff_25C4[i];
    *newsize = 0;

cleanup:
    for (i = 0; i < 0x10; i++)
        g_key_dest[i] = 0;
    for (i = 0; i < 0x90; i++)
        g_hwbuff_280[i] = 0;
    for (i = 0; i < 0x150; i++)
        g_hwbuff_A00[i] = 0;
    for (i = 0; i < 0xB4; i++)
        g_hwbuff_340[i] = 0;
    for (i = 0; i < 0x150; i++)
        g_swbuff_25C4[i] = 0;
    return ret;
}

/* 0x00000EE8 */
s32 memlmd_EF73E85B(u8 *prx, u32 size, u32 *newsize)
{
    sceKernelPowerLockForUser(0);
    s32 ret = sub_0000020C(prx, size, newsize, 0);
    sceKernelPowerUnlockForUser(0);
    return ret;
}

/* 0x00000F4C */
s32 memlmd_CF03556B(u8 *prx, u32 size, u32 *newsize)
{
    sceKernelPowerLockForUser(0);
    s32 ret = sub_0000020C(prx, size, newsize, 1);
    sceKernelPowerUnlockForUser(0);
    return ret;
}

/* 0x00000FB0 */
static int kirk8(u8 *prx, u32 size, u32 sync)
{
    g_prx_buff = prx;
    ((u32 *)prx)[0] = 5;
    ((u32 *)prx)[3] = 0x100;
    ((u32 *)prx)[1] = 0;
    ((u32 *)prx)[2] = 0;
    ((u32 *)prx)[4] = size;
    if (sync != 0)
        return sceUtilsBufferCopyByPollingWithRange(prx, size + 20, prx, size + 20, 8);
    return sceUtilsBufferCopyWithRange(prx, size + 20, prx, size + 20, 8);
}

/* 0x0000101C */
static int unsign(u8 *prx, u32 size, u32 sync)
{
    int ret;
    u32 i;

    if (prx == NULL) {
        ret = -201;
        goto cleanup;
    }
    if (size < 0x160) {
        ret = -202;
        goto cleanup;
    }
    if (((u32)prx & 0x3F) != 0) {
        ret = -203;
        goto cleanup;
    }
    if (((0x00220202 >> (((u32)prx >> 27) & 0x1F)) & 1) == 0) {
        ret = -204;
        goto cleanup;
    }

    for (i = 0; i < 0xD0; i++)
        g_hwbuff_A00[0x14 + i] = prx[0x80 + i];

    for (i = 0; i < 0xD0; i++)
        g_hwbuff_A00[0x14 + i] ^= g_key_22EC[i & 0xF];

    int ret2 = kirk8((u8 *)g_hwbuff_A00, 0xD0, sync);
    if (ret2 == 0) {
        for (i = 0; i < 0xD0; i++)
            g_hwbuff_A00[i] ^= g_key_22DC[i & 0xF];
        for (i = 0; i < 0x90; i++)
            prx[0x80 + i] = g_hwbuff_A00[0x40 + i];
        for (i = 0; i < 0x40; i++)
            prx[0x110 + i] = g_hwbuff_A00[i];
        ret = 0;
        goto cleanup;
    }

    if (ret2 < 0)
        ret = -103;
    else if (ret2 != 12)
        ret = -106;
    else
        ret = -107;

cleanup:
    for (i = 0; i < 0x164; i++)
        g_hwbuff_A00[i] = 0;
    return ret;
}

/* 0x000011BC */
s32 memlmd_6192F715(u8 *addr, u32 size)
{
    sceKernelPowerLockForUser(0);
    s32 ret = unsign(addr, size, 0);
    sceKernelPowerUnlockForUser(0);
    return ret;
}

/* 0x00001210 */
s32 memlmd_EA94592C(u8 *addr, u32 size)
{
    sceKernelPowerLockForUser(0);
    s32 ret = unsign(addr, size, 1);
    sceKernelPowerUnlockForUser(0);
    return ret;
}

/* 0x00001264 */
s32 memlmd_F26A33C3(u32 unk, vs32 *hashAddr)
{
    const volatile u8 *hash_addr = (const volatile u8 *)hashAddr;
    u32 i;
    if (unk != 0)
        return -207;

    for (i = 0; i < 16; i++) {
        if (hash_addr[i] != 0)
            break;
    }
    if (i == 16)
        return -207;

    sceKernelPowerLockForUser(0);
    for (i = 0; i < 16; i++)
        g_key_dest[i] = g_seed_241C[i] ^ hash_addr[i];
    sceKernelPowerUnlockForUser(0);
    return 0;
}

/* 0x00001310 */
s32 memlmd_76B7E315(u32 unk)
{
    u32 i;
    if (unk != 0)
        return -207;

    sceKernelPowerLockForUser(0);
    for (i = 0; i < 16; i++)
        g_key_dest[i] = 0;
    sceKernelPowerUnlockForUser(0);
    return 0;
}

/* 0x0000139C */
s32 memlmd_2AE425D2(u32 unk)
{
    unk &= 0xFFFF0000;
    return (0xF099FFFFU < unk);
}

/* 0x000013B0 */
s32 memlmd_9D36A439(u32 unk)
{
    unk &= 0xFFFF0000;
    return (0xF0A0FFFFU < unk);
}

/* 0x000013C4 */
static int InterruptHandler(int unk1, GlobalSysParam *my_sys)
{
    (void)unk1;
    if (my_sys->operation == OP_ASYNC_SEMA) {
        my_sys->result = KIRK_GET_PROCESSING_RESULT();
        my_sys->operation = OP_IDLE;
        my_sys->status_pattern = KIRK_GET_PROCESSING_STATUS_PATTERN();
        KIRK_SET_END_PROCESSING_PATTERN(KIRK_GET_PROCESSING_STATUS_PATTERN());
        __asm__ volatile("sync");
        sceKernelSetEventFlag(my_sys->utils_flag, 1);
    } else {
        KIRK_SET_END_PROCESSING_PATTERN(KIRK_GET_PROCESSING_STATUS_PATTERN());
        __asm__ volatile("sync");
    }
    return -1;
}

/* 0x00001444 */
static int BusClock(int bus, int mode)
{
    int intr = sceKernelCpuSuspendIntr();
    u32 bus_mask = SYSCONF_GET_BUS_CLOCK_MASK();
    u32 result_mask;
    if (mode != 0)
        result_mask = bus_mask | bus;
    else
        result_mask = bus_mask & ~bus;
    SYSCONF_SET_BUS_CLOCK_MASK(result_mask);
    sceKernelCpuResumeIntrWithSync(intr);
    return 0;
}

static int Semaphore(u8 *out_buf, int out_len, u8 *in_buf, int in_len, int mode, int skip_size_check);
static int SemaphoreAsync(u8 *out_buf, int out_len, u8 *in_buf, int in_len, int mode, int skip_size_check);

/* 0x0000149C */
static int SystemSuspendHandler(void)
{
    sceKernelDisableIntr(KIRK_INTERRUPT);
    if (g_sys.operation == OP_ASYNC_SEMA) {
        while ((KIRK_GET_PROCESSING_STATUS_PATTERN() & 0x33) == 0) {
        }
        g_sys.result = KIRK_GET_PROCESSING_RESULT();
        g_sys.status_pattern = KIRK_GET_PROCESSING_STATUS_PATTERN();
        g_sys.operation = OP_IDLE;
        KIRK_SET_END_PROCESSING_PATTERN(KIRK_GET_PROCESSING_STATUS_PATTERN());
        __asm__ volatile("sync");
        sceKernelSetEventFlag(g_sys.utils_flag, 1);
    }

    if ((SYSCONF_GET_BUS_CLOCK_MASK() & KIRK_BUS_CLOCK) == 0)
        g_sys.kirk_enabled = 0;
    else
        g_sys.kirk_enabled = 1;

    BusClock(KIRK_BUS_CLOCK, BUS_DISABLE);
    return 0;
}

/* 0x00001568 */
static int SystemResumeHandler(void)
{
    BusClock(KIRK_BUS_CLOCK, BUS_ENABLE);
    KIRK_SET_INCOMPLETE_PROCESSING_ERROR(1);
    while (KIRK_GET_INCOMPLETE_PROCESSING_ERROR() == 1) {
    }
    sceKernelEnableIntr(KIRK_INTERRUPT);
    Semaphore(g_selftest_buffer, 0x1C, g_selftest_buffer, 0x1C, 0xF, 0);
    if (g_sys.kirk_enabled != 1)
        BusClock(KIRK_BUS_CLOCK, BUS_DISABLE);
    return 0;
}

/* 0x000015F4 */
static int CallSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
    (void)ev_name;
    (void)param;
    (void)result;
    if (ev_id == 0x4000)
        return SystemSuspendHandler();
    if (ev_id == 0x10000)
        return SystemResumeHandler();
    return 0;
}

/* 0x00001640 */
static int __attribute__((noinline)) StopSys(void)
{
    sceKernelUnregisterSysEventHandler(&g_sysevent_handler);
    sceKernelReleaseIntrHandler(KIRK_INTERRUPT);
    sceKernelUnlockMutex(g_sys.utils_mutex, 1);
    BusClock(KIRK_BUS_CLOCK, BUS_ENABLE);
    return 0;
}

/* 0x00001688 */
static int __attribute__((noinline)) InitSys(void)
{
    int ret;
    u32 mutex_opt[2];
    u32 i;

    g_sys.operation = OP_IDLE;
    ret = sceKernelRegisterIntrHandler(KIRK_INTERRUPT, 1, InterruptHandler, &g_sys, NULL);
    if (ret != 0)
        return 1;

    ret = sceKernelEnableIntr(KIRK_INTERRUPT);
    if (ret != 0) {
        sceKernelReleaseIntrHandler(KIRK_INTERRUPT);
        return 1;
    }

    ret = sceKernelRegisterSysEventHandler(&g_sysevent_handler);
    if (ret != 0) {
        sceKernelDisableIntr(KIRK_INTERRUPT);
        sceKernelReleaseIntrHandler(KIRK_INTERRUPT);
        return 1;
    }

    mutex_opt[0] = 8;
    mutex_opt[1] = 0xF;
    g_sys.utils_mutex = sceKernelCreateMutex("SceUtils1024", 0x601, 0, mutex_opt);
    if (g_sys.utils_mutex <= 0) {
        sceKernelUnregisterSysEventHandler(&g_sysevent_handler);
        sceKernelDisableIntr(KIRK_INTERRUPT);
        sceKernelReleaseIntrHandler(KIRK_INTERRUPT);
        return 1;
    }

    g_sys.utils_flag = sceKernelCreateEventFlag("SceUtils1024", 0, 0, NULL);
    if (g_sys.utils_flag <= 0) {
        sceKernelUnregisterSysEventHandler(&g_sysevent_handler);
        sceKernelDisableIntr(KIRK_INTERRUPT);
        sceKernelReleaseIntrHandler(KIRK_INTERRUPT);
        sceKernelDeleteMutex(g_sys.utils_mutex);
        return 1;
    }

    for (i = 0; i < 0x1C; i++)
        g_selftest_buffer[i] = 0;

    ret = sceUtilsBufferCopyByPolling(g_selftest_buffer, g_selftest_buffer, 0xF);
    if (ret != 0) {
        sceKernelUnregisterSysEventHandler(&g_sysevent_handler);
        sceKernelDisableIntr(KIRK_INTERRUPT);
        sceKernelReleaseIntrHandler(KIRK_INTERRUPT);
        sceKernelDeleteMutex(g_sys.utils_mutex);
        sceKernelDeleteEventFlag(g_sys.utils_flag);
        return 1;
    }

    return 0;
}

/* 0x00001364 */
int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return InitSys();
}

/* 0x00001380 */
int module_reboot_before(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return StopSys();
}

/* 0x00001820 */
static int Semaphore(u8 *out_buf, int out_len, u8 *in_buf, int in_len, int mode, int skip_size_check)
{
    int res;

    g_sys.operation = OP_SYNC_SEMA;
    if (!skip_size_check) {
        if (out_buf != in_buf) {
            sceKernelDcacheWritebackInvalidateRange(out_buf, out_len);
            sceKernelDcacheWritebackInvalidateRange(in_buf, in_len);
        } else {
            int max_len = ((u32)in_len < (u32)out_len) ? out_len : in_len;
            sceKernelDcacheWritebackInvalidateRange(out_buf, max_len);
        }
    } else {
        sceKernelDcacheWritebackInvalidateAll();
    }

    u32 in_region = ((u32)in_buf >> 27) & 0x1F;
    if (((0x00220202 >> in_region) & 1) != 0)
        UtilsForKernel_66463BEC(5);

    u32 in_phys  = (u32)in_buf & 0x1FFFFFFF;
    u32 out_phys = (u32)out_buf & 0x1FFFFFFF;

    KIRK_SET_COMMAND(mode);
    KIRK_SET_SOURCE_ADDR(in_phys);
    KIRK_SET_DEST_ADDR(out_phys);
    KIRK_SET_PROCESSING_PHASE(1);

    while ((KIRK_GET_PROCESSING_STATUS_PATTERN() & 0x33) == 0) {
    }

    if ((KIRK_GET_PROCESSING_STATUS_PATTERN() & 0x22) != 0) {
        KIRK_SET_INCOMPLETE_PROCESSING_ERROR(1);
        res = -1;
    } else if ((KIRK_GET_PROCESSING_STATUS_PATTERN() & 0x10) != 0) {
        KIRK_SET_PROCESSING_PHASE(2);
        while ((KIRK_GET_PROCESSING_STATUS_PATTERN() & 0x33) == 0) {
        }
        if ((KIRK_GET_PROCESSING_STATUS_PATTERN() & 2) == 0)
            KIRK_SET_INCOMPLETE_PROCESSING_ERROR(1);
        res = -1;
    } else {
        res = KIRK_GET_PROCESSING_RESULT();
    }

    if (((0x00220202 >> in_region) & 1) != 0)
        UtilsForKernel_66463BEC(5);

    g_sys.operation = OP_IDLE;
    KIRK_SET_END_PROCESSING_PATTERN(KIRK_GET_PROCESSING_STATUS_PATTERN());
    __asm__ volatile("sync");
    return res;
}

/* 0x000019F4 */
static int SemaphoreAsync(u8 *out_buf, int out_len, u8 *in_buf, int in_len, int mode, int skip_size_check)
{
    int intr;
    int res = -1;

    if (!skip_size_check) {
        if (out_buf != in_buf) {
            sceKernelDcacheWritebackInvalidateRange(out_buf, out_len);
            sceKernelDcacheWritebackInvalidateRange(in_buf, in_len);
        } else {
            int max_len = ((u32)in_len < (u32)out_len) ? out_len : in_len;
            sceKernelDcacheWritebackInvalidateRange(out_buf, max_len);
        }
    } else {
        sceKernelDcacheWritebackInvalidateAll();
    }

    u32 in_region = ((u32)in_buf >> 27) & 0x1F;
    if (((0x00220202 >> in_region) & 1) != 0)
        UtilsForKernel_66463BEC(5);

    intr = sceKernelCpuSuspendIntr();
    u32 in_phys  = (u32)in_buf & 0x1FFFFFFF;
    u32 out_phys = (u32)out_buf & 0x1FFFFFFF;

    KIRK_SET_COMMAND(mode);
    KIRK_SET_SOURCE_ADDR(in_phys);
    g_sys.operation = OP_ASYNC_SEMA;
    KIRK_SET_DEST_ADDR(out_phys);
    KIRK_SET_INIT_ASYNC_PROCESSING_PATTERN(0x33);
    KIRK_SET_PROCESSING_PHASE(1);
    sceKernelCpuResumeIntrWithSync(intr);

    if (sceKernelWaitEventFlag(g_sys.utils_flag, 1, 0x21, NULL, NULL) == 0) {
        KIRK_SET_END_ASYNC_PROCESSING_PATTERN(0x33);
        if ((g_sys.status_pattern & 0x22) != 0) {
            KIRK_SET_INCOMPLETE_PROCESSING_ERROR(1);
            __asm__ volatile("sync");
            res = -1;
        } else if ((g_sys.status_pattern & 0x10) != 0) {
            intr = sceKernelCpuSuspendIntr();
            KIRK_SET_INIT_ASYNC_PROCESSING_PATTERN(2);
            g_sys.operation = OP_ASYNC_SEMA;
            KIRK_SET_PROCESSING_PHASE(2);
            sceKernelCpuResumeIntrWithSync(intr);

            if (sceKernelWaitEventFlag(g_sys.utils_flag, 1, 0x21, NULL, NULL) == 0) {
                KIRK_SET_END_ASYNC_PROCESSING_PATTERN(2);
                if ((g_sys.status_pattern & 2) == 0) {
                    KIRK_SET_INCOMPLETE_PROCESSING_ERROR(1);
                    __asm__ volatile("sync");
                }
            }
            res = -1;
        } else {
            res = g_sys.result;
        }
    }

    if (((0x00220202 >> in_region) & 1) != 0)
        UtilsForKernel_66463BEC(5);

    return res;
}

/* 0x00001C18 */
int sceUtilsBufferCopy(u8 *outbuff, u8 *inbuff, int cmd)
{
    int res = -1;
    if (sceKernelLockMutex(g_sys.utils_mutex, 1, NULL) == 0) {
        BusClock(KIRK_BUS_CLOCK, BUS_ENABLE);
        res = SemaphoreAsync(outbuff, 0, inbuff, 0, cmd, 1);
        BusClock(KIRK_BUS_CLOCK, BUS_DISABLE);
        if (sceKernelUnlockMutex(g_sys.utils_mutex, 1) != 0)
            res = -1;
    }
    return res;
}

/* 0x00001CD4 */
int sceUtilsBufferCopyByPolling(u8 *outbuff, u8 *inbuff, int cmd)
{
    int res = -1;
    if (sceKernelLockMutex(g_sys.utils_mutex, 1, NULL) == 0) {
        BusClock(KIRK_BUS_CLOCK, BUS_ENABLE);
        res = Semaphore(outbuff, 0, inbuff, 0, cmd, 1);
        BusClock(KIRK_BUS_CLOCK, BUS_DISABLE);
        if (sceKernelUnlockMutex(g_sys.utils_mutex, 1) != 0)
            res = -1;
    }
    return res;
}

/* 0x00001D90 */
int sceUtilsBufferCopyWithRange(u8 *outbuff, int outsize, u8 *inbuff, int insize, int cmd)
{
    int res = -1;
    if (sceKernelLockMutex(g_sys.utils_mutex, 1, NULL) == 0) {
        BusClock(KIRK_BUS_CLOCK, BUS_ENABLE);
        res = SemaphoreAsync(outbuff, outsize, inbuff, insize, cmd, 0);
        BusClock(KIRK_BUS_CLOCK, BUS_DISABLE);
        if (sceKernelUnlockMutex(g_sys.utils_mutex, 1) != 0)
            res = -1;
    }
    return res;
}

/* 0x00001E64 */
int sceUtilsBufferCopyByPollingWithRange(u8 *outbuff, int outsize, u8 *inbuff, int insize, int cmd)
{
    int res = -1;
    if (sceKernelLockMutex(g_sys.utils_mutex, 1, NULL) == 0) {
        BusClock(KIRK_BUS_CLOCK, BUS_ENABLE);
        res = Semaphore(outbuff, outsize, inbuff, insize, cmd, 0);
        BusClock(KIRK_BUS_CLOCK, BUS_DISABLE);
        if (sceKernelUnlockMutex(g_sys.utils_mutex, 1) != 0)
            res = -1;
    }
    return res;
}

/* 0x00001F38 */
s32 memlmd_2F3D7E2D(void)
{
    s32 res = -1;
    if (sceKernelLockMutex(g_sys.utils_mutex, 1, NULL) == 0) {
        BusClock(KIRK_BUS_CLOCK, BUS_ENABLE);
        if (sceKernelUnlockMutex(g_sys.utils_mutex, 1) == 0)
            res = 0;
    }
    return res;
}
