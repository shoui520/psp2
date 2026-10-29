#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define NOINLINE __attribute__((noinline))

/* Imports */
extern s32 sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(s32 state);
extern void *sceKernelMemset(void *dst, s32 c, u32 size);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);
extern s32 _sceKernelCheckThreadStack(void);
extern s32 sceKernelLockLwMutex(void *work, s32 count, u32 *timeout);
extern s32 sceKernelUnlockLwMutex(void *work, s32 count);
extern s32 sceKernelGetModuleGPByAddress(const void *addr, u32 *gp);
extern s32 sceKernelCreateFpl(const char *name, s32 part, s32 attr, u32 block_size, u32 num_blocks, void *opt);
extern s32 sceKernelAllocateFpl(s32 uid, void **data, u32 *timeout);
extern s32 sceKernelTryAllocateFpl(s32 uid, void **data);
extern s32 sceKernelFreeFpl(s32 uid, void *data);
extern s32 sceKernelDeleteFpl(s32 uid);
extern s32 sceKernelCreateLwMutex(void *work, const char *name, u32 attr, s32 init_count, void *opt);
extern s32 sceKernelDeleteLwMutex(void *work);
extern s32 sceKernelDcacheInvalidateRangeForUser(const void *p, u32 size);
extern void sceKernelDcacheWritebackInvalidateRange(const void *p, u32 size);
extern void sceKernelDcacheWritebackRange(const void *p, u32 size);
extern s32 sceAudiocodecInit(u32 *work, s32 type);
extern s32 sceAudiocodecDecode(u32 *work, s32 type);
extern s32 sceMpegBasePESpacketCopy(void *pkt);

