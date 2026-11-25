/*
 * SceRazorCapture: VDM/PDS/USSE command stream analyser and memory dump writer.
 */

#include "razor_capture_internal.h"

/* External helpers in sgx_decode.c, usse_decode.c, capture_file.c */
extern void sgxRegBankReset(SgxRegBank *bank);
extern int sgxRegBankRead(const SgxRegBank *bank, uint32_t regIdx, uint32_t *outVal);
extern void *sgxPdsProgramInit(SgxPdsProgram *prog);
extern int sgxPdsProgramScan(SgxPdsProgram *prog, uint32_t *codePtr, uint32_t gpuAddr, uint32_t dataSize, int maxBytes);
extern void sgxPdsVmSetupWithIr(void *vm, const SgxPdsProgram *prog, uint32_t ir0, uint32_t ir1, uint8_t isSigned);
extern void sgxPdsVmSetupA(void *vm, const SgxPdsProgram *prog);
extern void sgxPdsVmSetupB(void *vm, const SgxPdsProgram *prog);
extern int sgxPdsVmRun(void *vm, void *output, void *callback);
extern int sgxPdsOutputApplyToRegBank(const void *pdsOut, SgxRegBank *bank);
extern void sgxUsseVmSetProgram(void *vm, const SgxUsseProgramInfo *program, SgxRegBank *paBank);
extern uint32_t sgxUsseVmRun(void *vm, void *emitOut);
extern int razorCaptureFileWriteJobHeader(RazorCaptureFile *file, const void *jobMeta, const void *regs, const uint32_t *kickInfo);
extern void razorCaptureFileMarkAddrSize(RazorCaptureFile *file, uint32_t cpuAddr, uint32_t size);
extern void razorCaptureFileMark8Bytes(RazorCaptureFile *file, uint32_t cpuAddr);
extern void razorCaptureFileMarkVdmCtrl(RazorCaptureFile *file, uint32_t cpuAddr);
extern void razorCaptureFileMark4Bytes(RazorCaptureFile *file, uint32_t cpuAddr);
extern void razorCaptureFileMarkData(RazorCaptureFile *file, uint32_t cpuAddr, uint32_t size);
extern void razorCaptureFileMarkPdsProgram(RazorCaptureFile *file, const SgxPdsProgram *prog);
extern void razorCaptureFileMarkUsseProgram(RazorCaptureFile *file, const SgxUsseProgramInfo *info);
extern void razorCaptureFileMarkBuffer(RazorCaptureFile *file, uint32_t cpuAddr, uint32_t size);
extern uint64_t razorCaptureFileMarkTexture(RazorCaptureFile *file, const uint32_t *texWords);
extern int razorCaptureFileWritePdsState(RazorCaptureFile *file, const void *data);
extern int razorCaptureFileWriteDrawState(RazorCaptureFile *file, const void *data);
extern int razorCaptureSerializeProgram(RazorProgramCache *progCache, RazorRangeList *ranges,
					const SceGxmProgram *program, const void *pdsOut,
					uint32_t drawId, RazorCaptureFile *file);

/* Static constructor constants (initialized in .init_array at 0x81002578) */
static uint32_t s_initGuard0;
static uint32_t s_initGuard1;
static uint32_t s_initGuard2;
static uint32_t s_const448;
static uint32_t s_const12288;
static uint32_t s_const3584;

void razorAnalyseStaticInit(void)
{
	if ((s_initGuard2 & 1U) == 0U) {
		s_const3584 = 3584U;
		s_initGuard2 = 1U;
	}
	if ((s_initGuard1 & 1U) == 0U) {
		s_const12288 = 12288U;
		s_initGuard1 = 1U;
	}
	if ((s_initGuard0 & 1U) == 0U) {
		s_const448 = 448U;
		s_initGuard0 = 1U;
	}
}

/* --- PDS output & analyser context layouts --- */

typedef struct RazorPdsDmaPtrList {
	uint32_t count;
	uint32_t pad;
	uint32_t ptrs[50];
} RazorPdsDmaPtrList;

typedef struct RazorPdsResult {
	uint32_t douttCount;
	uint32_t douttPad;
	uint32_t doutt[64];
	uint32_t doutaCount;
	uint32_t doutaPad;
	uint32_t douta[4][4];
	uint32_t doutdCount;
	uint32_t doutdPad;
	uint32_t doutd[50][2];
	uint32_t doutuCount;
	uint32_t doutuPad;
	uint32_t doutu[32][4];
	uint32_t doutcCount;
	uint32_t doutcPad;
	uint32_t doutc[6][2];
	RazorPdsDmaPtrList dmaPtrs;
} RazorPdsResult;

typedef struct RazorUsseProgList {
	uint32_t count;
	uint32_t pad;
	SgxUsseProgramInfo progs[18];
} RazorUsseProgList;

