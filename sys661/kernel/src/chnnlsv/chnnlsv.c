/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/
/**
 * Big thanks to Hykem for most of the info used here.
 * http://www.emunewz.net/forum/showthread.php?tid=3673
 */

#include <common_imp.h>
#include <sysmem_sysclib.h>
#include <sysmem_utils_kernel.h>
#include <threadman_kernel.h>
#include "chnnlsv.h"

SCE_MODULE_INFO("sceChnnlsv", SCE_MODULE_KERNEL | SCE_MODULE_KIRK_SEMAPHORE_LIB | SCE_MODULE_ATTR_EXCLUSIVE_LOAD
                              | SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 4);
SCE_SDK_VERSION(SDK_VERSION);

//Subroutine _SdCrypt - Address 0x00000000
s32 _SdCrypt(void *buf, u8 *data, u32 size, u8 *keySeed, u32 *ivSeed, u32 mode)
{
	SceKirkAesBuf *kirk = (SceKirkAesBuf *)buf;
	s32 ret;
	u32 scramble_code;
	u8 block1[16], block2[16];
	u32 i, j;

	for (i = 0; i < 16; i++)
		kirk->hdr.payload[i] = keySeed[i];

	if (mode == 6) {
		scramble_code = 100;

		for (i = 0; i < 16; i++)
			kirk->hdr.payload[i] ^= SceSdKey6[i];

		ret = _kirk8(buf, 16);

		for (i = 0; i < 16; i++)
			kirk->raw[i] ^= SceSdKey5[i];
	} else if (mode == 4) {
		scramble_code = 87;

		for (i = 0; i < 16; i++)
			kirk->hdr.payload[i] ^= SceSdKey3[i];

		ret = _kirk8(buf, 16);

		for (i = 0; i < 16; i++)
			kirk->raw[i] ^= SceSdKey2[i];
	} else if (mode == 2) {
		ret = _kirk8(buf, 16);

		scramble_code = 83;
	} else if (mode == 1) {
		ret = _kirk7(buf, 16, 4);

		scramble_code = 83;
	} else if (mode == 3) {
		scramble_code = 87;

		for (i = 0; i < 16; i++)
			kirk->hdr.payload[i] ^= SceSdKey3[i];

		ret = _kirk7(buf, 16, 14);

		for (i = 0; i < 16; i++)
			kirk->raw[i] ^= SceSdKey2[i];
	} else {
		scramble_code = 100;

		for (i = 0; i < 16; i++)
			kirk->hdr.payload[i] ^= SceSdKey6[i];

		ret = _kirk7(buf, 16, 18);

		for (i = 0; i < 16; i++)
			kirk->raw[i] ^= SceSdKey5[i];
	}

	if (ret == 0) {
		for (i = 0; i < 16; i++)
			block2[i] = kirk->raw[i];

		u32 ivSeedVal = *ivSeed;
		u8 *ivBytes = (u8 *)ivSeed;

		if (ivSeedVal != 1) {
			for (i = 0; i < 12; i++)
				block1[i] = block2[i];

			block1[12] = ivBytes[0] - 1;
			block1[13] = (ivSeedVal - 1) >> 8;
			block1[14] = (ivSeedVal - 1) >> 16;
			block1[15] = (ivSeedVal - 1) >> 24;
		} else {
			for (i = 0; i < 16; i++)
				block1[i] = 0;
		}

		if (size > 0) {
			for (i = 20; i < (size + 20); i += 16) {
				for (j = 0; j < 12; j++)
					kirk->raw[i + j] = block2[j];

				kirk->raw[i + 12] = ivBytes[0];
				kirk->raw[i + 13] = ivBytes[1];
				kirk->raw[i + 14] = ivBytes[2];
				kirk->raw[i + 15] = ivBytes[3];
				(*ivSeed)++;
			}
		}

		ret = _kirk7Xor(buf, size, block1, scramble_code);

		if (ret == 0) {
			if (ret < (s32)size) {
				for (i = 0; i < size; i++)
					data[i] ^= kirk->raw[i];
			}
		}
	}

	return ret;
}

