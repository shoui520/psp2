/*
 * The utilities ("UTIL:"): the heap the responder allocates from (paf's heap allocator over a
 * memory block of its own), the C library routines it uses in place of libc's, MTP strings
 * and arrays packed for the host, dates, and the transfer buffers, which come from the heap or
 * from the large buffers the caller lends.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/sysmem/memblock.h>
#include <rtc.h>
#include <libmd5.h>
#include <ces/libces_ucs.h>
#include <vshbridge/vshbridge.h>
#include <system/system.h>
#include <memory/heap_allocator.h>
#include "mtp_internal.h"

#define MTP_TAG_UTIL	"UTIL:"

/* the heap's memory block */
#define MTP_UTIL_MEMBLOCK_ALIGNMENT	16

/* the ID storage leaf of the console's serial number, in UTF-16 */
#define MTP_UTIL_IDSTORAGE_SERIAL	274
#define MTP_UTIL_SERIAL_LENGTH		32

/* the transfer buffers' sizes */
#define MTP_UTIL_HEAP_BUFFER_SIZE	0x10000
#define MTP_UTIL_LARGE_BUFFER_SIZE	0x100000
#define MTP_UTIL_BUFFER_ALIGNMENT	64


static int s_bufferState = MTP_UTIL_BUFFER_STATE_NONE;
/* the caller allowed the large buffers */
static int s_largeBufferEnabled = 0;

