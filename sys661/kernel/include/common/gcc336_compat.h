/* Compatibility builtins for reconstructed GCC 3.3.6 + Allegrex (sys661). */
#ifndef _SYS661_GCC336_COMPAT_H
#define _SYS661_GCC336_COMPAT_H

#if defined(__GNUC__) && __GNUC__ < 4

#define __builtin_assume_aligned(p, ...) (p)

static __inline__ void __builtin_allegrex_sync(void)
{
    __asm__ __volatile__("sync" ::: "memory");
}

#define __builtin_allegrex_cache(op, addr) \
    __asm__ __volatile__("cache %0, 0(%1)" :: "i"(op), "r"(addr) : "memory")

static __inline__ unsigned int __builtin_allegrex_clz(unsigned int x)
{
    unsigned int r;
    __asm__("clz %0, %1" : "=r"(r) : "r"(x));
    return r;
}

static __inline__ unsigned int __builtin_allegrex_clo(unsigned int x)
{
    unsigned int r;
    __asm__("clo %0, %1" : "=r"(r) : "r"(x));
    return r;
}

static __inline__ unsigned int __builtin_allegrex_bitrev(unsigned int x)
{
    unsigned int r;
    __asm__("bitrev %0, %1" : "=r"(r) : "r"(x));
    return r;
}

static __inline__ unsigned int __builtin_allegrex_wsbh(unsigned int x)
{
    unsigned int r;
    __asm__("wsbh %0, %1" : "=r"(r) : "r"(x));
    return r;
}

static __inline__ unsigned int __builtin_allegrex_wsbw(unsigned int x)
{
    unsigned int r;
    __asm__("wsbw %0, %1" : "=r"(r) : "r"(x));
    return r;
}

static __inline__ int __builtin_allegrex_min(int a, int b)
{
    int r;
    __asm__("min %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

static __inline__ int __builtin_allegrex_max(int a, int b)
{
    int r;
    __asm__("max %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

static __inline__ int __builtin_clz(unsigned int x)
{
    return (int)__builtin_allegrex_clz(x);
}

static __inline__ int __builtin_ctz(unsigned int x)
{
    return (int)__builtin_allegrex_clz(__builtin_allegrex_bitrev(x));
}

static __inline__ unsigned int __builtin_bswap32(unsigned int x)
{
    return __builtin_allegrex_wsbw(x);
}

static __inline__ unsigned short __builtin_bswap16(unsigned short x)
{
    return (unsigned short)__builtin_allegrex_wsbh((unsigned int)x);
}

#endif
#endif
