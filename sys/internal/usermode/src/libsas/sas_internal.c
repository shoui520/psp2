/*
 * The SAS system on the AS audio system: set up from a configuration in one buffer, its voices
 * the SYNTH's, its effects the PVERBs', and its calls each on the handle it was given.
 */
#include <stdarg.h>
#include "as/as_ssp.h"
#include "sas_internal.h"
#include "as/as_buss.h"
#include "as/as_core.h"
#include "as/as_master.h"
#include "as/as_rack.h"
#include "as/as_reverb.h"
#include "as/as_synth.h"
#include "as/as_util.h"

#define SCE_SAS_BLOCK(block, index)	(((block) << 16) | (index))

/* the curves sceSasSetSimpleADSR takes from its bits */
static const SceUInt8 sceSasReleaseModes[2] = { SCE_SAS_ADSR_MODE_LINEAR_INC,
						SCE_SAS_ADSR_MODE_REVEXPONENT };
static const SceUInt8 sceSasAttackModes[2] = { SCE_SAS_ADSR_MODE_LINEAR_DEC,
					       SCE_SAS_ADSR_MODE_BENT_LINEAR };
static const SceUInt8 sceSasSustainModes[4] = { SCE_SAS_ADSR_MODE_LINEAR_DEC,
						SCE_SAS_ADSR_MODE_LINEAR_INC,
						SCE_SAS_ADSR_MODE_BENT_LINEAR,
						SCE_SAS_ADSR_MODE_REVEXPONENT };

/*
 * One AS object of the class (its VENEZIA build when venezia), made from the configuration
 * format makes: its size is added to the total, and, while there is a buffer, it is made there.
 */
static int sceSasCreateHandle(SceSasAsHandle *parent, const char *className, const char *format,
			      int venezia, void **buffer, SceSize *total, SceSasAsHandle **handle,
			      ...)
{
	char name[64];
	char config[256];
	SceSize size;
	va_list arguments;

	if (venezia) {
		sceSasAsStrlcpy(name, sizeof(name), className);
		sceSasAsStrlcat(name, sizeof(name), "_VNZ");
		className = name;
	}
	va_start(arguments, handle);
	sceSasAsVsnprintf(config, sizeof(config), format, arguments);
	if (sceSasAsHandleGetSize(className, config, &size) < 0)
		return SCE_SAS_ERROR_FAIL;
	if (*buffer == NULL) {
		*handle = NULL;
	} else {
		if (sceSasAsHandleCreate(parent, className, config, *buffer, size, handle) < 0)
			return SCE_SAS_ERROR_FAIL;
		*buffer = (SceUInt8 *)*buffer + size;
	}
	*total += size;
	return 0;
}

/*
 * The SAS system the configuration describes: its size, and, with a buffer, the system set
 * up in it, its voices sent to the dry buss and each reverb, the reverbs on their own busses,
 * the mixer taking the dry signal and the master the mixer's.
 */
