/*
 * DTCPIP-SW SDI / Wagner hardware abstraction layer:
 *   - SwTtlProbe (TTL check wrapper over sw_net_vita.c)
 *   - SwDrvBase / SwDrv (/dev/swdrv singleton wrapping swOpen/swIoctl/swClose)
 *   - SacSessionBase / SacSessionCXD3705 (SAC channel wrapper)
 *   - SdiBufferBase / SdiBuffer / SdiBufferCXD3705 / SdiBufferFactoryCXD3705
 *   - SdiDevice / SdiDeviceCXD3705 (full Wagner ioctl C++ interface)
 */

#ifndef SW_SDI_H
#define SW_SDI_H

#include "../osal/sw_osal_cpp.h"

/* ---- C Wagner driver exports (sw_wgn_ioctl.c) ---- */
extern "C" {
int swOpen(const char *path);
int swIoctl(int fd, unsigned int cmd, void *arg, unsigned int argSize);
int swClose(int fd);
}

/* ---- Forward declarations for protocol-layer certificate/SRM/callback objects ---- */
struct SwCertObj;
struct SwSrmObj;
struct SwPubKeyObj;
struct SwStreamCallback;
struct SwStreamEventListener;

extern "C" {
int swCertConstruct(void *certObj);
int swCertDestruct(void *certObj);
int swCertSetRaw(void *certObj, const void *data, unsigned int len);
int swCertGetDeviceId(const void *certObj, void *outId5);
int swPubKeySerialize(const void *pubKeyObj, void *out40);
int swSrmSetRaw(void *srmObj, const void *data, unsigned int len, const void *certObj);
int swSrmGetRaw(const void *srmObj, void **outPtr, size_t *outLen);
}

/* ---- Scatter/gather chunk descriptor (8 bytes) ---- */
struct SdiBufferChunk {
	void *ptr;
	uint32_t size;
};

/* ---- Stream transfer descriptor passed to startTxStream / startRxStream ---- */
struct SdiStreamParams {
	uint8_t modeParam[20];		/* +0x00: 20-byte mode/config block */
	uint32_t streamType;		/* +0x14 */
	uint8_t cciMode;		/* +0x18 */
	uint8_t hasTimestamp;		/* +0x19 */
	uint8_t isPcpMode;		/* +0x1A */
	uint8_t reserved1b;		/* +0x1B */
	void *sacSession;		/* +0x1C */
	uint8_t extKeyA[16];		/* +0x20 */
	uint8_t extKeyB[16];		/* +0x30 */
	uint32_t extKeyMode;		/* +0x40 */
	uint8_t cciInfo[48];		/* +0x44 (+0x46 = cciType, +0x47 = cciVal, +0x64 = rxCci) */
	uint8_t extFlag;		/* +0x74 */
	uint8_t reserved75[3];		/* +0x75 */
	uint32_t chunkMaxSize;		/* +0x78 */
	uint32_t chunkAlign;		/* +0x7C */
	uint32_t chunkOffset;		/* +0x80 */
	void *auxBuffer;		/* +0x84 */
	void *mainBuffer;		/* +0x88 */
	void *customMem;		/* +0x8C */
	uint32_t customMemSize;		/* +0x90 */
	uint32_t transferUnit;		/* +0x94 */
	uint8_t optionData[1024];	/* +0x98 */
};

/* ---- SwTtlProbe ---- */
class SwTtlProbe {
public:
	SwTtlProbe() : m_handle(0) {}
	virtual ~SwTtlProbe() { close(); }

	bool openSink(int sock);
	int openSource(int sock);
	bool openSourceFromSocket(int sock, const SwSocket &refSock);
	int getPeerTtl();
	int close();

private:
	int m_handle;
};

/* ---- SwDrvBase / SwDrv ---- */
class SwDrvBase {
public:
	SwDrvBase();
	virtual ~SwDrvBase();