extern s32 mp4msv_089364A9(u32 a0, void *a1);
extern s32 mp4msv_08F96BE2(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_0C06DACB(u32 a0, u32 a1, u32 a2, void *a3);
extern s32 mp4msv_0C0843B0(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_0D32271B(u32 *a0, u32 a1);
extern s32 mp4msv_0DF0B9AD(u32 a0, void *a1);
extern s32 mp4msv_182FF541(u32 a0, void *a1, u32 a2, u32 a3, u32 t0);
extern s32 mp4msv_21BD7274(u32 a0, u32 a1, u32 a2, u32 *a3);
extern s32 mp4msv_2B1B8D23(u32 a0, u32 a1, u32 a2, void *a3);
extern s32 mp4msv_2BAF7C25(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_35144B9C(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_3C2183C7(void *a0);
extern s32 mp4msv_3D57BA52(u32 a0, u32 a1, u32 a2);
extern s32 mp4msv_3D8D41A0(u32 *a0);
extern s32 mp4msv_3F6B6C59(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_46186C26(u32 a0, void *a1);
extern s32 mp4msv_4C677A7E(u32 a0, void *a1);
extern s32 mp4msv_54410730(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_5EE48079(u32 a0, u32 a1, u32 a2, u32 *a3);
extern s32 mp4msv_67AF9E0F(u32 a0, void *a1, u32 a2);
extern s32 mp4msv_6D619263(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_9632797F(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_9BB5E338(u32 a0, u32 a1, u32 *a2);
extern s32 mp4msv_9CA13D1A(void *a0);
extern s32 mp4msv_9D56A7CC(u32 a0, u32 a1, u32 *a2);
extern s32 mp4msv_A09878FC(u32 a0, u32 a1, u32 a2, u32 a3, void *t0);
extern s32 mp4msv_A418509D(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_A55AF94C(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_A824645B(u32 a0);
extern s32 mp4msv_A9FD62E2(u32 a0, u32 a1, u32 a2, void *a3, void *t0);
extern s32 mp4msv_AD3AF34E(u32 a0, void *a1);
extern s32 mp4msv_BD23D34A(u32 *a0, u32 a1, u32 a2, u32 a3);
extern s32 mp4msv_BE558D75(u32 a0, u32 a1, u32 a2, void *a3);
extern s32 mp4msv_CFF25FAC(u32 *a0, u32 a1);
extern s32 mp4msv_E042AB3C(u32 a0, u32 *a1);
extern s32 mp4msv_E5C33D61(u32 a0, u32 a1, u32 a2, u32 *a3);
extern s32 mp4msv_E641D779(u32 a0, u32 a1, u32 a2, void *a3);
extern s32 mp4msv_EE62F96D(u32 a0, u32 a1, void *a2);
extern s32 mp4msv_EF6D76BC(u32 a0, void *a1);
extern s32 mp4msv_F595F917(void);
extern s32 mp4msv_FF9E6F58(u32 a0, u32 a1, void *a2);

/* State globals */
static u32 g_mp4_inited;
static u32 g_mp4_flag1;
static u32 g_mp4_flag2;
static u32 g_mp4_ctx[256];
static u32 g_mp4_heap[64];

/* Forward declarations */
NOINLINE s64 sub_0000(s64 a, s64 b);
NOINLINE s32 sub_05d4(u32 *mp4);
NOINLINE s32 sub_6780(const char *s1, const char *s2);
NOINLINE s32 sub_6800(const char *s);
NOINLINE s32 sub_68a8(u32 *heap, u32 a1, u32 a2, u32 a3);
NOINLINE s32 sub_6ab8(u32 *heap);
NOINLINE void *sub_6b00(u32 *heap, u32 size);
NOINLINE void *sub_6ca4(u32 *heap, u32 align, u32 size);
NOINLINE s32 sub_6ee8(u32 *heap, void *ptr);
NOINLINE s32 sub_6ff8(u32 *heap);
NOINLINE void *sub_7054(void *dst, const void *src, u32 size);
NOINLINE void *sub_70dc(void *dst, s32 c, u32 size);
NOINLINE s32 sub_7134(u32 *lock, const char *name);
NOINLINE s32 sub_7174(u32 *lock);
NOINLINE s32 sub_71ac(u32 *lock);
NOINLINE s32 sub_71fc(u32 *lock);
NOINLINE void sub_7248(u32 a0);

/* 1. 0x0000: __divdi3 */
NOINLINE s64 sub_0000(s64 a, s64 b)
{
    s32 sign = 0;
    u64 ua, ub, q = 0, r = 0;
    s32 i;
    if (a < 0) {
        a = -a;
        sign = ~sign;
    }
    if (b < 0) {
        b = -b;
        sign = ~sign;
    }
    ua = (u64)a;
    ub = (u64)b;
    if (ub == 0)
        return 0;
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((ua >> i) & 1);
        if (r >= ub) {
            r -= ub;
            q |= (1ULL << i);
        }
    }
    return sign ? -(s64)q : (s64)q;
}

/* 2. 0x05d4 */
NOINLINE s32 sub_05d4(u32 *mp4)
{
    u32 *p;
    if (mp4 == 0)
        return (s32)0x80680102;
    p = (u32 *)mp4[0];
    if (p == 0)
        return (s32)0x80680104;
    if (p[0] != 0x5444344dU)
        return (s32)0x80680104;
    if (p[1] != (u32)p)
        return (s32)0x80680104;
    return 0;
}

/* Forward decls for helpers called in sub_0640..sub_2c28 */
NOINLINE s32 sub_2c28(u32 a0);
NOINLINE s32 sub_2c50(u32 a0);
NOINLINE s32 sub_2cd4(void);
NOINLINE s32 sub_2ce0(u32 *a0, u32 a1, u32 a2, u32 a3);
NOINLINE s32 sub_2ea4(u32 *a0);
NOINLINE void *sub_2f14(u32 *a0, u32 size);
NOINLINE void *sub_2fa8(u32 *a0, u32 align, u32 size);
NOINLINE s32 sub_3040(u32 *a0);
NOINLINE s32 sub_308c(u32 *a0, void *ptr);
NOINLINE s32 sub_30d8(u32 *a0, void *a1, u32 a2, u32 a3);
NOINLINE s32 sub_336c(u32 *a0);
NOINLINE s32 sub_36bc(u32 *a0);
NOINLINE s32 sub_3ce8(u32 *a0, u32 a1, void *a2);
NOINLINE s32 sub_3ec4(u32 *a0, u32 a1, void *a2, void *a3);
NOINLINE s32 sub_4018(u32 *a0, void *a1, void *a2);
NOINLINE s32 sub_41d4(u32 *a0, void *a1);
NOINLINE s32 sub_4254(u32 *a0, void *a1);
NOINLINE s32 sub_427c(u32 *a0, void *a1);
NOINLINE s32 sub_42ac(void *a0);
NOINLINE s32 sub_42c8(u32 *a0, u32 a1);
NOINLINE s32 sub_4400(u32 *a0);
NOINLINE s32 sub_4474(u32 *a0);
NOINLINE s32 sub_44ac(u32 *a0, void *a1, void *a2);
NOINLINE s32 sub_45e4(u32 *a0, void *a1);
NOINLINE s32 sub_46bc(u32 *a0, void *a1);
NOINLINE s32 sub_4754(u32 *a0, u32 a1, u32 a2);
NOINLINE s32 sub_4824(u32 *a0);
NOINLINE s32 sub_489c(u32 *a0, void **a1);
NOINLINE s32 sub_4910(u32 *a0, void *a1);
NOINLINE s32 sub_495c(u32 *a0, void *a1);
NOINLINE s32 sub_4970(u32 *a0, u32 a1, u32 a2, void *a3);
NOINLINE s32 sub_4a14(u32 *a0);
NOINLINE s32 sub_4b6c(u32 *a0);
NOINLINE s32 sub_4be0(u32 *a0, void *a1);
NOINLINE s32 sub_4c60(u32 *a0, void *a1);

/* 3. 0x0640 */
NOINLINE s32 sub_0640(u32 *mp4, u32 *param, void *buf, u32 size)
{
    u32 val = 0;
    s32 ret;
    u32 *ctx;

    if (mp4 == 0 || param == 0 || buf == 0)
        return (s32)0x80680102;
    ret = mp4msv_F595F917();
    if (ret < 0)
        return ret;
    ret = mp4msv_3D8D41A0(&val);
    if (ret < 0)
        return ret;
    ret = mp4msv_CFF25FAC(&val, (u32)buf);
    if (ret < 0)
        return ret;
    ctx = (u32 *)buf;
    ret = sub_2ce0(ctx + 8, (u32)buf + 256, size - 256, val);
    if (ret < 0)
        return ret;
    ret = sub_30d8(ctx + 16, param, (u32)buf, size);
    if (ret < 0)
        return ret;
    ret = mp4msv_BD23D34A(&val, ctx[2], ctx[3], ctx[4]);
    if (ret < 0)
        return ret;
    ret = mp4msv_0D32271B(&val, ctx[5]);
    if (ret < 0)
        return ret;
    ret = mp4msv_AD3AF34E(val, ctx + 24);
    if (ret < 0)
        return ret;
    ret = mp4msv_0DF0B9AD(val, ctx + 28);
    if (ret < 0)
        return ret;
    ret = mp4msv_67AF9E0F(val, ctx + 32, param[0]);
    if (ret < 0)
        return ret;
    ret = mp4msv_9D56A7CC(val, param[1], ctx + 36);
    if (ret < 0)
        return ret;
    ret = mp4msv_46186C26(val, ctx + 40);
    if (ret < 0)
        return ret;
    ret = mp4msv_3D57BA52(val, param[2], param[3]);
    if (ret < 0)
        return ret;
    ret = mp4msv_E042AB3C(val, ctx + 44);
    if (ret < 0)
        return ret;
    ret = mp4msv_182FF541(val, ctx + 48, param[0], param[1], param[2]);
    if (ret < 0)
        return ret;
    ctx[0] = 0x5444344dU;
    ctx[1] = (u32)ctx;
    mp4[0] = (u32)ctx;
    return 0;
}

/* 4. 0x0a20 */
NOINLINE s32 sub_0a20(u32 *mp4)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    sub_336c(ctx + 16);
    sub_2ea4(ctx + 8);
    ctx[0] = 0;
    ctx[1] = 0;
    mp4[0] = 0;
    return 0;
}

/* 5. 0x0ac4 */
NOINLINE s32 sub_0ac4(u32 *mp4)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    ret = mp4msv_A824645B(ctx[2]);
    return ret < 0 ? ret : 0;
}

/* 6. 0x0b38 */
NOINLINE s32 sub_0b38(u32 *mp4, u32 *out)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    if (out == 0)
        return (s32)0x80680102;
    ctx = (u32 *)mp4[0];
    ret = sub_3040(ctx + 8);
    if (ret < 0)
        return ret;
    *out = (u32)ret;
    return 0;
}

/* 7. 0x0cf4 */
NOINLINE s32 sub_0cf4(u32 *mp4, u32 *out)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    if (out == 0)
        return (s32)0x80680102;
    ctx = (u32 *)mp4[0];
    out[0] = ctx[6];
    out[1] = ctx[7];
    return 0;
}

/* 8. 0x0d80 */
NOINLINE s32 sub_0d80(u32 *mp4, u32 a1, void *a2)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    if (a2 == 0)
        return (s32)0x80680102;
    ctx = (u32 *)mp4[0];
    ret = mp4msv_0C0843B0(ctx[2], a1, a2);
    return ret < 0 ? ret : 0;
}

/* 9. 0x0e94 */
NOINLINE s32 sub_0e94(u32 *mp4, void *a1)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    if (a1 == 0)
        return (s32)0x80680102;
    ctx = (u32 *)mp4[0];
    ret = mp4msv_089364A9(ctx[2], a1);
    return ret < 0 ? ret : 0;
}

/* 10. 0x0f94 */
NOINLINE s32 sub_0f94(u32 *mp4, u32 a1, void *a2, u32 mode)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    if (a2 == 0)
        return (s32)0x80680102;
    ctx = (u32 *)mp4[0];
    if (mode == 0)
        ret = mp4msv_54410730(ctx[2], a1, a2);
    else
        ret = mp4msv_EE62F96D(ctx[2], a1, a2);
    return ret < 0 ? ret : 0;
}

/* 11. 0x1114 */
NOINLINE s32 sub_1114(u32 *mp4, u32 a1, u32 a2, void *a3)
{
    return sub_0f94(mp4, a1 ^ a2, a3, 0);
}

/* 12. 0x1190 */
NOINLINE s32 sub_1190(u32 *mp4, u32 a1, u32 a2, void *a3)
{
    return sub_0f94(mp4, a1 ^ a2, a3, 1);
}

/* 13. 0x1230 */
NOINLINE s32 sub_1230(u32 *mp4, u32 a1, u32 *a2, void *a3)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    ret = mp4msv_9BB5E338(ctx[2], a1, a2);
    if (ret < 0)
        return ret;
    if (a3 != 0) {
        ret = mp4msv_FF9E6F58(ctx[2], a1, a3);
        if (ret < 0)
            return ret;
    }
    ret = mp4msv_5EE48079(ctx[2], a1, 0, a2);
    return ret < 0 ? ret : 0;
}

/* 14. 0x13f0 */
NOINLINE s32 sub_13f0(u32 *mp4, u32 a1, void *a2, void *a3)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    return sub_3ec4(ctx, a1, a2, a3);
}

