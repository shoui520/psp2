/*
 * The AVC encoder's server thread and RPCs (16-20, 0x8219dc-0x82224c): the
 * ARM's dsp0VideocodecQueryEncoderMemory .. TerminateEncoder on the 404-byte
 * command. The pictures come in two buffers used in turn, as the decoders'
 * access units do; the NAL units made go back as physical ranges.
 */
#include "video.h"

#define COMMAND_SIZE			404
#define CONTEXT_SIZE			256
#define PARAMETER_SETS_SIZE		256
#define MAX_SEI				12

/* the AVC encoder's server: one command at a time until VIDEO_COMMAND_EXIT */
void avcencThread(VideoServer *server, uint32_t argument2)
{
	uint32_t *argument = server->argument;
	VideoEncoderWork *work;
	uint32_t running = 1;

	vnzThreadSetFlags(server->client, VIDEO_FLAG_DONE);
	do {
		if (!(vnzThreadWaitFlags(VIDEO_FLAG_COMMAND) & VIDEO_FLAG_COMMAND))
			continue;
		switch (server->command) {
		case VIDEO_COMMAND_EXIT:
			if (argument[0])
				avcencTerminate(argument[0]);
			server->result = 0;
			running = 0;
			break;
		case AVCENC_QUERY_MEMORY:
			server->result = avcencQueryMemory((const uint32_t *)argument[0]);
			break;
		case AVCENC_QUERY_MEMORY2:
			server->result = avcencQueryMemory2((const uint32_t *)argument[0]);
			break;
		case AVCENC_INITIALIZE:
			work = (VideoEncoderWork *)argument[3];
			server->result = avcencInitialize((uint32_t *)argument[0],
				(const uint32_t *)argument[1], argument[2], work->workSize, work->work2,
				work->work2Size);
			break;
		case AVCENC_GET_PARAMETER_SETS:
			server->result = avcencGetParameterSets(argument[0], argument[1],
				argument[2]);
			break;
		case AVCENC_SET_PARAM:
			server->result = avcencSetParam(argument[0], (const uint32_t *)argument[1]);
			break;
		case AVCENC_ENCODE:
			server->result = avcencEncode(argument[0], (VideoEncoderFrame *)argument[1],
				argument[2], argument[3]);
			break;
		case AVCENC_ENCODE_STOP:
			server->result = avcencEncodeStop(argument[0],
				(VideoEncoderFrame *)argument[1], argument[2], argument[3]);
			break;
		case AVCENC_GET_SEI:
			server->result = avcencGetSei(argument[0], argument[1]);
			break;
		default:
			server->result = -1;
			break;
		}
		vnzThreadSetFlags(server->client, VIDEO_FLAG_DONE);
	} while (running);
	vnzThreadExit();
}

