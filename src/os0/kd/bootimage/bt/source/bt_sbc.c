#include <arm_neon.h>
#include "bt_sbc_codec.h"

const float gSbcAnalysisCoefficients[11][4] = {
	{ 0.707106769f, -0.707106769f, -0.707106769f, 0.707106769f },
	{ 0.923879504f, 0.382683426f, -0.382683426f, -0.923879504f },
	{ 0.382683426f, -0.923879504f, 0.923879504f, -0.382683426f },
	{ 0.831469595f, -0.195090324f, -0.980785251f, -0.555570245f },
	{ 0.555570245f, 0.980785251f, 0.195090324f, -0.831469595f },
	{ 0.980785251f, 0.831469595f, 0.555570245f, 0.195090324f },
	{ -0.195090324f, -0.555570245f, -0.831469595f, -0.980785251f },
	{ 0.555570245f, -0.980785251f, 0.195090324f, 0.831469595f },
	{ -0.831469595f, -0.195090324f, 0.980785251f, -0.555570245f },
	{ 0.195090324f, -0.555570245f, 0.831469595f, -0.980785251f },
	{ 0.980785251f, -0.831469595f, 0.555570245f, -0.195090324f },
};

const int gSbcLoudnessOffsetsFour[4][4] = {
	{ -1, 0, 0, 0 },
	{ -2, 0, 0, 1 },
	{ -2, 0, 0, 1 },
	{ -2, 0, 0, 1 }
};

const float gSbcSynthesisCoefficients[8][8] = {
	{ 0.707106769f, 0.555570245f, 0.382683426f, 0.195090324f, -0.831469595f, -0.923879504f, -0.980785251f, -1.00000000f },
	{ -0.707106769f, -0.980785251f, -0.923879504f, -0.555570245f, 0.195090324f, -0.382683426f, -0.831469595f, -1.00000000f },
	{ -0.707106769f, 0.195090324f, 0.923879504f, 0.831469595f, 0.980785251f, 0.382683426f, -0.555570245f, -1.00000000f },
	{ 0.707106769f, 0.831469595f, -0.382683426f, -0.980785251f, 0.555570245f, 0.923879504f, -0.195090324f, -1.00000000f },
	{ 0.707106769f, -0.831469595f, -0.382683426f, 0.980785251f, -0.555570245f, 0.923879504f, 0.195090324f, -1.00000000f },
	{ -0.707106769f, -0.195090324f, 0.923879504f, -0.831469595f, -0.980785251f, 0.382683426f, 0.555570245f, -1.00000000f },
	{ -0.707106769f, 0.980785251f, -0.923879504f, 0.555570245f, -0.195090324f, -0.382683426f, 0.831469595f, -1.00000000f },
	{ 0.707106769f, -0.555570245f, 0.382683426f, -0.195090324f, 0.831469595f, -0.923879504f, 0.980785251f, -1.00000000f },
};

const int gSbcLoudnessOffsetsEight[4][8] = {
	{ -2, 0, 0, 0, 0, 0, 0, 1 },
	{ -3, 0, 0, 0, 0, 0, 1, 2 },
	{ -4, 0, 0, 0, 0, 0, 1, 2 },
	{ -4, 0, 0, 0, 0, 0, 1, 2 }
};