//Subroutine module_start - Address 0x00000418
s32 module_start(SceSize args __attribute__((unused)), void *argp __attribute__((unused)))
{
	s32 ret = 1;

	g_chnnlsv_sema1 = sceKernelCreateSema("SceChnnlsv1", 0, 1, 1, NULL);

	if (g_chnnlsv_sema1 > 0) {
		g_chnnlsv_sema2 = sceKernelCreateSema("SceChnnlsv2", 0, 1, 1, NULL);

		if (g_chnnlsv_sema2 < 1)
			sceKernelDeleteSema(g_chnnlsv_sema1);
		else
			ret = SCE_ERROR_OK;
	}

	return ret;
}

//Subroutine module_stop - Address 0x000004A4
s32 module_stop(SceSize args __attribute__((unused)), void *argp __attribute__((unused)))
{
	s32 ret = 1;

	SceUInt timeout = 1000000;

	if (sceKernelWaitSema(g_chnnlsv_sema1, 1, &timeout) == 0) {
		sceKernelDeleteSema(g_chnnlsv_sema1);

		if (sceKernelWaitSema(g_chnnlsv_sema2, 1, &timeout) == 0) {
			sceKernelDeleteSema(g_chnnlsv_sema2);

			ret = SCE_ERROR_OK;
		}
	}

	return ret;
}

/**
 * Initialize the SceSdHashCtx struct and set the mode.
 */
//Subroutine sceSdSetIndex - Address 0x00000528
s32 sceSdSetIndex(SceSdHashCtx *ctx, s32 mode)
{
	s32 oldK1 = pspShiftK1();
	s32 ret = SCE_CHNNLSV_ERROR_ILLEGAL_ADDR;
	u32 i;

	if (pspK1StaBufOk(ctx, sizeof(SceSdHashCtx))) {
		ctx->mode = mode;

		for (i = 0; i < 16; i++)
			ctx->data[i] = 0;

		for (i = 0; i < 16; i++)
			ctx->key[i] = 0;

		ctx->size = 0;

		ret = SCE_ERROR_OK;
	}

	pspSetK1(oldK1);

	return ret;
}

/**
 * Generates a hash storing the result into ctx->data and updates ctx->key
 */
