/*
 * SceVoiceQoS: voice chat quality of service on top of libvoice (voice_qos.h). A connection
 * ties a local endpoint (the microphone and speaker ports) to a remote endpoint (an in and an
 * out voice port); it measures the frames that arrive and picks the bit rate of the frames
 * read from the out port. Every call takes the library's mutex.
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <kernel/error.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <voice.h>
#include <voice_qos.h>

void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len) __attribute__((__nonnull__));

#define VOICEQOS_LOCK_RETRIES	10
#define VOICEQOS_LOCK_TIMEOUT	1000000

#define SCE_VOICE_QOS_ERROR_ARGUMENT_INVALID	SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID
#define VOICEQOS_MEMBLOCK_TYPE			0x0C20D060
#define VOICEQOS_MEMBLOCK_SIZE			0x40000
#define VOICEQOS_WINDOW_SIZE			256

/* the frame kinds the high nibble of a frame's first byte selects */
enum {
	VOICEQOS_FRAME_NONE,
	VOICEQOS_FRAME_AUDIO,
	VOICEQOS_FRAME_STATE
};

/* a connection: what has been measured of the frames that arrive and leave */
typedef struct VoiceQoSConnection {
	SceInt32 localId;			/* 0x00 */
	SceInt32 remoteId;			/* 0x04 */
	SceInt32 desiredBitRate;		/* 0x08 */
	union {
		SceInt32 value;
		float rate;
	} outReadBitRate;			/* 0x0C */
	SceInt32 outBitRate;			/* 0x10 */
	SceInt32 outFrameCount;			/* 0x14 */
	float outReadRate;			/* 0x18 */
	SceUInt32 outReadBytes;			/* 0x1C */
	float adaptTime;			/* 0x20 */
	float outReadTime;			/* 0x24 */
	float heartbeatTime;			/* 0x28 */
	float receiveTime;			/* 0x2C */
	float writeTime;			/* 0x30 */
	float lateRate;				/* 0x34 */
	float inRate;				/* 0x38 */
	float receivedRate;			/* 0x3C */
	float receivedRatio;			/* 0x40 */
	SceInt32 lostFrames;			/* 0x44 */
	SceInt32 lateFrames;			/* 0x48 */
	SceInt32 duplicateFrames;		/* 0x4C */
	SceInt32 receivedFrames;		/* 0x50 */
	SceUInt8 expectedSequence;		/* 0x54 */
	SceUInt8 sendSequence;			/* 0x55 */
	SceUInt8 seen[VOICEQOS_WINDOW_SIZE];	/* 0x56 */
} VoiceQoSConnection;

/* a remote endpoint: the ports of voice from and to it */
typedef struct VoiceQoSRemoteEndpoint {
	SceUInt32 inPortId;			/* 0x00 */
	SceUInt32 outPortId;			/* 0x04 */
	SceUInt8 reserved[0x18];		/* 0x08 */
	SceVoicePortType inPortType;		/* 0x20 */
	SceUInt16 inThreshold;			/* 0x24 */
	SceUInt16 inMute;			/* 0x26 */
	float inVolume;				/* 0x28 */
	SceInt32 inBitRate;			/* 0x2C */
	SceUInt8 inReserved[0x10];		/* 0x30 */
	SceVoicePortType outPortType;		/* 0x40 */
	SceUInt16 outThreshold;			/* 0x44 */
	SceUInt16 outMute;			/* 0x46 */
	float outVolume;			/* 0x48 */
	SceInt32 outBitRate;			/* 0x4C */
	SceUInt8 outReserved[0x08];		/* 0x50 */
	SceInt32 state;				/* 0x58 */
	SceUInt8 padding[4];			/* 0x5C */
} VoiceQoSRemoteEndpoint;

/* its last two bytes are padding */
_Static_assert(((sizeof(VoiceQoSConnection) + 3) & ~3u) == 0x158, "VoiceQoSConnection size");
_Static_assert(sizeof(VoiceQoSRemoteEndpoint) == 0x60, "VoiceQoSRemoteEndpoint size");

/* the bit rate a frame kind carries, and the bytes of voice one frame of that kind holds */
static const SceInt32 voiceQoSFrameKind[16] = {
	VOICEQOS_FRAME_AUDIO, VOICEQOS_FRAME_AUDIO, VOICEQOS_FRAME_AUDIO, VOICEQOS_FRAME_AUDIO,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, VOICEQOS_FRAME_STATE
};
static const SceInt32 voiceQoSBitRate[4] = { 4650, 5700, 7300, 0 };
static const SceInt32 voiceQoSFrameBytes[16] = { 20, 24, 15, 19 };

static SceVoiceStartParam voiceQoSStart;
static VoiceQoSConnection voiceQoSConnections[SCE_VOICE_QOS_MAX_CONNECTIONS];
static SceUInt8 voiceQoSRemoteUsed[SCE_VOICE_QOS_MAX_REMOTE_ENDPOINTS] __attribute__((aligned(8)));
static SceUInt8 voiceQoSInitialized;
static SceUInt8 voiceQoSLocalUsed;
static VoiceQoSRemoteEndpoint voiceQoSRemotes[SCE_VOICE_QOS_MAX_REMOTE_ENDPOINTS];
static SceUInt32 voiceQoSLocalInPortId;
static SceUInt32 voiceQoSLocalOutPortId;
static SceUInt8 voiceQoSReadBuffer[0x800];
static SceKernelLwMutexWork voiceQoSMutex;

/* not in the SDK's headers */
SceInt64 sceKernelGetSystemTimeWide(void);

/* the error codes, as voice_qos.h names them */
#define VOICEQOS_NOT_INIT		((SceInt32)SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT)
#define VOICEQOS_ARGUMENT_INVALID	((SceInt32)SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID)
#define VOICEQOS_PACKET_INVALID		((SceInt32)SCE_VOICE_ERROR_LIBVOICEQOS_PACKET_INVALID)
#define VOICEQOS_INTERNAL_UNRECOVERABLE	((SceInt32)SCE_VOICE_ERROR_LIBVOICEQOS_INTERNAL_UNRECOVERABLE)
#define VOICEQOS_INITIALIZED		((SceInt32)SCE_VOICE_ERROR_LIBVOICEQOS_INITIALIZED)
#define VOICEQOS_SYSTEM			((SceInt32)SCE_VOICE_ERROR_LIBVOICEQOS_SYSTEM)
#define VOICEQOS_RESOURCE_USED		((SceInt32)SCE_VOICE_ERROR_LIBVOICEQOS_RESOURCE_USED)

