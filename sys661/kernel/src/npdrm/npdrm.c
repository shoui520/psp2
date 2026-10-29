/*
 * ePSP 6.61 SceNpDrm (`kd/npdrm.prx` / `scePspNpDrm_Driver`)
 * Reimplemented from the decrypted 6.61 flash0 image inside Vita 3.65 `pcff.skprx`.
 */

#include <stdint.h>
#include <stddef.h>

extern int sceIoOpen(const char *file, int flags, int mode);
extern int64_t sceIoLseek(int fd, int64_t offset, int whence);
extern int sceIoValidateFd(int fd, int arg1);
extern int sceIoIoctl(int fd, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);
extern int sceIoRead(int fd, void *data, int size);
extern int sceIoClose(int fd);

extern int sceKernelSetNpDrmGetModuleKeyFunction(void *func);
extern void *sceKernelMemcpy(void *dst, const void *src, unsigned int size);
extern void *sceKernelMemset(void *dst, int val, unsigned int size);

extern void *memset(void *dst, int val, size_t size);
extern size_t strlen(const char *s);
extern int memcmp(const void *s1, const void *s2, size_t n);
extern char *strncpy(char *dst, const char *src, size_t n);
extern int strcmp(const char *s1, const char *s2);
extern int snprintf(char *str, size_t size, const char *format, ...);

extern int sceKernelUnlockMutex(int uid, int count);
extern int sceKernelLockMutex(int uid, int count, unsigned int *timeout);
extern int sceKernelCreateMutex(const char *name, unsigned int attr, int init_count, void *opt);
extern int sceKernelExtendKernelStack(unsigned int size, int (*cb)(void *), void *arg);
extern int sceKernelDelayThread(unsigned int delay);
extern int sceKernelDeleteMutex(int uid);

extern int sceDrmBBMacInit(void *ctx, int type);
extern int sceDrmBBMacUpdate(void *ctx, const void *buf, int size);
extern int sceDrmBBMacFinal(void *ctx, void *out, const void *key);
extern int sceDrmBBMacFinal2(void *ctx, const void *mac, const void *key);

extern int sceDdrdbHash(const void *src, int size, void *digest);
extern int sceDdrdbSigvry(const void *pubkey, const void *digest, const void *sig);
extern int sceMgMemoryStick_driver_735526D6(void *out);
extern int sceOpenPSIDGetPSID(void *psid, int flag);
extern int sceRtc_driver_CEEF238F(uint64_t *tick);

typedef struct SceSysEventHandler {
    int size;
    const char *name;
    int type_mask;
    int (*handler)(int ev_id, char *ev_name, void *param, int *result);
    int r28;
    int busy;
    struct SceSysEventHandler *next;
    int reserved[9];
} SceSysEventHandler;

extern int sceKernelRegisterSysEventHandler(SceSysEventHandler *handler);
extern int sceKernelUnregisterSysEventHandler(SceSysEventHandler *handler);

static inline uint32_t psp_get_k1(void)
{
    uint32_t k1;
    __asm__ volatile("move %0, $k1" : "=r"(k1));
    return k1;
}

static inline void psp_set_k1(uint32_t k1)
{
    __asm__ volatile("move $k1, %0" : : "r"(k1) : "memory");
}

/* Forward declarations */
int sceNpDrmGetModuleKey(int fd, void *key_out, void *type_out);
static int __attribute__((noinline)) sub_01c0(void *arg);
int sceNpDrmGetIDps(void *idps);
int sceNpDrmVerifyAct(const uint8_t *act);
int sceNpDrmVerifyRif(const uint8_t *rif);
int sceNpDrmVerifyRifById(const char *content_id);
int sceNpDrmCheckRifTimeLimit(const uint8_t *rif);
int sceNpDrmCheckRifTimeLimitById(const char *content_id);
int sceNpDrmGetContentKey(uint8_t *key_out, const uint8_t *act, const uint8_t *rif);
int sceNpDrmGetVersionKey(uint8_t *key_out, const uint8_t *act, const uint8_t *rif, int type);
int sceNpDrmGetFixedKey(uint8_t *key_out, const char *content_id, int type);
int sceNpDrmSetRifDevice(int dev);
static int __attribute__((noinline)) sub_0650(const char *content_id);
static int __attribute__((noinline)) sub_06fc(uint8_t *key, int type);
static int __attribute__((noinline)) sub_0760(uint8_t *key_out, const uint8_t *edata_hdr, uint32_t hdr_size);
int sceNpDrmDecActivation(uint8_t *act_buf, const uint8_t *key);
static int __attribute__((noinline)) sub_0ae4(uint8_t *key_out, const uint8_t *rif, int type);
static int __attribute__((noinline)) sub_0c14(const uint8_t *src, uint8_t *dst, int count);
static int __attribute__((noinline)) sub_0ce8(uint8_t *key_out, const uint8_t *act, const uint8_t *rif);
static int __attribute__((noinline)) sub_0e4c(uint8_t *key_out, const uint8_t *rif);
static int __attribute__((noinline)) sub_0f18(const uint8_t *rif, uint8_t *ms_id_in);
static int __attribute__((noinline)) sub_1008(uint8_t *out);
static int __attribute__((noinline)) sub_1038(uint8_t *out);
static int __attribute__((noinline)) sub_1068(uint8_t *rif_out, const char *content_id);
static int __attribute__((noinline)) sub_1184(const uint8_t *rif);
static int __attribute__((noinline)) sub_12bc(const uint8_t *rif);
static int sub_1434(int ev_id, char *ev_name, void *param, int *result);
int sub_17d8(void *arg);
static int __attribute__((noinline)) sub_191c(int err);
static int __attribute__((noinline)) sub_1978(const void *sig, const void *pubkey, const void *data, uint32_t size);
static int __attribute__((noinline)) sub_19f8(const uint8_t *in, uint8_t *out, const uint8_t *key);
static int __attribute__((noinline)) sub_1a70(const uint8_t *in, uint8_t *out, const uint8_t *key);
static int __attribute__((noinline)) sub_1cc0(const uint8_t *data, uint32_t size, uint8_t *digest);
static int __attribute__((noinline)) sub_208c(const uint32_t *ctx, const uint8_t *in, uint8_t *out);
static int __attribute__((noinline)) sub_262c(const uint32_t *ctx, const uint8_t *in, uint8_t *out);
static int __attribute__((noinline)) sub_2d1c(uint32_t *ctx, int block_bits, int key_bits, const uint8_t *key);

/* .rodata constants */
static const uint8_t g_keys_3690[3][16] = {
    { 0x07, 0x3d, 0x9e, 0x9d, 0xa8, 0xfd, 0x3b, 0x2f, 0x63, 0x18, 0x93, 0x2e, 0xf8, 0x57, 0xa6, 0x64 },
    { 0x37, 0x49, 0xb7, 0x01, 0xca, 0xe2, 0xe0, 0xc5, 0x44, 0x2e, 0x06, 0xb6, 0x1e, 0xff, 0x84, 0xf2 },
    { 0x9d, 0x31, 0xb8, 0x5a, 0xc8, 0xfa, 0x16, 0x80, 0x73, 0x60, 0x18, 0x82, 0x18, 0x77, 0x91, 0x9d },
};

static const uint8_t g_pubkey_36c0[40] = {
    0x62, 0x27, 0xb0, 0x0a, 0x02, 0x85, 0x6f, 0xb0, 0x41, 0x08, 0x87, 0x67, 0x19, 0xe0, 0xa0, 0x18,
    0x32, 0x91, 0xee, 0xb9, 0x6e, 0x73, 0x6a, 0xbf, 0x81, 0xf7, 0x0e, 0xe9, 0x16, 0x1b, 0x0d, 0xde,
    0xb0, 0x26, 0x76, 0x1a, 0xff, 0x7b, 0xc8, 0x5b,
};

static const uint8_t g_pubkey_36e8[40] = {
    0x1f, 0x07, 0x2b, 0xcc, 0xc1, 0x62, 0xf2, 0xcf, 0xae, 0xa0, 0xe7, 0xf4, 0xcd, 0xfd, 0x9c, 0xae,
    0xc6, 0xc4, 0x55, 0x21, 0x53, 0x01, 0xf4, 0xe3, 0x70, 0xc3, 0xed, 0xe2, 0xd4, 0xf5, 0xdb, 0xc3,
    0xa7, 0xde, 0x8c, 0xaa, 0xe8, 0xad, 0x5b, 0x7d,
};

static const uint8_t g_key_3710[16] = {
    0xda, 0x7d, 0x4b, 0x5e, 0x49, 0x9a, 0x4f, 0x53, 0xb1, 0xc1, 0xa1, 0x4a, 0x74, 0x84, 0x44, 0x3b,
};