typedef struct RazorTagEmitList {
	uint32_t count;
	uint32_t pad;
	SgxTagState entries[3];
} RazorTagEmitList;

/* Exact layout matching offsets in sub_81002830 / sub_81003ED0 / sub_810040EC */
typedef struct RazorAnalyserCtx {
	uint32_t jobId;                  /* +0 */
	uint32_t jobFlags;               /* +4 */
	uint32_t vdmCtrlCpuAddr;         /* +8 */
	uint32_t jobWord3;               /* +12 */
	uint32_t jobWord4;               /* +16 */
	uint32_t pdsBaseGpuAddr;         /* +20 */
	uint32_t useBaseGpuAddrs[16];    /* +24 .. +87 */
	RazorCaptureFile file;           /* +88 .. +122487 */
	uint64_t errorFlags;             /* +122488 (dword 30622) */
	uint32_t pdsVm0[136];            /* +122496 (dword 30624) */
	RazorUsseProgList usseList0;     /* +123040 (dword 30760) */
	RazorTagEmitList tagEmitList;    /* +123768 (dword 30942) */
	uint32_t usseVm[2068];           /* +124136 (dword 31034) */
	SgxRegBank paBank;               /* +132408 (dword 33102) */
	RazorPdsResult pdsRes0;          /* +134520 (dword 33630) */
	uint32_t pdsVm1[136];            /* +136048 (dword 34012) */
	RazorUsseProgList usseList1;     /* +136592 (dword 34148) */
	RazorUsseProgList usseList2;     /* +137320 (dword 34330) */
	RazorPdsResult pdsRes1;          /* +138048 (dword 34512) */
	uint32_t pdsVm2[136];            /* +139576 (dword 34894) */
	uint32_t pdsRes2Body[330];       /* +140120 (dword 35030) */
	RazorUsseProgList usseList3;     /* +141440 (dword 35360) */
	RazorPdsDmaPtrList pdsRes2Dma;   /* +142168 (dword 35542) */
	uint32_t pdsVm3[136];            /* +142376 (dword 35594) */
	uint32_t pdsRes3Body[330];       /* +142920 (dword 35730) */
	RazorUsseProgList usseList4;     /* +144240 (dword 36060) */
	RazorPdsDmaPtrList pdsRes3Dma;   /* +144968 (dword 36242) */
	RazorPdsResult saPdsRes;         /* +145176 (dword 36294) */
	SgxRegBank saBank;               /* +146704 (dword 36676) */
	uint32_t saPdsGpuAddr;           /* +148816 (dword 37204) */
	uint32_t saPdsPad;               /* +148820 (dword 37205) */
	RazorPdsResult fragPdsRes;       /* +148824 (dword 37206) */
	SgxRegBank fragBank;             /* +150352 (dword 37588) */
	uint32_t mergedTagState[26];     /* +152464 (dword 38116) */
	RazorProgramCache progCache;     /* +152568 (dword 38142) */
	RazorRangeList ranges;           /* +152704 (dword 38176) */
} RazorAnalyserCtx;

static inline __attribute__((always_inline)) void razorClearPdsResult(RazorPdsResult *r)
{
	r->douttCount = 0;
	r->doutaCount = 0;
	r->doutdCount = 0;
	r->doutuCount = 0;
	r->doutcCount = 0;
	r->dmaPtrs.count = 0;
}

static inline __attribute__((always_inline)) void razorScanUsseTextures(RazorAnalyserCtx *ctx, const RazorUsseProgList *list, const SgxRegBank *bank)
{
	uint32_t i, j, k;
	int allOk = 1;

	for (i = 0; i < list->count; i++) {
		int texRegs[66];
		texRegs[0] = 0;
		sgxFindUsseTexSamples(&list->progs[i], texRegs, 1);
		for (j = 0; j < (uint32_t)texRegs[0]; j++) {
			uint32_t texWords[4];
			int reg = texRegs[j + 2];
			int ok = 1;
			for (k = 0; k < 4U; k++) {
				if (!sgxRegBankRead(bank, (uint32_t)(reg + (int)k), &texWords[k])) {
					ok = 0;
					break;
				}
			}
			if (ok) {
				ctx->errorFlags |= razorCaptureFileMarkTexture(&ctx->file, texWords);
			} else {
				ctx->errorFlags |= 4ULL;
				allOk = 0;
			}
		}
	}
	if (!allOk)
		ctx->errorFlags |= 4ULL;
}

/* --- 0x81002600: process PDS DMA/texture/USSE outputs --- */