const float gSbcPrototypeWindow[80] = {
	0.00000000f, 0.000156575392f, 0.000343256426f, 0.000554620230f,
	0.000823919487f, 0.00113992509f, 0.00147640170f, 0.00178371731f,
	0.00201182533f, 0.00210371986f, 0.00199454557f, 0.00161656283f,
	0.000902154483f, -0.000178805363f, -0.00164973096f, -0.00349717448f,
	0.00565949455f, 0.00802941155f, 0.0104584442f, 0.0127472337f,
	0.0146525260f, 0.0159045607f, 0.0162208471f, 0.0153184105f,
	0.0129371807f, 0.00885757525f, 0.00292408443f, -0.00491578039f,
	-0.0146404076f, -0.0261098761f, -0.0390751399f, -0.0531873032f,
	0.0679989457f, 0.0829847604f, 0.0975753888f, 0.111196689f,
	0.123264551f, 0.133264422f, 0.140753508f, 0.145389840f,
	0.146955073f, 0.145389840f, 0.140753508f, 0.133264422f,
	0.123264551f, 0.111196689f, 0.0975753888f, 0.0829847604f,
	-0.0679989457f, -0.0531873032f, -0.0390751399f, -0.0261098761f,
	-0.0146404076f, -0.00491578039f, 0.00292408443f, 0.00885757525f,
	0.0129371807f, 0.0153184105f, 0.0162208471f, 0.0159045607f,
	0.0146525260f, 0.0127472337f, 0.0104584442f, 0.00802941155f,
	-0.00565949455f, -0.00349717448f, -0.00164973096f, -0.000178805363f,
	0.000902154483f, 0.00161656283f, 0.00199454557f, 0.00210371986f,
	0.00201182533f, 0.00178371731f, 0.00147640170f, 0.00113992509f,
	0.000823919487f, 0.000554620230f, 0.000343256426f, 0.000156575392f,
};

const float gSbcPrototypeWindowFour[40] = {
	0.00000000f, 0.000536548963f, 0.00149188354f, 0.00273370906f,
	0.00383720198f, 0.00389205152f, 0.00186581688f, -0.00306012295f,
	0.0109137623f, 0.0204385091f, 0.0288757384f, 0.0321939290f,
	0.0258767810f, 0.00613245182f, -0.0288217273f, -0.0776463524f,
	0.135593280f, 0.194987848f, 0.246636659f, 0.281828195f,
	0.294315338f, 0.281828195f, 0.246636659f, 0.194987848f,
	-0.135593280f, -0.0776463524f, -0.0288217273f, 0.00613245182f,
	0.0258767810f, 0.0321939290f, 0.0288757384f, 0.0204385091f,
	-0.0109137623f, -0.00306012295f, 0.00186581688f, 0.00389205152f,
	0.00383720198f, 0.00273370906f, 0.00149188354f, 0.000536548963f
};

void SbcSynthesizeEight(float *historyPosition, const float *subbands,
		       float *output)
{
	float32x4_t scale = vcvtq_f32_s32(vdupq_n_s32(-8));
	int blocks = 16;

	do {
		float32x4_t low, high, negative, input;

		historyPosition -= 16;
		input = vld1q_f32(subbands);
		low = vmulq_f32(vdupq_n_f32(vgetq_lane_f32(input, 0)), vld1q_f32(gSbcSynthesisCoefficients[0]));
		high = vmulq_f32(vdupq_n_f32(vgetq_lane_f32(input, 0)), vld1q_f32(gSbcSynthesisCoefficients[0] + 4));
#define SYNTHESIS_BAND(band, lane) \
		low = vmlaq_f32(low, vdupq_n_f32(vgetq_lane_f32(input, lane)), \
				  vld1q_f32(gSbcSynthesisCoefficients[band])); \
		high = vmlaq_f32(high, vdupq_n_f32(vgetq_lane_f32(input, lane)), \
				   vld1q_f32(gSbcSynthesisCoefficients[band] + 4))
		SYNTHESIS_BAND(1, 1);
		SYNTHESIS_BAND(2, 2);
		SYNTHESIS_BAND(3, 3);
		input = vld1q_f32(subbands + 4);
		SYNTHESIS_BAND(4, 0);
		SYNTHESIS_BAND(5, 1);
		SYNTHESIS_BAND(6, 2);
		SYNTHESIS_BAND(7, 3);
#undef SYNTHESIS_BAND
		vst1q_f32(historyPosition, low);
		vst1q_f32(historyPosition + 9, high);
		negative = vnegq_f32(low);
		historyPosition[8] = vgetq_lane_f32(negative, 0);
		historyPosition[7] = vgetq_lane_f32(negative, 1);
		historyPosition[6] = vgetq_lane_f32(negative, 2);
		historyPosition[5] = vgetq_lane_f32(negative, 3);
		historyPosition[15] = vgetq_lane_f32(high, 0);
		historyPosition[14] = vgetq_lane_f32(high, 1);
		historyPosition[13] = vgetq_lane_f32(high, 2);
		historyPosition[4] = vgetq_lane_f32(negative, 0) - vgetq_lane_f32(negative, 0);

		low = vmulq_f32(vld1q_f32(historyPosition), vld1q_f32(gSbcPrototypeWindow));
		high = vmulq_f32(vld1q_f32(historyPosition + 4), vld1q_f32(gSbcPrototypeWindow + 4));
#define WINDOW_STEP(historyOffset, windowOffset) \
		low = vmlaq_f32(low, vld1q_f32(historyPosition + historyOffset), \
				  vld1q_f32(gSbcPrototypeWindow + windowOffset)); \
		high = vmlaq_f32(high, vld1q_f32(historyPosition + historyOffset + 4), \
				   vld1q_f32(gSbcPrototypeWindow + windowOffset + 4))
		WINDOW_STEP(24, 8);
		WINDOW_STEP(32, 16);
		WINDOW_STEP(56, 24);
		WINDOW_STEP(64, 32);
		WINDOW_STEP(88, 40);
		WINDOW_STEP(96, 48);
		WINDOW_STEP(120, 56);
		WINDOW_STEP(128, 64);
		WINDOW_STEP(152, 72);
#undef WINDOW_STEP
		vst1q_f32(output, vmulq_f32(low, scale));
		vst1q_f32(output + 4, vmulq_f32(high, scale));
		output += 8;
		subbands += 8;
	} while (--blocks);
}

