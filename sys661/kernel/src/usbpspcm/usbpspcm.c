#include <common_imp.h>

extern int sceUsb_driver_6C68EE25(int id, void *cb);
extern int sceUsb_driver_9F89B84C(void *out);
extern int sceUsb_driver_C99E1766(int id, void *cb);

extern int sceIoAddDrv(void *drv);
extern int sceIoDelDrv(const char *name);

extern int sceKernelSetEventFlag(int evid, u32 bits);
extern int sceKernelWaitThreadEnd(int thid, u32 *timeout);
extern int sceKernelDeleteSema(int semaid);
extern int sceKernelTerminateDeleteThread(int thid);
extern int sceKernelSignalSema(int semaid, int signal);
extern int sceKernelWaitEventFlag(int evid, u32 bits, int wait, u32 *outBits, u32 *timeout);
extern int sceKernelCreateThread(const char *name, void *entry, int initPriority, int stackSize, u32 attr, void *option);
extern int sceKernelCreateEventFlag(const char *name, int attr, int bits, void *opt);
extern int sceKernelGetThreadmanIdType(int uid);
extern int sceKernelPollSema(int semaid, int signal);
extern int sceKernelTerminateThread(int thid);
extern int sceKernelUnlockMutex(int mutexid, int count);
extern int sceKernelExitThread(int status);
extern int sceKernelLockMutex(int mutexid, int count, u32 *timeout);
extern int sceKernelCreateMutex(const char *name, u32 attr, int initCount, void *opt);
extern int sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *option);
extern int sceKernelDeleteEventFlag(int evid);
extern int sceKernelStartThread(int thid, u32 arglen, void *argp);
extern int sceKernelDeleteMutex(int mutexid);

extern void sceKernelDcacheWritebackRange(const void *p, unsigned int size);
extern int sceKernelDcacheInvalidateRangeForUser(void *p, unsigned int size);

extern void *memset(void *s, int c, u32 n);
extern u32 strlen(const char *s);
extern char *strncpy(char *dst, const char *src, u32 n);
extern int snprintf(char *str, u32 size, const char *format, ...);

extern void *sceKernelMemcpy(void *dst, const void *src, u32 n);
extern void *sceKernelMemset(void *s, int c, u32 n);
extern int sceKernelGetUIDcontrolBlock(int uid, void **cb);

extern int sceKermit_driver_36666181(void *pkt, u32 mode, u32 cmd, u32 argc, u32 allow_cb, void *resp);
extern int sceKermit_driver_5280B410(u32 id, void *cb);
extern int sceKermitMemory_driver_90B662D0(void *addr, u32 size);
extern int sceKermitMemory_driver_AAF047AC(void *pkt, u32 arg_idx, void *addr, u32 size, u32 dir);

typedef struct {
    void *iob;
    int kermit_fd;
} ScePspcmFdSlot;

typedef struct {
    int unk0;
    int fs_num;
    int unk8;
    int unkc;
    ScePspcmFdSlot *slot;
} ScePspcmIob;

static u32 g_io_drv[27];          /* 0x1b80..0x1beb (size 0x6c) */
static ScePspcmFdSlot g_fds[8];   /* 0x1bec..0x1c2b (size 0x40) */
static int g_conn_thid;           /* 0x1c2c */
static int g_conn_evid;           /* 0x1c30 */
static int g_fd_mutex;            /* 0x1c34 */
static int g_buf_mutex;           /* 0x1c38 */
static u8 g_buf_used[0x60];       /* 0x1c3c..0x1c9b */
static int g_connected;           /* 0x1c9c */
static int g_wait_thid;           /* 0x1ca0 */
static int g_conn_semaid;         /* 0x1ca4 */