/* RPC 16: dsp0VideocodecQueryEncoderMemory, the three work sizes */
int32_t rpcAvcencQueryMemory(uint32_t paddr)
{
	VideoEncoderCommand *command;
	VnzMemory memory;
	int32_t result;

	videoInvalidate(paddr, COMMAND_SIZE);
	vnzMemoryCreate(&memory, paddr, COMMAND_SIZE);
	if (!videoMemoryOpenPrivate(&memory)) {
		result = VIDEO_ERROR_MEMORY;
	} else {
		command = (VideoEncoderCommand *)memory.vaddrVnz;
		command->workSize = avcencQueryMemory(command->param);
		command->work2Size = avcencQueryMemory2(command->param);
		command->work3Size = avcencQueryMemory3(command->param);
		result = 0;
	}
	if (memory.vaddrVnz && !videoMemoryClosePrivate(&memory) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

static uint32_t frameSize(const VideoEncoderFrame *frame)
{
	return (int32_t)(frame->pitch * frame->height * 3) / 2;
}

/*
 * RPC 17: dsp0VideocodecInitializeEncoder, on the library given; then the
 * parameter sets are made and the encode parameters set.
 */
int32_t rpcAvcencInitialize(uint32_t paddr, uint32_t libraryPaddr)
{
	VnzMemory library, memory, context, sps, pps;
	VideoLibraryCommand *libraryCommand;
	VideoEncoderContext *encoder;
	VideoEncoderCommand *command;
	VnzMemory *frame;
	VideoServer *server;
	int32_t result;
	uint32_t i;

	videoInvalidate(libraryPaddr, 64);
	vnzMemoryCreate(&library, libraryPaddr, 64);
	memory.vaddrVnz = 0;
	if (!videoMemoryOpenPublic(&library))
		goto fail;
	libraryCommand = (VideoLibraryCommand *)library.vaddrVnz;
	server = (VideoServer *)libraryCommand->context;

	videoInvalidate(paddr, COMMAND_SIZE);
	vnzMemoryCreate(&memory, paddr, COMMAND_SIZE);
	if (!videoMemoryOpenPublic(&memory))
		goto fail;
	command = (VideoEncoderCommand *)memory.vaddrVnz;

	videoInvalidate(command->context, CONTEXT_SIZE);
	vnzMemoryCreate(&context, command->context, CONTEXT_SIZE);
	if (!videoMemoryOpenPublic(&context))
		goto fail;
	command->context = context.vaddrVnz;
	videoInvalidate(command->parameterSets, PARAMETER_SETS_SIZE);
	vnzMemoryCreate(&sps, command->parameterSets, PARAMETER_SETS_SIZE);
	if (!videoMemoryOpenPublic(&sps))
		goto fail;
	command->parameterSets = sps.vaddrVnz;
	videoInvalidate(command->parameterSets2, PARAMETER_SETS_SIZE);
	vnzMemoryCreate(&pps, command->parameterSets2, PARAMETER_SETS_SIZE);
	if (!videoMemoryOpenPublic(&pps))
		goto fail;
	command->parameterSets2 = pps.vaddrVnz;

	encoder = (VideoEncoderContext *)command->context;
	command->option = command->option1Present ? (uint32_t)&command->option1Present : 0;
	command->option2 = command->option2Present ? (uint32_t)command->option2Data : 0;

	/* both picture buffers must map */
	for (i = 0; i < 2; i++) {
		frame = &encoder->frameMemory[i];
		videoInvalidate(command->frame[i].luma, frameSize(&command->frame[i]));
		vnzMemoryCreate(frame, command->frame[i].luma, frameSize(&command->frame[i]));
		if (!videoMemoryOpenPublic(frame))
			goto fail;
		if (frame->vaddrVnz)
			videoMemoryClosePublic(frame, 0);
	}

	videoInvalidate(command->workAddress, command->workSize);
	vnzMemoryCreate(&encoder->work, command->workAddress, command->workSize);
	if (!videoMemoryOpenPublic(&encoder->work))
		goto fail;
	videoInvalidate(command->work2Address, command->work2Size);
	vnzMemoryCreate(&encoder->work2Memory, command->work2Address, command->work2Size);
	if (!videoMemoryOpenPublic(&encoder->work2Memory))
		goto fail;

	encoder->workInfo.work2 = encoder->work2Memory.vaddrVnz;
	encoder->workInfo.workSize = command->workSize;
	encoder->workInfo.work2Size = command->work2Size;
	result = videoServerCall(server, AVCENC_INITIALIZE, (uint32_t)encoder,
		(uint32_t)&command->handle, (uint32_t)command->param, encoder->work.vaddrVnz,
		(uint32_t)&encoder->workInfo);
	if (result) {
		if (encoder->work.vaddrVnz && !videoMemoryClosePublic(&encoder->work, 0) &&
		    result >= 0)
			result = VIDEO_ERROR_MEMORY;
		if (encoder->work2Memory.vaddrVnz &&
		    !videoMemoryClosePublic(&encoder->work2Memory, 0) && result >= 0)
			goto fail;
		goto out;
	}

	result = videoServerCall(&encoder->server, AVCENC_GET_PARAMETER_SETS, command->handle,
		command->parameterSets, command->parameterSets2, 0, 0);
	if (result)
		goto out;
	memcpy(&command->sps, (void *)command->parameterSets, sizeof(command->sps));
	command->sps.address = videoPurgeToPhysical(command->sps.address, command->sps.size);
	memcpy(&command->pps, (void *)command->parameterSets2, sizeof(command->pps));
	command->pps.address = videoPurgeToPhysical(command->pps.address, command->pps.size);

	if (!videoMemoryOpenPublic(&encoder->frameMemory[1]))
		goto fail;
	command->frameIndex = 0;
	result = videoServerCall(&encoder->server, AVCENC_SET_PARAM, command->handle,
		(uint32_t)command->encodeParam, 0, 0, 0);
	goto out;
fail:
	result = VIDEO_ERROR_MEMORY;
out:
	if (memory.vaddrVnz && !videoMemoryClosePublic(&memory, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	if (library.vaddrVnz && !videoMemoryClosePublic(&library, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

/* the ARM's picture, with its buffer mapped at address */
static int32_t frameMap(VideoEncoderFrame *frame, const VideoEncoderFrame *from,
	uint32_t address)
{
	memcpy(frame, from, sizeof(*frame));
	frame->luma = address;
	frame->chroma = address + frame->pitch * frame->height;
	return 0;
}

static int32_t frameReturn(const VideoEncoderFrame *frame, VideoEncoderFrame *to)
{
	to->numOfNal = frame->numOfNal;
	return 0;
}

static uint32_t previousFrame(uint32_t index)
{
	if (!index)
		return 1;
	return index - 1;
}

static int32_t nextFrame(VideoEncoderCommand *command)
{
	if (command->frameIndex == 1)
		command->frameIndex = 0;
	else
		command->frameIndex++;
	return 0;
}

static int32_t outputsToPhysical(VideoEncoderOutput *output, uint32_t count)
{
	uint32_t i;

	for (i = 0; i < count; i++, output++)
		output->address = videoPurgeToPhysical(output->address, output->size);
	return 0;
}

/*
 * RPCs 18 and 19: a picture encoded, or the encoder drained, into the
 * outputs; then the SEI units the first output announces are fetched.
 */
static int32_t encode(uint32_t paddr, uint32_t operation)
{
	VideoEncoderContext *encoder;
	VideoEncoderCommand *command;
	VideoEncoderOutput *outputs;
	VideoEncoderBuffer *sei;
	VnzMemory memory, output;
	uint32_t count, index, i;
	VnzMemory *frame;
	int32_t result;

	videoInvalidate(paddr, COMMAND_SIZE);
	vnzMemoryCreate(&memory, paddr, COMMAND_SIZE);
	output.vaddrVnz = 0;
	if (!videoMemoryOpenPublic(&memory))
		goto fail;
	command = (VideoEncoderCommand *)memory.vaddrVnz;
	encoder = (VideoEncoderContext *)command->context;

	count = command->numOfOutputs ? command->numOfOutputs : 1;
	videoInvalidate(command->outputs, count * sizeof(VideoEncoderOutput));
	vnzMemoryCreate(&output, command->outputs, count * sizeof(VideoEncoderOutput));
	if (!videoMemoryOpenPublic(&output))
		goto fail;
	outputs = (VideoEncoderOutput *)output.vaddrVnz;

	if (command->numOfOutputs) {
		result = videoServerCall(&encoder->server, AVCENC_SET_PARAM, command->handle,
			(uint32_t)command->encodeParam, 0, 0, 0);
		if (result)
			goto out;
	}
	if (!videoMemoryOpenPublic(&encoder->frameMemory[command->frameIndex]))
		goto fail;

	index = command->frameIndex;
	frameMap(&encoder->frame[index], &command->frame[index],
		encoder->frameMemory[index].vaddrVnz);
	result = videoServerCall(&encoder->server, operation, command->handle,
		(uint32_t)&encoder->frame[command->frameIndex], (uint32_t)outputs,
		command->numOfOutputs, (uint32_t)command->pictureParam);
	frameReturn(&encoder->frame[command->frameIndex], &command->frame[command->frameIndex]);
	if (result)
		goto out;

	frame = &encoder->frameMemory[previousFrame(command->frameIndex)];
	if (frame->vaddrVnz && !videoMemoryClosePublic(frame, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	count = command->frame[command->frameIndex].numOfNal;
	outputsToPhysical(outputs, count);
	nextFrame(command);
	if (result || !count)
		goto out;

	count = outputs[0].numOfSei;
	if (count > MAX_SEI)
		count = MAX_SEI;
	for (i = 0; i < count; i++) {
		result = videoServerCall(&encoder->server, AVCENC_GET_SEI, command->handle,
			command->parameterSets, 0, 0, 0);
		if (result)
			goto out;
		sei = (VideoEncoderBuffer *)command->parameterSets;
		sei->address = videoPurgeToPhysical(sei->address, sei->size);
		memcpy(&command->sei[i], sei, sizeof(command->sei[i]));
	}
	goto out;
fail:
	result = VIDEO_ERROR_MEMORY;
out:
	if (output.vaddrVnz && !videoMemoryClosePublic(&output, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	if (memory.vaddrVnz && !videoMemoryClosePublic(&memory, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

/* RPC 18: dsp0VideocodecEncodePicture */
int32_t rpcAvcencEncode(uint32_t paddr)
{
	return encode(paddr, AVCENC_ENCODE);
}

/* RPC 19: dsp0VideocodecStopEncoder, the pictures still held encoded */
int32_t rpcAvcencEncodeStop(uint32_t paddr)
{
	return encode(paddr, AVCENC_ENCODE_STOP);
}

/*
 * RPC 20: dsp0VideocodecTerminateEncoder. Sony's code goes on through a
 * stale register when a command cannot be mapped; here it fails.
 */
int32_t rpcAvcencTerminate(uint32_t paddr, uint32_t libraryPaddr)
{
	VnzMemory library, memory, context, sps, pps;
	VideoLibraryCommand *libraryCommand;
	VideoEncoderContext *encoder;
	VideoEncoderCommand *command;
	int32_t result;
	uint32_t i;

	videoInvalidate(libraryPaddr, 64);
	vnzMemoryCreate(&library, libraryPaddr, 64);
	memory.vaddrVnz = 0;
	if (!videoMemoryOpenPublic(&library)) {
		result = VIDEO_ERROR_MEMORY;
		goto out;
	}
	libraryCommand = (VideoLibraryCommand *)library.vaddrVnz;

	videoInvalidate(paddr, COMMAND_SIZE);
	vnzMemoryCreate(&memory, paddr, COMMAND_SIZE);
	if (!videoMemoryOpenPublic(&memory)) {
		result = VIDEO_ERROR_MEMORY;
		goto out;
	}
	command = (VideoEncoderCommand *)memory.vaddrVnz;
	encoder = (VideoEncoderContext *)command->context;
	result = videoServerCall((VideoServer *)libraryCommand->context, AVCENC_TERMINATE,
		(uint32_t)encoder, command->handle, 0, 0, 0);
	command->handle = 0;

	for (i = 0; i < 2; i++)
		if (encoder->frameMemory[i].vaddrVnz &&
		    !videoMemoryClosePublic(&encoder->frameMemory[i], 0) && result >= 0)
			result = VIDEO_ERROR_MEMORY;
	if (encoder->work.vaddrVnz && !videoMemoryClosePublic(&encoder->work, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	if (encoder->work2Memory.vaddrVnz &&
	    !videoMemoryClosePublic(&encoder->work2Memory, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;

	videoInvalidate(command->parameterSets2, PARAMETER_SETS_SIZE);
	vnzMemoryCreate(&pps, command->parameterSets2, PARAMETER_SETS_SIZE);
	if (!videoMemoryClosePublic(&pps, 1) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	videoInvalidate(command->parameterSets, PARAMETER_SETS_SIZE);
	vnzMemoryCreate(&sps, command->parameterSets, PARAMETER_SETS_SIZE);
	if (!videoMemoryClosePublic(&sps, 1) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	videoInvalidate(command->context, CONTEXT_SIZE);
	vnzMemoryCreate(&context, command->context, CONTEXT_SIZE);
	if (!videoMemoryClosePublic(&context, 1) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
out:
	if (memory.vaddrVnz && !videoMemoryClosePublic(&memory, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	if (library.vaddrVnz && !videoMemoryClosePublic(&library, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}