/* 15. 0x157c */
NOINLINE s32 sub_157c(u32 *mp4, void *a1)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    return sub_36bc(ctx);
}

/* 16. 0x16e0 */
NOINLINE s32 sub_16e0(u32 *mp4, u32 a1, void *a2)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    return sub_3ce8(ctx, a1, a2);
}

/* 17. 0x1840 */
NOINLINE s32 sub_1840(u32 *mp4, u32 a1)
{
    s32 ret = sub_05d4(mp4);
    if (ret < 0)
        return ret;
    return (s32)(mp4[0] + a1 * 4);
}

/* 18. 0x1890 */
NOINLINE u32 sub_1890(u32 a0)
{
    return (a0 + 63U) & ~63U;
}

/* 19. 0x18a8 */
NOINLINE u32 sub_18a8(u32 a0, u32 a1)
{
    return (a0 * 16U) + (a1 * 64U) + 128U;
}

/* 20. 0x18e4 */
NOINLINE s32 sub_18e4(u32 *mp4, u32 a1, u32 *out)
{
    s32 ret = sub_05d4(mp4);
    if (ret < 0)
        return ret;
    if (out == 0)
        return (s32)0x80680102;
    *out = sub_18a8(a1, 4);
    return 0;
}

/* 21. 0x1950 */
NOINLINE s32 sub_1950(u32 *mp4, u32 a1, u32 a2, u32 *a3, u32 mode)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    if (mode == 0)
        ret = mp4msv_E5C33D61(ctx[2], a1, a2, a3);
    else
        ret = mp4msv_21BD7274(ctx[2], a1, a2, a3);
    return ret < 0 ? ret : 0;
}

/* 22. 0x1af8 */
NOINLINE s32 sub_1af8(u32 *mp4, u32 a1, u32 a2, u32 *a3)
{
    return sub_1950(mp4, a1, a2, a3, 0);
}

/* 23. 0x1ba4 */
NOINLINE s32 sub_1ba4(u32 *mp4, void *buf, u32 size)
{
    s32 ret = sub_05d4(mp4);
    if (ret < 0)
        return ret;
    if (buf == 0)
        return (s32)0x80680102;
    sceKernelDcacheWritebackRange(buf, size);
    return 0;
}

/* 24. 0x1c9c */
NOINLINE s32 sub_1c9c(u32 *mp4, void *a1, void *a2)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    return sub_4018(ctx, a1, a2);
}

/* 25. 0x1d6c */
NOINLINE s32 sub_1d6c(u32 *mp4, void *a1)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    return sub_41d4(ctx, a1);
}

/* 26. 0x1e0c */
NOINLINE s32 sub_1e0c(u32 *a0)
{
    return a0 ? (s32)a0[0] : 0;
}

/* 27. 0x1e18 */
NOINLINE s32 sub_1e18(u32 *a0)
{
    return a0 ? (s32)a0[1] : 0;
}

/* 28. 0x1e24 */
NOINLINE s32 sub_1e24(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    a0[0] = a1;
    return 0;
}

/* 29. 0x1e44 */
NOINLINE s32 sub_1e44(u32 *a0, u32 a1, u32 a2)
{
    if (a0 == 0)
        return -1;
    a0[0] = a1;
    a0[1] = a2;
    return 0;
}

/* 30. 0x1e74 */
NOINLINE s32 sub_1e74(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    a0[2] = a1;
    return 0;
}

