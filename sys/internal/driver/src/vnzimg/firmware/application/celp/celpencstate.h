/* generated into the source once from the layout the code uses; fields are in order of offset */
/* the filters' memories, run backwards: the subframe's samples, then the order's carried over */
typedef struct CelpEncFilterMemory {
	int32_t  weightInput[100];	/* the perceptual weighting filter's input */
	int32_t  weightOutput[100];	/* and output */
	int32_t  synthesis[100];	/* the synthesis filter's output */
	int32_t  weightedSynthesis[100];	/* weighted */
} CelpEncFilterMemory;

typedef struct CelpEncoderState {
	int16_t  input[320];	/* the frame's samples as the ARM gave them */
	int32_t reserved1[160];
	uint8_t  stream[640];	/* the bits written so far */
	int32_t  invalid;	/* the bit rate is not one the encoder has */
	int32_t  bitRate;
	int32_t  align;	/* the frame ends on a byte */
	int32_t  alignHeader;
	uint8_t * writeBase;
	int32_t  writePosition;	/* in bits */
	int32_t  frameBits;
	int32_t  modeIndex;	/* of the bit rate */
	int32_t  gainMode;
	int32_t  gainIndex;
	int32_t  lspIndex[4];
	int32_t  lspPredicted;
	int32_t reserved2[6];
	int32_t  positionIndex[8];
	int32_t  signIndex[8];
	int32_t  gainCode[8];
	int16_t  lagIndex[8];
	int32_t reserved3[31];
	int32_t  order;
	int32_t  interpolationTaps;	/* 5: celpEncLimits[0] - 1; only written */
	int32_t  frameLength;
	int32_t  subframes;
	int32_t  subframeLength;
	int32_t reserved4[1];
	int32_t  lpc[100];	/* Q12 of each subframe, one after another */
	int32_t  previousLsp[10];
	int32_t  lsp[10];
	int32_t  angle[10];
	int16_t  gainLevel[4];	/* each subframe's fixed codebook gain level */
	int16_t  previousGainLevel;	/* the last subframe's of the frame before */
	uint8_t reserved6[2];
	int32_t  pulseCount;
	int32_t  pulseWidth[12];
	int32_t  pulsePosition[12][80];
	int32_t  history[386];
	int32_t  frameCounter;
	int32_t  shortSubframeLength;	/* 40; only written */
	int32_t  frameLengthCopy;
	int32_t  bufferLength;	/* frameLength + 120; only written */
	int32_t  pitchBlocks;	/* blocks of the open-loop pitch analysis; only written */
	int32_t  analysisBlocks;
	int32_t  analysisOffset[4];
	int32_t  analysisLength;
	int16_t  analysisWindow[200];
	int32_t reserved7[100];
	int32_t  lagResolution;	/* 6: lags in sixths of a sample; only written */
	int32_t  pitchBlockLength;	/* celpEncPitchBlocks; only written */
	int32_t reserved8[1];
	CelpEncFilterMemory memory;
	int32_t  perceptualInput[100];
	int32_t  perceptualOutput[100];
	int16_t  weightedHistory[295];
	int16_t  speech[760];
	uint8_t reserved9[2];
	int32_t  analysisLpc[40];
	int32_t reserved10[60];
	int32_t  perceptualNumerator[100];
	int32_t  perceptualDenominator[100];
	int16_t  filterMemoryLength;	/* 100; only written */
	uint8_t reserved11[6];
	int32_t  zeroInput[100];	/* the synthesis filter's memory, run without input */
	int32_t  weightedZeroInput[100];	/* and the weighting filter's */
	int32_t  candidate[32][80];	/* the beam search's pulse positions, by candidate and pulse */
	int32_t  nextCandidate[32][80];
	int16_t  reducedTarget[80];	/* the target less the adaptive codebook's part */
	int16_t  impulse16[80];	/* the weighted synthesis filter's impulse response */
	int32_t  impulse[80];
	int32_t  correlation[80];	/* of the reduced target with the impulse response */
	int32_t  matrix[80][80];	/* the impulse response's correlations, signed */
	int32_t  sumCorrelation[2560];	/* of each candidate extended by each position */
	int32_t  sumEnergy[2560];
	int32_t  cost[2560];
} CelpEncoderState;