/* libgcc's 64-bit unsigned to float, which the compiler calls for the time below */
static inline float voiceQoSNow(void)
{
	return (float)(SceUInt64)sceKernelGetSystemTimeWide() / 1000000.0f;
}

static inline SceInt32 voiceQoSLock(void)
{
	SceInt32 retries = VOICEQOS_LOCK_RETRIES;
	SceUInt32 timeout = VOICEQOS_LOCK_TIMEOUT;

	while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
		if (--retries == 0)
			return VOICEQOS_SYSTEM;
	}
	return 0;
}

/* newlib's __ieee754_expf, which Sony built into the smoothing below */
static float voiceQoSExp(float x)
{
	static const float one = 1.0f, huge = 1.0e+30f, twom100 = 7.8886090522e-31f;
	static const float oThreshold = 8.8721679688e+01f, uThreshold = -1.0397208405e+02f;
	static const float invLn2 = 1.4426950216e+00f;
	static const float p1 = 1.6666667163e-01f, p2 = -2.7777778450e-03f, p3 = 6.6137559770e-05f,
			   p4 = -1.6533901998e-06f, p5 = 4.1381369442e-08f;
	float half[2] = { 0.5f, -0.5f };
	float ln2Hi[2] = { 6.9313812256e-01f, -6.9313812256e-01f };
	float ln2Lo[2] = { 9.0580006145e-06f, -9.0580006145e-06f };
	float y, hi = 0.0f, lo = 0.0f, c, t;
	SceInt32 k = 0, xsb;
	SceUInt32 hx;
	union {
		float f;
		SceUInt32 i;
	} u;

	u.f = x;
	hx = u.i;
	xsb = (hx >> 31) & 1;
	hx &= 0x7fffffff;
	if (hx >= 0x42b17218) {
		if (hx > 0x7f800000)
			return x + x;
		if (hx == 0x7f800000)
			return xsb == 0 ? x : 0.0f;
		if (x > oThreshold)
			return huge * huge;
		if (x < uThreshold)
			return twom100 * twom100;
	}
	if (hx > 0x3eb17218) {
		if (hx < 0x3f851592) {
			hi = x - ln2Hi[xsb];
			lo = ln2Lo[xsb];
			k = 1 - xsb - xsb;
		} else {
			k = invLn2 * x + half[xsb];
			t = k;
			hi = x - t * ln2Hi[0];
			lo = t * ln2Lo[0];
		}
		x = hi - lo;
	} else if (hx < 0x31800000) {
		if (huge + x > one)
			return one + x;
	} else {
		k = 0;
	}
	t = x * x;
	c = x - t * (p1 + t * (p2 + t * (p3 + t * (p4 + t * p5))));
	if (k == 0)
		return one - ((x * c) / (c - 2.0f) - x);
	y = one - ((lo - (x * c) / (2.0f - c)) - hi);
	if (k >= -125) {
		u.f = y;
		u.i += k << 23;
		return u.f;
	}
	u.f = y;
	u.i += (k + 100) << 23;
	return u.f * twom100;
}

/* an exponential moving average: the old value weighs 2^(-dt / halfLife) */
static int voiceQoSSmooth(float *pValue, float sample, float dt, float halfLife)
{
	float weight = voiceQoSExp((dt * -0.6931472f) / halfLife);
	float old = *pValue;

	if (!(old - 0.000001f < 0.0f && old + 0.000001f > 0.0f))
		sample = sample * (1.0f - weight) + old * weight;
	*pValue = sample;
	return 0;
}

SceVoiceQoSError sceVoiceQoSInit(void)
{
	SceVoiceInitParam initParam __attribute__((aligned(32)));
	SceInt32 result;
	SceInt32 i;

	if (voiceQoSInitialized)
		return VOICEQOS_INITIALIZED;
	if (sceKernelCreateLwMutex(&voiceQoSMutex, "_VoiceQoS001", 0, 0, NULL) != 0)
		return VOICEQOS_SYSTEM;
	voiceQoSLocalUsed = 0;
	__builtin_memset(voiceQoSRemoteUsed, 0, sizeof(voiceQoSRemoteUsed));
	for (i = SCE_VOICE_QOS_MAX_CONNECTIONS; i != 0; i--) {
		VoiceQoSConnection *pConnection = &voiceQoSConnections[SCE_VOICE_QOS_MAX_CONNECTIONS - i];
		float now;

		pConnection->localId = -1;
		pConnection->remoteId = -1;
		pConnection->expectedSequence = 0;
		pConnection->sendSequence = 0;
		pConnection->duplicateFrames = 0;
		memset(pConnection->seen, 0, sizeof(pConnection->seen));
		pConnection->desiredBitRate = SCEVOICE_BITRATE_7300;
		pConnection->outReadBitRate.value = SCEVOICE_BITRATE_7300;
		pConnection->outBitRate = SCEVOICE_BITRATE_7300;
		pConnection->outReadBytes = 0;
		pConnection->outReadRate = 0.0f;
		now = voiceQoSNow();
		pConnection->lostFrames = 0;
		pConnection->lateFrames = 0;
		pConnection->receivedFrames = 0;
		pConnection->lateRate = 0.0f;
		pConnection->inRate = 0.0f;
		pConnection->receivedRate = 1.0f;
		pConnection->receivedRatio = 1.0f;
		pConnection->adaptTime = now;
		pConnection->outReadTime = now;
		pConnection->writeTime = now;
		pConnection->heartbeatTime = now;
		pConnection->receiveTime = now;
	}
	sceClibMemset(&initParam, 0, 0x20);
	initParam.appType = SCEVOICE_APPTYPE_GAME;
	initParam.onEvent = NULL;
	result = sceVoiceInit(&initParam, SCEVOICE_VERSION_100);
	if (result != 0)
		return result;
	voiceQoSStart.container = sceKernelAllocMemBlock("SceUserQoS", VOICEQOS_MEMBLOCK_TYPE,
							 VOICEQOS_MEMBLOCK_SIZE, NULL);
	result = sceVoiceStart(&voiceQoSStart);
	if (result != 0) {
		sceVoiceEnd();
		return result;
	}
	voiceQoSInitialized = 1;
	return 0;
}