	virtual bool open() = 0;
	virtual int close() = 0;
	virtual int reset() = 0;
	virtual int setParamA(void *buf, unsigned int size) = 0;
	virtual int setParamB(void *buf, unsigned int size) = 0;
	virtual int getParamB(void *buf, unsigned int size) = 0;
	virtual int getInfo(uint8_t kind, void *outBuf, uint32_t *outSize) = 0;
	virtual int setDebugOption(void *buf, unsigned int size) = 0;
	virtual int setModeFlag(int flag) = 0;
	virtual int setDeviceConfig(int p0, int p1, int p2, char p3, int p4, int p5,
				    char p6, char p7, char p8, char p9, char p10,
				    char p11, char p12) = 0;
	virtual int setTimeOffset(uint32_t offset) = 0;
	virtual int getSrm(void *outBuf, size_t *outSize) = 0;
	virtual int setSrm(const void *buf, size_t size) = 0;
};

class SwDrv : public SwDrvBase {
public:
	SwDrv();
	virtual ~SwDrv();

	static int getInstance(SwDrv **out);
	static int drvOpen(const char *path);
	static int drvIoctl(int fd, unsigned int cmd, void *arg, unsigned int size);
	static int drvClose(int fd);
	static int isStreamIoctl(unsigned int cmd);

	int fd() const { return m_fd; }
	bool isOpen() const { return m_fd != -1; }

	virtual bool open();
	virtual int close();
	virtual int reset();
	virtual int setParamA(void *buf, unsigned int size);
	virtual int setParamB(void *buf, unsigned int size);
	virtual int getParamB(void *buf, unsigned int size);
	virtual int getInfo(uint8_t kind, void *outBuf, uint32_t *outSize);
	virtual int setDebugOption(void *buf, unsigned int size);
	virtual int setModeFlag(int flag);
	virtual int setDeviceConfig(int p0, int p1, int p2, char p3, int p4, int p5,
				    char p6, char p7, char p8, char p9, char p10,
				    char p11, char p12);
	virtual int setTimeOffset(uint32_t offset);
	virtual int getSrm(void *outBuf, size_t *outSize);
	virtual int setSrm(const void *buf, size_t size);

private:
	int m_fd;
};

/* ---- SacSessionBase / SacSessionCXD3705 ---- */
class SacSessionBase {
public:
	SacSessionBase();
	virtual int getVersion(void *outBuf, size_t size) = 0;
	virtual int exchangeKey(const void *inBuf, unsigned int inSize,
				void *outBuf, size_t outSize) = 0;
	virtual int authenticate(const void *inBuf, unsigned int inSize) = 0;
	virtual bool isAuthenticated() = 0;
	virtual int close() = 0;
	virtual int getState() = 0;
	virtual ~SacSessionBase();
};

class SacSessionCXD3705 : public SacSessionBase {
public:
	SacSessionCXD3705();
	virtual ~SacSessionCXD3705();

	virtual int getVersion(void *outBuf, size_t size);
	virtual int exchangeKey(const void *inBuf, unsigned int inSize,
				void *outBuf, size_t outSize);
	virtual int authenticate(const void *inBuf, unsigned int inSize);
	virtual bool isAuthenticated();
	virtual int close();
	virtual int getState();

	int getChannelId(uint8_t *outId, int outSize) const;

private:
	bool isDriverOpen() const;
	bool hasChannel() const { return m_channelId != 0; }
	static bool isAllZero(const uint8_t *buf, int len);

	int m_state;
	uint8_t m_channelId;
	uint8_t m_reserved[3];
	SwDrv *m_drv;
};

/* ---- SdiBuffer hierarchy (ABI-compatible with SwSdiBuffer in SwMwContent.h) ---- */
class SdiBufferBase {
public:
	SdiBufferBase() {}
	virtual ~SdiBufferBase() {}

