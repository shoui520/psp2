/* Built-in input module description. */

#include "ngs_module_memory.h"

const struct NgsAudioFormat *ngsGetInputFormat(void);
int ngsGetInputMemoryInfo(const struct NgsModuleInitParams *params,
			NgsModuleMemoryInfo *info);
extern const char *ngsModuleOutputName;

NgsModuleAudioInterface ngsInputOutputs[1];
NgsModuleDefinition ngsInputModule;

int ngsGetInputModule(NgsModuleDefinition **module)
{
	ngsInputOutputs[0].formats = ngsGetInputFormat();
	ngsInputOutputs[0].name = ngsModuleOutputName;
	ngsInputOutputs[0].supportedFormatCount = 1;
	ngsInputModule.outputCount = 1;
	ngsInputModule.outputs = ngsInputOutputs;
	ngsInputModule.info = "name = Input\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Input/Mixer\n"
		"desc = Acts as a signal input allowing multiple outputs to route into it dynamically at runtime.\n";
	ngsInputModule.getMemoryInfo = ngsGetInputMemoryInfo;
	ngsInputModule.version = 0x10002;
	ngsInputModule.dspControl = 3;
	ngsInputModule.inputCount = 0;
	ngsInputModule.inputs = 0;
	ngsInputModule.paramInterfaceCount = 0;
	ngsInputModule.paramInterfaces = 0;
	ngsInputModule.codecResourceId = 0;
	ngsInputModule.presetCount = 0;
	ngsInputModule.presets = 0;
	ngsInputModule.flags = 0;
	ngsInputModule.moduleId = 0x5ce0;
	ngsInputModule.dspDispatchIds[0] = 0x5ce0;
	ngsInputModule.dspDispatchIds[1] = 0x5ce0;
	*module = &ngsInputModule;
	return 0;
}