static int sceSasCreate(const char *config, void *buffer, SceSasSystem **outSas, SceSize *outSize)
{
	SceSasAsConfigEntry **list;
	SceSize configSize, total;
	SceUInt32 numVoices, numGrains, numReverbs, venezia, numBusses, i, j;
	SceSasAsHandle *core, *buss, *synth, *reverbs[SCE_SAS_MAX_REVERBS], *mixer, *master;
	SceSasAsHandle *racks[SCE_SAS_MAX_REVERBS + 3];
	SceSasSystem *sas;
	void *cursor;
	void *block;

	total = 0;
	sas = NULL;
	core = NULL;
	buss = NULL;
	synth = NULL;
	reverbs[0] = NULL;
	reverbs[1] = NULL;
	reverbs[2] = NULL;
	reverbs[3] = NULL;
	mixer = NULL;
	master = NULL;
	if (outSize != NULL)
		*outSize = 0;
	if (outSas != NULL)
		*outSas = NULL;
	if (sceSasAsConfigGetSize(config, &configSize) < 0 ||
	    sceSasAsConfigCreate(config, __builtin_alloca(configSize), configSize, &list) < 0)
		return SCE_SAS_ERROR_CONFIG;
	sceSasAsConfigGetUInt(list, "numVoices", SCE_SAS_VOICE_MAX, &numVoices);
	sceSasAsConfigGetUInt(list, "numGrains", SCE_SAS_GRAIN_SAMPLES, &numGrains);
	sceSasAsConfigGetUInt(list, "numReverbs", 1, &numReverbs);
	sceSasAsConfigGetUInt(list, "isVenezia", 0, &venezia);
	numBusses = numReverbs + 1;
	if (sceSasAsConfigCheckUnused(list) < 0 || numVoices - 1 > 0xff)
		return SCE_SAS_ERROR_CONFIG;
	if ((numGrains & 31) != 0 || numGrains - 64 > 2048 - 64)
		return SCE_SAS_ERROR_NSMPL;
	if (numReverbs > SCE_SAS_MAX_REVERBS)
		return SCE_SAS_ERROR_CONFIG;

	cursor = buffer;
	if (buffer != NULL) {
		sas = (SceSasSystem *)(((SceUIntPtr)buffer + 15) & ~15);
		sceSasAsMemZero(sas, sizeof(*sas));
		cursor = sas + 1;
		sceSasAsMemZero(cursor, numVoices * sizeof(SceSasVoice));
		cursor = (SceSasVoice *)cursor + numVoices;
		sceSasAsMemcpy(sas->magic, "SASH", sizeof(sas->magic));
		sceSasAsMemcpy(sas->endMagic, "SASE", sizeof(sas->endMagic));
		sas->grain = numGrains;
		sas->buffer = buffer;
		sas->numReverbs = numReverbs;
		sas->numVoices = numVoices;
		sas->voices = (SceSasVoice *)(sas + 1);
	}
	total += sizeof(SceSasSystem) + 20 + numVoices * sizeof(SceSasVoice);

	if (sceSasCreateHandle(NULL, "CORE", "numGranularities=256", 0, &cursor, &total,
			       &core) < 0)
		goto error;
	if (sas != NULL)
		sas->core = core;
	if (sceSasCreateHandle(core, "BUSS", "numGranularities=2048 numBusses=%d ", 0, &cursor,
			       &total, &buss, numBusses) < 0)
		goto error;
	if (sas != NULL)
		sas->buss = buss;
	if (sceSasCreateHandle(core, "SYNTH",
			       "numVoices=%d numVoiceSources=2 numVoiceEnvelopes=0 numVoiceFilters=0 "
			       "numVoiceVolumes=2 numVoiceSends=%d ", venezia != 0, &cursor, &total,
			       &synth, numVoices, numBusses) < 0)
		goto error;
	if (sas != NULL)
		sas->synth = synth;
	for (i = 0; i < numReverbs; i++) {
		if (sceSasCreateHandle(core, "PVERB", " ", venezia != 0, &cursor, &total,
				       &reverbs[i]) < 0)
			goto error;
		if (sas != NULL)
			sas->reverbs[i] = reverbs[i];
	}
	if (sceSasCreateHandle(core, "MIXER", "numInputs=%d", venezia != 0, &cursor, &total, &mixer,
			       numBusses) < 0)
		goto error;
	if (sas != NULL)
		sas->mixer = mixer;
	if (sceSasCreateHandle(core, "MASTER", "numBuffers=2", venezia != 0, &cursor, &total,
			       &master) < 0)
		goto error;
	if (sas != NULL) {
		SceSasAsSynthSends *sends;
		SceSasAsRackPorts *ports;
		SceSasAsReverbRackEffect *effect;

		sas->master = master;
		/* each voice sent at full level to the dry buss and to every reverb's */
		for (i = 0; i < sas->numVoices; i++) {
			if (sceSasAsHandleLock(sas->synth,
					       SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_SENDS, i),
					       &block) != 0)
				continue;
			sends = block;
			sends->mask = 1;
			sends->sends[0].buss = 0;
			sends->sends[0].volume = 0;
			sends->sends[0].level = 1.0f;
			for (j = 1; j <= numReverbs; j++) {
				sends->mask |= 1 << j;
				sends->sends[j].buss = j;
				sends->sends[j].volume = j;
				sends->sends[j].level = 1.0f;
			}
			sceSasAsHandleUnlock(sends);
		}
		if (sceSasAsCoreSetOutput(sas->core, sas->buss) < 0)
			goto error;
		/* each reverb from its buss to its buss, off */
		for (i = 0; i < numReverbs; i++) {
			if (sceSasAsHandleLock(sas->reverbs[i], SCE_SAS_AS_RACK_BLOCK_INPUTS,
					       &block) == 0) {
				ports = block;
				ports->mask = 1;
				ports->ports[0].buss = i + 1;
				ports->ports[0].level = 1.0f;
				sceSasAsHandleUnlock(ports);
			}
			if (sceSasAsHandleLock(sas->reverbs[i],
					       SCE_SAS_BLOCK(SCE_SAS_AS_REVERB_BLOCK_EFFECT, 0),
					       &block) == 0) {
				effect = block;
				effect->type = SCE_SAS_FX_TYPE_OFF;
				effect->feedback = 0.0f;
				effect->delay = 0.0f;
				effect->levels[0] = 0.0f;
				effect->levels[1] = 0.0f;
				effect->levels[2] = 0.0f;
				effect->levels[3] = 0.0f;
				sceSasAsHandleUnlock(effect);
			}
			if (sceSasAsHandleLock(sas->reverbs[i], SCE_SAS_AS_RACK_BLOCK_OUTPUTS,
					       &block) == 0) {
				ports = block;
				ports->mask = 1;
				ports->ports[0].buss = i + 1;
				ports->ports[0].level = 1.0f;
				sceSasAsHandleUnlock(ports);
			}
		}
		/* the mixer: the dry buss at full level, the reverbs' silent, into the dry buss */
		if (sceSasAsHandleLock(sas->mixer, SCE_SAS_AS_RACK_BLOCK_INPUTS, &block) == 0) {
			ports = block;
			ports->mask = 1;
			ports->ports[0].buss = 0;
			ports->ports[0].level = 1.0f;
			for (j = 1; j <= numReverbs; j++) {
				ports->mask |= 1 << j;
				ports->ports[j].buss = j;
				ports->ports[j].level = 0.0f;
			}
			sceSasAsHandleUnlock(ports);
		}
		if (sceSasAsHandleLock(sas->mixer, SCE_SAS_AS_RACK_BLOCK_OUTPUTS, &block) == 0) {
			ports = block;
			ports->mask = 1;
			ports->ports[0].buss = 0;
			ports->ports[0].level = 1.0f;
			sceSasAsHandleUnlock(ports);
		}
		if (sceSasAsHandleLock(sas->master, SCE_SAS_AS_RACK_BLOCK_INPUTS, &block) == 0) {
			ports = block;
			ports->mask = 1;
			ports->ports[0].buss = 0;
			ports->ports[0].mute = 0;
			ports->ports[1].buss = 1;
			ports->ports[0].level = 1.0f;
			ports->ports[1].mute = 1;
			ports->ports[1].level = 1.0f;
			sceSasAsHandleUnlock(ports);
		}
		/* rendered in this order: the voices, the reverbs, the mixer, the master */
		racks[0] = sas->synth;
		for (i = 0; i < numReverbs; i++)
			racks[i + 1] = sas->reverbs[i];
		racks[numReverbs + 1] = sas->mixer;
		racks[numReverbs + 2] = sas->master;
		if (sceSasAsCoreSetRacks(sas->core, (const SceUIntPtr *)racks, numReverbs + 3) < 0)
			goto error;
		sas->size = total;
	}
	if (outSize != NULL)
		*outSize = total;
	if (outSas != NULL)
		*outSas = sas;
	return 0;