//Subroutine sceSdRemoveValue - Address 0x0000058C
s32 sceSdRemoveValue(SceSdHashCtx *ctx, u8 *data, u32 size)
{
	s32 oldK1 = pspShiftK1();
	s32 ret = SCE_CHNNLSV_ERROR_ILLEGAL_ADDR;
	u32 scramble_code;
	u32 i;

	if (pspK1DynBufOk(data, size) && pspK1StaBufOk(ctx, sizeof(SceSdHashCtx))) {
		if (sceKernelWaitSema(g_chnnlsv_sema1, 1, NULL) == 0) {
			if (ctx->size < 17) {
				if ((ctx->size + size) < 17) {
					if (size != 0) {
						for (i = 0; i < size; i++)
							ctx->key[ctx->size + i] = data[i];
					}

					ctx->size += size;
					ret = SCE_ERROR_OK;
				} else {
					if (ctx->mode == 6)
						scramble_code = 17;
					else if (ctx->mode == 4)
						scramble_code = 13;
					else if (ctx->mode == 2)
						scramble_code = 5;
					else if (ctx->mode == 1)
						scramble_code = 3;
					else {
						scramble_code = 12;

						if ((ctx->mode ^ 3) != 0)
							scramble_code = 16;
					}

					if (ctx->size != 0) {
						for (i = 0; i < ctx->size; i++)
							g_chnnlsv.hash_body[i] = ctx->key[i];
					}

					u32 oldsize = ctx->size;
					u32 tempsize = (ctx->size + size) & 0xF;
					tempsize = (tempsize == 0) ? 16 : tempsize;
					ctx->size = tempsize;
					u32 newsize = size - tempsize;

					if (tempsize != 0) {
						for (i = 0; i < tempsize; i++)
							ctx->key[i] = data[newsize + i];
					}

					ret = SCE_ERROR_OK;

					if (newsize != 0) {
						for (i = 0; i < newsize; i++) {
							if (oldsize == 2048) {
								ret = _kirk4Xor(g_chnnlsv.hash_hdr, 2048, ctx->data, scramble_code);

								if (ret != 0)
									break;

								oldsize = 0;
							}

							g_chnnlsv.hash_body[oldsize] = data[i];
							oldsize++;
						}
					}

					if (ret == 0 && oldsize != 0)
						ret = _kirk4Xor(g_chnnlsv.hash_hdr, oldsize, ctx->data, scramble_code);
				}
			} else
				ret = SCE_CHNNLSV_ERROR_ILLEGAL_SIZE;

			{
				u32 clear_size = (size < 0x7F1) ? (size + 0x24) : 0x814;
				memset(g_chnnlsv.hash_hdr, 0, clear_size);
				sceKernelDcacheWritebackRange(g_chnnlsv.hash_hdr, clear_size);
			}
			if (sceKernelSignalSema(g_chnnlsv_sema1, 1) != 0)
				ret = SCE_CHNNLSV_ERROR_SEMA_ERROR;
		} else
			ret = SCE_CHNNLSV_ERROR_SEMA_ERROR;
	}

	pspSetK1(oldK1);

	return ret;
}

/**
 * Generates a hash based on the context collected by sceSdRemoveValue
 */
