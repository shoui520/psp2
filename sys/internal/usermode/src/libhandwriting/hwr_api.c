/*
 * hwr_api.c: SceHandwriting module lifecycle, dictionary loader, and public API exports.
 */
#include "hwr_internal.h"

const char g_hwrLicenseKeyInput[17] = {
	(char)0x92, (char)0x38, (char)0x0b, (char)0x2e, (char)0x16, (char)0xe3, (char)0x44, (char)0xef, (char)0x9a,
	(char)0x17, (char)0x1e, (char)0x4b, (char)0xf6, (char)0x34, (char)0xb8, (char)0xda, (char)0x00,
};
#include "deflt.h"
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>

#define HWR_DIC_COUNT			24u
#define HWR_MIN_FRAME_DIM		50u
#define HWR_MAX_FRAME_DIM		127u
#define HWR_GZIP_CHUNK_SIZE		0x2000u
#define HWR_SDK_VALIDATE_VER		0x01650000u

typedef struct HwrDicDesc {
	const char *filename;
	uint32_t bufferSize;
} HwrDicDesc;

typedef struct HwrIoReadContext {
	SceUID fd;
	void *readBuf;
} HwrIoReadContext;

static const HwrDicDesc s_dicTable[HWR_DIC_COUNT] = {
	{ "alnum_u.bin.hwz",			0x0000e500u },
	{ "alnum_native_u.bin.hwz",		0x0000e780u },
	{ "hira_u.bin.hwz",			0x0000e800u },
	{ "kata_u.bin.hwz",			0x0000dd80u },
	{ "kanji_u.bin.hwz",			0x000b2900u },
	{ "kanji2_u.bin.hwz",			0x000f7600u },
	{ "all_u.bin.hwz",			0x000d3200u },
	{ "all2_u.bin.hwz",			0x00117f00u },
	{ "alnumlat1_u.bin.hwz",		0x00011d80u },
	{ "korean_u.bin.hwz",			0x00135600u },
	{ "hangul_u.bin.hwz",			0x00077e40u },
	{ "german_u.bin.hwz",			0x0000ed40u },
	{ "dutch_u.bin.hwz",			0x0000fbc0u },
	{ "french_u.bin.hwz",			0x00010140u },
	{ "italian_u.bin.hwz",			0x0000f180u },
	{ "spanish_u.bin.hwz",			0x0000f580u },
	{ "portuguese_u.bin.hwz",		0x0000fc80u },
	{ "russian_u.bin.hwz",			0x00011340u },
	{ "dan_nor_u.bin.hwz",			0x0000ec80u },
	{ "swe_fin_u.bin.hwz",			0x0000ed40u },
	{ "icelandic_u.bin.hwz",		0x0000fa40u },
	{ "polish_u.bin.hwz",			0x0000f040u },
	{ "chinese_simplified1_u.bin.hwz",	0x00156880u },
	{ "chinese_traditional_u.bin.hwz",	0x002bf2c0u },
};

static uint8_t s_initialized;
static uint8_t s_moduleStarted;
static uint8_t s_registerInitialized;
uint16_t g_hwrApiRegisterCount;
static void *s_dicBufferAligned;
void *g_hwrApiRegBuffer;
static HwrWorkBuffer *s_workBufferAligned;
static uint32_t s_processSdkVersion;
static SceKernelLwMutexWork s_mutex;

void hwrModuleInit(void)
{
	if (!s_moduleStarted) {
		if (sceKernelCreateLwMutex(&s_mutex, "SceHandwritingMutex", 0, 0, NULL) >= 0) {
			hwrMspaceCreate();
			s_processSdkVersion = SceLibKernel_9F793F84();
			s_moduleStarted = 1;
		}
	}
}

void hwrModuleTerm(void)
{
	if (s_moduleStarted) {
		sceKernelDeleteLwMutex(&s_mutex);
		hwrMspaceDestroy();
		s_moduleStarted = 0;
	}
}

int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	hwrModuleInit();
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	hwrModuleTerm();
	return 0;
}

static const void *hwrDeflateRefill(SceDeflatePartialInputParam *param,
				    unsigned int outputsize)
{
	HwrIoReadContext *ioCtx = (HwrIoReadContext *)param->cookie;

	(void)outputsize;
	if (sceIoRead(ioCtx->fd, ioCtx->readBuf, HWR_GZIP_CHUNK_SIZE) < 0)
		return NULL;
	param->pBufEnd = (const uint8_t *)ioCtx->readBuf + HWR_GZIP_CHUNK_SIZE;
	return ioCtx->readBuf;
}

