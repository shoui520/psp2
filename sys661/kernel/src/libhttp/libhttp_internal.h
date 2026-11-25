#ifndef LIBHTTP_INTERNAL_H
#define LIBHTTP_INTERNAL_H

#include <stddef.h>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;

static inline u32 byteswap32(u32 v)
{
    return ((v & 0x000000FFU) << 24) |
           ((v & 0x0000FF00U) << 8)  |
           ((v & 0x00FF0000U) >> 8)  |
           ((v & 0xFF000000U) >> 24);
}

static inline u16 byteswap16(u16 v)
{
    return (u16)((v << 8) | (v >> 8));
}

static inline u16 _wsbh(u16 v) { return byteswap16(v); }
static inline s32 http_min_s32(s32 a, s32 b) { return a < b ? a : b; }
static inline s32 http_max_s32(s32 a, s32 b) { return a > b ? a : b; }

#define LODWORD(x) ((u32)((u64)(x)))
#define HIDWORD(x) ((u32)((u64)(x) >> 32))
#define SHIDWORD(x) ((s32)((u64)(x) >> 32))
#define SLODWORD(x) ((s32)((u64)(x)))
#define LOWORD(x)  ((u16)((u32)(x)))
#define HIWORD(x)  ((u16)((u32)(x) >> 16))
#define LOBYTE(x)  ((u8)((u32)(x)))
#define HIBYTE(x)  ((u8)((u32)(x) >> 8))
#define BYTE1(x)   ((u8)((u32)(x) >> 8))
#define BYTE2(x)   ((u8)((u32)(x) >> 16))
#define BYTE3(x)   ((u8)((u32)(x) >> 24))
#define __PAIR64__(hi, lo) (((u64)(u32)(hi) << 32) | (u32)(lo))
#define __CFADD__(x, y)    ((u32)((u32)(x) + (u32)(y)) < (u32)(x))
#define true 1
#define false 0

#define _break(...) __asm__ volatile("break 7")

static inline void *qmemcpy(void *dst, const void *src, size_t n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

static inline char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != '\0') {}
    return dst;
}

extern u8 g_http_data[0x8C];
extern u8 g_http_bss[0x3600];

#define HTTP_DATA_U32(off) (*(u32 *)(g_http_data + (off)))
#define HTTP_DATA_U8(off)  (*(u8 *)(g_http_data + (off)))
#define HTTP_BSS_U32(off)  (*(u32 *)(g_http_bss + (off)))
#define HTTP_BSS_U8(off)   (*(u8 *)(g_http_bss + (off)))

extern const u8 g_oid_subject_alt_name[4];
extern const u8 g_b64_decode_tab[256];
extern const u8 g_clz_tab[256];
extern const u8 g_md5_empty_digest[16];