static int sub_0000(void);
static int sub_00cc(void);
static int sub_022c(void);
static int sub_0288(void);
static int sub_02b0(void);
static int sub_03ac(void);
static int sub_03d0(void);
static int sub_03d8(void);
static int sub_03e0(ScePspcmIob *iob, const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);
static int sub_06a4(ScePspcmIob *iob, const char *file, int flags, int mode);
static int sub_06f0(ScePspcmIob *iob);
static int sub_070c(ScePspcmIob *iob, char *data, int len);
static int sub_073c(ScePspcmIob *iob, const char *data, int len);
static int sub_07a0(ScePspcmIob *iob, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);
static int sub_08b4(void);
static void sub_0a38(void);
static int sub_0b14(const char *dev, u32 cmd, const void *indata, u32 inlen, void *outdata, u32 outlen);
static int sub_0cd4(ScePspcmIob *iob, const char *file, int flags, int mode);
static int sub_0eb4(ScePspcmIob *iob);
static int sub_0fd4(ScePspcmIob *iob, char *data, int len);
static int sub_1260(ScePspcmIob *iob, const char *data, int len);
static int sub_139c(ScePspcmIob *iob, u32 cmd, const void *indata, u32 inlen);
static int sub_150c(void);
static int sub_1530(u32 args, void *argp);
static int sub_1570(u32 **out_buf);
static int sub_163c(int slot);
static void sub_16b8(void);

static const void * const g_usb_cb[2] = {
    sub_0000,
    sub_00cc
};

static int sub_0000(void)
{
    if (sub_022c() < 0)
        return -1;
    if (sub_02b0() < 0) {
        sub_0288();
        return -1;
    }
    g_connected = 1;
    return 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    if (sceUsb_driver_C99E1766(5, (void *)g_usb_cb) < 0)
        return 1;
    return (sub_08b4() < 0) ? 1 : 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    if (sceUsb_driver_6C68EE25(5, (void *)g_usb_cb) < 0)
        return 1;
    sub_0a38();
    return 0;
}

static int sub_00cc(void)
{
    g_connected = -1;
    sub_03ac();
    sub_0288();
    return 0;
}

static int sub_00fc(u32 cmd, int thid)
{
    int ret = sceKernelPollSema(g_conn_semaid, 1);
    if (ret < 0)
        return (ret == (int)0x800201ad) ? (int)0x80243406 : ret;

    if (cmd == 0x03415001) {
        if (g_wait_thid > 0)
            ret = 0x80243401;
        else
            g_wait_thid = thid;
    } else {
        if (g_wait_thid < 0) {
            ret = 0x80243405;
        } else if (g_wait_thid == thid) {
            sceKernelTerminateThread(thid);
            g_wait_thid = -1;
        } else {
            ret = 0x80243402;
        }
    }
    sceKernelSignalSema(g_conn_semaid, 1);
    return ret;
}

static int sub_01e4(void *arg)
{
    int ret = sceKernelStartThread(g_wait_thid, 8, arg);
    if (ret >= 0) {
        ret = sceKernelWaitThreadEnd(g_wait_thid, NULL);
        if (ret > 0)
            ret = 0;
    }
    return ret;
}

static int sub_022c(void)
{
    int semaid = sceKernelCreateSema("SceUsbPspcmConnect", 0x101, 1, 1, NULL);
    g_conn_semaid = semaid;
    if (semaid <= 0)
        return -1;
    g_wait_thid = -1;
    return 0;
}

static int sub_0288(void)
{
    sceKernelDeleteSema(g_conn_semaid);
    return 0;
}

static int sub_02b0(void)
{
    g_io_drv[0] = 0;
    g_io_drv[1] = 0;
    g_io_drv[2] = 0;
    g_io_drv[3] = 0;
    g_io_drv[4] = 0;
    memset(&g_io_drv[5], 0, 0x58);
    g_io_drv[0] = (u32)"usbpspcm";
    g_io_drv[1] = 1;
    g_io_drv[2] = 0x200;
    g_io_drv[3] = (u32)"USBPSPCommunicationDriver";
    g_io_drv[4] = (u32)&g_io_drv[5];
    g_io_drv[5] = (u32)sub_03d0;
    g_io_drv[6] = (u32)sub_03d8;
    g_io_drv[7] = (u32)sub_06a4;
    g_io_drv[8] = (u32)sub_06f0;
    g_io_drv[9] = (u32)sub_070c;
    g_io_drv[10] = (u32)sub_073c;
    g_io_drv[12] = (u32)sub_07a0;
    g_io_drv[25] = (u32)sub_03e0;
    return (sceIoAddDrv(g_io_drv) < 0) ? -1 : 0;
}