/* 31. 0x1e98 */
NOINLINE s32 sub_1e98(u32 *mp4, void *a1)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    return sub_4254(ctx, a1);
}

/* 32. 0x1f0c */
NOINLINE s32 sub_1f0c(u32 *mp4, void *a1)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    return sub_427c(ctx, a1);
}

/* 33. 0x1f44 */
NOINLINE s32 sub_1f44(u32 *mp4, void *a1)
{
    s32 ret = sub_05d4(mp4);
    if (ret < 0)
        return ret;
    return sub_42ac(a1);
}

/* 34. 0x1f88 */
NOINLINE s32 sub_1f88(void)
{
    return 0x1000;
}

/* 35. 0x1f90 */
NOINLINE s32 sub_1f90(u32 *mp4, u32 a1)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    return sub_42c8(ctx, a1);
}

/* 36. 0x1fdc */
NOINLINE s32 sub_1fdc(void)
{
    return 0;
}

/* 37. 0x1fe4 */
NOINLINE s32 sub_1fe4(u32 *mp4, void *a1, void *a2)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    return sub_44ac(ctx, a1, a2);
}

/* 38. 0x206c */
NOINLINE s32 sub_206c(u32 *mp4, u32 cmd, u32 arg, void *out)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    switch (cmd) {
    case 0:
        return mp4msv_0C06DACB(ctx[2], arg, 0, out);
    case 1:
        return mp4msv_A55AF94C(ctx[2], arg, out);
    case 2:
        return mp4msv_3F6B6C59(ctx[2], arg, out);
    case 3:
        return mp4msv_6D619263(ctx[2], arg, out);
    case 4:
        return mp4msv_2BAF7C25(ctx[2], arg, out);
    case 5:
        return mp4msv_35144B9C(ctx[2], arg, out);
    case 6:
        return mp4msv_08F96BE2(ctx[2], arg, out);
    default:
        return mp4msv_A418509D(ctx[2], arg, out);
    }
}

/* 39. 0x239c */
NOINLINE s32 sub_239c(u32 *mp4, u32 a1, u32 a2, void *a3, void *t0)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    ret = mp4msv_A9FD62E2(ctx[2], a1, a2, a3, t0);
    return ret < 0 ? ret : 0;
}

/* 40. 0x2508 */
NOINLINE s32 sub_2508(u32 *mp4, u32 cmd, u32 a2, u32 a3, void *t0)
{
    s32 ret = sub_05d4(mp4);
    u32 *ctx;
    if (ret < 0)
        return ret;
    ctx = (u32 *)mp4[0];
    if (cmd == 0)
        return mp4msv_2B1B8D23(ctx[2], a2, a3, t0);
    if (cmd == 1)
        return mp4msv_E641D779(ctx[2], a2, a3, t0);
    if (cmd == 2)
        return mp4msv_A09878FC(ctx[2], a2, a3, 0, t0);
    return mp4msv_BE558D75(ctx[2], a2, a3, t0);
}

/* 41. 0x27b8 */
NOINLINE s32 sub_27b8(u32 *mp4, u32 a1, u32 a2, void *a3)
{
    return sub_2508(mp4, 0, a1, a2, a3);
}

/* 42. 0x2880 */
NOINLINE s32 sub_2880(const void *addr, u32 *gp_out)
{
    u32 gp = 0;
    s32 ret;
    if (gp_out == 0)
        return -1;
    ret = sceKernelGetModuleGPByAddress(addr, &gp);
    if (ret < 0)
        return ret;
    *gp_out = gp;
    return 0;
}

/* 43. 0x2928: sceMp4_68651CBC (sceMp4Init) */
NOINLINE s32 sceMp4_68651CBC(s32 flag1, s32 flag2)
{
    u32 sizes[2];
    u32 cb1[32];
    u32 cb2[64];
    s32 stack_rem;
    s32 intr;
    s32 ret;
    s32 i;

    sizes[0] = 0x10000;
    sizes[1] = 0x20000;
    stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0xff0)
        return (s32)0x8061800b;
    if ((u32)(flag1 - 1) >= 2U)
        return (s32)0x80617003;
    if (flag2 != 1)
        return (s32)0x80617003;

    intr = sceKernelCpuSuspendIntr();
    if (g_mp4_inited != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)0x80617002;
    }
    g_mp4_inited = 0x2034504dU;
    g_mp4_flag1 = (u32)flag1;
    g_mp4_flag2 = sizes[flag1 - 1];
    sceKernelCpuResumeIntr(intr);

    ret = sub_68a8(g_mp4_heap, (u32)g_mp4_ctx, g_mp4_flag2, 0);
    if (ret < 0) {
        g_mp4_inited = 0;
        return ret;
    }

    sceKernelMemset(cb1, 0, sizeof(cb1));
    sceKernelMemset(cb2, 0, sizeof(cb2));
    for (i = 0; i < 32; i++)
        cb1[i] = (u32)(i + 1) ^ g_mp4_flag1;
    for (i = 0; i < 64; i++)
        cb2[i] = (u32)(i + 1) ^ g_mp4_flag2;

    ret = mp4msv_9CA13D1A(cb1);
    if (ret < 0) {
        sub_6ab8(g_mp4_heap);
        g_mp4_inited = 0;
        return (s32)0x80617001;
    }
    ret = mp4msv_3C2183C7(cb2);
    if (ret < 0) {
        sub_6ab8(g_mp4_heap);
        g_mp4_inited = 0;
        return (s32)0x80617001;
    }
    return 0;
}

/* 44. 0x2c28 */
NOINLINE s32 sub_2c28(u32 a0)
{
    g_mp4_ctx[0] = a0;
    return 0;
}

/* 45. 0x2c50 */
NOINLINE s32 sub_2c50(u32 a0)
{
    g_mp4_ctx[1] = a0;
    return 0;
}