/* Imported library functions */
extern int _sceKernelCheckThreadStack();
extern int name();
extern int sceCertLoader_DD629A24();
extern int sceHttpStorage_driver_24AA94F4();
extern int sceHttpStorage_driver_2D8DAE58();
extern int sceHttpStorage_driver_700AAD44();
extern int sceHttpStorage_driver_B33389CE();
extern int sceHttpStorage_driver_CDA3D8F6();
extern int sceHttpStorage_driver_CDDF1103();
extern int sceKernelCpuResumeIntr();
extern int sceKernelCpuSuspendIntr();
extern int sceKernelCreateVpl();
extern int sceKernelDelayThread();
extern int sceKernelDeleteVpl();
extern int sceKernelFreeVpl();
extern int sceKernelGetCompiledSdkVersion();
extern int sceKernelGetSystemTimeLow();
extern int sceKernelMemcpy();
extern int sceKernelMemset();
extern int sceKernelQueryMemoryInfo();
extern int sceKernelTryAllocateVpl();
extern int sceKernelUtilsMd5BlockInit();
extern int sceKernelUtilsMd5BlockResult();
extern int sceKernelUtilsMd5BlockUpdate();
extern int sceKernelUtilsMd5Digest();
extern int sceNetApctlGetInfo();
extern int sceNetInetClose();
extern int sceNetInetConnect();
extern int sceNetInetGetErrno();
extern int sceNetInetGetPspError();
extern int sceNetInetGetTcpcbstat();
extern int sceNetInetGetsockname();
extern int sceNetInetGetsockopt();
extern int sceNetInetInetPton();
extern int sceNetInetRecv();
extern int sceNetInetSelect();
extern int sceNetInetSend();
extern int sceNetInetSetsockopt();
extern int sceNetInetSocket();
extern int sceNetInetSocketAbort();
extern int sceNetResolverCreate();
extern int sceNetResolverDelete();
extern int sceNetResolverStartNtoA();
extern int sceNetResolverStop();
extern int sceNet_lib_1FB2FDDD();
extern int sceNet_lib_2A73ADDC();
extern int sceNet_lib_5705F6F9();
extern int sceNet_lib_750F705D();
extern int sceNet_lib_80C9F02A();
extern int sceNet_lib_9CFBC7E3();
extern int sceNet_lib_A0F16ABD();
extern int sceNet_lib_B5CE388A();
extern int sceNet_lib_B9085A96();
extern int sceNet_lib_BCBE14CF();
extern int sceNet_lib_CF705E46();
extern int sceNet_lib_D8722983();
extern int sceNet_lib_E0A81C7C();
extern int sceNet_lib_F48963C6();
extern int sceParseHttpResponseHeader();
extern int sceParseHttpStatusLine();
extern int sceRtcFormatRFC2822();
extern int sceRtcGetCurrentTick();
extern int sceRtcParseDateTime();
extern int sceRtcTickAddSeconds();
extern int sceSsl_lib_0217E73C();
extern int sceSsl_lib_047AB6BB();
extern int sceSsl_lib_05E019A4();
extern int sceSsl_lib_0861D934();
extern int sceSsl_lib_0FF7BA68();
extern int sceSsl_lib_12C7718F();
extern int sceSsl_lib_28B4DE33();
extern int sceSsl_lib_2B7BD166();
extern int sceSsl_lib_341843AB();
extern int sceSsl_lib_3454E622();
extern int sceSsl_lib_37C7B76C();
extern int sceSsl_lib_3E040FD4();
extern int sceSsl_lib_3E3133D6();
extern int sceSsl_lib_42FFA860();
extern int sceSsl_lib_4BE9030A();
extern int sceSsl_lib_4C75B8E9();
extern int sceSsl_lib_5040E836();
extern int sceSsl_lib_529A9477();
extern int sceSsl_lib_54A7D8F3();
extern int sceSsl_lib_58352068();
extern int sceSsl_lib_588F2FE8();
extern int sceSsl_lib_5E5C873A();
extern int sceSsl_lib_6038DEDE();
extern int sceSsl_lib_69F1B678();
extern int sceSsl_lib_7317C1BC();
extern int sceSsl_lib_76A46EDA();
extern int sceSsl_lib_7BDAF864();
extern int sceSsl_lib_7D0E41F4();
extern int sceSsl_lib_80608663();
extern int sceSsl_lib_84833472();
extern int sceSsl_lib_8D484361();
extern int sceSsl_lib_9266C0D5();
extern int sceSsl_lib_9299C6CE();
extern int sceSsl_lib_92FA8B34();
extern int sceSsl_lib_A7BED83B();
extern int sceSsl_lib_A8F0AD39();
extern int sceSsl_lib_A920B39E();
extern int sceSsl_lib_AEBF278B();
extern int sceSsl_lib_B3B04C58();
extern int sceSsl_lib_B40D11EA();
extern int sceSsl_lib_B4D78E98();
extern int sceSsl_lib_B7CA8717();
extern int sceSsl_lib_B87FFE37();
extern int sceSsl_lib_B9C8CCE6();
extern int sceSsl_lib_BF55C31C();
extern int sceSsl_lib_C1A2E14B();
extern int sceSsl_lib_C85F6FCE();
extern int sceSsl_lib_CFA2F8EB();
extern int sceSsl_lib_D53ACF30();
extern int sceSsl_lib_D5ABC603();
extern int sceSsl_lib_E7BFC0EF();
extern int sceSsl_lib_E7C29542();
extern int sceSsl_lib_EBFB0E3C();
extern int sceSsl_lib_FB8273FE();
extern int sceUriBuild();
extern int sceUriParse();
extern int sceUriSweepPath();