//Subroutine sceSdGetLastIndex - Address 0x00000840
s32 sceSdGetLastIndex(SceSdHashCtx *ctx, u8 *hash, u8 *key)
{
	s32 oldK1 = pspShiftK1();
	s32 ret = SCE_CHNNLSV_ERROR_ILLEGAL_ADDR;
	u8 block1[16], block2[16];
	u8 xorbyte, shiftbyte1, shiftbyte2;
	u32 scramble_code;
	u32 i;

	if (pspK1StaBufOk(hash, 16) && pspK1StaBufOk(key, 16) && pspK1StaBufOk(ctx, sizeof(SceSdHashCtx))) {
		if (sceKernelWaitSema(g_chnnlsv_sema1, 1, NULL) == 0) {
			if (ctx->size < 17) {
				if (ctx->mode == 6)
					scramble_code = 17;
				else if (ctx->mode == 4)
					scramble_code = 13;
				else if (ctx->mode == 2)
					scramble_code = 5;
				else if (ctx->mode == 1)
					scramble_code = 3;
				else {
					scramble_code = 12;

					if ((ctx->mode ^ 3) != 0)
						scramble_code = 16;
				}

				for (i = 0; i < 16; i++)
					g_chnnlsv.hash_body[i] = 0;

				ret = _kirk4(g_chnnlsv.hash_hdr, 16, scramble_code);

				if (ret == 0) {
					for (i = 0; i < 16; i++)
						block1[i] = g_chnnlsv.hash_body[i];

					xorbyte = (block1[0] & 0x80) ? 135 : 0;

					for (i = 0; i < 15; i++) {
						shiftbyte1 = block1[i] << 1;
						shiftbyte2 = block1[i + 1] >> 7;
						block1[i] = shiftbyte1 | shiftbyte2;
					}

					shiftbyte1 = block1[15] << 1;
					block1[15] = shiftbyte1 ^ xorbyte;

					if (ctx->size != 16) {
						xorbyte = (block1[0] & 0x80) ? 135 : 0;

						for (i = 0; i < 15; i++) {
							shiftbyte1 = block1[i] << 1;
							shiftbyte2 = block1[i + 1] >> 7;
							block1[i] = shiftbyte1 | shiftbyte2;
						}

						shiftbyte1 = block1[15] << 1;
						block1[15] = shiftbyte1 ^ xorbyte;

						ctx->key[ctx->size] = -128;

						if ((ctx->size + 1) < 16) {
							for (i = (ctx->size + 1); i < 16; i++)
								ctx->key[i] = 0;
						}
					}

					for (i = 0; i < 16; i++)
						ctx->key[i] ^= block1[i];

					for (i = 0; i < 16; i++)
						g_chnnlsv.hash_body[i] = ctx->key[i];

					for (i = 0; i < 16; i++)
						block2[i] = ctx->data[i];

					ret = _kirk4Xor(g_chnnlsv.hash_hdr, 16, block2, scramble_code);

					if (ret == 0) {
						if ((ctx->mode - 3) < 2) {
							for (i = 0; i < 16; i++)
								block2[i] ^= SceSdKey1[i];
						} else if ((ctx->mode - 5) < 2) {
							for (i = 0; i < 16; i++)
								block2[i] ^= SceSdKey4[i];
						}

						u32 xor1 = (ctx->mode ^ 2) < 1;
						u32 xor2 = ((ctx->mode ^ 4) < 1) | xor1;

						if ((xor2 != 0) || (ctx->mode == 6)) {
							for (i = 0; i < 16; i++)
								g_chnnlsv.hash_body[i] = block2[i];

							ret = _kirk5(g_chnnlsv.hash_hdr, 16);

							if (ret == 0)
								ret = _kirk4(g_chnnlsv.hash_hdr, 16, scramble_code);

							if (ret == 0) {
								for (i = 0; i < 16; i++)
									block2[i] = g_chnnlsv.hash_body[i];
							}
						}

						if (ret == 0 && key != 0) {
							for (i = 0; i < 16; i++)
								block2[i] ^= key[i];

							for (i = 0; i < 16; i++)
								g_chnnlsv.hash_body[i] = block2[i];

							ret = _kirk4(g_chnnlsv.hash_hdr, 16, scramble_code);

							if (ret == 0) {
								for (i = 0; i < 16; i++)
									block2[i] = g_chnnlsv.hash_body[i];
							}
						}

						if (ret == 0) {
							for (i = 0; i < 16; i++)
								hash[i] = block2[i];

							for (i = 0; i < 16; i++)
								ctx->data[i] = 0;

							for (i = 0; i < 16; i++)
								ctx->key[i] = 0;

							ctx->size = 0;
							ctx->mode = 0;
							ret = SCE_ERROR_OK;
						}
					}
				}
			} else
				ret = SCE_CHNNLSV_ERROR_ILLEGAL_SIZE;

			memset(g_chnnlsv.hash_hdr, 0, 0x24);
			sceKernelDcacheWritebackRange(g_chnnlsv.hash_hdr, 0x24);
			if (sceKernelSignalSema(g_chnnlsv_sema1, 1) != 0)
				ret = SCE_CHNNLSV_ERROR_SEMA_ERROR;
		} else
			ret = SCE_CHNNLSV_ERROR_SEMA_ERROR;
	}

	pspSetK1(oldK1);

	return ret;
}

/**
 * The main key generating function, 1 for encryption, 2 for decryption
 */