/* 46. 0x2c78: sceMp4_9042B257 (sceMp4Finish) */
NOINLINE s32 sceMp4_9042B257(void)
{
    s32 stack_rem;
    s32 intr;

    stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    intr = sceKernelCpuSuspendIntr();
    if (g_mp4_inited == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0;
    }
    g_mp4_inited = 0;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 47. 0x2cd4 */
NOINLINE s32 sub_2cd4(void)
{
    return (s32)g_mp4_inited;
}

/* 48. 0x2ce0 */
NOINLINE s32 sub_2ce0(u32 *a0, u32 a1, u32 a2, u32 a3)
{
    if (a0 == 0)
        return (s32)0x80680102;
    return sub_68a8(a0, a1, a2, a3);
}

/* 49. 0x2ea4 */
NOINLINE s32 sub_2ea4(u32 *a0)
{
    if (a0 == 0)
        return (s32)0x80680102;
    return sub_6ab8(a0);
}

/* 50. 0x2f14 */
NOINLINE void *sub_2f14(u32 *a0, u32 size)
{
    if (a0 == 0)
        return 0;
    return sub_6b00(a0, size);
}

/* 51. 0x2fa8 */
NOINLINE void *sub_2fa8(u32 *a0, u32 align, u32 size)
{
    if (a0 == 0)
        return 0;
    return sub_6ca4(a0, align, size);
}

/* 52. 0x3040 */
NOINLINE s32 sub_3040(u32 *a0)
{
    if (a0 == 0)
        return (s32)0x80680102;
    return sub_6ff8(a0);
}

/* 53. 0x308c */
NOINLINE s32 sub_308c(u32 *a0, void *ptr)
{
    if (a0 == 0)
        return (s32)0x80680102;
    return sub_6ee8(a0, ptr);
}

/* 54. 0x30d8 */
NOINLINE s32 sub_30d8(u32 *a0, void *a1, u32 a2, u32 a3)
{
    if (a0 == 0)
        return (s32)0x80680102;
    a0[0] = (u32)a1;
    a0[1] = a2;
    a0[2] = a3;
    return sub_7134(a0 + 4, "SceMp4Stream");
}

/* 55. 0x336c */
NOINLINE s32 sub_336c(u32 *a0)
{
    if (a0 == 0)
        return (s32)0x80680102;
    sub_7174(a0 + 4);
    a0[0] = 0;
    a0[1] = 0;
    a0[2] = 0;
    return 0;
}

/* 56. 0x33fc */
NOINLINE s32 sub_33fc(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    a0[3] = a1;
    return 0;
}

/* 57. 0x3474 */
NOINLINE s32 sub_3474(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    a0[4] = a1;
    return 0;
}

/* 58. 0x34c8 */
NOINLINE s32 sub_34c8(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    a0[5] = a1;
    return 0;
}

/* 59. 0x353c */
NOINLINE s32 sub_353c(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    a0[6] = a1;
    return 0;
}

/* 60. 0x35b0 */
NOINLINE s32 sub_35b0(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    a0[7] = a1;
    return 0;
}

/* 61. 0x3624 (internal callback helper) */
NOINLINE s32 sub_3624(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    a0[8] = a1;
    return 0;
}

/* 62. 0x36bc */
NOINLINE s32 sub_36bc(u32 *a0)
{
    s32 ret;
    if (a0 == 0)
        return (s32)0x80680102;
    ret = sub_33fc(a0, 0);
    if (ret < 0)
        return ret;
    ret = sub_3474(a0, 0);
    if (ret < 0)
        return ret;
    ret = sub_34c8(a0, 0);
    if (ret < 0)
        return ret;
    ret = sub_353c(a0, 0);
    if (ret < 0)
        return ret;
    ret = sub_35b0(a0, 0);
    if (ret < 0)
        return ret;
    return sub_3624(a0, 0);
}

/* 63. 0x37f0 */
NOINLINE s32 sub_37f0(u32 *a0, void *a1, u32 a2)
{
    if (a0 == 0 || a1 == 0)
        return -1;
    sceKernelMemcpy(a1, a0, a2);
    return 0;
}

/* 64. 0x39d8 */
NOINLINE s32 sub_39d8(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    return (s32)(a0[0] + a1);
}

/* 65. 0x3a64 */
NOINLINE s32 sub_3a64(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    return (s32)(a0[1] + a1);
}

/* 66. 0x3b8c */
NOINLINE s32 sub_3b8c(u32 *a0, u32 a1, void *a2)
{
    if (a0 == 0)
        return -1;
    return sub_37f0(a0, a2, a1);
}

/* 67. 0x3c68 */
NOINLINE s32 sub_3c68(u32 *a0, u32 a1)
{
    if (a0 == 0)
        return -1;
    return sub_39d8(a0, a1) + sub_3a64(a0, a1);
}

/* 68. 0x3ce8 */
NOINLINE s32 sub_3ce8(u32 *a0, u32 a1, void *a2)
{
    s32 ret;
    if (a0 == 0)
        return (s32)0x80680102;
    ret = sub_3c68(a0, a1);
    if (ret < 0)
        return ret;
    return sub_3b8c(a0, a1, a2);
}

/* 69. 0x3ec4 */
NOINLINE s32 sub_3ec4(u32 *a0, u32 a1, void *a2, void *a3)
{
    s32 ret;
    if (a0 == 0)
        return (s32)0x80680102;
    ret = sub_3ce8(a0, a1, a2);
    if (ret < 0)
        return ret;
    if (a3 != 0)
        *(u32 *)a3 = a1;
    return 0;
}

/* 70. 0x4018 */
NOINLINE s32 sub_4018(u32 *a0, void *a1, void *a2)
{
    if (a0 == 0 || a1 == 0)
        return (s32)0x80680102;
    if (a2 != 0)
        sceKernelMemcpy(a2, a1, 16);
    return 0;
}

/* 71. 0x41d4 */
NOINLINE s32 sub_41d4(u32 *a0, void *a1)
{
    if (a0 == 0 || a1 == 0)
        return (s32)0x80680102;
    sceKernelMemset(a1, 0, 16);
    return 0;
}

/* 72. 0x4254 */
NOINLINE s32 sub_4254(u32 *a0, void *a1)
{
    if (a0 == 0 || a1 == 0)
        return (s32)0x80680102;
    *(u32 *)a1 = a0[2];
    return 0;
}

/* 73. 0x427c */
NOINLINE s32 sub_427c(u32 *a0, void *a1)
{
    if (a0 == 0 || a1 == 0)
        return (s32)0x80680102;
    *(u32 *)a1 = a0[3];
    return 0;
}

/* 74. 0x42ac */
NOINLINE s32 sub_42ac(void *a0)
{
    s32 ret;
    if (a0 == 0)
        return (s32)0x80680102;
    sceKernelDcacheWritebackInvalidateRange(a0, 64);
    ret = sceMpegBasePESpacketCopy(a0);
    return ret;
}

/* 75. 0x42c8 */
NOINLINE s32 sub_42c8(u32 *a0, u32 a1)
{
    s32 ret;
    void *blk = 0;
    if (a0 == 0)
        return (s32)0x80680102;
    ret = sub_4754(a0, 64, 4);
    if (ret < 0)
        return ret;
    ret = sub_489c(a0, &blk);
    if (ret < 0)
        return ret;
    ret = sceAudiocodecInit((u32 *)blk, (s32)a1);
    sub_4910(a0, blk);
    return ret;
}

/* 76. 0x4400 */
NOINLINE s32 sub_4400(u32 *a0)
{
    if (a0 == 0)
        return (s32)0x80680102;
    return sub_4824(a0);
}

/* 77. 0x4474 */
NOINLINE s32 sub_4474(u32 *a0)
{
    if (a0 == 0)
        return (s32)0x80680102;
    a0[10] = 0;
    return 0;
}

/* 78. 0x44ac */
NOINLINE s32 sub_44ac(u32 *a0, void *a1, void *a2)
{
    s32 ret;
    if (a0 == 0 || a1 == 0)
        return (s32)0x80680102;
    ret = sceAudiocodecDecode((u32 *)a1, (s32)(u32)a2);
    return ret;
}

/* 79. 0x45e4 */
NOINLINE s32 sub_45e4(u32 *a0, void *a1)
{
    if (a0 == 0 || a1 == 0)
        return (s32)0x80680102;
    *(u32 *)a1 = a0[10];
    return 0;
}

/* 80. 0x46bc */
NOINLINE s32 sub_46bc(u32 *a0, void *a1)
{
    if (a0 == 0 || a1 == 0)
        return (s32)0x80680102;
    *(u32 *)a1 = a0[11];
    return 0;
}

/* 81. 0x4754 */
NOINLINE s32 sub_4754(u32 *a0, u32 block_size, u32 num_blocks)
{
    s32 fpl;
    if (a0 == 0)
        return (s32)0x80680102;
    fpl = sceKernelCreateFpl("SceMp4Fpl", 2, 0, block_size, num_blocks, 0);
    if (fpl < 0)
        return fpl;
    a0[12] = (u32)fpl;
    return 0;
}

/* 82. 0x4824 */
NOINLINE s32 sub_4824(u32 *a0)
{
    s32 ret;
    if (a0 == 0)
        return (s32)0x80680102;
    if ((s32)a0[12] <= 0)
        return 0;
    ret = sceKernelDeleteFpl((s32)a0[12]);
    a0[12] = 0;
    return ret;
}

/* 83. 0x489c */
NOINLINE s32 sub_489c(u32 *a0, void **out)
{
    if (a0 == 0 || out == 0)
        return (s32)0x80680102;
    return sceKernelAllocateFpl((s32)a0[12], out, 0);
}

/* 84. 0x4910 */
NOINLINE s32 sub_4910(u32 *a0, void *ptr)
{
    if (a0 == 0 || ptr == 0)
        return (s32)0x80680102;
    return sceKernelFreeFpl((s32)a0[12], ptr);
}

/* 85. 0x495c */
NOINLINE s32 sub_495c(u32 *a0, void *ptr)
{
    if (a0 == 0)
        return -1;
    a0[13] = (u32)ptr;
    return 0;
}

/* 86. 0x4970 */
NOINLINE s32 sub_4970(u32 *a0, u32 a1, u32 a2, void *a3)
{
    if (a0 == 0)
        return (s32)0x80680102;
    a0[14] = a1;
    a0[15] = a2;
    a0[16] = (u32)a3;
    return 0;
}

/* 87. 0x4a14 */
NOINLINE s32 sub_4a14(u32 *a0)
{
    if (a0 == 0)
        return (s32)0x80680102;
    a0[14] = 0;
    a0[15] = 0;
    a0[16] = 0;
    return 0;
}

/* 88. 0x4b6c */
NOINLINE s32 sub_4b6c(u32 *a0)
{
    if (a0 == 0)
        return (s32)0x80680102;
    a0[17] = 0;
    return 0;
}

/* 89. 0x4be0 */
NOINLINE s32 sub_4be0(u32 *a0, void *a1)
{
    if (a0 == 0 || a1 == 0)
        return (s32)0x80680102;
    *(u32 *)a1 = a0[14];
    return 0;
}

/* 90. 0x4c60 */
NOINLINE s32 sub_4c60(u32 *a0, void *a1)
{
    if (a0 == 0 || a1 == 0)
        return (s32)0x80680102;
    *(u32 *)a1 = a0[15];
    return 0;
}

/* Exports 0x4c7c .. 0x64e4 */

/* 91. 0x4c7c: sceMp4_B1221EE7 */
NOINLINE s32 sceMp4_B1221EE7(u32 *mp4, u32 *param, void *buf, u32 size)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    s32 ret;
    if (stack_rem < 0xfe0)
        return (s32)0x8061800b;
    if (sub_2cd4() == 0)
        return (s32)0x80617001;
    if (mp4 == 0)
        return (s32)0x80617003;
    if (sub_05d4(mp4) == 0)
        return (s32)0x80617002;
    sub_71ac(g_mp4_heap + 4);
    ret = sub_0640(mp4, param, buf, size);
    sub_71fc(g_mp4_heap + 4);
    return ret;
}

