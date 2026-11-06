/*
 * SceAvcdecForPlayer: a player's AVC decoder (avcdec_for_player.h). Every call checks the
 * handle and takes its mutex; a decode or get-picture call that must wait drops the mutex
 * while it waits on its event flag, which the decoder's callbacks, the cancel calls and
 * stop and flush set.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <libkernel/libkernel.h>
#include "avcdec_for_player.h"

/* what es.pBuf points to in the empty AU that restarts the decoder (es.size is 0) */
static SceUInt8 s_emptyEs[256];

/* what has been created when a create call fails, so far as it must be undone */
enum {
	CREATED_PARAM_MUTEX,
	CREATED_DECODE_AU_EVENT_FLAG,
	CREATED_GET_PICTURE_EVENT_FLAG,
	CREATED_CALLBACK_EVENT_FLAG,
	CREATED_CALLBACK_THREAD,
	STARTED_CALLBACK_THREAD,
	CREATED_DECODER,
	REGISTERED_CALLBACK,
};

/* the decoder's other events: whatever waits looks again */
static int eventCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
			 void *pCommon)
{
	SceAvcdecPlayer *player = pCommon;

	sceKernelSetEventFlag(player->decodeAuEventFlag,
			      SCE_AVCDEC_PLAYER_EVF_AU_READY | SCE_AVCDEC_PLAYER_EVF_NAL_AU_READY);
	sceKernelSetEventFlag(player->getPictureEventFlag, SCE_AVCDEC_PLAYER_EVF_PICTURE_READY);
	return 0;
}

/* notifyCount pictures decoded */
static int pictureCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
			   void *pCommon)
{
	SceAvcdecPlayer *player = pCommon;

	if (sceKernelLockLwMutex(&player->paramMutex, 1, NULL) >= 0) {
		player->decodedPictures += notifyCount;
		sceKernelSetEventFlag(player->decodeAuEventFlag,
				      SCE_AVCDEC_PLAYER_EVF_AU_READY |
				      SCE_AVCDEC_PLAYER_EVF_NAL_AU_READY);
		sceKernelSetEventFlag(player->getPictureEventFlag,
				      SCE_AVCDEC_PLAYER_EVF_PICTURE_READY);
		sceKernelUnlockLwMutex(&player->paramMutex, 1);
	}
	return 0;
}