SceInt32 sceHandwritingGetBufferSize(SceUInt32 dic)
{
	if (dic < HWR_DIC_COUNT)
		return (SceInt32)s_dicTable[dic].bufferSize;
	return SCE_HANDWRITING_ERROR_INVALID_PARAM;
}

SceInt32 sceHandwritingInit(SceUChar8 maxX, SceUChar8 maxY,
			    SceUInt32 dic, ScePVoid dicBuffer)
{
	char path[128];
	SceDeflatePartialInputParam deflateParam;
	HwrIoReadContext ioCtx;
	uintptr_t dicAligned;
	uintptr_t workAligned;
	SceUID fd;
	SceSSize bytesRead;
	const void *compressed;
	int status;
	char *dst;
	const char *src;

	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (dicBuffer == NULL) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_POINTER;
	}
	if (dic >= HWR_DIC_COUNT || maxX < HWR_MIN_FRAME_DIM || maxX > HWR_MAX_FRAME_DIM ||
	    maxY < HWR_MIN_FRAME_DIM || maxY > HWR_MAX_FRAME_DIM) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_PARAM;
	}
	if (s_initialized) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_ALREADY_INITIALIZED;
	}
	if (hwrSetLicenseKey(g_hwrLicenseKeyInput) != 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}

	dicAligned = ((uintptr_t)dicBuffer + 63u) & ~(uintptr_t)63u;
	workAligned = ((uintptr_t)dicBuffer + s_dicTable[dic].bufferSize - HWR_GZIP_CHUNK_SIZE) &
		      ~(uintptr_t)63u;
	s_dicBufferAligned = (void *)dicAligned;
	s_workBufferAligned = (HwrWorkBuffer *)workAligned;

	{
		char ch;

		dst = path;
		src = "sa0:data/libhwr/";
		do {
			ch = *src++;
			*dst++ = ch;
		} while (ch != '\0');

		dst = path;
		if (*dst != '\0') {
			do {
				ch = *++dst;
			} while (ch != '\0');
		}
		src = s_dicTable[dic].filename;
		do {
			ch = *src++;
			*dst++ = ch;
		} while (ch != '\0');
	}

	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd <= 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_NOT_OPENED;
	}

	bytesRead = sceIoRead(fd, (void *)workAligned, HWR_GZIP_CHUNK_SIZE);
	if (bytesRead < 0) {
		sceIoClose(fd);
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_READ;
	}
	compressed = sceGzipGetCompressedData((const void *)workAligned);
	if (compressed == NULL) {
		sceIoClose(fd);
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_READ;
	}

	{
		const uint8_t *readBytes = (const uint8_t *)workAligned;

		ioCtx.fd = fd;
		ioCtx.readBuf = (void *)workAligned;
		deflateParam.size = sizeof(SceDeflatePartialInputParam);
		deflateParam.pBufEnd = readBytes + bytesRead;
		deflateParam.cookie = &ioCtx;
		deflateParam.callback = hwrDeflateRefill;
	}

	status = sceDeflateDecompressPartial((void *)dicAligned,
					     (unsigned int)(workAligned - dicAligned),
					     compressed, NULL, &deflateParam);
	sceIoClose(fd);
	if (status < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return status;
	}

	if (hwrEngineInit(maxX, maxY, s_dicBufferAligned) != 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}

	s_registerInitialized = 0;
	s_initialized = 1;
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return 0;
}

SceInt32 sceHandwritingTerm(void)
{
	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (s_initialized) {
		if (hwrEngineTerm() != 0) {
			sceKernelUnlockLwMutex(&s_mutex, 1);
			return SCE_HANDWRITING_ERROR_INTERNAL;
		}
		s_initialized = 0;
		s_registerInitialized = 0;
	}
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return 0;
}

SceInt32 sceHandwritingRecognize(SceHandwritingData *data)
{
	HwrWorkBuffer *work;
	int count;
	int idx;

	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (!s_initialized) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_NOT_INITIALIZED;
	}
	if (data == NULL) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_POINTER;
	}

	if (s_processSdkVersion >= HWR_SDK_VALIDATE_VER) {
		int ptIdx = 0;

		while (ptIdx < SCE_HANDWRITING_MAX_POINTS) {
			if (data->points[ptIdx].x == 255u && data->points[ptIdx].y == 255u)
				break;
			ptIdx++;
		}
		if (ptIdx == SCE_HANDWRITING_MAX_POINTS || ptIdx == 0 ||
		    (data->points[ptIdx - 1].x & 0x80u) != 0) {
			sceKernelUnlockLwMutex(&s_mutex, 1);
			return SCE_HANDWRITING_ERROR_INVALID_PARAM;
		}
	}

	work = s_workBufferAligned;
	sceClibMemcpy_safe(work->pointBytes, data->points, sizeof(work->pointBytes));
	count = hwrEngineRecognize(work);
	if (count < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (count > SCE_HANDWRITING_MAX_CANDIDATES)
		count = SCE_HANDWRITING_MAX_CANDIDATES;
	for (idx = 0; idx < count; idx++)
		data->codes[idx] = work->codes[idx];
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return count;
}