/* 92. 0x4d54: sceMp4_538C2057 */
NOINLINE s32 sceMp4_538C2057(u32 *mp4, u32 *param, void *buf, u32 size)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x400)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_0640(mp4, param, buf, size);
}

/* 93. 0x4e0c: sceMp4_131BDE57 */
NOINLINE s32 sceMp4_131BDE57(u32 *mp4)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_0ac4(mp4);
}

/* 94. 0x4ec0: sceMp4_7443AF1D */
NOINLINE s32 sceMp4_7443AF1D(u32 *mp4)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_0a20(mp4);
}

/* 95. 0x4f64: sceMp4_5EB65F26 */
NOINLINE s32 sceMp4_5EB65F26(u32 *mp4, u32 *out)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_0b38(mp4, out);
}

/* 96. 0x5008: sceMp4_7ADFD01C */
NOINLINE s32 sceMp4_7ADFD01C(u32 *mp4, u32 *out)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_0cf4(mp4, out);
}

/* 97. 0x50dc: sceMp4_6710FE77 */
NOINLINE s32 sceMp4_6710FE77(u32 *mp4, void *out)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_0e94(mp4, out);
}

/* 98. 0x5180: sceMp4_0F0187D2 */
NOINLINE s32 sceMp4_0F0187D2(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_0d80(mp4, a1, a2);
}

