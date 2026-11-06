/*
 * SceVideodecUser (os0:us/avcodec_us.suprx): the decoder exports Sony's SDK headers do not
 * declare. Each one is avcodec_us's wrapper of the kernel's syscall of the same name with a
 * leading underscore (driver/src/avcodec/videocodec/vnz_videocodec.c), whose definition gives
 * the prototype.
 */
#pragma once

#include <scetypes.h>
#include <avcdec.h>
#include <vnz_wrapper/venezia_debug.h>

#ifdef __cplusplus
extern "C" {
#endif

int sceAvcdecQueryDecoderMemSizeNongameapp(unsigned int codecType,
					   const SceAvcdecQueryDecoderInfo *pQuery,
					   SceAvcdecDecoderInfo *pDecoderInfo);
int sceAvcdecCreateDecoderNongameapp(unsigned int codecType, SceAvcdecCtrl *pCtrl,
				     const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int sceAvcdecDecodeAuInternal(SceAvcdecCtrl *pCtrl, const SceAvcdecAu *pAu,
			      const SceUInt *timeout);
int sceAvcdecDecodeAuNalAuInternal(SceAvcdecCtrl *pCtrl, const VeneziaVipVideoInput *pAu,
				   const SceUInt *timeout);
int sceAvcdecDecodeGetPictureInternal(SceAvcdecCtrl *pCtrl,
				      SceAvcdecArrayPicture *pArrayPicture,
				      const SceUInt *timeout);
int sceAvcdecRegisterCallbackInternal(SceAvcdecCtrl *pCtrl, SceUID cbid,
				      unsigned int eventMask);
int sceAvcdecUnregisterCallbackInternal(SceAvcdecCtrl *pCtrl);

#ifdef __cplusplus
}
#endif