extern "C" {

paf::memory::HeapAllocator *g_mtpUtilHeap;
static const MtpLargeBufferFuncs *s_largeBufferFuncs;
/* the serial number as ID storage keeps it, then compacted to ASCII */
static char s_serialNumber[128];
static SceUID s_memBlock;

static void largeBufferStateCallback(int state);

SceUInt8 mtpUtilUInt2HexChar(SceUInt8 value)
{
	if (value <= 9)
		return value + '0';
	if (value >= 10 && value <= 15)
		return value + 'A' - 10;
	return '0';
}

void *mtpUtilMemset(void *dst, int ch, int size)
{
	if (dst != NULL) {
		char *p;

		for (p = (char *)dst; size > 0; size--)
			*p++ = ch;
	}
	return dst;
}

int mtpUtilInitialize(SceSize size, const MtpLargeBufferFuncs *largeBufferFuncs)
{
	SceKernelAllocMemBlockOpt opt;
	void *base;

	if ((size & 0xFFF) != 0) {
		MTP_ERR(MTP_TAG_UTIL, 124, "heap size is not divide 4KiB %d\n", size);
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	if (g_mtpUtilHeap != NULL) {
		MTP_ERR(MTP_TAG_UTIL, 141, "utility is already initialized\n");
		return SCE_MTP_ERROR_ALREADY;
	}
	mtpUtilMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT |
		   SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	opt.alignment = MTP_UTIL_MEMBLOCK_ALIGNMENT;
	s_memBlock = sceKernelAllocMemBlock("SceMtpUser", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size,
					    &opt);
	sceKernelGetMemBlockBase(s_memBlock, &base);
	g_mtpUtilHeap = new paf::memory::HeapAllocator(base, size, "SceMtpUtilHeap");
	s_largeBufferFuncs = largeBufferFuncs;
	return 0;
}

int mtpUtilUninitialize(void)
{
	int res = 0;

	if (g_mtpUtilHeap != NULL) {
		delete g_mtpUtilHeap;
		sceKernelFreeMemBlock(s_memBlock);
		g_mtpUtilHeap = NULL;
	} else {
		MTP_ERR(MTP_TAG_UTIL, 172, "utility is not initialized\n");
		res = SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	s_largeBufferFuncs = NULL;
	return res;
}

void *mtpUtilMalloc(SceSize size)
{
	return g_mtpUtilHeap->sce_paf_malloc(size);
}

void *mtpUtilMallocAlign(SceSize align, SceSize size)
{
	return g_mtpUtilHeap->sce_paf_memalign(align, size);
}

void mtpUtilFree(void *ptr)
{
	if (ptr != NULL)
		g_mtpUtilHeap->sce_paf_free(ptr);
}

int mtpUtilMemcmp(const void *a, const void *b, int size)
{
	const SceUInt8 *p = (const SceUInt8 *)a;
	const SceUInt8 *q = (const SceUInt8 *)b;

	for (; size > 0; size--, p++, q++) {
		if (*p != *q)
			return *p < *q ? -1 : 1;
	}
	return 0;
}

void *mtpUtilMemcpy(void *dst, const void *src, int size)
{
	SceUInt8 *d = (SceUInt8 *)dst;
	const SceUInt8 *s = (const SceUInt8 *)src;

	if (dst == NULL)
		return NULL;
	for (; size > 0; size--)
		*d++ = *s++;
	return dst;
}

SceSize mtpUtilStrlen(const char *s)
{
	const SceUInt8 *p = (const SceUInt8 *)s;
	SceSize length = 0;

	if (s != NULL) {
		while (*p++ != '\0')
			length++;
	}
	return length;
}

char *mtpUtilStrcpy(char *dst, const char *src)
{
	SceUInt8 *d = (SceUInt8 *)dst;
	const SceUInt8 *s = (const SceUInt8 *)src;

	if (dst == NULL || src == NULL)
		return NULL;
	while ((*d++ = *s++) != '\0')
		;
	return dst;
}

char *mtpUtilStrncpy(char *dst, const char *src, int size)
{
	SceUInt8 *d = (SceUInt8 *)dst;
	const SceUInt8 *s = (const SceUInt8 *)src;

	if (dst == NULL || src == NULL)
		return NULL;
	while (size-- != 0 && (*d++ = *s++) != '\0')
		;
	return dst;
}

char *mtpUtilStrcat(char *dst, const char *src)
{
	SceUInt8 *d = (SceUInt8 *)dst;
	const SceUInt8 *s = (const SceUInt8 *)src;
	SceUInt8 *end;

	if (dst == NULL || src == NULL)
		return NULL;
	/* the same string */
	if (dst + mtpUtilStrlen(dst) == src + mtpUtilStrlen(src))
		return NULL;
	do {
		end = d;
	} while (*d++ != '\0');
	while ((*end++ = *s++) != '\0')
		;
	return dst;
}

int mtpUtilStrcmp(const char *a, const char *b)
{
	const SceUInt8 *p = (const SceUInt8 *)a;
	const SceUInt8 *q = (const SceUInt8 *)b;

	if (a != NULL && b != NULL) {
		while (*p == *q++) {
			if (*p++ == '\0')
				return 0;
		}
		return *p - q[-1];
	}
	if (a == b)
		return 0;
	return a != NULL ? 1 : -1;
}

int mtpUtilStrncmp(const char *a, const char *b, int size)
{
	const SceUInt8 *p = (const SceUInt8 *)a;
	const SceUInt8 *q = (const SceUInt8 *)b;
	int c1, c2;

	if (a == NULL || b == NULL) {
		if (a == b)
			return 0;
		return a != NULL ? 1 : -1;
	}
	do {
		c2 = *q++;
		c1 = *p;
		if (c1 != c2 || size-- == 0)
			return c1 - c2;
		if (*p++ == '\0')
			return 0;
	} while (size != 0);
	return 0;
}

char *mtpUtilStrchr(const char *s, int c)
{
	const SceUInt8 *p = (const SceUInt8 *)s;

	if (s == NULL)
		return NULL;
	for (; *p != '\0'; p++) {
		if (*p == (SceUInt8)c)
			return (char *)p;
	}
	MTP_ERR(MTP_TAG_UTIL, 463, "[%s] does not include %c\n", s, c);
	return NULL;
}

char *mtpUtilStrrchr(const char *s, int c)
{
	const SceUInt8 *q = (const SceUInt8 *)s;
	const SceUInt8 *p;

	if (s == NULL)
		return NULL;
	do {
		p = q;
	} while (*q++ != '\0');
	do {
		if (*p == (SceUInt8)c)
			return (char *)p;
	} while (p-- > (const SceUInt8 *)s);
	MTP_ERR(MTP_TAG_UTIL, 495, "[%s] does not include '%c' \n", s, c);
	return NULL;
}

int mtpUtilSetMtpResponse(SceUInt32 code, const SceUInt32 *params, SceUInt32 numParams,
			  const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 i;

	if (numParams > 5) {
		MTP_ERR(MTP_TAG_UTIL, 519, "wrong num of param: %u\n", numParams);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	response->sessionId = operation->sessionId;
	response->code = code;
	response->transactionId = operation->transactionId;
	response->numParams = numParams;
	if (params != NULL) {
		for (i = 0; i < numParams; i++)
			response->param[i] = params[i];
	}
	return 0;
}

int mtpUtilPackMtpString(MtpString *string, const char *src)
{
	SceCesUcsContext ctx;
	uint32_t utf16Len = 0;
	uint32_t utf8Len = 0;
	int ret = 0;
	uint16_t *chars = (uint16_t *)string->chars;

	if (mtpUtilStrlen(src) == 0) {
		string->numChars = 0;
		return ret;
	}
	ret = sceCesUcsContextInit(&ctx);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UTIL, 560, "sceCesUcsContextInit failed: 0x%x\n", ret);
		return ret;
	}
	ret = sceCesUtf8StrToUtf16Str(&ctx, (const uint8_t *)src, mtpUtilStrlen(src) + 1,
				      &utf8Len, chars, string->numChars, &utf16Len);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UTIL, 568, "unable to convert UTF8 -> UTF16=0x%x", ret);
		return ret;
	}
	string->numChars = utf16Len + 1;
	return utf16Len * 2 + 2;
}