	virtual void setEndOfStream() = 0;
	virtual int append(unsigned int size) = 0;
	virtual int consume(unsigned int size) = 0;
	virtual int available() = 0;
	virtual int size() = 0;
	virtual int isEndOfStream() = 0;
	virtual int setName(const SwString &name) = 0;
	virtual int reset() = 0;
	virtual int setSize(unsigned int newSize) = 0;
	virtual int read(void *dst, unsigned int len) = 0;
	virtual int write(const void *src, unsigned int len) = 0;
	virtual int getWriteChunkList(SdiBufferChunk **outList) = 0;
	virtual int getReadChunkList(SdiBufferChunk **outList) = 0;
	virtual int setError(int error) = 0;
	virtual int getError() = 0;
	virtual int setTimeout(int timeoutMs) = 0;
	virtual int isKeyExpiring() = 0;
};

class SdiBuffer : public SdiBufferBase {
public:
	SdiBuffer(void *extMem, uint32_t capacity, const SwString &label);
	virtual ~SdiBuffer();

	virtual void setEndOfStream();
	virtual int append(unsigned int size);
	virtual int consume(unsigned int size);
	virtual int available();
	virtual int size();
	virtual int isEndOfStream();
	virtual int setName(const SwString &name);
	virtual int reset();
	virtual int setSize(unsigned int newSize);
	virtual int read(void *dst, unsigned int len);
	virtual int write(const void *src, unsigned int len);
	virtual int getWriteChunkList(SdiBufferChunk **outList);
	virtual int getReadChunkList(SdiBufferChunk **outList);
	virtual int setError(int error);
	virtual int getError();
	virtual int setTimeout(int timeoutMs);
	virtual int isKeyExpiring();
	virtual int appendUnlocked(unsigned int size);

	int lockBuffer();
	int unlockBuffer();

protected:
	uint32_t availableUnlocked() const;

	uint8_t m_ownsMem;		/* +0x04 */
	uint8_t m_reserved5[3];		/* +0x05 */
	uint8_t *m_mem;			/* +0x08 */
	uint32_t m_capacity;		/* +0x0C */
	SwString m_name;		/* +0x10 (12B) */
	uint32_t m_alignPad;		/* +0x1C */
	SwMutex m_mutex;		/* +0x20 (48B) */
	uint32_t m_writePos;		/* +0x50 */
	uint32_t m_readPos;		/* +0x54 */
	uint8_t m_eof;			/* +0x58 */
	uint8_t m_full;			/* +0x59 */
	uint8_t m_reserved5a[2];	/* +0x5A */
	SdiBufferChunk *m_readVec;	/* +0x5C */
	SdiBufferChunk *m_writeVec;	/* +0x60 */
	int m_error;			/* +0x64 */
};

class SdiDeviceCXD3705;

class SdiBufferCXD3705 : public SdiBuffer {
public:
	SdiBufferCXD3705(void *extMem, uint32_t capacity, const SwString &label);
	SdiBufferCXD3705(const uint32_t *chunkMax, const uint32_t *chunkAlign,
			 const uint32_t *chunkOff, void *extMem, uint32_t capacity,
			 const SwString &label);
	virtual ~SdiBufferCXD3705();

	virtual void setEndOfStream();
	virtual int append(unsigned int size);
	virtual int consume(unsigned int size);
	virtual int available();
	virtual int isEndOfStream();
	virtual int reset();
	virtual int setSize(unsigned int newSize);
	virtual int getWriteChunkList(SdiBufferChunk **outList);
	virtual int getReadChunkList(SdiBufferChunk **outList);
	virtual int setTimeout(int timeoutMs);
	virtual int isKeyExpiring();

	virtual int attachStream(int drvFd, int sessionId, uint8_t streamId,
				 int isReadSide, uint8_t dirFlag, void *streamCb,
				 void *eventCb, SdiStreamParams *params);
	virtual int pollStream(uint32_t outAddrLen[2]);
	virtual void updateStreamPosition();
	virtual int sendLastPacket();
	virtual int stopStream();