SceVoiceQoSError sceVoiceQoSCreateLocalEndpoint(SceVoiceQoSLocalId *pLocalId)
{
	SceVoicePortParam param __attribute__((aligned(32)));
	SceInt32 result;

	if (!voiceQoSInitialized)
		return VOICEQOS_NOT_INIT;
	if (pLocalId == NULL)
		return VOICEQOS_ARGUMENT_INVALID;
	result = voiceQoSLock();
	if (result != 0)
		return result;
	if (voiceQoSLocalUsed) {
		*pLocalId = -1;
		if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
			return VOICEQOS_SYSTEM;
		return VOICEQOS_RESOURCE_USED;
	}
	param.portType = SCEVOICE_PORTTYPE_IN_DEVICE;
	param.bMute = voiceQoSLocalUsed;
	param.threshold = voiceQoSLocalUsed;
	param.device.playerId = voiceQoSLocalUsed;
	param.volume = 1.0f;
	result = sceVoiceCreatePort(&voiceQoSLocalInPortId, &param);
	if (result != 0) {
		if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
			return VOICEQOS_SYSTEM;
		return result;
	}
	param.portType = SCEVOICE_PORTTYPE_OUT_DEVICE;
	result = sceVoiceCreatePort(&voiceQoSLocalOutPortId, &param);
	if (result != 0) {
		sceVoiceDeletePort(voiceQoSLocalInPortId);
		if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
			return VOICEQOS_SYSTEM;
		return result;
	}
	*pLocalId = 0;
	voiceQoSLocalUsed = 1;
	result = sceKernelUnlockLwMutex(&voiceQoSMutex, 1);
	if (result != 0)
		return VOICEQOS_SYSTEM;
	return result;
}

SceVoiceQoSError sceVoiceQoSDeleteLocalEndpoint(SceVoiceQoSLocalId localId)
{
	SceInt32 result;

	if (!voiceQoSInitialized)
		return VOICEQOS_NOT_INIT;
	if (localId != 0)
		return VOICEQOS_ARGUMENT_INVALID;
	result = voiceQoSLock();
	if (result != 0)
		return result;
	if (!voiceQoSLocalUsed) {
		if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
			return VOICEQOS_SYSTEM;
		return VOICEQOS_ARGUMENT_INVALID;
	}
	sceVoiceDeletePort(voiceQoSLocalInPortId);
	result = sceVoiceDeletePort(voiceQoSLocalOutPortId);
	voiceQoSLocalUsed = 0;
	if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
		return VOICEQOS_SYSTEM;
	return result;
}

SceVoiceQoSError sceVoiceQoSEnd(void)
{
	SceInt32 result;
	SceInt32 i;
	SceUInt32 timeout;
	SceUInt32 remoteTimeout;
	SceInt32 retries;

	if (!voiceQoSInitialized)
		return VOICEQOS_NOT_INIT;
	for (i = 0; i < SCE_VOICE_QOS_MAX_CONNECTIONS; i++) {
		VoiceQoSConnection *pConnection = &voiceQoSConnections[i];
		SceUInt32 remoteId;

		if (!voiceQoSInitialized)
			continue;
		retries = VOICEQOS_LOCK_RETRIES;
		timeout = VOICEQOS_LOCK_TIMEOUT;
		while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
			if (--retries == 0)
				goto next;
		}
		remoteId = pConnection->remoteId;
		if (pConnection->localId == 0 && remoteId <= 6) {
			sceVoiceDisconnectIPortFromOPort(voiceQoSLocalInPortId, voiceQoSRemotes[remoteId].outPortId);
			sceVoiceDisconnectIPortFromOPort(voiceQoSRemotes[remoteId].inPortId, voiceQoSLocalOutPortId);
			pConnection->localId = -1;
			pConnection->remoteId = -1;
		}
		sceKernelUnlockLwMutex(&voiceQoSMutex, 1);
next:
		;
	}
	for (i = 0; i < SCE_VOICE_QOS_MAX_REMOTE_ENDPOINTS; i++) {
		if (voiceQoSRemoteUsed[i] != 0 && voiceQoSInitialized) {
			retries = VOICEQOS_LOCK_RETRIES;
			remoteTimeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &remoteTimeout) != 0) {
				if (--retries == 0)
					goto nextRemote;
			}
			if (voiceQoSRemoteUsed[i] != 0) {
				sceVoiceDeletePort(voiceQoSRemotes[i].inPortId);
				sceVoiceDeletePort(voiceQoSRemotes[i].outPortId);
				voiceQoSRemoteUsed[i] = 0;
			}
			sceKernelUnlockLwMutex(&voiceQoSMutex, 1);
		}
nextRemote:
		;
	}
	if (voiceQoSLocalUsed)
		sceVoiceQoSDeleteLocalEndpoint(0);
	sceVoiceStop();
	result = sceVoiceEnd();
	sceKernelDeleteLwMutex(&voiceQoSMutex);
	sceKernelFreeMemBlock(voiceQoSStart.container);
	voiceQoSInitialized = 0;
	return result;
}

SceVoiceQoSError sceVoiceQoSCreateRemoteEndpoint(SceVoiceQoSRemoteId *pRemoteId)
{
	VoiceQoSRemoteEndpoint *pRemote;
	SceInt32 result, remoteId, bitRate;
	float connectionRate;

	if (!voiceQoSInitialized)
		return VOICEQOS_NOT_INIT;
	if (pRemoteId == NULL)
		return VOICEQOS_ARGUMENT_INVALID;
	result = voiceQoSLock();
	if (result != 0)
		return result;
	if (!voiceQoSRemoteUsed[0])
		remoteId = 0;
	else if (!voiceQoSRemoteUsed[1])
		remoteId = 1;
	else if (!voiceQoSRemoteUsed[2])
		remoteId = 2;
	else if (!voiceQoSRemoteUsed[3])
		remoteId = 3;
	else if (!voiceQoSRemoteUsed[4])
		remoteId = 4;
	else if (!voiceQoSRemoteUsed[5])
		remoteId = 5;
	else {
		remoteId = 6;
		if (voiceQoSRemoteUsed[6]) {
			*pRemoteId = -1;
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			return VOICEQOS_RESOURCE_USED;
		}
	}
	pRemote = &voiceQoSRemotes[remoteId];
	connectionRate = (float)voiceQoSConnections[remoteId].desiredBitRate;
	pRemote->inPortType = SCEVOICE_PORTTYPE_IN_VOICE;
	pRemote->outPortType = SCEVOICE_PORTTYPE_OUT_VOICE;
	bitRate = SCEVOICE_BITRATE_3850;
	if (connectionRate >= 4650.0f)
		bitRate = SCEVOICE_BITRATE_4650;
	if (connectionRate >= 5700.0f)
		bitRate = SCEVOICE_BITRATE_5700;
	if (connectionRate >= 7300.0f)
		bitRate = SCEVOICE_BITRATE_7300;
	pRemote->inVolume = 1.0f;
	pRemote->inMute = 0;
	pRemote->inBitRate = bitRate;
	pRemote->outBitRate = bitRate;
	pRemote->outMute = 0;
	pRemote->state = 0;
	pRemote->inThreshold = 100;
	pRemote->outThreshold = 100;
	pRemote->outVolume = 1.0f;
	result = sceVoiceCreatePort(&pRemote->inPortId, (const SceVoicePortParam *)&pRemote->inPortType);
	if (result != 0) {
		if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
			return VOICEQOS_SYSTEM;
		return result;
	}
	{
		SceInt32 outResult = sceVoiceCreatePort(&pRemote->outPortId,
							 (const SceVoicePortParam *)&pRemote->outPortType);

		if (outResult != 0) {
			sceVoiceDeletePort(pRemote->inPortId);
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			return outResult;
		}
	}
	*pRemoteId = remoteId;
	voiceQoSRemoteUsed[remoteId] = 1;
	result = sceKernelUnlockLwMutex(&voiceQoSMutex, 1);
	if (result != 0)
		return VOICEQOS_SYSTEM;
	return result;
}