/* The original Thumb callers pass the scale in S0, rather than a core register. */
void __attribute__((pcs("aapcs-vfp")))
SbcConvertStereo(float *leftPosition, float *rightPosition,
		 const int16_t *pcm, float scale)
{
	float32x4_t factor = vdupq_n_f32(scale);
	int blocks = 16;

	leftPosition -= 128;
	rightPosition -= 128;
	pcm += 240;
	do {
		int32x4_t leftHigh = vdupq_n_s32(pcm[14]);
		int32x4_t rightHigh = vdupq_n_s32(pcm[15]);
		int32x4_t leftLow, rightLow;

		leftHigh = vsetq_lane_s32(pcm[12], leftHigh, 1);
		rightHigh = vsetq_lane_s32(pcm[13], rightHigh, 1);
		leftHigh = vsetq_lane_s32(pcm[10], leftHigh, 2);
		rightHigh = vsetq_lane_s32(pcm[11], rightHigh, 2);
		leftHigh = vsetq_lane_s32(pcm[8], leftHigh, 3);
		rightHigh = vsetq_lane_s32(pcm[9], rightHigh, 3);
		leftLow = vdupq_n_s32(pcm[6]);
		rightLow = vdupq_n_s32(pcm[7]);
		leftLow = vsetq_lane_s32(pcm[4], leftLow, 1);
		rightLow = vsetq_lane_s32(pcm[5], rightLow, 1);
		leftLow = vsetq_lane_s32(pcm[2], leftLow, 2);
		rightLow = vsetq_lane_s32(pcm[3], rightLow, 2);
		leftLow = vsetq_lane_s32(pcm[0], leftLow, 3);
		rightLow = vsetq_lane_s32(pcm[1], rightLow, 3);

		vst1q_f32(leftPosition, vmulq_f32(vcvtq_f32_s32(leftHigh), factor));
		vst1q_f32(rightPosition, vmulq_f32(vcvtq_f32_s32(rightHigh), factor));
		vst1q_f32(leftPosition + 4, vmulq_f32(vcvtq_f32_s32(leftLow), factor));
		vst1q_f32(rightPosition + 4, vmulq_f32(vcvtq_f32_s32(rightLow), factor));
		pcm -= 16;
		leftPosition += 8;
		rightPosition += 8;
	} while (--blocks);
}

