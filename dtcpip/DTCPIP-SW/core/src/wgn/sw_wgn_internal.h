/*
 * sw_wgn_internal.h — internal structures and declarations for the DTCPIP-SW
 * Wagner software device driver, command processor, DMA/WSGL bridge, and key pool.
 */
#ifndef DTCPIP_SW_WGN_INTERNAL_H
#define DTCPIP_SW_WGN_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SW_MAX_STREAMS 1
#define SW_MAX_SRC_CDEVS 36
#define SW_MAX_SNK_CDEVS 34
#define SW_MAX_CDEVS (SW_MAX_SRC_CDEVS + SW_MAX_SNK_CDEVS)
#define SW_MAX_SINKS 34
#define SW_MAX_SRM_SIZE 2048
#define SW_PAGE_CHUNK_SIZE 0x40000u

/* Wagner driver debug log mask (default 7 = err|warn|info) */
extern uint32_t g_swWgnLogMask;

struct SwWgnInfo;
struct SwInfo;
struct SwWgnCtx;

/* generic intrusive doubly-linked list node */
struct SwListNode {
	struct SwListNode *next;
	struct SwListNode *prev;
};

/* lightweight condition-variable event (56 bytes) */
struct SwWgnEvent {
	SceKernelLwCondWork cond;
	SceKernelLwMutexWork mutex;
	int signaled;
	int tailAlign;
};

/* lightweight worker thread handle (12 bytes) */
struct SwWgnThread {
	int (*entry)(void *arg);
	SceUID threadId;
	void *arg;
};

/* periodic software timer (92 bytes, embedded at SwWgnCtx + 192) */
struct SwWgnTimer {
	int timerId;
	int periodMs;
	void (*callback)(void *arg);
	void *arg;
	uint32_t ticks;
	int stopRequested;
	struct SwWgnEvent event;
	struct SwWgnThread thread;
};

/* per-DMA scatter-gather descriptor (24 bytes) */
struct SwDmaSgl {
	uint32_t offset;
	uint32_t length;
	void *vaddr;
	uintptr_t paddr;
	uint32_t xferLen;
	uint32_t flags;
};

/* DMA completion notification node (16 bytes) */
struct SwDmaDoneInfo {
	uintptr_t addr;
	uint32_t length;
	int pos;
	struct SwDmaDoneInfo *next;
};

/* page scatter-gather entry (20 bytes) */
struct SwScatterEntry {
	uintptr_t page;
	uint32_t offset;
	uint32_t length;
	uintptr_t dmaAddr;
	uint32_t dmaLen;
};

/* user buffer page map (20 bytes) */
struct SwDmaPageMap {
	int numPages;
	uint32_t firstOffset;
	uint32_t totalLen;
	int direction;
	uintptr_t *pages;
};

/* DMA buffer mapping state (36 bytes) */
struct SwDmaMapInfo {
	uint32_t bufSize;
	struct SwDmaPageMap pageMap;
	struct SwScatterEntry *sgList;
	int mappedCount;
	int sgCount;
};

/* single DMA port (TX or RX, 248 bytes = 0xF8) */
struct SwDmaPort {
	int nWsgl;
	struct SwDmaSgl *wsgl;
	int curPos;
	int endPos;
	int pcpStartPos;
	uint32_t recvLenLo;
	uint32_t recvLenHi;
	uint32_t *recvLenPtr;
	struct SwDmaMapInfo map;
	void *userBase;
	int direction;
	uint32_t syncLen;
	uint32_t unitLen;
	uint32_t unitHdrLen;
	uint32_t unitAccum;
	uint32_t lastLen;
	int syncCalled;
	int lastSyncPos;
	SceKernelLwMutexWork opMutex;
	int waiting;
	int eventAlign;
	struct SwWgnEvent event;
	struct SwDmaDoneInfo *doneHead;
	struct SwDmaDoneInfo *doneTail;
	struct SwDmaDoneInfo *doneFree;
	uint32_t stateFlags;
	SceKernelLwMutexWork dmaMutex;
};