SceVoiceQoSError sceVoiceQoSDeleteRemoteEndpoint(SceVoiceQoSRemoteId remoteId)
{
	SceInt32 result;

	if (!voiceQoSInitialized)
		return VOICEQOS_NOT_INIT;
	if ((SceUInt32)remoteId > 6)
		return VOICEQOS_ARGUMENT_INVALID;
	result = voiceQoSLock();
	if (result != 0)
		return result;
	if (!voiceQoSRemoteUsed[remoteId]) {
		if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
			return VOICEQOS_SYSTEM;
		return VOICEQOS_ARGUMENT_INVALID;
	}
	sceVoiceDeletePort(voiceQoSRemotes[remoteId].inPortId);
	sceVoiceDeletePort(voiceQoSRemotes[remoteId].outPortId);
	voiceQoSRemoteUsed[remoteId] = 0;
	result = sceKernelUnlockLwMutex(&voiceQoSMutex, 1);
	if (result != 0)
		return VOICEQOS_SYSTEM;
	return result;
}

SceVoiceQoSError sceVoiceQoSConnect(SceVoiceQoSConnectionId *pConnectionId, SceVoiceQoSLocalId localId,
				    SceVoiceQoSRemoteId remoteId)
{
	SceUInt32 timeout;
	SceInt32 retries;
	VoiceQoSConnection *pConnection;
	SceInt32 result, connectionId, error;
	float now;

	if (voiceQoSInitialized) {
		if (pConnectionId != NULL && localId == 0 && (SceUInt32)remoteId <= 6) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					return VOICEQOS_SYSTEM;
			}
			if (voiceQoSConnections[0].localId == -1)
				connectionId = 0;
			else if (voiceQoSConnections[1].localId == -1)
				connectionId = 1;
			else if (voiceQoSConnections[2].localId == -1)
				connectionId = 2;
			else if (voiceQoSConnections[3].localId == -1)
				connectionId = 3;
			else if (voiceQoSConnections[4].localId == -1)
				connectionId = 4;
			else if (voiceQoSConnections[5].localId == -1)
				connectionId = 5;
			else {
				if (voiceQoSConnections[6].localId != -1) {
					if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
						return VOICEQOS_SYSTEM;
					*pConnectionId = -1;
					return VOICEQOS_RESOURCE_USED;
				}
				connectionId = 6;
			}
			pConnection = &voiceQoSConnections[connectionId];
			if (pConnection->remoteId != -1) {
				if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
					return VOICEQOS_SYSTEM;
				return VOICEQOS_INTERNAL_UNRECOVERABLE;
			}
			error = sceVoiceConnectIPortToOPort(voiceQoSLocalInPortId, voiceQoSRemotes[remoteId].outPortId);
			if (error != 0) {
				if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
					return VOICEQOS_SYSTEM;
				return error;
			}
			error = sceVoiceConnectIPortToOPort(voiceQoSRemotes[remoteId].inPortId, voiceQoSLocalOutPortId);
			if (error != 0) {
				sceVoiceDisconnectIPortFromOPort(voiceQoSLocalInPortId, voiceQoSRemotes[remoteId].outPortId);
				if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
					return VOICEQOS_SYSTEM;
				return error;
			}
			*pConnectionId = connectionId;
			pConnection->localId = -1;
			pConnection->expectedSequence = 0;
			pConnection->sendSequence = 0;
			pConnection->duplicateFrames = 0;
			pConnection->remoteId = -1;
			memset(pConnection->seen, 0, sizeof(pConnection->seen));
			pConnection->desiredBitRate = SCEVOICE_BITRATE_7300;
			pConnection->outReadBitRate.value = SCEVOICE_BITRATE_7300;
			pConnection->outReadBytes = 0;
			pConnection->outReadRate = 0.0f;
			pConnection->outBitRate = SCEVOICE_BITRATE_7300;
			now = voiceQoSNow();
			pConnection->lostFrames = 0;
			pConnection->lateFrames = 0;
			pConnection->receivedFrames = 0;
			pConnection->lateRate = 0.0f;
			pConnection->inRate = 0.0f;
			pConnection->receivedRate = 1.0f;
			pConnection->receivedRatio = 1.0f;
			pConnection->adaptTime = now;
			pConnection->outReadTime = now;
			pConnection->writeTime = now;
			pConnection->heartbeatTime = now;
			pConnection->receiveTime = now;
			pConnection->localId = 0;
			pConnection->remoteId = remoteId;
			result = sceKernelUnlockLwMutex(&voiceQoSMutex, 1);
			if (result != 0)
				return VOICEQOS_SYSTEM;
			return result;
		}
		result = VOICEQOS_ARGUMENT_INVALID;
	} else {
		result = VOICEQOS_NOT_INIT;
	}
	return result;
}