void razorProcessPdsOutput(const RazorPdsResult *pdsOut, RazorUsseProgList *usseList,
			   RazorPdsDmaPtrList *dmaList, RazorAnalyserCtx *ctx)
{
	uint32_t i;

	for (i = 0; i < pdsOut->doutdCount; i++) {
		uint32_t cpuAddr = razorCaptureFileGpuToCpu(&ctx->file, pdsOut->doutd[i][0]);
		uint32_t byteSize = sgxDmaByteSize(pdsOut->doutd[i]);
		if (cpuAddr != 0U)
			razorCaptureFileMarkBuffer(&ctx->file, cpuAddr, byteSize);
		else
			ctx->errorFlags |= 1ULL;

		if (dmaList->count <= 49U) {
			dmaList->ptrs[dmaList->count] = cpuAddr;
			dmaList->count++;
		}
	}

	for (i = 0; i < pdsOut->doutuCount; i++)
		ctx->errorFlags |= razorCaptureFileMarkTexture(&ctx->file, pdsOut->doutu[i]);

	for (i = 0; i < pdsOut->doutaCount; i++) {
		uint32_t w0 = pdsOut->douta[i][0];
		uint32_t base = ctx->useBaseGpuAddrs[w0 & 0xFU];
		uint32_t gpuAddr, cpuAddr;

		if (base == 0U)
			continue;
		gpuAddr = base + 8U * ((w0 >> 4) & 0xFFFFFU);
		cpuAddr = razorCaptureFileGpuToCpu(&ctx->file, gpuAddr);
		if (cpuAddr == 0U) {
			ctx->errorFlags |= 1ULL;
			continue;
		}
		if (usseList->count <= 17U) {
			SgxUsseProgramInfo *info = &usseList->progs[usseList->count];
			sceClibMemset(info, 0, sizeof(*info));
			usseList->count++;
			if (sgxScanUsseProgram(info, (uint64_t *)(uintptr_t)cpuAddr, gpuAddr, -65536, 0x10000, 1))
				razorCaptureFileMarkUsseProgram(&ctx->file, info);
		}
	}
}

/* --- 0x81002830: VDM command stream analyser --- */