static const uint8_t g_key_3720[16] = {
    0x5e, 0x06, 0xe0, 0x4f, 0xd9, 0x4a, 0x71, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

static const uint8_t g_key_3730[16] = {
    0x69, 0xb4, 0x53, 0xf2, 0xe4, 0x21, 0x89, 0x8e, 0x53, 0xe4, 0xa3, 0x5a, 0x5b, 0x91, 0x79, 0x51,
};

static const uint8_t g_key_3740[16] = {
    0xba, 0x87, 0xe4, 0xab, 0x2c, 0x60, 0x5f, 0x59, 0xb8, 0x3b, 0xdb, 0xa6, 0x82, 0xfd, 0xae, 0x14,
};

static const uint8_t g_key_3750[16] = {
    0x38, 0x20, 0xd0, 0x11, 0x07, 0xa3, 0xff, 0x3e, 0x0a, 0x4c, 0x20, 0x85, 0x39, 0x10, 0xb5, 0x54,
};

static const uint8_t g_key_37dc[16] = {
    0xeb, 0x71, 0x5d, 0xb8, 0xd3, 0x73, 0xce, 0xa4, 0x6f, 0xe7, 0x1d, 0xcf, 0xff, 0x63, 0xfa, 0xea,
};

static const uint32_t g_sha1_iv[5] = {
    0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0,
};

static const uint32_t g_aes_te0[256] = {
    0xa56363c6, 0x847c7cf8, 0x997777ee, 0x8d7b7bf6, 0x0df2f2ff, 0xbd6b6bd6, 0xb16f6fde, 0x54c5c591,
    0x50303060, 0x03010102, 0xa96767ce, 0x7d2b2b56, 0x19fefee7, 0x62d7d7b5, 0xe6abab4d, 0x9a7676ec,
    0x45caca8f, 0x9d82821f, 0x40c9c989, 0x877d7dfa, 0x15fafaef, 0xeb5959b2, 0xc947478e, 0x0bf0f0fb,
    0xecadad41, 0x67d4d4b3, 0xfda2a25f, 0xeaafaf45, 0xbf9c9c23, 0xf7a4a453, 0x967272e4, 0x5bc0c09b,
    0xc2b7b775, 0x1cfdfde1, 0xae93933d, 0x6a26264c, 0x5a36366c, 0x413f3f7e, 0x02f7f7f5, 0x4fcccc83,
    0x5c343468, 0xf4a5a551, 0x34e5e5d1, 0x08f1f1f9, 0x937171e2, 0x73d8d8ab, 0x53313162, 0x3f15152a,
    0x0c040408, 0x52c7c795, 0x65232346, 0x5ec3c39d, 0x28181830, 0xa1969637, 0x0f05050a, 0xb59a9a2f,
    0x0907070e, 0x36121224, 0x9b80801b, 0x3de2e2df, 0x26ebebcd, 0x6927274e, 0xcdb2b27f, 0x9f7575ea,
    0x1b090912, 0x9e83831d, 0x742c2c58, 0x2e1a1a34, 0x2d1b1b36, 0xb26e6edc, 0xee5a5ab4, 0xfba0a05b,
    0xf65252a4, 0x4d3b3b76, 0x61d6d6b7, 0xceb3b37d, 0x7b292952, 0x3ee3e3dd, 0x712f2f5e, 0x97848413,
    0xf55353a6, 0x68d1d1b9, 0x00000000, 0x2cededc1, 0x60202040, 0x1ffcfce3, 0xc8b1b179, 0xed5b5bb6,
    0xbe6a6ad4, 0x46cbcb8d, 0xd9bebe67, 0x4b393972, 0xde4a4a94, 0xd44c4c98, 0xe85858b0, 0x4acfcf85,
    0x6bd0d0bb, 0x2aefefc5, 0xe5aaaa4f, 0x16fbfbed, 0xc5434386, 0xd74d4d9a, 0x55333366, 0x94858511,
    0xcf45458a, 0x10f9f9e9, 0x06020204, 0x817f7ffe, 0xf05050a0, 0x443c3c78, 0xba9f9f25, 0xe3a8a84b,
    0xf35151a2, 0xfea3a35d, 0xc0404080, 0x8a8f8f05, 0xad92923f, 0xbc9d9d21, 0x48383870, 0x04f5f5f1,
    0xdfbcbc63, 0xc1b6b677, 0x75dadaaf, 0x63212142, 0x30101020, 0x1affffe5, 0x0ef3f3fd, 0x6dd2d2bf,
    0x4ccdcd81, 0x140c0c18, 0x35131326, 0x2fececc3, 0xe15f5fbe, 0xa2979735, 0xcc444488, 0x3917172e,
    0x57c4c493, 0xf2a7a755, 0x827e7efc, 0x473d3d7a, 0xac6464c8, 0xe75d5dba, 0x2b191932, 0x957373e6,
    0xa06060c0, 0x98818119, 0xd14f4f9e, 0x7fdcdca3, 0x66222244, 0x7e2a2a54, 0xab90903b, 0x8388880b,
    0xca46468c, 0x29eeeec7, 0xd3b8b86b, 0x3c141428, 0x79dedea7, 0xe25e5ebc, 0x1d0b0b16, 0x76dbdbad,
    0x3be0e0db, 0x56323264, 0x4e3a3a74, 0x1e0a0a14, 0xdb494992, 0x0a06060c, 0x6c242448, 0xe45c5cb8,
    0x5dc2c29f, 0x6ed3d3bd, 0xefacac43, 0xa66262c4, 0xa8919139, 0xa4959531, 0x37e4e4d3, 0x8b7979f2,
    0x32e7e7d5, 0x43c8c88b, 0x5937376e, 0xb76d6dda, 0x8c8d8d01, 0x64d5d5b1, 0xd24e4e9c, 0xe0a9a949,
    0xb46c6cd8, 0xfa5656ac, 0x07f4f4f3, 0x25eaeacf, 0xaf6565ca, 0x8e7a7af4, 0xe9aeae47, 0x18080810,
    0xd5baba6f, 0x887878f0, 0x6f25254a, 0x722e2e5c, 0x241c1c38, 0xf1a6a657, 0xc7b4b473, 0x51c6c697,
    0x23e8e8cb, 0x7cdddda1, 0x9c7474e8, 0x211f1f3e, 0xdd4b4b96, 0xdcbdbd61, 0x868b8b0d, 0x858a8a0f,
    0x907070e0, 0x423e3e7c, 0xc4b5b571, 0xaa6666cc, 0xd8484890, 0x05030306, 0x01f6f6f7, 0x120e0e1c,
    0xa36161c2, 0x5f35356a, 0xf95757ae, 0xd0b9b969, 0x91868617, 0x58c1c199, 0x271d1d3a, 0xb99e9e27,
    0x38e1e1d9, 0x13f8f8eb, 0xb398982b, 0x33111122, 0xbb6969d2, 0x70d9d9a9, 0x898e8e07, 0xa7949433,
    0xb69b9b2d, 0x221e1e3c, 0x92878715, 0x20e9e9c9, 0x49cece87, 0xff5555aa, 0x78282850, 0x7adfdfa5,
    0x8f8c8c03, 0xf8a1a159, 0x80898909, 0x170d0d1a, 0xdabfbf65, 0x31e6e6d7, 0xc6424284, 0xb86868d0,
    0xc3414182, 0xb0999929, 0x772d2d5a, 0x110f0f1e, 0xcbb0b07b, 0xfc5454a8, 0xd6bbbb6d, 0x3a16162c,
};

static const uint32_t g_aes_td0[256] = {
    0x50a7f451, 0x5365417e, 0xc3a4171a, 0x965e273a, 0xcb6bab3b, 0xf1459d1f, 0xab58faac, 0x9303e34b,
    0x55fa3020, 0xf66d76ad, 0x9176cc88, 0x254c02f5, 0xfcd7e54f, 0xd7cb2ac5, 0x80443526, 0x8fa362b5,
    0x495ab1de, 0x671bba25, 0x980eea45, 0xe1c0fe5d, 0x02752fc3, 0x12f04c81, 0xa397468d, 0xc6f9d36b,
    0xe75f8f03, 0x959c9215, 0xeb7a6dbf, 0xda595295, 0x2d83bed4, 0xd3217458, 0x2969e049, 0x44c8c98e,
    0x6a89c275, 0x78798ef4, 0x6b3e5899, 0xdd71b927, 0xb64fe1be, 0x17ad88f0, 0x66ac20c9, 0xb43ace7d,
    0x184adf63, 0x82311ae5, 0x60335197, 0x457f5362, 0xe07764b1, 0x84ae6bbb, 0x1ca081fe, 0x942b08f9,
    0x58684870, 0x19fd458f, 0x876cde94, 0xb7f87b52, 0x23d373ab, 0xe2024b72, 0x578f1fe3, 0x2aab5566,
    0x0728ebb2, 0x03c2b52f, 0x9a7bc586, 0xa50837d3, 0xf2872830, 0xb2a5bf23, 0xba6a0302, 0x5c8216ed,
    0x2b1ccf8a, 0x92b479a7, 0xf0f207f3, 0xa1e2694e, 0xcdf4da65, 0xd5be0506, 0x1f6234d1, 0x8afea6c4,
    0x9d532e34, 0xa055f3a2, 0x32e18a05, 0x75ebf6a4, 0x39ec830b, 0xaaef6040, 0x069f715e, 0x51106ebd,
    0xf98a213e, 0x3d06dd96, 0xae053edd, 0x46bde64d, 0xb58d5491, 0x055dc471, 0x6fd40604, 0xff155060,
    0x24fb9819, 0x97e9bdd6, 0xcc434089, 0x779ed967, 0xbd42e8b0, 0x888b8907, 0x385b19e7, 0xdbeec879,
    0x470a7ca1, 0xe90f427c, 0xc91e84f8, 0x00000000, 0x83868009, 0x48ed2b32, 0xac70111e, 0x4e725a6c,
    0xfbff0efd, 0x5638850f, 0x1ed5ae3d, 0x27392d36, 0x64d90f0a, 0x21a65c68, 0xd1545b9b, 0x3a2e3624,
    0xb1670a0c, 0x0fe75793, 0xd296eeb4, 0x9e919b1b, 0x4fc5c080, 0xa220dc61, 0x694b775a, 0x161a121c,
    0x0aba93e2, 0xe52aa0c0, 0x43e0223c, 0x1d171b12, 0x0b0d090e, 0xadc78bf2, 0xb9a8b62d, 0xc8a91e14,
    0x8519f157, 0x4c0775af, 0xbbdd99ee, 0xfd607fa3, 0x9f2601f7, 0xbcf5725c, 0xc53b6644, 0x347efb5b,
    0x7629438b, 0xdcc623cb, 0x68fcedb6, 0x63f1e4b8, 0xcadc31d7, 0x10856342, 0x40229713, 0x2011c684,
    0x7d244a85, 0xf83dbbd2, 0x1132f9ae, 0x6da129c7, 0x4b2f9e1d, 0xf330b2dc, 0xec52860d, 0xd0e3c177,
    0x6c16b32b, 0x99b970a9, 0xfa489411, 0x2264e947, 0xc48cfca8, 0x1a3ff0a0, 0xd82c7d56, 0xef903322,
    0xc74e4987, 0xc1d138d9, 0xfea2ca8c, 0x360bd498, 0xcf81f5a6, 0x28de7aa5, 0x268eb7da, 0xa4bfad3f,
    0xe49d3a2c, 0x0d927850, 0x9bcc5f6a, 0x62467e54, 0xc2138df6, 0xe8b8d890, 0x5ef7392e, 0xf5afc382,
    0xbe805d9f, 0x7c93d069, 0xa92dd56f, 0xb31225cf, 0x3b99acc8, 0xa77d1810, 0x6e639ce8, 0x7bbb3bdb,
    0x097826cd, 0xf418596e, 0x01b79aec, 0xa89a4f83, 0x656e95e6, 0x7ee6ffaa, 0x08cfbc21, 0xe6e815ef,
    0xd99be7ba, 0xce366f4a, 0xd4099fea, 0xd67cb029, 0xafb2a431, 0x31233f2a, 0x3094a5c6, 0xc066a235,
    0x37bc4e74, 0xa6ca82fc, 0xb0d090e0, 0x15d8a733, 0x4a9804f1, 0xf7daec41, 0x0e50cd7f, 0x2ff69117,
    0x8dd64d76, 0x4db0ef43, 0x544daacc, 0xdf0496e4, 0xe3b5d19e, 0x1b886a4c, 0xb81f2cc1, 0x7f516546,
    0x04ea5e9d, 0x5d358c01, 0x737487fa, 0x2e410bfb, 0x5a1d67b3, 0x52d2db92, 0x335610e9, 0x1347d66d,
    0x8c61d79a, 0x7a0ca137, 0x8e14f859, 0x893c13eb, 0xee27a9ce, 0x35c961b7, 0xede51ce1, 0x3cb1477a,
    0x59dfd29c, 0x3f73f255, 0x79ce1418, 0xbf37c773, 0xeacdf753, 0x5baafd5f, 0x146f3ddf, 0x86db4478,
    0x81f3afca, 0x3ec468b9, 0x2c342438, 0x5f40a3c2, 0x72c31d16, 0x0c25e2bc, 0x8b493c28, 0x41950dff,
    0x7101a839, 0xdeb30c08, 0x9ce4b4d8, 0x90c15664, 0x6184cb7b, 0x70b632d5, 0x745c6c48, 0x4257b8d0,
};

static const uint8_t g_aes_isbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d,
};

