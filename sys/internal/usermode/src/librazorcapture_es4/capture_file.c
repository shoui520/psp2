/*
 * SceRazorCapture: .sgx capture file chunk writers, 2-level 4 KiB page bitmap,
 * GXM memory mapping table, and shader program metadata serializer.
 */

#include "razor_capture_internal.h"
#include <fw.h>

/* External GXM shader program reflection imports */
int sceGxmProgramCheck(const SceGxmProgram *program);
uint32_t sceGxmProgramGetSize(const SceGxmProgram *program);
const SceGxmProgramParameter *sceGxmProgramGetParameter(const SceGxmProgram *program, uint32_t index);
uint32_t sceGxmProgramParameterGetArraySize(const SceGxmProgramParameter *parameter);
uint32_t sceGxmProgramParameterGetResourceIndex(const SceGxmProgramParameter *parameter);
const char *sceGxmProgramParameterGetName(const SceGxmProgramParameter *parameter);

/* External helpers in sgx_decode.c */
extern uint32_t sgxPdsProgramByteSize(const SgxPdsProgram *prog);
extern int sgxPdsOutputReadDmaReg(const void *pdsOut, int regIdx, uint32_t *outVal);

/* Zero padding buffer for 4-byte aligned strings */
static const uint32_t s_zeroPad = 0;

/* --- Internal helpers --- */

static inline void razorWriteChunk(RazorCaptureFile *file, uint32_t chunkId, const void *payload, uint32_t payloadSize)
{
	uint32_t hdr[2];

	hdr[0] = 0x80000000U | (chunkId & 0x00FFFFFFU);
	hdr[1] = payloadSize + 8U;
	if (razorFileWriterWrite(&file->writer, hdr, 8U) == 0)
		razorFileWriterWrite(&file->writer, payload, payloadSize);
}

static inline void razorPageBitmapMarkRange(RazorCaptureFile *file, uint32_t startAddr, uint32_t endAddr)
{
	uint32_t lastAddr;
	uint32_t startDir, endDir;
	uint32_t startPage, endPage;
	uint32_t dirIdx;

	if (startAddr >= endAddr)
		return;

	lastAddr = endAddr - 1U;
	startDir = startAddr >> 24;
	endDir = lastAddr >> 24;
	startPage = (startAddr >> 12) & 0xFFFU;
	endPage = (lastAddr >> 12) & 0xFFFU;

	for (dirIdx = startDir; dirIdx <= endDir; dirIdx++) {
		RazorPageBlock *blk = file->pageDir[dirIdx];
		uint32_t pageStart = (dirIdx == startDir) ? startPage : 0U;
		uint32_t pageEnd = (dirIdx == endDir) ? endPage : 4095U;
		uint32_t p;

		if (blk == NULL) {
			uint32_t i;
			for (i = 0; i < 33U; i++) {
				if (file->pagePool[i].inUse == 0) {
					blk = &file->pagePool[i];
					break;
				}
			}
			if (blk == NULL)
				return;
			blk->inUse = 0;
			razorMemset(blk->bits, 0, sizeof(blk->bits));
			blk->inUse = 1;
			file->pageDir[dirIdx] = blk;
		}

		for (p = pageStart; p <= pageEnd; p++)
			blk->bits[p >> 3] |= (uint8_t)(1U << ((~p) & 7U));
	}
}

/* --- Capture file lifecycle --- */

void razorCaptureFileReset(RazorCaptureFile *file)
{
	int i;

	for (i = 0; i < 256; i++) {
		if (file->pageDir[i] != NULL) {
			file->pageDir[i]->inUse = 0;
			file->pageDir[i] = NULL;
		}
	}
	file->mappingCount = 0;
}

RazorCaptureFile *razorCaptureFileInit(RazorCaptureFile *file)
{
	int i;

	razorFileWriterInit(&file->writer);
	razorMemset(file->pageDir, 0, sizeof(file->pageDir));
	for (i = 0; i < 33; i++) {
		file->pagePool[i].inUse = 0;
		razorMemset(file->pagePool[i].bits, 0, sizeof(file->pagePool[i].bits));
	}
	file->mappingCount = 0;
	file->errorFlags = 0;
	return file;
}