SceVoiceQoSError sceVoiceQoSDisconnect(SceVoiceQoSConnectionId connectionId)
{
	VoiceQoSConnection *pConnection;
	SceInt32 result, error;
	SceUInt32 remoteId;

	if (!voiceQoSInitialized)
		return VOICEQOS_NOT_INIT;
	if ((SceUInt32)connectionId > 6)
		return VOICEQOS_ARGUMENT_INVALID;
	result = voiceQoSLock();
	if (result != 0)
		return result;
	pConnection = &voiceQoSConnections[connectionId];
	remoteId = pConnection->remoteId;
	if (pConnection->localId != 0 || remoteId > 6) {
		result = sceKernelUnlockLwMutex(&voiceQoSMutex, 1);
		if (result == 0)
			return result;
		return VOICEQOS_SYSTEM;
	}
	sceVoiceDisconnectIPortFromOPort(voiceQoSLocalInPortId, voiceQoSRemotes[remoteId].outPortId);
	error = sceVoiceDisconnectIPortFromOPort(voiceQoSRemotes[remoteId].inPortId, voiceQoSLocalOutPortId);
	pConnection->localId = -1;
	pConnection->remoteId = -1;
	if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
		return VOICEQOS_SYSTEM;
	return error;
}

SceVoiceQoSError sceVoiceQoSGetLocalEndpoint(SceVoiceQoSConnectionId connectionId, SceVoiceQoSLocalId *pLocalId)
{
	SceUInt32 timeout;
	SceInt32 retries;
	SceInt32 result;

	if (voiceQoSInitialized) {
		if ((SceUInt32)connectionId <= 6 && pLocalId != NULL) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					goto error;
			}
			*pLocalId = voiceQoSConnections[connectionId].localId;
			result = sceKernelUnlockLwMutex(&voiceQoSMutex, 1);
			if (result != 0)
error:
				result = VOICEQOS_SYSTEM;
		} else {
			result = VOICEQOS_ARGUMENT_INVALID;
		}
	} else {
		result = VOICEQOS_NOT_INIT;
	}
	return result;
}

SceVoiceQoSError sceVoiceQoSGetRemoteEndpoint(SceVoiceQoSConnectionId connectionId, SceVoiceQoSRemoteId *pRemoteId)
{
	SceUInt32 timeout;
	SceInt32 retries;
	SceInt32 result;

	if (voiceQoSInitialized) {
		if ((SceUInt32)connectionId <= 6 && pRemoteId != NULL) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					goto error;
			}
			*pRemoteId = voiceQoSConnections[connectionId].remoteId;
			result = sceKernelUnlockLwMutex(&voiceQoSMutex, 1);
			if (result != 0)
error:
				result = VOICEQOS_SYSTEM;
		} else {
			result = VOICEQOS_ARGUMENT_INVALID;
		}
	} else {
		result = VOICEQOS_NOT_INIT;
	}
	return result;
}

SceVoiceQoSError sceVoiceQoSSetLocalEndpointAttribute(SceVoiceQoSLocalId localId, SceVoiceQoSAttributeId attributeId,
						      const void *pAttributeValue, int attributeSize)
{
	SceUInt32 timeout;
	SceInt32 retries;
	SceInt32 result, error;

	if (voiceQoSInitialized) {
		if (pAttributeValue != NULL && localId == 0) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					return VOICEQOS_SYSTEM;
			}
			switch (attributeId) {
			case SCE_VOICE_QOS_ATTR_MIC_VOLUME:
				if (attributeSize != 4)
					goto invalid;
				error = sceVoiceSetVolume(voiceQoSLocalInPortId, *(const float *)pAttributeValue);
				break;
			case SCE_VOICE_QOS_ATTR_MIC_MUTE:
				if (attributeSize != 2)
					goto invalid;
				error = sceVoiceSetMuteFlag(voiceQoSLocalInPortId, *(const SceUInt16 *)pAttributeValue);
				break;
			case SCE_VOICE_QOS_ATTR_SPEAKER_VOLUME:
				if (attributeSize != 4)
					goto invalid;
				error = sceVoiceSetVolume(voiceQoSLocalOutPortId, *(const float *)pAttributeValue);
				break;
			case SCE_VOICE_QOS_ATTR_SPEAKER_MUTE:
				if (attributeSize != 2)
					goto invalid;
				error = sceVoiceSetMuteFlag(voiceQoSLocalOutPortId, *(const SceUInt16 *)pAttributeValue);
				break;
			default:
invalid:
				error = VOICEQOS_ARGUMENT_INVALID;
				break;
			}
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			result = error;
		} else {
			result = VOICEQOS_ARGUMENT_INVALID;
		}
	} else {
		result = VOICEQOS_NOT_INIT;
	}
	return result;
}

SceVoiceQoSError sceVoiceQoSGetLocalEndpointAttribute(SceVoiceQoSLocalId localId, SceVoiceQoSAttributeId attributeId,
						      void *pAttributeValue, int attributeSize)
{
	SceUInt32 timeout;
	SceInt32 retries;
	SceInt32 result, error;

	if (voiceQoSInitialized) {
		if (pAttributeValue != NULL && localId == 0) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					return VOICEQOS_SYSTEM;
			}
			switch (attributeId) {
			case SCE_VOICE_QOS_ATTR_MIC_VOLUME:
				if (attributeSize != 4)
					goto invalid;
				error = sceVoiceGetVolume(voiceQoSLocalInPortId, (float *)pAttributeValue);
				break;
			case SCE_VOICE_QOS_ATTR_MIC_MUTE:
				if (attributeSize != 2)
					goto invalid;
				error = sceVoiceGetMuteFlag(voiceQoSLocalInPortId, (SceUInt16 *)pAttributeValue);
				break;
			case SCE_VOICE_QOS_ATTR_SPEAKER_VOLUME:
				if (attributeSize != 4)
					goto invalid;
				error = sceVoiceGetVolume(voiceQoSLocalOutPortId, (float *)pAttributeValue);
				break;
			case SCE_VOICE_QOS_ATTR_SPEAKER_MUTE:
				if (attributeSize != 2)
					goto invalid;
				error = sceVoiceGetMuteFlag(voiceQoSLocalOutPortId, (SceUInt16 *)pAttributeValue);
				break;
			case SCE_VOICE_QOS_ATTR_MIC_ACTIVE:
				if (attributeSize != 4)
					goto invalid;
				error = sceVoiceGetPortAttr(voiceQoSLocalInPortId, SCEVOICE_ATTR_AUDIOINPUT_ACTIVE, pAttributeValue);
				break;
			default:
invalid:
				error = VOICEQOS_ARGUMENT_INVALID;
				break;
			}
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			result = error;
		} else {
			result = VOICEQOS_ARGUMENT_INVALID;
		}
	} else {
		result = VOICEQOS_NOT_INIT;
	}
	return result;
}