void razorAnalyseVdmStream(RazorAnalyserCtx *ctx, const uint32_t *jobDesc)
{
	uint32_t *vdmPtr = (uint32_t *)(uintptr_t)ctx->vdmCtrlCpuAddr;

	if (vdmPtr == NULL) {
		ctx->errorFlags |= 1ULL;
		return;
	}

	sceClibPrintf("razor_capture: Analysing command stream...\n");
	razorCaptureFileWriteJobHeader(&ctx->file, ctx, ctx->useBaseGpuAddrs, (const uint32_t *)(uintptr_t)jobDesc[5]);

	ctx->saPdsGpuAddr = 0;
	razorClearPdsResult(&ctx->saPdsRes);
	sgxRegBankReset(&ctx->saBank);
	razorZeroInit(ctx->mergedTagState);

	while (1) {
		uint32_t hdr;
		uint32_t cmdType;

		sceClibPrintf(".");
		hdr = vdmPtr[0];
		cmdType = hdr & 0xC0000000U;

		if (cmdType == 0x40000000U) {
			/* PPP / TA state update (PDS state block) */
			uint32_t *nextPtr = vdmPtr + 2;
			uint32_t pdsGpuAddr = hdr * 16U;
			uint32_t pdsCpuAddr;
			uint32_t word1 = vdmPtr[1];
			int isSa = (word1 & 0x100U) != 0U;
			SgxPdsProgram pdsProg;
			RazorPdsResult *pdsRes;
			SgxRegBank *regBank;
			uint32_t i;

			razorCaptureFileMark8Bytes(&ctx->file, (uint32_t)(uintptr_t)vdmPtr);
			sgxPdsProgramInit(&pdsProg);
			pdsCpuAddr = razorCaptureFileGpuToCpu(&ctx->file, pdsGpuAddr);
			if (pdsCpuAddr == 0U) {
				ctx->errorFlags |= 1ULL;
				vdmPtr = nextPtr;
				continue;
			}

			sgxPdsProgramScan(&pdsProg, (uint32_t *)(uintptr_t)pdsCpuAddr, pdsGpuAddr,
					  16U * (word1 >> 27), 0x4000);
			razorCaptureFileMarkPdsProgram(&ctx->file, &pdsProg);

			razorClearPdsResult(&ctx->pdsRes0);
			ctx->usseList0.count = 0;
			ctx->tagEmitList.count = 0;

			if (isSa) {
				pdsRes = &ctx->saPdsRes;
				ctx->saPdsGpuAddr = pdsGpuAddr;
				regBank = &ctx->saBank;
			} else {
				pdsRes = &ctx->pdsRes0;
				regBank = &ctx->paBank;
			}
			razorClearPdsResult(pdsRes);

			sgxPdsVmSetupWithIr(ctx->pdsVm0, &pdsProg, 0, 0, 0);
			sgxPdsVmRun(ctx->pdsVm0, pdsRes, NULL);
			razorProcessPdsOutput(pdsRes, &ctx->usseList0, &pdsRes->dmaPtrs, ctx);

			sgxRegBankReset(regBank);
			if (!sgxPdsOutputApplyToRegBank(pdsRes, regBank))
				ctx->errorFlags |= 2ULL;

			for (i = 0; i < ctx->usseList0.count; i++) {
				uint32_t progHash, progPtr;
				uint8_t hasProg;
				if (sgxUsseGetFooter(&ctx->usseList0.progs[i], &progHash, &hasProg, &progPtr)) {
					ctx->errorFlags |= (uint64_t)(uint32_t)razorCaptureSerializeProgram(
						&ctx->progCache, &ctx->ranges,
						(const SceGxmProgram *)(uintptr_t)progHash,
						NULL, 0U, &ctx->file);
				}
			}

			if (isSa)
				razorScanUsseTextures(ctx, &ctx->usseList0, regBank);

			for (i = 0; i < ctx->usseList0.count; i++) {
				uint32_t t;
				sgxUsseVmSetProgram(ctx->usseVm, &ctx->usseList0.progs[i], regBank);
				sgxUsseVmRun(ctx->usseVm, &ctx->tagEmitList);
				sgxRegBankReset(&ctx->fragBank);

				for (t = 0; t < ctx->tagEmitList.count; t++) {
					SgxTagState *ts = &ctx->tagEmitList.entries[t];
					ctx->usseList1.count = 0;
					ctx->usseList2.count = 0;
					razorClearPdsResult(&ctx->pdsRes1);
					sgxTagStateMerge(ts, ctx->mergedTagState);

					if (ts->words[0] & 0x40U) {
						uint32_t w8 = ts->words[8];
						uint32_t w9 = ts->words[9];
						uint32_t w7 = ts->words[7];
						uint32_t off0 = 16U * (w9 & 0x7FFFFFFU);
						uint32_t off1 = 16U * (w7 & 0x7FFFFFFU);

						if (((w8 & 0x2000U) != 0U || off0 != 0U) &&
						    ((w8 & 0x4000U) != 0U || off1 != 0U)) {
							uint32_t addrs[2];
							uint32_t dataSizes[2];
							int pass;

							if (w8 & 0x2000U)
								off0 |= 0x80000000U;
							if (w8 & 0x4000U)
								off1 |= 0x80000000U;
							addrs[0] = off0 + ctx->pdsBaseGpuAddr;
							addrs[1] = off1 + ctx->pdsBaseGpuAddr;
							dataSizes[0] = 16U * (w9 >> 27);
							dataSizes[1] = 16U * (w7 >> 27);

							for (pass = 1; pass >= 0; pass--) {
								uint32_t fragCpu = razorCaptureFileGpuToCpu(&ctx->file, addrs[pass]);
								if (fragCpu != 0U) {
									SgxPdsProgram fprog;
									sgxPdsProgramInit(&fprog);
									sgxPdsProgramScan(&fprog, (uint32_t *)(uintptr_t)fragCpu,
											  addrs[pass], dataSizes[pass], 0x4000);
									razorCaptureFileMarkPdsProgram(&ctx->file, &fprog);

									if (pass == 1) {
										razorClearPdsResult(&ctx->fragPdsRes);
										sgxPdsVmSetupWithIr(ctx->pdsVm1, &fprog, 0, 0, 0);
										sgxPdsVmRun(ctx->pdsVm1, &ctx->fragPdsRes, NULL);
										razorProcessPdsOutput(&ctx->fragPdsRes, &ctx->usseList2,
												      &ctx->fragPdsRes.dmaPtrs, ctx);
										if (!sgxPdsOutputApplyToRegBank(&ctx->fragPdsRes, &ctx->fragBank))
											ctx->errorFlags |= 2ULL;
									} else {
										uint32_t u;
										razorClearPdsResult(&ctx->pdsRes1);
										sgxPdsVmSetupWithIr(ctx->pdsVm1, &fprog, 0, 0, 0);
										sgxPdsVmRun(ctx->pdsVm1, &ctx->pdsRes1, NULL);
										razorProcessPdsOutput(&ctx->pdsRes1, &ctx->usseList1,
												      &ctx->pdsRes1.dmaPtrs, ctx);
										for (u = 0; u < ctx->usseList1.count; u++) {
											uint32_t ph, pp;
											uint8_t hp;
											if (sgxUsseGetFooter(&ctx->usseList1.progs[u], &ph, &hp, &pp)) {
												ctx->errorFlags |= (uint64_t)(uint32_t)razorCaptureSerializeProgram(
													&ctx->progCache, &ctx->ranges,
													(const SceGxmProgram *)(uintptr_t)ph,
													&ctx->fragPdsRes, addrs[1], &ctx->file);
											}
										}
									}
								} else {
									ctx->errorFlags |= 1ULL;
								}
							}

							razorScanUsseTextures(ctx, &ctx->usseList1, &ctx->fragBank);
							razorScanUsseTextures(ctx, &ctx->usseList2, &ctx->fragBank);
						}
					}
				}
			}

			vdmPtr = nextPtr;
			continue;
		}

		if (cmdType == 0x80000000U) {
			/* Index list / draw call */
			uint32_t cmdBytes = 8U;
			uint32_t *nextPtr;
			uint32_t idxCpuAddr;
			uint32_t idxCount;
			uint32_t idxStride;

			if (hdr & 0x00400000U)
				cmdBytes += 4U;
			if (hdr & 0x00800000U)
				cmdBytes += 4U;
			if (hdr & 0x01000000U)
				cmdBytes += 8U;
			if (hdr & 0x02000000U)
				cmdBytes += 8U;
			nextPtr = (uint32_t *)((uintptr_t)vdmPtr + cmdBytes);

			razorCaptureFileMarkVdmCtrl(&ctx->file, (uint32_t)(uintptr_t)vdmPtr);
			idxCpuAddr = razorCaptureFileGpuToCpu(&ctx->file, vdmPtr[1] & ~1U);
			if (idxCpuAddr == 0U) {
				ctx->errorFlags |= 1ULL;
				vdmPtr = nextPtr;
				continue;
			}

			if ((hdr & 0x00800000U) == 0U ||
			    (idxCount = vdmPtr[2 + ((hdr & 0x00400000U) != 0U)] & 0x3FFFFFU) == 0U)
				idxCount = hdr & 0x3FFFFFU;

			idxStride = (vdmPtr[2] & 0x01000000U) ? 4U : 2U;
			razorCaptureFileMarkData(&ctx->file, idxCpuAddr, idxCount * idxStride);

			if (hdr & 0x01000000U) {
				uint32_t pdsWordOff = 2U + ((hdr & 0x00400000U) != 0U) + ((hdr & 0x00800000U) != 0U);
				uint32_t vpdsGpu = vdmPtr[pdsWordOff] * 16U;
				uint32_t vpdsCpu = razorCaptureFileGpuToCpu(&ctx->file, vpdsGpu);
				SgxPdsProgram vpdsProg;
				uint32_t instMod = 0x7FFFFFFFU;
				uint32_t vtxCount = hdr & 0x3FFFFFU;
				uint32_t minIdx = 0xFFFFFFFFU, maxIdx = 0U;
				uint32_t minDmaAddrs[16], maxDmaAddrs[16];
				uint32_t minDmaCount, i;
				RazorPdsResult *vpdsRes = (RazorPdsResult *)ctx->pdsRes3Body;

				if (vpdsCpu == 0U) {
					ctx->errorFlags |= 1ULL;
					vdmPtr = nextPtr;
					continue;
				}

				sgxPdsProgramInit(&vpdsProg);
				sgxPdsProgramScan(&vpdsProg, (uint32_t *)(uintptr_t)vpdsCpu, vpdsGpu,
						  16U * (vdmPtr[pdsWordOff + 1U] & 0x1FU), 0x4000);
				razorCaptureFileMarkPdsProgram(&ctx->file, &vpdsProg);

				if (hdr & 0x00800000U) {
					uint32_t m = vdmPtr[2 + ((hdr & 0x00400000U) != 0U)] & 0x3FFFFFU;
					if (m != 0U)
						instMod = m;
				}

				if (vtxCount != 0U) {
					int isQuad = (hdr & 0x3C000000U) == 0x14000000U;
					int is32Bit = (vdmPtr[2] & 0x01000000U) != 0U;

					for (i = 0; i < vtxCount; i++) {
						uint32_t idx;
						if (isQuad && (i & 3U) == 3U)
							continue;
						if (is32Bit)
							idx = ((const uint32_t *)(uintptr_t)idxCpuAddr)[i % instMod];
						else
							idx = ((const uint16_t *)(uintptr_t)idxCpuAddr)[i % instMod];
						if (idx < minIdx)
							minIdx = idx;
						if (idx > maxIdx)
							maxIdx = idx;
					}
				}

				/* Run vertex PDS with minIdx / instance 0 */
				vpdsRes->douttCount = 0;
				vpdsRes->doutaCount = 0;
				vpdsRes->doutdCount = 0;
				vpdsRes->doutuCount = 0;
				vpdsRes->doutcCount = 0;
				ctx->usseList4.count = 0;
				ctx->pdsRes3Dma.count = 0;

				sgxPdsVmSetupWithIr(ctx->pdsVm3, &vpdsProg, minIdx, 0U, 0);
				sgxPdsVmRun(ctx->pdsVm3, vpdsRes, NULL);
				razorProcessPdsOutput(vpdsRes, &ctx->usseList4, &ctx->pdsRes3Dma, ctx);

				minDmaCount = ctx->pdsRes3Dma.count;
				for (i = 0; i < minDmaCount && i < 16U; i++)
					minDmaAddrs[i] = ctx->pdsRes3Dma.ptrs[i];

				/* Run vertex PDS with maxIdx / maxInstance */
				vpdsRes->douttCount = 0;
				vpdsRes->doutaCount = 0;
				vpdsRes->doutdCount = 0;
				vpdsRes->doutuCount = 0;
				vpdsRes->doutcCount = 0;
				ctx->usseList4.count = 0;
				ctx->pdsRes3Dma.count = 0;

				sgxPdsVmSetupWithIr(ctx->pdsVm3, &vpdsProg, maxIdx, (vtxCount - 1U) / instMod, 0);
				sgxPdsVmRun(ctx->pdsVm3, vpdsRes, NULL);
				razorProcessPdsOutput(vpdsRes, &ctx->usseList4, &ctx->pdsRes3Dma, ctx);

				razorScanUsseTextures(ctx, &ctx->usseList4, &ctx->saBank);

				for (i = 0; i < minDmaCount && i < 16U; i++)
					maxDmaAddrs[i] = ctx->pdsRes3Dma.ptrs[i];
				for (i = 0; i < minDmaCount; i++) {
					uint32_t elemBytes = sgxDmaByteSize(vpdsRes->doutd[i]) & ~3U;
					razorCaptureFileMarkRange(&ctx->file, minDmaAddrs[i], maxDmaAddrs[i] + elemBytes);
				}

				if (vtxCount != 0U) {
					for (i = 0; i < ctx->usseList4.count; i++) {
						uint32_t ph, pp;
						uint8_t hp;
						if (sgxUsseGetFooter(&ctx->usseList4.progs[i], &ph, &hp, &pp)) {
							ctx->errorFlags |= (uint64_t)(uint32_t)razorCaptureSerializeProgram(
								&ctx->progCache, &ctx->ranges,
								(const SceGxmProgram *)(uintptr_t)ph,
								&ctx->saPdsRes, ctx->saPdsGpuAddr, &ctx->file);
						}
					}
				}
			}

			vdmPtr = nextPtr;
			continue;
		}

		if (cmdType == 0xC0000000U) {
			/* Terminate */
			razorCaptureFileMark4Bytes(&ctx->file, (uint32_t)(uintptr_t)vdmPtr);
			return;
		}

		if (cmdType == 0x00000000U) {
			/* Stream link */
			uint32_t targetCpu = razorCaptureFileGpuToCpu(&ctx->file, hdr * 4U);
			uint32_t remaining = 0;
			uint32_t markLen;

			if (targetCpu == 0U || targetCpu == (uint32_t)(uintptr_t)vdmPtr) {
				ctx->errorFlags |= 1ULL;
				return;
			}
			if (!razorCaptureFileFindCpuMapping(&ctx->file, (uint32_t)(uintptr_t)vdmPtr, &remaining))
				return;

			if (remaining <= 7U) {
				markLen = (remaining < 4U) ? remaining : 4U;
			} else {
				uint32_t dist = targetCpu - (uint32_t)(uintptr_t)vdmPtr;
				switch (vdmPtr[1]) {
				case 0xC0DEBF00U:
				case 0xC0DEBF01U:
				case 0xC0DEBF02U:
				case 0xC0DEBF03U:
				case 0xC0DEBF04U:
				case 0xC0DEBF06U:
					markLen = (dist == 8U) ? 8U : 4U;
					break;
				case 0xC0DEBF05U:
				case 0xC0DEBF07U:
					if ((uint32_t)(uintptr_t)vdmPtr < targetCpu && dist <= 0x48U)
						markLen = (remaining < dist) ? remaining : dist;
					else
						markLen = 4U;
					break;
				case 0xC0DEBF08U:
					markLen = 8U;
					break;
				case 0xC0DEBF09U:
					markLen = (remaining < 12U) ? remaining : 12U;
					break;
				default:
					markLen = 4U;
					break;
				}
			}

			razorCaptureFileMarkAddrSize(&ctx->file, (uint32_t)(uintptr_t)vdmPtr, markLen);
			vdmPtr = (uint32_t *)(uintptr_t)targetCpu;
			if (vdmPtr == NULL)
				return;
		}
	}
}