uint64_t razorCaptureFileOpen(RazorCaptureFile *file, const char *filename, const void *initData)
{
	int i;

	for (i = 0; i < 256; i++) {
		if (file->pageDir[i] != NULL) {
			file->pageDir[i]->inUse = 0;
			file->pageDir[i] = NULL;
		}
	}
	file->errorFlags = 0;

	if (razorFileWriterOpen(&file->writer, filename) != 0)
		return 64ULL;

	{
		uint32_t version = 2U;
		razorWriteChunk(file, 0x1A2B00U, &version, sizeof(version));
	}

	{
		struct {
			uint32_t platform;
			uint16_t gpuFreq;
			uint16_t wireless;
			uint32_t reserved0;
			uint32_t reserved1;
			uint32_t reserved2;
			uint32_t sdkVersion;
			uint32_t reserved3;
			uint32_t hardwareId;
			uint16_t majorVer;
			uint16_t minorVer;
			uint16_t buildVer;
			uint16_t pad;
		} sysInfo;
		int gpuFreq = scePowerGetGpuClockFrequency();
		int wireless = scePowerGetUsingWireless();

		sysInfo.platform = 4U;
		if ((gpuFreq | wireless) < 0) {
			sysInfo.gpuFreq = 0;
			sysInfo.wireless = 0;
		} else {
			sysInfo.gpuFreq = (uint16_t)gpuFreq;
			sysInfo.wireless = (uint16_t)wireless;
		}
		sysInfo.reserved0 = 0U;
		sysInfo.reserved1 = 0U;
		sysInfo.reserved2 = 0U;
		sysInfo.sdkVersion = FW_VERSION | 0x11U;
		sysInfo.reserved3 = 0U;
		sysInfo.hardwareId = 0x13A5U;
		sysInfo.majorVer = 1U;
		sysInfo.minorVer = 7U;
		sysInfo.buildVer = 17U;
		sysInfo.pad = 0U;
		razorWriteChunk(file, 0x1A2B08U, &sysInfo, sizeof(sysInfo));
	}

	razorWriteChunk(file, 0x1A2B1EU, initData, 0x10U);
	return 0ULL;
}

int razorCaptureFileClose(RazorCaptureFile *file)
{
	uint32_t hdr[2];

	hdr[0] = 0x801A2B07U;
	hdr[1] = 8U;
	razorFileWriterWrite(&file->writer, hdr, 8U);
	return razorFileWriterClose(&file->writer);
}

int razorCaptureFileWriteFrameInfo(RazorCaptureFile *file, uint32_t a2, uint32_t a3)
{
	uint32_t payload[2];
	uint32_t hdr[2];
	int res;

	payload[0] = a2;
	payload[1] = a3;
	hdr[0] = 0x801A2B13U;
	hdr[1] = 16U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, payload, 8U);
	return res;
}

int razorCaptureFileWriteJobHeader(RazorCaptureFile *file, const void *jobMeta, const void *regs, const uint32_t *kickInfo)
{
	uint32_t hdr[2];
	int res;

	{
		struct {
			uint32_t jobId;
			uint8_t flags0;
			uint8_t flags1;
			uint16_t pad;
		} meta;
		meta.jobId = *(const uint32_t *)jobMeta;
		meta.flags0 = 1;
		meta.flags1 = ((const uint8_t *)jobMeta)[4];
		meta.pad = 0;
		razorWriteChunk(file, 0x1A2B01U, &meta, 8U);
	}

	if (kickInfo != NULL) {
		uint32_t flags = kickInfo[1] & 0x2FFFFFFFU;
		uint32_t summary[8];

		razorWriteChunk(file, 0x1A2B09U, kickInfo, 0x108U);
		summary[0] = kickInfo[2];
		summary[1] = kickInfo[3];
		summary[2] = kickInfo[6];
		summary[3] = kickInfo[7];
		summary[4] = flags;
		summary[5] = 0U;
		summary[6] = kickInfo[57];
		summary[7] = 0U;
		razorWriteChunk(file, 0x1A2B1AU, summary, 0x20U);
	}

	razorWriteChunk(file, 0x1A2B02U, jobMeta, 0x18U);

	hdr[0] = 0x801A2B0CU;
	hdr[1] = 72U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, regs, 0x40U);
	return res;
}