typedef char celpEnc_input[offsetof(CelpEncoderState, input) == 0 ? 1 : -1];
typedef char celpEnc_stream[offsetof(CelpEncoderState, stream) == 1280 ? 1 : -1];
typedef char celpEnc_invalid[offsetof(CelpEncoderState, invalid) == 1920 ? 1 : -1];
typedef char celpEnc_bitRate[offsetof(CelpEncoderState, bitRate) == 1924 ? 1 : -1];
typedef char celpEnc_align[offsetof(CelpEncoderState, align) == 1928 ? 1 : -1];
typedef char celpEnc_alignHeader[offsetof(CelpEncoderState, alignHeader) == 1932 ? 1 : -1];
typedef char celpEnc_writeBase[offsetof(CelpEncoderState, writeBase) == 1936 ? 1 : -1];
typedef char celpEnc_writePosition[offsetof(CelpEncoderState, writePosition) == 1940 ? 1 : -1];
typedef char celpEnc_frameBits[offsetof(CelpEncoderState, frameBits) == 1944 ? 1 : -1];
typedef char celpEnc_modeIndex[offsetof(CelpEncoderState, modeIndex) == 1948 ? 1 : -1];
typedef char celpEnc_gainMode[offsetof(CelpEncoderState, gainMode) == 1952 ? 1 : -1];
typedef char celpEnc_gainIndex[offsetof(CelpEncoderState, gainIndex) == 1956 ? 1 : -1];
typedef char celpEnc_lspIndex[offsetof(CelpEncoderState, lspIndex) == 1960 ? 1 : -1];
typedef char celpEnc_lspPredicted[offsetof(CelpEncoderState, lspPredicted) == 1976 ? 1 : -1];
typedef char celpEnc_positionIndex[offsetof(CelpEncoderState, positionIndex) == 2004 ? 1 : -1];
typedef char celpEnc_signIndex[offsetof(CelpEncoderState, signIndex) == 2036 ? 1 : -1];
typedef char celpEnc_gainCode[offsetof(CelpEncoderState, gainCode) == 2068 ? 1 : -1];
typedef char celpEnc_lagIndex[offsetof(CelpEncoderState, lagIndex) == 2100 ? 1 : -1];
typedef char celpEnc_order[offsetof(CelpEncoderState, order) == 2240 ? 1 : -1];
typedef char celpEnc_interpolationTaps[offsetof(CelpEncoderState, interpolationTaps) == 2244 ? 1 : -1];
typedef char celpEnc_frameLength[offsetof(CelpEncoderState, frameLength) == 2248 ? 1 : -1];
typedef char celpEnc_subframes[offsetof(CelpEncoderState, subframes) == 2252 ? 1 : -1];
typedef char celpEnc_subframeLength[offsetof(CelpEncoderState, subframeLength) == 2256 ? 1 : -1];
typedef char celpEnc_lpc[offsetof(CelpEncoderState, lpc) == 2264 ? 1 : -1];
typedef char celpEnc_previousLsp[offsetof(CelpEncoderState, previousLsp) == 2664 ? 1 : -1];
typedef char celpEnc_lsp[offsetof(CelpEncoderState, lsp) == 2704 ? 1 : -1];
typedef char celpEnc_angle[offsetof(CelpEncoderState, angle) == 2744 ? 1 : -1];
typedef char celpEnc_previousGainLevel[offsetof(CelpEncoderState, previousGainLevel) == 2792 ? 1 : -1];
typedef char celpEnc_pulseCount[offsetof(CelpEncoderState, pulseCount) == 2796 ? 1 : -1];
typedef char celpEnc_pulseWidth[offsetof(CelpEncoderState, pulseWidth) == 2800 ? 1 : -1];
typedef char celpEnc_pulsePosition[offsetof(CelpEncoderState, pulsePosition) == 2848 ? 1 : -1];
typedef char celpEnc_history[offsetof(CelpEncoderState, history) == 6688 ? 1 : -1];
typedef char celpEnc_frameCounter[offsetof(CelpEncoderState, frameCounter) == 8232 ? 1 : -1];
typedef char celpEnc_shortSubframeLength[offsetof(CelpEncoderState, shortSubframeLength) == 8236 ? 1 : -1];
typedef char celpEnc_frameLengthCopy[offsetof(CelpEncoderState, frameLengthCopy) == 8240 ? 1 : -1];
typedef char celpEnc_bufferLength[offsetof(CelpEncoderState, bufferLength) == 8244 ? 1 : -1];
typedef char celpEnc_pitchBlocks[offsetof(CelpEncoderState, pitchBlocks) == 8248 ? 1 : -1];
typedef char celpEnc_analysisBlocks[offsetof(CelpEncoderState, analysisBlocks) == 8252 ? 1 : -1];
typedef char celpEnc_analysisOffset[offsetof(CelpEncoderState, analysisOffset) == 8256 ? 1 : -1];
typedef char celpEnc_analysisLength[offsetof(CelpEncoderState, analysisLength) == 8272 ? 1 : -1];
typedef char celpEnc_analysisWindow[offsetof(CelpEncoderState, analysisWindow) == 8276 ? 1 : -1];
typedef char celpEnc_lagResolution[offsetof(CelpEncoderState, lagResolution) == 9076 ? 1 : -1];
typedef char celpEnc_pitchBlockLength[offsetof(CelpEncoderState, pitchBlockLength) == 9080 ? 1 : -1];
typedef char celpEnc_memory[offsetof(CelpEncoderState, memory) == 9088 ? 1 : -1];
typedef char celpEnc_perceptualInput[offsetof(CelpEncoderState, perceptualInput) == 10688 ? 1 : -1];
typedef char celpEnc_perceptualOutput[offsetof(CelpEncoderState, perceptualOutput) == 11088 ? 1 : -1];
typedef char celpEnc_weightedHistory[offsetof(CelpEncoderState, weightedHistory) == 11488 ? 1 : -1];
typedef char celpEnc_speech[offsetof(CelpEncoderState, speech) == 12078 ? 1 : -1];
typedef char celpEnc_analysisLpc[offsetof(CelpEncoderState, analysisLpc) == 13600 ? 1 : -1];
typedef char celpEnc_perceptualNumerator[offsetof(CelpEncoderState, perceptualNumerator) == 14000 ? 1 : -1];
typedef char celpEnc_perceptualDenominator[offsetof(CelpEncoderState, perceptualDenominator) == 14400 ? 1 : -1];
typedef char celpEnc_filterMemoryLength[offsetof(CelpEncoderState, filterMemoryLength) == 14800 ? 1 : -1];
typedef char celpEnc_gainLevel[offsetof(CelpEncoderState, gainLevel) == 2784 ? 1 : -1];
typedef char celpEnc_zeroInput[offsetof(CelpEncoderState, zeroInput) == 14808 ? 1 : -1];
typedef char celpEnc_candidate[offsetof(CelpEncoderState, candidate) == 15608 ? 1 : -1];
typedef char celpEnc_reducedTarget[offsetof(CelpEncoderState, reducedTarget) == 36088 ? 1 : -1];
typedef char celpEnc_matrix[offsetof(CelpEncoderState, matrix) == 37048 ? 1 : -1];
typedef char celpEnc_cost[offsetof(CelpEncoderState, cost) == 83128 ? 1 : -1];
typedef char celpEncoderStateSize[sizeof(CelpEncoderState) == 93368 ? 1 : -1];