void SbcAnalyzeEight(const float *historyPosition, float *subbands)
{
	int blocks = 16;

	do {
		float32x4_t phase0, phase1, phase2, phase3;
		float32x4_t low, high, coefficient, sample, odd;

		historyPosition -= 8;
		phase0 = vmulq_f32(vld1q_f32(historyPosition), vld1q_f32(gSbcPrototypeWindow));
		phase1 = vmulq_f32(vld1q_f32(historyPosition + 4), vld1q_f32(gSbcPrototypeWindow + 4));
		phase2 = vmulq_f32(vld1q_f32(historyPosition + 8), vld1q_f32(gSbcPrototypeWindow + 8));
		phase3 = vmulq_f32(vld1q_f32(historyPosition + 12), vld1q_f32(gSbcPrototypeWindow + 12));
#define ANALYSIS_WINDOW(base) \
		phase0 = vmlaq_f32(phase0, vld1q_f32(historyPosition + base), vld1q_f32(gSbcPrototypeWindow + base)); \
		phase1 = vmlaq_f32(phase1, vld1q_f32(historyPosition + base + 4), vld1q_f32(gSbcPrototypeWindow + base + 4)); \
		phase2 = vmlaq_f32(phase2, vld1q_f32(historyPosition + base + 8), vld1q_f32(gSbcPrototypeWindow + base + 8)); \
		phase3 = vmlaq_f32(phase3, vld1q_f32(historyPosition + base + 12), vld1q_f32(gSbcPrototypeWindow + base + 12))
		ANALYSIS_WINDOW(16);
		ANALYSIS_WINDOW(32);
		ANALYSIS_WINDOW(48);
		ANALYSIS_WINDOW(64);
#undef ANALYSIS_WINDOW
#define DUPLICATE(vector, lane) vdupq_n_f32(vgetq_lane_f32(vector, lane))
		coefficient = vld1q_f32(gSbcAnalysisCoefficients[0]);
		low = vmulq_f32(DUPLICATE(phase0, 0), coefficient);
		low = vmlaq_f32(low, DUPLICATE(phase2, 0), coefficient);
		coefficient = vld1q_f32(gSbcAnalysisCoefficients[1]);
		sample = DUPLICATE(phase0, 2);
		high = vmlsq_f32(low, sample, coefficient);
		low = vmlaq_f32(low, sample, coefficient);
		sample = DUPLICATE(phase1, 2);
		low = vmlaq_f32(low, sample, coefficient);
		high = vmlsq_f32(high, sample, coefficient);
		coefficient = vld1q_f32(gSbcAnalysisCoefficients[2]);
		sample = DUPLICATE(phase2, 2);
		low = vmlaq_f32(low, sample, coefficient);
		high = vmlsq_f32(high, sample, coefficient);
		sample = DUPLICATE(phase3, 2);
		low = vmlsq_f32(low, sample, coefficient);
		high = vmlaq_f32(high, sample, coefficient);
		sample = DUPLICATE(phase1, 0);
		low = vaddq_f32(low, sample);
		high = vaddq_f32(high, sample);
		odd = vuzpq_f32(phase0, phase1).val[1];
#define ANALYSIS_PAIR(row, vector, lane, operation) \
		sample = DUPLICATE(vector, lane); \
		low = operation(low, sample, vld1q_f32(gSbcAnalysisCoefficients[row])); \
		high = operation(high, sample, vld1q_f32(gSbcAnalysisCoefficients[row + 1]))
		ANALYSIS_PAIR(3, odd, 0, vmlaq_f32);
		ANALYSIS_PAIR(3, odd, 3, vmlaq_f32);
		ANALYSIS_PAIR(5, odd, 1, vmlaq_f32);
		ANALYSIS_PAIR(5, odd, 2, vmlaq_f32);
		ANALYSIS_PAIR(7, phase2, 1, vmlaq_f32);
		ANALYSIS_PAIR(7, phase3, 3, vmlsq_f32);
		ANALYSIS_PAIR(9, phase2, 3, vmlaq_f32);
		ANALYSIS_PAIR(9, phase3, 1, vmlsq_f32);
#undef ANALYSIS_PAIR
#undef DUPLICATE
		vst1q_f32(subbands, low);
		vst1q_f32(subbands + 4, high);
		subbands += 8;
	} while (--blocks);
}