/* stream DMA state (520 bytes = 0x208) */
struct SwStreamState {
	int txDmapNo;
	int rxDmapNo;
	struct SwDmaPort port[2];
	uint32_t route;
	int lastCount;
	int nextSyncPort;
	uint32_t flags;
};

/* per-direction DMA transfer statistics (40 bytes = 0x28) */
struct SwDmaStat {
	uint32_t state;
	uint32_t alignWord;
	uint64_t totalBytes;
	uint32_t spareWord;
	uint32_t startCount;
	uint32_t spareWord2;
	unsigned char clChgCount;
	unsigned char lastDoneCount;
	unsigned char countAlign[2];
	unsigned char setLastCount;
	unsigned char tailAlign[7];
};

/* device-wide memory and transfer statistics (128 bytes = 0x80) */
struct SwWgnStats {
	SceKernelLwMutexWork memMutex;
	int allocBytes;
	uint32_t route[SW_MAX_STREAMS];
	unsigned char active[SW_MAX_STREAMS];
	unsigned char activeAlign[7];
	struct SwDmaStat rxStat[SW_MAX_STREAMS];
	struct SwDmaStat txStat[SW_MAX_STREAMS];
};

/* top-level Wagner device info (984 bytes = 0x3D8) */
struct SwWgnInfo {
	unsigned char devIndex;
	unsigned char headerAlign[7];
	struct SwStreamState stream[SW_MAX_STREAMS];
	SceKernelLwMutexWork cmdMutex;
	int cdevRefCnt[SW_MAX_CDEVS];
	struct SwWgnStats stats;
	struct SwInfo *swInfo;
	struct SwWgnCtx *wgnCtx;
	unsigned char tsifMode[SW_MAX_STREAMS];
	unsigned char tailAlign[7];
};

/* cdev ownership node linked from SwFilePrivate (16 bytes) */
struct SwCdevRefNode {
	struct SwListNode link;
	uint32_t cdevId;
	int refCount;
};

/* per-open file descriptor private state (20 bytes) */
struct SwFilePrivate {
	struct SwWgnInfo *wgnInfo;
	unsigned char started[SW_MAX_STREAMS];
	unsigned char active[SW_MAX_STREAMS];
	unsigned char flagAlign[2];
	struct SwListNode cdevList;
	int closing;
};

/* TX scatter item queued to sw dep core (12 bytes) */
struct SwTxItem {
	uintptr_t addr;
	uint32_t length;
	uint32_t flags;
};

/* PCP header assembly state for sink decryption streams (32 bytes) */
struct SwPcpParser {
	int inPayload;
	uint32_t curOffset;
	uint32_t hdrPos;
	uint32_t payloadRemain;
	unsigned char hdr[14];
	unsigned char hdrAlign[2];
};

/* per-stream bridge state inside SwInfo (192 bytes = 0xC0) */
struct SwDepStreamCtx {
	SceKernelLwMutexWork txMutex;
	struct SwDmaPort *txPort;
	int txEmpty;
	int txWp;
	int txPp;
	int txRp;
	int txLast;
	struct SwPcpParser pcp;
	struct SwTxItem *txItems;
	int txW;
	int txR;
	int txP;
	struct SwDmaPort *rxPort;
	int rxRp;
	int rxSpareA;
	int rxSpareB;
	uint32_t rxSyncLen;
	int rxFull;
	uint32_t route;
	SceKernelLwMutexWork statMutex;
	unsigned char cci[8];
	uint32_t statusBits;
	int setLast;
	int stopTrans;
	uint32_t formatMode;
};

/* SwInfo binding depCore, wgnInfo, and wgnCtx (208 bytes = 0xD0) */
struct SwInfo {
	void *depCore;
	struct SwWgnInfo *wgnInfo;
	struct SwDepStreamCtx stream[SW_MAX_STREAMS];
	struct SwWgnCtx *wgnCtx;
	uint32_t tailAlign;
};