static int sub_03ac(void)
{
    sceIoDelDrv("usbpspcm");
    return 0;
}

static int sub_03d0(void)
{
    return 0;
}

static int sub_03d8(void)
{
    return 0;
}

static int sub_03e0(ScePspcmIob *iob, const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    u32 k1 = pspGetK1();
    (void)iob;

    if (cmd <= 0x03415002) {
        if (cmd >= 0x03415001) {
            void *cb = NULL;
            if (!indata || (u32)inlen < 4)
                return 0x80243402;
            if ((s32)(k1 & (((u32)indata + (u32)inlen) | (u32)indata | (u32)inlen)) < 0)
                return 0x80243402;
            if (sceKernelGetThreadmanIdType(*(int *)indata) != 1)
                return 0x80243402;
            if (sceKernelGetUIDcontrolBlock(*(int *)indata, &cb) != 0)
                return 0x80243402;
            if ((k1 >> 31) != 0 && (*(u16 *)((u8 *)cb + 0x16) & 2) == 0)
                return 0x80243402;
            int ret = sub_00fc(cmd, *(int *)indata);
            if (ret != 0)
                return ret;
            return sub_0b14(dev, cmd, indata, 4, NULL, 0);
        }
        if (cmd == 0x03407003)
            return sub_0b14(dev, 0x03407003, NULL, 0, NULL, 0);
        return 0x80243402;
    }

    if (cmd == 0x03427004) {
        if (!outdata || (u32)outlen < 4)
            return 0x80243402;
        if ((s32)(k1 & (((u32)outdata + (u32)outlen) | (u32)outdata | (u32)outlen)) < 0)
            return 0x80243402;
        return sub_0b14(dev, 0x03427004, NULL, 0, outdata, 4);
    }

    if (cmd < 0x03427004) {
        if (cmd != 0x03415006)
            return 0x80243402;
        if (!indata || (u32)inlen < 4)
            return 0x80243402;
        if ((s32)(k1 & (((u32)indata + (u32)inlen) | (u32)indata | (u32)inlen)) < 0)
            return 0x80243402;
        return sub_0b14(dev, 0x03415006, indata, 4, NULL, 0);
    }

    if (cmd == 0x03435005) {
        u32 tmp[4];
        if (!indata || (u32)inlen < 0x0c)
            return 0x80243402;
        if ((s32)(k1 & (((u32)indata + (u32)inlen) | (u32)indata | (u32)inlen)) < 0)
            return 0x80243402;
        if (!outdata || (u32)outlen < 0x10)
            return 0x80243402;
        if ((s32)(k1 & (((u32)outdata + (u32)outlen) | (u32)outdata | (u32)outlen)) < 0)
            return 0x80243402;
        u32 *in32 = (u32 *)indata;
        tmp[0] = in32[0];
        tmp[1] = in32[1];
        if (tmp[0] != 0x0c)
            return 0x80243402;
        u32 ptr = in32[2];
        if (ptr != 0) {
            if ((s32)(((ptr + 4) | ptr) & k1) < 0)
                return 0x80243402;
            tmp[2] = *(u32 *)ptr;
            tmp[3] = 0;
        } else {
            tmp[2] = 0;
            tmp[3] = 1;
        }
        return sub_0b14(dev, cmd, tmp, 0x10, outdata, 0x10);
    }

    return 0x80243402;
}