int mtpUtilUnpackMtpString(char *dst, int size, const MtpString *string)
{
	SceCesUcsContext ctx;
	uint32_t utf16Len;
	uint32_t utf8Len;
	int ret;

	if (string->numChars == 0) {
		*dst = '\0';
		return 1;
	}
	utf16Len = 0;
	utf8Len = 0;
	ret = sceCesUcsContextInit(&ctx);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UTIL, 605, "sceCesUcsContextInit failed: 0x%x\n", ret);
		return ret;
	}
	ret = sceCesUtf16StrToUtf8Str(&ctx, (const uint16_t *)string->chars, string->numChars,
				      &utf16Len, (uint8_t *)dst, size, &utf8Len);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UTIL, 612, "unable to convert UTF16 -> UTF8=0x%x\n", ret);
		return ret;
	}
	return utf8Len + 1;
}

int mtpUtilPackUInt8Array(void *dst, const MtpArray *array)
{
	int count = array->count;

	mtpUtilMemcpy(dst, &count, sizeof(count));
	mtpUtilMemcpy((char *)dst + 4, array->values, count);
	return count + 4;
}

int mtpUtilPackCodeList(void *dst, const MtpCodeList *list)
{
	int count = list->count;

	mtpUtilMemcpy(dst, &count, sizeof(count));
	mtpUtilMemcpy((char *)dst + 4, list->codes, count * 2);
	return count * 2 + 4;
}

int mtpUtilPackUInt32Array(void *dst, const MtpArray *array)
{
	int count = array->count;

	mtpUtilMemcpy(dst, &count, sizeof(count));
	mtpUtilMemcpy((char *)dst + 4, array->values, count * 4);
	return count * 4 + 4;
}

int mtpUtilPackUInt64Array(void *dst, const MtpArray *array)
{
	int count = array->count;

	mtpUtilMemcpy(dst, &count, sizeof(count));
	mtpUtilMemcpy((char *)dst + 4, array->values, count * 8);
	return count * 8 + 4;
}

int mtpUtilDateTimeToString(char *str, const SceDateTime *time)
{
	char *p;
	unsigned int value;

	for (p = &str[3], value = time->year; p >= &str[0]; value /= 10)
		*p-- = value % 10 + '0';
	for (p = &str[5], value = time->month; p >= &str[4]; value /= 10)
		*p-- = value % 10 + '0';
	for (p = &str[7], value = time->day; p >= &str[6]; value /= 10)
		*p-- = value % 10 + '0';
	str[8] = 'T';
	for (p = &str[10], value = time->hour; p >= &str[9]; value /= 10)
		*p-- = value % 10 + '0';
	for (p = &str[12], value = time->minute; p >= &str[11]; value /= 10)
		*p-- = value % 10 + '0';
	for (p = &str[14], value = time->second; p >= &str[13]; value /= 10)
		*p-- = value % 10 + '0';
	str[15] = '.';
	str[16] = '0';
	str[17] = '\0';
	return 0;
}

int mtpUtilStringToDateTime(const char *str, SceDateTime *time)
{
	const SceUInt8 *s = (const SceUInt8 *)str;

	time->year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
	time->month = (s[4] - '0') * 10 + (s[5] - '0');
	time->day = (s[6] - '0') * 10 + (s[7] - '0');
	time->hour = (s[9] - '0') * 10 + (s[10] - '0');
	time->minute = (s[11] - '0') * 10 + (s[12] - '0');
	time->second = (s[13] - '0') * 10 + (s[14] - '0');
	time->microsecond = 0;
	return 0;
}