/* the thread the decoder's callbacks run in, until the player is deleted */
static int callbackThreadEntry(SceSize argSize, void *pArgBlock)
{
	SceAvcdecPlayer *player = *(SceAvcdecPlayer **)pArgBlock;
	SceUInt32 outBits;
	char name[32];
	int ret;

	player->callbackThreadError = 0;
	formatString(name, sizeof(name), "SceAvcdecPlayer%08x", sceKernelGetProcessId());
	ret = sceKernelCreateCallback(name, 0, pictureCallback, player);
	player->pictureCallback = ret;
	if (ret < 0) {
		player->callbackThreadError = ret;
		sceKernelSetEventFlag(player->callbackEventFlag, SCE_AVCDEC_PLAYER_THREAD_ERROR);
	} else {
		ret = sceKernelCreateCallback(name, 0, eventCallback, player);
		player->eventCallback = ret;
		if (ret < 0) {
			player->callbackThreadError = ret;
			sceKernelSetEventFlag(player->callbackEventFlag,
					      SCE_AVCDEC_PLAYER_THREAD_ERROR);
		} else {
			sceKernelSetEventFlag(player->callbackEventFlag,
					      SCE_AVCDEC_PLAYER_THREAD_READY);
			outBits = 0;
			ret = sceKernelWaitEventFlagCB(player->callbackEventFlag,
						       SCE_AVCDEC_PLAYER_THREAD_EXIT,
						       SCE_KERNEL_EVF_WAITMODE_OR |
						       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						       &outBits, NULL);
			if (ret < 0) {
				player->callbackThreadError = ret;
				sceKernelSetEventFlag(player->callbackEventFlag,
						      SCE_AVCDEC_PLAYER_THREAD_ERROR);
			}
			sceKernelDeleteCallback(player->eventCallback);
		}
		sceKernelDeleteCallback(player->pictureCallback);
	}
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

int SceAvcdecForPlayer_3C1FFE1F(SceUInt32 codecType, const SceAvcdecQueryDecoderInfo *pQuery,
				SceAvcdecDecoderInfo *pDecoderInfo)
{
	return sceAvcdecQueryDecoderMemSize(codecType, pQuery, pDecoderInfo);
}

int SceAvcdecForPlayer_DD3E7360(SceUInt32 codecType, const SceAvcdecQueryDecoderInfo *pQuery,
				SceAvcdecDecoderInfo *pDecoderInfo)
{
	return sceAvcdecQueryDecoderMemSizeNongameapp(codecType, pQuery, pDecoderInfo);
}

/*
 * Undoes a create or the player, from what was created last (the handle's mutex is held and
 * stays locked; the param mutex is locked to be deleted).
 */
static inline __attribute__((always_inline)) void destroyPlayer(SceAvcdecPlayer *player,
								 int created)
{
	sceKernelLockLwMutex(&player->mutex, 1, NULL);
	sceKernelLockLwMutex(&player->paramMutex, 1, NULL);
	switch (created) {
	case REGISTERED_CALLBACK:
		sceAvcdecUnregisterCallbackInternal(&player->ctrl);
		/* fall through */
	case CREATED_DECODER:
		sceAvcdecDeleteDecoder(&player->ctrl);
		/* fall through */
	case STARTED_CALLBACK_THREAD:
		sceKernelSetEventFlag(player->callbackEventFlag, SCE_AVCDEC_PLAYER_THREAD_EXIT);
		sceKernelWaitThreadEnd(player->callbackThread, NULL, NULL);
		/* fall through */
	case CREATED_CALLBACK_THREAD:
		sceKernelDeleteThread(player->callbackThread);
		/* fall through */
	case CREATED_CALLBACK_EVENT_FLAG:
		sceKernelDeleteEventFlag(player->callbackEventFlag);
		/* fall through */
	case CREATED_GET_PICTURE_EVENT_FLAG:
		sceKernelDeleteEventFlag(player->getPictureEventFlag);
		/* fall through */
	case CREATED_DECODE_AU_EVENT_FLAG:
		sceKernelDeleteEventFlag(player->decodeAuEventFlag);
		/* fall through */
	case CREATED_PARAM_MUTEX:
		sceKernelDeleteLwMutex(&player->paramMutex);
	}
	sceKernelDeleteLwMutex(&player->mutex);
	player->magic = 0;
}

/* the two create calls, which differ in the decoder they create */
static inline __attribute__((always_inline)) int createPlayer(SceUInt32 codecType,
							       SceAvcdecPlayer *player,
							       const SceAvcdecQueryDecoderInfo *pDecoderInfo,
							       bool nongameapp)
{
	SceUInt32 outBits;
	char name[32];
	SceUID pid;
	int created, ret;

	if (player == NULL)
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_POINTER;
	if (player->size != sizeof(SceAvcdecPlayer))
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_ARGUMENT_SIZE;
	if (player->magic == SCE_AVCDEC_PLAYER_MAGIC)
		return SCE_AVCDEC_PLAYER_ERROR_ALREADY_USED;

	pid = sceKernelGetProcessId();
	formatString(name, sizeof(name), "SceAvcdecPlayer%08x", pid);
	ret = sceKernelCreateLwMutex(&player->mutex, name, SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0,
				     NULL);
	if (ret < 0) {
		player->magic = 0;
		return ret;
	}
	ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
	if (ret >= 0) {
		formatString(name, sizeof(name), "SceAvcdecPlayerParam%08x", pid);
		ret = sceKernelCreateLwMutex(&player->paramMutex, name,
					     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	}
	if (ret < 0) {
		sceKernelLockLwMutex(&player->mutex, 1, NULL);
		sceKernelDeleteLwMutex(&player->mutex);
		player->magic = 0;
		return ret;
	}

	created = CREATED_PARAM_MUTEX;
	formatString(name, sizeof(name), "SceAvcdecPlayerDecodeAu%08x", pid);
	player->decodeAuEventFlag = sceKernelCreateEventFlag(name, SCE_KERNEL_EVF_ATTR_MULTI, 0,
							     NULL);
	ret = player->decodeAuEventFlag;
	if (ret < 0)
		goto error;
	created = CREATED_DECODE_AU_EVENT_FLAG;
	formatString(name, sizeof(name), "SceAvcdecPlayerGetPic%08x", pid);
	player->getPictureEventFlag = sceKernelCreateEventFlag(name, SCE_KERNEL_EVF_ATTR_MULTI, 0,
							       NULL);
	ret = player->getPictureEventFlag;
	if (ret < 0)
		goto error;
	created = CREATED_GET_PICTURE_EVENT_FLAG;
	formatString(name, sizeof(name), "SceAvcdecPlayerCB%08x", pid);
	player->callbackEventFlag = sceKernelCreateEventFlag(name, 0, 0, NULL);
	ret = player->callbackEventFlag;
	if (ret < 0)
		goto error;
	created = CREATED_CALLBACK_EVENT_FLAG;
	player->callbackThread = sceKernelCreateThread(name, callbackThreadEntry, player->priority,
						       SCE_AVCDEC_PLAYER_THREAD_STACK_SIZE, 0,
						       player->cpuAffinityMask, NULL);
	ret = player->callbackThread;
	if (ret < 0)
		goto error;
	created = CREATED_CALLBACK_THREAD;
	ret = sceKernelStartThread(player->callbackThread, sizeof(player), &player);
	if (ret < 0)
		goto error;
	created = STARTED_CALLBACK_THREAD;
	ret = sceKernelWaitEventFlag(player->callbackEventFlag,
				     SCE_AVCDEC_PLAYER_THREAD_READY | SCE_AVCDEC_PLAYER_THREAD_ERROR,
				     SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				     &outBits, NULL);
	if (ret < 0)
		goto error;
	if (outBits & SCE_AVCDEC_PLAYER_THREAD_ERROR) {
		ret = player->callbackThreadError;
		goto error;
	}
	if (nongameapp)
		ret = sceAvcdecCreateDecoderNongameapp(codecType, &player->ctrl, pDecoderInfo);
	else
		ret = sceAvcdecCreateDecoder(codecType, &player->ctrl, pDecoderInfo);
	if (ret < 0)
		goto error;
	created = CREATED_DECODER;
	ret = sceAvcdecRegisterCallbackInternal(&player->ctrl, player->pictureCallback,
						SCE_AVCDEC_PLAYER_PICTURE_EVENTS);
	if (ret < 0)
		goto error;
	created = REGISTERED_CALLBACK;
	ret = sceAvcdecRegisterCallbackInternal(&player->ctrl, player->eventCallback,
						SCE_AVCDEC_PLAYER_OTHER_EVENTS);
	if (ret < 0)
		goto error;
	player->magic = SCE_AVCDEC_PLAYER_MAGIC;
	player->decodedPictures = 0;
	player->state = SCE_AVCDEC_PLAYER_STATE_RESTART;
	goto out;

error:
	destroyPlayer(player, created);
out:
	sceKernelUnlockLwMutex(&player->mutex, 1);
	return ret;
}

int SceAvcdecForPlayer_B2EBD4EE(SceUInt32 codecType, SceAvcdecPlayer *player,
				const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	return createPlayer(codecType, player, pDecoderInfo, false);
}

int SceAvcdecForPlayer_C2569BD6(SceUInt32 codecType, SceAvcdecPlayer *player,
				const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	return createPlayer(codecType, player, pDecoderInfo, true);
}

int SceAvcdecForPlayer_58335367(SceAvcdecPlayer *player)
{
	if (player == NULL)
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_POINTER;
	if (player->size != sizeof(SceAvcdecPlayer))
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_ARGUMENT_SIZE;
	if (player->magic != SCE_AVCDEC_PLAYER_MAGIC)
		return SCE_AVCDEC_PLAYER_ERROR_NOT_INITIALIZE;

	destroyPlayer(player, REGISTERED_CALLBACK);
	return 0;
}

/*
 * Starts a decode call: marks it running, and after a stop or flush clears the event flags
 * and says (true) that the decoder is restarted with an empty AU first.
 */
static inline __attribute__((always_inline)) bool beginDecode(SceAvcdecPlayer *player)
{
	bool restart = false;

	if (player->state & SCE_AVCDEC_PLAYER_STATE_RESTART) {
		player->state = (player->state | SCE_AVCDEC_PLAYER_STATE_DECODING) &
				~(SCE_AVCDEC_PLAYER_STATE_STOPPED | SCE_AVCDEC_PLAYER_STATE_FLUSHED |
				  SCE_AVCDEC_PLAYER_STATE_RESTART);
		restart = true;
		sceKernelClearEventFlag(player->decodeAuEventFlag, 0);
		sceKernelClearEventFlag(player->getPictureEventFlag, 0);
	} else {
		player->state |= SCE_AVCDEC_PLAYER_STATE_DECODING;
	}
	return restart;
}

/* decodes a NAL AU, waiting while the decoder's buffer is full */
int SceAvcdecForPlayer_291780A0(SceAvcdecPlayer *player, const VeneziaVipVideoInput *pAu)
{
	SceUInt timeout = 1;
	SceUInt32 outBits = 0;
	SceAvcdecAu emptyAu;
	bool restart;
	int ret, waitRet;

	if (player == NULL)
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_POINTER;
	if (player->size != sizeof(SceAvcdecPlayer))
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_ARGUMENT_SIZE;
	if (player->magic != SCE_AVCDEC_PLAYER_MAGIC)
		return SCE_AVCDEC_PLAYER_ERROR_NOT_INITIALIZE;

	ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (player->state & SCE_AVCDEC_PLAYER_STATE_DECODING) {
		ret = SCE_AVCDEC_PLAYER_ERROR_ALREADY_USED;
		goto out;
	}
	restart = beginDecode(player);
	sceKernelSetEventFlag(player->decodeAuEventFlag, SCE_AVCDEC_PLAYER_EVF_NAL_AU_READY);
	for (;;) {
		if (restart) {
			emptyAu.es.pBuf = &s_emptyEs[2];
			emptyAu.es.size = 0;
			timeout = 1;
			ret = sceAvcdecDecodeAuInternal(&player->ctrl, &emptyAu, &timeout);
			if (ret < 0)
				break;
			ret = sceKernelLockLwMutex(&player->paramMutex, 1, NULL);
			if (ret < 0)
				break;
			player->decodedPictures = 0;
			sceKernelUnlockLwMutex(&player->paramMutex, 1);
		} else {
			sceKernelUnlockLwMutex(&player->mutex, 1);
			waitRet = sceKernelWaitEventFlag(player->decodeAuEventFlag,
							 SCE_AVCDEC_PLAYER_EVF_NAL_AU_READY |
							 SCE_AVCDEC_PLAYER_EVF_NAL_AU_CANCEL |
							 SCE_AVCDEC_PLAYER_EVF_STOP |
							 SCE_AVCDEC_PLAYER_EVF_FLUSH,
							 SCE_KERNEL_EVF_WAITMODE_OR |
							 SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
							 &outBits, NULL);
			ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
			if (ret < 0) {
				player->state &= ~SCE_AVCDEC_PLAYER_STATE_DECODING;
				return ret;
			}
			ret = waitRet;
			if (ret < 0)
				break;
			if (outBits & SCE_AVCDEC_PLAYER_EVF_NAL_AU_CANCEL) {
				ret = SCE_AVCDEC_PLAYER_ERROR_CANCELED;
				break;
			}
			if (player->state & SCE_AVCDEC_PLAYER_STATE_STOPPED) {
				ret = SCE_AVCDEC_PLAYER_ERROR_STOPPED;
				break;
			}
			if (player->state & SCE_AVCDEC_PLAYER_STATE_FLUSHED) {
				ret = SCE_AVCDEC_PLAYER_ERROR_FLUSHED;
				break;
			}
		}
		timeout = 1;
		ret = sceAvcdecDecodeAuNalAuInternal(&player->ctrl, pAu, &timeout);
		if (restart || ret != SCE_AVCDEC_ERROR_ES_BUFFER_FULL)
			break;
	}
out:
	player->state &= ~SCE_AVCDEC_PLAYER_STATE_DECODING;
	sceKernelUnlockLwMutex(&player->mutex, 1);
	return ret;
}

/* decodes an AU, waiting while the decoder's buffer is full */
int SceAvcdecForPlayer_EB436C3B(SceAvcdecPlayer *player, const SceAvcdecAu *pAu)
{
	SceUInt timeout = 1;
	SceUInt32 outBits = 0;
	SceAvcdecAu emptyAu;
	bool restart;
	int ret, waitRet;

	if (player == NULL)
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_POINTER;
	if (player->size != sizeof(SceAvcdecPlayer))
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_ARGUMENT_SIZE;
	if (player->magic != SCE_AVCDEC_PLAYER_MAGIC)
		return SCE_AVCDEC_PLAYER_ERROR_NOT_INITIALIZE;

	ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (player->state & SCE_AVCDEC_PLAYER_STATE_DECODING) {
		ret = SCE_AVCDEC_PLAYER_ERROR_ALREADY_USED;
		goto out;
	}
	restart = beginDecode(player);
	sceKernelSetEventFlag(player->decodeAuEventFlag, SCE_AVCDEC_PLAYER_EVF_AU_READY);
	for (;;) {
		if (restart) {
			timeout = 1;
			emptyAu.es.pBuf = &s_emptyEs[2];
			emptyAu.es.size = 0;
			ret = sceAvcdecDecodeAuInternal(&player->ctrl, &emptyAu, &timeout);
			if (ret < 0)
				break;
			ret = sceKernelLockLwMutex(&player->paramMutex, 1, NULL);
			if (ret < 0)
				break;
			player->decodedPictures = 0;
			sceKernelUnlockLwMutex(&player->paramMutex, 1);
		} else {
			sceKernelUnlockLwMutex(&player->mutex, 1);
			waitRet = sceKernelWaitEventFlag(player->decodeAuEventFlag,
							 SCE_AVCDEC_PLAYER_EVF_AU_READY |
							 SCE_AVCDEC_PLAYER_EVF_AU_CANCEL |
							 SCE_AVCDEC_PLAYER_EVF_STOP |
							 SCE_AVCDEC_PLAYER_EVF_FLUSH,
							 SCE_KERNEL_EVF_WAITMODE_OR |
							 SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
							 &outBits, NULL);
			ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
			if (ret < 0) {
				player->state &= ~SCE_AVCDEC_PLAYER_STATE_DECODING;
				return ret;
			}
			ret = waitRet;
			if (ret < 0)
				break;
			if (outBits & SCE_AVCDEC_PLAYER_EVF_AU_CANCEL) {
				ret = SCE_AVCDEC_PLAYER_ERROR_CANCELED;
				break;
			}
			if (player->state & SCE_AVCDEC_PLAYER_STATE_STOPPED) {
				ret = SCE_AVCDEC_PLAYER_ERROR_STOPPED;
				break;
			}
			if (player->state & SCE_AVCDEC_PLAYER_STATE_FLUSHED) {
				ret = SCE_AVCDEC_PLAYER_ERROR_FLUSHED;
				break;
			}
		}
		timeout = 1;
		ret = sceAvcdecDecodeAuInternal(&player->ctrl, pAu, &timeout);
		if (restart || ret != SCE_AVCDEC_ERROR_ES_BUFFER_FULL)
			break;
	}
out:
	player->state &= ~SCE_AVCDEC_PLAYER_STATE_DECODING;
	sceKernelUnlockLwMutex(&player->mutex, 1);
	return ret;
}

/*
 * Gets the decoded pictures, waiting until there are some; after a stop or flush, the
 * remaining ones, and *pState says which (SCE_AVCDEC_PLAYER_STATE_STOPPED or _FLUSHED).
 */
int SceAvcdecForPlayer_44C56742(SceAvcdecPlayer *player, SceAvcdecArrayPicture *pArrayPicture,
				SceUInt32 *pState, SceAvcdecPlayerPictureInfo *pInfo)
{
	SceUInt timeout = 1;
	SceUInt32 outBits = 0;
	SceInt32 decodedPictures;
	int ret, waitRet;

	if (player == NULL || pArrayPicture == NULL || pState == NULL)
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_POINTER;
	if (player->size != sizeof(SceAvcdecPlayer) ||
	    (pInfo != NULL && pInfo->size != sizeof(SceAvcdecPlayerPictureInfo)))
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_ARGUMENT_SIZE;
	if (player->magic != SCE_AVCDEC_PLAYER_MAGIC)
		return SCE_AVCDEC_PLAYER_ERROR_NOT_INITIALIZE;

	*pState = 0;
	ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (player->state & SCE_AVCDEC_PLAYER_STATE_GETTING_PICTURE) {
		ret = SCE_AVCDEC_PLAYER_ERROR_ALREADY_USED;
		goto out;
	}
	player->state |= SCE_AVCDEC_PLAYER_STATE_GETTING_PICTURE;
	sceKernelSetEventFlag(player->getPictureEventFlag, SCE_AVCDEC_PLAYER_EVF_PICTURE_READY);
	do {
		sceKernelUnlockLwMutex(&player->mutex, 1);
		waitRet = sceKernelWaitEventFlag(player->getPictureEventFlag,
						 SCE_AVCDEC_PLAYER_EVF_PICTURE_READY |
						 SCE_AVCDEC_PLAYER_EVF_PICTURE_CANCEL |
						 SCE_AVCDEC_PLAYER_EVF_STOP | SCE_AVCDEC_PLAYER_EVF_FLUSH,
						 SCE_KERNEL_EVF_WAITMODE_OR |
						 SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						 &outBits, NULL);
		ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
		if (ret < 0) {
			player->state &= ~SCE_AVCDEC_PLAYER_STATE_GETTING_PICTURE;
			return ret;
		}
		ret = waitRet;
		if (ret < 0)
			goto out;
		if (outBits & SCE_AVCDEC_PLAYER_EVF_PICTURE_CANCEL) {
			ret = SCE_AVCDEC_PLAYER_ERROR_CANCELED;
			goto out;
		}
		if (player->state & (SCE_AVCDEC_PLAYER_STATE_STOPPED |
				     SCE_AVCDEC_PLAYER_STATE_FLUSHED)) {
			ret = sceAvcdecDecodeStop(&player->ctrl, pArrayPicture);
		} else {
			timeout = 1;
			ret = sceAvcdecDecodeGetPictureInternal(&player->ctrl, pArrayPicture,
								&timeout);
		}
		if (ret < 0)
			goto out;
		ret = sceKernelLockLwMutex(&player->paramMutex, 1, NULL);
		if (ret < 0)
			goto out;
		decodedPictures = player->decodedPictures - pArrayPicture->numOfOutput;
		player->decodedPictures = decodedPictures;
		sceKernelUnlockLwMutex(&player->paramMutex, 1);
		if (pInfo != NULL)
			pInfo->decodedPictures = decodedPictures;
		if (player->state & (SCE_AVCDEC_PLAYER_STATE_STOPPED |
				     SCE_AVCDEC_PLAYER_STATE_FLUSHED)) {
			*pState = player->state & (SCE_AVCDEC_PLAYER_STATE_STOPPED |
						   SCE_AVCDEC_PLAYER_STATE_FLUSHED);
			/* all of them got: the stop or flush is over */
			if (pArrayPicture->numOfElm != 0 && pArrayPicture->numOfOutput == 0)
				player->state &= ~(SCE_AVCDEC_PLAYER_STATE_STOPPED |
						   SCE_AVCDEC_PLAYER_STATE_FLUSHED);
			break;
		}
	} while (pArrayPicture->numOfOutput == 0 && pArrayPicture->numOfElm != 0);
out:
	player->state &= ~SCE_AVCDEC_PLAYER_STATE_GETTING_PICTURE;
	sceKernelUnlockLwMutex(&player->mutex, 1);
	return ret;
}

/* sets bits of one of the player's event flags, under its mutex */
static inline __attribute__((always_inline)) int signalPlayer(SceAvcdecPlayer *player,
							       bool getPicture, SceUInt32 bits)
{
	int ret;

	if (player == NULL)
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_POINTER;
	if (player->size != sizeof(SceAvcdecPlayer))
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_ARGUMENT_SIZE;
	if (player->magic != SCE_AVCDEC_PLAYER_MAGIC)
		return SCE_AVCDEC_PLAYER_ERROR_NOT_INITIALIZE;

	ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelSetEventFlag(getPicture ? player->getPictureEventFlag :
				    player->decodeAuEventFlag, bits);
	sceKernelUnlockLwMutex(&player->mutex, 1);
	return ret;
}

/* cancels a NAL AU decode call's wait */
int SceAvcdecForPlayer_32A8BBA9(SceAvcdecPlayer *player)
{
	return signalPlayer(player, false, SCE_AVCDEC_PLAYER_EVF_NAL_AU_CANCEL);
}

/* cancels an AU decode call's wait */
int SceAvcdecForPlayer_7A3AB6D6(SceAvcdecPlayer *player)
{
	return signalPlayer(player, false, SCE_AVCDEC_PLAYER_EVF_AU_CANCEL);
}

/* cancels a get-picture call's wait */
int SceAvcdecForPlayer_8D489544(SceAvcdecPlayer *player)
{
	return signalPlayer(player, true, SCE_AVCDEC_PLAYER_EVF_PICTURE_CANCEL);
}

/* stops the decoder: the waiting calls end, and the next decode restarts it */
int SceAvcdecForPlayer_4904E8C9(SceAvcdecPlayer *player)
{
	SceAvcdecArrayPicture arrayPicture;
	int ret;

	if (player == NULL)
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_POINTER;
	if (player->size != sizeof(SceAvcdecPlayer))
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_ARGUMENT_SIZE;
	if (player->magic != SCE_AVCDEC_PLAYER_MAGIC)
		return SCE_AVCDEC_PLAYER_ERROR_NOT_INITIALIZE;

	ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	__builtin_memset(&arrayPicture, 0, sizeof(arrayPicture));
	ret = sceAvcdecDecodeStop(&player->ctrl, &arrayPicture);
	player->state |= SCE_AVCDEC_PLAYER_STATE_STOPPED | SCE_AVCDEC_PLAYER_STATE_RESTART;
	sceKernelSetEventFlag(player->decodeAuEventFlag, SCE_AVCDEC_PLAYER_EVF_STOP);
	sceKernelSetEventFlag(player->getPictureEventFlag, SCE_AVCDEC_PLAYER_EVF_STOP);
	sceKernelUnlockLwMutex(&player->mutex, 1);
	return ret;
}

/* flushes the decoder: the waiting calls end, and the next decode restarts it */
int SceAvcdecForPlayer_B6BAAF04(SceAvcdecPlayer *player)
{
	int ret;

	if (player == NULL)
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_POINTER;
	if (player->size != sizeof(SceAvcdecPlayer))
		return SCE_AVCDEC_PLAYER_ERROR_INVALID_ARGUMENT_SIZE;
	if (player->magic != SCE_AVCDEC_PLAYER_MAGIC)
		return SCE_AVCDEC_PLAYER_ERROR_NOT_INITIALIZE;

	ret = sceKernelLockLwMutex(&player->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = sceAvcdecDecodeFlush(&player->ctrl);
	player->state |= SCE_AVCDEC_PLAYER_STATE_FLUSHED | SCE_AVCDEC_PLAYER_STATE_RESTART;
	sceKernelSetEventFlag(player->decodeAuEventFlag, SCE_AVCDEC_PLAYER_EVF_FLUSH);
	sceKernelSetEventFlag(player->getPictureEventFlag, SCE_AVCDEC_PLAYER_EVF_FLUSH);
	sceKernelUnlockLwMutex(&player->mutex, 1);
	return ret;
}

int formatString(char *destination, SceSize size, const char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = sceClibVsnprintf(destination, size, format, arguments);
	va_end(arguments);
	return result;
}