//Subroutine sceSdCreateList - Address 0x00000D9C
s32 sceSdCreateList(SceSdCryptCtx *ctx, int mode, int genMode, u8 *data, u8 *key)
{
	s32 oldK1 = pspShiftK1();
	s32 ret = SCE_CHNNLSV_ERROR_ILLEGAL_ADDR;
	u32 i;

	if (pspK1StaBufOk(data, 16) && pspK1StaBufOk(key, 16) && pspK1StaBufOk(ctx, sizeof(SceSdCryptCtx))) {
		if (sceKernelWaitSema(g_chnnlsv_sema2, 1, NULL) == 0) {
			ctx->mode = mode;
			ctx->ivSeed = 1;

			if (genMode == 2) {
				for (i = 0; i < 16; i++)
					ctx->data[i] = data[i];

				if (key != 0) {
					for (i = 0; i < 16; i++)
						ctx->data[i] ^= key[i];
				}

				ret = SCE_ERROR_OK;
			} else if (genMode == 1) {
				ret = _kirk14(g_chnnlsv.crypt_hdr);

				if (ret == 0) {
					for (i = 15; (int)i >= 0; i--)
						g_chnnlsv.crypt_body[i] = g_chnnlsv.crypt_hdr[i];

					for (i = 12; i < 16; i++)
						g_chnnlsv.crypt_body[i] = 0;

					if (ctx->mode == 6) {
						for (i = 0; i < 16; i++)
							g_chnnlsv.crypt_body[i] ^= SceSdKey5[i];

						ret = _kirk5(g_chnnlsv.crypt_hdr, 16);

						for (i = 0; i < 16; i++)
							g_chnnlsv.crypt_body[i] ^= SceSdKey6[i];
					} else if (ctx->mode == 4) {
						for (i = 0; i < 16; i++)
							g_chnnlsv.crypt_body[i] ^= SceSdKey2[i];

						ret = _kirk5(g_chnnlsv.crypt_hdr, 16);

						for (i = 0; i < 16; i++)
							g_chnnlsv.crypt_body[i] ^= SceSdKey3[i];
					} else if (ctx->mode == 2) {
						ret = _kirk5(g_chnnlsv.crypt_hdr, 16);
					} else if (ctx->mode == 1) {
						ret = _kirk4(g_chnnlsv.crypt_hdr, 16, 4);
					} else if (ctx->mode == 3) {
						for (i = 0; i < 16; i++)
							g_chnnlsv.crypt_body[i] ^= SceSdKey2[i];

						ret = _kirk4(g_chnnlsv.crypt_hdr, 16, 14);

						for (i = 0; i < 16; i++)
							g_chnnlsv.crypt_body[i] ^= SceSdKey3[i];
					} else {
						for (i = 0; i < 16; i++)
							g_chnnlsv.crypt_body[i] ^= SceSdKey5[i];

						ret = _kirk4(g_chnnlsv.crypt_hdr, 16, 18);

						for (i = 0; i < 16; i++)
							g_chnnlsv.crypt_body[i] ^= SceSdKey6[i];
					}

					if (ret == 0) {
						for (i = 0; i < 16; i++)
							ctx->data[i] = g_chnnlsv.crypt_body[i];

						for (i = 0; i < 16; i++)
							data[i] = g_chnnlsv.crypt_body[i];

						if (key != 0) {
							for (i = 0; i < 16; i++)
								ctx->data[i] ^= key[i];
						}
					}
				}
			} else
				ret = SCE_ERROR_OK;

			memset(g_chnnlsv.crypt_hdr, 0, 0x24);
			sceKernelDcacheWritebackRange(g_chnnlsv.crypt_hdr, 0x24);
			if (sceKernelSignalSema(g_chnnlsv_sema2, 1) != 0)
				ret = SCE_CHNNLSV_ERROR_SEMA_ERROR;
		} else
			ret = SCE_CHNNLSV_ERROR_SEMA_ERROR;
	}

	pspSetK1(oldK1);

	return ret;
}

/**
 * The main encryption/decryption function, the size of "data" must be 16 byte aligned
 */