uint64_t razorCaptureFileFlushMemory(RazorCaptureFile *file, int captureAll)
{
	uint32_t i;
	uint64_t err;

	for (i = 0; i < file->mappingCount; i++) {
		uint32_t mapChunk[3];
		mapChunk[0] = file->mappings[i].cpuAddr;
		mapChunk[1] = file->mappings[i].gpuAddr;
		mapChunk[2] = file->mappings[i].size;
		razorWriteChunk(file, 0x1A2B03U, mapChunk, sizeof(mapChunk));
		if (captureAll)
			razorPageBitmapMarkRange(file, file->mappings[i].cpuAddr,
						 file->mappings[i].cpuAddr + file->mappings[i].size);
	}

	err = razorWriteMemoryDump(file->pageDir, &file->writer);
	razorFileWriterFlush(&file->writer);

	for (i = 0; i < 256U; i++) {
		if (file->pageDir[i] != NULL) {
			file->pageDir[i]->inUse = 0;
			file->pageDir[i] = NULL;
		}
	}
	return err;
}

void razorCaptureFileClearPages(RazorCaptureFile *file)
{
	int i;

	for (i = 0; i < 256; i++) {
		if (file->pageDir[i] != NULL) {
			file->pageDir[i]->inUse = 0;
			file->pageDir[i] = NULL;
		}
	}
}

int razorCaptureFileWriteSync(RazorCaptureFile *file)
{
	uint32_t zero = 0U;

	razorWriteChunk(file, 0x1A2B0EU, &zero, sizeof(zero));
	return razorFileWriterFlush(&file->writer);
}

/* --- Page bitmap marking wrappers --- */

void razorCaptureFileMarkAddrSize(RazorCaptureFile *file, uint32_t cpuAddr, uint32_t size)
{
	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + size);
}

void razorCaptureFileMark8Bytes(RazorCaptureFile *file, uint32_t cpuAddr)
{
	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + 8U);
}

void razorCaptureFileMarkVdmCtrl(RazorCaptureFile *file, uint32_t cpuAddr)
{
	uint32_t header = *(const uint32_t *)(uintptr_t)cpuAddr;
	uint32_t size = 8U;

	if (header & 0x00400000U)
		size += 4U;
	if (header & 0x00800000U)
		size += 4U;
	if (header & 0x01000000U)
		size += 8U;
	if (header & 0x02000000U)
		size += 8U;
	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + size);
}

void razorCaptureFileMark4Bytes(RazorCaptureFile *file, uint32_t cpuAddr)
{
	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + 4U);
}

void razorCaptureFileMarkData(RazorCaptureFile *file, uint32_t cpuAddr, uint32_t size)
{
	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + size);
}

void razorCaptureFileMarkRange(RazorCaptureFile *file, uint32_t startAddr, uint32_t endAddr)
{
	razorPageBitmapMarkRange(file, startAddr, endAddr);
}

void razorCaptureFileMarkPdsProgram(RazorCaptureFile *file, const SgxPdsProgram *prog)
{
	uint32_t cpuAddr = (uint32_t)(uintptr_t)prog->codePtr;
	uint32_t size = sgxPdsProgramByteSize(prog);
	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + size);
}

void razorCaptureFileMarkUsseProgram(RazorCaptureFile *file, const SgxUsseProgramInfo *info)
{
	uint32_t cpuAddr = (uint32_t)(uintptr_t)info->startPtr;
	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + info->byteSize);
}

void razorCaptureFileMarkBuffer(RazorCaptureFile *file, uint32_t cpuAddr, uint32_t size)
{
	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + size);
}

/* --- Texture / RT / Depth / State chunk writers --- */

uint64_t razorCaptureFileMarkTexture(RazorCaptureFile *file, const uint32_t *texWords)
{
	uint32_t texSize = sgxCalcTexByteSize(texWords);
	uint32_t gpuAddr = texWords[2] & ~3U;
	uint64_t err = 0ULL;
	uint32_t palGpuAddr;
	int palEntries;

	if (gpuAddr != 0U) {
		uint32_t cpuAddr = razorCaptureFileGpuToCpu(file, gpuAddr);
		if (cpuAddr == 0U)
			err = 1ULL;
		else
			razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + texSize);
	}

	if (sgxTexGetPalette(texWords, &palGpuAddr, &palEntries) != 0) {
		uint32_t cpuAddr = razorCaptureFileGpuToCpu(file, palGpuAddr);
		if (cpuAddr == 0U)
			err = 1ULL;
		else
			razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + 4U * (uint32_t)palEntries);
	}

	return err;
}

