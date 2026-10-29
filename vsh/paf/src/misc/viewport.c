/* Forward and reverse conversions for the shared viewport. */
extern float sce_paf_misc_float_112c;
extern float sce_paf_misc_float_1130;
extern float sce_paf_misc_float_1144;
extern float sce_paf_misc_float_1148;
extern float sce_paf_misc_float_114c;
extern float sce_paf_misc_float_1150;

void ScePafMisc_8424A840(float *x, float *y, float inputX, float inputY)
{
	float halfWidth = sce_paf_misc_float_112c * 0.5f;
	float halfHeight = sce_paf_misc_float_1130 * 0.5f;

	*x = (inputX - halfWidth) * sce_paf_misc_float_1144;
	*y = (halfHeight - inputY) * sce_paf_misc_float_1148;
}

void ScePafMisc_9C8CBB11(float *x, float *y, float inputX, float inputY)
{
	float halfWidth = sce_paf_misc_float_112c * 0.5f;
	float halfHeight = sce_paf_misc_float_1130 * 0.5f;

	*x = halfWidth + sce_paf_misc_float_114c * inputX;
	*y = halfHeight - sce_paf_misc_float_1150 * inputY;
}