static const uint8_t g_aes_gf_log[256] = {
    0x00, 0xff, 0x19, 0x01, 0x32, 0x02, 0x1a, 0xc6, 0x4b, 0xc7, 0x1b, 0x68, 0x33, 0xee, 0xdf, 0x03,
    0x64, 0x04, 0xe0, 0x0e, 0x34, 0x8d, 0x81, 0xef, 0x4c, 0x71, 0x08, 0xc8, 0xf8, 0x69, 0x1c, 0xc1,
    0x7d, 0xc2, 0x1d, 0xb5, 0xf9, 0xb9, 0x27, 0x6a, 0x4d, 0xe4, 0xa6, 0x72, 0x9a, 0xc9, 0x09, 0x78,
    0x65, 0x2f, 0x8a, 0x05, 0x21, 0x0f, 0xe1, 0x24, 0x12, 0xf0, 0x82, 0x45, 0x35, 0x93, 0xda, 0x8e,
    0x96, 0x8f, 0xdb, 0xbd, 0x36, 0xd0, 0xce, 0x94, 0x13, 0x5c, 0xd2, 0xf1, 0x40, 0x46, 0x83, 0x38,
    0x66, 0xdd, 0xfd, 0x30, 0xbf, 0x06, 0x8b, 0x62, 0xb3, 0x25, 0xe2, 0x98, 0x22, 0x88, 0x91, 0x10,
    0x7e, 0x6e, 0x48, 0xc3, 0xa3, 0xb6, 0x1e, 0x42, 0x3a, 0x6b, 0x28, 0x54, 0xfa, 0x85, 0x3d, 0xba,
    0x2b, 0x79, 0x0a, 0x15, 0x9b, 0x9f, 0x5e, 0xca, 0x4e, 0xd4, 0xac, 0xe5, 0xf3, 0x73, 0xa7, 0x57,
    0xaf, 0x58, 0xa8, 0x50, 0xf4, 0xea, 0xd6, 0x74, 0x4f, 0xae, 0xe9, 0xd5, 0xe7, 0xe6, 0xad, 0xe8,
    0x2c, 0xd7, 0x75, 0x7a, 0xeb, 0x16, 0x0b, 0xf5, 0x59, 0xcb, 0x5f, 0xb0, 0x9c, 0xa9, 0x51, 0xa0,
    0x7f, 0x0c, 0xf6, 0x6f, 0x17, 0xc4, 0x49, 0xec, 0xd8, 0x43, 0x1f, 0x2d, 0xa4, 0x76, 0x7b, 0xb7,
    0xcc, 0xbb, 0x3e, 0x5a, 0xfb, 0x60, 0xb1, 0x86, 0x3b, 0x52, 0xa1, 0x6c, 0xaa, 0x55, 0x29, 0x9d,
    0x97, 0xb2, 0x87, 0x90, 0x61, 0xbe, 0xdc, 0xfc, 0xbc, 0x95, 0xcf, 0xcd, 0x37, 0x3f, 0x5b, 0xd1,
    0x53, 0x39, 0x84, 0x3c, 0x41, 0xa2, 0x6d, 0x47, 0x14, 0x2a, 0x9e, 0x5d, 0x56, 0xf2, 0xd3, 0xab,
    0x44, 0x11, 0x92, 0xd9, 0x23, 0x20, 0x2e, 0x89, 0xb4, 0x7c, 0xb8, 0x26, 0x77, 0x99, 0xe3, 0xa5,
    0x67, 0x4a, 0xed, 0xde, 0xc5, 0x31, 0xfe, 0x18, 0x0d, 0x63, 0x8c, 0x80, 0xc0, 0xf7, 0x70, 0x07,
};