//Subroutine sceSdSetMember - Address 0x00001268
s32 sceSdSetMember(SceSdCryptCtx *ctx, u8 *data, u32 size)
{
	s32 oldK1 = pspShiftK1();
	s32 ret = SCE_CHNNLSV_ERROR_ILLEGAL_ADDR;

	if (pspK1DynBufOk(data, size) && pspK1StaBufOk(ctx, sizeof(SceSdCryptCtx))) {
		if (sceKernelWaitSema(g_chnnlsv_sema2, 1, NULL) == 0) {
			if (size != 0) {
				if ((size & 0xF) == 0) {
					u32 length = 0;
					ret = SCE_ERROR_OK;

					while (size >= 2048) {
						ret = _SdCrypt(g_chnnlsv.crypt_hdr, data + length, 2048, ctx->data, &ctx->ivSeed, ctx->mode);

						size -= 2048;
						length += 2048;

						if (ret != 0)
							break;
					}

					if (ret == 0 && size != 0)
						ret = _SdCrypt(g_chnnlsv.crypt_hdr, data + length, size, ctx->data, &ctx->ivSeed, ctx->mode);
				} else
					ret = SCE_CHNNLSV_ERROR_ILLEGAL_ALIGNMENT_SIZE;
			} else
				ret = SCE_ERROR_OK;

			{
				u32 clear_size = (size < 0x801) ? (size + 0x14) : 0x814;
				memset(g_chnnlsv.crypt_hdr, 0, clear_size);
				sceKernelDcacheWritebackRange(g_chnnlsv.crypt_hdr, clear_size);
			}
			if (sceKernelSignalSema(g_chnnlsv_sema2, 1) != 0)
				ret = SCE_CHNNLSV_ERROR_SEMA_ERROR;
		} else
			ret = SCE_CHNNLSV_ERROR_SEMA_ERROR;
	}

	pspSetK1(oldK1);

	return ret;
}

/**
 * Initialize the SceSdCryptCtx struct.
 */
//Subroutine sceSdCleanList - Address 0x00001414
s32 sceSdCleanList(SceSdCryptCtx *ctx)
{
	s32 oldK1 = pspShiftK1();
	s32 ret = SCE_CHNNLSV_ERROR_ILLEGAL_ADDR;
	u32 i;

	if (pspK1StaBufOk(ctx, sizeof(SceSdCryptCtx))) {
		for (i = 0; i < 16; i++)
			ctx->data[i] = 0;

		ctx->ivSeed = 0;
		ctx->mode = 0;
		ret = SCE_ERROR_OK;
	}

	pspSetK1(oldK1);

	return ret;
}

//Subroutine _kirk4 - Address 0x0000145C
s32 _kirk4(void *buf, u32 size, u32 scramble_code)
{
	SceKirkAesBuf *kirk = (SceKirkAesBuf *)buf;

	kirk->hdr.keyslot = scramble_code;
	kirk->hdr.mode = 4;
	kirk->hdr.reservedA = 0;
	kirk->hdr.reservedB = 0;
	kirk->hdr.dataSize = size;

	size += 20;

	s32 ret = SCE_CHNNLSV_ERROR_KIRK_IV_ERROR;

	if (sceUtilsBufferCopyWithRange(buf, size, buf, size, 4) == 0)
		ret = SCE_ERROR_OK;

	return ret;
}

//Subroutine _kirk7 - Address 0x000014AC
s32 _kirk7(void *buf, u32 size, u32 scramble_code)
{
	SceKirkAesBuf *kirk = (SceKirkAesBuf *)buf;

	kirk->hdr.keyslot = scramble_code;
	kirk->hdr.mode = 5;
	kirk->hdr.reservedA = 0;
	kirk->hdr.reservedB = 0;
	kirk->hdr.dataSize = size;

	size += 20;

	s32 ret = SCE_CHNNLSV_ERROR_KIRK_IV_ERROR;

	if (sceUtilsBufferCopyWithRange(buf, size, buf, size, 7) == 0)
		ret = SCE_ERROR_OK;

	return ret;
}