/* --- 0x81003dc0: write draw/3D scene state chunk --- */

int razorAnalyseWrite3dState(RazorAnalyserCtx *ctx, const uint8_t *kickInfo)
{
	uint32_t dst[22];

	sceClibMemset(dst, 0, sizeof(dst));
	dst[0] = *(const uint32_t *)(kickInfo + 124);
	dst[1] = *(const uint32_t *)(kickInfo + 128);
	dst[2] = *(const uint32_t *)(kickInfo + 132);
	dst[3] = *(const uint32_t *)(kickInfo + 136);
	dst[4] = *(const uint32_t *)(kickInfo + 156);
	dst[5] = *(const uint32_t *)(kickInfo + 164);
	dst[6] = *(const uint32_t *)(kickInfo + 140);
	dst[7] = *(const uint32_t *)(kickInfo + 144);
	dst[8] = *(const uint32_t *)(kickInfo + 148);
	dst[9] = *(const uint32_t *)(kickInfo + 152);
	dst[10] = *(const uint32_t *)(kickInfo + 160);
	dst[11] = *(const uint32_t *)(kickInfo + 168);
	dst[12] = *(const uint32_t *)(kickInfo + 228);
	((uint16_t *)dst)[26] = *(const uint16_t *)(kickInfo + 32);
	((uint16_t *)dst)[27] = *(const uint16_t *)(kickInfo + 34);
	dst[14] = *(const uint32_t *)(kickInfo + 100);
	dst[15] = *(const uint32_t *)(kickInfo + 60);
	dst[16] = *(const uint32_t *)(kickInfo + 64);
	dst[17] = *(const uint32_t *)(kickInfo + 44);
	dst[18] = *(const uint32_t *)(kickInfo + 48);
	dst[19] = *(const uint32_t *)(kickInfo + 52);
	dst[20] = *(const uint32_t *)(kickInfo + 96);
	dst[21] = *(const uint32_t *)(kickInfo + 120);
	return razorCaptureFileWriteDrawState(&ctx->file, dst);
}