static const uint8_t g_aes_gf_exp[256] = {
    0x01, 0x03, 0x05, 0x0f, 0x11, 0x33, 0x55, 0xff, 0x1a, 0x2e, 0x72, 0x96, 0xa1, 0xf8, 0x13, 0x35,
    0x5f, 0xe1, 0x38, 0x48, 0xd8, 0x73, 0x95, 0xa4, 0xf7, 0x02, 0x06, 0x0a, 0x1e, 0x22, 0x66, 0xaa,
    0xe5, 0x34, 0x5c, 0xe4, 0x37, 0x59, 0xeb, 0x26, 0x6a, 0xbe, 0xd9, 0x70, 0x90, 0xab, 0xe6, 0x31,
    0x53, 0xf5, 0x04, 0x0c, 0x14, 0x3c, 0x44, 0xcc, 0x4f, 0xd1, 0x68, 0xb8, 0xd3, 0x6e, 0xb2, 0xcd,
    0x4c, 0xd4, 0x67, 0xa9, 0xe0, 0x3b, 0x4d, 0xd7, 0x62, 0xa6, 0xf1, 0x08, 0x18, 0x28, 0x78, 0x88,
    0x83, 0x9e, 0xb9, 0xd0, 0x6b, 0xbd, 0xdc, 0x7f, 0x81, 0x98, 0xb3, 0xce, 0x49, 0xdb, 0x76, 0x9a,
    0xb5, 0xc4, 0x57, 0xf9, 0x10, 0x30, 0x50, 0xf0, 0x0b, 0x1d, 0x27, 0x69, 0xbb, 0xd6, 0x61, 0xa3,
    0xfe, 0x19, 0x2b, 0x7d, 0x87, 0x92, 0xad, 0xec, 0x2f, 0x71, 0x93, 0xae, 0xe9, 0x20, 0x60, 0xa0,
    0xfb, 0x16, 0x3a, 0x4e, 0xd2, 0x6d, 0xb7, 0xc2, 0x5d, 0xe7, 0x32, 0x56, 0xfa, 0x15, 0x3f, 0x41,
    0xc3, 0x5e, 0xe2, 0x3d, 0x47, 0xc9, 0x40, 0xc0, 0x5b, 0xed, 0x2c, 0x74, 0x9c, 0xbf, 0xda, 0x75,
    0x9f, 0xba, 0xd5, 0x64, 0xac, 0xef, 0x2a, 0x7e, 0x82, 0x9d, 0xbc, 0xdf, 0x7a, 0x8e, 0x89, 0x80,
    0x9b, 0xb6, 0xc1, 0x58, 0xe8, 0x23, 0x65, 0xaf, 0xea, 0x25, 0x6f, 0xb1, 0xc8, 0x43, 0xc5, 0x54,
    0xfc, 0x1f, 0x21, 0x63, 0xa5, 0xf4, 0x07, 0x09, 0x1b, 0x2d, 0x77, 0x99, 0xb0, 0xcb, 0x46, 0xca,
    0x45, 0xcf, 0x4a, 0xde, 0x79, 0x8b, 0x86, 0x91, 0xa8, 0xe3, 0x3e, 0x42, 0xc6, 0x51, 0xf3, 0x0e,
    0x12, 0x36, 0x5a, 0xee, 0x29, 0x7b, 0x8d, 0x8c, 0x8f, 0x8a, 0x85, 0x94, 0xa7, 0xf2, 0x0d, 0x17,
    0x39, 0x4b, 0xdd, 0x7c, 0x84, 0x97, 0xa2, 0xfd, 0x1c, 0x24, 0x6c, 0xb4, 0xc7, 0x52, 0xf6, 0x01,
};

static const uint8_t g_aes_rcon[32] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f,
    0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x00, 0x00,
};

/* .bss globals */
static int g_rif_device;               /* 0x4360 */
static int g_npdrm_mutex;              /* 0x4364 */
static uint8_t g_licensee_key[16];     /* 0x4368 */
static int g_debug_mode;               /* 0x4378 */
static volatile int g_suspend_flag;    /* 0x437c */
static uint8_t g_act_buf[0x1038];      /* 0x4380 */
static SceSysEventHandler g_sysevent_handler; /* 0x53b8 */

/* 0x0000 */
int module_start(int args, void *argp)
{
    (void)args;
    (void)argp;

    int uid = sceKernelCreateMutex("SceNpDrmMutex", 0x200, 0, NULL);
    if (uid < 0) {
        return uid;
    }

    g_npdrm_mutex = uid;
    memset(&g_sysevent_handler, 0, 0x40);
    g_sysevent_handler.size = 0x40;
    g_sysevent_handler.name = "SceNpDrm";
    g_sysevent_handler.type_mask = 0x00ffff00;
    g_sysevent_handler.handler = sub_1434;

    int res = sceKernelRegisterSysEventHandler(&g_sysevent_handler);
    if (res != 0) {
        memset(&g_sysevent_handler, 0, 0x40);
        return res;
    }

    res = sceKernelSetNpDrmGetModuleKeyFunction(sceNpDrmGetModuleKey);
    if (res < 0) {
        sceKernelUnregisterSysEventHandler(&g_sysevent_handler);
        memset(&g_sysevent_handler, 0, 0x40);
        return res;
    }

    return 0;
}

/* 0x00f8 */
int module_stop(int args, void *argp)
{
    (void)args;
    (void)argp;

    if (g_sysevent_handler.handler != NULL) {
        sceKernelUnregisterSysEventHandler(&g_sysevent_handler);
        memset(&g_sysevent_handler, 0, 0x40);
    }

    sceKernelSetNpDrmGetModuleKeyFunction(NULL);

    if (g_npdrm_mutex > 0) {
        sceKernelDeleteMutex(g_npdrm_mutex);
        g_npdrm_mutex = 0;
    }
    return 0;
}

/* 0x0170 */
int sceNpDrmSetDebugMode(int mode)
{
    g_debug_mode = mode;
    return 0;
}

typedef struct ModuleKeyArgs {
    int fd;
    void *key_out;
    uint32_t *type_out;
} ModuleKeyArgs;

/* 0x0180 */
int sceNpDrmGetModuleKey(int fd, void *key_out, void *type_out)
{
    ModuleKeyArgs args;
    args.fd = fd;
    args.key_out = key_out;
    args.type_out = (uint32_t *)type_out;
    return sceKernelExtendKernelStack(0x1000, sub_01c0, &args);
}

/* 0x01c0 */
static int __attribute__((noinline)) sub_01c0(void *arg)
{
    ModuleKeyArgs *args = (ModuleKeyArgs *)arg;
    uint8_t hdr[0x90];

    int fd = args->fd;
    void *key_out = args->key_out;
    uint32_t *type_out = args->type_out;

    int res = sceIoValidateFd(fd, 4);
    if (res != 0) {
        return res;
    }

    int pos = (int)sceIoLseek(fd, 0, 0);
    if (pos != 0) {
        return (pos < 0) ? pos : (int)0x80550907;
    }

    int nread = sceIoRead(fd, hdr, 0x90);
    if (nread != 0x90) {
        return (nread < 0) ? nread : (int)0x80550907;
    }

    if (hdr[0x0e] != 1) {
        return 0x80550913;
    }

    res = sub_0760((uint8_t *)key_out, hdr, 0x90);
    if (res < 0) {
        return res;
    }

    uint16_t flags = *(uint16_t *)(hdr + 0x0c);
    type_out[0] = flags;
    type_out[1] = 0;
    return res;
}

/* 0x02b4 */
int sceNpDrmGetIDps(void *idps)
{
    if (idps == NULL) {
        return 0x80550901;
    }
    return sceOpenPSIDGetPSID(idps, 1);
}

/* 0x02e8 */
int sceNpDrmVerifyAct(const uint8_t *act)
{
    if (act == NULL) {
        return 0x80550901;
    }

    uint16_t v2 = *(const uint16_t *)(act + 2);
    v2 = (uint16_t)((v2 >> 8) | (v2 << 8));
    if (v2 >= 2) {
        return 0x80550906;
    }

    uint16_t v0 = *(const uint16_t *)(act + 0);
    v0 = (uint16_t)((v0 >> 8) | (v0 << 8));
    if (v0 >= 2) {
        return 0x80550906;
    }

    int res = sub_1978(act + 0x1010, g_pubkey_36c0, act, 0x1010);
    return (res < 0) ? (int)0x80550903 : 0;
}

/* 0x0380 */
int sceNpDrmVerifyRif(const uint8_t *rif)
{
    int res = sub_1184(rif);
    if (res >= 0) {
        if (*(const uint32_t *)(rif + 4) == 0x03000000) {
            res = sub_0f18(rif, NULL);
        }
    }
    return res;
}

/* 0x03d0 */
int sceNpDrmVerifyRifById(const char *content_id)
{
    uint8_t rif[0x98];
    int res = sub_1068(rif, content_id);
    if (res == 0) {
        res = sceNpDrmVerifyRif(rif);
    }
    return res;
}

/* 0x0400 */
int sceNpDrmCheckRifTimeLimit(const uint8_t *rif)
{
    int res = sceNpDrmVerifyRif(rif);
    if (res >= 0) {
        res = sub_12bc(rif);
    }
    return res;
}

/* 0x0440 */
int sceNpDrmCheckRifTimeLimitById(const char *content_id)
{
    uint8_t rif[0x98];
    int res = sub_1068(rif, content_id);
    if (res == 0) {
        res = sceNpDrmCheckRifTimeLimit(rif);
    }
    return res;
}

/* 0x0470 */
int sceNpDrmGetContentKey(uint8_t *key_out, const uint8_t *act, const uint8_t *rif)
{
    return sceNpDrmGetVersionKey(key_out, act, rif, 0);
}