SceVoiceQoSError sceVoiceQoSSetConnectionAttribute(SceVoiceQoSConnectionId connectionId, SceVoiceQoSAttributeId attributeId,
						   const void *pAttributeValue, int attributeSize)
{
	SceUInt32 timeout;
	SceInt32 retries;
	SceInt32 result, error;

	if (voiceQoSInitialized) {
		if ((SceUInt32)connectionId <= 6 && pAttributeValue != NULL) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					return VOICEQOS_SYSTEM;
			}
			if (attributeId == SCE_VOICE_QOS_ATTR_DESIRED_OUT_BIT_RATE && attributeSize == 4) {
				error = 0;
				voiceQoSConnections[connectionId].desiredBitRate = *(const SceInt32 *)pAttributeValue;
			} else {
				error = VOICEQOS_ARGUMENT_INVALID;
			}
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			result = error;
		} else {
			result = VOICEQOS_ARGUMENT_INVALID;
		}
	} else {
		result = VOICEQOS_NOT_INIT;
	}
	return result;
}

SceVoiceQoSError sceVoiceQoSGetConnectionAttribute(SceVoiceQoSConnectionId connectionId, SceVoiceQoSAttributeId attributeId,
						   void *pAttributeValue, int attributeSize)
{
	SceUInt32 timeout;
	SceInt32 retries;
	SceInt32 result, error;

	if (voiceQoSInitialized) {
		if ((SceUInt32)connectionId <= 6 && pAttributeValue != NULL) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					return VOICEQOS_SYSTEM;
			}
			if (attributeId == SCE_VOICE_QOS_ATTR_DESIRED_OUT_BIT_RATE && attributeSize == 4) {
				error = 0;
				*(SceInt32 *)pAttributeValue = voiceQoSConnections[connectionId].desiredBitRate;
			} else {
				error = VOICEQOS_ARGUMENT_INVALID;
			}
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			result = error;
		} else {
			result = VOICEQOS_ARGUMENT_INVALID;
		}
	} else {
		result = VOICEQOS_NOT_INIT;
	}
	return result;
}

SceVoiceQoSError sceVoiceQoSGetStatus(SceVoiceQoSConnectionId connectionId, SceVoiceQoSStatusId statusId,
				      void *pStatusValue, int statusSize)
{
	VoiceQoSConnection *pConnection;
	VoiceQoSRemoteEndpoint *pRemote;
	SceUInt32 timeout;
	SceInt32 retries;
	SceInt32 result, error;

	if (voiceQoSInitialized) {
		if ((SceUInt32)connectionId <= 6 && pStatusValue != NULL) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					return VOICEQOS_SYSTEM;
			}
			pConnection = &voiceQoSConnections[connectionId];
			pRemote = &voiceQoSRemotes[pConnection->remoteId];
			switch (statusId) {
			case SCE_VOICE_QOS_HEARTBEAT_FLAG:
				if (statusSize != 4)
					goto invalid;
				error = 0;
				if (voiceQoSNow() - pConnection->receiveTime > 60.0f) {
					*(SceInt32 *)pStatusValue = 0;
					pRemote->state = 7;
					pConnection->inRate = 0.0f;
				} else {
					*(SceInt32 *)pStatusValue = 1;
				}
				break;
			case SCE_VOICE_QOS_IN_FRAME_RECEIVED_RATIO:
				if (statusSize != 4)
					goto invalid;
				error = pConnection->receivedFrames;
				if (error != 0) {
					error = 0;
					*(float *)pStatusValue = pConnection->receivedRatio;
				} else {
					*(SceInt32 *)pStatusValue = 0;
				}
				break;
			case SCE_VOICE_QOS_OUT_READ_BITRATE:
				if (statusSize != 4)
					goto invalid;
				error = 0;
				*(SceInt32 *)pStatusValue = pConnection->outReadBitRate.value;
				break;
			case SCE_VOICE_QOS_OUT_BITRATE:
				if (statusSize != 4)
					goto invalid;
				error = 0;
				*(SceInt32 *)pStatusValue = pRemote->outBitRate;
				break;
			case SCE_VOICE_QOS_IN_BITRATE:
				if (statusSize != 4)
					goto invalid;
				error = 0;
				*(SceInt32 *)pStatusValue = pRemote->inBitRate;
				break;
			default:
invalid:
				error = VOICEQOS_ARGUMENT_INVALID;
				break;
			}
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			result = error;
		} else {
			result = VOICEQOS_ARGUMENT_INVALID;
		}
	} else {
		result = VOICEQOS_NOT_INIT;
	}
	return result;
}

