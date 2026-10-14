/* Built-in output module description. */

#include "ngs_module_memory.h"

const struct NgsAudioFormat *ngsGetOutputFormat(void);
int ngsGetOutputMemoryInfo(const struct NgsModuleInitParams *params,
			 NgsModuleMemoryInfo *info);
extern const char *ngsModuleInputName;

NgsModuleAudioInterface ngsOutputInputs[1];
NgsModuleDefinition ngsOutputModule;

int ngsGetOutputModule(NgsModuleDefinition **module)
{
	ngsOutputInputs[0].formats = ngsGetOutputFormat();
	ngsOutputInputs[0].name = ngsModuleInputName;
	ngsOutputInputs[0].supportedFormatCount = 1;
	ngsOutputModule.info = "name = Output\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Output\n"
		"desc = Allows user return of an audio signal\n";
	ngsOutputModule.inputCount = 1;
	ngsOutputModule.inputs = ngsOutputInputs;
	ngsOutputModule.getMemoryInfo = ngsGetOutputMemoryInfo;
	ngsOutputModule.version = 0x10002;
	ngsOutputModule.paramInterfaceCount = 0;
	ngsOutputModule.paramInterfaces = 0;
	ngsOutputModule.outputCount = 0;
	ngsOutputModule.outputs = 0;
	ngsOutputModule.codecResourceId = 0;
	ngsOutputModule.dspControl = 0;
	ngsOutputModule.flags = 0;
	ngsOutputModule.presetCount = 0;
	ngsOutputModule.presets = 0;
	ngsOutputModule.moduleId = 0x5ced;
	ngsOutputModule.dspDispatchIds[0] = 0x5ced;
	ngsOutputModule.dspDispatchIds[1] = 0x5ced;
	*module = &ngsOutputModule;
	return 0;
}