/* 0x048c */
int sceNpDrmGetVersionKey(uint8_t *key_out, const uint8_t *act, const uint8_t *rif, int type)
{
    if (key_out == NULL || rif == NULL) {
        return 0x80550901;
    }

    uint32_t ver = *(const uint32_t *)(rif + 4);
    int res;
    if (ver == 0x03000000) {
        res = sub_0e4c(key_out, rif);
    } else {
        if (act == NULL) {
            return 0x80550901;
        }
        res = sub_0ce8(key_out, act, rif);
    }

    if (res >= 0) {
        res = sub_12bc(rif);
        if (res >= 0) {
            res = sub_06fc(key_out, type);
        }
    }
    return res;
}

/* 0x0544 */
int sceNpDrmGetFixedKey(uint8_t *key_out, const char *content_id, int type)
{
    uint8_t buf[0x30];
    uint8_t bbmac_ctx[0x20];

    memset(buf, 0, 0x30);
    if ((type & 0x01000000) == 0) {
        return 0x80550901;
    }

    strncpy((char *)buf, content_id, 0x30);
    type &= ~0x01000000;

    int res = sceDrmBBMacInit(bbmac_ctx, 1);
    if (res != 0) {
        return res;
    }

    res = sceDrmBBMacUpdate(bbmac_ctx, buf, 0x30);
    if (res != 0) {
        return res;
    }

    res = sceDrmBBMacFinal(bbmac_ctx, key_out, g_key_3750);
    if (res != 0) {
        return 0x80550902;
    }

    return sub_06fc(key_out, type);
}

/* 0x062c */
int sceNpDrmSetRifDevice(int dev)
{
    if ((unsigned int)dev >= 3) {
        return 0x80550901;
    }
    g_rif_device = dev;
    return 0;
}

/* 0x0650 */
static int __attribute__((noinline)) sub_0650(const char *content_id)
{
    char path[0x40];

    if (strlen(content_id) >= 0x2a) {
        return 0x80550913;
    }

    snprintf(path, 0x40, "ms0:/PSP/LICENSE/%s.rif", content_id);
    path[0x3f] = '\0';

    int flags = 1;
    int dev = g_rif_device;
    if (dev == 1) {
        flags = 0x04000001;
    } else if (dev == 2) {
        sceKernelMemcpy(path, "ef0:", 4);
    }

    return sceIoOpen(path, flags, 0);
}

/* 0x06fc */
static int __attribute__((noinline)) sub_06fc(uint8_t *key, int type)
{
    uint32_t idx = (uint32_t)type & 0x00ffffffU;
    if (idx == 0) {
        return 0;
    }
    if (idx >= 4) {
        return 0x80550901;
    }

    int res = sub_19f8(key, key, g_keys_3690[idx - 1]);
    return (res < 0) ? (int)0x80550902 : 0;
}

/* 0x0760 */
static int __attribute__((noinline)) sub_0760(uint8_t *key_out, const uint8_t *edata_hdr, uint32_t hdr_size)
{
    uint8_t rif_buf[0x98];
    uint8_t bbmac_ctx[0x20];

    if (hdr_size < 0x80) {
        return 0x80550901;
    }

    int res = sub_1978(edata_hdr + 0x58, g_pubkey_36e8, edata_hdr, 0x58);
    if (res < 0) {
        return 0x80550913;
    }

    if (*(const uint32_t *)(edata_hdr + 0) != 0x50535000 ||
        *(const uint32_t *)(edata_hdr + 4) != 0x54414445) {
        return 0x80550913;
    }

    const char *content_id = (const char *)(edata_hdr + 0x10);
    if (strlen(content_id) >= 0x2a) {
        return 0x80550913;
    }

    int32_t drm_type = *(const int32_t *)(edata_hdr + 8);
    uint32_t is_fixed = ((uint32_t)drm_type >> 24) & 1;
    if (g_debug_mode != 0 && is_fixed == 0) {
        return 0x80550910;
    }
    if (drm_type < 0) {
        return 0x80550912;
    }

    if ((edata_hdr[0x0f] & 1) != 0) {
        res = sceDrmBBMacInit(bbmac_ctx, 3);
        if (res != 0) {
            return res;
        }
        res = sceDrmBBMacUpdate(bbmac_ctx, edata_hdr, 0x80);
        if (res != 0) {
            return res;
        }
        res = sceDrmBBMacFinal2(bbmac_ctx, edata_hdr + 0x80, g_licensee_key);
        if (res != 0) {
            return 0x80550916;
        }
    }

    if (is_fixed != 0) {
        res = sceNpDrmGetFixedKey(key_out, content_id, *(const int32_t *)(edata_hdr + 8));
        if (res != 0) {
            return res;
        }
    } else {
        int fd = sub_0650(content_id);
        if (fd < 0) {
            return (fd == (int)0x80010002) ? (int)0x80550910 : fd;
        }
        int nread = sceIoRead(fd, rif_buf, 0x98);
        sceIoClose(fd);
        if (nread != 0x98) {
            return (nread < 0) ? nread : (int)0x80550904;
        }
        res = sub_0ae4(key_out, rif_buf, *(const int32_t *)(edata_hdr + 8));
        if (res < 0) {
            return res;
        }
    }

    if ((edata_hdr[0x0f] & 1) != 0) {
        for (int i = 0; i < 16; i++) {
            key_out[i] ^= g_licensee_key[i];
        }
    }

    if ((edata_hdr[0x0f] & 2) != 0) {
        for (int i = 0; i < 16; i++) {
            key_out[i] ^= edata_hdr[0x40 + i];
        }
    }

    res = sub_1a70(key_out, key_out, g_key_3740);
    return (res < 0) ? res : 0;
}

/* 0x0a00 */
int sceNpDrmDecActivation(uint8_t *act_buf, const uint8_t *key)
{
    uint32_t ctx[0x3cc / 4];

    if (act_buf == NULL || key == NULL) {
        return 0x80550901;
    }

    int res = sub_2d1c(ctx, 0x80, 0x80, key);
    if (res >= 0) {
        for (int off = 0x1030; off > 0; off -= 0x10) {
            uint8_t *blk = act_buf + off;
            res = sub_262c(ctx, blk, blk);
            if (res < 0) {
                goto out;
            }
            for (int i = 0; i < 16; i++) {
                blk[i] ^= blk[i - 0x10];
            }
        }
        res = sub_262c(ctx, act_buf, act_buf);
    }

out:
    sceKernelMemset(ctx, 0, 0x3cc);
    return res;
}

/* 0x0ae4 */
static int __attribute__((noinline)) sub_0ae4(uint8_t *key_out, const uint8_t *rif, int type)
{
    if (g_npdrm_mutex == 0) {
        return 0x80000041;
    }

    int res = sceKernelLockMutex(g_npdrm_mutex, 1, NULL);
    if (res < 0) {
        return res;
    }

    uint8_t *act = g_act_buf;
    if (*(const uint32_t *)(rif + 4) == 0x03000000) {
        act = NULL;
    } else {
        int fd = sceIoOpen("flash2:/act.dat", 0x04000001, 0);
        if (fd < 0) {
            res = 0x80550911;
            goto unlock;
        }
        int nread = sceIoRead(fd, g_act_buf, 0x1038);
        sceIoClose(fd);
        if (nread < 0x1038) {
            res = 0x80550903;
            goto unlock;
        }
    }

    res = sceNpDrmGetVersionKey(key_out, act, rif, type);

unlock:
    sceKernelUnlockMutex(g_npdrm_mutex, 1);
    return res;
}

/* 0x0c14 */
static int __attribute__((noinline)) sub_0c14(const uint8_t *src, uint8_t *dst, int count)
{
    uint8_t idps[16];
    uint8_t master_key[16];

    int res = sceNpDrmGetIDps(idps);
    if (res >= 0) {
        res = sub_19f8(g_key_3720, master_key, idps);
        if (res >= 0) {
            int total = count << 4;
            for (int off = 0; off < total; off += 0x10) {
                res = sub_1a70(src + off, dst + off, master_key);
                if (res < 0) {
                    break;
                }
            }
        }
    }

    sceKernelMemset(idps, 0, 0x20);
    return (res < 0) ? (int)0x80550902 : 0;
}

/* 0x0ce8 */
static int __attribute__((noinline)) sub_0ce8(uint8_t *key_out, const uint8_t *act, const uint8_t *rif)
{
    uint8_t act_key[16];
    uint8_t dec_idx_buf[16];

    int res = sceNpDrmVerifyAct(act);
    if (res < 0) {
        goto cleanup;
    }

    res = sceNpDrmVerifyRif(rif);
    if (res < 0) {
        goto cleanup;
    }

    if (*(const uint32_t *)(rif + 8) != *(const uint32_t *)(act + 8) ||
        *(const uint32_t *)(rif + 12) != *(const uint32_t *)(act + 12)) {
        return 0x80550905;
    }

    uint16_t act_v = *(const uint16_t *)act;
    uint16_t rif_v = *(const uint16_t *)rif;
    act_v = (uint16_t)((act_v >> 8) | (act_v << 8));
    rif_v = (uint16_t)((rif_v >> 8) | (rif_v << 8));
    if (act_v < rif_v) {
        return 0x80550911;
    }

    res = sub_1a70(rif + 0x40, dec_idx_buf, g_key_3710);
    if (res < 0) {
        goto cleanup;
    }

    uint32_t idx_word = *(uint32_t *)(dec_idx_buf + 12);
    if ((idx_word & 0x00ffffffU) != 0) {
        return 0x80550902;
    }
    uint32_t idx = idx_word >> 24;
    if (idx >= 0x80) {
        return 0x80550902;
    }
    *(uint32_t *)(dec_idx_buf + 12) = idx;

    res = sub_0c14(act + idx * 16 + 0x10, act_key, 1);
    if (res < 0) {
        goto cleanup;
    }

    res = sub_1a70(rif + 0x50, key_out, act_key);
    if (res < 0) {
        res = 0x80550902;
    }

cleanup:
    sceKernelMemset(act_key, 0, 0x20);
    return res;
}