int razorCaptureFileWriteColorSurface(RazorCaptureFile *file, uint32_t surfaceId, const uint32_t *rtWords,
				      uint32_t flags, const uint32_t *extra)
{
	uint32_t rtState[12];
	uint32_t rtSize;
	uint32_t gpuAddr;
	uint32_t payload[12];
	uint32_t hdr[2];
	int res;

	sgxRtStateInit(rtState, rtWords);
	rtSize = sgxRtCalcByteSize(rtState);
	gpuAddr = rtState[2] & ~3U;

	{
		uint32_t cpuAddr = razorCaptureFileGpuToCpu(file, gpuAddr);
		if (cpuAddr != 0U) {
			razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + rtSize);
		} else if (gpuAddr != 0U) {
			file->errorFlags |= 1ULL;
		}
	}

	razorWriteChunk(file, 0x1A2B05U, rtWords, 0x18U);

	payload[0] = surfaceId;
	payload[1] = rtWords[0];
	payload[2] = rtWords[1];
	payload[3] = rtWords[2];
	payload[4] = rtWords[3];
	payload[5] = rtWords[4];
	payload[6] = rtWords[5];
	payload[7] = flags;
	payload[8] = extra[0];
	payload[9] = extra[1];
	payload[10] = extra[2];
	payload[11] = extra[3];

	hdr[0] = 0x801A2B14U;
	hdr[1] = 56U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, payload, 0x30U);
	return res;
}

int razorCaptureFileWriteDepthStencil(RazorCaptureFile *file, const void *dsData)
{
	const uint32_t *dsPtr;
	uint32_t hdr[2];
	int isStore;
	int res;

	sgxDepthStateInit(&dsPtr, dsData);

	for (isStore = 0; isStore < 2; isStore++) {
		if (sgxDepthIsDepthEnabled(&dsPtr, isStore)) {
			uint32_t gpuAddr = sgxDepthGetDepthAddr(&dsPtr, isStore);
			uint32_t size = sgxDepthCalcDepthSize(&dsPtr, isStore);
			if (gpuAddr != 0U) {
				uint32_t cpuAddr = razorCaptureFileGpuToCpu(file, gpuAddr);
				if (cpuAddr != 0U)
					razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + size);
			}
		}
		if (sgxDepthIsStencilEnabled(&dsPtr, isStore)) {
			uint32_t gpuAddr = sgxDepthGetStencilAddr(&dsPtr, isStore);
			uint32_t size = sgxDepthCalcStencilSize(&dsPtr, isStore);
			if (gpuAddr != 0U) {
				uint32_t cpuAddr = razorCaptureFileGpuToCpu(file, gpuAddr);
				if (cpuAddr != 0U)
					razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + size);
			}
		}
	}

	hdr[0] = 0x801A2B1DU;
	hdr[1] = 40U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, dsData, 0x20U);
	return res;
}

int razorCaptureFileWriteSceneInfo(RazorCaptureFile *file, const void *infoA, const void *infoB, const void *infoC)
{
	uint32_t hdr[2];
	int res;

	razorWriteChunk(file, 0x1A2B15U, infoA, 0x28U);
	razorWriteChunk(file, 0x1A2B1BU, infoB, 0x20U);

	hdr[0] = 0x801A2B1CU;
	hdr[1] = 24U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, infoC, 0x10U);
	return res;
}

int razorCaptureFileWritePdsState(RazorCaptureFile *file, const void *data)
{
	uint32_t hdr[2];
	int res;

	hdr[0] = 0x801A2B0BU;
	hdr[1] = 56U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, data, 0x30U);
	return res;
}

int razorCaptureFileWriteDrawState(RazorCaptureFile *file, const void *data)
{
	uint32_t hdr[2];
	int res;

	hdr[0] = 0x801A2B1FU;
	hdr[1] = 96U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, data, 0x58U);
	return res;
}

int razorCaptureFileWriteMarkerRange(RazorCaptureFile *file, uint32_t startAddr, uint32_t endAddr)
{
	uint32_t range[2];
	uint32_t hdr[2];
	int res;

	range[0] = startAddr;
	range[1] = endAddr;
	razorPageBitmapMarkRange(file, startAddr, endAddr);

	hdr[0] = 0x801A2B0DU;
	hdr[1] = 16U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, range, 8U);
	return res;
}