SceInt32 sceHandwritingSetMode(SceUInt32 mode)
{
	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (!s_initialized) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_NOT_INITIALIZED;
	}
	if ((mode & ~(uint32_t)SCE_HANDWRITING_MODE_ALL) != 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_PARAM;
	}
	if (hwrEngineSetMode((uint16_t)mode) != 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return 0;
}

SceInt32 sceHandwritingRegisterInit(SceUInt32 count, ScePVoid regBuffer)
{
	int res;

	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (!s_initialized) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_NOT_INITIALIZED;
	}
	if (regBuffer == NULL) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_POINTER;
	}
	if (count == 0 || count > SCE_HANDWRITING_MAX_REGISTER_COUNT) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_PARAM;
	}

	res = hwrUserRegInit((uint16_t)count, (HwrUserEntry *)regBuffer);
	if (res != 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		if (res == -301)
			return SCE_HANDWRITING_ERROR_MEMORY_ERROR;
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	g_hwrApiRegBuffer = regBuffer;
	g_hwrApiRegisterCount = (uint16_t)count;
	s_registerInitialized = 1;
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return 0;
}

SceInt32 sceHandwritingRegisterTerm(void)
{
	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (!s_initialized) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_NOT_INITIALIZED;
	}
	if (s_registerInitialized) {
		if (hwrUserRegTerm() != 0) {
			sceKernelUnlockLwMutex(&s_mutex, 1);
			return SCE_HANDWRITING_ERROR_INTERNAL;
		}
		s_registerInitialized = 0;
	}
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return 0;
}

SceInt32 sceHandwritingRegisterSet(SceWChar32 code, const SceHandwritingPointsData *data)
{
	int res;

	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (!s_initialized || !s_registerInitialized) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_NOT_INITIALIZED;
	}
	if (data == NULL) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_POINTER;
	}
	if (data->points[0].x == 255u && data->points[0].y == 255u) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_PARAM;
	}

	res = hwrUserRegAdd(code, (const uint8_t *)data->points);
	sceKernelUnlockLwMutex(&s_mutex, 1);
	if (res == -303)
		return SCE_HANDWRITING_ERROR_CODE_ERROR;
	if (res == -302)
		return SCE_HANDWRITING_ERROR_DIC_FULL;
	if (res == -1)
		return SCE_HANDWRITING_ERROR_INTERNAL;
	return res;
}

SceInt32 sceHandwritingRegisterGetResult(SceHandwritingRegisterData *data)
{
	int16_t scores[SCE_HANDWRITING_MAX_REGISTER_CANDIDATES];
	int res;

	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (!s_initialized || !s_registerInitialized) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_NOT_INITIALIZED;
	}
	if (data == NULL) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_POINTER;
	}

	res = hwrUserRegGetResult(data->codes, scores);
	sceKernelUnlockLwMutex(&s_mutex, 1);
	if (res == -1)
		return SCE_HANDWRITING_ERROR_INTERNAL;
	return res;
}

SceInt32 sceHandwritingRegisterDelete(SceWChar32 code)
{
	int res;

	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (!s_initialized || !s_registerInitialized) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_NOT_INITIALIZED;
	}

	res = hwrUserRegDelete(code);
	sceKernelUnlockLwMutex(&s_mutex, 1);
	if (res == -303)
		return SCE_HANDWRITING_ERROR_CODE_ERROR;
	if (res == -1)
		return SCE_HANDWRITING_ERROR_INTERNAL;
	return res;
}

SceInt32 sceHandwritingRegisterInfo(SceWChar32 *codes)
{
	int res;

	if (sceKernelLockLwMutex(&s_mutex, 1, NULL) < 0) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INTERNAL;
	}
	if (!s_initialized || !s_registerInitialized) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_NOT_INITIALIZED;
	}
	if (codes == NULL) {
		sceKernelUnlockLwMutex(&s_mutex, 1);
		return SCE_HANDWRITING_ERROR_INVALID_POINTER;
	}

	res = hwrUserRegGetInfo(codes);
	sceKernelUnlockLwMutex(&s_mutex, 1);
	if (res == -1)
		return SCE_HANDWRITING_ERROR_INTERNAL;
	return res;
}
