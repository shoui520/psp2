#ifndef _DOLCESDK_LIBAES_H_
#define _DOLCESDK_LIBAES_H_

#include <scetypes.h>

#define SCE_AES_ERROR_INVALID_POINTER		-2142732032	/* 0x80488100 */
#define SCE_AES_ERROR_INVALID_BLOCK_SIZE	-2142732031	/* 0x80488101 */
#define SCE_AES_ERROR_INVALID_KEY_SIZE		-2142732030	/* 0x80488102 */

typedef struct SceAesContext {
	SceUInt32 size;
	SceUInt32 keyWords;
	SceUInt32 blockWords;
	SceUInt32 rounds;
	SceUInt32 roundKeys[120];
	SceUInt32 decryptionRoundKeys[120];
} SceAesContext;

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif	/* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */

int sceAesScheduleKey(SceAesContext *work, SceUInt32 blockBitSize, SceUInt32 keyBitSize, const void *key);
int sceAesEncrypt(const SceAesContext *work, const void *src, void *dst);
int sceAesDecrypt(const SceAesContext *work, const void *src, void *dst);

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif	/* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */

#endif	/* _DOLCESDK_LIBAES_H_ */