/* --- 0x81003ed0: analyse 3D background/event PDS programs and write PDS state --- */

int razorAnalyse3dPdsState(RazorAnalyserCtx *ctx, const uint32_t *kickInfo)
{
	uint32_t w60 = kickInfo[60];
	uint32_t w58 = kickInfo[58];
	uint32_t w59 = kickInfo[59];
	uint32_t off0 = (16U * (w60 & 0x7FFFFFFU)) | ((w59 & 0x2000U) ? 0x80000000U : 0U);
	uint32_t off1 = (16U * (w58 & 0x7FFFFFFU)) | ((w59 & 0x4000U) ? 0x80000000U : 0U);
	uint32_t ds0 = 16U * (w60 >> 27);
	uint32_t ds1 = 16U * (w58 >> 27);
	int pass;
	uint32_t jobInfo[19];
	uint32_t pdsChunk[12];

	for (pass = 0; pass < 2; pass++) {
		uint32_t gpuAddr = ctx->pdsBaseGpuAddr + ((pass == 0) ? off0 : off1);
		uint32_t dataSize = (pass == 0) ? ds0 : ds1;
		uint32_t cpuAddr = razorCaptureFileGpuToCpu(&ctx->file, gpuAddr);

		if (cpuAddr != 0U) {
			SgxPdsProgram prog;
			sgxPdsProgramInit(&prog);
			sgxPdsProgramScan(&prog, (uint32_t *)(uintptr_t)cpuAddr, gpuAddr, dataSize, 0x4000);
			razorCaptureFileMarkPdsProgram(&ctx->file, &prog);
			sgxPdsVmSetupB(ctx->pdsVm2, &prog);
			sgxPdsVmRun(ctx->pdsVm2, ctx->pdsRes2Body, NULL);
			razorProcessPdsOutput((const RazorPdsResult *)ctx->pdsRes2Body,
					      &ctx->usseList3, &ctx->pdsRes2Dma, ctx);
		} else {
			ctx->errorFlags |= 1ULL;
		}
	}

	razorSalvageQueryJobInfo(kickInfo[188], jobInfo);
	sceClibMemset(pdsChunk, 0, sizeof(pdsChunk));
	pdsChunk[0] = kickInfo[9];
	pdsChunk[1] = jobInfo[3];
	pdsChunk[2] = kickInfo[10];
	pdsChunk[3] = kickInfo[26];
	pdsChunk[4] = kickInfo[58];
	pdsChunk[5] = kickInfo[59];
	pdsChunk[6] = kickInfo[60];
	pdsChunk[7] = 16U * (jobInfo[6] & 0xFFFFFFU);
	pdsChunk[8] = (jobInfo[6] >> 24) & 0x3FU;
	pdsChunk[9] = 0U;
	pdsChunk[10] = jobInfo[4];
	pdsChunk[11] = jobInfo[5];
	return razorCaptureFileWritePdsState(&ctx->file, pdsChunk);
}