int mtpUtilMd5(const char *str, const void *salt, unsigned char *digest)
{
	SceMd5Context ctx;

	sceMd5BlockInit(&ctx);
	sceMd5BlockUpdate(&ctx, str, mtpUtilStrlen(str));
	sceMd5BlockUpdate(&ctx, salt, 16);
	sceMd5BlockResult(&ctx, digest);
	return 0;
}

int mtpUtilGetSerialNumberFromIDStorage(char *serial)
{
	int ret;
	int i;

	if (serial == NULL) {
		MTP_ERR(MTP_TAG_UTIL, 856, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = vshIdStorageLookup(MTP_UTIL_IDSTORAGE_SERIAL, 0, s_serialNumber,
				 MTP_UTIL_SERIAL_LENGTH * 2);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UTIL, 862, "vshIdStorageLookup failed: 0x%x\n", ret);
		return ret;
	}
	/* the low byte of each UTF-16 character */
	for (i = 0; i < MTP_UTIL_SERIAL_LENGTH; i++)
		s_serialNumber[i] = s_serialNumber[i * 2 + 1];
	s_serialNumber[MTP_UTIL_SERIAL_LENGTH] = '\0';
	mtpUtilMemcpy(serial, s_serialNumber, MTP_UTIL_SERIAL_LENGTH + 1);
	return ret;
}

int mtpUtilResponseCodeToError(SceUInt32 code)
{
	int error;

	switch (code) {
	case 0x2001:
		return 0;
	case 0x2005:
	case 0x2006:
		return SCE_MTP_ERROR_NOT_SUPPORTED;
	case 0x2007:
		return SCE_MTP_ERROR_INCOMPLETE_TRANSFER;
	case 0x200C:
		return SCE_MTP_ERROR_STORE_FULL;
	case 0x200D:
		return SCE_MTP_ERROR_OBJECT_WRITE_PROTECTED;
	case 0x200F:
		error = SCE_MTP_ERROR_GENERAL;
		if (mtpStatusGetPlatformClass() == MTP_PLATFORM_CLASS_PS)
			error = SCE_MTP_ERROR_ACCESS_DENIED;
		return error;
	case 0x2010:
		return SCE_MTP_ERROR_NO_THUMBNAIL_PRESENT;
	case 0x2013:
		return SCE_MTP_ERROR_STORE_NOT_AVAILABLE;
	case 0xA00A:
		return SCE_MTP_ERROR_RESPONSE_A00A;
	case 0xA00D:
	case 0xA015:
		return SCE_MTP_ERROR_RESPONSE_A00D;
	case 0xA008:
	case 0xA016:
		return SCE_MTP_ERROR_RESPONSE_A008;
	case 0xA010:
		return SCE_MTP_ERROR_RESPONSE_A010;
	case 0xA017:
		return SCE_MTP_ERROR_RESPONSE_A017;
	case 0xA018:
		return SCE_MTP_ERROR_RESPONSE_A018;
	case 0xA01C:
		return SCE_MTP_ERROR_RESPONSE_A01C;
	case 0xA020:
		return SCE_MTP_ERROR_RESPONSE_A020;
	case 0xA021:
		return SCE_MTP_ERROR_RESPONSE_A021;
	case 0xA022:
		return SCE_MTP_ERROR_RESPONSE_A022;
	case 0xA023:
		return SCE_MTP_ERROR_RESPONSE_A023;
	case 0xA027:
		return SCE_MTP_ERROR_RESPONSE_A027;
	case 0xA028:
		return SCE_MTP_ERROR_RESPONSE_A028;
	case 0xA029:
		return SCE_MTP_ERROR_RESPONSE_A029;
	case 0xA001:
	case 0xA003:
	case 0xA004:
	case 0xA005:
	case 0xA024:
		return SCE_MTP_ERROR_RESPONSE_A001;
	case 0xA002:
		return SCE_MTP_ERROR_RESPONSE_A002;
	case 0xA007:
		return SCE_MTP_ERROR_RESPONSE_A007;
	case 0xA012:
		return SCE_MTP_ERROR_RESPONSE_A012;
	case 0xA013:
		return SCE_MTP_ERROR_RESPONSE_A013;
	case 0xA014:
		return SCE_MTP_ERROR_RESPONSE_A014;
	case 0xA01A:
		return SCE_MTP_ERROR_RESPONSE_A01A;
	case 0xA01B:
		return SCE_MTP_ERROR_RESPONSE_A01B;
	case 0xA01F:
		return SCE_MTP_ERROR_RESPONSE_A01F;
	case 0xA02A:
		return SCE_MTP_ERROR_RESPONSE_A02A;
	case 0xA02B:
		return SCE_MTP_ERROR_RESPONSE_A021;
	case 0xA02E:
		return SCE_MTP_ERROR_RESPONSE_A02E;
	case 0xA02C:
		return SCE_MTP_ERROR_RESPONSE_A02C;
	default:
		return SCE_MTP_ERROR_GENERAL;
	}
}

