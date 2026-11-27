/* paf's math exports (ScePafStdc) */
#ifndef PAF_STD_MATH_H_PUBLIC
#define PAF_STD_MATH_H_PUBLIC

#ifdef __cplusplus
extern "C" {
#endif

float sce_paf_sinf(float x);
float sce_paf_cosf(float x);
float sce_paf_tanf(float x);
float sce_paf_atanf(float x);
float sce_paf_atan2f(float x, float y);
float sce_paf_asinf(float x);
float sce_paf_acosf(float x);
float sce_paf_expf(float x);
float sce_paf_logf(float x);
float sce_paf_powf(float base, float exponent);
float sce_paf_floorf(float x);
float sce_paf_ceilf(float x);
float sce_paf_roundf(float x);
/* not exported; name ours */
double sce_paf_pow(double base, double exponent);
double sce_paf_floor(double x);
double sce_paf_fmod(double x, double y);

#ifdef __cplusplus
}
#endif

#endif