//Subroutine _kirk5 - Address 0x000014FC
s32 _kirk5(void *buf, u32 size)
{
	SceKirkAesBuf *kirk = (SceKirkAesBuf *)buf;

	kirk->hdr.mode = 4;
	kirk->hdr.keyslot = 256;
	kirk->hdr.reservedA = 0;
	kirk->hdr.reservedB = 0;
	kirk->hdr.dataSize = size;

	size += 20;

	s32 ret = SCE_CHNNLSV_ERROR_KIRK_IV_FUSE_ERROR;

	if (sceUtilsBufferCopyWithRange(buf, size, buf, size, 5) == 0)
		ret = SCE_ERROR_OK;

	return ret;
}

//Subroutine _kirk8 - Address 0x00001550
s32 _kirk8(void *buf, u32 size)
{
	SceKirkAesBuf *kirk = (SceKirkAesBuf *)buf;

	kirk->hdr.mode = 5;
	kirk->hdr.keyslot = 256;
	kirk->hdr.reservedA = 0;
	kirk->hdr.reservedB = 0;
	kirk->hdr.dataSize = size;

	size += 20;

	s32 ret = SCE_CHNNLSV_ERROR_KIRK_IV_FUSE_ERROR;

	if (sceUtilsBufferCopyWithRange(buf, size, buf, size, 8) == 0)
		ret = SCE_ERROR_OK;

	return ret;
}

//Subroutine _kirk4Xor - Address 0x000015A4
s32 _kirk4Xor(void *buf, u32 size, u8 *data, u32 scramble_code)
{
	SceKirkAesBuf *kirk = (SceKirkAesBuf *)buf;
	s32 ret;
	u32 i;

	for (i = 0; i < 16; i++)
		kirk->hdr.payload[i] ^= data[i];

	ret = _kirk4(buf, size, scramble_code);

	if (ret != 0)
		return ret;

	for (i = 0; i < 16; i++)
		data[i] = kirk->raw[size + 4 + i];

	return 0;
}

//Subroutine _kirk7Xor - Address 0x00001644
s32 _kirk7Xor(void *buf, u32 size, u8 *data, u32 scramble_code)
{
	SceKirkAesBuf *kirk = (SceKirkAesBuf *)buf;
	s32 ret;
	u32 i;
	u8 block[16];

	for (i = 0; i < 16; i++)
		block[i] = kirk->raw[size + 4 + i];

	ret = _kirk7(buf, size, scramble_code);

	if (ret != 0)
		return ret;

	for (i = 0; i < 16; i++)
		kirk->raw[i] ^= data[i];

	for (i = 0; i < 16; i++)
		data[i] = block[i];

	return 0;
}

//Subroutine _kirk5Xor - Address 0x000016F4
s32 _kirk5Xor(void *buf, u32 size, u8 *data)
{
	SceKirkAesBuf *kirk = (SceKirkAesBuf *)buf;
	s32 ret;
	u32 i;

	for (i = 0; i < 16; i++)
		kirk->hdr.payload[i] ^= data[i];

	ret = _kirk5(buf, size);

	if (ret != 0)
		return ret;

	for (i = 0; i < 16; i++)
		data[i] = kirk->raw[size + 4 + i];

	return 0;
}

//Subroutine _kirk8Xor - Address 0x00001790
s32 _kirk8Xor(void *buf, u32 size, u8 *data)
{
	SceKirkAesBuf *kirk = (SceKirkAesBuf *)buf;
	s32 ret;
	u32 i;
	u8 block[16];

	for (i = 0; i < 16; i++)
		block[i] = kirk->raw[size + 4 + i];

	ret = _kirk8(buf, size);

	if (ret != 0)
		return ret;

	for (i = 0; i < 16; i++)
		kirk->raw[i] ^= data[i];

	for (i = 0; i < 16; i++)
		data[i] = block[i];

	return 0;
}

//Subroutine _kirk14 - Address 0x0000183C
s32 _kirk14(void *buf)
{
	s32 ret = SCE_CHNNLSV_ERROR_KIRK_14_ERROR;

	if (sceUtilsBufferCopyWithRange(buf, 20, 0, 0, 14) == 0)
		ret = SCE_ERROR_OK;

	return ret;
}