int razorCaptureFileWriteUniformBuf(RazorCaptureFile *file, uint32_t drawId, uint32_t gpuAddr,
				    uint32_t cpuAddr, uint32_t size, uint32_t bufType,
				    uint16_t regOffset, uint32_t byteOffset, uint8_t clampToMapping)
{
	uint32_t markSize = size;
	struct {
		uint32_t drawId;
		uint32_t gpuAddr;
		uint32_t cpuAddr;
		uint32_t size;
		uint32_t bufType;
		uint16_t regOffset;
		uint16_t pad;
		uint32_t byteOffset;
	} chunk;
	uint32_t hdr[2];
	int res;

	if (clampToMapping) {
		uint32_t remaining;
		if (!razorCaptureFileFindCpuMapping(file, cpuAddr, &remaining)) {
			file->errorFlags |= 0x20ULL;
			return 0;
		}
		if (remaining < markSize)
			markSize = remaining;
	}

	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + markSize);

	chunk.drawId = drawId;
	chunk.gpuAddr = gpuAddr;
	chunk.cpuAddr = cpuAddr;
	chunk.size = size;
	chunk.bufType = bufType;
	chunk.regOffset = regOffset;
	chunk.pad = 0;
	chunk.byteOffset = byteOffset;

	hdr[0] = 0x801A2B11U;
	hdr[1] = 36U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, &chunk, 0x1CU);
	return res;
}

int razorCaptureFileWriteLiteralUniform(RazorCaptureFile *file, uint32_t drawId, uint32_t gpuAddr,
					uint32_t cpuAddr, uint32_t bufType, uint16_t regOffset,
					uint16_t dmaReg, uint16_t regCount, uint8_t clampToMapping)
{
	uint32_t markAddr = cpuAddr + 4U * regOffset;
	uint32_t markSize = 4U * regCount;
	struct {
		uint32_t drawId;
		uint32_t gpuAddr;
		uint32_t cpuAddr;
		uint32_t bufType;
		uint16_t regOffset;
		uint16_t dmaReg;
		uint16_t regCount;
		uint16_t pad;
	} chunk;
	uint32_t hdr[2];
	int res;

	if (clampToMapping) {
		uint32_t remaining;
		if (!razorCaptureFileFindCpuMapping(file, markAddr, &remaining)) {
			file->errorFlags |= 0x20ULL;
			return 0;
		}
		if (remaining < markSize)
			markSize = remaining;
	}

	razorPageBitmapMarkRange(file, markAddr, markAddr + markSize);

	chunk.drawId = drawId;
	chunk.gpuAddr = gpuAddr;
	chunk.cpuAddr = cpuAddr;
	chunk.bufType = bufType;
	chunk.regOffset = regOffset;
	chunk.dmaReg = dmaReg;
	chunk.regCount = regCount;
	chunk.pad = 0;

	hdr[0] = 0x801A2B12U;
	hdr[1] = 32U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, &chunk, 0x18U);
	return res;
}

int razorCaptureFileWriteProgram(RazorCaptureFile *file, const void *progMeta, uint32_t cpuAddr, uint32_t size)
{
	uint32_t hdr[2];
	int res;

	razorPageBitmapMarkRange(file, cpuAddr, cpuAddr + size);

	hdr[0] = 0x801A2B0FU;
	hdr[1] = 136U;
	res = razorFileWriterWrite(&file->writer, hdr, 8U);
	if (res == 0)
		res = razorFileWriterWrite(&file->writer, progMeta, 0x80U);
	return res;
}

int razorCaptureFileWriteProgramParam(RazorCaptureFile *file, const void *paramMeta, const char *name)
{
	uint32_t hdr[2];
	SceSize len;
	SceSize padLen;
	const void *strPtr;

	if (name != NULL) {
		len = sceClibStrnlen(name, 0x100U);
		padLen = 4U - (len & 3U);
		strPtr = name;
	} else {
		len = 0;
		padLen = 4U;
		strPtr = &s_zeroPad;
	}

	hdr[0] = 0x801A2B17U;
	hdr[1] = (uint32_t)(len + padLen + 32U);
	razorFileWriterWrite(&file->writer, hdr, 8U);
	razorFileWriterWrite(&file->writer, paramMeta, 0x18U);
	razorFileWriterWrite(&file->writer, strPtr, len);
	return razorFileWriterWrite(&file->writer, &s_zeroPad, padLen);
}