SceVoiceQoSError sceVoiceQoSWritePacket(SceVoiceQoSConnectionId connectionId, const void *pData, unsigned int *pSize)
{
	SceUInt32 timeout;
	SceInt32 retries;
	SceVoiceBasePortInfo portInfo __attribute__((aligned(32)));
	VoiceQoSConnection *pConnection;
	const SceUInt8 *pBytes = pData;
	SceUInt32 size, consumed;
	SceInt32 result;

	if (voiceQoSInitialized) {
		if ((SceUInt32)connectionId <= 6 && pData != NULL && pSize != NULL) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					return VOICEQOS_SYSTEM;
			}
			size = *pSize;
			pConnection = &voiceQoSConnections[connectionId];
			*pSize = 0;
			consumed = 0;
			while (consumed < size) {
				const SceUInt8 *pFrame = &pBytes[consumed];
				SceUInt32 kind = pFrame[0] >> 4;
				SceUInt32 bytesPerFrame = voiceQoSFrameBytes[kind];

				if (voiceQoSFrameKind[kind] == VOICEQOS_FRAME_AUDIO) {
					SceUInt32 count = pFrame[0] & 0xF;
					SceUInt32 payloadSize = bytesPerFrame * count;
					SceUInt32 frameSize = payloadSize + 2;
					SceUInt32 remoteId;
					VoiceQoSRemoteEndpoint *pRemote;
					SceInt32 bitRate, late, gap;
					SceInt32 reordered;
					SceUInt8 sequence, expected;
					float now, elapsed;

					if (payloadSize == 0 || payloadSize >= 0xFFFFFFFE) {
						result = VOICEQOS_PACKET_INVALID;
						goto fail;
					}
					remoteId = pConnection->remoteId;
					if (remoteId > 6) {
						result = VOICEQOS_ARGUMENT_INVALID;
						goto fail;
					}
					pRemote = &voiceQoSRemotes[remoteId];
					bitRate = SCEVOICE_BITRATE_3850;
					if (kind - 1 <= 2)
						bitRate = voiceQoSBitRate[kind - 1];
					if (pRemote->inBitRate != bitRate) {
						pRemote->inBitRate = bitRate;
						result = sceVoiceSetBitRate(pRemote->inPortId, bitRate);
						if (result != 0)
							goto fail;
					}
					sceClibMemset(&portInfo, 0, 0x18);
					result = sceVoiceGetPortInfo(pRemote->inPortId, &portInfo);
					if (result != 0)
						goto fail;
					sequence = pFrame[1];
					if (pConnection->seen[sequence] != 0) {
						pConnection->duplicateFrames += count;
						consumed += frameSize;
						continue;
					}
					{
						SceUInt32 j;

						pConnection->seen[sequence] = 1;
						for (j = 0; j < 85; j++)
							pConnection->seen[(SceUInt8)(sequence + 85 + j)] = 0;
					}
					expected = pConnection->expectedSequence;
					reordered = 0;
					late = 0;
					gap = 0;
					if (sequence > expected) {
						if (sequence - expected > 127) {
							late = expected - sequence + 256;
							goto lateFrame;
						}
						gap = sequence - expected;
						goto aheadFrame;
					} else if (sequence < expected) {
						if ((SceInt32)(expected - sequence) > 126) {
							gap = sequence + 256 - expected;
							goto aheadFrame;
						}
						late = expected - sequence;
lateFrame:
						gap = -late;
						pConnection->receivedFrames += count;
						pConnection->lostFrames -= count;
						pConnection->lateFrames += count;
						reordered = 1;
						goto measured;
					} else {
						pConnection->expectedSequence = sequence + count;
						pConnection->receivedFrames += count;
						gap = 0;
						goto measured;
					}
aheadFrame:
					pConnection->lostFrames += gap;
					pConnection->receivedFrames += count;
					pConnection->expectedSequence = count + sequence;
					reordered = 2;
measured:
					pConnection->receivedRatio = (float)pConnection->receivedFrames /
								     (float)(pConnection->lostFrames + pConnection->receivedFrames);
					now = voiceQoSNow();
					elapsed = now - pConnection->writeTime;
					if (elapsed > 1.0f && (reordered == 0 || elapsed > 2.5f))
						goto write;
					if (elapsed > 0.0f) {
						if (gap < 0)
							gap = -gap;
						voiceQoSSmooth(&pConnection->receivedRate, (float)count / (float)(SceInt32)(count + gap), elapsed,
							       2.0f);
						if (reordered != 0)
							voiceQoSSmooth(&pConnection->lateRate, (float)(SceInt32)(8 * payloadSize) / elapsed, elapsed,
								       1.0f);
						if (pConnection->inRate - 0.000001f >= 0.0f || pConnection->inRate + 0.000001f <= 0.0f)
							voiceQoSSmooth(&pConnection->inRate, (float)(8 * payloadSize) / elapsed, elapsed, 1.0f);
						else
							pConnection->inRate = (float)(8 * payloadSize) / elapsed;
					}
write:
					result = sceVoiceWriteToIPort(pRemote->inPortId, pFrame + 2, &payloadSize, (SceInt16)gap);
					if (result != 0)
						goto fail;
					pConnection->writeTime = now;
					consumed += frameSize;
				} else if (voiceQoSFrameKind[kind] == VOICEQOS_FRAME_STATE) {
					SceUInt32 remoteId;

					pConnection->receiveTime = voiceQoSNow();
					if ((pFrame[0] & 8) != 0)
						break;
					remoteId = pConnection->remoteId;
					if (remoteId > 6) {
						*pSize = consumed;
						if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
							return VOICEQOS_SYSTEM;
						return VOICEQOS_ARGUMENT_INVALID;
					}
					switch (pFrame[0] & 7) {
					case 1:
						voiceQoSRemotes[remoteId].state = 1;
						break;
					case 2:
						voiceQoSRemotes[remoteId].state = 2;
						break;
					case 3:
						voiceQoSRemotes[remoteId].state = 3;
						break;
					case 4:
						voiceQoSRemotes[remoteId].state = 4;
						break;
					case 5:
						voiceQoSRemotes[remoteId].state = 5;
						break;
					case 6:
						voiceQoSRemotes[remoteId].state = 6;
						break;
					case 7:
						voiceQoSRemotes[remoteId].state = 7;
						break;
					default:
						voiceQoSRemotes[remoteId].state = 0;
						break;
					}
					consumed++;
				} else {
					result = VOICEQOS_PACKET_INVALID;
					if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
						return VOICEQOS_SYSTEM;
					return result;
				}
			}
			*pSize = consumed;
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			return 0;
fail:
			*pSize = consumed;
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			return result;
		}
		result = VOICEQOS_ARGUMENT_INVALID;
	} else {
		result = VOICEQOS_NOT_INIT;
	}
	return result;
}

