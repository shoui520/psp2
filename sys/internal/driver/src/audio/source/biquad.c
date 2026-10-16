#include "audio_work.h"
/* Fixed-point biquad processing; original source filename is unavailable. */

#include "biquad.h"
#include "fixed_point.h"

int32_t *audioBiquadProcess(int32_t *samples, int count,
		const AudioBiquadCoefficients *coefficients, AudioBiquadState *state)
{
	int32_t b0 = coefficients->b0;
	int32_t b1 = coefficients->b1;
	int32_t b2 = coefficients->b2;
	int32_t feedback1 = coefficients->feedback1;
	int32_t feedback2 = coefficients->feedback2;
	int32_t previousInput = state->previousInput;
	int32_t secondPreviousInput = state->secondPreviousInput;
	int32_t previousOutput = state->previousOutput;
	int32_t secondPreviousOutput = state->secondPreviousOutput;

	for (;;) {
		int32_t input;
		uint32_t output;
		int pairAvailable = count >= 2;

		count = (int32_t)((uint32_t)count - 2);
		if (!pairAvailable)
			break;
		input = *samples;
		output = MUL_HIGH(b1, previousInput);
		output += MUL_HIGH(b2, secondPreviousInput);
		secondPreviousInput = input;
		output += MUL_HIGH(b0, input);
		output += MUL_HIGH(feedback2, secondPreviousOutput);
		output += MUL_HIGH(feedback1, previousOutput);
		secondPreviousOutput = output << 2;
		*samples++ = secondPreviousOutput;
		input = *samples;
		output = MUL_HIGH(b1, secondPreviousInput);
		output += MUL_HIGH(b2, previousInput);
		previousInput = input;
		output += MUL_HIGH(b0, input);
		output += MUL_HIGH(feedback2, previousOutput);
		output += MUL_HIGH(feedback1, secondPreviousOutput);
		previousOutput = output << 2;
		*samples++ = previousOutput;
	}
	if ((int32_t)((uint32_t)count + 2) != 0) {
		int32_t input = *samples;
		uint32_t output = MUL_HIGH(b0, input);

		output += MUL_HIGH(b1, previousInput);
		output += MUL_HIGH(b2, secondPreviousInput);
		output += MUL_HIGH(feedback1, previousOutput);
		output += MUL_HIGH(feedback2, secondPreviousOutput);
		secondPreviousInput = previousInput;
		previousInput = input;
		secondPreviousOutput = previousOutput;
		previousOutput = output << 2;
		*samples++ = previousOutput;
	}
	state->previousInput = previousInput;
	state->secondPreviousInput = secondPreviousInput;
	state->previousOutput = previousOutput;
	state->secondPreviousOutput = secondPreviousOutput;
	return samples;
}

/* Seven bands, each with seven coefficient choices; signed Q30 words. */
const AudioBiquadCoefficients audioEqualizerCoefficients[7][7] = {
	{
		{ 1078518559, -2142992531, 1064546845, 2142992532, -1069323579 },
		{ 1076488731, -2141890847, 1065474951, 2141890848, -1068221857 },
		{ 1075258653, -2140057221, 1064871341, 2140057222, -1066388169 },
		{ 1073741823, 0, 0, 0, 0 },
		{ 1072227135, -2137038319, 1064883855, 2137038320, -1063369164 },
		{ 1071001926, -2136425323, 1065496046, 2136425324, -1062756148 },
		{ 1068986245, -2133501264, 1064587569, 2133501265, -1059831989 },
	},
	{
		{ 1085834164, -2135830305, 1050464667, 2135830306, -1062557006 },
		{ 1080690168, -2133052890, 1052830639, 2133052891, -1059778983 },
		{ 1077573650, -2128439884, 1051333140, 2128439885, -1055164965 },
		{ 1073741823, 0, 0, 0, 0 },
		{ 1069923624, -2120871204, 1051412825, 2120871205, -1047594624 },
		{ 1066838154, -2119338334, 1052965089, 2119338335, -1046061418 },
		{ 1061784150, -2112044734, 1050723892, 2112044735, -1038766217 },
	},
	{
		{ 1103726962, -2116843834, 1016021915, 2116843835, -1046007052 },
		{ 1090938408, -2110031066, 1021988351, 2110031067, -1039184935 },
		{ 1083195126, -2098773382, 1018458500, 2098773383, -1027911801 },
		{ 1073741823, 0, 0, 0, 0 },
		{ 1064371023, -2080456886, 1018940971, 2080456887, -1009570169 },
		{ 1056816311, -2076770410, 1022804147, 2076770411, -1005878634 },
		{ 1044571297, -2059335177, 1017590001, 2059335178, -988419474 },
	},
	{
		{ 1147109307, -2061830893, 932513044, 2061830894, -1005880526 },
		{ 1115623326, -2045669907, 947698587, 2045669908, -989580088 },
		{ 1096589929, -2019290660, 940125111, 2019290661, -962973215 },
		{ 1073741823, 0, 0, 0, 0 },
		{ 1051369773, -1977217535, 942909095, 1977217536, -920537043 },
		{ 1033432591, -1968873621, 952430363, 1968873622, -912121130 },
		{ 1005066821, -1929959116, 941545837, 1929959117, -872870833 },
	},
	{
		{ 1246437751, -1882258895, 741310698, 1882258896, -914006623 },
		{ 1171289382, -1847896112, 780170452, 1847896113, -877718009 },
		{ 1126061358, -1793330620, 767774908, 1793330621, -820094441 },
		{ 1073741823, 0, 0, 0, 0 },
		{ 1023853182, -1710008142, 781990871, 1710008143, -732102228 },
		{ 984318242, -1693999257, 804619722, 1693999258, -715196139 },
		{ 924973192, -1621468940, 787369557, 1621468941, -638600924 },
	},
	{
		{ 1433856095, -1231613180, 380539644, 1231613181, -740653915 },
		{ 1273201134, -1185638236, 473464945, 1185638237, -672924255 },
		{ 1177520114, -1116194134, 466841862, 1116194135, -570620151 },
		{ 1073741823, 0, 0, 0, 0 },
		{ 979109818, -1017820682, 520329730, 1017820683, -425697724 },
		{ 905529750, -999896503, 567504142, 999896504, -399292067 },
		{ 804070582, -922292402, 554638007, 922292403, -284966763 },
	},
	{
		{ 1775297152, 749289816, -276717519, -749289815, -424837807 },
		{ 1449034869, 696662295, -55710277, -696662294, -319582766 },
		{ 1259418915, 623656508, -12105898, -623656507, -173571191 },
		{ 1073741823, 0, 0, 0, 0 },
		{ 915439248, 531710353, 147981459, -531710352, 10321117 },
		{ 795647869, 516230120, 236812371, -516230119, 41281584 },
		{ 649424522, 453188252, 256951983, -453188251, 167365319 },
	},
};