/* single Nc + 6 E-EMI (Kc, IV) key slot (204 bytes = 0xCC) */
struct SwKeySlot {
	uint32_t flags;
	unsigned char nc[8];
	unsigned char keyIv[6][32];
};

/* per-stream content-key pool (664 bytes = 0x298) */
struct SwKeyPool {
	SceKernelLwMutexWork mutex;
	unsigned char kx[12];
	unsigned char lastEemi;
	unsigned char streamId;
	unsigned char front;
	unsigned char rear;
	struct SwKeySlot slot[3];
};

/* shared-memory content device entry (28 bytes = 0x1C) */
struct SwShmCdev {
	int streamRef;
	int expired;
	int alarm;
	unsigned char kx[12];
	unsigned char lifetime;
	unsigned char kxLabel;
	unsigned char cognizant;
	unsigned char cipherAlgo;
};

/* shared-memory active stream entry (16 bytes = 0x10) */
struct SwShmStream {
	int inUse;
	unsigned char cdevId;
	unsigned char padBytes[3];
	unsigned char nc[8];
};

/* shared-memory Wagner state table (4076 bytes = 0xFEC) */
struct SwShmTable {
	int refCount;
	struct SwShmCdev cdev[SW_MAX_CDEVS];
	struct SwShmStream stream[SW_MAX_STREAMS];
	int paramState;
	int commonKeyExpired;
	uint16_t ownerId;
	uint16_t ownerAlign;
	unsigned char prngSeed[20];
	unsigned char srmAlign[12];
	uint32_t srmSize;
	unsigned char srm[SW_MAX_SRM_SIZE];
};

/* non-volatile parameter table in memory (288 bytes = 0x120) */
struct SwNvpTable {
	unsigned char keyId[16];
	uint16_t version;
	unsigned char meta[12];
	uint16_t metaAlign;
	uint16_t keyPeriod;
	unsigned char keyPeriodAlign[14];
	unsigned char privKeyEnc[32];
	unsigned char commonKeyFlags;
	unsigned char flagsAlign[3];
	unsigned char startTime[8];
	uint32_t certLen;
	unsigned char cert[132];
	unsigned char idu[5];
	unsigned char iduAlign[3];
	unsigned char dtlaPubKey[48];
	unsigned char sinkLimit;
	unsigned char kxLabel;
	unsigned char tailAlign[2];
};

/* sink content device entry (76 bytes = 0x4C) */
struct SwSnkCdev {
	struct SwListNode link;
	unsigned char cdevId;
	unsigned char idAlign[3];
	int isMove;
	struct SwShmCdev *shm;
	int alarmTime;
	int akeState;
	unsigned char akeAlign[2];
	unsigned char akeId;
	unsigned char kxLabel;
	unsigned char ksx[12];
	unsigned char kauth[12];
	unsigned char authLocId[20];
};

/* source content device entry (24 bytes = 0x18) */
struct SwSrcCdev {
	struct SwListNode link;
	unsigned char cdevId;
	unsigned char idAlign[3];
	int isMove;
	struct SwShmCdev *shm;
	unsigned char sinkCount;
	unsigned char cntAlign[3];
};

/* authenticated sink tracked by source AKE (28 bytes = 0x1C) */
struct SwSrcSinkEntry {
	struct SwListNode link;
	unsigned char deviceId[5];
	unsigned char isAp;
	unsigned char akeId;
	unsigned char state;
	unsigned char kauth[12];
};

/* per-stream command table entry inside SwWgnCtx (40 bytes = 0x28) */
struct SwWgnStreamTbl {
	int active;
	uint32_t route;
	unsigned char cdevId;
	unsigned char kxLabel;
	unsigned char cognizant;
	unsigned char cipherAlgo;
	struct SwKeyPool *keyPool;
	int expired;
	int ncDirty;
	uint32_t pcpCl;
	int ncInitMode;
	unsigned char initNc[8];
};