static int sub_06a4(ScePspcmIob *iob, const char *file, int flags, int mode)
{
    if (g_connected < 0)
        return 0x80243407;
    if (iob->fs_num >= 8)
        return 0x80243402;
    return sub_0cd4(iob, file, flags, mode);
}

static int sub_06f0(ScePspcmIob *iob)
{
    return sub_0eb4(iob);
}

static int sub_070c(ScePspcmIob *iob, char *data, int len)
{
    if (!data)
        return 0x80243402;
    return sub_0fd4(iob, data, len);
}

static int sub_073c(ScePspcmIob *iob, const char *data, int len)
{
    if (!data && (u32)len > 0)
        return 0x80243402;
    if ((u32)data & 3)
        return 0x80243402;
    if (((0x00220202 >> (((u32)data >> 27) & 0x1f)) & 1) == 0)
        return 0x80243402;
    return sub_1260(iob, data, len);
}

static int sub_07a0(ScePspcmIob *iob, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    u32 k1 = pspGetK1();
    (void)outdata;
    (void)outlen;

    if (cmd == 0x03406105 || cmd == 0x03405101 || cmd == 0x03406103)
        return sub_139c(iob, cmd, NULL, 0);

    if (cmd == 0x03415104) {
        if (indata) {
            if ((u32)inlen < 4)
                return 0x80243402;
            if ((s32)(k1 & (((u32)indata + (u32)inlen) | (u32)indata | (u32)inlen)) < 0)
                return 0x80243402;
        }
        return sub_139c(iob, cmd, indata, 4);
    }

    if (cmd == 0x03416102) {
        if (!indata || (u32)inlen < 4)
            return 0x80243402;
        if ((s32)(k1 & (((u32)indata + (u32)inlen) | (u32)indata | (u32)inlen)) < 0)
            return 0x80243402;
        if ((*(u32 *)indata & ~3u) != 0)
            return 0x80243402;
        return sub_139c(iob, cmd, indata, 4);
    }

    return 0x80243402;
}

static int sub_08b4(void)
{
    u32 *pkt;
    u64 resp;

    sceKernelMemset(g_buf_used, 0, 0x60);
    if (sceKermit_driver_5280B410(0x0d, sub_150c) != 0)
        return 1;
    int mtx = sceKernelCreateMutex("SceKermitPspCmFd", 0x100, 0, NULL);
    if (mtx <= 0)
        return 1;
    g_fd_mutex = mtx;

    mtx = sceKernelCreateMutex("SceKermitPspCmBuffer", 0x100, 0, NULL);
    if (mtx <= 0)
        return 1;
    g_buf_mutex = mtx;

    int evid = sceKernelCreateEventFlag("ScePspcmkconnIntr", 0x201, 0, NULL);
    if (evid <= 0)
        return 1;
    g_conn_evid = evid;

    int thid = sceKernelCreateThread("pspcmkconn", sub_1530, 0x10, 0x800, 0x00100001, NULL);
    if (thid < 0)
        return 1;
    g_conn_thid = thid;

    if (sceKernelStartThread(thid, 0, NULL) < 0)
        return 1;

    int slot = sub_1570(&pkt);
    int res = slot;
    if (slot >= 0) {
        int kret = sceKermit_driver_36666181(pkt, 8, 0xc63, 0, 0, &resp);
        res = (kret == 0) ? (int)resp : kret;
        sub_163c(slot);
    }
    return (res != 0) ? 1 : 0;
}

static void sub_0a38(void)
{
    u32 *pkt;
    u64 resp;

    sceKermit_driver_5280B410(0x0d, NULL);
    int slot = sub_1570(&pkt);
    if (slot >= 0) {
        sceKermit_driver_36666181(pkt, 8, 0xc6e, 0, 0, &resp);
        sub_163c(slot);
    }
    if (g_conn_thid != 0)
        sceKernelTerminateDeleteThread(g_conn_thid);
    if (g_conn_evid != 0)
        sceKernelDeleteEventFlag(g_conn_evid);
    if (g_fd_mutex != 0)
        sceKernelDeleteMutex(g_fd_mutex);
    if (g_buf_mutex != 0)
        sceKernelDeleteMutex(g_buf_mutex);
}