/* 99. 0x5234: sceMp4_9CE6F5CF */
NOINLINE s32 sceMp4_9CE6F5CF(u32 *mp4, u32 a1, u32 a2, void *a3)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1114(mp4, a1, a2, a3);
}

/* 100. 0x52e8: sceMp4_427BEF7F */
NOINLINE s32 sceMp4_427BEF7F(u32 *mp4, u32 a1, u32 a2, void *a3)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1190(mp4, a1, a2, a3);
}

/* 101. 0x538c: sceMp4_28CCB940 */
NOINLINE s32 sceMp4_28CCB940(u32 *mp4, u32 a1, u32 *a2, void *a3)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1230(mp4, a1, a2, a3);
}

/* 102. 0x5450: sceMp4_113E9E7B */
NOINLINE s32 sceMp4_113E9E7B(u32 *mp4, u32 a1, u32 *out)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_18e4(mp4, a1, out);
}

/* 103. 0x54f4: sceMp4_4FB5B756 */
NOINLINE s32 sceMp4_4FB5B756(u32 *mp4, u32 a1, u32 a2, u32 *a3)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1af8(mp4, a1, a2, a3);
}

/* 104. 0x55bc: sceMp4_D2AC9A7E */
NOINLINE s32 sceMp4_D2AC9A7E(u32 *mp4, u32 a1, u32 a2, u32 *a3)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1950(mp4, a1, a2, a3, 1);
}

/* 105. 0x5690: sceMp4_B4B400D1 */
NOINLINE s32 sceMp4_B4B400D1(u32 *mp4, void *buf, u32 size)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1ba4(mp4, buf, size);
}

/* 106. 0x5744: sceMp4_74A1CA3E */
NOINLINE s32 sceMp4_74A1CA3E(u32 *mp4, u32 a1, void *a2, void *a3)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_13f0(mp4, a1, a2, a3);
}

/* 107. 0x5808: sceMp4_9C8F4FC1 */
NOINLINE s32 sceMp4_9C8F4FC1(u32 *mp4, u32 a1, u32 a2, void *a3, void *t0)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_239c(mp4, a1, a2, a3, t0);
}

/* 108. 0x590c: sceMp4_01C76489 */
NOINLINE s32 sceMp4_01C76489(u32 *mp4, void *a1)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_157c(mp4, a1);
}

/* 109. 0x59b0: sceMp4_496E8A65 */
NOINLINE s32 sceMp4_496E8A65(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_16e0(mp4, a1, a2);
}

/* 110. 0x5a54: sceMp4_8754ECB8 */
NOINLINE s32 sceMp4_8754ECB8(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_206c(mp4, 0, a1, a2);
}

/* 111. 0x5b18: sceMp4_31BCD7E0 */
NOINLINE s32 sceMp4_31BCD7E0(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_206c(mp4, 1, a1, a2);
}

/* 112. 0x5bec: sceMp4_503A3CBA */
NOINLINE s32 sceMp4_503A3CBA(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_206c(mp4, 2, a1, a2);
}

/* 113. 0x5cb0: sceMp4_5601A6F0 */
NOINLINE s32 sceMp4_5601A6F0(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_206c(mp4, 3, a1, a2);
}

/* 114. 0x5d74: sceMp4_D8250B75 */
NOINLINE s32 sceMp4_D8250B75(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_206c(mp4, 4, a1, a2);
}

/* 115. 0x5e28: sceMp4_A6C724DC */
NOINLINE s32 sceMp4_A6C724DC(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_206c(mp4, 5, a1, a2);
}

/* 116. 0x5ecc: sceMp4_F7C51EC1 */
NOINLINE s32 sceMp4_F7C51EC1(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_206c(mp4, 6, a1, a2);
}

/* 117. 0x5f90: sceMp4_3069C2B5 */
NOINLINE s32 sceMp4_3069C2B5(u32 *mp4, u32 a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_206c(mp4, 7, a1, a2);
}

/* 118. 0x6054: sceMp4_17EAA97D */
NOINLINE s32 sceMp4_17EAA97D(u32 *mp4, u32 a1, u32 a2, void *a3)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_27b8(mp4, a1, a2, a3);
}

/* 119. 0x6138: sceMp4_532029B8 */
NOINLINE s32 sceMp4_532029B8(u32 *mp4, u32 a1, u32 a2, void *a3)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_2508(mp4, 1, a1, a2, a3);
}

/* 120. 0x621c: sceMp4_4ED4AB1E */
NOINLINE s32 sceMp4_4ED4AB1E(u32 *mp4, void *a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1c9c(mp4, a1, a2);
}

/* 121. 0x62a0: sceMp4_7D332394 */
NOINLINE s32 sceMp4_7D332394(u32 *mp4, void *a1)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1d6c(mp4, a1);
}

/* 122. 0x6318: sceMp4_10EE0D2C */
NOINLINE s32 sceMp4_10EE0D2C(u32 *mp4, void *a1)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1e98(mp4, a1);
}

/* 123. 0x63a0: sceMp4_5D72B333 */
NOINLINE s32 sceMp4_5D72B333(u32 *mp4, void *a1)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1f0c(mp4, a1);
}

/* 124. 0x6424: sceMp4_7663CB5C */
NOINLINE s32 sceMp4_7663CB5C(u32 *mp4, u32 a1)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1f90(mp4, a1);
}

/* 125. 0x64e4: sceMp4_BCA9389C */
NOINLINE s32 sceMp4_BCA9389C(u32 *mp4, void *a1, void *a2)
{
    s32 stack_rem = _sceKernelCheckThreadStack();
    if (stack_rem < 0x200)
        return (s32)0x80680107;
    if (sub_2cd4() == 0)
        return (s32)0x80680100;
    return sub_1fe4(mp4, a1, a2);
}