error:
	if (sas != NULL) {
		if (sas->synth != NULL)
			sceSasAsHandleDestroy(sas->synth, NULL, NULL);
		if (sas->reverbs[0] != NULL)
			sceSasAsHandleDestroy(sas->reverbs[0], NULL, NULL);
		if (sas->reverbs[1] != NULL)
			sceSasAsHandleDestroy(sas->reverbs[1], NULL, NULL);
		if (sas->reverbs[2] != NULL)
			sceSasAsHandleDestroy(sas->reverbs[2], NULL, NULL);
		if (sas->reverbs[3] != NULL)
			sceSasAsHandleDestroy(sas->reverbs[3], NULL, NULL);
		if (sas->mixer != NULL)
			sceSasAsHandleDestroy(sas->mixer, NULL, NULL);
		if (sas->master != NULL)
			sceSasAsHandleDestroy(sas->master, NULL, NULL);
		if (sas->buss != NULL)
			sceSasAsHandleDestroy(sas->buss, NULL, NULL);
		if (sas->core != NULL)
			sceSasAsHandleDestroy(sas->core, NULL, NULL);
	}
	return SCE_SAS_ERROR_FAIL;
}

SceSasResult sceSasCheckGrain(SceUInt32 grain)
{
	if ((grain & 31) != 0)
		return SCE_SAS_ERROR_NSMPL;
	if (grain - 64 <= 2048 - 64)
		return 0;
	return SCE_SAS_ERROR_NSMPL;
}

SceSasResult sceSasGetNeededMemorySizeInternal(const char *config, SceSize *outSize)
{
	SceSize size;
	int result;

	if (outSize == NULL) {
		result = sceSasCreate(config, NULL, NULL, &size);
		return result < 0 ? result : 0;
	}
	*outSize = 0;
	result = sceSasCreate(config, NULL, NULL, &size);
	if (result < 0)
		return result;
	*outSize = size;
	return 0;
}

SceSasResult sceSasInitInternal(const char *config, void *buffer, SceSize bufferSize,
				SceSasSystem **outSas)
{
	SceSize size;
	int result;

	result = sceSasCreate(config, NULL, NULL, &size);
	if (result < 0)
		return result;
	if (buffer == NULL || size != bufferSize)
		return SCE_SAS_ERROR_ADDRESS;
	result = sceSasCreate(config, buffer, outSas, &size);
	return result < 0 ? result : 0;
}

SceSasResult sceSasExitInternal(SceSasSystem *sas, void **outBuffer, SceSize *outBufferSize)
{
	if (outBuffer != NULL)
		*outBuffer = NULL;
	if (outBufferSize != NULL)
		*outBufferSize = 0;
	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (sas->synth != NULL)
		sceSasAsHandleDestroy(sas->synth, NULL, NULL);
	if (sas->reverbs[0] != NULL)
		sceSasAsHandleDestroy(sas->reverbs[0], NULL, NULL);
	if (sas->reverbs[1] != NULL)
		sceSasAsHandleDestroy(sas->reverbs[1], NULL, NULL);
	if (sas->reverbs[2] != NULL)
		sceSasAsHandleDestroy(sas->reverbs[2], NULL, NULL);
	if (sas->reverbs[3] != NULL)
		sceSasAsHandleDestroy(sas->reverbs[3], NULL, NULL);
	if (sas->mixer != NULL)
		sceSasAsHandleDestroy(sas->mixer, NULL, NULL);
	if (sas->master != NULL)
		sceSasAsHandleDestroy(sas->master, NULL, NULL);
	if (sas->buss != NULL)
		sceSasAsHandleDestroy(sas->buss, NULL, NULL);
	if (sas->core != NULL)
		sceSasAsHandleDestroy(sas->core, NULL, NULL);
	if (outBuffer != NULL)
		*outBuffer = sas->buffer;
	if (outBufferSize != NULL)
		*outBufferSize = sas->size;
	sceSasAsMemZero(sas, sizeof(*sas));
	return 0;
}

