/*
 * SceAvcdecForPlayer (vs0:sys/external/avcdec_for_player.suprx): a player's AVC decoder. It
 * wraps one SceVideodecUser decoder in a handle the caller allocates, with a callback thread
 * that turns the decoder's events into event flags, so that decoding an access unit and
 * getting the pictures can block, be cancelled, stopped or flushed from other threads.
 * No binary names the exports or the handle: the export link names are the NIDs', the other
 * names are ours, from what the code does (the handle's objects carry Sony's names,
 * "SceAvcdecPlayer%08x" and the others).
 */
#ifndef AVCDEC_FOR_PLAYER_H
#define AVCDEC_FOR_PLAYER_H

#include <scetypes.h>
#include <avcdec.h>
#include <kernel/threadmgr.h>
#include <avcodec/videodec_user.h>

/*
 * The module's errors: the decoder's (SCE_AVCDEC_ERROR_*, 0x806200xx) with 0x4000 added,
 * and three of its own.
 */
#define SCE_AVCDEC_PLAYER_ERROR_ALREADY_USED		(-2141044728)	/* 0x80624008 */
#define SCE_AVCDEC_PLAYER_ERROR_INVALID_POINTER		(-2141044727)	/* 0x80624009 */
#define SCE_AVCDEC_PLAYER_ERROR_NOT_INITIALIZE		(-2141044724)	/* 0x8062400C */
#define SCE_AVCDEC_PLAYER_ERROR_INVALID_ARGUMENT_SIZE	(-2141044722)	/* 0x8062400E */
#define SCE_AVCDEC_PLAYER_ERROR_CANCELED		(-2141044639)	/* 0x80624061 */
#define SCE_AVCDEC_PLAYER_ERROR_STOPPED			(-2141044638)	/* 0x80624062 */
#define SCE_AVCDEC_PLAYER_ERROR_FLUSHED			(-2141044637)	/* 0x80624063 */

/* SceAvcdecPlayer.magic while the player is created */
#define SCE_AVCDEC_PLAYER_MAGIC		0x4144504C

/* SceAvcdecPlayer.state */
#define SCE_AVCDEC_PLAYER_STATE_STOPPED		0x00000001	/* stopped, not restarted yet */
#define SCE_AVCDEC_PLAYER_STATE_FLUSHED		0x00000002	/* flushed, not restarted yet */
#define SCE_AVCDEC_PLAYER_STATE_RESTART		0x00000010	/* the next decode restarts */
#define SCE_AVCDEC_PLAYER_STATE_DECODING	0x00000100	/* a decode call runs */
#define SCE_AVCDEC_PLAYER_STATE_GETTING_PICTURE	0x00000200	/* a get-picture call runs */

/* the decode and get-picture event flags' bits */
#define SCE_AVCDEC_PLAYER_EVF_AU_READY		0x00000001	/* the decoder takes an AU */
#define SCE_AVCDEC_PLAYER_EVF_AU_CANCEL		0x00000002
#define SCE_AVCDEC_PLAYER_EVF_NAL_AU_READY	0x00000100	/* the decoder takes a NAL AU */
#define SCE_AVCDEC_PLAYER_EVF_NAL_AU_CANCEL	0x00000200
#define SCE_AVCDEC_PLAYER_EVF_PICTURE_READY	0x00010000	/* a picture may be ready */
#define SCE_AVCDEC_PLAYER_EVF_PICTURE_CANCEL	0x00020000
#define SCE_AVCDEC_PLAYER_EVF_STOP		0x01000000
#define SCE_AVCDEC_PLAYER_EVF_FLUSH		0x02000000

/* the callback thread's event flag */
#define SCE_AVCDEC_PLAYER_THREAD_READY		0x00000001	/* its callbacks are created */
#define SCE_AVCDEC_PLAYER_THREAD_ERROR		0x00000002	/* see callbackThreadError */
#define SCE_AVCDEC_PLAYER_THREAD_EXIT		0x00010000	/* the thread is to end */

/* the decoder events each callback is registered for */
#define SCE_AVCDEC_PLAYER_PICTURE_EVENTS	0x00000002
#define SCE_AVCDEC_PLAYER_OTHER_EVENTS		0x00003B00