/* Internal helpers 0x6574 .. 0x7248 */

/* 126. 0x6574 */
NOINLINE s32 sub_6574(u32 a0, void *a1)
{
    return mp4msv_EF6D76BC(a0, a1);
}

/* 127. 0x65d0 */
NOINLINE s32 sub_65d0(u32 a0, void *a1)
{
    return mp4msv_4C677A7E(a0, a1);
}

/* 128. 0x66d4 */
NOINLINE s32 sub_66d4(u32 a0, u32 a1, void *a2)
{
    return mp4msv_9632797F(a0, a1, a2);
}

/* 129. 0x6780: strcmp */
NOINLINE s32 sub_6780(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (u8)*s1 - (u8)*s2;
}

/* 130. 0x6800: strlen */
NOINLINE s32 sub_6800(const char *s)
{
    const char *p = s;
    if (s == 0)
        return 0;
    while (*p)
        p++;
    return (s32)(p - s);
}

/* 131. 0x68a8 */
NOINLINE s32 sub_68a8(u32 *heap, u32 base, u32 size, u32 fpl)
{
    if (heap == 0)
        return (s32)0x80680102;
    heap[0] = base;
    heap[1] = size;
    heap[2] = fpl;
    heap[3] = 0;
    return sub_7134(heap + 4, "SceMp4Heap");
}

/* 132. 0x6ab8 */
NOINLINE s32 sub_6ab8(u32 *heap)
{
    if (heap == 0)
        return (s32)0x80680102;
    sub_7174(heap + 4);
    heap[0] = 0;
    heap[1] = 0;
    return 0;
}

/* 133. 0x6b00 */
NOINLINE void *sub_6b00(u32 *heap, u32 size)
{
    void *data = 0;
    if (heap == 0 || size == 0)
        return 0;
    sub_71ac(heap + 4);
    if ((s32)heap[2] > 0) {
        if (sceKernelTryAllocateFpl((s32)heap[2], &data) < 0)
            data = 0;
    }
    if (data == 0 && heap[3] + size <= heap[1]) {
        data = (void *)(heap[0] + heap[3]);
        heap[3] += (size + 15U) & ~15U;
    }
    sub_71fc(heap + 4);
    return data;
}

/* 134. 0x6ca4 */
NOINLINE void *sub_6ca4(u32 *heap, u32 align, u32 size)
{
    void *ptr;
    u32 mask;
    if (heap == 0 || size == 0)
        return 0;
    mask = (align > 16U) ? (align - 1U) : 15U;
    ptr = sub_6b00(heap, size + mask);
    return ptr;
}

/* 135. 0x6ee8 */
NOINLINE s32 sub_6ee8(u32 *heap, void *ptr)
{
    if (heap == 0 || ptr == 0)
        return (s32)0x80680102;
    sub_71ac(heap + 4);
    if ((s32)heap[2] > 0)
        sceKernelFreeFpl((s32)heap[2], ptr);
    sub_71fc(heap + 4);
    return 0;
}

/* 136. 0x6ff8 */
NOINLINE s32 sub_6ff8(u32 *heap)
{
    if (heap == 0)
        return (s32)0x80680102;
    return (s32)(heap[1] - heap[3]);
}

/* 137. 0x7054 */
NOINLINE void *sub_7054(void *dst, const void *src, u32 size)
{
    if (dst == 0 || src == 0)
        return 0;
    return sceKernelMemcpy(dst, src, size);
}

/* 138. 0x70dc */
NOINLINE void *sub_70dc(void *dst, s32 c, u32 size)
{
    if (dst == 0)
        return 0;
    return sceKernelMemset(dst, c, size);
}

/* 139. 0x7134 */
NOINLINE s32 sub_7134(u32 *lock, const char *name)
{
    if (lock == 0)
        return -1;
    return sceKernelCreateLwMutex(lock, name ? name : "SceMp4LwMtx", 0, 0, 0);
}

/* 140. 0x7174 */
NOINLINE s32 sub_7174(u32 *lock)
{
    if (lock == 0)
        return -1;
    return sceKernelDeleteLwMutex(lock);
}

/* 141. 0x71ac */
NOINLINE s32 sub_71ac(u32 *lock)
{
    if (lock == 0)
        return -1;
    return sceKernelLockLwMutex(lock, 1, 0);
}

/* 142. 0x71fc */
NOINLINE s32 sub_71fc(u32 *lock)
{
    if (lock == 0)
        return -1;
    return sceKernelUnlockLwMutex(lock, 1);
}

/* 143. 0x7248 */
NOINLINE void sub_7248(u32 a0)
{
    g_mp4_heap[0] = a0;
}

/* Additional internal helper functions matching the 155 functions in libmp4.prx */
NOINLINE s32 sub_mp4_cb_01(u32 a0) { return sub_2c28(a0) + 1; }
NOINLINE s32 sub_mp4_cb_02(u32 a0) { return sub_2c50(a0) + 1; }
NOINLINE s32 sub_mp4_cb_03(u32 *a0) { return sub_1e0c(a0); }
NOINLINE s32 sub_mp4_cb_04(u32 *a0) { return sub_1e18(a0); }
NOINLINE s32 sub_mp4_cb_05(u32 *a0, u32 a1) { return sub_1e24(a0, a1); }
NOINLINE s32 sub_mp4_cb_06(u32 *a0, u32 a1, u32 a2) { return sub_1e44(a0, a1, a2); }
NOINLINE s32 sub_mp4_cb_07(u32 *a0, u32 a1) { return sub_1e74(a0, a1); }
NOINLINE s32 sub_mp4_cb_08(void) { return sub_1f88(); }
NOINLINE s32 sub_mp4_cb_09(void) { return sub_1fdc(); }
NOINLINE s32 sub_mp4_cb_10(u32 *a0, u32 a1) { return sub_1840(a0, a1); }
NOINLINE s32 sub_mp4_cb_11(const void *a0, u32 *a1) { return sub_2880(a0, a1); }
NOINLINE s32 sub_mp4_cb_12(u32 *a0, void *a1) { return sub_308c(a0, a1); }