/* 0x0e4c */
static int __attribute__((noinline)) sub_0e4c(uint8_t *key_out, const uint8_t *rif)
{
    uint8_t ms_key[16];

    int res = sub_1184(rif);
    if (res < 0) {
        return res;
    }

    res = sub_1008(ms_key);
    if (res < 0) {
        goto cleanup;
    }

    res = sub_0f18(rif, ms_key);
    if (res < 0) {
        goto cleanup;
    }

    res = sub_19f8(ms_key, ms_key, g_key_3730);
    if (res < 0) {
        res = 0x80550902;
        goto cleanup;
    }

    res = sub_1a70(rif + 0x50, key_out, ms_key);
    if (res < 0) {
        res = 0x80550902;
    }

cleanup:
    sceKernelMemset(ms_key, 0, 0x10);
    return res;
}

/* 0x0f18 */
static int __attribute__((noinline)) sub_0f18(const uint8_t *rif, uint8_t *ms_id_in)
{
    uint8_t local_ms_id[16];
    uint8_t dec_buf[16];
    uint8_t hash[20];

    int res;
    uint8_t *ms_id = ms_id_in;
    if (ms_id == NULL) {
        res = sub_1008(local_ms_id);
        if (res < 0) {
            goto cleanup;
        }
        ms_id = local_ms_id;
    }

    res = sceDdrdbHash(ms_id, 0x10, hash);
    if (res < 0) {
        goto cleanup;
    }

    if (memcmp(rif + 8, hash, 8) != 0) {
        res = 0x80550908;
        goto cleanup;
    }

    res = sub_1a70(rif + 0x40, dec_buf, g_key_3710);
    if (res < 0) {
        goto cleanup;
    }

    if (memcmp(dec_buf, hash + 8, 12) != 0) {
        res = 0x80550908;
        goto cleanup;
    }

    if (*(uint32_t *)(dec_buf + 12) != 0) {
        res = 0x80550913;
    }

cleanup:
    sceKernelMemset(local_ms_id, 0, 0x34);
    return res;
}

/* 0x1008 */
static int __attribute__((noinline)) sub_1008(uint8_t *out)
{
    if (out == NULL) {
        return 0x80550901;
    }
    return sceMgMemoryStick_driver_735526D6(out);
}

/* 0x1038 */
static int __attribute__((noinline)) sub_1038(uint8_t *out)
{
    uint32_t v0 = *(volatile uint32_t *)0xbc100094;
    uint32_t v1 = *(volatile uint32_t *)0xbc100090;
    *(uint32_t *)(out + 0) = __builtin_bswap32(v0);
    *(uint32_t *)(out + 4) = __builtin_bswap32(v1);
    return 0;
}

/* 0x1068 */
static int __attribute__((noinline)) sub_1068(uint8_t *rif_out, const char *content_id)
{
    if (rif_out == NULL || content_id == NULL || content_id[0] == '\0') {
        return 0x80550901;
    }
    if (strlen(content_id) >= 0x2a) {
        return 0x80550901;
    }

    int fd = sub_0650(content_id);
    if (fd < 0) {
        if (fd == (int)0x80020328) {
            return fd;
        }
        return (fd == (int)0x80010002) ? (int)0x80550910 : (int)0x80550907;
    }

    int nread = sceIoRead(fd, rif_out, 0x98);
    sceIoClose(fd);
    if (nread < 0x98) {
        return (nread == (int)0x80020328) ? nread : (int)0x80550904;
    }

    if (strcmp(content_id, (const char *)(rif_out + 0x10)) != 0) {
        return 0x80550909;
    }
    return 0;
}

/* 0x1184 */
static int __attribute__((noinline)) sub_1184(const uint8_t *rif)
{
    uint8_t buf[0x90];

    if (rif == NULL) {
        return 0x80550901;
    }

    uint16_t v2 = *(const uint16_t *)(rif + 2);
    v2 = (uint16_t)((v2 >> 8) | (v2 << 8));
    if (v2 >= 3) {
        return 0x80550906;
    }

    uint16_t v0 = *(const uint16_t *)(rif + 0);
    v0 = (uint16_t)((v0 >> 8) | (v0 << 8));
    if (v0 >= 2) {
        return 0x80550906;
    }

    int res;
    int err_code;
    if ((*(const uint32_t *)(rif + 4) & 0x00010000) != 0) {
        sceKernelMemset(buf, 0, 0x90);
        sceKernelMemcpy(buf, rif, 0x70);
        res = sceOpenPSIDGetPSID(buf + 0x70, 1);
        if (res < 0) {
            return res;
        }
        res = sub_1038(buf + 0x80);
        if (res < 0) {
            return res;
        }
        res = sub_1978(rif + 0x70, g_pubkey_36c0, buf, 0x90);
        sceKernelMemset(buf, 0, 0x90);
        err_code = 0x80550918;
    } else {
        res = sub_1978(rif + 0x70, g_pubkey_36c0, rif, 0x70);
        err_code = 0x80550904;
    }

    return (res >= 0) ? 0 : err_code;
}

/* 0x12bc */
static int __attribute__((noinline)) sub_12bc(const uint8_t *rif)
{
    uint64_t cur_tick;

    while (g_suspend_flag != 0) {
        sceKernelDelayThread(100000);
    }

    uint32_t end_hi = *(const uint32_t *)(rif + 0x68);
    uint32_t end_lo = *(const uint32_t *)(rif + 0x6c);
    if ((end_hi | end_lo) == 0) {
        return 0;
    }

    if (sceRtc_driver_CEEF238F(&cur_tick) < 0) {
        return 0x80000001;
    }

    uint64_t end_ms = ((uint64_t)__builtin_bswap32(end_hi) << 32) | __builtin_bswap32(end_lo);
    uint64_t end_tick = end_ms * 1000ULL + 0x00dcbffeff2bc000ULL;

    uint32_t start_hi = *(const uint32_t *)(rif + 0x60);
    uint32_t start_lo = *(const uint32_t *)(rif + 0x64);
    uint64_t start_ms = ((uint64_t)__builtin_bswap32(start_hi) << 32) | __builtin_bswap32(start_lo);
    uint64_t start_tick = start_ms * 1000ULL + 0x00dcbffeff2bc000ULL;

    if (end_tick < cur_tick) {
        return 0x80550914;
    }
    if (cur_tick < start_tick) {
        return 0x80550915;
    }
    return 0;
}

/* 0x1434 */
static int sub_1434(int ev_id, char *ev_name, void *param, int *result)
{
    (void)ev_name;
    (void)param;
    (void)result;

    if (ev_id == 0x402) {
        g_suspend_flag = 1;
        return 0;
    }
    if (ev_id == 0x400000) {
        g_suspend_flag = 0;
        return 0;
    }
    return 0;
}

/* 0x1470 */
int sceNpDrmSetLicenseeKey(const void *key)
{
    uint32_t k1 = psp_get_k1();
    if (key == NULL) {
        return 0x80550901;
    }

    uint32_t chk = ((uintptr_t)key + 0x10) | (uintptr_t)key;
    uint32_t k1_sll = k1 << 11;
    psp_set_k1(k1_sll);
    if ((int32_t)(chk & k1_sll) < 0) {
        psp_set_k1(k1);
        return 0x80550901;
    }

    sceKernelMemcpy(g_licensee_key, key, 16);
    psp_set_k1(k1);
    return 0;
}

/* 0x14e8 */
int sceNpDrmClearLicenseeKey(void)
{
    sceKernelMemset(g_licensee_key, 0, 16);
    return 0;
}

/* 0x1514 */
int sceNpDrmEdataGetDataSize(int fd)
{
    int res = sceIoValidateFd(fd, 4);
    if (res != 0) {
        return 0x80550901;
    }

    res = sceIoIoctl(fd, 0x04100010, NULL, 0, NULL, 0);
    if (res < 0) {
        return (res == (int)0x80020328) ? res : (int)0x80550901;
    }
    return res;
}