int mtpUtilTrimFileName(const char *src, char *dst, SceUInt32 dstSize, SceUInt32 srcSize)
{
	SceCesUcsContext ctx;
	uint32_t utf8Len = 0;
	uint32_t copyLen = 0;
	const char *ext;
	char *extName = NULL;
	SceUInt32 extSize = 0;
	int ret;

	if (dst == NULL) {
		MTP_ERR(MTP_TAG_UTIL, 1002, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	/* the extension is kept whole */
	ext = mtpUtilStrrchr(src, '.');
	if (ext != NULL) {
		extSize = mtpUtilStrlen(ext) + 1;
		extName = (char *)mtpUtilMalloc(extSize);
		if (extName == NULL) {
			MTP_ERR(MTP_TAG_UTIL, 1012,
				"unable to alloc memory for object file extention name\n");
			return SCE_MTP_ERROR_NO_MEMORY;
		}
		mtpUtilStrncpy(extName, ext, extSize);
	}
	ret = sceCesUcsContextInit(&ctx);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UTIL, 1021, "sceCesUcsContextInit failed: 0x%x\n", ret);
		goto end;
	}
	ret = sceCesUtf8StrToCopyStr(&ctx, (const uint8_t *)src, srcSize - extSize, &utf8Len,
				     (uint8_t *)dst, dstSize, &copyLen);
	if (ret < 0) {
		/* the name was cut */
		if (ret != SCE_CES_ERROR_SRC_BUFFER_END) {
			MTP_ERR(MTP_TAG_UTIL, 1031, "unable to copy string=0x%x\n", ret);
			goto end;
		}
		ret = 0;
	}
	if (extName == NULL)
		return ret;
	mtpUtilStrcat(dst, extName);
end:
	mtpUtilFree(extName);
	return ret;
}

int mtpUtilGetUnpackMtpStringSize(MtpString *string)
{
	SceCesUcsContext ctx;
	uint32_t utf16Len = 0;
	uint32_t utf8Len = 0;
	int ret;

	ret = sceCesUcsContextInit(&ctx);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UTIL, 1062, "sceCesUcsContextInit failed: 0x%x\n", ret);
		return ret;
	}
	ret = sceCesUtf16StrGetUtf8Len(&ctx, (const uint16_t *)string->chars, string->numChars,
				       &utf16Len, &utf8Len);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UTIL, 1068, "sceCesUtf16StrGetUtf8Len failed: 0x%x\n", ret);
		return ret;
	}
	string->numChars = utf16Len + 1;
	return utf8Len;
}

void mtpUtilReplaceInvalidChars(char *name, SceSize length)
{
	SceUInt8 invalid[] = { '"', ':', '<', '>', '|', '*', '?', '\\', '/', '\0' };
	SceUInt8 *c = (SceUInt8 *)name;
	SceSize i;
	const SceUInt8 *p;

	for (i = 0; i < length; i++, c++) {
		for (p = invalid; *p != '\0'; p++) {
			if (*c == *p)
				*c = '_';
		}
	}
}

int mtpUtilIsUsbAvailable(void)
{
	return paf::system::Model::GetType() == paf::system::Model::Type_Vita;
}

int mtpUtilIsLargeBufferEnabled(void)
{
	return s_largeBufferEnabled;
}

void mtpUtilEnableLargeBuffer(void)
{
	s_largeBufferEnabled = 1;
}

int mtpUtilGetBufferState(void)
{
	return s_bufferState;
}

void mtpUtilSetBufferState(int state)
{
	s_bufferState = state;
}

int mtpUtilLargeBufferStart(void)
{
	int ret;

	if (s_largeBufferFuncs == NULL) {
		MTP_WARN(MTP_TAG_UTIL, 1134, "Large buffer is not available\n");
		return SCE_MTP_ERROR_DEVICE;
	}
	ret = s_largeBufferFuncs->start(largeBufferStateCallback);
	if (ret == 0)
		s_bufferState = MTP_UTIL_BUFFER_STATE_LARGE;
	return ret;
}