/* --- Memory mapping table --- */

int razorCaptureFileAddMapping(RazorCaptureFile *file, uint32_t handle, uint32_t cpuAddr, uint32_t gpuAddr, uint32_t size)
{
	uint32_t count = file->mappingCount;

	if (count >= 6000U)
		return 1;

	file->mappings[count].handle = handle;
	file->mappings[count].cpuAddr = cpuAddr;
	file->mappings[count].gpuAddr = gpuAddr;
	file->mappings[count].size = size;
	file->mappingCount = count + 1U;
	return 0;
}

int razorCaptureFileRemoveMapping(RazorCaptureFile *file, uint32_t handle, uint32_t cpuAddr)
{
	uint32_t count = file->mappingCount;
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (file->mappings[i].handle == handle && file->mappings[i].cpuAddr == cpuAddr) {
			uint32_t j;
			for (j = i + 1U; j < count; j++)
				file->mappings[j - 1U] = file->mappings[j];
			file->mappingCount = count - 1U;
			return 0;
		}
	}
	return 2;
}

int razorCaptureFileIsMappingsFull(const RazorCaptureFile *file)
{
	return file->mappingCount == 6000U;
}

uint32_t razorCaptureFileGpuToCpu(const RazorCaptureFile *file, uint32_t gpuAddr)
{
	uint32_t count = file->mappingCount;
	uint32_t i;

	for (i = 0; i < count; i++) {
		uint32_t base = file->mappings[i].gpuAddr;
		if (gpuAddr >= base && gpuAddr < base + file->mappings[i].size)
			return gpuAddr + file->mappings[i].cpuAddr - base;
	}
	return 0U;
}

int razorCaptureFileFindCpuMapping(const RazorCaptureFile *file, uint32_t cpuAddr, uint32_t *outRemaining)
{
	uint32_t count = file->mappingCount;
	uint32_t i;

	for (i = 0; i < count; i++) {
		uint32_t base = file->mappings[i].cpuAddr;
		uint32_t end = base + file->mappings[i].size;
		if (cpuAddr >= base && cpuAddr < end) {
			*outRemaining = end - cpuAddr;
			return 1;
		}
	}
	return 0;
}

/* --- GXM shader program reflection & uniform buffer serializer --- */