SceSasResult sceSasSetGrainInternal(SceSasSystem *sas, SceUInt32 grain)
{
	if (sas == NULL || sas->core == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if ((grain & 31) != 0 || grain - 64 > 2048 - 64)
		return SCE_SAS_ERROR_NSMPL;
	if (sceSasAsCompareAndSwap(&sas->lock, 0, 1) != 0)
		return SCE_SAS_ERROR_BUSY;
	sas->grain = grain;
	sceSasAsCompareAndSwap(&sas->lock, 1, 0);
	return 0;
}

SceSasResult sceSasGetGrainInternal(SceSasSystem *sas)
{
	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	return sas->grain;
}

/*
 * Stereo: the reverbs and the mixer on, the master's two buffers from the first; multichannel:
 * all off, the master's buffers from both.
 */
SceSasResult sceSasSetOutputmodeInternal(SceSasSystem *sas, SceUInt32 outputmode)
{
	SceUInt32 on, buffers, inputs, i;
	SceSasAsRackPorts *ports;
	void *block;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (outputmode == SCE_SAS_OUTPUTMODE_STEREO) {
		buffers = 1;
		inputs = 1;
		on = 1;
	} else if (outputmode == SCE_SAS_OUTPUTMODE_MULTI) {
		on = 0;
		buffers = 2;
		inputs = 3;
	} else {
		return SCE_SAS_ERROR_MFMT;
	}
	if (sceSasAsCompareAndSwap(&sas->lock, 0, 1) != 0)
		return SCE_SAS_ERROR_BUSY;
	for (i = 0; i < sas->numReverbs; i++) {
		if (sceSasAsHandleLock(sas->reverbs[i], SCE_SAS_AS_RACK_BLOCK_OUTPUTS, &block) == 0) {
			ports = block;
			ports->mask = on;
			sceSasAsHandleUnlock(ports);
		}
	}
	if (sceSasAsHandleLock(sas->mixer, SCE_SAS_AS_RACK_BLOCK_INPUTS, &block) == 0) {
		ports = block;
		ports->mask = on;
		for (i = 0; i < sas->numReverbs; i++)
			ports->mask |= on << (i + 1);
		sceSasAsHandleUnlock(ports);
	}
	if (sceSasAsHandleLock(sas->mixer, SCE_SAS_AS_RACK_BLOCK_OUTPUTS, &block) == 0) {
		ports = block;
		ports->mask = on;
		sceSasAsHandleUnlock(ports);
	}
	if (sceSasAsHandleLock(sas->master, SCE_SAS_AS_RACK_BLOCK_INPUTS, &block) == 0) {
		ports = block;
		ports->mask = inputs;
		sceSasAsHandleUnlock(ports);
	}
	if (sceSasAsHandleLock(sas->master, SCE_SAS_BLOCK(SCE_SAS_AS_MASTER_BLOCK_BUFFERS, 0),
			       &block) == 0) {
		*(SceUInt32 *)block = buffers;
		sceSasAsHandleUnlock(block);
	}
	sas->outputmode = outputmode;
	sceSasAsCompareAndSwap(&sas->lock, 1, 0);
	return 0;
}

SceSasResult sceSasGetOutputmodeInternal(SceSasSystem *sas)
{
	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	return sas->outputmode;
}

static inline SceInt32 sceSasClamp16(SceInt32 value)
{
	value = value > 32767 ? 32767 : value;
	return value < -32768 ? -32768 : value;
}

/* a grain rendered into out, or, with a volume, added to out scaled by it (4.12) */
SceSasResult sceSasCoreInternal(SceSasSystem *sas, SceInt16 *out, SceInt32 lvol, SceInt32 rvol)
{
	SceInt16 *rendered;
	SceUInt32 grain, i;
	int mix, result;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (out == NULL)
		return SCE_SAS_ERROR_ADDRESS;
	if ((SceUInt32)lvol > SCE_SAS_VOLUME_MAX || (SceUInt32)rvol > SCE_SAS_VOLUME_MAX)
		return SCE_SAS_ERROR_VOLUME_VAL;
	if (sceSasAsCompareAndSwap(&sas->lock, 0, 1) != 0)
		return SCE_SAS_ERROR_BUSY;
	mix = rvol > 0 || lvol > 0;
	rendered = out;
	if (mix)
		rendered = __builtin_alloca(sas->grain * 2 * sizeof(SceInt16));
	result = sceSasAsCoreRender(sas->core, rendered, sas->grain) == 0 ? 0 : SCE_SAS_ERROR_FAIL;
	if (mix) {
		grain = sas->grain;
		for (i = 0; i < grain; i++) {
			out[0] = sceSasClamp16(rendered[0] + ((lvol * out[0]) >> 12));
			out[1] = sceSasClamp16(rendered[1] + ((rvol * out[1]) >> 12));
			out += 2;
			rendered += 2;
		}
	}
	sceSasAsCompareAndSwap(&sas->lock, 1, 0);
	return result;
}

/* an ADPCM (VAG) source: size bytes, 28 samples to 16 */
SceSasResult sceSasSetVoiceInternal(SceSasSystem *sas, SceInt32 iVoiceNum, const void *vagBuf,
				    SceSize size, SceUInt32 loopflag)
{
	SceSasAsSynthSources *sources;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (size == 0 || (size & 15) != 0)
		return SCE_SAS_ERROR_ADPCM_SIZE;
	if (loopflag > SCE_SAS_LOOP_ENABLE)
		return SCE_SAS_ERROR_LOOP_MODE;
	if (sceSasAsHandleLock(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_SOURCES, iVoiceNum),
			       (void **)&sources) != 0)
		return SCE_SAS_ERROR_FAIL;
	sources->sources[0].flags = loopflag;
	sources->sources[0].type = SCE_SAS_AS_SYNTH_SOURCE_ADPCM;
	sources->sources[0].loopCount = 0;
	sources->sources[0].next = SCE_SAS_AS_SYNTH_SOURCE_NEXT_STOP;
	sources->sources[0].pcm.address = (SceUIntPtr)vagBuf;
	sources->sources[0].length = SCE_SAS_AS_SYNTH_ADPCM_SAMPLES * (size >> 4);
	sources->sources[0].pcm.start = 0;
	sources->sources[0].numChannels = 1;
	sources->sources[0].channel = 0;
	*(SceUInt16 *)sources->sources[0].reserved = 0;
	sceSasAsHandleUnlock(sources);
	return 0;
}

/* a 16-bit PCM source of size samples, from loopsize on played again and again */
SceSasResult sceSasSetVoicePCMInternal(SceSasSystem *sas, SceInt32 iVoiceNum, const void *pcmBuf,
				       SceSize size, SceInt32 loopsize)
{
	SceSasAsSynthSources *sources;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if ((SceInt32)size <= 0)
		return SCE_SAS_ERROR_PCM_SIZE;
	if (loopsize >= 0 && loopsize >= (SceInt32)size)
		return SCE_SAS_ERROR_LOOP_MODE;
	if (sceSasAsHandleLock(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_SOURCES, iVoiceNum),
			       (void **)&sources) != 0)
		return SCE_SAS_ERROR_FAIL;
	if (loopsize < 0) {
		sources->sources[0].flags = 0;
		sources->sources[0].type = SCE_SAS_AS_SYNTH_SOURCE_PCM16;
		sources->sources[0].loopCount = 0;
		sources->sources[0].next = SCE_SAS_AS_SYNTH_SOURCE_NEXT_STOP;
		sources->sources[0].pcm.address = (SceUIntPtr)pcmBuf;
		sources->sources[0].length = size;
		sources->sources[0].pcm.start = 0;
		sources->sources[0].numChannels = 1;
		sources->sources[0].channel = 0;
		*(SceUInt16 *)sources->sources[0].reserved = 0;
	} else {
		sources->sources[0].flags = 0;
		sources->sources[0].type = SCE_SAS_AS_SYNTH_SOURCE_PCM16;
		sources->sources[0].loopCount = 0;
		sources->sources[0].next = 1;
		sources->sources[0].pcm.address = (SceUIntPtr)pcmBuf;
		sources->sources[0].length = size;
		sources->sources[0].pcm.start = 0;
		sources->sources[0].numChannels = 1;
		sources->sources[0].channel = 0;
		*(SceUInt16 *)sources->sources[0].reserved = 0;
		sources->sources[1].flags = 0;
		sources->sources[1].type = SCE_SAS_AS_SYNTH_SOURCE_PCM16;
		sources->sources[1].loopCount = SCE_SAS_AS_SYNTH_SOURCE_LOOP_FOREVER;
		sources->sources[1].next = SCE_SAS_AS_SYNTH_SOURCE_NEXT_STOP;
		sources->sources[1].pcm.address = (SceUIntPtr)((const SceInt16 *)pcmBuf + loopsize);
		sources->sources[1].length = size - loopsize;
		sources->sources[1].pcm.start = 0;
		sources->sources[1].numChannels = 1;
		sources->sources[1].channel = 0;
		*(SceUInt16 *)sources->sources[1].reserved = 0;
	}
	sceSasAsHandleUnlock(sources);
	return 0;
}