int mtpUtilLargeBufferEnd(void)
{
	int ret;

	if (s_largeBufferFuncs == NULL) {
		MTP_WARN(MTP_TAG_UTIL, 1149, "Large buffer is not available\n");
		return SCE_MTP_ERROR_DEVICE;
	}
	ret = s_largeBufferFuncs->end();
	s_bufferState = MTP_UTIL_BUFFER_STATE_NONE;
	s_largeBufferEnabled = 0;
	return ret;
}

void *mtpUtilLargeBufferMemalign(SceSize align, SceSize size)
{
	if (s_largeBufferFuncs != NULL)
		return s_largeBufferFuncs->memalign(align, size);
	MTP_WARN(MTP_TAG_UTIL, 1183, "Large buffer is not available\n");
	return NULL;
}

void mtpUtilLargeBufferFree(void *ptr)
{
	if (s_largeBufferFuncs != NULL)
		s_largeBufferFuncs->free(ptr);
	else
		MTP_WARN(MTP_TAG_UTIL, 1193, "Large buffer is not available\n");
}

int mtpUtilChangeBufferAllocator(void **buffer0, void **buffer1, SceSize *size,
				 void (**freeFunc)(void *ptr), int allocator)
{
	int ret = 0;

	if (allocator == MTP_UTIL_BUFFER_HEAP) {
		mtpUtilLargeBufferFree(*buffer0);
		mtpUtilLargeBufferFree(*buffer1);
		*size = MTP_UTIL_HEAP_BUFFER_SIZE;
		*buffer0 = mtpUtilMallocAlign(MTP_UTIL_BUFFER_ALIGNMENT, *size);
		*buffer1 = mtpUtilMallocAlign(MTP_UTIL_BUFFER_ALIGNMENT, *size);
		if (*buffer0 == NULL || *buffer1 == NULL) {
			MTP_ERR(MTP_TAG_UTIL, 1214, "unable to alloc memory for xfer buffers\n");
			ret = SCE_MTP_ERROR_NO_MEMORY;
		}
		*freeFunc = mtpUtilFree;
		s_bufferState = MTP_UTIL_BUFFER_STATE_HEAP;
	} else if (allocator == MTP_UTIL_BUFFER_LARGE) {
		mtpUtilFree(*buffer0);
		mtpUtilFree(*buffer1);
		*size = MTP_UTIL_LARGE_BUFFER_SIZE;
		*buffer0 = mtpUtilLargeBufferMemalign(MTP_UTIL_BUFFER_ALIGNMENT, *size);
		*buffer1 = mtpUtilLargeBufferMemalign(MTP_UTIL_BUFFER_ALIGNMENT, *size);
		if (*buffer0 != NULL && *buffer1 != NULL) {
			*freeFunc = mtpUtilLargeBufferFree;
			s_bufferState = MTP_UTIL_BUFFER_STATE_LARGE;
		} else {
			MTP_ERR(MTP_TAG_UTIL, 1227, "unable to alloc memory for xfer buffers\n");
		}
	}
	return ret;
}

/* a time from a nasne carries no zone: it is local time, which MTP's times are not */
static __attribute__((noinline)) int isLocalTime(const char *str)
{
	MtpStatus status;

	if (mtpUtilStrrchr(str, 'Z') != NULL)
		return 0;
	if (mtpDevGetIfType() == MTP_IF_TYPE_NET) {
		mtpStatusGet(&status);
		if (status.platformType == MTP_PLATFORM_TYPE_NASNE)
			return 1;
	}
	return 0;
}

int mtpUtilStringToUtcDateTime(const char *str, SceDateTime *time)
{
	SceRtcTick utc;
	SceRtcTick tick;
	SceDateTime dateTime;
	int ret;

	if (str == NULL || time == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	ret = mtpUtilStringToDateTime(str, &dateTime);
	if (ret < 0)
		return ret;
	if (!isLocalTime(str)) {
		mtpUtilMemcpy(time, &dateTime, sizeof(dateTime));
	} else {
		ret = sceRtcGetTick(&dateTime, &tick);
		if (ret < 0)
			return ret;
		ret = sceRtcConvertLocalTimeToUtc(&tick, &utc);
		if (ret < 0)
			return ret;
		ret = sceRtcSetTick(&dateTime, &utc);
		if (ret < 0)
			return ret;
		mtpUtilMemcpy(time, &dateTime, sizeof(dateTime));
	}
	return ret;
}

static void largeBufferStateCallback(int state)
{
	if (s_bufferState != MTP_UTIL_BUFFER_STATE_NONE)
		s_bufferState = state;
}

} /* extern "C" */
