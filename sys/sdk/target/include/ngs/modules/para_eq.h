/*
 * ngs/modules/para_eq.h: the NGS parametric equalizer module, under Sony's names (SDK
 * Release 03.570.011, target/include/ngs/modules/para_eq.h).
 */
#ifndef _SCE_NGS_MODULE_PARA_EQ_H
#define _SCE_NGS_MODULE_PARA_EQ_H

#define SCE_NGS_PARAM_EQ_ID			(0x5CEC)
#define SCE_NGS_PARAM_EQ_STRUCT_ID		(SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_PARAM_EQ_ID,1,1))
#define SCE_NGS_PARAM_EQ_COEFF_STRUCT_ID	(SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_PARAM_EQ_ID,2,1))

#define SCE_NGS_MAX_EQ_FILTERS			(4)

typedef struct SceNgsParamEqFilter {
	SceUInt32	eFilterMode;
	SceFloat32	fFrequency;
	SceFloat32	fResonance;
	SceFloat32	fGain;
} SceNgsParamEqFilter;

typedef struct SceNgsParamEqCoEff {
	SceFloat32 fB0;
	SceFloat32 fB1;
	SceFloat32 fB2;
	SceFloat32 fA1;
	SceFloat32 fA2;
} SceNgsParamEqCoEff;

typedef struct SceNgsParamEqParams {
	SceNgsParamsDescriptor	desc;
	SceNgsParamEqFilter	filter[SCE_NGS_MAX_EQ_FILTERS];
} SceNgsParamEqParams;

typedef struct SceNgsParamEqParamsCoEff {
	SceNgsParamsDescriptor	desc;
	SceNgsParamEqCoEff	filterCoEff[SCE_NGS_MAX_EQ_FILTERS];
} SceNgsParamEqParamsCoEff;

typedef struct SceNgsParamEqParamsBlock {
	SceNgsModuleParamHeader	moduleInfo;
	SceNgsParamEqParams	params;
} SceNgsParamEqParamsBlock;

typedef struct SceNgsParamEqCoEffParamsBlock {
	SceNgsModuleParamHeader		moduleInfo;
	SceNgsParamEqParamsCoEff	params;
} SceNgsParamEqCoEffParamsBlock;

#endif /* _SCE_NGS_MODULE_PARA_EQ_H */