	uint8_t *bindMemory(uint32_t *outSize);
	int getNextChunk(void **outPtr);
	int getNextReadChunk(void **outPtr);
	int getLastIv(void *outIv8);
	bool setChunkOffset(uint32_t off);
	bool setChunkMax(uint32_t maxVal);
	bool setChunkAlign(uint32_t alignVal);
	bool isLastPacketSent() const { return m_lastPacketSent != 0; }
	void clearRetryLast() { m_retryLast = 0; }
	bool isStopped() const { return (m_stoppedRx | m_stoppedTx) != 0; }

private:
	void initFields(uint32_t chunkMax, uint32_t chunkAlign, uint32_t chunkOff);
	uint32_t scanPacketHeaders(uint32_t addr, uint32_t len, uint8_t *outPartial);

	SdiDeviceCXD3705 *m_device;	/* +0x68 */
	int m_sessionId;		/* +0x6C */
	int m_drvFd;			/* +0x70 */
	uint8_t m_streamId;		/* +0x74 */
	uint8_t m_reserved75[3];	/* +0x75 */
	int m_isReadSide;		/* +0x78 */
	uint8_t m_dirFlag;		/* +0x7C */
	uint8_t m_bound;		/* +0x7D */
	uint8_t m_reserved7e[2];	/* +0x7E */
	SwPtrList<SdiBufferChunk> m_chunks; /* +0x80 (8B) */
	uint8_t m_streamDone;		/* +0x88 */
	uint8_t m_lastPacketSent;	/* +0x89 */
	uint8_t m_stoppedRx;		/* +0x8A */
	uint8_t m_stoppedTx;		/* +0x8B */
	uint32_t m_lastEmi;		/* +0x8C */
	void *m_streamCb;		/* +0x90 */
	void *m_eventCb;		/* +0x94 */
	void *m_lastIv;			/* +0x98 */
	int m_timeoutMs;		/* +0x9C */
	uint32_t m_pendingHeaderBytes;	/* +0xA0 */
	uint32_t m_chunkMax;		/* +0xA4 */
	uint32_t m_chunkAlign;		/* +0xA8 */
	int32_t m_chunkOffset;		/* +0xAC */
	uint8_t m_hasChunkOffset;	/* +0xB0 */
	uint8_t m_reservedb1[3];	/* +0xB1 */
	uint32_t m_reservedb4;		/* +0xB4 */
	uint32_t m_reservedb8;		/* +0xB8 */
	uint32_t m_scanCursor;		/* +0xBC */
	uint8_t m_hdrAccum[4];		/* +0xC0 */
	uint8_t m_hdrAccumLen;		/* +0xC4 */
	uint8_t m_retryLast;		/* +0xC5 */
	uint8_t m_reservedc6[2];	/* +0xC6 */
	uint32_t m_lastCci;		/* +0xC8 */
	uint32_t m_lastCopyMode;	/* +0xCC */
	uint8_t m_lastRetention;	/* +0xD0 */
	uint8_t m_reservedd1[3];	/* +0xD1 */
	int m_lastStatus;		/* +0xD4 */
	int m_lastErrorFlags;		/* +0xD8 */
	uint8_t m_lastCciInfo[48];	/* +0xDC */
	SdiStreamParams *m_params;	/* +0x10C */
};

class SdiBufferFactoryBase {
public:
	SdiBufferFactoryBase() {}
	virtual ~SdiBufferFactoryBase() {}
	virtual SdiBuffer *create(void *extMem, uint32_t capacity, const SwString &label) = 0;
};

class SdiBufferFactoryCXD3705 : public SdiBufferFactoryBase {
public:
	SdiBufferFactoryCXD3705() {}
	virtual ~SdiBufferFactoryCXD3705() {}