/* Module functions */
int http_core_fn_1();
int http_core_fn_2();
int http_core_fn_3();
s64 http_core_fn_4();
s32 http_core_fn_5();
int sceHttpIsRequestInCache(int a1);
int sceHttpSendRequestInCacheFirstMode(int a1, int a2, int a3);
int sceHttpAbortRequest(int a1);
int sceHttpReadData(int a1, int a2, unsigned int a3);
int sceHttpSendRequest(int a1, int a2, int a3);
int sceHttpCreateTemplate(int a1, int a2, int a3);
int sceHttpDeleteTemplate(int a1);
int sceHttpCreateConnection(int a1, int a2, int a3, unsigned int a4, int a5);
int sceHttpCreateConnectionWithURL(int a1, int a2, int a3);
int sceHttpDeleteConnection(int a1);
int sceHttpCreateRequest(int a1, int a2, int a3, int a4, int a5, int a6);
int sceHttpCreateRequestWithURL(int a1, int a2, int a3, int a4, int a5, int a6);
int sceHttpDeleteRequest(int a1);
int sceHttp_4E4A284A(int a1);
int sceHttp_A909F2AE();
int http_core_fn_6();
s32 http_core_fn_7();
int http_core_fn_8();
int http_core_fn_9();
int (*http_core_fn_10(u8 *a1, unsigned int a2, u32 *a3, int *a4))(u8 *a1, u8 *a2, int a3);
int http_core_fn_11();
int http_core_fn_12();
int http_core_fn_13();
int http_core_fn_14();
int http_core_fn_15();
int http_core_fn_16();
int http_core_fn_17();
int sceHttpFlushAuthList();
int http_core_fn_18();
int http_core_fn_19();
void http_core_fn_20();
void http_core_fn_21();
void http_core_fn_22();
int * http_core_fn_23();
int * http_cookie_fn_1();
int http_cookie_fn_2();
int http_cookie_fn_3();
int sceHttp_77EE5319();
int sceHttp_DD6E7857();
int http_cookie_fn_4();
int http_cookie_fn_5();
unsigned int http_cookie_fn_6();
int http_cookie_fn_7();
unsigned int http_cookie_fn_8();
void http_cookie_fn_9();
int http_cookie_fn_10();
int sceHttpSaveSystemCookie();
int sceHttpLoadSystemCookie();
int sceHttpGetCookie(int a1, int a2, int a3, int a4, int a5);
int sceHttpAddCookie(int a1, int a2, int a3);
int http_cookie_fn_11();
int http_cookie_fn_12();
int http_cookie_fn_13();
int sceHttp_457D221D();
int * http_cookie_fn_14();
int http_cookie_fn_15();
int http_cookie_fn_16();
void http_cookie_fn_17();
int http_cookie_fn_18();
int http_cookie_fn_19();
int http_cookie_fn_20();
s32 http_cookie_fn_21();
int http_cookie_fn_22();
int http_cookie_fn_23();
int http_cookie_fn_24();
int http_cookie_fn_25();
int http_cookie_fn_26();
int http_cookie_fn_27();
int http_cookie_fn_28();
int http_cookie_fn_29();
int http_cookie_fn_30();
int http_cookie_fn_31();
int http_cookie_fn_32();
int http_ssl_fn_1();
int http_ssl_fn_2();
int http_ssl_fn_3();
int http_ssl_fn_4();
int http_ssl_fn_5();
void http_ssl_fn_6();
int http_ssl_fn_7();
int http_ssl_fn_8();
int http_ssl_fn_9();
int http_ssl_fn_10();
int sceHttpsInit(int a1, int a2, u32 *a3, u32 *a4);
int sceHttp_68AB0F86(int a1, int a2, int a3);
int sceHttpsLoadDefaultCert(int a1, int a2);
int http_ssl_fn_11();
int sceHttp_87F1E666(int a1, int a2);
int sceHttp_3C478044(int a1, int a2);
int http_ssl_fn_12();
int http_ssl_fn_13();
int http_ssl_fn_14();
int http_ssl_fn_15();
int sceHttpsGetSslError(int a1, u32 *a2, u32 *a3);
int sceHttpsSetSslCallback(int a1, int a2, int a3);
int sceHttpsGetCaList(u32 *a1, int *a2);
int http_ssl_fn_16();
int sceHttpsEnd();
int sceHttpsEnableOption(int a1);
int sceHttpsDisableOption(int a1);
int http_ssl_fn_17();
int http_ssl_fn_18();
int http_ssl_fn_19();
char * http_ssl_fn_20();
int http_ssl_fn_21();
int http_ssl_fn_22();
int http_ssl_fn_23();
int http_ssl_fn_24();
int http_ssl_fn_25();
s32 http_ssl_fn_26();
int http_ssl_fn_27();
int http_ssl_fn_28();
int http_opts_fn_1();
int http_opts_fn_2();
int http_opts_fn_3();
int sceHttpSetMallocFunction(int (*a1)(u32, u32, u32), int (*a2)(void), int a3);
int http_opts_fn_4();
int http_opts_fn_5();
int http_opts_fn_6();
int sceHttpSetResolveRetry(int a1, int a2);
int sceHttpSetResolveTimeOut(int a1, int a2);
int sceHttpSetConnectTimeOut(int a1, int a2);
int sceHttpSetSendTimeOut(int a1, int a2);
int sceHttpSetRecvTimeOut(int a1, int a2);
int sceHttp_CC920C12(int a1);
int sceHttp_D29163DA(int a1);
int sceHttpSetResHeaderMaxSize(int a1, int a2);
int sceHttpSetRecvBlockSize(int a1, int a2);
int sceHttpSetCacheContentLengthMaxSize(int a1, int a2);
int sceHttpEnableKeepAlive(int a1);
int sceHttpDisableKeepAlive(int a1);
int sceHttpEnableCookie(int a1);
int sceHttpDisableCookie(int a1);
int sceHttpEnableAuth(int a1);
int sceHttpDisableAuth(int a1);
int sceHttp_CDB0DC58(int a1);
int sceHttp_951D310E(int a1);
int sceHttp_D80BE761(int a1);
int sceHttp_C0E69162(int a1);
int sceHttpEnableRedirect(int a1);
int sceHttpDisableRedirect(int a1);
int sceHttpChangeHttpVersion(int a1, int a2);
int sceHttpDisableCache(int a1);
int sceHttpSetAuthInfoCB(int a1, int a2);
int sceHttpSetAuthInfoCallback(int a1, int a2, int a3);
int sceHttpSetRedirectCallback(int a1, int a2, int a3);
int sceHttpSetCookieSendCallback(int a1, int a2, int a3);
int sceHttpSetCookieRecvCallback(int a1, int a2, int a3);
int http_opts_fn_7();
int http_opts_fn_8();
int http_opts_fn_9();
int sceHttpSetProxy(int a1, int a2, int a3, int a4, int a5);
int sceHttpGetProxy(int a1, u32 *a2, u32 *a3, int a4, unsigned int a5, u16 *a6);
int sceHttpEnableCache(int a1);
int sceHttp_8046E250(int a1);
int sceHttpAddExtraHeader(int a1, int a2, int a3, int a4);
int sceHttpDeleteHeader(int a1, int a2);
int http_opts_fn_10();
int http_opts_fn_11();
int http_opts_fn_12();
int http_opts_fn_13();
int http_opts_fn_14();
int http_opts_fn_15();
int http_opts_fn_16();
int http_opts_fn_17();
void http_opts_fn_18();
void http_opts_fn_19();
void http_opts_fn_20();
void http_opts_fn_21();
int * http_opts_fn_22();
int sceHttpInit(int a1);
int sceHttpEnd();
int http_opts_fn_23();
int module_reboot_before();
int http_opts_fn_24();
int http_opts_fn_25();
int http_opts_fn_26();
int http_opts_fn_27();
int http_opts_fn_28();
int http_io_fn_1();
int http_io_fn_2();
int http_io_fn_3();
int http_io_fn_4();
int http_io_fn_5();
int http_io_fn_6();
int http_io_fn_7();
int http_io_fn_8();
int http_io_fn_9();
int http_io_fn_10();
int http_io_fn_11();
int http_io_fn_12();
int http_io_fn_13();
int http_io_fn_14();
int http_io_fn_15();
int http_io_fn_16();
int http_io_fn_17();
int http_io_fn_18();
int http_io_fn_19();
int http_io_fn_20();
int http_io_fn_21();
int http_io_fn_22();
int http_io_fn_23();
int http_io_fn_24();
int http_io_fn_25();
int http_io_fn_26();
// write access to const memory has been detected, the output may be wrong! int http_io_fn_27();
int http_io_fn_28();
int http_io_fn_29();
int http_io_fn_30();
int http_io_fn_31();
int http_io_fn_32();
int http_io_fn_33();
int http_io_fn_34();
void http_io_fn_35();
int http_io_fn_36();
int http_io_fn_37();
int http_io_fn_38();
int http_io_fn_39();
int http_io_fn_40();
int http_io_fn_41();
int http_io_fn_42();
int http_io_fn_43();
int http_io_fn_44();
int http_io_fn_45();
void http_io_fn_46();
int sceHttpInitCache(int a1);
int sceHttpFlushCache();
int sceHttpEndCache();
int sceHttp_739C2D79(int a1, unsigned int a2);
int sceHttp_B0257723();
int sceHttp_A461A167();
int http_io_fn_47();
int http_io_fn_48();
int http_io_fn_49();
int http_io_fn_50();
void http_io_fn_51();
int http_io_fn_52();
unsigned int http_io_fn_53();
unsigned int http_io_fn_54();
int http_io_fn_55();
void http_io_fn_56();
u32 * http_io_fn_57();
u32 * http_io_fn_58();
void http_io_fn_59();
void http_io_fn_60();
void http_io_fn_61();
void http_io_fn_62();
u32 * http_io_fn_63();
int http_io_fn_64();
int http_io_fn_65();
int http_io_fn_66();
int http_io_fn_67();
int http_io_fn_68();
int http_io_fn_69();
int http_io_fn_70();
int http_io_fn_71();
u32 * http_io_fn_72();
int http_io_fn_73();
int sceHttpGetContentLength(int a1, u32 *a2);
int sceHttpGetStatusCode(int a1, u32 *a2);
int sceHttpGetNetworkErrno(int a1, int *a2);
int sceHttpGetNetworkPspError(int a1, int *a2);
int sceHttpGetAllHeader(int a1, u32 *a2, u32 *a3);
int http_io_fn_74();
int http_io_fn_75();
int http_io_fn_76();
int http_io_fn_77();
int http_io_fn_78();
int http_io_fn_79();
int http_io_fn_80();
int http_io_fn_81();
int http_io_fn_82();
int http_io_fn_83();
int http_io_fn_84();
int http_io_fn_85();
int http_io_fn_86();
int http_io_fn_87();
int http_io_fn_88();
int http_io_fn_89();
int http_io_fn_90();
int http_io_fn_91();
int http_io_fn_92();
int http_io_fn_93();
int http_io_fn_94();
int http_io_fn_95();
int http_io_fn_96();
int http_io_fn_97();
int http_io_fn_98();
int http_io_fn_99();
int http_io_fn_100();
int http_io_fn_101();
int http_io_fn_102();
int http_io_fn_103();
int http_io_fn_104();
int http_io_fn_105();
int http_io_fn_106();
int http_io_fn_107();
int http_io_fn_108();
int http_io_fn_109();

#endif /* LIBHTTP_INTERNAL_H */