SceVoiceQoSError sceVoiceQoSReadPacket(SceVoiceQoSConnectionId connectionId, void *pData, unsigned int *pSize)
{
	SceUInt32 timeout;
	SceInt32 retries;
	SceVoiceBasePortInfo portInfo __attribute__((aligned(32)));
	VoiceQoSConnection *pConnection;
	VoiceQoSRemoteEndpoint *pRemote;
	SceUInt8 *pBytes = pData;
	SceUInt32 remoteId, capacity, threshold, header, offset, readSize, produced, bitRate, i;
	SceInt32 result;
	float now, elapsed;

	result = VOICEQOS_NOT_INIT;
	if (voiceQoSInitialized) {
		result = VOICEQOS_ARGUMENT_INVALID;
		if ((SceUInt32)connectionId <= 6 && pData != NULL && pSize != NULL) {
			retries = VOICEQOS_LOCK_RETRIES;
			timeout = VOICEQOS_LOCK_TIMEOUT;
			while (sceKernelLockLwMutex(&voiceQoSMutex, 1, &timeout) != 0) {
				if (--retries == 0)
					return VOICEQOS_SYSTEM;
			}
			pConnection = &voiceQoSConnections[connectionId];
			remoteId = pConnection->remoteId;
			if (remoteId > 6 || (capacity = *pSize, *pSize = 0, pRemote = &voiceQoSRemotes[remoteId], capacity <= 2)) {
				if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) == 0)
					return VOICEQOS_ARGUMENT_INVALID;
				return VOICEQOS_SYSTEM;
			}
			header = 0;
			threshold = 2;
			now = voiceQoSNow();
			if (now - pConnection->heartbeatTime >= 5.0f) {
				float ratio;

				pBytes[0] = 0xF0;
				if (pConnection->receivedFrames != 0) {
					ratio = pConnection->receivedRate;
					if (ratio < 0.95f) {
						if (ratio >= 0.9f && ratio < 0.95f)
							pBytes[0] = 0xF1;
						else if (ratio >= 0.85f && ratio < 0.9f)
							pBytes[0] = 0xF2;
						else if (ratio >= 0.8f && ratio < 0.85f)
							pBytes[0] = 0xF3;
						else if (ratio >= 0.75f && ratio < 0.8f)
							pBytes[0] = 0xF4;
						else if (ratio >= 0.7f && ratio < 0.75f)
							pBytes[0] = 0xF5;
						else if (ratio >= 0.6f && ratio < 0.7f)
							pBytes[0] = 0xF6;
						else
							pBytes[0] = 0xF7;
					}
				}
				pConnection->heartbeatTime = now;
				threshold = 3;
				*pSize += 1;
				capacity--;
				header = 1;
			}
			elapsed = now - pConnection->adaptTime;
			if (elapsed > 0.0f) {
				SceInt32 rate, state;

				voiceQoSSmooth(&pConnection->outReadBitRate.rate, (float)(8 * (capacity - 2)) / elapsed, elapsed, 30.0f);
				if (pConnection->outReadBytes != 0) {
					if (pConnection->outReadRate - 0.000001f < 0.0f && pConnection->outReadRate + 0.000001f > 0.0f) {
						float newRate = (float)(8 * pConnection->outReadBytes) / (now - pConnection->outReadTime);

						if (newRate < 1000.0f)
							newRate = 0.0f;
						pConnection->outReadRate = newRate;
					} else {
						float readElapsed = now - pConnection->outReadTime;

						voiceQoSSmooth(&pConnection->outReadRate, (float)(8 * pConnection->outReadBytes) / readElapsed,
							       readElapsed, 1.0f);
					}
					pConnection->outReadTime = now;
				}
				pConnection->outReadBytes = 0;
				rate = pConnection->outBitRate;
				state = pRemote->state;
				if (state > 6) {
					rate = rate > 3879 ? rate - 30 : 3850;
					pConnection->outBitRate = rate;
				} else if ((SceUInt32)(state - 4) <= 2) {
					rate = rate > 3869 ? rate - 20 : 3850;
					pConnection->outBitRate = rate;
				} else if ((SceUInt32)(state - 2) <= 1) {
					rate = rate > 3859 ? rate - 10 : 3850;
					pConnection->outBitRate = rate;
				} else if (state != 1) {
					if (state == 0 && rate < pConnection->desiredBitRate) {
						rate += 100;
						pConnection->outBitRate = rate;
					}
				} else {
					rate = rate > 3854 ? rate - 5 : 3850;
					pConnection->outBitRate = rate;
				}
				if (pConnection->desiredBitRate < rate) {
					rate = pConnection->desiredBitRate;
					pConnection->outBitRate = rate;
				}
				if (pConnection->outReadBitRate.value < rate) {
					rate = pConnection->outReadBitRate.value;
					pConnection->outBitRate = rate;
				}
				bitRate = SCEVOICE_BITRATE_3850;
				if ((float)rate >= 4650.0f)
					bitRate = SCEVOICE_BITRATE_4650;
				if ((float)rate >= 5700.0f)
					bitRate = SCEVOICE_BITRATE_5700;
				if ((float)rate >= 7300.0f)
					bitRate = SCEVOICE_BITRATE_7300;
				if (pRemote->outBitRate != bitRate) {
					for (i = 0; i < SCE_VOICE_QOS_MAX_CONNECTIONS; i++) {
						if (voiceQoSConnections[i].remoteId != -1) {
							VoiceQoSRemoteEndpoint *pOther = &voiceQoSRemotes[voiceQoSConnections[i].remoteId];

							pOther->outBitRate = bitRate;
							sceVoiceSetBitRate(pOther->outPortId, bitRate);
							voiceQoSConnections[i].outBitRate = bitRate;
						}
					}
				}
			}
			pConnection->adaptTime = now;
			sceClibMemset(&portInfo, 0, 0x18);
			result = sceVoiceGetPortInfo(pRemote->outPortId, &portInfo);
			if (result == 0) {
				sceClibMemset(voiceQoSReadBuffer, 0, sizeof(voiceQoSReadBuffer));
				readSize = sizeof(voiceQoSReadBuffer);
				result = sceVoiceReadFromOPort(pRemote->outPortId, voiceQoSReadBuffer, &readSize);
			}
			if (result != 0) {
				if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
					return VOICEQOS_SYSTEM;
				return result;
			}
			if (readSize != 0 && capacity > threshold) {
				SceUInt32 frameSize = portInfo.frameSize;
				SceUInt32 limit = 15 * frameSize;
				SceUInt32 chunk = readSize < limit ? readSize : limit;

				if (chunk > capacity - 2 - header)
					chunk = capacity - 2 - header;
				offset = 0;
				produced = header;
				while (chunk >= frameSize) {
					SceUInt32 frames = chunk / frameSize;
					SceUInt32 bytes = frames * frameSize;
					SceUInt8 *pFrame = &pBytes[produced];
					SceUInt8 kind;

					if (pRemote->outBitRate == 4650)
						kind = 0x10;
					else if (pRemote->outBitRate == 5700)
						kind = 0x20;
					else if (pRemote->outBitRate == 3850)
						kind = 0x00;
					else
						kind = 0x30;
					pFrame[0] = (frames & 0xF) | kind;
					pFrame[1] = pConnection->sendSequence;
					sceClibMemcpy_safe(pFrame + 2, &voiceQoSReadBuffer[offset], bytes);
					offset += bytes;
					produced += bytes + 2;
					pConnection->outReadBytes += bytes;
					pConnection->outFrameCount += frames;
					pConnection->sendSequence += frames;
					*pSize += bytes + 2;
					if (capacity > produced + 2 && readSize > offset) {
						chunk = readSize - offset;
						if (chunk > limit)
							chunk = limit;
						if (chunk > capacity - 2 - produced)
							chunk = capacity - 2 - produced;
						continue;
					}
					break;
				}
			}
			if (sceKernelUnlockLwMutex(&voiceQoSMutex, 1) != 0)
				return VOICEQOS_SYSTEM;
			return 0;
		}
	}
	return result;
}