	virtual SdiBuffer *create(void *extMem, uint32_t capacity, const SwString &label);
	SdiBuffer *createWithChunks(const uint32_t *chunkMax, const uint32_t *chunkAlign,
				    const uint32_t *chunkOff, void *extMem,
				    uint32_t capacity, const SwString &label);
};

/* ---- SdiDevice / SdiDeviceCXD3705 ---- */
class SdiDevice {
public:
	SdiDevice();
	virtual ~SdiDevice();

	static int getInstance(SdiDevice **out);

	virtual int init() = 0;
	virtual int openSourceSession(uint32_t *outSessionId, uint8_t *outLabel,
				      unsigned int akeType, uint8_t flags, uint8_t moveFlag) = 0;
	virtual int openSinkSession(uint32_t *outSessionId, const SwNetAddress &addr,
				    unsigned int akeType, uint8_t moveFlag) = 0;
	virtual int closeSession(uint32_t sessionId) = 0;
	virtual int isAkeTypeSupported(uint8_t akeType) = 0;
	virtual int getRandom(void *outBuf, size_t len) = 0;
	virtual int verifySignature(const void *msg, size_t msgLen,
				    const void *sig40, const void *pubKeyObj) = 0;
	virtual int genKeyPair(void *outPub40, void *outPriv20) = 0;
	virtual int signData(void *outSig40, const void *msg, size_t msgLen) = 0;
	virtual int getExchangeKeyLabel(uint32_t *outLabel, uint8_t sessionId) = 0;
	virtual int setRemoteKey(uint8_t sessionId, uint8_t keyType,
				 const void *pub20, const void *sig40) = 0;
	virtual int setExchangeKey(uint8_t sessionId, uint8_t keyType, int unusedArg,
				   const void *kx12, uint8_t label, uint8_t flags) = 0;
	virtual int clearSinkKey(uint8_t sessionId, uint8_t keyType) = 0;
	virtual void deleteSinkKey(uint8_t sessionId, uint8_t keyType) = 0;
	virtual int verifyRemoteCert(uint32_t *outResult, uint8_t sessionId,
				     const void *certObj, uint8_t flags) = 0;
	virtual int computeSharedSecret(uint8_t sessionId, uint8_t keyType,
					const void *pub20, const void *sig40) = 0;
	virtual int getExchangeKey(void *outKx12, uint8_t *outLabel, uint8_t *outFlags,
				   uint8_t sessionId, uint8_t keyType) = 0;
	virtual int commitSourceKey(uint8_t sessionId, uint8_t keyType) = 0;
	virtual void deleteSourceKey(uint8_t sessionId, uint8_t keyType) = 0;
	virtual int checkCertRevoked(uint8_t *outRevoked, const void *certObj) = 0;
	virtual int getSrm(void *srmObj) = 0;
	virtual int setSrm(const void *srmObj) = 0;
	virtual int startTxStream(uint32_t *outStreamId, uint8_t sessionId,
				  SdiBufferCXD3705 *buf, void *streamCb,
				  unsigned int mediaFmt, const void *iv8,
				  int chunkSize, SdiStreamParams *params) = 0;
	virtual int startRxStream(uint32_t *outStreamId, uint8_t sessionId,
				  SdiBufferCXD3705 *buf, void *streamCb,
				  uint32_t *outLifetimeSec, unsigned int mediaFmt,
				  const void *iv8, void *eventCb, int unusedArg,
				  SdiStreamParams *params) = 0;
	virtual int stopTxStream(int sessionId, int streamId, SdiStreamParams *params) = 0;
	virtual int stopRxStream(int sessionId, int streamId, SdiStreamParams *params) = 0;
	virtual int setCci(uint8_t sessionId, int cci) = 0;
	virtual int getDeviceCert(void *certObj) = 0;
	virtual int getCkcInfo(void *outNonce8, bool *outFlag, uint8_t *outA,
			       uint8_t *outB, uint8_t sessionId) = 0;
	virtual int checkCkc(uint8_t *outOk, uint8_t sessionId, int mode) = 0;
	virtual int checkSessionRevoked(uint8_t *outOk, uint8_t sessionId) = 0;
	virtual int computeRttMac(void *outMac20, uint8_t sessionId, uint8_t role,
				  uint16_t nVal, uint8_t mode) = 0;
	virtual int genMoveKey(void *outMac10, void *outNonce8, uint8_t sessionId,
			       const void *inNonce8) = 0;
	virtual int verifyMoveMac(uint8_t *outOk, uint8_t sessionId, const void *nonceA8,
				  const void *nonceB8, const void *mac10) = 0;
	virtual int computeMoveCompleteMac(void *outMac10, uint8_t sessionId,
					   const void *nonceA8, const void *nonceB8,
					   const void *inMac10) = 0;
	virtual int getKeyLifetime(uint8_t sessionId, uint32_t *outSec) = 0;
	virtual int isMediaFormatSupported(unsigned int mediaFmt) = 0;
	virtual int getStreamIv(void *outIv8, int unusedArg, SdiBufferCXD3705 *buf) = 0;
	virtual int setOptionData(size_t len, const void *data) = 0;
	virtual int setMoveMode(int mode) = 0;
	virtual int computeMoveAuth(void *outMac20, uint8_t sessionId, const void *nonce8) = 0;
	virtual int getDeviceId(uint8_t *outValid, uint8_t *outFlag, void *outId5) = 0;
	virtual int getCertExpire(uint32_t *outExpire) = 0;
};