/* 0x1590 */
int sceNpDrmRenameCheck(const char *path)
{
    uint8_t hdr[0x80];
    uint8_t bbmac_ctx[0x20];

    if (path == NULL) {
        return 0x80550901;
    }

    const char *base = path;
    const char *p = path;
    if (*p != '\0') {
        do {
            char c = *p++;
            if (c == '/') {
                base = p;
            }
        } while (*p != '\0');
    }

    uint32_t k1 = psp_get_k1();
    uint32_t k1_sll = k1 << 11;
    psp_set_k1(k1_sll);
    if ((int32_t)((uintptr_t)path & k1_sll) < 0) {
        psp_set_k1(k1);
        return 0x80550901;
    }

    psp_set_k1(k1_sll >> 11);
    int fd = sceIoOpen(path, 1, 0);
    psp_set_k1(k1_sll);

    int nread = fd;
    if (fd >= 0) {
        nread = sceIoRead(fd, hdr, 0x80);
        sceIoClose(fd);
    }

    if (nread != 0x80 ||
        *(const uint32_t *)(hdr + 0) != 0x50535000 ||
        *(const uint32_t *)(hdr + 4) != 0x54414445) {
        psp_set_k1(k1);
        return 0x80550901;
    }

    int res = sceDrmBBMacInit(bbmac_ctx, 3);
    if (res >= 0) {
        res = sceDrmBBMacUpdate(bbmac_ctx, hdr + 0x10, 0x30);
        if (res >= 0) {
            size_t len = strlen(base);
            res = sceDrmBBMacUpdate(bbmac_ctx, base, (int)len);
            if (res >= 0) {
                res = sceDrmBBMacFinal2(bbmac_ctx, hdr + 0x40, g_key_37dc);
                if (res < 0) {
                    res = 0x80550917;
                }
            }
        }
    }

    psp_set_k1(k1);
    return res;
}

/* 0x1714 */
int __attribute__((optimize("no-crossjumping", "no-reorder-blocks"))) sceNpDrmEdataSetupKey(int fd)
{
    int fd_arg = fd;
    int res = sceIoValidateFd(fd, 4);
    if (res == 0) {
        res = sceIoIoctl(fd, 0x04100006, NULL, 0, NULL, 0);
        uint32_t k1 = psp_get_k1();
        if (res != 0) {
            psp_set_k1(k1 << 11);
            res = sceKernelLockMutex(g_npdrm_mutex, 1, NULL);
            if (res == 0) {
                res = sceKernelExtendKernelStack(0x1000, sub_17d8, &fd_arg);
                sceKernelUnlockMutex(g_npdrm_mutex, 1);
            }
            psp_set_k1(k1);
        }
    }
    return res;
}

/* 0x17d8 */
int sub_17d8(void *arg)
{
    uint8_t hdr[0x90];
    uint32_t range[2];
    uint8_t key[16];
    uint32_t flags_word;

    int fd = *(int *)arg;
    range[0] = 0;
    range[1] = 0x90;
    memset(key, 0, 16);

    int res = sceIoIoctl(fd, 0x04100005, range, 8, hdr, 0x90);
    if (res < 0) {
        if (res == (int)0x80020325) {
            return 0x80550901;
        }
        return (res == (int)0x80020328) ? res : (int)0x80550913;
    }

    if (hdr[0x0e] != 0) {
        return 0x80550913;
    }

    res = sub_0760(key, hdr, 0x90);
    if (res < 0) {
        return sub_191c(res);
    }

    flags_word = *(uint16_t *)(hdr + 0x0c);
    res = sceIoIoctl(fd, 0x04100002, &flags_word, 4, NULL, 0);
    if (res < 0) {
        return sub_191c(res);
    }

    res = sceIoIoctl(fd, 0x04100001, key, 16, NULL, 0);
    if (res < 0) {
        return sub_191c(res);
    }
    return 0;
}

/* 0x191c */
static int __attribute__((noinline)) sub_191c(int err)
{
    switch ((uint32_t)err) {
    case 0x80510201:
        return 0x80550901;
    case 0x80510202:
    case 0x80510203:
        return 0x80550902;
    case 0x80510204:
    case 0x80510205:
    case 0x80510206:
    case 0x80510207:
        return 0x80550913;
    default:
        return err;
    }
}

/* 0x1978 */
static int __attribute__((noinline)) sub_1978(const void *sig, const void *pubkey, const void *data, uint32_t size)
{
    uint8_t digest[20];
    int res;

    if (size < 0x801) {
        res = sceDdrdbHash(data, (int)size, digest);
    } else {
        res = sub_1cc0((const uint8_t *)data, size, digest);
    }
    if (res >= 0) {
        res = sceDdrdbSigvry(pubkey, digest, sig);
    }
    return res;
}

/* 0x19f8 */
static int __attribute__((noinline)) sub_19f8(const uint8_t *in, uint8_t *out, const uint8_t *key)
{
    uint32_t ctx[0x3cc / 4];
    int res = sub_2d1c(ctx, 0x80, 0x80, key);
    if (res >= 0) {
        res = sub_208c(ctx, in, out);
    }
    volatile uint8_t *p = (volatile uint8_t *)ctx;
    for (int i = 0; i < 0x3cc; i++) {
        p[i] = 0;
    }
    return res;
}

/* 0x1a70 */
static int __attribute__((noinline)) sub_1a70(const uint8_t *in, uint8_t *out, const uint8_t *key)
{
    uint32_t ctx[0x3cc / 4];
    int res = sub_2d1c(ctx, 0x80, 0x80, key);
    if (res >= 0) {
        res = sub_262c(ctx, in, out);
    }
    volatile uint8_t *p = (volatile uint8_t *)ctx;
    for (int i = 0; i < 0x3cc; i++) {
        p[i] = 0;
    }
    return res;
}

static inline uint32_t rotl32(uint32_t x, unsigned int n)
{
    return (x << n) | (x >> (32 - n));
}

static inline uint32_t rotr32(uint32_t x, unsigned int n)
{
    return (x >> n) | (x << (32 - n));
}

/* 0x1b68 */
static void __attribute__((noinline)) sub_1b68(uint32_t *w, uint32_t *state)
{
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (unsigned int i = 0; i < 80; i++) {
        uint32_t wt = w[i & 15];
        if (i >= 16) {
            wt ^= w[(i + 2) & 15] ^ w[(i + 8) & 15] ^ w[(i + 13) & 15];
            wt = rotl32(wt, 1);
            w[i & 15] = wt;
        }
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5a827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdc;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6;
        }
        uint32_t temp = rotl32(a, 5) + f + e + k + wt;
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

/* 0x1cc0 */
static int __attribute__((noinline)) sub_1cc0(const uint8_t *data, uint32_t size, uint8_t *digest)
{
    uint32_t w[16];
    uint32_t state[5];

    for (int i = 0; i < 5; i++) {
        state[i] = g_sha1_iv[i];
    }

    uint32_t bit_len = size << 3;
    uint32_t rem = size;
    const uint8_t *p = data;

    while (rem >= 64) {
        for (int i = 0; i < 16; i++) {
            uint32_t v = ((uint32_t)p[0] << 24) |
                         ((uint32_t)p[1] << 16) |
                         ((uint32_t)p[2] << 8) |
                         (uint32_t)p[3];
            w[i] = v;
            p += 4;
        }
        sub_1b68(w, state);
        rem -= 64;
    }

    uint8_t block[64];
    memset(block, 0, 64);
    for (uint32_t i = 0; i < rem; i++) {
        block[i] = p[i];
    }
    block[rem] = 0x80;

    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4 + 0] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               (uint32_t)block[i * 4 + 3];
    }

    if (rem >= 56) {
        sub_1b68(w, state);
        for (int i = 0; i < 16; i++) {
            w[i] = 0;
        }
    }

    w[15] = bit_len;
    sub_1b68(w, state);

    for (int i = 0; i < 5; i++) {
        uint32_t v = state[i];
        digest[i * 4 + 0] = (uint8_t)(v >> 24);
        digest[i * 4 + 1] = (uint8_t)(v >> 16);
        digest[i * 4 + 2] = (uint8_t)(v >> 8);
        digest[i * 4 + 3] = (uint8_t)v;
    }
    return 0;
}

/* 0x1f70 */
static uint32_t __attribute__((noinline)) sub_1f70(uint32_t w)
{
    uint32_t b3 = (g_aes_te0[(w >> 24) & 0xff] >> 16) & 0xff;
    uint32_t b2 = (g_aes_te0[(w >> 16) & 0xff] >> 16) & 0xff;
    uint32_t b1 = (g_aes_te0[(w >> 8) & 0xff] >> 16) & 0xff;
    uint32_t b0 = (g_aes_te0[w & 0xff] >> 16) & 0xff;
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

/* 0x1fb0 */
static uint8_t __attribute__((noinline)) sub_1fb0(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0) {
        return 0;
    }
    uint32_t sum = (uint32_t)g_aes_gf_log[a] + (uint32_t)g_aes_gf_log[b];
    sum = (sum & 0xff) + (sum >> 8);
    return g_aes_gf_exp[sum & 0xff];
}

