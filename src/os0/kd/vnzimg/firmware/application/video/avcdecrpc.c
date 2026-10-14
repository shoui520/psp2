/*
 * The AVC decoder's server thread and RPCs (0-7, 0x82075c-0x8210b4): the ARM's decoder
 * operations, dsp0VideocodecQueryDecoderMemory .. TerminateDecoder. Each maps
 * the 208-byte command; the decoder runs on its own server thread, made
 * by the library thread.
 *
 * The access units come in two buffers used in turn: the current one is
 * opened for a decode, and the previous one closed after it.
 */
#include "video.h"

#define COMMAND_SIZE			208
#define CONTEXT_SIZE			256

/* the AVC decoder's server: one command at a time until VIDEO_COMMAND_EXIT */
void avcdecThread(VideoServer *server, uint32_t argument2)
{
	uint32_t *argument = server->argument;
	uint32_t running = 1;

	vnzThreadSetFlags(server->client, VIDEO_FLAG_DONE);
	do {
		if (!(vnzThreadWaitFlags(VIDEO_FLAG_COMMAND) & VIDEO_FLAG_COMMAND))
			continue;
		switch (server->command) {
		case VIDEO_COMMAND_EXIT:
			if (argument[0])
				avcdecTerminate(argument[0]);
			server->result = 0;
			running = 0;
			break;
		case AVCDEC_QUERY_MEMORY:
			server->result = avcdecQueryMemory((const uint32_t *)argument[0]);
			break;
		case AVCDEC_INITIALIZE:
			server->result = avcdecInitialize((uint32_t *)argument[0],
				(const uint32_t *)argument[1], argument[2], argument[3]);
			break;
		case AVCDEC_QUERY_SEQUENCE:
			server->result = avcdecQuerySequence(argument[0], (VideoAu *)argument[1],
				(const uint32_t *)argument[2], argument[3]);
			break;
		case AVCDEC_QUERY_CONTEXT:
			server->result = avcdecQueryContext((const uint32_t *)argument[0]);
			break;
		case AVCDEC_INITIALIZE_CONTEXT:
			server->result = avcdecInitializeContext(argument[0],
				(const uint32_t *)argument[1], argument[2], argument[3]);
			break;
		case AVCDEC_DECODE:
			server->result = avcdecDecode(argument[0], (VideoAu *)argument[1],
				argument[2], argument[3], (uint32_t *)argument[4]);
			break;
		case AVCDEC_DECODE_STOP:
			server->result = avcdecDecodeStop(argument[0], (VideoAu *)argument[1],
				argument[2], argument[3], (uint32_t *)argument[4]);
			break;
		default:
			server->result = -1;
			break;
		}
		vnzThreadSetFlags(server->client, VIDEO_FLAG_DONE);
	} while (running);
	vnzThreadExit();
}