/* Wagner command processor context (5112 bytes = 0x13F8) */
struct SwWgnCtx {
	uint32_t magic;
	struct SwInfo *swInfo;
	SceKernelLwMutexWork semCdev;
	SceKernelLwMutexWork semStrm;
	SceKernelLwMutexWork semSrm;
	uint32_t shmTblHandle;
	struct SwShmTable *shmTbl;
	uint32_t shmNvpHandle;
	struct SwNvpTable *shmNvp;
	SceKernelLwMutexWork randMutex;
	unsigned char prngXkey[20];
	unsigned char prngTail[12];
	int isPrimary;
	int timerAlign;
	struct SwWgnTimer timer;
	unsigned char snkAkeSeq;
	unsigned char snkAlign[7];
	SceKernelLwMutexWork snkMutex;
	struct SwListNode snkUsedList;
	struct SwListNode snkFreeList;
	struct SwSnkCdev snkCdev[SW_MAX_SNK_CDEVS];
	SceKernelLwMutexWork srcMutex;
	struct SwListNode srcUsedList;
	struct SwListNode srcFreeList;
	struct SwSrcCdev srcCdev[SW_MAX_SRC_CDEVS];
	struct SwListNode srcSinkUsedList;
	struct SwListNode srcSinkFreeList;
	struct SwSrcSinkEntry srcSinks[SW_MAX_SINKS];
	unsigned char kxmLabels[256];
	uint32_t kxmNextLabel;
	int srcActiveCount;
	struct SwWgnStreamTbl strmTbl[SW_MAX_STREAMS];
};

/* DTCP 160-bit ECC curve table (148 bytes) */
struct SwEccCurveTable {
	uint32_t header;
	unsigned char prime[24];
	unsigned char coeffA[24];
	unsigned char coeffB[24];
	unsigned char order[24];
	unsigned char genX[24];
	unsigned char genY[24];
};

/* external logging / Vita dep / middleware hooks */
int swMwTrace(const char *fmt, ...);
int SwsGetToolMode(void);
int SwsGetAffinity(void);
int SwMwThreadTrampoline(SceSize args, void *argp);
uint64_t SwMwKeyBinGetDeviceId(void);
int64_t SwMwKeyBinGetSeed(void);
int SwMwReadKeyBinExtra(const void *keyId, int keyIdLen, void *outKey, int outKeyLen);

int swDepInitLib(void **outDep, void **outCore);
void swDepFiniLib(void *depCore);
int64_t swDepGetSecureTimeSec(void);
uint64_t swDepElapsedSec(int64_t nowSec, int64_t baseSec);
int swDepGetDeviceIdu(void *ctx, unsigned char *outIdu);
int swDepAes128Ecb(const unsigned char *src, unsigned int srcLen, unsigned char *dst,
		   unsigned int dstLen, const void *key, int decrypt);
int swDepEccEcdsaVerify(const void *message, unsigned int messageLen,
			const unsigned char *sigBytes, const unsigned char *pubKeyBytes);
int swDepEccEcdsaSign(const void *message, unsigned int messageLen,
		      const void *privKeyBytes, const void *nonceBytes,
		      unsigned char *outSig);
int swDepEccEcdhPhase1(void *outPrivKey, unsigned char *outPubPoint, const void *randomBytes);
int swDepEccEcdhPhase2(void *outSharedX, const void *privKeyBytes,
		       const unsigned char *peerPubBytes);
int swDepCryptoNoop(void *unused);
void swDepGetRandom(unsigned char *xkey, unsigned char *outRandom);
int swDepSha1Digest(const void *input, unsigned int size, unsigned char *digest);
int swDepHmacSha1(const void *input, unsigned int inputLen, const void *key,
		  unsigned int keyLen, unsigned char *outMac, size_t outLen);

int swDepCoreReset(void *core);
int swDepCoreConfigureStream(void *core, unsigned char streamId, const void *config);
int swDepCoreStartStream(void *core, unsigned char streamId);
int swDepCoreStopStreamGraceful(void *core, unsigned char streamId);
int swDepCoreStopStreamForce(void *core, unsigned char streamId);
int swDepCorePushHdr(void *core, unsigned char streamId, const void *keyParam,
		     const void *pcpHdr);