/* noise at the clock; a voice already playing noise takes it at once */
SceSasResult sceSasSetNoiseInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 uClk)
{
	SceSasAsSynthSources *sources;
	SceUInt8 type;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (uClk > SCE_SAS_NOISE_CLOCK_MAX)
		return SCE_SAS_ERROR_NOISE_CLOCK;
	if (sceSasAsHandleLock(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_SOURCES, iVoiceNum),
			       (void **)&sources) != 0)
		return SCE_SAS_ERROR_FAIL;
	type = sources->sources[0].type;
	sources->sources[0].flags = 0;
	sources->sources[0].type = SCE_SAS_AS_SYNTH_SOURCE_NOISE;
	sources->sources[0].loopCount = SCE_SAS_AS_SYNTH_SOURCE_LOOP_FOREVER;
	sources->sources[0].next = SCE_SAS_AS_SYNTH_SOURCE_NEXT_STOP;
	sources->sources[0].length = 0xffffffff;
	sources->sources[0].noise = uClk;
	sceSasAsHandleUnlock(sources);
	if (type == SCE_SAS_AS_SYNTH_SOURCE_NOISE)
		sceSasAsHandleSetEvent(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_EVENT, iVoiceNum),
				       SCE_SAS_AS_SYNTH_EVENT_RELOAD);
	return 0;
}

/* the voice's dry (l, r) and wet (wl, wr) volumes, 4.12, -1..1 */
SceSasResult sceSasSetVolumeInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceInt32 l,
				     SceInt32 r, SceInt32 wl, SceInt32 wr)
{
	SceSasAsSynthVolumes *volumes;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if ((SceUInt32)(l + SCE_SAS_VOLUME_MAX) > 2 * SCE_SAS_VOLUME_MAX ||
	    (SceUInt32)(r + SCE_SAS_VOLUME_MAX) > 2 * SCE_SAS_VOLUME_MAX ||
	    (SceUInt32)(wl + SCE_SAS_VOLUME_MAX) > 2 * SCE_SAS_VOLUME_MAX ||
	    (SceUInt32)(wr + SCE_SAS_VOLUME_MAX) > 2 * SCE_SAS_VOLUME_MAX)
		return SCE_SAS_ERROR_VOLUME_VAL;
	if (sceSasAsHandleLock(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_VOLUMES, iVoiceNum),
			       (void **)&volumes) != 0)
		return SCE_SAS_ERROR_FAIL;
	volumes->levels[0] = (float)l * (1.0f / SCE_SAS_VOLUME_MAX);
	volumes->levels[1] = (float)r * (1.0f / SCE_SAS_VOLUME_MAX);
	volumes->levels[2] = (float)wl * (1.0f / SCE_SAS_VOLUME_MAX);
	volumes->levels[3] = (float)wr * (1.0f / SCE_SAS_VOLUME_MAX);
	sceSasAsHandleUnlock(volumes);
	return 0;
}

/* the pitch, 4.12 (0x1000: as recorded) */
SceSasResult sceSasSetPitchInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceInt32 pitch)
{
	float *ratio;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if ((SceUInt32)(pitch - SCE_SAS_PITCH_MIN) >= SCE_SAS_PITCH_MAX)
		return SCE_SAS_ERROR_PITCH_VAL;
	if (sceSasAsHandleLock(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_PITCH, iVoiceNum),
			       (void **)&ratio) != 0)
		return SCE_SAS_ERROR_FAIL;
	*ratio = (float)pitch / SCE_SAS_PITCH_BASE;
	sceSasAsHandleUnlock(ratio);
	return 0;
}

SceSasResult sceSasSetDistortionInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 flag,
					 SceUInt32 drive, SceUInt32 gain)
{
	SceSasAsSynthDistortion *distortion;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (flag > SCE_SAS_DISTORTION_FLAG_ENABLE || drive > SCE_SAS_DISTORTION_DRIVE_MAX ||
	    gain > SCE_SAS_DISTORTION_GAIN_MAX)
		return SCE_SAS_ERROR_DISTORTION_VAL;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (sceSasAsHandleLock(sas->synth,
			       SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_DISTORTION, iVoiceNum),
			       (void **)&distortion) != 0)
		return SCE_SAS_ERROR_FAIL;
	distortion->enable = flag;
	distortion->gain = (float)(SceInt32)drive * (1.0f / SCE_SAS_DISTORTION_DRIVE_BASE);
	distortion->level = (float)(SceInt32)gain * (1.0f / SCE_SAS_DISTORTION_GAIN_BASE);
	sceSasAsHandleUnlock(distortion);
	return 0;
}