/* RPC 0: dsp0VideocodecQueryDecoderMemory */
int32_t rpcAvcdecQueryMemory(uint32_t paddr)
{
	VideoDecoderCommand *command;
	VnzMemory memory;
	int32_t result;

	videoInvalidate(paddr, COMMAND_SIZE);
	vnzMemoryCreate(&memory, paddr, COMMAND_SIZE);
	if (!videoMemoryOpenPrivate(&memory)) {
		result = VIDEO_ERROR_MEMORY;
	} else {
		command = (VideoDecoderCommand *)memory.vaddrVnz;
		command->workSize = avcdecQueryMemory(command->param);
		result = 0;
	}
	if (memory.vaddrVnz && !videoMemoryClosePrivate(&memory) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

/* RPC 1: dsp0VideocodecInitializeDecoder, on the library given */
int32_t rpcAvcdecInitialize(uint32_t paddr, uint32_t libraryPaddr)
{
	VnzMemory library, memory, context, work;
	VideoLibraryCommand *libraryCommand;
	VideoDecoderContext *decoder;
	VideoDecoderCommand *command;
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
	command = (VideoDecoderCommand *)memory.vaddrVnz;

	videoInvalidate(command->context, CONTEXT_SIZE);
	vnzMemoryCreate(&context, command->context, CONTEXT_SIZE);
	if (!videoMemoryOpenPublic(&context))
		goto fail;
	command->context = context.vaddrVnz;

	videoInvalidate(command->workAddress, command->workSize);
	vnzMemoryCreate(&work, command->workAddress, command->workSize);
	if (!videoMemoryOpenPublic(&work))
		goto fail;
	command->workAddress = work.vaddrVnz;
	decoder = (VideoDecoderContext *)command->context;

	/* both buffers must map */
	for (i = 0; i < 2; i++) {
		videoInvalidate(command->au[i].buffer, command->au[i].bufferSize);
		vnzMemoryCreate(&decoder->stream[i], command->au[i].buffer,
			command->au[i].bufferSize);
		if (!videoMemoryOpenPublic(&decoder->stream[i]))
			goto fail;
		if (decoder->stream[i].vaddrVnz)
			videoMemoryClosePublic(&decoder->stream[i], 0);
	}

	memcpy(&decoder->au, &command->au[0], sizeof(decoder->au));
	decoder->au.bufferSize *= 2;
	if (!videoMemoryOpenPublic(&decoder->stream[0]))
		goto fail;
	decoder->au.buffer = decoder->stream[0].vaddrVnz;
	if (decoder->au.buffer)
		videoMemoryClosePublic(&decoder->stream[0], 0);
	if (!videoMemoryOpenPublic(&decoder->stream[1]))
		goto fail;

	command->auIndex = 0;
	result = videoServerCall(server, AVCDEC_INITIALIZE, (uint32_t)decoder,
		(uint32_t)&command->handle, (uint32_t)command->param, command->workAddress,
		command->workSize);
	if (result && decoder->stream[1].vaddrVnz &&
	    !videoMemoryClosePublic(&decoder->stream[1], 0) && result >= 0)
		goto fail;
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

/* the ARM's unit, with its stream mapped at address */
static int32_t auMap(VideoAu *au, const VideoAu *from, uint32_t address)
{
	au->es = address + (from->es - from->buffer);
	au->esSize = from->esSize;
	au->advance = from->advance;
	au->pts[0] = from->pts[0];
	au->pts[1] = from->pts[1];
	au->dts[0] = from->dts[0];
	au->dts[1] = from->dts[1];
	au->numOfOutput = from->numOfOutput;
	au->status = from->status;
	return 0;
}

static int32_t auReturn(const VideoAu *au, VideoAu *to)
{
	to->numOfOutput = au->numOfOutput;
	to->status = au->status;
	return 0;
}

/* RPC 2: the sequence the access unit starts, into the parameters */
int32_t rpcAvcdecQuerySequence(uint32_t paddr)
{
	VideoDecoderContext *decoder;
	VideoDecoderCommand *command;
	VnzMemory memory, pictures;
	VnzMemory *stream;
	int32_t result;

	videoInvalidate(paddr, COMMAND_SIZE);
	vnzMemoryCreate(&memory, paddr, COMMAND_SIZE);
	pictures.vaddrVnz = 0;
	if (!videoMemoryOpenPublic(&memory))
		goto fail;
	command = (VideoDecoderCommand *)memory.vaddrVnz;
	decoder = (VideoDecoderContext *)command->context;

	videoInvalidate(command->pictures, command->numOfPictures * sizeof(VideoPicture));
	vnzMemoryCreate(&pictures, command->pictures,
		command->numOfPictures * sizeof(VideoPicture));
	if (!videoMemoryOpenPublic(&pictures))
		goto fail;
	stream = &decoder->stream[command->auIndex];
	if (!videoMemoryOpenPublic(stream))
		goto fail;

	auMap(&decoder->au, &command->au[command->auIndex], stream->vaddrVnz);
	result = videoServerCall(&decoder->server, AVCDEC_QUERY_SEQUENCE, command->handle,
		(uint32_t)&decoder->au, (uint32_t)command->param, pictures.vaddrVnz, 0);
	auReturn(&decoder->au, &command->au[command->auIndex]);
	if (stream->vaddrVnz && !videoMemoryClosePublic(stream, 0) && result >= 0)
		goto fail;
	goto out;
fail:
	result = VIDEO_ERROR_MEMORY;
out:
	if (pictures.vaddrVnz && !videoMemoryClosePublic(&pictures, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	if (memory.vaddrVnz && !videoMemoryClosePublic(&memory, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

/* RPC 3: dsp0VideocodecQueryDecoderContext */
int32_t rpcAvcdecQueryContext(uint32_t paddr)
{
	VideoDecoderCommand *command;
	VnzMemory memory;
	int32_t result;

	videoInvalidate(paddr, COMMAND_SIZE);
	vnzMemoryCreate(&memory, paddr, COMMAND_SIZE);
	if (!videoMemoryOpenPrivate(&memory)) {
		result = VIDEO_ERROR_MEMORY;
	} else {
		command = (VideoDecoderCommand *)memory.vaddrVnz;
		command->contextSize = avcdecQueryContext(command->param);
		result = 0;
	}
	if (memory.vaddrVnz && !videoMemoryClosePrivate(&memory) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

/* RPC 4: dsp0VideocodecInitializeDecoderContext */
int32_t rpcAvcdecInitializeContext(uint32_t paddr)
{
	VideoDecoderContext *decoder;
	VideoDecoderCommand *command;
	VnzMemory memory;
	int32_t result;

	videoInvalidate(paddr, COMMAND_SIZE);
	vnzMemoryCreate(&memory, paddr, COMMAND_SIZE);
	if (!videoMemoryOpenPublic(&memory))
		goto fail;
	command = (VideoDecoderCommand *)memory.vaddrVnz;
	decoder = (VideoDecoderContext *)command->context;

	videoInvalidate(command->contextAddress, command->contextSize);
	vnzMemoryCreate(&decoder->context, command->contextAddress, command->contextSize);
	if (!videoMemoryOpenPublic(&decoder->context))
		goto fail;
	result = videoServerCall(&decoder->server, AVCDEC_INITIALIZE_CONTEXT, command->handle,
		(uint32_t)command->param, decoder->context.vaddrVnz, command->contextSize, 0);
	if (result && decoder->context.vaddrVnz &&
	    !videoMemoryClosePublic(&decoder->context, 0) && result >= 0)
		goto fail;
	goto out;
fail:
	result = VIDEO_ERROR_MEMORY;
out:
	if (memory.vaddrVnz && !videoMemoryClosePublic(&memory, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

static uint32_t previousAu(uint32_t index)
{
	if (!index)
		return 1;
	return index - 1;
}

static int32_t nextAu(VideoDecoderCommand *command)
{
	if (command->auIndex == 1)
		command->auIndex = 0;
	else
		command->auIndex++;
	return 0;
}

/* the pictures' planes, written back for the ARM, as physical addresses */
static int32_t picturesToPhysical(VideoPicture *picture, uint32_t count)
{
	uint32_t i, lumaSize, chromaSize;

	for (i = 0; i < count; i++, picture++) {
		lumaSize = picture->height * picture->pitch;
		chromaSize = (int32_t)(picture->chromaHeight * picture->pitch) / 2;
		if (picture->plane[1]) {
			chromaSize >>= 1;
			lumaSize >>= 1;
		}
		if (picture->plane[0])
			picture->plane[0] = videoPurgeToPhysical(picture->plane[0], lumaSize);
		if (picture->plane[1])
			picture->plane[1] = videoPurgeToPhysical(picture->plane[1], lumaSize);
		if (picture->plane[2])
			picture->plane[2] = videoPurgeToPhysical(picture->plane[2], chromaSize);
		if (picture->plane[3])
			picture->plane[3] = videoPurgeToPhysical(picture->plane[3], chromaSize);
		if (picture->data[0])
			picture->data[0] = videoPurgeToPhysical(picture->data[0], 256);
		if (picture->data[1])
			picture->data[1] = videoPurgeToPhysical(picture->data[1], 256);
		if (picture->data[2])
			picture->data[2] = videoPurgeToPhysical(picture->data[2], 256);
	}
	return 0;
}

/* RPCs 5 and 6: a unit decoded, or the decoder drained, into the pictures */
static int32_t decode(uint32_t paddr, uint32_t operation)
{
	VideoDecoderContext *decoder;
	VideoDecoderCommand *command;
	VnzMemory memory, pictures;
	VnzMemory *stream;
	uint32_t count, index;
	int32_t result;

	videoInvalidate(paddr, COMMAND_SIZE);
	vnzMemoryCreate(&memory, paddr, COMMAND_SIZE);
	pictures.vaddrVnz = 0;
	if (!videoMemoryOpenPublic(&memory))
		goto fail;
	command = (VideoDecoderCommand *)memory.vaddrVnz;
	decoder = (VideoDecoderContext *)command->context;

	count = command->numOfPictures ? command->numOfPictures : 1;
	videoInvalidate(command->pictures, count * sizeof(VideoPicture));
	vnzMemoryCreate(&pictures, command->pictures, count * sizeof(VideoPicture));
	if (!videoMemoryOpenPublic(&pictures))
		goto fail;
	stream = &decoder->stream[command->auIndex];
	if (!videoMemoryOpenPublic(stream))
		goto fail;

	index = command->auIndex;
	auMap(&decoder->au, &command->au[index], stream->vaddrVnz);
	result = videoServerCall(&decoder->server, operation, command->handle,
		(uint32_t)&decoder->au, pictures.vaddrVnz, command->numOfPictures,
		(uint32_t)command->option);
	auReturn(&decoder->au, &command->au[command->auIndex]);

	stream = &decoder->stream[previousAu(command->auIndex)];
	if (stream->vaddrVnz && !videoMemoryClosePublic(stream, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;

	count = command->numOfPictures;
	if (command->au[command->auIndex].numOfOutput <= count)
		count = command->au[command->auIndex].numOfOutput;
	picturesToPhysical((VideoPicture *)pictures.vaddrVnz, count);
	if (command->au[command->auIndex].advance)
		nextAu(command);
	goto out;
fail:
	result = VIDEO_ERROR_MEMORY;
out:
	if (pictures.vaddrVnz && !videoMemoryClosePublic(&pictures, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	if (memory.vaddrVnz && !videoMemoryClosePublic(&memory, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

/* RPC 5: dsp0VideocodecDecodePicture */
int32_t rpcAvcdecDecode(uint32_t paddr)
{
	return decode(paddr, AVCDEC_DECODE);
}

/* RPC 6: dsp0VideocodecStopDecoder, the pictures still held given out */
int32_t rpcAvcdecDecodeStop(uint32_t paddr)
{
	return decode(paddr, AVCDEC_DECODE_STOP);
}

/*
 * RPC 7: dsp0VideocodecTerminateDecoder. As in RPC 98, Sony's code goes on
 * through a stale register when a command cannot be mapped; here it fails.
 */
int32_t rpcAvcdecTerminate(uint32_t paddr, uint32_t libraryPaddr)
{
	VnzMemory library, memory, context, work;
	VideoLibraryCommand *libraryCommand;
	VideoDecoderContext *decoder;
	VideoDecoderCommand *command;
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
	command = (VideoDecoderCommand *)memory.vaddrVnz;
	decoder = (VideoDecoderContext *)command->context;
	result = videoServerCall((VideoServer *)libraryCommand->context, AVCDEC_TERMINATE,
		(uint32_t)decoder, command->handle, 0, 0, 0);
	command->handle = 0;

	for (i = 0; i < 2; i++)
		if (decoder->stream[i].vaddrVnz &&
		    !videoMemoryClosePublic(&decoder->stream[i], 0) && result >= 0)
			result = VIDEO_ERROR_MEMORY;
	if (decoder->context.vaddrVnz && !videoMemoryClosePublic(&decoder->context, 0) &&
	    result >= 0)
		result = VIDEO_ERROR_MEMORY;

	videoInvalidate(command->workAddress, command->workSize);
	vnzMemoryCreate(&work, command->workAddress, command->workSize);
	if (!videoMemoryClosePublic(&work, 1) && result >= 0)
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