unsigned int swDepCorePushInput(void *core, unsigned char streamId, const void *items,
				unsigned int count);
int swDepCoreKickRx(void *core, unsigned char streamId);
int swDepCoreSetCci(void *core, unsigned char streamId, unsigned char eemi);

/* sw_wgn_ioctl.c */
int swCheckFd(int fd);
int swIoctl(int fd, uint32_t cmd, void *arg, uint32_t argLen);
int swOpen(const char *devName);
int swClose(int fd);
void swListRemoveNode(struct SwListNode *node);
void swListAddTail(struct SwListNode *node, struct SwListNode *head);
int swResetWagner(struct SwWgnInfo *info, struct SwFilePrivate *priv);
int swDispatchIoctl(struct SwFilePrivate *priv, uint32_t cmd, unsigned char *arg, uint32_t argLen);
int swExecCommand(struct SwWgnInfo *info, uint32_t cmd, void *arg, uint32_t argLen, int *outStatus);
int swStreamDoneCheck(struct SwWgnInfo *info, unsigned char streamId);
void swFreeStreamResource(struct SwWgnInfo *info, int streamId);
void swMarkPortDone(struct SwWgnInfo *info, unsigned char streamId, int portIdx,
		    struct SwFilePrivate *priv);
struct SwDmaDoneInfo *swAllocDoneInfo(struct SwWgnInfo *info, struct SwDmaPort *port, int unused);
void swRecycleDoneInfo(struct SwDmaPort *port, struct SwDmaDoneInfo *node);
void swFreeDonePool(struct SwWgnInfo *info, struct SwDmaPort *port);
int swTxCompCallback(struct SwWgnInfo *info, unsigned char streamId);
int swTxErrCallback(struct SwWgnInfo *info, unsigned char streamId);
int swRxCompCallback(struct SwWgnInfo *info, unsigned char streamId, uint32_t contentLen);
int swRxErrCallback(struct SwWgnInfo *info, unsigned char streamId);
void *swAllocMem(struct SwWgnInfo *info, size_t size, int unused);
void swFreeMem(struct SwWgnInfo *info, void *ptr, int size);
int swInitWgnInfo(struct SwWgnInfo *info);
void swFiniWgnInfo(struct SwWgnInfo *info);
void swForceStopStream(struct SwWgnInfo *info, int streamId, struct SwFilePrivate *priv);

/* sw_wgn_core.c */
struct SwWgnCtx *swWgnInit(struct SwInfo *swInfo);
void swWgnExit(struct SwWgnCtx *ctx);
int swWgnReset(struct SwWgnCtx *ctx);
int swWgnCommand(struct SwWgnCtx *ctx, uint32_t cmd, void *arg, uint32_t argLen);
int swWgnIsCommonKeyExpired(struct SwWgnCtx *ctx);
void swWgnGenRandom20(struct SwWgnCtx *ctx, unsigned char *out20);
struct SwSnkCdev *swSnkGetCdev(struct SwWgnCtx *ctx, int cdevId);
void swSnkFreeCdev(struct SwWgnCtx *ctx, struct SwSnkCdev *cdev);
void swSnkExpireKx(struct SwSnkCdev *cdev);
void swSnkPcpDetect(struct SwWgnCtx *ctx, int streamId, const unsigned char *pcpHdr,
		    uint32_t contentLen);
struct SwSrcSinkEntry *swSrcGetSink(struct SwWgnCtx *ctx, unsigned char akeId);
struct SwSrcCdev *swSrcGetCdev(struct SwWgnCtx *ctx, int cdevId);
void swSrcFreeCdev(struct SwWgnCtx *ctx, struct SwSrcCdev *cdev);
void swSrcExpireKx(struct SwWgnCtx *ctx, struct SwSrcCdev *cdev);
void swSrcExpireKxm(struct SwWgnCtx *ctx, struct SwSrcCdev *cdev);
void swWgnGetEncParam(struct SwWgnCtx *ctx, unsigned char streamId, unsigned char eemi,
		      int changeNc, void *outKey, void *outIv, void *outNc,
		      unsigned char *outKxLabel, uint32_t *outPcpCl);
