/* Audio-buffer references used while assigning module input/output buffers. */

#include <stddef.h>
#include <string.h>

#include "ngs_rack.h"

typedef struct NgsBufferReference {
	int referenceCount;
	unsigned int moduleIndex;
	unsigned int outputIndex;
	struct NgsBufferReference *alias;
} NgsBufferReference;

typedef char NgsBufferReferenceSize[(sizeof(NgsBufferReference) == 16) ? 1 : -1];

void ngsReleaseBufferReference(NgsBufferReference *reference,
			       NgsBufferReference **alias)
{
	NgsBufferReference *first;
	NgsBufferReference *second;
	NgsBufferReference *third;

	--reference->referenceCount;
	first = *alias;
	if (first != NULL && reference->referenceCount == 0) {
		--first->referenceCount;
		second = first->alias;
		if (second != NULL && first->referenceCount == 0) {
			--second->referenceCount;
			third = second->alias;
			if (third != NULL && second->referenceCount == 0) {
				ngsReleaseBufferReference(third, &third->alias);
				second->alias = NULL;
			}
			first->alias = NULL;
		}
		*alias = NULL;
	}
}

/* The buffers a voice's modules pass audio in. */
#define NGS_VOICE_BUFFER_COUNT	6

/* The first buffer nothing references, or -1. */
static inline int ngsFindFreeBuffer(const NgsBufferReference *buffers)
{
	int slot;

	for (slot = 0; slot < NGS_VOICE_BUFFER_COUNT; slot++) {
		if (buffers[slot].referenceCount == 0)
			return slot;
	}
	return -1;
}

/*
 * Assigns each module input and output of a voice one of the buffers: an
 * input the buffer its bound output was written to, an output a free one.
 * A bypassed module writes its first output into its first input's buffer.
 */
void ngsAssignModuleBuffers(unsigned char *inputSlots,
			    unsigned char *outputSlots, const NgsVoiceDsp *dsp,
			    const NgsRackModuleList *list)
{
	NgsBufferReference buffers[NGS_VOICE_BUFFER_COUNT];
	NgsBufferReference *inputs[NGS_VOICE_BUFFER_COUNT];
	NgsBufferReference *outputs[NGS_VOICE_BUFFER_COUNT];
	NgsBufferReference *target;
	const NgsRackModule *module;
	unsigned int moduleIndex, index, binding, source, output;
	unsigned int inputIndex = 0, outputIndex = 0;
	int count, slot, buffer, alias;

	for (buffer = 0; buffer < NGS_VOICE_BUFFER_COUNT; buffer++)
		buffers[buffer].referenceCount = 0;

	for (moduleIndex = 0; moduleIndex < list->moduleCount; moduleIndex++) {
		outputs[0] = NULL;
		memset(inputs, 0, sizeof(inputs));
		module = &list->modules[moduleIndex];

		for (index = 0; index < module->inputCount; index++) {
			if (module->inputBindings != NULL)
				binding = module->inputBindings[index];
			else
				binding = (unsigned short)(moduleIndex - 1) |
					(index << 16);
			source = binding & 0xffff;
			output = binding >> 16;
			slot = -1;
			for (buffer = 0; buffer < NGS_VOICE_BUFFER_COUNT; buffer++) {
				if (buffers[buffer].outputIndex != output ||
				    buffers[buffer].moduleIndex != source ||
				    buffers[buffer].referenceCount == 0)
					continue;
				target = buffers[buffer].alias;
				if (target == NULL)
					target = &buffers[buffer];
				for (alias = 0; alias < NGS_VOICE_BUFFER_COUNT; alias++) {
					if (target == &buffers[alias])
						break;
				}
				if (alias < NGS_VOICE_BUFFER_COUNT) {
					inputs[index] = &buffers[buffer];
					slot = alias;
					break;
				}
			}
			inputSlots[inputIndex++] = slot;
			module = &list->modules[moduleIndex];
		}

		for (index = 0; index < module->outputCount; index++) {
			count = 1;
			if (module->outputBindings != NULL) {
				count = module->outputBindings[index];
				if (count < 0) {
					/* Written to the voice output, not to a buffer. */
					slot = (unsigned char)
						(((ngsFindFreeBuffer(buffers) + 8) << 4) | count);
					outputSlots[outputIndex] = slot;
					outputs[index] = (slot & 0x80) ?
						NULL : &buffers[slot];
					goto next;
				}
			}
			slot = ngsFindFreeBuffer(buffers);
			if (slot < 0) {
				outputSlots[outputIndex] = 0xff;
				outputs[index] = NULL;
			} else {
				buffers[slot].referenceCount = count;
				buffers[slot].outputIndex = index;
				outputSlots[outputIndex] = slot;
				buffers[slot].moduleIndex = moduleIndex;
				buffers[slot].alias = NULL;
				outputs[index] = &buffers[slot];
			}
next:
			outputIndex++;
			module = &list->modules[moduleIndex];
		}

		if ((dsp->bypassFlags & (1 << moduleIndex)) != 0 &&
		    inputs[0] != NULL && outputs[0] != NULL) {
			if (inputs[0]->referenceCount == 1) {
				inputs[0]->referenceCount = outputs[0]->referenceCount;
				inputs[0]->moduleIndex = outputs[0]->moduleIndex;
				inputs[0]->outputIndex = outputs[0]->outputIndex;
				outputs[0]->referenceCount = 0;
			} else {
				outputs[0]->alias = inputs[0];
			}
			for (index = 1; index < list->modules[moduleIndex].inputCount; index++)
				ngsReleaseBufferReference(inputs[index],
							  &inputs[index]->alias);
		} else {
			for (index = 0; index < list->modules[moduleIndex].inputCount; index++)
				ngsReleaseBufferReference(inputs[index],
							  &inputs[index]->alias);
		}
	}
}