static int sub_0b14(const char *dev, u32 cmd, const void *indata, u32 inlen, void *outdata, u32 outlen)
{
    u32 *pkt;
    u64 resp;

    int slot = sub_1570(&pkt);
    if (slot < 0)
        return slot;

    char *dev_buf = (char *)pkt + 0x4000;
    void *in_buf = (u8 *)pkt + 0x4800;
    void *out_buf = (u8 *)pkt + 0x5000;

    strncpy(dev_buf, dev, 0x800);
    dev_buf[0x7ff] = '\0';
    if (indata && inlen > 0)
        sceKernelMemcpy(in_buf, indata, inlen);
    sceKernelMemset(out_buf, 0, outlen);

    pkt[0] = 0xc60;
    u32 dev_len = strlen(dev_buf) + 1;
    sceKermitMemory_driver_AAF047AC(pkt, 0, dev_buf, dev_len, 1);
    pkt[6] = cmd;
    sceKermitMemory_driver_AAF047AC(pkt, 2, in_buf, inlen, 1);
    pkt[10] = inlen;
    sceKermitMemory_driver_AAF047AC(pkt, 4, out_buf, outlen, 2);
    pkt[14] = outlen;

    int ret = sceKermit_driver_36666181(pkt, 8, 0xc60, 6, 0, &resp);
    if (ret == 0) {
        ret = (int)resp;
        if (outdata && outlen > 0)
            sceKernelMemcpy(outdata, out_buf, outlen);
    }
    sub_163c(slot);
    return ret;
}

static int sub_0cd4(ScePspcmIob *iob, const char *file, int flags, int mode)
{
    u32 *pkt;
    u64 resp;

    if (iob->fs_num >= 8)
        return 0x80010013;

    int ret = sceKernelLockMutex(g_fd_mutex, 1, NULL);
    if (ret != 0)
        return 0;

    int idx;
    for (idx = 0; idx < 8; idx++) {
        if (g_fds[idx].iob == NULL)
            break;
    }
    if (idx >= 8)
        return 0x80010018;

    ScePspcmFdSlot *fd_slot = &g_fds[idx];
    fd_slot->iob = iob;
    ret = sceKernelUnlockMutex(g_fd_mutex, 1);
    if (ret != 0)
        return 0;

    iob->slot = fd_slot;
    int bslot = sub_1570(&pkt);
    if (bslot < 0)
        return bslot;

    char *path_buf = (char *)pkt + 0x4000;
    snprintf(path_buf, 0x2000, "%s%d", file, iob->fs_num);
    path_buf[0x1fff] = '\0';
    u32 path_len = strlen(path_buf) + 1;
    sceKermitMemory_driver_AAF047AC(pkt, 0, path_buf, path_len, 1);
    pkt[6] = flags;
    pkt[8] = mode;

    ret = sceKermit_driver_36666181(pkt, 8, 0xc67, 3, 0, &resp);
    int kfd = (int)resp;
    if (ret == 0) {
        fd_slot->kermit_fd = kfd;
        sub_163c(bslot);
        return kfd;
    }

    iob->slot = NULL;
    ret = sceKernelLockMutex(g_fd_mutex, 1, NULL);
    if (ret != 0)
        return ret;
    fd_slot->iob = NULL;
    ret = sceKernelUnlockMutex(g_fd_mutex, 1);
    if (ret != 0)
        return ret;
    sub_163c(bslot);
    return 0;
}

