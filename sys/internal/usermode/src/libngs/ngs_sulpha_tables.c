/*
 * Static Sulpha agent, event, field, and enum descriptor tables for SceNgsUser.
 */
#include "ngs_internal.h"

/* Runtime agent config instances populated by ngsSulphaBuildAgentsConfig */
NgsSulphaSynthTraceConfig g_ngsSynthTraceCfg;
NgsSulphaApiTraceConfig g_ngsApiTraceCfg;
NgsSulphaModuleTraceConfig g_ngsModuleTraceCfg;
NgsSulphaPcmCaptureConfig g_ngsPcmCaptureCfg;
NgsSulphaNamedObjectsConfig g_ngsNamedObjectsCfg;
NgsSulphaMessageLogConfig g_ngsMessageLogCfg;

static void *const g_ngsAgentConfigPtrs[6] = {
	&g_ngsSynthTraceCfg,
	&g_ngsApiTraceCfg,
	&g_ngsModuleTraceCfg,
	&g_ngsPcmCaptureCfg,
	&g_ngsNamedObjectsCfg,
	&g_ngsMessageLogCfg,
};

const NgsSulphaAgentsConfig g_ngsSulphaAgentsConfig = {
	6,
	g_ngsAgentConfigPtrs,
};

/* Parameter type mapping from SceNgsInternal query type (1..9) to Sulpha field type */
const SceUInt8 g_ngsModuleParamTypeMap[9] = {
	0x83, 0x07, 0x81, 0x00, 0x03, 0x01, 0x81, 0x02, 0x82
};

/* Agent callback tables */
static const NgsSulphaAgentCallbacks g_ngsApiTraceCallbacks = {
	ngsApiTraceAgentGetDef,
	ngsApiTraceAgentGetMemReq,
	ngsApiTraceAgentInit,
	NULL,
	NULL,
	NULL,
	NULL,
};