/* --- 0x810040ec: analyse single PDS program (e.g. pixel event / EOT) --- */

void razorAnalyseSinglePds(RazorAnalyserCtx *ctx, uint32_t gpuAddr, uint32_t dataSize)
{
	uint32_t cpuAddr = razorCaptureFileGpuToCpu(&ctx->file, gpuAddr);
	RazorPdsResult *res = (RazorPdsResult *)ctx->pdsRes2Body;

	if (cpuAddr == 0U) {
		ctx->errorFlags |= 1ULL;
		return;
	}

	{
		SgxPdsProgram prog;
		sgxPdsProgramInit(&prog);
		sgxPdsProgramScan(&prog, (uint32_t *)(uintptr_t)cpuAddr, gpuAddr, dataSize, 0x4000);
		razorCaptureFileMarkPdsProgram(&ctx->file, &prog);
		sgxPdsVmSetupA(ctx->pdsVm2, &prog);
		res->douttCount = 0;
		res->doutaCount = 0;
		res->doutdCount = 0;
		res->doutuCount = 0;
		res->doutcCount = 0;
		ctx->usseList3.count = 0;
		ctx->pdsRes2Dma.count = 0;
		sgxPdsVmRun(ctx->pdsVm2, res, NULL);
		razorProcessPdsOutput(res, &ctx->usseList3, &ctx->pdsRes2Dma, ctx);
	}
}