static int sub_0eb4(ScePspcmIob *iob)
{
    u32 *pkt;
    u64 resp;

    int bslot = sub_1570(&pkt);
    if (bslot < 0)
        return bslot;
    if (iob->fs_num >= 8)
        return 0x80010013;

    ScePspcmFdSlot *fd_slot = iob->slot;
    if (fd_slot < &g_fds[0] || fd_slot >= &g_fds[8] || fd_slot->iob != iob)
        return 0x80010009;

    pkt[4] = fd_slot->kermit_fd;
    int ret = sceKermit_driver_36666181(pkt, 8, 0xc6b, 1, 0, &resp);
    if (ret == 0) {
        iob->slot = NULL;
        ret = (int)resp;
        int mret = sceKernelLockMutex(g_fd_mutex, 1, NULL);
        if (mret != 0)
            return mret;
        fd_slot->iob = NULL;
        mret = sceKernelUnlockMutex(g_fd_mutex, 1);
        if (mret != 0)
            return mret;
    }
    sub_163c(bslot);
    return ret;
}

static int sub_0fd4(ScePspcmIob *iob, char *data, int len)
{
    u32 *pkt;
    u64 resp;

    if (iob->fs_num >= 8)
        return 0x80010013;
    ScePspcmFdSlot *fd_slot = iob->slot;
    if (fd_slot < &g_fds[0] || fd_slot >= &g_fds[8] || fd_slot->iob != iob)
        return 0x80010009;

    int bslot = sub_1570(&pkt);
    if (bslot < 0)
        return bslot;

    u8 *bounce = (u8 *)pkt + 0x4000;
    int total = 0;
    u32 unaligned = (u32)data & 0x3f;
    u32 rem = (u32)len;

    if (unaligned != 0) {
        u32 head = 0x40 - unaligned;
        if (rem < head)
            head = rem;
        pkt[4] = fd_slot->kermit_fd;
        sceKermitMemory_driver_AAF047AC(pkt, 1, bounce, head, 2);
        pkt[8] = head;
        int ret = sceKermit_driver_36666181(pkt, 8, 0xc5e, 3, 0, &resp);
        total = ret;
        if (ret != 0)
            goto done;
        int rlen = (int)resp;
        if (rlen <= 0)
            goto done;
        total = rlen;
        sceKernelMemcpy(data, bounce, rlen);
        if ((u32)rlen != head)
            goto done;
        rem -= rlen;
    }

    for (;;) {
        u32 tail = rem & 0x3f;
        u32 body = rem - tail;
        char *cur_dst = data + total;
        if (body >= 0x40) {
            sceKernelDcacheInvalidateRangeForUser(cur_dst, body);
            pkt[4] = fd_slot->kermit_fd;
            sceKermitMemory_driver_AAF047AC(pkt, 1, cur_dst, body, 2);
            pkt[8] = body;
            int ret = sceKermit_driver_36666181(pkt, 8, 0xc5e, 3, 0, &resp);
            if (ret != 0)
                goto done;
            int rlen = (int)resp;
            if (rlen <= 0) {
                if (rlen < 0)
                    total = rlen;
                goto done;
            }
            sceKermitMemory_driver_90B662D0(cur_dst, rlen);
            total += rlen;
            if ((u32)rlen != body)
                goto done;
            rem -= rlen;
            continue;
        }
        if (rem != 0) {
            pkt[4] = fd_slot->kermit_fd;
            sceKermitMemory_driver_AAF047AC(pkt, 1, bounce, rem, 2);
            pkt[8] = rem;
            int ret = sceKermit_driver_36666181(pkt, 8, 0xc5e, 3, 0, &resp);
            if (ret != 0)
                goto done;
            int rlen = (int)resp;
            if (rlen > 0) {
                sceKernelMemcpy(data + total, bounce, rlen);
                total += rlen;
            } else if (rlen < 0) {
                total = rlen;
            }
        }
        break;
    }

done:
    sub_163c(bslot);
    return total;
}

