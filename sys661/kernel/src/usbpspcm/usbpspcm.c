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

extern int sceKernelDcacheWritebackRange(const void *p, unsigned int size);
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
    u8 pad[0x16];
    u16 attr;
} ScePspcmUidCb;

typedef struct {
    u32 pkt[0x1000];
    union {
        struct {
            char dev[0x800];
            u8 inData[0x800];
            u8 outData[0x3000];
        } devctl;
        struct {
            char path[0x2000];
            u8 rest[0x2000];
        } open;
        struct {
            u8 inData[0x1000];
            u8 outData[0x3000];
        } ioctl;
        u8 bounce[0x4000];
    } payload;
} ScePspcmKermitBuf;

typedef struct {
    void *iob;
    int kermit_fd;
} ScePspcmFdSlot;

typedef struct {
    int flags;
    int fs_num;
    int dev_entry;
    int dev_type;
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

static int usbPspcmAttach(void);
static int usbPspcmDetach(void);
static int pspcmCreateConnectSema(void);
static int pspcmDeleteConnectSema(void);
static int pspcmRegisterIoDriver(void);
static int pspcmUnregisterIoDriver(void);
static int pspcmDrvInit(void);
static int pspcmDrvExit(void);
static int pspcmDrvDevctl(ScePspcmIob *iob, const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);
static int pspcmDrvOpen(ScePspcmIob *iob, const char *file, int flags, int mode);
static int pspcmDrvClose(ScePspcmIob *iob);
static int pspcmDrvRead(ScePspcmIob *iob, char *data, int len);
static int pspcmDrvWrite(ScePspcmIob *iob, const char *data, int len);
static int pspcmDrvIoctl(ScePspcmIob *iob, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);
static int kermitPspcmInit(void);
static void kermitPspcmTerm(void);
static int kermitPspcmDevctl(const char *dev, u32 cmd, const void *indata, u32 inlen, void *outdata, u32 outlen);
static int kermitPspcmOpen(ScePspcmIob *iob, const char *file, int flags, int mode);
static int kermitPspcmClose(ScePspcmIob *iob);
static int kermitPspcmRead(ScePspcmIob *iob, char *data, int len);
static int kermitPspcmWrite(ScePspcmIob *iob, const char *data, int len);
static int kermitPspcmIoctl(ScePspcmIob *iob, u32 cmd, const void *indata, u32 inlen);
static int pspcmConnIntrHandler(void);
static int pspcmConnThread(u32 args, void *argp);
static int pspcmAllocBufSlot(ScePspcmKermitBuf **out_buf);
static int pspcmFreeBufSlot(int slot);
static void pspcmHandleConnEvent(void);

static const void * const g_usb_cb[2] = {
    usbPspcmAttach,
    usbPspcmDetach
};

static int usbPspcmAttach(void)
{
    if (pspcmCreateConnectSema() < 0)
        return -1;
    if (pspcmRegisterIoDriver() < 0) {
        pspcmDeleteConnectSema();
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
    return (kermitPspcmInit() < 0) ? 1 : 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    if (sceUsb_driver_6C68EE25(5, (void *)g_usb_cb) < 0)
        return 1;
    kermitPspcmTerm();
    return 0;
}

static int usbPspcmDetach(void)
{
    g_connected = -1;
    pspcmUnregisterIoDriver();
    pspcmDeleteConnectSema();
    return 0;
}

static int pspcmSetWaitThread(u32 cmd, int thid)
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

static int pspcmNotifyWaitThread(void *arg)
{
    int ret = sceKernelStartThread(g_wait_thid, 8, arg);
    if (ret >= 0) {
        ret = sceKernelWaitThreadEnd(g_wait_thid, NULL);
        if (ret > 0)
            ret = 0;
    }
    return ret;
}

static int pspcmCreateConnectSema(void)
{
    int semaid = sceKernelCreateSema("SceUsbPspcmConnect", 0x101, 1, 1, NULL);
    g_conn_semaid = semaid;
    if (semaid <= 0)
        return -1;
    g_wait_thid = -1;
    return 0;
}

static int pspcmDeleteConnectSema(void)
{
    sceKernelDeleteSema(g_conn_semaid);
    return 0;
}

static int pspcmRegisterIoDriver(void)
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
    g_io_drv[5] = (u32)pspcmDrvInit;
    g_io_drv[6] = (u32)pspcmDrvExit;
    g_io_drv[7] = (u32)pspcmDrvOpen;
    g_io_drv[8] = (u32)pspcmDrvClose;
    g_io_drv[9] = (u32)pspcmDrvRead;
    g_io_drv[10] = (u32)pspcmDrvWrite;
    g_io_drv[12] = (u32)pspcmDrvIoctl;
    g_io_drv[25] = (u32)pspcmDrvDevctl;
    return (sceIoAddDrv(g_io_drv) < 0) ? -1 : 0;
}

static int pspcmUnregisterIoDriver(void)
{
    sceIoDelDrv("usbpspcm");
    return 0;
}

static int pspcmDrvInit(void)
{
    return 0;
}

static int pspcmDrvExit(void)
{
    return 0;
}

static int pspcmDrvDevctl(ScePspcmIob *iob, const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    u32 k1 = pspGetK1();
    (void)iob;

    if (cmd <= 0x03415002) {
        if (cmd >= 0x03415001) {
            ScePspcmUidCb *cb = NULL;
            if (!indata || (u32)inlen < 4)
                return 0x80243402;
            if ((s32)(k1 & (((u32)indata + (u32)inlen) | (u32)indata | (u32)inlen)) < 0)
                return 0x80243402;
            if (sceKernelGetThreadmanIdType(*(int *)indata) != 1)
                return 0x80243402;
            if (sceKernelGetUIDcontrolBlock(*(int *)indata, (void **)&cb) != 0)
                return 0x80243402;
            if ((k1 >> 31) != 0 && (cb->attr & 2) == 0)
                return 0x80243402;
            int ret = pspcmSetWaitThread(cmd, *(int *)indata);
            if (ret != 0)
                return ret;
            return kermitPspcmDevctl(dev, cmd, indata, 4, NULL, 0);
        }
        if (cmd == 0x03407003)
            return kermitPspcmDevctl(dev, 0x03407003, NULL, 0, NULL, 0);
        return 0x80243402;
    }

    if (cmd == 0x03427004) {
        if (!outdata || (u32)outlen < 4)
            return 0x80243402;
        if ((s32)(k1 & (((u32)outdata + (u32)outlen) | (u32)outdata | (u32)outlen)) < 0)
            return 0x80243402;
        return kermitPspcmDevctl(dev, 0x03427004, NULL, 0, outdata, 4);
    }

    if (cmd < 0x03427004) {
        if (cmd != 0x03415006)
            return 0x80243402;
        if (!indata || (u32)inlen < 4)
            return 0x80243402;
        if ((s32)(k1 & (((u32)indata + (u32)inlen) | (u32)indata | (u32)inlen)) < 0)
            return 0x80243402;
        return kermitPspcmDevctl(dev, 0x03415006, indata, 4, NULL, 0);
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
        return kermitPspcmDevctl(dev, cmd, tmp, 0x10, outdata, 0x10);
    }

    return 0x80243402;
}

static int pspcmDrvOpen(ScePspcmIob *iob, const char *file, int flags, int mode)
{
    if (g_connected < 0)
        return 0x80243407;
    if (iob->fs_num >= 8)
        return 0x80243402;
    return kermitPspcmOpen(iob, file, flags, mode);
}

static int pspcmDrvClose(ScePspcmIob *iob)
{
    return kermitPspcmClose(iob);
}

static int pspcmDrvRead(ScePspcmIob *iob, char *data, int len)
{
    if (!data)
        return 0x80243402;
    return kermitPspcmRead(iob, data, len);
}

static int pspcmDrvWrite(ScePspcmIob *iob, const char *data, int len)
{
    if (!data && (u32)len > 0)
        return 0x80243402;
    if ((u32)data & 3)
        return 0x80243402;
    if (((0x00220202 >> (((u32)data >> 27) & 0x1f)) & 1) == 0)
        return 0x80243402;
    return kermitPspcmWrite(iob, data, len);
}

static int pspcmDrvIoctl(ScePspcmIob *iob, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    u32 k1 = pspGetK1();
    (void)outdata;
    (void)outlen;

    if (cmd == 0x03406105 || cmd == 0x03405101 || cmd == 0x03406103)
        return kermitPspcmIoctl(iob, cmd, NULL, 0);

    if (cmd == 0x03415104) {
        if (indata) {
            if ((u32)inlen < 4)
                return 0x80243402;
            if ((s32)(k1 & (((u32)indata + (u32)inlen) | (u32)indata | (u32)inlen)) < 0)
                return 0x80243402;
        }
        return kermitPspcmIoctl(iob, cmd, indata, 4);
    }

    if (cmd == 0x03416102) {
        if (!indata || (u32)inlen < 4)
            return 0x80243402;
        if ((s32)(k1 & (((u32)indata + (u32)inlen) | (u32)indata | (u32)inlen)) < 0)
            return 0x80243402;
        if ((*(u32 *)indata & ~3u) != 0)
            return 0x80243402;
        return kermitPspcmIoctl(iob, cmd, indata, 4);
    }

    return 0x80243402;
}

static int kermitPspcmInit(void)
{
    ScePspcmKermitBuf *kbuf;
    u64 resp;

    sceKernelMemset(g_buf_used, 0, 0x60);
    if (sceKermit_driver_5280B410(0x0d, pspcmConnIntrHandler) != 0)
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

    int thid = sceKernelCreateThread("pspcmkconn", pspcmConnThread, 0x10, 0x800, 0x00100001, NULL);
    if (thid < 0)
        return 1;
    g_conn_thid = thid;

    if (sceKernelStartThread(thid, 0, NULL) < 0)
        return 1;

    int slot = pspcmAllocBufSlot(&kbuf);
    int res = slot;
    if (slot >= 0) {
        int kret = sceKermit_driver_36666181(kbuf->pkt, 8, 0xc63, 0, 0, &resp);
        res = (kret == 0) ? (int)resp : kret;
        pspcmFreeBufSlot(slot);
    }
    return (res != 0) ? 1 : 0;
}

static void kermitPspcmTerm(void)
{
    ScePspcmKermitBuf *kbuf;
    u64 resp;

    sceKermit_driver_5280B410(0x0d, NULL);
    int slot = pspcmAllocBufSlot(&kbuf);
    if (slot >= 0) {
        sceKermit_driver_36666181(kbuf->pkt, 8, 0xc6e, 0, 0, &resp);
        pspcmFreeBufSlot(slot);
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

static int kermitPspcmDevctl(const char *dev, u32 cmd, const void *indata, u32 inlen, void *outdata, u32 outlen)
{
    ScePspcmKermitBuf *kbuf;
    u64 resp;

    int slot = pspcmAllocBufSlot(&kbuf);
    if (slot < 0)
        return slot;

    char *dev_buf = kbuf->payload.devctl.dev;
    void *in_buf = kbuf->payload.devctl.inData;
    void *out_buf = kbuf->payload.devctl.outData;

    strncpy(dev_buf, dev, 0x800);
    dev_buf[0x7ff] = '\0';
    if (indata && inlen > 0)
        sceKernelMemcpy(in_buf, indata, inlen);
    sceKernelMemset(out_buf, 0, outlen);

    kbuf->pkt[0] = 0xc60;
    u32 dev_len = strlen(dev_buf) + 1;
    sceKermitMemory_driver_AAF047AC(kbuf->pkt, 0, dev_buf, dev_len, 1);
    kbuf->pkt[6] = cmd;
    sceKermitMemory_driver_AAF047AC(kbuf->pkt, 2, in_buf, inlen, 1);
    kbuf->pkt[10] = inlen;
    sceKermitMemory_driver_AAF047AC(kbuf->pkt, 4, out_buf, outlen, 2);
    kbuf->pkt[14] = outlen;

    int ret = sceKermit_driver_36666181(kbuf->pkt, 8, 0xc60, 6, 0, &resp);
    if (ret == 0) {
        ret = (int)resp;
        if (outdata && outlen > 0)
            sceKernelMemcpy(outdata, out_buf, outlen);
    }
    pspcmFreeBufSlot(slot);
    return ret;
}

static int kermitPspcmOpen(ScePspcmIob *iob, const char *file, int flags, int mode)
{
    ScePspcmKermitBuf *kbuf;
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
    int bslot = pspcmAllocBufSlot(&kbuf);
    if (bslot < 0)
        return bslot;

    char *path_buf = kbuf->payload.open.path;
    snprintf(path_buf, 0x2000, "%s%d", file, iob->fs_num);
    path_buf[0x1fff] = '\0';
    u32 path_len = strlen(path_buf) + 1;
    sceKermitMemory_driver_AAF047AC(kbuf->pkt, 0, path_buf, path_len, 1);
    kbuf->pkt[6] = flags;
    kbuf->pkt[8] = mode;

    ret = sceKermit_driver_36666181(kbuf->pkt, 8, 0xc67, 3, 0, &resp);
    int kfd = (int)resp;
    if (ret == 0) {
        fd_slot->kermit_fd = kfd;
        pspcmFreeBufSlot(bslot);
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
    pspcmFreeBufSlot(bslot);
    return 0;
}

static int kermitPspcmClose(ScePspcmIob *iob)
{
    ScePspcmKermitBuf *kbuf;
    u64 resp;

    int bslot = pspcmAllocBufSlot(&kbuf);
    if (bslot < 0)
        return bslot;
    if (iob->fs_num >= 8)
        return 0x80010013;

    ScePspcmFdSlot *fd_slot = iob->slot;
    if (fd_slot < &g_fds[0] || fd_slot >= &g_fds[8] || fd_slot->iob != iob)
        return 0x80010009;

    kbuf->pkt[4] = fd_slot->kermit_fd;
    int ret = sceKermit_driver_36666181(kbuf->pkt, 8, 0xc6b, 1, 0, &resp);
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
    pspcmFreeBufSlot(bslot);
    return ret;
}

static int kermitPspcmRead(ScePspcmIob *iob, char *data, int len)
{
    ScePspcmKermitBuf *kbuf;
    u64 resp;

    if (iob->fs_num >= 8)
        return 0x80010013;
    ScePspcmFdSlot *fd_slot = iob->slot;
    if (fd_slot < &g_fds[0] || fd_slot >= &g_fds[8] || fd_slot->iob != iob)
        return 0x80010009;

    int bslot = pspcmAllocBufSlot(&kbuf);
    if (bslot < 0)
        return bslot;

    u8 *bounce = kbuf->payload.bounce;
    int total = 0;
    u32 unaligned = (u32)data & 0x3f;
    u32 rem = (u32)len;

    do {
        if (unaligned != 0) {
            u32 head = 0x40 - unaligned;
            if (rem < head)
                head = rem;
            kbuf->pkt[4] = fd_slot->kermit_fd;
            sceKermitMemory_driver_AAF047AC(kbuf->pkt, 1, bounce, head, 2);
            kbuf->pkt[8] = head;
            int ret = sceKermit_driver_36666181(kbuf->pkt, 8, 0xc5e, 3, 0, &resp);
            total = ret;
            if (ret != 0)
                break;
            int rlen = (int)resp;
            if (rlen <= 0)
                break;
            total = rlen;
            sceKernelMemcpy(data, bounce, rlen);
            if ((u32)rlen != head)
                break;
            rem -= rlen;
        }

        for (;;) {
            u32 tail = rem & 0x3f;
            u32 body = rem - tail;
            char *cur_dst = data + total;
            if (body >= 0x40) {
                sceKernelDcacheInvalidateRangeForUser(cur_dst, body);
                kbuf->pkt[4] = fd_slot->kermit_fd;
                sceKermitMemory_driver_AAF047AC(kbuf->pkt, 1, cur_dst, body, 2);
                kbuf->pkt[8] = body;
                int ret = sceKermit_driver_36666181(kbuf->pkt, 8, 0xc5e, 3, 0, &resp);
                if (ret != 0)
                    break;
                int rlen = (int)resp;
                if (rlen <= 0) {
                    if (rlen < 0)
                        total = rlen;
                    break;
                }
                sceKermitMemory_driver_90B662D0(cur_dst, rlen);
                total += rlen;
                if ((u32)rlen != body)
                    break;
                rem -= rlen;
                continue;
            }
            if (rem != 0) {
                kbuf->pkt[4] = fd_slot->kermit_fd;
                sceKermitMemory_driver_AAF047AC(kbuf->pkt, 1, bounce, rem, 2);
                kbuf->pkt[8] = rem;
                int ret = sceKermit_driver_36666181(kbuf->pkt, 8, 0xc5e, 3, 0, &resp);
                if (ret != 0)
                    break;
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
    } while (0);

    pspcmFreeBufSlot(bslot);
    return total;
}

static int kermitPspcmWrite(ScePspcmIob *iob, const char *data, int len)
{
    ScePspcmKermitBuf *kbuf;
    u64 resp;

    if (iob->fs_num >= 8)
        return 0x80010013;
    ScePspcmFdSlot *fd_slot = iob->slot;
    if (fd_slot < &g_fds[0] || fd_slot >= &g_fds[8] || fd_slot->iob != iob)
        return 0x80010009;

    u32 off = (u32)data & 0x3f;
    u32 wb_size = (off + (u32)len + 0x3f) & ~0x3fu;
    int wb_ret = sceKernelDcacheWritebackRange(data - off, wb_size);
    if (wb_ret != 0)
        return wb_ret;

    int bslot = pspcmAllocBufSlot(&kbuf);
    if (bslot < 0)
        return bslot;

    kbuf->pkt[4] = fd_slot->kermit_fd;
    sceKermitMemory_driver_AAF047AC(kbuf->pkt, 1, (void *)data, len, 1);
    kbuf->pkt[8] = len;
    int ret = sceKermit_driver_36666181(kbuf->pkt, 8, 0xc6a, 3, 0, &resp);
    if (ret == 0)
        ret = (int)resp;
    pspcmFreeBufSlot(bslot);
    return ret;
}

static int kermitPspcmIoctl(ScePspcmIob *iob, u32 cmd, const void *indata, u32 inlen)
{
    ScePspcmKermitBuf *kbuf;
    u64 resp;

    ScePspcmFdSlot *fd_slot = iob->slot;
    if (fd_slot < &g_fds[0] || fd_slot >= &g_fds[8] || fd_slot->iob != iob)
        return 0x80010009;

    int bslot = pspcmAllocBufSlot(&kbuf);
    if (bslot < 0)
        return bslot;

    void *in_buf = kbuf->payload.ioctl.inData;
    void *out_buf = kbuf->payload.ioctl.outData;
    if (indata && inlen > 0)
        sceKernelMemcpy(in_buf, indata, inlen);

    kbuf->pkt[0] = 0xc6f;
    kbuf->pkt[4] = fd_slot->kermit_fd;
    kbuf->pkt[6] = cmd;
    sceKermitMemory_driver_AAF047AC(kbuf->pkt, 2, in_buf, inlen, 1);
    kbuf->pkt[10] = inlen;
    sceKermitMemory_driver_AAF047AC(kbuf->pkt, 4, out_buf, 0, 2);
    kbuf->pkt[14] = 0;

    int ret = sceKermit_driver_36666181(kbuf->pkt, 8, 0xc6f, 6, 0, &resp);
    if (ret == 0)
        ret = (int)resp;
    pspcmFreeBufSlot(bslot);
    return ret;
}

static int pspcmConnIntrHandler(void)
{
    return sceKernelSetEventFlag(g_conn_evid, 1);
}

static int pspcmConnThread(u32 args, void *argp)
{
    (void)args;
    (void)argp;
    for (;;) {
        if (sceKernelWaitEventFlag(g_conn_evid, 1, 0x21, NULL, NULL) == 0)
            pspcmHandleConnEvent();
    }
    return 0;
}

static int pspcmAllocBufSlot(ScePspcmKermitBuf **out_buf)
{
    int ret = sceKernelLockMutex(g_buf_mutex, 1, NULL);
    if (ret != 0)
        return ret;

    int slot;
    for (slot = 0; slot < 0x60; slot++) {
        if (g_buf_used[slot] == 0) {
            void *buf = (void *)(0xABA00000 + (slot << 15));
            g_buf_used[slot] = 1;
            *out_buf = (ScePspcmKermitBuf *)buf;
            sceKernelMemset(buf, 0, 0x8000);
            break;
        }
    }

    ret = sceKernelUnlockMutex(g_buf_mutex, 1);
    if (ret != 0)
        return ret;
    return (slot < 0x60) ? slot : (int)0x80000022;
}

static int pspcmFreeBufSlot(int slot)
{
    if ((u32)slot >= 0x60)
        return 0;
    int ret = sceKernelLockMutex(g_buf_mutex, 1, NULL);
    if (ret != 0)
        return ret;
    g_buf_used[slot] = 0;
    return sceKernelUnlockMutex(g_buf_mutex, 1);
}

static void pspcmHandleConnEvent(void)
{
    u32 info[2];
    if (sceUsb_driver_9F89B84C(info) < 0)
        return;
    if (info[1] & 0x80) {
        if (info[1] & 2)
            g_connected = -1;
        else if (info[1] & 1)
            g_connected = (int)info[1];
    }
    if (pspcmNotifyWaitThread(info) < 0)
        sceKernelExitThread(g_wait_thid);
}