void swWgnCheckKxAlarm(struct SwWgnCtx *ctx, unsigned char streamId);
void swWgnDumpHex(const char *prefix, const unsigned char *data, uint32_t len);
void swWgnAddBigEndian(const unsigned char *a, int aLen, const unsigned char *b,
		       int bLen, unsigned char *out);
void swBaseExit(struct SwInfo *swInfo);
int swBaseInit(void *depCore, struct SwInfo **outInfo);
int swDepCoreResetWrap(struct SwInfo *swInfo);
const struct SwEccCurveTable *swGetEccCurveTable(void);
void swGetCciInfo(struct SwInfo *swInfo, int streamId, unsigned char *outCci);

/* sw_wgn_dma.c */
uint32_t swPollStreamStatus(struct SwInfo *swInfo, unsigned char streamId, int clearAll);
int swSetStreamStatusWrap(struct SwInfo *swInfo, unsigned char streamId, uint32_t bits);
int swSetTxWp(struct SwInfo *swInfo, unsigned char streamId, int wp);
int swSetRxRp(struct SwInfo *swInfo, unsigned char streamId, int rp);
int swKickDmaPort(struct SwWgnInfo *info, struct SwDmaPort *port, unsigned char streamId);
void swClearDepStream(struct SwInfo *swInfo, unsigned char streamId);
int swDepPrepareTrans(struct SwInfo *swInfo, const unsigned char *param, int tsifMode,
		      int streamId);
int swSetLastWrap(struct SwInfo *swInfo, unsigned char streamId);
int swStopStreamWrap(struct SwInfo *swInfo, unsigned char streamId);
int swFreeStreamWrap(struct SwInfo *swInfo, unsigned char streamId);
void swStopCompCallback(struct SwInfo *swInfo, unsigned char streamId);
void swSetCciBasic(struct SwInfo *swInfo, unsigned char streamId, unsigned char cci,
		   unsigned char eemi);
void swSetCciExt(struct SwInfo *swInfo, unsigned char streamId, const unsigned char *ext,
		 unsigned char cci, unsigned char eemi);
void swGetEncParamWrap(struct SwInfo *swInfo, int streamId, int eemi, int changeNc,
		       uint32_t *outEncParam, unsigned char *outPcpHdr);
int swTimerInit(struct SwWgnTimer *timer, int timerId);
int swTimerStart(struct SwWgnTimer *timer, int periodMs, void (*cb)(void *), void *arg);
void swTimerStop(struct SwWgnTimer *timer);
void swTxQueuePush(struct SwDepStreamCtx *st, uintptr_t addr, uint32_t len, uint32_t flags);
void swTxQueueFlush(struct SwInfo *swInfo, unsigned char streamId);
void swNotifyTxRp(struct SwInfo *swInfo, unsigned char streamId, int count);
void swTxCompEvent(struct SwInfo *swInfo, unsigned char streamId, int count);
void swTxAvailEvent(struct SwInfo *swInfo, unsigned char streamId);
int swTxErrEvent(struct SwInfo *swInfo, unsigned char streamId);
void swResetDmaPort(struct SwWgnInfo *info, struct SwDmaPort *port);
void swUnmapDmaPort(struct SwWgnInfo *info, struct SwDmaPort *port);
void swMarkDmaDone(struct SwWgnInfo *info, int streamId, int portIdx);
void swPostDoneDma(struct SwWgnInfo *info, int streamId, int portIdx, struct SwFilePrivate *priv);
int swSetupAllDmacs(struct SwWgnInfo *info, struct SwFilePrivate *priv, unsigned char streamId,
		    uint32_t route, const uint32_t *param);