static int sub_1260(ScePspcmIob *iob, const char *data, int len)
{
    u32 *pkt;
    u64 resp;

    if (iob->fs_num >= 8)
        return 0x80010013;
    ScePspcmFdSlot *fd_slot = iob->slot;
    if (fd_slot < &g_fds[0] || fd_slot >= &g_fds[8] || fd_slot->iob != iob)
        return 0x80010009;

    u32 off = (u32)data & 0x3f;
    u32 wb_size = (off + (u32)len + 0x3f) & ~0x3fu;
    sceKernelDcacheWritebackRange(data - off, wb_size);

    int bslot = sub_1570(&pkt);
    if (bslot < 0)
        return bslot;

    pkt[4] = fd_slot->kermit_fd;
    sceKermitMemory_driver_AAF047AC(pkt, 1, (void *)data, len, 1);
    pkt[8] = len;
    int ret = sceKermit_driver_36666181(pkt, 8, 0xc6a, 3, 0, &resp);
    if (ret == 0)
        ret = (int)resp;
    sub_163c(bslot);
    return ret;
}

static int sub_139c(ScePspcmIob *iob, u32 cmd, const void *indata, u32 inlen)
{
    u32 *pkt;
    u64 resp;

    ScePspcmFdSlot *fd_slot = iob->slot;
    if (fd_slot < &g_fds[0] || fd_slot >= &g_fds[8] || fd_slot->iob != iob)
        return 0x80010009;

    int bslot = sub_1570(&pkt);
    if (bslot < 0)
        return bslot;

    void *in_buf = (u8 *)pkt + 0x4000;
    void *out_buf = (u8 *)pkt + 0x5000;
    if (indata && inlen > 0)
        sceKernelMemcpy(in_buf, indata, inlen);

    pkt[0] = 0xc6f;
    pkt[4] = fd_slot->kermit_fd;
    pkt[6] = cmd;
    sceKermitMemory_driver_AAF047AC(pkt, 2, in_buf, inlen, 1);
    pkt[10] = inlen;
    sceKermitMemory_driver_AAF047AC(pkt, 4, out_buf, 0, 2);
    pkt[14] = 0;

    int ret = sceKermit_driver_36666181(pkt, 8, 0xc6f, 6, 0, &resp);
    if (ret == 0)
        ret = (int)resp;
    sub_163c(bslot);
    return ret;
}

static int sub_150c(void)
{
    return sceKernelSetEventFlag(g_conn_evid, 1);
}

static int sub_1530(u32 args, void *argp)
{
    (void)args;
    (void)argp;
    for (;;) {
        if (sceKernelWaitEventFlag(g_conn_evid, 1, 0x21, NULL, NULL) == 0)
            sub_16b8();
    }
    return 0;
}

static int sub_1570(u32 **out_buf)
{
    int ret = sceKernelLockMutex(g_buf_mutex, 1, NULL);
    if (ret != 0)
        return ret;

    int slot;
    for (slot = 0; slot < 0x60; slot++) {
        if (g_buf_used[slot] == 0) {
            void *buf = (void *)(0xABA00000 + (slot << 15));
            g_buf_used[slot] = 1;
            *out_buf = (u32 *)buf;
            sceKernelMemset(buf, 0, 0x8000);
            break;
        }
    }

    ret = sceKernelUnlockMutex(g_buf_mutex, 1);
    if (ret != 0)
        return ret;
    return (slot < 0x60) ? slot : (int)0x80000022;
}

static int sub_163c(int slot)
{
    if ((u32)slot >= 0x60)
        return 0;
    int ret = sceKernelLockMutex(g_buf_mutex, 1, NULL);
    if (ret != 0)
        return ret;
    g_buf_used[slot] = 0;
    return sceKernelUnlockMutex(g_buf_mutex, 1);
}

static void sub_16b8(void)
{
    u32 info[2];
    if (sceUsb_driver_9F89B84C(info) < 0)
        return;
    if (info[1] & 0x80) {
        if ((info[1] & 2) || (info[1] & 1))
            g_connected = -1;
    }
    if (sub_01e4(info) < 0)
        sceKernelExitThread(g_wait_thid);
}