static const NgsSulphaAgentCallbacks g_ngsMessageLogCallbacks = {
	ngsMessageLogAgentGetDef,
	ngsMessageLogAgentGetMemReq,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static const NgsSulphaAgentCallbacks g_ngsModuleTraceCallbacks = {
	ngsModuleTraceAgentGetDef,
	ngsModuleTraceAgentGetMemReq,
	ngsModuleTraceAgentInit,
	ngsModuleTraceAgentOnStart,
	NULL,
	ngsModuleTraceAgentOnStarted,
	NULL,
};

static const NgsSulphaAgentCallbacks g_ngsNamedObjectsCallbacks = {
	ngsNamedObjectsAgentGetDef,
	ngsNamedObjectsAgentGetMemReq,
	ngsNamedObjectsAgentInit,
	ngsNamedObjectsAgentOnStart,
	ngsNamedObjectsAgentShutdown,
	ngsNamedObjectsAgentOnStarted,
	NULL,
};

static const NgsSulphaAgentCallbacks g_ngsPcmCaptureCallbacks = {
	ngsPcmCaptureAgentGetDef,
	ngsPcmCaptureAgentGetMemReq,
	ngsPcmCaptureAgentInit,
	ngsPcmCaptureAgentOnStart,
	NULL,
	ngsPcmCaptureAgentOnStarted,
	NULL,
};

static const NgsSulphaAgentCallbacks g_ngsSynthTraceCallbacks = {
	ngsSynthTraceAgentGetDef,
	ngsSynthTraceAgentGetMemReq,
	ngsSynthTraceAgentInit,
	ngsSynthTraceAgentOnStart,
	NULL,
	ngsSynthTraceAgentOnStarted,
	NULL,
};

/* Default agent configs */
const NgsSulphaApiTraceConfig g_ngsDefaultApiTraceConfig = {
	{ NGS_SULPHA_AGENT_API_TRACE, sizeof(NgsSulphaApiTraceConfig), NGS_SULPHA_AGENT_VERSION, &g_ngsApiTraceCallbacks },
	0, NULL, 0, 0, 0x4000U,
};

const NgsSulphaMessageLogConfig g_ngsDefaultMessageLogConfig = {
	{ NGS_SULPHA_AGENT_MESSAGE_LOG, sizeof(NgsSulphaMessageLogConfig), NGS_SULPHA_AGENT_VERSION, &g_ngsMessageLogCallbacks },
	0x1000U,
};

const NgsSulphaModuleTraceConfig g_ngsDefaultModuleTraceConfig = {
	{ NGS_SULPHA_AGENT_MODULE_TRACE, sizeof(NgsSulphaModuleTraceConfig), NGS_SULPHA_AGENT_VERSION, &g_ngsModuleTraceCallbacks },
	0, NULL, 0x1000U,
};

const NgsSulphaNamedObjectsConfig g_ngsDefaultNamedObjectsConfig = {
	{ NGS_SULPHA_AGENT_NAMED_OBJECTS, sizeof(NgsSulphaNamedObjectsConfig), NGS_SULPHA_AGENT_VERSION, &g_ngsNamedObjectsCallbacks },
	0x200U,
};

const NgsSulphaPcmCaptureConfig g_ngsDefaultPcmCaptureConfig = {
	{ NGS_SULPHA_AGENT_PCM_CAPTURE, sizeof(NgsSulphaPcmCaptureConfig), NGS_SULPHA_AGENT_VERSION, &g_ngsPcmCaptureCallbacks },
	4U, 256U, 4U,
};

const NgsSulphaSynthTraceConfig g_ngsDefaultSynthTraceConfig = {
	{ NGS_SULPHA_AGENT_SYNTH_TRACE, sizeof(NgsSulphaSynthTraceConfig), NGS_SULPHA_AGENT_VERSION, &g_ngsSynthTraceCallbacks },
	0x400U,
};

/* Return code enum table for API Trace */
static const NgsSulphaEnumValueDef g_ngsReturnCodeValues[20] = {
	{ SCE_NGS_OK,				"SCE_NGS_OK",				NULL },
	{ SCE_NGS_ERROR,			"SCE_NGS_ERROR",			NULL },
	{ SCE_NGS_ERROR_INVALID_PARAM,		"SCE_NGS_ERROR_INVALID_PARAM",		NULL },
	{ SCE_NGS_ERROR_INVALID_ALIGNMENT,	"SCE_NGS_ERROR_INVALID_ALIGNMENT",	NULL },
	{ SCE_NGS_ERROR_NOT_IMPL,		"SCE_NGS_ERROR_NOT_IMPL",		NULL },
	{ SCE_NGS_ERROR_DEPENDENCY,		"SCE_NGS_ERROR_DEPENDENCY",		NULL },
	{ SCE_NGS_ERROR_OUT_OF_ASSETS,		"SCE_NGS_ERROR_OUT_OF_ASSETS",		NULL },
	{ SCE_NGS_ERROR_MODULE_NOT_AVAIL,	"SCE_NGS_ERROR_MODULE_NOT_AVAIL",	NULL },
	{ SCE_NGS_ERROR_RESOURCE_LOCKED,	"SCE_NGS_ERROR_RESOURCE_LOCKED",	NULL },
	{ SCE_NGS_ERROR_PARAM_OUT_OF_RANGE,	"SCE_NGS_ERROR_PARAM_OUT_OF_RANGE",	NULL },
	{ SCE_NGS_ERROR_INVALID_VOICE_TYPE,	"SCE_NGS_ERROR_INVALID_VOICE_TYPE",	NULL },
	{ SCE_NGS_ERROR_SYSTEM_MISMATCH,	"SCE_NGS_ERROR_SYSTEM_MISMATCH",	NULL },
	{ SCE_NGS_ERROR_INVALID_HANDLE,		"SCE_NGS_ERROR_INVALID_HANDLE",		NULL },
	{ SCE_NGS_ERROR_SIZE_MISMATCH,		"SCE_NGS_ERROR_SIZE_MISMATCH",		NULL },
	{ SCE_NGS_ERROR_PATCH_NOT_AVAIL,	"SCE_NGS_ERROR_PATCH_NOT_AVAIL",	NULL },
	{ SCE_NGS_ERROR_PARAM_TYPE_MISMATCH,	"SCE_NGS_ERROR_PARAM_TYPE_MISMATCH",	NULL },
	{ SCE_NGS_ERROR_INVALID_STATE,		"SCE_NGS_ERROR_INVALID_STATE",		NULL },
	{ SCE_NGS_ERROR_INTERNAL_ALLOC,		"SCE_NGS_ERROR_INTERNAL_ALLOC",		NULL },
	{ SCE_NGS_ERROR_INTERNAL_PROCESSING,	"SCE_NGS_ERROR_INTERNAL_PROCESSING",	NULL },
	{ SCE_NGS_ERROR_INVALID_BUFFER,		"SCE_NGS_ERROR_INVALID_BUFFER",		NULL },
};

const NgsSulphaEnumDef g_ngsReturnCodeEnumDef = {
	1, 20, 0, "ReturnCode", NULL, g_ngsReturnCodeValues,
};

/* API Trace parameter descriptor lists */
static const NgsSulphaApiTraceParamDesc g_paramsSystemGetRequiredMemorySize[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "pSynthParams", 0x03, "0x{0:x8}" },
	{ "*pnSize", 0x03, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsSystemInit[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "pSynthSysMemory", 0x03, "0x{0:x8}" },
	{ "uMemSize", 0x03, NULL },
	{ "pSynthParams", 0x03, "0x{0:x8}" },
	{ "*pSystemHandle", 0x8D, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsSystemHandleOnly[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "SystemHandle", 0x8D, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsSystemSetParamErrorCallback[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "SystemHandle", 0x8D, NULL },
	{ "Callback", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsModuleLink[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "SystemHandle", 0x8D, NULL },
	{ "pModuleInfo", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsModuleGetInfo[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "SystemHandle", 0x8D, NULL },
	{ "ModuleID", 0x03, NULL },
	{ "*ppModuleInfo", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsModuleGetNumPresets[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "SystemHandle", 0x8D, NULL },
	{ "ModuleID", 0x03, NULL },
	{ "*puNumPresets", 0x03, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsModuleGetPreset[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "SystemHandle", 0x8D, NULL },
	{ "ModuleID", 0x03, NULL },
	{ "uPresetIndex", 0x03, NULL },
	{ "pBufferInfo", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsRackGetRequiredMemorySize[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "SystemHandle", 0x8D, NULL },
	{ "pRackDesc", 0x03, "0x{0:x8}" },
	{ "*puSize", 0x03, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsRackInit[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "SystemHandle", 0x8D, NULL },
	{ "pRackBuffer", 0x03, "0x{0:x8}" },
	{ "pRackDesc", 0x03, "0x{0:x8}" },
	{ "*pRackHandle", 0x8D, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsRackGetVoiceHandle[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "RackHandle", 0x8D, NULL },
	{ "uIndex", 0x03, "0" },
	{ "*pVoiceHandle", 0x8D, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsRackCallback[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "RackHandle", 0x8D, NULL },
	{ "FuncPtr", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceInit[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "pPreset", 0x03, "0x{0:x8}" },
	{ "uInitFlags", 0x03, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceHandleOnly[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceSetPreset[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "pVoicePreset", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceLockParams[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "uModule", 0x03, NULL },
	{ "ParamsInterfaceId", 0x03, "0x{0:x8}" },
	{ "pParamsBuffer", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceUnlockParams[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "uModule", 0x03, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceSetParamsBlock[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "pParamData", 0x03, "0x{0:x8}" },
	{ "uSize", 0x03, NULL },
	{ "*pnErrorCount", 0x83, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceBypassModule[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "uModule", 0x03, NULL },
	{ "uBypassFlag", 0x03, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceSetModuleCallback[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "uModule", 0x03, NULL },
	{ "Callback", 0x03, "0x{0:x8}" },
	{ "pCallbackUsrData", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceSetFinishedCallback[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "Callback", 0x03, "0x{0:x8}" },
	{ "pCallbackUsrData", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceGetStateData[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "uModule", 0x03, NULL },
	{ "pMem", 0x03, "0x{0:x8}" },
	{ "uMemSize", 0x03, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceGetInfo[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "pInfo", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceGetModuleType[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "uModule", 0x03, NULL },
	{ "puModuleType", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceGetModuleBypass[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "uModule", 0x03, NULL },
	{ "puBypassFlag", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceGetParamsOutOfRange[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "uModule", 0x03, NULL },
	{ "messageBuffer", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsPatchCreateRouting[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "pPatchInfo", 0x03, "0x{0:x8}" },
	{ "*pPatchHandle", 0x8D, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsPatchGetInfo[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "PatchHandle", 0x8D, NULL },
	{ "pRouteInfo", 0x03, "0x{0:x8}" },
	{ "pSetup", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceGetOutputPatch[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "VoiceHandle", 0x8D, NULL },
	{ "nOutputIndex", 0x83, NULL },
	{ "nSubIndex", 0x83, NULL },
	{ "*pPatchHandle", 0x8D, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsPatchRemoveRouting[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "PatchHandle", 0x8D, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoicePatchSetVolume[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "PatchHandle", 0x8D, NULL },
	{ "nOutputChannel", 0x83, NULL },
	{ "nInputChannel", 0x83, NULL },
	{ "fVol", 0x07, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoicePatchSetVolumes[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "PatchHandle", 0x8D, NULL },
	{ "nOutputChannel", 0x83, NULL },
	{ "pVolumes", 0x03, NULL },
	{ "nVols", 0x83, NULL },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoicePatchSetVolumesMatrix[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "PatchHandle", 0x8D, NULL },
	{ "pMatrix", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsQuerySizeRuntimeVoiceDef[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "pFileData", 0x03, "0x{0:x8}" },
	{ "puSize", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsCreateRuntimeVoiceDef[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "hSystem", 0x03, "0x{0:x8}" },
	{ "pFileData", 0x03, "0x{0:x8}" },
	{ "pOutDef", 0x03, "0x{0:x8}" },
	{ "uMemSize", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsSystemSetFlags[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "hSystem", 0x03, "0x{0:x8}" },
	{ "uSystemFlags", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsVoiceDefReturn[] = {
	{ "pVoiceDef", 0x03, "0x{0:x8}" },
};

static const NgsSulphaApiTraceParamDesc g_paramsAt9GetSectionDetails[] = {
	{ "ReturnCode", 0x83, "STRING_TABLE:1" },
	{ "nStartSampleOffset", 0x83, NULL },
	{ "nNumSamples", 0x83, NULL },
	{ "configData", 0x83, "0x{0:x8}" },
	{ "pAt9InfoBuffer", 0x03, "0x{0:x8}" },
};

const NgsSulphaApiTraceEventDesc g_ngsApiTraceEvents[63] = {
	{  1, "SystemGetRequiredMemorySize",		3, g_paramsSystemGetRequiredMemorySize },
	{  2, "SystemInit",				5, g_paramsSystemInit },
	{  3, "SystemUpdate",				2, g_paramsSystemHandleOnly },
	{  4, "SystemRelease",				2, g_paramsSystemHandleOnly },
	{  5, "SystemLock",				2, g_paramsSystemHandleOnly },
	{  6, "SystemUnlock",				2, g_paramsSystemHandleOnly },
	{  7, "SystemSetParamErrorCallback",		3, g_paramsSystemSetParamErrorCallback },
	{  8, "ModuleLink",				3, g_paramsModuleLink },
	{  9, "ModuleGetInfo",				4, g_paramsModuleGetInfo },
	{ 10, "ModuleGetNumPresets",			4, g_paramsModuleGetNumPresets },
	{ 11, "ModuleGetPreset",			5, g_paramsModuleGetPreset },
	{ 12, "RackGetRequiredMemorySize",		4, g_paramsRackGetRequiredMemorySize },
	{ 13, "RackInit",				5, g_paramsRackInit },
	{ 14, "RackGetVoiceHandle",			4, g_paramsRackGetVoiceHandle },
	{ 15, "RackRelease",				3, g_paramsRackCallback },
	{ 16, "RackSetParamErrorCallback",		3, g_paramsRackCallback },
	{ 17, "VoiceInit",				4, g_paramsVoiceInit },
	{ 18, "VoicePlay",				2, g_paramsVoiceHandleOnly },
	{ 19, "VoiceKeyOff",				2, g_paramsVoiceHandleOnly },
	{ 20, "VoiceKill",				2, g_paramsVoiceHandleOnly },
	{ 21, "VoicePause",				2, g_paramsVoiceHandleOnly },
	{ 22, "VoiceResume",				2, g_paramsVoiceHandleOnly },
	{ 23, "VoiceSetPreset",				3, g_paramsVoiceSetPreset },
	{ 24, "VoiceLockParams",			5, g_paramsVoiceLockParams },
	{ 25, "VoiceUnlockParams",			3, g_paramsVoiceUnlockParams },
	{ 26, "VoiceSetParamsBlock",			5, g_paramsVoiceSetParamsBlock },
	{ 27, "VoiceBypassModule",			4, g_paramsVoiceBypassModule },
	{ 28, "VoiceSetModuleCallback",			5, g_paramsVoiceSetModuleCallback },
	{ 29, "VoiceSetFinishedCallback",		4, g_paramsVoiceSetFinishedCallback },
	{ 30, "VoiceGetStateData",			5, g_paramsVoiceGetStateData },
	{ 31, "VoiceGetInfo",				3, g_paramsVoiceGetInfo },
	{ 32, "VoiceGetModuleType",			4, g_paramsVoiceGetModuleType },
	{ 33, "VoiceGetModuleBypass",			4, g_paramsVoiceGetModuleBypass },
	{ 34, "VoiceGetParamsOutOfRange",		4, g_paramsVoiceGetParamsOutOfRange },
	{ 35, "PatchCreateRouting",			3, g_paramsPatchCreateRouting },
	{ 36, "PatchGetInfo",				4, g_paramsPatchGetInfo },
	{ 37, "VoiceGetOutputPatch",			5, g_paramsVoiceGetOutputPatch },
	{ 38, "PatchRemoveRouting",			2, g_paramsPatchRemoveRouting },
	{ 39, "VoicePatchSetVolume",			5, g_paramsVoicePatchSetVolume },
	{ 40, "VoicePatchSetVolumes",			5, g_paramsVoicePatchSetVolumes },
	{ 41, "VoicePatchSetVolumesMatrix",		3, g_paramsVoicePatchSetVolumesMatrix },
	{ 42, "QuerySizeRuntimeVoiceDef",		3, g_paramsQuerySizeRuntimeVoiceDef },
	{ 43, "CreateRuntimeVoiceDef",			5, g_paramsCreateRuntimeVoiceDef },
	{ 44, "SystemSetFlags",				3, g_paramsSystemSetFlags },
	{ 45, "VoiceDefGetCompressorBuss",		1, g_paramsVoiceDefReturn },
	{ 46, "VoiceDefGetCompressorSideChainBuss",	1, g_paramsVoiceDefReturn },
	{ 47, "VoiceDefGetDelayBuss",			1, g_paramsVoiceDefReturn },
	{ 48, "VoiceDefGetDistortionBuss",		1, g_paramsVoiceDefReturn },
	{ 49, "VoiceDefGetEnvelopeBuss",		1, g_paramsVoiceDefReturn },
	{ 50, "VoiceDefGetEqBuss",			1, g_paramsVoiceDefReturn },
	{ 51, "VoiceDefGetMasterBuss",			1, g_paramsVoiceDefReturn },
	{ 52, "VoiceDefGetMixerBuss",			1, g_paramsVoiceDefReturn },
	{ 53, "VoiceDefGetPauserBuss",			1, g_paramsVoiceDefReturn },
	{ 54, "VoiceDefGetReverbBuss",			1, g_paramsVoiceDefReturn },
	{ 55, "VoiceDefGetSimpleVoice",			1, g_paramsVoiceDefReturn },
	{ 56, "VoiceDefGetTemplate1",			1, g_paramsVoiceDefReturn },
	{ 57, "VoiceDefGetSasEmuVoice",			1, g_paramsVoiceDefReturn },
	{ 58, "VoiceDefGetAtrac9Voice",			1, g_paramsVoiceDefReturn },
	{ 59, "VoiceDefGetSimpleAtrac9Voice",		1, g_paramsVoiceDefReturn },
	{ 60, "VoiceDefGetScreamVoice",			1, g_paramsVoiceDefReturn },
	{ 61, "VoiceDefGetScreamAtrac9Voice",		1, g_paramsVoiceDefReturn },
	{ 62, "AT9GetSectionDetails",			5, g_paramsAt9GetSectionDetails },
	{ 63, "VoiceDefGetPitchShiftBuss",		1, g_paramsVoiceDefReturn },
};

/* Message Log Agent */
static NgsSulphaFieldDef g_ngsMessageLogFields[4] = {
	{ 1, 0x8E, 32,  "File",      "Source file",                       NULL },
	{ 2, 0x83, 1,   "Line",      "Source line",                       NULL },
	{ 3, 0x8E, 128, "Component", "Originating component or function", NULL },
	{ 4, 0x8E, 256, "Message",   "Message to report",                 NULL },
};

NgsSulphaEventDef g_ngsMessageLogEventDefs[1] = {
	{ 4, 4, 2, "Info", "Informational message", g_ngsMessageLogFields },
};

const NgsSulphaAgentDef g_ngsMessageLogAgentDef = {
	"NGS Message Log",
	1, 0, 0,
	"Captures the NGS specific TTY messages.",
	{ 0x79EA2A0AU, 0x494A147CU, 0xCB1167BDU, 0xB70EDB53U },
	NGS_SULPHA_AGENT_MESSAGE_LOG,
	1, 0, 0, 0,
	g_ngsMessageLogEventDefs,
	NULL,
	NULL,
};

/* Named Objects Agent */
static NgsSulphaFieldDef g_ngsNamedObjectFields[3] = {
	{ 1, 0x03, 1,  "Handle", "Location",           NULL },
	{ 2, 0x03, 1,  "Length", "Length in bytes",    NULL },
	{ 3, 0x8E, 36, "Name",   "User-specified name", NULL },
};

NgsSulphaEventDef g_ngsNamedObjectsEventDefs[1] = {
	{ 1, 3, 7, "Named Object", "Registered/Unregistered", g_ngsNamedObjectFields },
};

const NgsSulphaAgentDef g_ngsNamedObjectsAgentDef = {
	"NGS Named Objects",
	1, 0, 0,
	"Captures the user objects that have been registered with NGS.",
	{ 0xB97C73A4U, 0x4DA51412U, 0x8B48A79FU, 0xCE5F34E6U },
	NGS_SULPHA_AGENT_NAMED_OBJECTS,
	1, 0, 0, 0,
	g_ngsNamedObjectsEventDefs,
	NULL,
	NULL,
};

/* PCM Audio Capture Agent */
static NgsSulphaFieldDef g_ngsPcmSinkDataFields[4] = {
	{ 1, 0x03, 1,    "SinkId", "Sink Id",                        NULL },
	{ 2, 0x03, 1,    "Seq",    "Sequence number of this packet", NULL },
	{ 3, 0xFE, 2048, "Data",   "PCM data",                       NULL },
	{ 4, 0x03, 1,    "Size",   "Size of PCM data in bytes",      NULL },
};

static NgsSulphaFieldDef g_ngsPcmSinkRegFields[5] = {
	{ 1, 0x03, 1, "SinkId",   "Sink Id",       NULL },
	{ 2, 0x03, 1, "DataType", "Data Type",     NULL },
	{ 3, 0x03, 1, "Format",   "Sub-Format",    NULL },
	{ 4, 0x03, 1, "Channels", "Channel Count", NULL },
	{ 5, 0x03, 1, "Rate",     "Sample Rate",   NULL },
};

static NgsSulphaFieldDef g_ngsPcmSourceUpdateFields[4] = {
	{ 1, 0x01, 1, "BufferCount", "Number of playback buffers", NULL },
	{ 2, 0x81, 1, "Current",     "Current playback buffer",    NULL },
	{ 3, 0x03, 1, "Position",    "Current playback position",  NULL },
	{ 4, 0x82, 1, "LoopCount",   "Current loop count",         NULL },
};

NgsSulphaEventDef g_ngsPcmCaptureEventDefs[3] = {
	{ 1, 4, 3, "PCM Sink Data Update", "Sink data updated",  g_ngsPcmSinkDataFields },
	{ 2, 5, 7, "PCM Sink Register",    "Sink registered",    g_ngsPcmSinkRegFields },
	{ 3, 4, 7, "PCM Source Update",    "Source updated",     g_ngsPcmSourceUpdateFields },
};

const NgsSulphaAgentDef g_ngsPcmCaptureAgentDef = {
	"NGS PCM Audio Capture",
	1, 0, 0,
	"Captures PCM audio from the NGS output modules. Note: NGS Synth Voice and NGS Module Output are required in order to capture PCM.",
	{ 0x83D6828DU, 0x40B121C7U, 0x921941A1U, 0x4864A182U },
	NGS_SULPHA_AGENT_PCM_CAPTURE,
	3, 0, 0, 0,
	g_ngsPcmCaptureEventDefs,
	NULL,
	NULL,
};

/* Synth Trace Agent */
static NgsSulphaFieldDef g_ngsSynthFields[10] = {
	{  1, 0x03, 1,  "Handle",        "SceNgsHSynSystem",                    NULL },
	{  2, 0x03, 1,  "MaxRacks",      "Maximum rack allocations",            NULL },
	{  3, 0x03, 1,  "MaxVoices",     "Maximum voice allocations",           NULL },
	{  4, 0x03, 1,  "Granularity",   "Size of audio frame in samples",      NULL },
	{  5, 0x03, 1,  "SampleRate",    "Sample rate of the system",           NULL },
	{  6, 0x03, 1,  "ModulesLinked", "Number of modules available",         NULL },
	{  7, 0x03, 1,  "MaxModules",    "Maximum number of modules allowed",   NULL },
	{  8, 0x02, 1,  "Index",         "Instance index",                      NULL },
	{  9, 0x8E, 28, "Name",          "User supplied name",                  NULL },
	{ 10, 0x03, 1,  "CycleCount",    "Not implemented",                     NULL },
};

static NgsSulphaFieldDef g_ngsRackFields[9] = {
	{ 1, 0x03, 1,  "Handle",                "SceNgsHRack",                  NULL },
	{ 2, 0x03, 1,  "Voices",                "Voice count",                  NULL },
	{ 3, 0x03, 1,  "AudioChannelsPerVoice", "Max channels per voice",       NULL },
	{ 4, 0x02, 1,  "Index",                 "Zero-based index",             NULL },
	{ 5, 0x03, 1,  "ProcessIndex",          "Processing stage",             NULL },
	{ 6, 0x03, 1,  "SubPatchesPerOutput",   "Number of patches at output",  NULL },
	{ 7, 0x03, 1,  "State",                 "Rack state",                   NULL },
	{ 8, 0x8E, 28, "Name",                  "Rack name",                    NULL },
	{ 9, 0x03, 1,  "CycleCount",            "Not implemented",              NULL },
};

static NgsSulphaFieldDef g_ngsVoiceFields[9] = {
	{ 1, 0x03, 1,  "Handle",        "SceNgsHVoice",          NULL },
	{ 2, 0x03, 1,  "PauseState",    "Pause State",           NULL },
	{ 3, 0x03, 1,  "ActiveState",   "Active State",          NULL },
	{ 4, 0x03, 1,  "KeyStateFlags", "Key States",            NULL },
	{ 5, 0x03, 1,  "NumChannels",   "Actual channel count",  NULL },
	{ 6, 0x03, 1,  "InstanceUId",   "Unique playback id",    NULL },
	{ 7, 0x02, 1,  "Index",         "Index within Rack",     NULL },
	{ 8, 0x8E, 28, "Name",          "Voice name",            NULL },
	{ 9, 0x03, 1,  "CycleCount",    "Not implemented",       NULL },
};

static NgsSulphaFieldDef g_ngsOutputFields[6] = {
	{ 1, 0x03, 1,  "Handle",         "SceNgsHPatch",           NULL },
	{ 2, 0x03, 1,  "OutputVoiceUId", "Destination id",         NULL },
	{ 3, 0x83, 1,  "NumOutChannels", "Output channel count",   NULL },
	{ 4, 0x83, 1,  "NumInChannels",  "Input channel count",    NULL },
	{ 5, 0x03, 1,  "State",          "Current state of output", NULL },
	{ 6, 0x8E, 28, "Name",           "Patch name",             NULL },
};

NgsSulphaEventDef g_ngsSynthTraceEventDefs[4] = {
	{ 1, 10, 7, "Synth",  "Parameters registered/updated", g_ngsSynthFields },
	{ 2,  9, 7, "Rack",   "Parameters registered/updated", g_ngsRackFields },
	{ 3,  9, 7, "Voice",  "Parameters registered/updated", g_ngsVoiceFields },
	{ 4,  6, 7, "Output", "Parameters registered/updated", g_ngsOutputFields },
};

const NgsSulphaAgentDef g_ngsSynthTraceAgentDef = {
	"NGS Synth Trace",
	1, 0, 0,
	"Captures and monitors changes to the NGS system, rack and voice objects.",
	{ 0x263B0B8FU, 0x46B587C8U, 0xA6034185U, 0x1991B1F5U },
	NGS_SULPHA_AGENT_SYNTH_TRACE,
	4, 0, 0, 0,
	g_ngsSynthTraceEventDefs,
	NULL,
	NULL,
};