/* the rates flag selects */
SceSasResult sceSasSetADSRInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 flag,
				   SceUInt32 ar, SceUInt32 dr, SceUInt32 sr, SceUInt32 rr)
{
	SceSasAsSynthAdsr *adsr;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (((flag & SCE_SAS_ATTACK_VALID) && (SceInt32)ar < 0) ||
	    ((flag & SCE_SAS_DECAY_VALID) && (SceInt32)dr < 0) ||
	    ((flag & SCE_SAS_SUSTAIN_VALID) && (SceInt32)sr < 0) ||
	    ((flag & SCE_SAS_RELEASE_VALID) && (SceInt32)rr < 0))
		return SCE_SAS_ERROR_ADSR_VAL;
	if (sceSasAsHandleLock(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_ADSR, iVoiceNum),
			       (void **)&adsr) != 0)
		return SCE_SAS_ERROR_FAIL;
	adsr->enable = 1;
	if (flag & SCE_SAS_ATTACK_VALID)
		adsr->attackRate = ar;
	if (flag & SCE_SAS_DECAY_VALID)
		adsr->decayRate = dr;
	if (flag & SCE_SAS_SUSTAIN_VALID)
		adsr->sustainRate = sr;
	if (flag & SCE_SAS_RELEASE_VALID)
		adsr->releaseRate = rr;
	sceSasAsHandleUnlock(adsr);
	return 0;
}

/* the curves flag selects; each phase takes only some */
SceSasResult sceSasSetADSRmodeInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 flag,
				       SceUInt32 am, SceUInt32 dm, SceUInt32 sm, SceUInt32 rm)
{
	SceSasAsSynthAdsr *adsr;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (((flag & SCE_SAS_ATTACK_VALID) && !((0x15 >> am) & 1)) ||
	    ((flag & SCE_SAS_DECAY_VALID) && !((0x2a >> dm) & 1)) ||
	    ((flag & SCE_SAS_SUSTAIN_VALID) && !((0x3f >> sm) & 1)) ||
	    ((flag & SCE_SAS_RELEASE_VALID) && !((0x2a >> rm) & 1)))
		return SCE_SAS_ERROR_ADSR_MODE;
	if (sceSasAsHandleLock(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_ADSR, iVoiceNum),
			       (void **)&adsr) != 0)
		return SCE_SAS_ERROR_FAIL;
	adsr->enable = 1;
	if (flag & SCE_SAS_ATTACK_VALID)
		adsr->modes.attack = am;
	if (flag & SCE_SAS_DECAY_VALID)
		adsr->modes.decay = dm;
	if (flag & SCE_SAS_SUSTAIN_VALID)
		adsr->modes.sustain = sm;
	if (flag & SCE_SAS_RELEASE_VALID)
		adsr->modes.release = rm;
	sceSasAsHandleUnlock(adsr);
	return 0;
}

SceSasResult sceSasSetSLInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 sl)
{
	SceSasAsSynthAdsr *adsr;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (sl > SCE_SAS_ENVELOPE_HEIGHT_MAX)
		return SCE_SAS_ERROR_ADSR_VAL;
	if (sceSasAsHandleLock(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_ADSR, iVoiceNum),
			       (void **)&adsr) != 0)
		return SCE_SAS_ERROR_FAIL;
	adsr->sustainLevel = sl;
	adsr->enable = 1;
	sceSasAsHandleUnlock(adsr);
	return 0;
}

/* the envelope from the PSP's two ADSR words; both 0: no envelope */
SceSasResult sceSasSetSimpleADSRInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt16 adsr1,
					 SceUInt16 adsr2)
{
	SceSasAsSynthAdsr *adsr;
	SceUInt32 sustainMode, releaseMode, attackMode, shift;
	SceUInt32 attackRate, sustainRate, releaseRate, top;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (sceSasAsHandleLock(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_BLOCK_ADSR, iVoiceNum),
			       (void **)&adsr) != 0)
		return SCE_SAS_ERROR_FAIL;
	if ((adsr1 | adsr2) != 0) {
		sustainMode = sceSasSustainModes[adsr2 >> 14];
		attackRate = (SceUInt32)(7 - ((adsr1 >> 8) & 3)) << 26 >> ((adsr1 >> 10) & 31);
		if (attackRate == 0)
			attackRate = 1;
		releaseMode = sceSasReleaseModes[(adsr2 & 0x20) != 0];
		if (sustainMode == SCE_SAS_ADSR_MODE_REVEXPONENT)
			sustainRate = (7 - ((adsr2 & 0xff) >> 6)) << 24;
		else
			sustainRate = (7 - ((adsr2 & 0xff) >> 6)) << 26;
		sustainRate >>= ((adsr2 >> 6) & 0x7f) >> 2;
		shift = adsr2 & 31;
		if (sustainRate == 0)
			sustainRate = 1;
		if (releaseMode == SCE_SAS_ADSR_MODE_LINEAR_DEC) {
			top = 0x40000000;
			shift += 2;
		} else {
			top = 0x80000000;
		}
		if (releaseMode == SCE_SAS_ADSR_MODE_LINEAR_DEC)
			releaseRate = (SceInt32)top >> shift;
		else
			releaseRate = top >> shift;
		attackMode = sceSasAttackModes[adsr1 >> 15];
		adsr->modes.sustain = sustainMode;
		adsr->sustainLevel = ((adsr1 & 15) + 1) << 26;
		if (releaseRate == 0)
			releaseRate = 1;
		adsr->modes.attack = attackMode;
		adsr->modes.release = releaseMode;
		adsr->attackRate = attackRate;
		adsr->decayRate = 0x80000000u >> ((adsr1 & 0xff) >> 4);
		adsr->sustainRate = sustainRate;
		adsr->releaseRate = releaseRate;
		adsr->enable = 1;
		adsr->modes.decay = SCE_SAS_ADSR_MODE_REVEXPONENT;
	} else {
		adsr->enable = 0;
	}
	sceSasAsHandleUnlock(adsr);
	return 0;
}

SceSasResult sceSasSetKeyOnInternal(SceSasSystem *sas, SceInt32 iVoiceNum)
{
	int result;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (sceSasAsCoreLockUpdate(sas->core) != 0)
		return SCE_SAS_ERROR_FAIL;
	if (sas->voices[iVoiceNum].paused != 0 || sas->voices[iVoiceNum].keyOn != 0) {
		result = SCE_SAS_ERROR_INVALID_STATE;
	} else {
		result = SCE_SAS_ERROR_FAIL;
		if (sceSasAsHandleSetEvent(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_EVENT, iVoiceNum),
					   SCE_SAS_AS_SYNTH_EVENT_KEY_ON) == 0) {
			result = 0;
			sas->voices[iVoiceNum].keyOn = 1;
		}
	}
	sceSasAsCoreUnlockUpdate(sas->core);
	return result;
}