#define SCE_AVCDEC_PLAYER_THREAD_STACK_SIZE	0x1000

/* the handle, allocated by the caller, size set and the rest left to the create call */
typedef struct SceAvcdecPlayer {
	SceUInt32 size;				/* 0x00: sizeof(SceAvcdecPlayer) */
	SceAvcdecCtrl ctrl;			/* 0x04 */
	SceUInt32 cpuAffinityMask;		/* 0x10: the callback thread's, set by the caller */
	SceInt32 priority;			/* 0x14: the callback thread's, set by the caller */
	SceUInt32 magic;			/* 0x18: SCE_AVCDEC_PLAYER_MAGIC */
	SceUID decodeAuEventFlag;		/* 0x1C */
	SceUID getPictureEventFlag;		/* 0x20 */
	SceUID callbackEventFlag;		/* 0x24 */
	SceInt32 callbackThreadError;		/* 0x28 */
	SceUID pictureCallback;			/* 0x2C: SCE_AVCDEC_PLAYER_PICTURE_EVENTS */
	SceUID eventCallback;			/* 0x30: SCE_AVCDEC_PLAYER_OTHER_EVENTS */
	SceUID callbackThread;			/* 0x34 */
	SceInt32 decodedPictures;		/* 0x38: decoded, not got yet */
	SceUInt32 state;			/* 0x3C: SCE_AVCDEC_PLAYER_STATE_* */
	SceKernelLwMutexWork mutex;		/* 0x40: the handle's */
	SceKernelLwMutexWork paramMutex;	/* 0x60: decodedPictures' */
	SceUInt8 reserved[16];			/* 0x80 */
} SceAvcdecPlayer;

_Static_assert(sizeof(SceAvcdecPlayer) == 144, "SceAvcdecPlayer size");

/* what the get-picture call reports besides the pictures (optional) */
typedef struct SceAvcdecPlayerPictureInfo {
	SceUInt32 size;				/* sizeof(SceAvcdecPlayerPictureInfo) */
	SceInt32 decodedPictures;		/* decoded, not got yet */
} SceAvcdecPlayerPictureInfo;

/* the module's own vsnprintf over sceClibVsnprintf, linked last (avcdec_for_player.c) */
int formatString(char *destination, SceSize size, const char *format, ...);

/* SceThreadmgrCoredumpTime's sceKernelExitThread */
int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

int SceAvcdecForPlayer_3C1FFE1F(SceUInt32 codecType, const SceAvcdecQueryDecoderInfo *pQuery,
				SceAvcdecDecoderInfo *pDecoderInfo);
int SceAvcdecForPlayer_DD3E7360(SceUInt32 codecType, const SceAvcdecQueryDecoderInfo *pQuery,
				SceAvcdecDecoderInfo *pDecoderInfo);
int SceAvcdecForPlayer_B2EBD4EE(SceUInt32 codecType, SceAvcdecPlayer *player,
				const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int SceAvcdecForPlayer_C2569BD6(SceUInt32 codecType, SceAvcdecPlayer *player,
				const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int SceAvcdecForPlayer_58335367(SceAvcdecPlayer *player);
int SceAvcdecForPlayer_291780A0(SceAvcdecPlayer *player, const VeneziaVipVideoInput *pAu);
int SceAvcdecForPlayer_EB436C3B(SceAvcdecPlayer *player, const SceAvcdecAu *pAu);
int SceAvcdecForPlayer_44C56742(SceAvcdecPlayer *player, SceAvcdecArrayPicture *pArrayPicture,
				SceUInt32 *pState, SceAvcdecPlayerPictureInfo *pInfo);
int SceAvcdecForPlayer_32A8BBA9(SceAvcdecPlayer *player);
int SceAvcdecForPlayer_7A3AB6D6(SceAvcdecPlayer *player);
int SceAvcdecForPlayer_8D489544(SceAvcdecPlayer *player);
int SceAvcdecForPlayer_4904E8C9(SceAvcdecPlayer *player);
int SceAvcdecForPlayer_B6BAAF04(SceAvcdecPlayer *player);

#endif