/* 0x1fe8 */
static uint8_t __attribute__((noinline)) sub_1fe8(uint32_t w, uint32_t coeff)
{
    uint8_t r0 = sub_1fb0((uint8_t)(w & 0xff), (uint8_t)(coeff & 0xff));
    uint8_t r1 = sub_1fb0((uint8_t)((w >> 8) & 0xff), (uint8_t)((coeff >> 8) & 0xff));
    uint8_t r2 = sub_1fb0((uint8_t)((w >> 16) & 0xff), (uint8_t)((coeff >> 16) & 0xff));
    uint8_t r3 = sub_1fb0((uint8_t)((w >> 24) & 0xff), (uint8_t)((coeff >> 24) & 0xff));
    return r0 ^ r1 ^ r2 ^ r3;
}

/* 0x2038 */
static uint32_t __attribute__((noinline)) sub_2038(uint32_t w)
{
    const uint32_t c = 0x0e090d0b;
    uint32_t b3 = sub_1fe8(w, c);
    uint32_t b2 = sub_1fe8(w, rotr32(c, 8));
    uint32_t b1 = sub_1fe8(w, rotr32(c, 16));
    uint32_t b0 = sub_1fe8(w, rotr32(c, 24));
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

static inline uint32_t aes_enc_col(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    return g_aes_te0[s0 & 0xff] ^
           rotr32(g_aes_te0[(s1 >> 8) & 0xff], 24) ^
           rotr32(g_aes_te0[(s2 >> 16) & 0xff], 16) ^
           rotr32(g_aes_te0[(s3 >> 24) & 0xff], 8);
}

static inline uint32_t aes_enc_last(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    uint32_t b0 = (g_aes_te0[s0 & 0xff] >> 16) & 0xff;
    uint32_t b1 = (g_aes_te0[(s1 >> 8) & 0xff] >> 16) & 0xff;
    uint32_t b2 = (g_aes_te0[(s2 >> 16) & 0xff] >> 16) & 0xff;
    uint32_t b3 = (g_aes_te0[(s3 >> 24) & 0xff] >> 16) & 0xff;
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

static inline uint32_t aes_dec_col(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    return g_aes_td0[s0 & 0xff] ^
           rotr32(g_aes_td0[(s1 >> 8) & 0xff], 24) ^
           rotr32(g_aes_td0[(s2 >> 16) & 0xff], 16) ^
           rotr32(g_aes_td0[(s3 >> 24) & 0xff], 8);
}

static inline uint32_t aes_dec_last(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    uint32_t b0 = g_aes_isbox[s0 & 0xff];
    uint32_t b1 = g_aes_isbox[(s1 >> 8) & 0xff];
    uint32_t b2 = g_aes_isbox[(s2 >> 16) & 0xff];
    uint32_t b3 = g_aes_isbox[(s3 >> 24) & 0xff];
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

/* 0x208c */
static int __attribute__((noinline)) sub_208c(const uint32_t *ctx, const uint8_t *in, uint8_t *out)
{
    uint32_t nb = ctx[0x3c4 / 4];
    uint32_t nr = ctx[0x3c8 / 4];
    if (nb != 4) {
        return -1;
    }

    const uint32_t *rk = ctx;
    const uint32_t *in32 = (const uint32_t *)in;
    uint32_t *out32 = (uint32_t *)out;

    uint32_t s0 = in32[0] ^ rk[0];
    uint32_t s1 = in32[1] ^ rk[1];
    uint32_t s2 = in32[2] ^ rk[2];
    uint32_t s3 = in32[3] ^ rk[3];

    for (uint32_t r = 1; r < nr; r++) {
        rk += 4;
        uint32_t t0 = aes_enc_col(s0, s1, s2, s3) ^ rk[0];
        uint32_t t1 = aes_enc_col(s1, s2, s3, s0) ^ rk[1];
        uint32_t t2 = aes_enc_col(s2, s3, s0, s1) ^ rk[2];
        uint32_t t3 = aes_enc_col(s3, s0, s1, s2) ^ rk[3];
        s0 = t0;
        s1 = t1;
        s2 = t2;
        s3 = t3;
    }

    rk += 4;
    out32[0] = aes_enc_last(s0, s1, s2, s3) ^ rk[0];
    out32[1] = aes_enc_last(s1, s2, s3, s0) ^ rk[1];
    out32[2] = aes_enc_last(s2, s3, s0, s1) ^ rk[2];
    out32[3] = aes_enc_last(s3, s0, s1, s2) ^ rk[3];
    return 0;
}

/* 0x262c */
static int __attribute__((noinline)) sub_262c(const uint32_t *ctx, const uint8_t *in, uint8_t *out)
{
    uint32_t nb = ctx[0x3c4 / 4];
    uint32_t nr = ctx[0x3c8 / 4];
    if (nb != 4) {
        return -1;
    }

    const uint32_t *rk = ctx + (0x1e0 / 4);
    const uint32_t *in32 = (const uint32_t *)in;
    uint32_t *out32 = (uint32_t *)out;

    uint32_t s0 = in32[0] ^ rk[0];
    uint32_t s1 = in32[1] ^ rk[1];
    uint32_t s2 = in32[2] ^ rk[2];
    uint32_t s3 = in32[3] ^ rk[3];

    for (uint32_t r = 1; r < nr; r++) {
        rk += 4;
        uint32_t t0 = aes_dec_col(s0, s3, s2, s1) ^ rk[0];
        uint32_t t1 = aes_dec_col(s1, s0, s3, s2) ^ rk[1];
        uint32_t t2 = aes_dec_col(s2, s1, s0, s3) ^ rk[2];
        uint32_t t3 = aes_dec_col(s3, s2, s1, s0) ^ rk[3];
        s0 = t0;
        s1 = t1;
        s2 = t2;
        s3 = t3;
    }

    rk += 4;
    out32[0] = aes_dec_last(s0, s3, s2, s1) ^ rk[0];
    out32[1] = aes_dec_last(s1, s0, s3, s2) ^ rk[1];
    out32[2] = aes_dec_last(s2, s1, s0, s3) ^ rk[2];
    out32[3] = aes_dec_last(s3, s2, s1, s0) ^ rk[3];
    return 0;
}

/* 0x2d1c */
static int __attribute__((noinline)) sub_2d1c(uint32_t *ctx, int block_bits, int key_bits, const uint8_t *key)
{
    if (ctx == NULL || key == NULL) {
        return 0x80000103;
    }
    if (block_bits != 128 && block_bits != 192 && block_bits != 256) {
        return 0x800001fe;
    }
    if (key_bits != 128 && key_bits != 192 && key_bits != 256) {
        return 0x800001fe;
    }

    int nb = block_bits / 32;
    int nk = key_bits / 32;
    int nr = (nb < nk ? nk : nb) + 6;

    ctx[0x3c0 / 4] = (uint32_t)nk;
    ctx[0x3c4 / 4] = (uint32_t)nb;
    ctx[0x3c8 / 4] = (uint32_t)nr;

    for (int i = 0; i < nk; i++) {
        ctx[i] = ((uint32_t)key[i * 4 + 3] << 24) |
                 ((uint32_t)key[i * 4 + 2] << 16) |
                 ((uint32_t)key[i * 4 + 1] << 8) |
                 (uint32_t)key[i * 4 + 0];
    }

    int total = nb * (nr + 1);
    if (nk < 7) {
        int rcon_idx = 0;
        for (int i = nk; i < total; i += nk) {
            uint32_t temp = sub_1f70(rotr32(ctx[i - 1], 8)) ^ g_aes_rcon[rcon_idx++];
            ctx[i] = ctx[i - nk] ^ temp;
            for (int j = 1; j < nk && (i + j) < total; j++) {
                ctx[i + j] = ctx[i + j - nk] ^ ctx[i + j - 1];
            }
        }
    } else {
        int rcon_idx = 0;
        for (int i = nk; i < total; i += nk) {
            uint32_t temp = sub_1f70(rotr32(ctx[i - 1], 8)) ^ g_aes_rcon[rcon_idx++];
            ctx[i] = ctx[i - nk] ^ temp;
            for (int j = 1; j < 4 && (i + j) < total; j++) {
                ctx[i + j] = ctx[i + j - nk] ^ ctx[i + j - 1];
            }
            if (i + 4 < total) {
                ctx[i + 4] = ctx[i + 4 - nk] ^ sub_1f70(ctx[i + 3]);
            }
            for (int j = 5; j < nk && (i + j) < total; j++) {
                ctx[i + j] = ctx[i + j - nk] ^ ctx[i + j - 1];
            }
        }
    }

    uint32_t *drk = ctx + (0x1e0 / 4);
    for (int j = 0; j < nb; j++) {
        drk[(total - nb) + j] = ctx[j];
    }
    for (int i = nb; i < total - nb; i += nb) {
        int dst = (total - nb) - i;
        for (int j = 0; j < nb; j++) {
            drk[dst + j] = sub_2038(ctx[i + j]);
        }
    }
    for (int j = 0; j < nb; j++) {
        drk[j] = ctx[(total - nb) + j];
    }
    return 0;
}