/* --- 0x81004224: clear page directory --- */

void razorPageDirClear(RazorPageBlock **pageDir)
{
	int i;

	for (i = 0; i < 256; i++) {
		if (pageDir[i] != NULL) {
			pageDir[i]->inUse = 0;
			pageDir[i] = NULL;
		}
	}
}

/* --- 0x8100424c & 0x81004470: write marked memory pages to capture file --- */

static inline uint32_t razorWriterStatusToErr(int st)
{
	if (st == 1)
		return 64U;
	if (st == 2)
		return 128U;
	return 0U;
}

uint64_t razorWriteMemChunk(RazorFileWriter *writer, uint32_t startAddr, uint32_t endAddr)
{
	uint32_t alignedStart = startAddr & ~3U;
	uint32_t alignedEnd = (endAddr + 3U) & ~3U;
	uint32_t hdr[2];
	uint32_t err = 0U;
	int st;

	hdr[0] = 0x801A2B04U;
	hdr[1] = 16U + (alignedEnd - alignedStart);
	err |= razorWriterStatusToErr(razorFileWriterWrite(writer, hdr, 8U));
	err |= razorWriterStatusToErr(razorFileWriterWrite(writer, &alignedStart, 4U));
	err |= razorWriterStatusToErr(razorFileWriterWrite(writer, &alignedEnd, 4U));

	st = razorFileWriterWriteMem(writer, (const void *)(uintptr_t)alignedStart, alignedEnd - alignedStart);
	if (st == 1) {
		err |= 64U;
	} else if (st == 2) {
		uint32_t warnPayload[2];
		err |= 128U;
		hdr[0] = 0x801A2B18U;
		hdr[1] = 16U;
		warnPayload[0] = alignedStart;
		warnPayload[1] = 0U;
		st = razorFileWriterWrite(writer, hdr, 8U);
		if (st == 0)
			st = razorFileWriterWrite(writer, warnPayload, 8U);
		err |= razorWriterStatusToErr(st);
	}
	return (uint64_t)err;
}

uint64_t razorWriteMemoryDump(RazorPageBlock **pageDir, RazorFileWriter *writer)
{
	uint32_t dirIdx;
	uint32_t runStart = 0U;
	uint64_t err = 0ULL;
	uint32_t hdr[2];

	for (dirIdx = 0; dirIdx < 256U; dirIdx++) {
		RazorPageBlock *blk = pageDir[dirIdx];
		uint32_t dirBase = dirIdx << 24;

		if (blk != NULL) {
			uint32_t p;
			for (p = 0; p < 4096U; p++) {
				uint32_t addr = dirBase + (p << 12);
				int bit = (blk->bits[p >> 3] >> ((~p) & 7U)) & 1;

				if (runStart != 0U) {
					if (!bit) {
						err |= razorWriteMemChunk(writer, runStart, addr);
						runStart = 0U;
					}
				} else {
					if (bit)
						runStart = addr;
				}
			}
		} else if (runStart != 0U) {
			err |= razorWriteMemChunk(writer, runStart, dirBase);
			runStart = 0U;
		}
	}

	if (runStart != 0U)
		err |= razorWriteMemChunk(writer, runStart, 0U);

	hdr[0] = 0x801A2B19U;
	hdr[1] = 8U;
	err |= (uint64_t)razorWriterStatusToErr(razorFileWriterWrite(writer, hdr, 8U));
	return err;
}