int razorCaptureSerializeProgram(RazorProgramCache *progCache, RazorRangeList *ranges,
				 const SceGxmProgram *program, const void *pdsOut,
				 uint32_t drawId, RazorCaptureFile *file)
{
	const uint32_t *prog32 = (const uint32_t *)program;
	const uint16_t *prog16 = (const uint16_t *)program;
	uint8_t alreadyCached = 0;
	int err = 0;
	int bufErr = 0;
	int litErr = 0;

	if (program == NULL || sceGxmProgramCheck(program) != 0)
		return 256;

	razorProgramCacheInsert(progCache, (uint32_t)(uintptr_t)program, &alreadyCached);
	if (!alreadyCached) {
		uint32_t meta[32];
		uint32_t progSize;
		uint32_t paramCount;
		uint32_t i;

		sceClibMemset(meta, 0, sizeof(meta));
		meta[0] = (uint32_t)(uintptr_t)program;
		meta[1] = (uint32_t)(uintptr_t)program;
		meta[2] = prog32[3];
		meta[3] = prog32[4];
		meta[6] = prog16[28];
		meta[8] = prog32[6];
		meta[9] = 0;
		meta[11] = prog32[22];
		meta[12] = prog32[26];
		meta[13] = prog32[25];
		meta[14] = prog32[20];
		meta[15] = prog32[21];
		meta[16] = prog32[36];
		meta[17] = prog32[30];
		meta[18] = prog32[9];
		meta[20] = prog32[28];
		meta[22] = prog16[24];
		meta[23] = prog16[25];
		meta[24] = prog16[26];
		meta[25] = prog16[27];
		meta[27] = prog32[23];
		meta[28] = prog32[5] & 2U;

		if (prog32[5] & 1U) {
			uint32_t extOff = prog32[11];
			if (extOff == 0U) {
				err = 16;
				goto serialize_uniforms;
			}
			{
				const uint16_t *ext = (const uint16_t *)((const char *)program + extOff + 44);
				meta[26] = ext[4];
				meta[30] = ext[6];
				meta[31] = ext[7];
			}
		}

		progSize = sceGxmProgramGetSize(program);
		razorCaptureFileWriteProgram(file, meta, (uint32_t)(uintptr_t)program, progSize);

		paramCount = prog32[9];
		for (i = 0; i < paramCount; i++) {
			const SceGxmProgramParameter *param = sceGxmProgramGetParameter(program, i);
			uint32_t pword = ((const uint32_t *)param)[1];
			uint32_t cat = pword & 0xFU;
			uint32_t type = (pword >> 4) & 0xFU;
			uint32_t compCount = (pword >> 8) & 0xFU;
			uint32_t container = (pword >> 12) & 0xFU;
			uint32_t arrSize = sceGxmProgramParameterGetArraySize(param);
			uint32_t resIdx = sceGxmProgramParameterGetResourceIndex(param);
			uint32_t sem = (pword >> 24) & 0xFU;
			uint32_t isRef = (pword & 0x10000000U) != 0U;
			uint8_t semanticIdx = 0;
			uint8_t isBuf = 0;
			uint8_t isTex = 0;
			struct {
				uint32_t progPtr;
				uint32_t paramIdx;
				uint8_t catType;
				uint8_t compCont;
				uint8_t semIdx;
				uint8_t flags;
				uint32_t arraySize;
				uint32_t resourceIndex;
				uint32_t pad;
			} pmeta;
			const char *pname;

			if (cat == 0U) {
				semanticIdx = (uint8_t)((pword >> 16) & 0xFFU);
			} else if (cat == 4U) {
				isBuf = (pword & 0x10000U) != 0U;
				isTex = (pword & 0x20000U) != 0U;
			}

			pmeta.progPtr = meta[0];
			pmeta.paramIdx = i;
			pmeta.catType = (uint8_t)((cat & 0xFU) | ((type & 0xFU) << 4));
			pmeta.compCont = (uint8_t)((compCount & 0xFU) | ((container & 0xFU) << 4));
			pmeta.semIdx = semanticIdx;
			pmeta.flags = (uint8_t)((sem & 0xFU) | (isRef << 4) | (isBuf << 5) | (isTex << 6));
			pmeta.arraySize = arrSize;
			pmeta.resourceIndex = resIdx;
			pmeta.pad = 0;

			pname = sceGxmProgramParameterGetName(param);
			razorCaptureFileWriteProgramParam(file, &pmeta, pname);
		}
	}

serialize_uniforms:
	if (pdsOut == NULL)
		return err;

	/* Dependent buffer loads */
	{
		uint32_t depCount = prog32[30];
		uint32_t depOff = prog32[31];
		uint32_t litCount = prog32[36];
		uint32_t litOff = prog32[37];

		if (depCount != 0U) {
			if (depOff == 0U) {
				bufErr = 32;
			} else if (litCount == 0U || litOff == 0U) {
				bufErr = 16;
			} else {
				const uint16_t *litTable = (const uint16_t *)((const char *)program + litOff + 148);
				const uint16_t *litEnd = litTable + 4U * litCount;
				const uint16_t *baseEntry = NULL;
				const uint16_t *curLit;

				for (curLit = litTable; curLit < litEnd; curLit += 4) {
					if (curLit[0] == 19U) {
						baseEntry = curLit;
						break;
					}
				}
				if (baseEntry == NULL) {
					bufErr = 16;
				} else {
					uint16_t baseReg = baseEntry[2];
					const uint8_t *depPtr = (const uint8_t *)program + depOff + 124;
					const uint8_t *depEnd = depPtr + 8U * depCount;

					for (; depPtr < depEnd; depPtr += 8) {
						uint16_t bufIdx = *(const uint16_t *)(depPtr + 0);
						uint16_t regOff = *(const uint16_t *)(depPtr + 2);
						uint32_t byteOff = *(const uint32_t *)(depPtr + 4);
						uint32_t regVal = 0;
						uint32_t outType = 0;
						uint32_t outSize = 0;
						uint8_t clampMap = 0;
						int clampRange = 0;

						if (!sgxPdsOutputReadDmaReg(pdsOut, baseReg + regOff, &regVal)) {
							bufErr = 32;
							continue;
						}

						switch (bufIdx) {
						case 14U: outType = 0U; outSize = 4U * prog32[25]; break;
						case 15U: outType = 3U; outSize = 4U * prog32[24]; break;
						case 16U: outType = 2U; outSize = 4U * prog32[22]; break;
						case 18U: outType = 1U; outSize = 4U * prog32[21]; break;
						case 19U: outType = 4U; outSize = 4U * prog32[23]; break;
						default:
							if (bufIdx > 13U) {
								bufErr = 32;
								continue;
							}
							{
								const uint8_t *pbase = (prog32[10] != 0U)
									? ((const uint8_t *)program + prog32[10] + 40)
									: NULL;
								uint32_t pcnt = prog32[9];
								uint32_t k;
								int found = 0;

								for (k = 0; k < pcnt; k++) {
									const SceGxmProgramParameter *pp =
										(const SceGxmProgramParameter *)(pbase + 16U * k);
									uint32_t pw = ((const uint32_t *)pp)[1];
									if ((pw & 0xFU) == 4U &&
									    sceGxmProgramParameterGetResourceIndex(pp) == bufIdx) {
										outType = bufIdx + 256U;
										outSize = ((const uint32_t *)pp)[2];
										clampRange = (pw & 0x10000U) != 0U;
										clampMap = 1;
										found = 1;
										break;
									}
								}
								if (!found)
									continue;
							}
							break;
						}

						if (outSize != 0U) {
							uint32_t gpuAddr = regVal - byteOff;
							uint32_t cpuAddr = razorCaptureFileGpuToCpu(file, gpuAddr);
							if (cpuAddr != 0U) {
								if (clampRange) {
									uint32_t endAddr = 0;
									if (razorRangeListFindEnd(ranges, cpuAddr, &endAddr))
										outSize = endAddr - cpuAddr;
								}
								razorCaptureFileWriteUniformBuf(file, drawId, gpuAddr, cpuAddr,
												outSize, outType,
												(uint16_t)(baseReg + regOff),
												byteOff, clampMap);
							}
						}
					}
				}
			}
		}

		/* Literal / direct DMA uniform loads */
		if (litCount != 0U) {
			if (litOff == 0U) {
				litErr = 32;
			} else {
				const uint16_t *litPtr = (const uint16_t *)((const char *)program + litOff + 148);
				const uint16_t *litEnd = litPtr + 4U * litCount;
				const uint32_t *pds32 = (const uint32_t *)pdsOut;
				uint32_t dmaCount = pds32[84];

				for (; litPtr < litEnd; litPtr += 4) {
					uint16_t bufIdx = litPtr[0];
					uint16_t regOff = litPtr[1];
					uint16_t dmaReg = litPtr[2];
					uint16_t regCnt = litPtr[3];
					uint32_t outType;
					uint8_t clampMap = 0;
					uint32_t d;
					int foundDma = 0;

					switch (bufIdx) {
					case 14U: outType = 0U; break;
					case 15U: outType = 3U; break;
					case 16U: outType = 2U; break;
					case 18U: outType = 1U; break;
					case 19U: outType = 4U; break;
					default:
						if (bufIdx > 13U) {
							litErr = 32;
							continue;
						}
						outType = bufIdx + 256U;
						clampMap = 1;
						break;
					}

					for (d = 0; d < dmaCount; d++) {
						const uint32_t *dmaEntry = &pds32[86 + 2U * d];
						uint32_t diff = 0;
						if (sgxDmaFindReg(dmaEntry, dmaReg, &diff)) {
							uint32_t gpuAddr = dmaEntry[0] + 4U * (diff - regOff);
							uint32_t cpuAddr = razorCaptureFileGpuToCpu(file, gpuAddr);
							if (cpuAddr != 0U) {
								razorCaptureFileWriteLiteralUniform(file, drawId, gpuAddr, cpuAddr,
												    outType, regOff, dmaReg, regCnt, clampMap);
								foundDma = 1;
							}
							break;
						}
					}
					if (!foundDma)
						litErr = 32;
				}
			}
		}
	}

	return err | bufErr | litErr;
}