class SdiDeviceCXD3705 : public SdiDevice {
public:
	SdiDeviceCXD3705();
	virtual ~SdiDeviceCXD3705();

	int drvFd() const;
	bool isDrvOpen() const;
	uint32_t getHwCapabilities() const;

	int updateStreamPos(int drvFd, int sessionId, uint8_t streamId,
			    uint32_t *posPtr, uint8_t dirFlag);
	int getStreamStatus(int drvFd, int sessionId, uint8_t streamId,
			    uint32_t addrLen[2], uint8_t outCci48[48],
			    int *outState, int *outFlags, uint8_t dirFlag,
			    void **outIvPtr, int timeoutMs);
	int stopStreamIoctl(int drvFd, int sessionId, uint8_t streamId,
			    SdiStreamParams *params);
	int setLastPacketIoctl(int drvFd, int sessionId, uint8_t streamId);

	virtual int init();
	virtual int openSourceSession(uint32_t *outSessionId, uint8_t *outLabel,
				      unsigned int akeType, uint8_t flags, uint8_t moveFlag);
	virtual int openSinkSession(uint32_t *outSessionId, const SwNetAddress &addr,
				    unsigned int akeType, uint8_t moveFlag);
	virtual int closeSession(uint32_t sessionId);
	virtual int isAkeTypeSupported(uint8_t akeType);
	virtual int getRandom(void *outBuf, size_t len);
	virtual int verifySignature(const void *msg, size_t msgLen,
				    const void *sig40, const void *pubKeyObj);
	virtual int genKeyPair(void *outPub40, void *outPriv20);
	virtual int signData(void *outSig40, const void *msg, size_t msgLen);
	virtual int getExchangeKeyLabel(uint32_t *outLabel, uint8_t sessionId);
	virtual int setRemoteKey(uint8_t sessionId, uint8_t keyType,
				 const void *pub20, const void *sig40);
	virtual int setExchangeKey(uint8_t sessionId, uint8_t keyType, int unusedArg,
				   const void *kx12, uint8_t label, uint8_t flags);
	virtual int clearSinkKey(uint8_t sessionId, uint8_t keyType);
	virtual void deleteSinkKey(uint8_t sessionId, uint8_t keyType);
	virtual int verifyRemoteCert(uint32_t *outResult, uint8_t sessionId,
				     const void *certObj, uint8_t flags);
	virtual int computeSharedSecret(uint8_t sessionId, uint8_t keyType,
					const void *pub20, const void *sig40);
	virtual int getExchangeKey(void *outKx12, uint8_t *outLabel, uint8_t *outFlags,
				   uint8_t sessionId, uint8_t keyType);
	virtual int commitSourceKey(uint8_t sessionId, uint8_t keyType);
	virtual void deleteSourceKey(uint8_t sessionId, uint8_t keyType);
	virtual int checkCertRevoked(uint8_t *outRevoked, const void *certObj);
	virtual int getSrm(void *srmObj);
	virtual int setSrm(const void *srmObj);
	virtual int startTxStream(uint32_t *outStreamId, uint8_t sessionId,
				  SdiBufferCXD3705 *buf, void *streamCb,
				  unsigned int mediaFmt, const void *iv8,
				  int chunkSize, SdiStreamParams *params);
	virtual int startRxStream(uint32_t *outStreamId, uint8_t sessionId,
				  SdiBufferCXD3705 *buf, void *streamCb,
				  uint32_t *outLifetimeSec, unsigned int mediaFmt,
				  const void *iv8, void *eventCb, int unusedArg,
				  SdiStreamParams *params);
	virtual int stopTxStream(int sessionId, int streamId, SdiStreamParams *params);
	virtual int stopRxStream(int sessionId, int streamId, SdiStreamParams *params);
	virtual int setCci(uint8_t sessionId, int cci);
	virtual int getDeviceCert(void *certObj);
	virtual int getCkcInfo(void *outNonce8, bool *outFlag, uint8_t *outA,
			       uint8_t *outB, uint8_t sessionId);
	virtual int checkCkc(uint8_t *outOk, uint8_t sessionId, int mode);
	virtual int checkSessionRevoked(uint8_t *outOk, uint8_t sessionId);
	virtual int computeRttMac(void *outMac20, uint8_t sessionId, uint8_t role,
				  uint16_t nVal, uint8_t mode);
	virtual int genMoveKey(void *outMac10, void *outNonce8, uint8_t sessionId,
			       const void *inNonce8);
	virtual int verifyMoveMac(uint8_t *outOk, uint8_t sessionId, const void *nonceA8,
				  const void *nonceB8, const void *mac10);
	virtual int computeMoveCompleteMac(void *outMac10, uint8_t sessionId,
					   const void *nonceA8, const void *nonceB8,
					   const void *inMac10);
	virtual int getKeyLifetime(uint8_t sessionId, uint32_t *outSec);
	virtual int isMediaFormatSupported(unsigned int mediaFmt);
	virtual int getStreamIv(void *outIv8, int unusedArg, SdiBufferCXD3705 *buf);
	virtual int setOptionData(size_t len, const void *data);
	virtual int setMoveMode(int mode);
	virtual int computeMoveAuth(void *outMac20, uint8_t sessionId, const void *nonce8);
	virtual int getDeviceId(uint8_t *outValid, uint8_t *outFlag, void *outId5);
	virtual int getCertExpire(uint32_t *outExpire);

private:
	SwPtrList<uint32_t> m_sourceSessions;
	SwPtrList<uint32_t> m_sinkSessions;
	SwDrv *m_drv;
};

/* ---- Global buffer factory singleton & C bridges for DTCPIP-MW ---- */
extern SdiBufferFactoryCXD3705 g_sdiBufferFactory;
extern uint32_t g_sdiTransferUnit;

extern "C" {
int SwMwSacChannelOpen(SacSessionBase **outSession);
int SwMwSacChannelRelease(SacSessionBase **sessionPtr);
int SwMwSessionInit(SwDrv **outDrv);
void swSetBufferFactory(SdiBufferFactoryBase *factory);
}

#endif /* SW_SDI_H */