SceSasResult sceSasSetKeyOffInternal(SceSasSystem *sas, SceInt32 iVoiceNum)
{
	int result;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (sceSasAsCoreLockUpdate(sas->core) != 0)
		return SCE_SAS_ERROR_FAIL;
	if (sas->voices[iVoiceNum].paused != 0 || sas->voices[iVoiceNum].keyOn == 0) {
		result = SCE_SAS_ERROR_INVALID_STATE;
	} else {
		result = SCE_SAS_ERROR_FAIL;
		if (sceSasAsHandleSetEvent(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_EVENT, iVoiceNum),
					   SCE_SAS_AS_SYNTH_EVENT_KEY_OFF) == 0) {
			result = 0;
			sas->voices[iVoiceNum].keyOn = 0;
		}
	}
	sceSasAsCoreUnlockUpdate(sas->core);
	return result;
}

/* a keyed-on voice paused, or a paused one resumed */
SceSasResult sceSasSetPauseInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 pauseFlag)
{
	SceSasVoice *voice;
	SceUInt32 event;
	int result;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (sceSasAsCoreLockUpdate(sas->core) != 0)
		return SCE_SAS_ERROR_FAIL;
	result = SCE_SAS_ERROR_INVALID_STATE;
	if (pauseFlag == SCE_SAS_PAUSE_ENABLE) {
		voice = &sas->voices[iVoiceNum];
		if (voice->paused != 0 || voice->keyOn == 0)
			goto unlock;
		event = SCE_SAS_AS_SYNTH_EVENT_PAUSE;
	} else if (pauseFlag == SCE_SAS_PAUSE_DISABLE) {
		voice = &sas->voices[iVoiceNum];
		if (voice->paused != 1 || voice->keyOn == 0)
			goto unlock;
		event = SCE_SAS_AS_SYNTH_EVENT_RESUME;
	} else {
		goto unlock;
	}
	result = SCE_SAS_ERROR_FAIL;
	if (sceSasAsHandleSetEvent(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_EVENT, iVoiceNum),
				   event) == 0) {
		result = 0;
		sas->voices[iVoiceNum].paused = pauseFlag != 0;
	}
unlock:
	sceSasAsCoreUnlockUpdate(sas->core);
	return result;
}

/* 1 once the voice has stopped */
SceSasResult sceSasGetEndStateInternal(SceSasSystem *sas, SceInt32 iVoiceNum)
{
	SceInt16 state[2];			/* the voice's flags and error */

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	if (sceSasAsHandleGetState(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_STATE_VOICE, iVoiceNum),
				   state) < 0)
		return SCE_SAS_ERROR_FAIL;
	return !(state[0] & 1);
}

SceSasResult sceSasGetPauseStateInternal(SceSasSystem *sas, SceInt32 iVoiceNum)
{
	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	return sas->voices[iVoiceNum].paused != 0;
}

/* the envelope's level, 2.30 */
SceSasResult sceSasGetEnvelopeInternal(SceSasSystem *sas, SceInt32 iVoiceNum)
{
	float envelope[2];			/* the level and the peak */

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (iVoiceNum < 0 || iVoiceNum >= sas->numVoices)
		return SCE_SAS_ERROR_VOICE_INDEX;
	sceSasAsHandleGetState(sas->synth, SCE_SAS_BLOCK(SCE_SAS_AS_SYNTH_STATE_ENVELOPE, iVoiceNum),
			       envelope);
	return (SceInt32)(envelope[0] * 1073741824.0f);
}

/* the dry signal and each reverb (bit i: reverb i) into the mix, on or off */
SceSasResult sceSasSetEffectInternal(SceSasSystem *sas, SceInt32 drySwitch, SceInt32 wetSwitch)
{
	SceSasAsRackPorts *ports;
	SceUInt32 i;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (sas->outputmode == SCE_SAS_OUTPUTMODE_MULTI)
		return SCE_SAS_ERROR_FX_UNAVAILABLE;
	if (sceSasAsHandleLock(sas->mixer, SCE_SAS_AS_RACK_BLOCK_INPUTS, (void **)&ports) != 0)
		return 0;
	ports->mask = 1;
	ports->ports[0].buss = 0;
	ports->ports[0].level = drySwitch != 0 ? 1.0f : 0.0f;
	for (i = 0; i < sas->numReverbs; i++) {
		ports->mask |= 1 << (i + 1);
		ports->ports[i + 1].buss = i + 1;
		ports->ports[i + 1].level = ((SceUInt32)wetSwitch >> i) & 1 ? 1.0f : 0.0f;
	}
	sceSasAsHandleUnlock(ports);
	return 0;
}

SceSasResult sceSasSetEffectTypeInternal(SceSasSystem *sas, SceInt32 type)
{
	SceSasAsReverbRackEffect *effect;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (sas->outputmode == SCE_SAS_OUTPUTMODE_MULTI)
		return SCE_SAS_ERROR_FX_UNAVAILABLE;
	if ((SceUInt32)(type - SCE_SAS_FX_TYPE_OFF) > SCE_SAS_FX_TYPE_COUNT)
		return SCE_SAS_ERROR_FX_TYPE;
	if (sceSasAsHandleLock(sas->reverbs[0], SCE_SAS_BLOCK(SCE_SAS_AS_REVERB_BLOCK_EFFECT, 0),
			       (void **)&effect) != 0)
		return SCE_SAS_ERROR_FAIL;
	effect->type = type;
	sceSasAsHandleUnlock(effect);
	return 0;
}