/* sw_wgn_crypto.c */
int swCmdGetCert(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdGetSrm(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdUpdSrm(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdChkRevoked(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdExpKx(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdEcdsaVerify(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdEcdsaSign(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdGenEcdhParm(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdGenRand(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdGetRttHash(struct SwWgnCtx *ctx, unsigned char *arg);
void swComputeCkcMac(const unsigned char *kx, const unsigned char *nc,
		     const unsigned char *nonce, unsigned char *outMac);
int swVerifySrmSignature(const unsigned char *srm, int srmLen, const unsigned char *pubKey);
int swCmdGetMoveHash(struct SwWgnCtx *ctx, unsigned char *arg);
int swCmdDelCdev(struct SwWgnCtx *ctx, int cdevId);
int swGetCdevCognizant(struct SwWgnCtx *ctx, unsigned char cdevId);
int swCryptoSelfTestWrap(void);
const struct SwEccCurveTable *swGetEccCurveTableRaw(void);
int swGetDeviceIduWrap(struct SwWgnCtx *ctx, unsigned char *outIdu);
int swWgnSemGet(struct SwInfo *swInfo, const char *name, SceKernelLwMutexWork *mtx);
void swWgnSemFree(struct SwInfo *swInfo, SceKernelLwMutexWork *mtx);
void *swWgnShmGet(struct SwInfo *swInfo, const char *name, size_t size, int create,
		  uint32_t *outHandle);
void swWgnShmFree(struct SwInfo *swInfo, uint32_t *handle, void *ptr, int size);
void *swWgnAllocTbl(struct SwInfo *swInfo, uint32_t magicId, int size);
void swWgnFreeTbl(struct SwInfo *swInfo, void *ptr, int size);
void swSetStreamStatus(struct SwInfo *swInfo, unsigned char streamId, uint32_t bits);
void swSetStreamEemi(struct SwInfo *swInfo, unsigned char streamId, unsigned char eemi);
void swSendKeyChange(struct SwInfo *swInfo, int streamId, uint32_t contentLen,
		     const void *key, const void *iv, const void *pcpHdr);
void swIncrementNc(const unsigned char *inNc, unsigned char *outNc);
void swKmGetKx(struct SwKeyPool *pool, void *outKx);
void swDeriveContentKeyIv(int eemiIdx, const unsigned char *kx, const unsigned char *nc,
			  unsigned char *outKeyIv);
void swKmPrepareKeys(struct SwKeyPool *pool, uint32_t count, const unsigned char *initNc);
struct SwKeyPool *swKmAllocPool(struct SwWgnCtx *ctx, const unsigned char *kx);
void swKmSetStreamId(struct SwKeyPool *pool, unsigned char streamId);
void swKmFreePool(struct SwWgnCtx *ctx, struct SwKeyPool *pool);
void swKmSched(struct SwWgnCtx *ctx);
void swKmGetCurrentNc(struct SwKeyPool *pool, unsigned char *outNc);
void swKmGetNextNc(struct SwKeyPool *pool, unsigned char *outNc);
void swKmGetCurrentKey(struct SwKeyPool *pool, unsigned char eemi,
		       void **outNc, void **outKey, void **outIv);
void swKmGetNextKey(struct SwKeyPool *pool, unsigned char eemi,
		    void **outNc, void **outKey, void **outIv);
int swKmGetThisKey(struct SwKeyPool *pool, unsigned char eemi, const unsigned char *nc,
		   void **outKey, void **outIv);
void swKmResetKx(struct SwKeyPool *pool, const unsigned char *newKx);
int swDmaMapSg(struct SwScatterEntry *sgList, int count, int dir);
void swDmaUnmapSg(struct SwScatterEntry *sgList, int count, int dir);
void *swDmaAllocZero(struct SwWgnInfo *info, size_t size);
void swDmaFreeZero(struct SwWgnInfo *info, void *ptr, int size);
int swDmaPinUserPages(struct SwWgnInfo *info, struct SwDmaPageMap *map,
		      const uint32_t *iov, int dir);
void swDmaUnpinUserPages(struct SwWgnInfo *info, struct SwDmaPageMap *map);
int swPcpProcessTxSgl(struct SwInfo *swInfo, unsigned char streamId,
		      const struct SwDmaSgl *sgl, uint32_t len, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