SceSasResult sceSasSetEffectVolumeInternal(SceSasSystem *sas, SceInt32 valL, SceInt32 valR)
{
	SceSasAsReverbRackEffect *effect;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (sas->outputmode == SCE_SAS_OUTPUTMODE_MULTI)
		return SCE_SAS_ERROR_FX_UNAVAILABLE;
	if (valL < 0 || valL > SCE_SAS_FX_VOLUME_MAX || valR < 0 || valR > SCE_SAS_FX_VOLUME_MAX)
		return SCE_SAS_ERROR_FX_VOLUME_VAL;
	if (sceSasAsHandleLock(sas->reverbs[0], SCE_SAS_BLOCK(SCE_SAS_AS_REVERB_BLOCK_EFFECT, 0),
			       (void **)&effect) != 0)
		return SCE_SAS_ERROR_FAIL;
	effect->levels[0] = 0.0f;
	effect->levels[1] = 0.0f;
	effect->levels[2] = (float)valL * (1.0f / SCE_SAS_FX_VOLUME_MAX);
	effect->levels[3] = (float)valR * (1.0f / SCE_SAS_FX_VOLUME_MAX);
	sceSasAsHandleUnlock(effect);
	return 0;
}

/* the delay and the feedback, 0..127 */
SceSasResult sceSasSetEffectParamInternal(SceSasSystem *sas, SceUInt32 delayTime,
					  SceUInt32 feedback)
{
	SceSasAsReverbRackEffect *effect;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (sas->outputmode == SCE_SAS_OUTPUTMODE_MULTI)
		return SCE_SAS_ERROR_FX_UNAVAILABLE;
	if (delayTime > 0x7f)
		return SCE_SAS_ERROR_FX_DELAY;
	if (feedback > 0x7f)
		return SCE_SAS_ERROR_FX_FEEDBACK;
	if (sceSasAsHandleLock(sas->reverbs[0], SCE_SAS_BLOCK(SCE_SAS_AS_REVERB_BLOCK_EFFECT, 0),
			       (void **)&effect) != 0)
		return SCE_SAS_ERROR_FAIL;
	effect->delay = (float)(SceInt32)delayTime * (1.0f / 128);
	effect->feedback = (float)(SceInt32)feedback * (1.0f / 128);
	sceSasAsHandleUnlock(effect);
	return 0;
}

/* the largest magnitude of count samples */
static inline SceInt32 sceSasPeak(const SceInt32 *samples, SceUInt32 count)
{
	SceInt32 peak, value;

	peak = 0;
	while (count-- != 0) {
		value = *samples++;
		if (value < 0)
			value = -value;
		if (peak < value)
			peak = value;
	}
	return peak;
}

/* the dry buss's peaks, left and right */
SceSasResult sceSasGetDryPeakInternal(SceSasSystem *sas, SceInt32 *outL, SceInt32 *outR)
{
	SceSasAsBussBuffers *buffers;
	SceInt32 peak;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (outL != NULL)
		*outL = 0;
	if (outR != NULL)
		*outR = 0;
	if (sceSasAsHandleLock(sas->buss, SCE_SAS_AS_BUSS_BLOCK_BUFFERS, (void **)&buffers) != 0)
		return SCE_SAS_ERROR_FAIL;
	if (buffers->numBusses == 0) {
		if (outL != NULL)
			*outL = 0;
		if (outR != NULL)
			*outR = 0;
	} else {
		peak = sceSasPeak(buffers->buffers[0], buffers->numGranularities);
		if (outL != NULL)
			*outL = peak;
		peak = sceSasPeak(buffers->buffers[1], buffers->numGranularities);
		if (outR != NULL)
			*outR = peak;
	}
	sceSasAsHandleUnlock(buffers);
	return 0;
}

/* the first reverb's buss's peaks, left and right */
SceSasResult sceSasGetWetPeakInternal(SceSasSystem *sas, SceInt32 *outL, SceInt32 *outR)
{
	SceSasAsBussBuffers *buffers;
	SceInt32 peak;
	SceInt32 buss;
	SceUInt32 channel;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (outL != NULL)
		*outL = 0;
	if (outR != NULL)
		*outR = 0;
	if (sceSasAsHandleLock(sas->buss, SCE_SAS_AS_BUSS_BLOCK_BUFFERS, (void **)&buffers) != 0)
		return SCE_SAS_ERROR_FAIL;
	buss = 1;
	if (buffers->numBusses <= buss) {
		if (outL != NULL)
			*outL = 0;
		if (outR != NULL)
			*outR = 0;
	} else {
		for (channel = 0; channel < 2; channel++) {
			peak = sceSasPeak(buffers->buffers[buss * buffers->numChannels + channel],
					  buffers->numGranularities);
			if (channel == 0) {
				if (outL != NULL)
					*outL = peak;
			} else {
				if (outR != NULL)
					*outR = peak;
			}
		}
	}
	sceSasAsHandleUnlock(buffers);
	return 0;
}

/* the dry buss's peaks, as sceSasGetDryPeakInternal */
SceSasResult sceSasGetPreMasterPeakInternal(SceSasSystem *sas, SceInt32 *outL, SceInt32 *outR)
{
	SceSasAsBussBuffers *buffers;
	SceInt32 peak;

	if (sas == NULL)
		return SCE_SAS_ERROR_NOTINIT;
	if (outL != NULL)
		*outL = 0;
	if (outR != NULL)
		*outR = 0;
	if (sceSasAsHandleLock(sas->buss, SCE_SAS_AS_BUSS_BLOCK_BUFFERS, (void **)&buffers) != 0)
		return SCE_SAS_ERROR_FAIL;
	if (buffers->numBusses == 0) {
		if (outL != NULL)
			*outL = 0;
		if (outR != NULL)
			*outR = 0;
	} else {
		peak = sceSasPeak(buffers->buffers[0], buffers->numGranularities);
		if (outL != NULL)
			*outL = peak;
		peak = sceSasPeak(buffers->buffers[1], buffers->numGranularities);
		if (outR != NULL)
			*outR = peak;
	}
	sceSasAsHandleUnlock(buffers);
	return 0;
}
