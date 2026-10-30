/* Sony source: videocodec/ce_dsp0_videocodec.c. */
#include "../audiodec/audiodec_internal.h"
#include "videocodec_internal.h"
#include <sysmem/debug.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/cpu.h>
#include <string.h>

int dsp0VideocodecLocalStorageId = -1;
char dsp0VideocodecDecoderMutexName[24] = "SceDsp0VcodecDec0";
char dsp0VideocodecEncoderMutexName[24] = "SceDsp0VcodecEnc0";

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, dsp0VideocodecCreateStorage, 415,
		   "_sceDsp0VideocodecProcessCreateEntry");
SCE_DBGINFO_DEFINE(static, dsp0VideocodecExitStorage, 436,
		   "_sceDsp0VideocodecProcessExitEntry");
SCE_DBGINFO_DEFINE(static, dsp0VideocodecGlobalStorage, 319,
		   "_dsp0GetDsp0VideocodecGlobal");
#endif

/* RPC addresses are virtual on input and physical after packing. */
typedef struct Dsp0VideocodecRpcBuffers {
	uintptr_t bufferAddress;
	uintptr_t optionalBufferAddress;
	SceSize bufferSize;
	SceSize optionalBufferSize;
} Dsp0VideocodecRpcBuffers;

_Static_assert(sizeof(Dsp0VideocodecRpcBuffers) == 16, "DSP0 RPC buffers size");

int dsp0VideocodecCalculateLibraryMemory(const Dsp0VideocodecInitParam *param,
				       SceSize *memorySize, SceSize *librarySize);
int dsp0VideocodecReleaseLibraryBuffers(Dsp0VideocodecGlobal *global);


void dsp0VideocodecFinishCommand(VeneziaRpcThunk *thunk)
{
	sceCodecEngineDsp0ConvertPhysicalToVirtual(thunk);
	sceCodecEngineDsp0MemcpyChain(thunk);
	sceCodecEngineDsp0FreeCommunicationMemories(thunk);
}

/* The RPC pack ABI serializes Dsp0VideocodecRpcBuffers as four words. */
int dsp0VideocodecPackCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
			    unsigned int *packed)
{
	VeneziaMemoryOption option;
	SceSize optionalSize;
	int ret;

	option.flags = 1;
	option.allocationSize = input[2];
	optionalSize = input[3];
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, (void *)input[0],
					      option.allocationSize, &option);
	if (optionalSize != 0) {
		option.allocationSize = optionalSize;
		sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, (void *)input[1],
						      optionalSize, &option);
		sceCodecEngineDsp0AllocCommunicationMemories(thunk);
		ret = sceCodecEngineDsp0MemcpyChain(thunk);
		packed[0] = thunk->memories[0].physicalBase;
		packed[1] = thunk->memories[1].physicalBase;
	} else {
		sceCodecEngineDsp0AllocCommunicationMemories(thunk);
		ret = sceCodecEngineDsp0MemcpyChain(thunk);
		packed[0] = thunk->memories[0].physicalBase;
		packed[1] = input[1];
	}
	packed[2] = input[2];
	packed[3] = input[3];
	return ret;
}

static inline uintptr_t dsp0VideocodecToPhysical(uintptr_t address)
{
	uintptr_t physical;

	if (address != 0) {
		if (sceKernelVAtoPA((const void *)address, &physical) < 0)
			address = 0;
		else
			address = physical;
	}
	return address;
}

/* The coprocessor reads the command's buffers by physical address. */
int dsp0VideocodecCallDecoderCommand(Dsp0VideocodecDecoderCommand *command,
				     unsigned int operation, const unsigned int *input)
{
	uintptr_t au0Buffer = command->au[0].bufferAddress;
	uintptr_t au0Es, au1Buffer, au1Es, frameBuf;
	Dsp0VideocodecPicture *pPicture;
	int ret;

	command->au[0].bufferAddress = dsp0VideocodecToPhysical(au0Buffer);
	au0Es = command->au[0].esAddress;
	command->au[0].esAddress = dsp0VideocodecToPhysical(au0Es);
	au1Buffer = command->au[1].bufferAddress;
	command->au[1].bufferAddress = dsp0VideocodecToPhysical(au1Buffer);
	au1Es = command->au[1].esAddress;
	command->au[1].esAddress = dsp0VideocodecToPhysical(au1Es);
	pPicture = command->pPicture;
	command->pPicture = (Dsp0VideocodecPicture *)
		dsp0VideocodecToPhysical((uintptr_t)pPicture);
	frameBuf = command->frameBufAddress;
	command->frameBufAddress = dsp0VideocodecToPhysical(frameBuf);
	ret = sceVeneziaRpcCallGenericThunk(operation, input,
			dsp0VideocodecPackCommand, dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620380;
	else if (ret < 0)
		ret = (int)0x806203c0;
	else if (ret != 0)
		ret = ret == 0x2009 ? (int)0x80620380 : 0;
	command->au[0].bufferAddress = au0Buffer;
	command->au[0].esAddress = au0Es;
	command->au[1].bufferAddress = au1Buffer;
	command->au[1].esAddress = au1Es;
	command->pPicture = pPicture;
	command->frameBufAddress = frameBuf;
	if (pPicture != NULL) {
		if (pPicture->planeAddress[0] != 0)
			pPicture->planeAddress[0] = (uintptr_t)
				SceCodecEngineWrapperForDriver_F9495589(pPicture->planeAddress[0]);
		if (pPicture->planeAddress[1] != 0)
			pPicture->planeAddress[1] = (uintptr_t)
				SceCodecEngineWrapperForDriver_F9495589(pPicture->planeAddress[1]);
		if (pPicture->planeAddress[2] != 0)
			pPicture->planeAddress[2] = (uintptr_t)
				SceCodecEngineWrapperForDriver_F9495589(pPicture->planeAddress[2]);
	}
	return ret;
}

/* the coprocessor's errors 0x10008012..0x10008015 */
static const int dsp0VideocodecEncoderErrors[4] = {
	(int)0x80620481, (int)0x80620482, (int)0x80620483, (int)0x80620483,
};

/* The stream's in-buffer pointers move with the buffer between address spaces. */
int dsp0VideocodecCallEncoderCommand(Dsp0VideocodecEncoderCommand *command,
				     unsigned int operation, const unsigned int *input)
{
	uintptr_t stream = command->streamAddress;
	SceSize streamSize = command->streamSize;
	uintptr_t physicalStream, buffer, address;
	uintptr_t picture[2];
	Dsp0VideocodecEncodeResult *pResult, *result;
	unsigned int numOfNal, i;
	int mapped, ret;

	command->streamAddress = dsp0VideocodecToPhysical(stream);
	buffer = command->bufferAddress;
	command->bufferAddress = dsp0VideocodecToPhysical(buffer);
	for (i = 0; i < 2; i++) {
		picture[i] = command->picture[i].address;
		command->picture[i].address = dsp0VideocodecToPhysical(picture[i]);
	}
	physicalStream = command->streamAddress;
	if (physicalStream != 0 && stream != 0) {
		address = command->streamWriteAddress;
		if (address != 0 && stream <= address && address < streamSize + stream)
			address = physicalStream - stream + address;
		else
			address = 0;
		command->streamWriteAddress = address;
		address = command->streamMarkAddress;
		if (address != 0 && stream <= address && address < streamSize + stream)
			address = physicalStream - stream + address;
		else
			address = 0;
		command->streamMarkAddress = address;
	} else {
		command->streamWriteAddress = 0;
		command->streamMarkAddress = 0;
	}
	if (command->pEncodeResult != NULL)
		command->pEncodeResult->streamAddress = 0;
	result = command->pEncodeResult;
	command->pEncodeResult = (Dsp0VideocodecEncodeResult *)
		dsp0VideocodecToPhysical((uintptr_t)result);
	ret = sceVeneziaRpcCallGenericThunk(operation, input,
			dsp0VideocodecPackCommand, dsp0VideocodecFinishCommand);
	if (ret & 0x10000000) {
		if ((unsigned int)(ret - 0x10008012) > 3)
			ret = (int)0x80620480;
		else
			ret = dsp0VideocodecEncoderErrors[ret - 0x10008012];
	} else if (ret < 0) {
		ret = (int)0x806204c0;
	} else if (ret != 0) {
		ret &= 0x10000000;
	}
	physicalStream = command->streamAddress;
	streamSize = command->streamSize;
	mapped = physicalStream != 0 && stream != 0;
	command->streamAddress = stream;
	command->bufferAddress = buffer;
	command->picture[0].address = picture[0];
	command->picture[1].address = picture[1];
	if (mapped) {
		address = command->streamWriteAddress;
		if (address != 0 && physicalStream <= address &&
		    address < physicalStream + streamSize)
			address = address + stream - physicalStream;
		else
			address = 0;
		command->streamWriteAddress = address;
		address = command->streamMarkAddress;
		if (address != 0 && physicalStream <= address &&
		    address < physicalStream + streamSize)
			address = address + stream - physicalStream;
		else
			address = 0;
	} else {
		command->streamWriteAddress = 0;
		address = 0;
	}
	command->streamMarkAddress = address;
	command->pEncodeResult = result;
	if (command->pEncodeResult != NULL &&
	    command->pEncodeResult->streamAddress != 0) {
		pResult = command->pEncodeResult;
		if (mapped) {
			address = pResult->streamAddress;
			if (address != 0 && physicalStream <= address &&
			    address < physicalStream + streamSize)
				address = stream + address - physicalStream;
			else
				address = 0;
			pResult->streamAddress = address;
			numOfNal = command->pEncodeResult->numOfNal;
			if (numOfNal >= 12)
				numOfNal = 12;
			for (i = 0; i != numOfNal; i++) {
				address = command->nal[i].address;
				if (address != 0 && physicalStream <= address &&
				    address < physicalStream + streamSize)
					address = address + stream - physicalStream;
				else
					address = 0;
				command->nal[i].address = address;
			}
		} else {
			pResult->streamAddress = 0;
			numOfNal = command->pEncodeResult->numOfNal;
			if (numOfNal >= 12)
				numOfNal = 12;
			for (i = 0; i < numOfNal; i++)
				command->nal[i].address = 0;
		}
	}
	return ret;
}

void *dsp0VideocodecAllocMemory(unsigned int argument, SceSize size)
{
	uintptr_t physical;

	physical = veneziaBlockingRpc(100, argument, size);
	return SceCodecEngineWrapperForDriver_F9495589(physical);
}

__attribute__((noinline)) int dsp0VideocodecInitializeMemory(void *memory)
{
	Dsp0VideocodecRpcBuffers buffers;
	int ret;

	buffers.bufferAddress = (uintptr_t)memory;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	ret = sceVeneziaRpcCallGenericThunk(99,
			(const unsigned int *)&buffers, dsp0VideocodecPackCommand,
			dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620280;
	else if (ret < 0)
		ret = (int)0x806202c0;
	else if (ret != 0)
		ret &= 0x10000000;
	return ret;
}

__attribute__((noinline)) int dsp0VideocodecQueryMemory(void *command)
{
	Dsp0VideocodecRpcBuffers buffers;
	int ret;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	ret = sceVeneziaRpcCallGenericThunk(96,
			(const unsigned int *)&buffers, dsp0VideocodecPackCommand,
			dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620280;
	else if (ret < 0)
		ret = (int)0x806202c0;
	else if (ret != 0)
		ret &= 0x10000000;
	return ret;
}

__attribute__((noinline)) int dsp0VideocodecCreateDecoder(void *command)
{
	Dsp0VideocodecRpcBuffers buffers;
	int ret;

	buffers.bufferAddress = (uintptr_t)command;
	/* With zero optional size, the packer forwards this channel unchanged. */
	buffers.optionalBufferAddress = sceCodecEngineDsp0GetThreadExecGroup(0);
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	ret = sceVeneziaRpcCallGenericThunk(97,
			(const unsigned int *)&buffers, dsp0VideocodecPackCommand,
			dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620280;
	else if (ret < 0)
		ret = (int)0x806202c0;
	else if (ret != 0)
		ret &= 0x10000000;
	return ret;
}

__attribute__((noinline)) int dsp0VideocodecDeleteDecoder(void *command)
{
	Dsp0VideocodecRpcBuffers buffers;
	int ret;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	ret = sceVeneziaRpcCallGenericThunk(98,
			(const unsigned int *)&buffers, dsp0VideocodecPackCommand,
			dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620280;
	else if (ret < 0)
		ret = (int)0x806202c0;
	else if (ret != 0)
		ret &= 0x10000000;
	return ret;
}

__attribute__((noinline)) int dsp0VideocodecQueryDecoderMemory(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 0,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecInitializeDecoder(struct Dsp0VideocodecDecoderCommand *command,
				   struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 256;
	buffers.bufferAddress = (uintptr_t)command;
	return dsp0VideocodecCallDecoderCommand(command, 1,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecQueryDecoderContext(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 3,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecInitializeDecoderContext(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 4,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecDecodePicture(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 5,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecStopDecoder(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 6,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecTerminateDecoder(struct Dsp0VideocodecDecoderCommand *command,
				  struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 256;
	buffers.bufferAddress = (uintptr_t)command;
	return dsp0VideocodecCallDecoderCommand(command, 7,
					      (const unsigned int *)&buffers);
}

static inline int dsp0VideocodecCallM4vDecoderCommand(Dsp0VideocodecM4vDecoderCommand *command,
						     unsigned int operation,
						     const Dsp0VideocodecRpcBuffers *buffers)
{
	uintptr_t buffer0 = command->au[0].bufferAddress;
	uintptr_t es0, buffer1, es1, picture, frameBuf;
	int ret;

	command->au[0].bufferAddress = dsp0VideocodecToPhysical(buffer0);
	es0 = command->au[0].esAddress;
	command->au[0].esAddress = dsp0VideocodecToPhysical(es0);
	buffer1 = command->au[1].bufferAddress;
	command->au[1].bufferAddress = dsp0VideocodecToPhysical(buffer1);
	es1 = command->au[1].esAddress;
	command->au[1].esAddress = dsp0VideocodecToPhysical(es1);
	picture = (uintptr_t)command->pPicture;
	command->pPicture = (Dsp0VideocodecPicture *)dsp0VideocodecToPhysical(picture);
	frameBuf = command->frameBufAddress;
	command->frameBufAddress = dsp0VideocodecToPhysical(frameBuf);
	ret = sceVeneziaRpcCallGenericThunk(operation, (const unsigned int *)buffers,
			dsp0VideocodecPackCommand, dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620480;
	else if (ret < 0)
		ret = (int)0x806204c0;
	else if (ret != 0)
		ret &= 0x10000000;
	command->au[0].bufferAddress = buffer0;
	command->au[0].esAddress = es0;
	command->au[1].bufferAddress = buffer1;
	command->au[1].esAddress = es1;
	command->pPicture = (Dsp0VideocodecPicture *)picture;
	command->frameBufAddress = frameBuf;
	return ret;
}

__attribute__((noinline)) int dsp0VideocodecQueryM4vDecoderMemory(Dsp0VideocodecM4vDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallM4vDecoderCommand(command, 32, &buffers);
}

__attribute__((noinline)) int dsp0VideocodecInitializeM4vDecoder(Dsp0VideocodecM4vDecoderCommand *command,
		struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 256;
	return dsp0VideocodecCallM4vDecoderCommand(command, 33, &buffers);
}

__attribute__((noinline)) int dsp0VideocodecQueryM4vDecoderContext(Dsp0VideocodecM4vDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallM4vDecoderCommand(command, 35, &buffers);
}

__attribute__((noinline)) int dsp0VideocodecInitializeM4vDecoderContext(Dsp0VideocodecM4vDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallM4vDecoderCommand(command, 36, &buffers);
}

__attribute__((noinline)) int dsp0VideocodecDecodeM4vPicture(Dsp0VideocodecM4vDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallM4vDecoderCommand(command, 37, &buffers);
}

__attribute__((noinline)) int dsp0VideocodecStopM4vDecoder(Dsp0VideocodecM4vDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallM4vDecoderCommand(command, 38, &buffers);
}

__attribute__((noinline)) int dsp0VideocodecTerminateM4vDecoder(Dsp0VideocodecM4vDecoderCommand *command,
		struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 256;
	return dsp0VideocodecCallM4vDecoderCommand(command, 39, &buffers);
}

__attribute__((noinline)) int dsp0VideocodecQueryEncoderMemory(struct Dsp0VideocodecEncoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallEncoderCommand(command, 16,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecInitializeEncoder(struct Dsp0VideocodecEncoderCommand *command,
				   struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 256;
	buffers.bufferAddress = (uintptr_t)command;
	return dsp0VideocodecCallEncoderCommand(command, 17,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecEncodePicture(struct Dsp0VideocodecEncoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallEncoderCommand(command, 18,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecStopEncoder(struct Dsp0VideocodecEncoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallEncoderCommand(command, 19,
					      (const unsigned int *)&buffers);
}

__attribute__((noinline)) int dsp0VideocodecTerminateEncoder(struct Dsp0VideocodecEncoderCommand *command,
				  struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 256;
	buffers.bufferAddress = (uintptr_t)command;
	return dsp0VideocodecCallEncoderCommand(command, 20,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecReleaseLibraryBuffers(Dsp0VideocodecGlobal *global)
{
	Dsp0VideocodecLibraryCommand *library = global->library;
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecEncoder *encoder;
	int i;

	if (library == NULL)
		return 0;
	encoder = global->encoders;
	if (encoder != NULL) {
		for (i = 0; i < library->numOfEncoders; i++, encoder++) {
			if (encoder->pEncodeResult != NULL)
				encoder->pEncodeResult = NULL;
			if (encoder->coprocessorBufferAddress[1] != 0)
				encoder->coprocessorBufferAddress[1] = 0;
			if (encoder->coprocessorBufferAddress[0] != 0)
				encoder->coprocessorBufferAddress[0] = 0;
			if (encoder->contextAddress != 0)
				encoder->contextAddress = 0;
			if (encoder->pCommand != NULL)
				encoder->pCommand = NULL;
			if (encoder->pMutex != NULL) {
				sceKernelFinalizeFastMutex(encoder->pMutex);
				encoder->pMutex = NULL;
			}
		}
		if (global->encoders != NULL)
			global->encoders = NULL;
	}
	decoder = global->decoders;
	if (decoder != NULL) {
		for (i = 0; i < library->numOfDecoders; i++, decoder++) {
			if (decoder->workAddress != 0)
				decoder->workAddress = 0;
			if (decoder->au[1].address != 0)
				decoder->au[1].address = 0;
			if (decoder->au[0].address != 0)
				decoder->au[0].address = 0;
			if (decoder->pPicture != NULL)
				decoder->pPicture = NULL;
			if (decoder->contextAddress != 0)
				decoder->contextAddress = 0;
			if (decoder->pCommand != NULL)
				decoder->pCommand = NULL;
			if (decoder->pMutex != NULL) {
				sceKernelFinalizeFastMutex(decoder->pMutex);
				decoder->pMutex = NULL;
			}
		}
		if (global->decoders != NULL)
			global->decoders = NULL;
	}
	if (library->workAddress != 0)
		library->workAddress = 0;
	return 0;
}

int dsp0VideocodecQueryEncoderMemorySize(Dsp0VideocodecEncoderCommand *command,
		const Dsp0VideocodecEncoderInitParam *param, SceSize *streamSize,
		SceSize *bufferSize, SceSize *workSize)
{
	unsigned char profile = param->profile, mainProfile;
	int ret;

	if (profile != 'N' && profile != 'B')
		return (int)0x80620201;
	if (profile == 'B') {
		mainProfile = 0;
		profile = 'B';
	} else {
		mainProfile = 1;
		profile = 'M';
	}
	command->mainProfile = mainProfile;
	command->profile = profile;
	command->level = param->level;
	command->horizontal = param->horizontal;
	command->vertical = param->vertical;
	command->fixedMode0 = 0;
	command->userOption0 = param->userOption[0];
	command->fixedMode1 = 0;
	command->fixedMode2 = 1;
	command->userOption1 = param->userOption[1];
	command->rateControl[0] = param->rateControl[0];
	command->rateControl[1] = param->rateControl[1];
	command->rateControl[2] = param->rateControl[2];
	command->rateControl[3] = param->rateControl[3];
	command->codingOption[0] = param->codingOption[0];
	command->coprocessorCounters[0] = 0;
	command->coprocessorCounters[1] = 0;
	command->coprocessorCounters[2] = 0;
	command->sliceOption = param->sliceOption;
	command->sliceParam[0] = param->sliceParam[0];
	command->sliceParam[1] = param->sliceParam[1];
	command->codingOption[1] = param->codingOption[1];
	command->codingOption[2] = param->codingOption[2];
	if (command->codingOption[1] != 0)
		command->sliceOption = 0;
	if (param->vuiPresent != 0 && param->codingOption[1] == 0) {
		command->vuiPresent = 1;
		command->vui[0] = param->vui[0];
		command->vui[1] = param->vui[1];
		command->vui[2] = param->vui[2];
		command->vui[3] = param->vui[3];
		command->vui[4] = param->vui[4];
		command->vui[5] = param->vui[5];
		command->vuiReserved[0] = 0;
		command->vuiReserved[1] = 0;
	} else {
		command->vuiPresent = 0;
		command->vui[0] = 0;
		command->vui[1] = 0;
		command->vui[2] = 0;
		command->vui[3] = 0;
		command->vui[4] = 0;
		command->vui[5] = 0;
		command->vuiReserved[0] = 0;
		command->vuiReserved[1] = 0;
	}
	ret = dsp0VideocodecQueryEncoderMemory(command);
	if (ret < 0)
		return ret;
	*streamSize = (command->streamSize + 255) & ~255;
	*bufferSize = (command->bufferSize + 255) & ~255;
	*workSize = (command->workSize + 255) & ~255;
	return 0;
}

int dsp0VideocodecCheckEncoderInitParam(const Dsp0VideocodecEncoderInitParam *param)
{
	if (param->horizontal - 64 > 1920 - 64)
		return (int)0x80620201;
	if (param->vertical - 64 > 1088 - 64)
		return (int)0x80620201;
	if ((param->horizontal | param->vertical) & 1)
		return (int)0x80620201;
	if (param->pitch - 64 > 1920 - 64 || (param->pitch & 15) != 0)
		return (int)0x80620201;
	if (param->alignedHorizontal - 64 > 1920 - 64)
		return (int)0x80620201;
	if (param->alignedVertical - 64 > 1088 - 64)
		return (int)0x80620201;
	if (((param->alignedHorizontal | param->alignedVertical) & 15) != 0 ||
	    param->horizontal > param->pitch ||
	    param->horizontal > param->alignedHorizontal ||
	    param->vertical > param->alignedVertical ||
	    param->pitch < param->alignedHorizontal)
		return (int)0x80620201;
	if (param->numOfStreams != 1)
		return (int)0x80620201;
	return 0;
}

int dsp0VideocodecCalculateLibraryMemory(const Dsp0VideocodecInitParam *param,
				       SceSize *memorySize, SceSize *librarySize)
{
	Dsp0VideocodecLibraryCommand *command;
	unsigned int pictureSize, decoderMemory;
	int ret;

	command = dsp0VideocodecAllocMemory(0, sizeof(*command));
	if (command == NULL)
		return (int)0x80620203;
	memset(command, 0, sizeof(*command));
	command->commandSize = sizeof(*command);
	command->numOfDecoders = param->numOfStreams;
	command->horizontal = param->horizontal;
	command->numOfEncoders = 1;
	command->coprocessorQueryData[0] = 0;
	command->coprocessorQueryData[1] = 0;
	ret = dsp0VideocodecQueryMemory(command);
	if (ret < 0) {
		ret = (int)0x8062020d;
	} else {
		pictureSize = (((3 * param->vertical * param->horizontal) >> 2) +
			       255) & 0x7fffff00;
		*librarySize = (command->workSize + 255) & ~255u;
		decoderMemory = pictureSize < 81920 ? 92544 : pictureSize + 10624;
		*memorySize = (((command->workSize + 255) & ~255u) +
			1536 * command->numOfEncoders + 262911 +
			((188 * command->numOfEncoders + 255) & ~255u) +
			((116 * command->numOfDecoders + 255) & ~255u) +
			decoderMemory * (2 * command->numOfDecoders)) >> 18 << 18;
	}
	SceCodecEngineWrapperForDriver_3C5E9CC3(0, command, sizeof(*command));
	return ret;
}

int initializeDsp0Videocodec(void)
{
	int ret = sceKernelRegisterKPLS("SceDsp0Videodec",
						    sizeof(Dsp0VideocodecGlobal));

	if (ret >= 0) {
		void *storage = NULL;

		dsp0VideocodecLocalStorageId = ret;
		ret = sceKernelGetRemoteKPLS(0, ret, &storage, 1);
		if (ret >= 0) {
			Dsp0VideocodecGlobal *global;
			struct ThreadmgrFastMutex *mutex;

#if FW_DEBUG
			sceKernelAssert(storage != NULL, &dsp0VideocodecCreateStorage,
					 __builtin_return_address(0));
#endif
			global = storage;
			mutex = &global->mutex;
			memset(storage, 0, sizeof(*global));
			ret = sceKernelInitializeFastMutex((SceKernelFastMutex *)mutex,
						   "SceDsp0Vcodec", 2, NULL);
			if (ret < 0) {
				SceKernelIntrStatus intr;

				if (global->pMutex != NULL) {
					sceKernelFinalizeFastMutex(
						(SceKernelFastMutex *)global->pMutex);
					global->pMutex = NULL;
				}
				intr = sceKernelSpinlockLowLockCpuSuspendIntr(
					&global->statusLock);
				global->status = 0;
				sceKernelSpinlockLowUnlockCpuResumeIntr(
					&global->statusLock, intr);
			} else {
				global->pMutex = mutex;
				global->statusLock = 0;
				global->status = 0;
			}
		}
	}
	return ret;
}

int _sceDsp0VideocodecProcessCreateEntry(SceUID pid, SceUInt32 type)
{
	void *storage = NULL;
	int ret = sceKernelGetRemoteKPLS(pid,
					 dsp0VideocodecLocalStorageId, &storage, 1);

	if (ret >= 0) {
		Dsp0VideocodecGlobal *global;
		struct ThreadmgrFastMutex *mutex;

#if FW_DEBUG
		sceKernelAssert(storage != NULL, &dsp0VideocodecCreateStorage,
				 __builtin_return_address(0));
#endif
		global = storage;
		mutex = &global->mutex;
		memset(storage, 0, sizeof(*global));
		ret = sceKernelInitializeFastMutex((SceKernelFastMutex *)mutex,
						   "SceDsp0Vcodec", 2, NULL);
		if (ret < 0) {
			SceKernelIntrStatus intr;

			if (global->pMutex != NULL) {
				sceKernelFinalizeFastMutex((SceKernelFastMutex *)global->pMutex);
				global->pMutex = NULL;
			}
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&global->statusLock);
			global->status = 0;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
		} else {
			global->pMutex = mutex;
			global->statusLock = 0;
			global->status = 0;
		}
	}
	return ret;
}

int _sceDsp0VideocodecProcessExitEntry(SceUID pid, SceUInt32 type)
{
	void *storage = NULL;
	int ret = sceKernelGetRemoteKPLS(pid,
					 dsp0VideocodecLocalStorageId, &storage, 0);

	if (ret >= 0) {
		Dsp0VideocodecGlobal *global;
		SceKernelIntrStatus intr;

#if FW_DEBUG
		sceKernelAssert(storage != NULL, &dsp0VideocodecExitStorage,
				 __builtin_return_address(0));
#endif
		global = storage;
		if (global->pMutex != NULL) {
			sceKernelFinalizeFastMutex((SceKernelFastMutex *)global->pMutex);
			global->pMutex = NULL;
		}
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&global->statusLock);
		global->status = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
		ret = 0;
	}
	return ret;
}

int dsp0VideocodecIsProcessActive(SceUID pid)
{
	void *storage = NULL;
	Dsp0VideocodecGlobal *global;
	SceKernelIntrStatus intr;
	int status;

	if (pid != 0) {
		if (sceKernelGetRemoteKPLS(pid,
				dsp0VideocodecLocalStorageId, &storage, 0) < 0)
			storage = NULL;
	} else {
		storage = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
	}
#if FW_DEBUG
	sceKernelAssert(storage != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	global = storage;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&global->statusLock);
	status = global->status;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
	return status == 1;
}

int dsp0VideocodecIsProcessSuspended(SceUID pid)
{
	void *storage = NULL;
	Dsp0VideocodecGlobal *global;
	SceKernelIntrStatus intr;
	int status;
	int suspended = 1;

	if (pid != 0) {
		if (sceKernelGetRemoteKPLS(pid,
				dsp0VideocodecLocalStorageId, &storage, 0) < 0)
			storage = NULL;
	} else {
		storage = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
	}
#if FW_DEBUG
	sceKernelAssert(storage != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	global = storage;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&global->statusLock);
	status = global->status;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
	if (status != 0) {
		storage = NULL;
		if (pid != 0) {
			if (sceKernelGetRemoteKPLS(pid,
					dsp0VideocodecLocalStorageId, &storage, 0) < 0)
				storage = NULL;
		} else {
			storage = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
		}
#if FW_DEBUG
		sceKernelAssert(storage != NULL, &dsp0VideocodecGlobalStorage,
				 __builtin_return_address(0));
#endif
		global = storage;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&global->statusLock);
		status = global->status;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
		suspended = status == 3;
	}
	return suspended;
}

int dsp0VideocodecProcessResume(SceUID pid)
{
	void *storage = NULL;
	Dsp0VideocodecGlobal *global;
	SceKernelIntrStatus intr;
	int status;

	if (pid != 0) {
		if (sceKernelGetRemoteKPLS(pid,
				dsp0VideocodecLocalStorageId, &storage, 0) < 0)
			storage = NULL;
	} else {
		storage = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
	}
#if FW_DEBUG
	sceKernelAssert(storage != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	global = storage;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&global->statusLock);
	status = global->status;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
	if (status == 3) {
		storage = NULL;
		if (pid != 0) {
			if (sceKernelGetRemoteKPLS(pid,
					dsp0VideocodecLocalStorageId, &storage, 0) < 0)
				storage = NULL;
		} else {
			storage = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
		}
#if FW_DEBUG
		sceKernelAssert(storage != NULL, &dsp0VideocodecGlobalStorage,
				 __builtin_return_address(0));
#endif
		global = storage;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&global->statusLock);
		global->status = 1;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
	}
	return 0;
}

int dsp0VideocodecQueryLibraryMemory(const Dsp0VideocodecInitParam *param,
				   SceSize *memorySize)
{
	Dsp0VideocodecGlobal *global;
	SceSize requiredSize;
	SceSize librarySize;
	int ret;

	global = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	sceKernelAssert(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	if (param == NULL || memorySize == NULL)
		return (int)0x80620209;
	if (global->initialized != 0)
		return (int)0x80620208;
	if (param->horizontal - 64 > 1216 || param->vertical - 64 > 656 ||
	    ((param->horizontal | param->vertical) & 15) != 0 ||
	    param->numOfRefFrames - 1 > 15 || param->numOfStreams - 1 > 3)
		return (int)0x80620201;
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret >= 0) {
		lockFastMutex((SceKernelFastMutex *)global->pMutex);
		ret = dsp0VideocodecCalculateLibraryMemory(param, &requiredSize, &librarySize);
		if (ret >= 0)
			*memorySize = requiredSize;
		sceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
		_unlockProcessSuspendCore();
	}
	return ret;
}

/*
 * The caller's memory holds, in order: the memory and library commands, the
 * library's context and work memory, the decoder slots and their buffers, then
 * the encoder slots and theirs.
 */
int dsp0VideocodecInitLibrary(unsigned int *pLibrary, const Dsp0VideocodecLibraryMemory *pMemory)
{
	Dsp0VideocodecGlobal *global;
	const Dsp0VideocodecInitParam *param;
	Dsp0VideocodecLibraryCommand *library;
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecEncoder *encoder;
	unsigned char *memory, *buffer;
	SceSize requiredSize, librarySize, tableSize;
	unsigned int pictureSize;
	uintptr_t physical;
	SceKernelIntrStatus intr;
	int i, ret;

	global = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	sceKernelAssert(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	if (pMemory == NULL || pLibrary == NULL || (param = pMemory->param) == NULL)
		return (int)0x80620209;
	if (global->initialized != 0)
		return (int)0x80620208;
	if (param->horizontal - 64 > 1216 || param->vertical - 64 > 656 ||
	    ((param->horizontal | param->vertical) & 15) != 0 ||
	    param->numOfRefFrames - 1 > 15 || param->numOfStreams - 1 > 3)
		return (int)0x80620201;
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0)
		return ret;
	lockFastMutex((SceKernelFastMutex *)global->pMutex);
	ret = dsp0VideocodecCalculateLibraryMemory(param, &requiredSize, &librarySize);
	if (ret < 0)
		goto release;
	memory = pMemory->base;
	if (memory == NULL || ((uintptr_t)memory & 255) != 0)
		goto invalid;
	if (pMemory->size < requiredSize) {
		ret = (int)0x80620203;
		goto release;
	}
	memset(memory, 0, pMemory->size);
	global->workMemory = memory;
	library = (Dsp0VideocodecLibraryCommand *)(memory + 256);
	global->library = library;
	library->contextAddress = (uintptr_t)(memory + 512);
	if (dsp0VideocodecInitializeMemory(memory) < 0)
		goto failed;
	global->param = *param;
	library = global->library;
	library->numOfDecoders = global->param.numOfStreams;
	library->commandSize = sizeof(*library);
	library->horizontal = global->param.horizontal;
	library->coprocessorQueryData[0] = 0;
	library->coprocessorQueryData[1] = 0;
	library->numOfEncoders = 1;
	if (dsp0VideocodecQueryMemory(library) < 0)
		goto failed;

	library = global->library;
	library->workAddress = (uintptr_t)(memory + 768);
	library->workSize = librarySize;
	global->decoders = (Dsp0VideocodecDecoder *)(memory + 768 + librarySize);
	tableSize = (sizeof(Dsp0VideocodecDecoder) * library->numOfDecoders + 255) & ~255u;
	buffer = (unsigned char *)global->decoders + tableSize;
	memset(global->decoders, 0, tableSize);
	pictureSize = (((3 * global->param.vertical * global->param.horizontal) >> 2) +
		       255) & 0x7fffff00;
	if (pictureSize < 81920)
		pictureSize = 81920;
	decoder = global->decoders;
	for (i = 0; i < library->numOfDecoders; i++, decoder++) {
		dsp0VideocodecDecoderMutexName[16] = '0' + i % 10;
		ret = sceKernelInitializeFastMutex(&decoder->mutex,
				dsp0VideocodecDecoderMutexName, 0x8002, 0);
		if (ret < 0)
			goto releaseBuffers;
		decoder->pCommand = buffer;
		decoder->contextAddress = (uintptr_t)(buffer + 256);
		decoder->pPicture = (Dsp0VideocodecPicture *)(buffer + 512);
		decoder->au[0].address = (uintptr_t)(buffer + 768);
		decoder->au[0].size = pictureSize;
		decoder->au[1].address = (uintptr_t)(buffer + 768 + pictureSize);
		decoder->au[1].size = pictureSize;
		decoder->workAddress = (uintptr_t)(buffer + 768 + 2 * pictureSize);
		decoder->workSize = 20480;
		decoder->pMutex = &decoder->mutex;
		buffer += 768 + 2 * pictureSize + 20480;
	}

	global->encoders = (Dsp0VideocodecEncoder *)buffer;
	tableSize = (sizeof(Dsp0VideocodecEncoder) * library->numOfEncoders + 255) & ~255u;
	memset(buffer, 0, tableSize);
	buffer += tableSize;
	encoder = global->encoders;
	for (i = 0; i < library->numOfEncoders; i++, encoder++) {
		dsp0VideocodecEncoderMutexName[16] = '0' + i % 10;
		ret = sceKernelInitializeFastMutex(&encoder->mutex,
				dsp0VideocodecEncoderMutexName, 0x8002, 0);
		if (ret < 0)
			goto releaseBuffers;
		encoder->pCommand = (Dsp0VideocodecEncoderCommand *)buffer;
		encoder->contextAddress = (uintptr_t)(buffer + 512);
		encoder->coprocessorBufferAddress[0] = (uintptr_t)(buffer + 768);
		encoder->coprocessorBufferAddress[1] = (uintptr_t)(buffer + 1024);
		encoder->pEncodeResult = (Dsp0VideocodecEncodeResult *)(buffer + 1280);
		encoder->pMutex = &encoder->mutex;
		buffer += 1536;
	}

	library = global->library;
	if (sceKernelVAtoPA((const void *)library->contextAddress, &physical) < 0)
		goto invalid;
	library->contextAddress = physical;
	if (sceKernelVAtoPA((const void *)library->workAddress, &physical) < 0)
		goto invalid;
	library->workAddress = physical;
	for (i = 0; i < library->numOfDecoders; i++) {
		decoder = &global->decoders[i];
		if (sceKernelVAtoPA((const void *)decoder->contextAddress, &physical) < 0)
			goto invalid;
		decoder->contextAddress = physical;
		if (sceKernelVAtoPA((const void *)decoder->workAddress, &physical) < 0)
			goto invalid;
		decoder->workAddress = physical;
	}
	for (i = 0; i < library->numOfEncoders; i++) {
		encoder = &global->encoders[i];
		if (sceKernelVAtoPA((const void *)encoder->contextAddress, &physical) < 0)
			goto invalid;
		encoder->contextAddress = physical;
		if (sceKernelVAtoPA((const void *)encoder->coprocessorBufferAddress[0],
				    &physical) < 0)
			goto invalid;
		encoder->coprocessorBufferAddress[0] = physical;
		if (sceKernelVAtoPA((const void *)encoder->coprocessorBufferAddress[1],
				    &physical) < 0)
			goto invalid;
		encoder->coprocessorBufferAddress[1] = physical;
	}
	ret = dsp0VideocodecCreateDecoder(global->library);
	if (ret < 0)
		goto failed;
	global->initialized = 1;
	*pLibrary = 1;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&global->statusLock);
	global->status = 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
	sceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
	_unlockProcessSuspendCore();
	return ret;

releaseBuffers:
	dsp0VideocodecReleaseLibraryBuffers(global);
	goto release;
failed:
	ret = (int)0x8062020d;
	goto release;
invalid:
	ret = (int)0x80620209;
release:
	dsp0VideocodecReleaseLibraryBuffers(global);
	if (global->library != NULL) {
		if (global->library->contextAddress != 0)
			global->library->contextAddress = 0;
		global->library = NULL;
	}
	if (global->workMemory != NULL)
		global->workMemory = NULL;
	sceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
	_unlockProcessSuspendCore();
	return ret;
}

int dsp0VideocodecTermLibraryForProcess(unsigned int codecType, SceUID pid)
{
	void *storage = NULL;
	Dsp0VideocodecGlobal *global;
	SceKernelIntrStatus intr;
	int ret;

	if (pid != 0) {
		if (sceKernelGetRemoteKPLS(pid,
				dsp0VideocodecLocalStorageId, &storage, 0) < 0)
			storage = NULL;
	} else {
		storage = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
	}
#if FW_DEBUG
	sceKernelAssert(storage != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	global = storage;
	if (codecType != 1)
		return (int)0x80620201;
	if (global->initialized != 1)
		return (int)0x8062020c;
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret >= 0) {
		lockFastMutex((SceKernelFastMutex *)global->pMutex);
		ret = dsp0VideocodecDeleteDecoder(global->library);
		if (ret < 0)
			ret = (int)0x8062020d;
		dsp0VideocodecReleaseLibraryBuffers(global);
		if (global->library != NULL) {
			if (global->library->contextAddress != 0)
				global->library->contextAddress = 0;
			global->library = NULL;
		}
		if (global->workMemory != NULL)
			global->workMemory = NULL;
		global->initialized = 0;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&global->statusLock);
		global->status = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
		sceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
		_unlockProcessSuspendCore();
	}
	return ret;
}

int dsp0VideocodecQueryAvcDecoderMemSize(const VideocodecDsp0Decoder *pInfo,
					 SceSize *frameMemSize)
{
	Dsp0VideocodecGlobal *global;
	Dsp0VideocodecDecoderCommand *command;
	int ret;

	global = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	sceKernelAssert(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	if (frameMemSize == NULL || pInfo == NULL)
		return (int)0x80620209;
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0)
		return ret;
	lockFastMutex((SceKernelFastMutex *)global->pMutex);
	command = dsp0VideocodecAllocMemory(0, sizeof(*command));
	if (command != NULL) {
		memset(command, 0, sizeof(*command));
		command->coprocessorModes[0] = 1;
		command->coprocessorModes[1] = 1;
		command->coprocessorModes[2] = 1;
		command->decodeMode = pInfo->mode;
		command->horizontal = pInfo->info.horizontal;
		command->vertical = pInfo->info.vertical;
		command->numOfRefFrames = pInfo->info.numOfRefFrames;
		ret = dsp0VideocodecQueryDecoderContext(command);
		if (ret >= 0) {
			ret = 0;
			*frameMemSize = command->frameBufSize;
		}
		SceCodecEngineWrapperForDriver_3C5E9CC3(0, command, sizeof(*command));
	} else {
		ret = (int)0x80620203;
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
	_unlockProcessSuspendCore();
	return ret;
}

int dsp0VideocodecCreateAvcDecoder(unsigned int codecType, unsigned int *pHandle,
				   const Dsp0VideocodecDecoderParam *param)
{
	Dsp0VideocodecGlobal *global;
	const VideocodecDsp0Decoder *pInfo;
	Dsp0VideocodecLibraryCommand *library;
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecDecoderCommand *command;
	unsigned int index;
	int i, ret;

	global = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	sceKernelAssert(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	if (codecType != 1)
		return (int)0x80620201;
	if (global->initialized != 1)
		return (int)0x8062020c;
	if (param == NULL || pHandle == NULL || param->pFrameBuf == NULL ||
	    param->pInfo == NULL)
		return (int)0x80620209;
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0)
		return ret;
	lockFastMutex((SceKernelFastMutex *)global->pMutex);
	pInfo = param->pInfo;
	if (pInfo->info.horizontal < 64 ||
	    pInfo->info.horizontal > global->param.horizontal ||
	    pInfo->info.vertical < 64 ||
	    pInfo->info.vertical > global->param.vertical ||
	    ((pInfo->info.horizontal | pInfo->info.vertical) & 15) != 0 ||
	    pInfo->info.numOfRefFrames == 0 ||
	    pInfo->info.numOfRefFrames > global->param.numOfRefFrames ||
	    pInfo->mode > 2) {
		ret = (int)0x80620201;
		goto unlock;
	}
	library = global->library;
	decoder = NULL;
	for (i = 0; i < library->numOfDecoders; i++) {
		if (global->decoders[i].state == 0) {
			decoder = &global->decoders[i];
			index = i + 1;
			break;
		}
	}
	if (decoder == NULL) {
		ret = (int)0x8062020e;
		goto unlock;
	}
	command = decoder->pCommand;
	decoder->status = 2;
	memset(command, 0, 208);
	command->libraryToken = library->libraryToken;
	command->coprocessorModes[0] = 1;
	command->coprocessorModes[1] = 1;
	command->coprocessorModes[2] = 1;
	command->coprocessorSetup[0] = 2;
	command->coprocessorSetup[1] = 4;
	command->decodeMode = ((const VideocodecDsp0Decoder *)param->pInfo)->mode;
	ret = dsp0VideocodecQueryDecoderMemory(command);
	if (ret < 0)
		goto unlock;
	if (command->workSize > decoder->workSize) {
		ret = (int)0x8062020d;
		goto unlock;
	}
	command->workAddress = decoder->workAddress;
	command->au[0].bufferAddress = decoder->au[0].address;
	command->au[0].bufferSize = decoder->au[0].size;
	command->contextAddress = decoder->contextAddress;
	command->au[1].bufferAddress = decoder->au[1].address;
	command->au[1].bufferSize = decoder->au[1].size;
	command->pPicture = decoder->pPicture;
	command->numOfPicture = 1;
	ret = dsp0VideocodecInitializeDecoder(command, library);
	if (ret < 0)
		goto unlock;
	pInfo = param->pInfo;
	command->coprocessorModes[0] = 1;
	command->coprocessorModes[1] = 1;
	command->coprocessorModes[2] = 1;
	command->decodeMode = pInfo->mode;
	command->horizontal = pInfo->info.horizontal;
	command->vertical = pInfo->info.vertical;
	command->numOfRefFrames = pInfo->info.numOfRefFrames;
	if (dsp0VideocodecQueryDecoderContext(command) >= 0 &&
	    param->frameBufSize >= command->frameBufSize) {
		command->frameBufAddress = (uintptr_t)param->pFrameBuf;
		command->frameBufSize = param->frameBufSize;
		ret = dsp0VideocodecInitializeDecoderContext(command);
		if (ret >= 0) {
			decoder->state = 256;
			decoder->auStatus = 7;
			command->au[0].valid = 7;
			command->au[1].valid = decoder->auStatus;
			*pHandle = index + 256;
			goto unlock;
		}
	}
	ret = dsp0VideocodecTerminateDecoder(command, library);
	decoder->state = 0;
	if (ret < 0)
		ret = (int)0x8062020d;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
	_unlockProcessSuspendCore();
	return ret;
}

int dsp0VideocodecCreateM4vDecoder(unsigned int codecType, unsigned int *pHandle,
				   const Dsp0VideocodecDecoderParam *param)
{
	Dsp0VideocodecGlobal *global;
	const Dsp0VideocodecM4vDecoderInfo *pInfo;
	Dsp0VideocodecLibraryCommand *library;
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecM4vDecoderCommand *command;
	unsigned int index;
	int i, ret;

	global = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	sceKernelAssert(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	if (codecType != 1)
		return (int)0x80620201;
	if (global->initialized != 1)
		return (int)0x8062020c;
	if (param == NULL || pHandle == NULL || param->pFrameBuf == NULL ||
	    param->pInfo == NULL)
		return (int)0x80620209;
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0)
		return ret;
	lockFastMutex((SceKernelFastMutex *)global->pMutex);
	pInfo = param->pInfo;
	if (pInfo->horizontal < 64 || pInfo->horizontal > global->param.horizontal ||
	    pInfo->vertical < 64 || pInfo->vertical > global->param.vertical ||
	    ((pInfo->horizontal | pInfo->vertical) & 15) != 0 ||
	    pInfo->maxEsSize - 2048 > 0x17c000 - 2048) {
		ret = (int)0x80620201;
		goto unlock;
	}
	library = global->library;
	decoder = NULL;
	for (i = 0; i < library->numOfDecoders; i++) {
		if (global->decoders[i].state == 0) {
			decoder = &global->decoders[i];
			index = i + 1;
			break;
		}
	}
	if (decoder == NULL) {
		ret = (int)0x8062020e;
		goto unlock;
	}
	command = decoder->pCommand;
	decoder->status = 2;
	memset(command, 0, 200);
	command->libraryToken = library->libraryToken;
	command->coprocessorModes[0] = 1;
	command->coprocessorModes[1] = 1;
	ret = dsp0VideocodecQueryM4vDecoderMemory(command);
	if (ret < 0)
		goto unlock;
	if (command->workSize > decoder->workSize) {
		ret = (int)0x8062020d;
		goto unlock;
	}
	command->contextAddress = decoder->contextAddress;
	command->workAddress = decoder->workAddress;
	command->au[0].bufferAddress = decoder->au[0].address;
	command->au[0].bufferSize = decoder->au[0].size;
	command->au[1].bufferAddress = decoder->au[1].address;
	command->au[1].bufferSize = decoder->au[1].size;
	command->pPicture = decoder->pPicture;
	command->numOfPicture = 1;
	ret = dsp0VideocodecInitializeM4vDecoder(command, library);
	if (ret < 0)
		goto unlock;
	pInfo = param->pInfo;
	command->vertical = pInfo->vertical;
	command->horizontal = pInfo->horizontal;
	command->maxEsSize = pInfo->maxEsSize;
	command->coprocessorModes[0] = 1;
	command->coprocessorModes[1] = 1;
	if (dsp0VideocodecQueryM4vDecoderContext(command) >= 0 &&
	    param->frameBufSize >= command->frameBufSize) {
		command->frameBufAddress = (uintptr_t)param->pFrameBuf;
		command->frameBufSize = param->frameBufSize;
		ret = dsp0VideocodecInitializeM4vDecoderContext(command);
		if (ret >= 0) {
			decoder->state = 512;
			decoder->auStatus = 7;
			command->au[0].valid = 7;
			command->au[1].valid = decoder->auStatus;
			*pHandle = index + 512;
			goto unlock;
		}
	}
	ret = dsp0VideocodecTerminateM4vDecoder(command, library);
	decoder->state = 0;
	if (ret < 0)
		ret = (int)0x8062020d;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
	_unlockProcessSuspendCore();
	return ret;
}

int dsp0VideocodecDeleteAvcDecoder(unsigned int handle, SceUID pid)
{
	void *storage = NULL;
	Dsp0VideocodecGlobal *global;
	Dsp0VideocodecLibraryCommand *library;
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecDecoderCommand *command;
	unsigned int index;
	int ret;

	if (pid != 0) {
		if (sceKernelGetRemoteKPLS(pid,
				dsp0VideocodecLocalStorageId, &storage, 0) < 0)
			storage = NULL;
	} else {
		storage = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
	}
#if FW_DEBUG
	sceKernelAssert(storage != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	global = storage;
	if (global->initialized != 1)
		return (int)0x8062020c;
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0)
		return ret;
	lockFastMutex((SceKernelFastMutex *)global->pMutex);
	library = global->library;
	index = handle - 257;
	decoder = &global->decoders[index];
	if (index < library->numOfDecoders && decoder->state == 256) {
		command = decoder->pCommand;
		if (decoder->status != 2) {
			decoder->status = 2;
			dsp0VideocodecStopDecoder(decoder->pCommand);
		}
		ret = dsp0VideocodecTerminateDecoder(command, library);
		decoder->state = 0;
		if (ret < 0)
			ret = (int)0x8062020d;
	} else {
		ret = (int)0x8062020c;
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
	_unlockProcessSuspendCore();
	return ret;
}

unsigned int dsp0VideocodecAvcDecodeAvailableSize(unsigned int handle)
{
	Dsp0VideocodecGlobal *global;
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecDecoderCommand *command;
	unsigned int size, valid;

	global = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	sceKernelAssert(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	if (global->initialized != 1)
		return 0x8062020c;
	decoder = &global->decoders[handle - 257];
	if (handle - 257 >= global->library->numOfDecoders)
		return 0x8062020c;
	if (decoder->state != 256)
		return 0x8062020c;
	lockFastMutex((SceKernelFastMutex *)decoder->pMutex);
	command = decoder->pCommand;
	valid = command->au[command->auIndex].valid;
	if (valid != 0) {
		size = global->param.horizontal * global->param.vertical;
		sceKernelUnlockFastMutex((SceKernelFastMutex *)decoder->pMutex);
		return (((3 * size) >> 2) + 255) & 0x7fffff00;
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)decoder->pMutex);
	return valid;
}

int dsp0VideocodecAvcDecodeCore(Dsp0VideocodecGlobal *global, unsigned int handle,
				const SceAvcdecAu *pAu,
				const Dsp0VideocodecArrayPicture *pArrayPicture,
				unsigned int *pNumOfOutput, unsigned int *pNumOfDecoded)
{
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecDecoderCommand *command;
	unsigned int index, auIndex;
	int ret;

	if (global->initialized != 1)
		return (int)0x8062020c;
	if (pNumOfOutput == NULL || pArrayPicture == NULL ||
	    pArrayPicture->pPicture == NULL)
		return (int)0x80620209;
	if (pArrayPicture->numOfElm > 1)
		return (int)0x80620201;
	index = handle - 257;
	if (index >= global->library->numOfDecoders)
		return (int)0x8062020c;
	decoder = &global->decoders[index];
	if (decoder->state != 256)
		return (int)0x8062020c;
	command = decoder->pCommand;
	auIndex = command->auIndex;
	if (pAu == NULL || pAu->es.pBuf == NULL || command->au[auIndex].valid == 0) {
		command->au[auIndex].esSize = 0;
		command->au[auIndex].valid = 0;
	} else {
		if (decoder->au[auIndex].size < pAu->es.size)
			return (int)0x8062020f;
		ret = SceCodecEngineWrapperForDriver_FAF590A6(
			(void *)decoder->au[auIndex].address, pAu->es.pBuf,
			pAu->es.size);
		if (ret < 0)
			return ret;
		command->au[auIndex].valid = decoder->auStatus;
		command->au[auIndex].esSize = pAu->es.size;
		command->au[auIndex].pts = (SceUInt64)pAu->pts.upper << 32 |
					   pAu->pts.lower;
		command->au[auIndex].dts = (SceUInt64)pAu->dts.upper << 32 |
					   pAu->dts.lower;
	}
	command->au[auIndex].numOfOutput = 0;
	command->au[auIndex].esAddress = command->au[auIndex].bufferAddress;
	ret = dsp0VideocodecDecodePicture(command);
	if (ret >= 0) {
		decoder->status = 1;
		if (pAu != NULL && pAu->es.pBuf != NULL && command->au[auIndex].valid == 0)
			ret = (int)0x80620210;
		*pNumOfOutput = command->au[auIndex].numOfOutput > 0 ? command->numOfPicture : 0;
		if (pNumOfDecoded != NULL)
			*pNumOfDecoded = command->au[auIndex].numOfOutput;
		if ((unsigned int)command->au[auIndex].numOfOutput <= command->numOfPicture)
			command->au[command->auIndex].valid = decoder->auStatus;
		else
			command->au[command->auIndex].valid = 0;
		*pArrayPicture->pPicture[0] = command->pPicture;
	}
	return ret;
}

int dsp0VideocodecAvcDecode(unsigned int handle, const SceAvcdecAu *pAu,
			    const Dsp0VideocodecArrayPicture *pArrayPicture,
			    unsigned int *pNumOfOutput)
{
	Dsp0VideocodecGlobal *global;
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecArrayPicture retryPicture;
	Dsp0VideocodecPicture *picture;
	Dsp0VideocodecPicture **pPicture;
	unsigned int numOfDecoded, numOfOutput;
	int ret;

	global = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	sceKernelAssert(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	decoder = &global->decoders[handle - 257];
	numOfDecoded = 0;
	if (handle - 257 >= global->library->numOfDecoders ||
	    decoder->state != 256)
		return (int)0x8062020c;
	ret = lockFastMutex((SceKernelFastMutex *)decoder->pMutex);
	if (ret < 0)
		return ret;
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret >= 0) {
		ret = dsp0VideocodecAvcDecodeCore(global, handle, pAu,
				pArrayPicture, pNumOfOutput, &numOfDecoded);
		if (ret == (int)0x80620210 && pNumOfOutput != NULL) {
			if (*pNumOfOutput >= numOfDecoded && pAu != NULL &&
			    pAu->es.pBuf != NULL) {
				retryPicture.numOfElm = 1;
				pPicture = &picture;
				retryPicture.pPicture = &pPicture;
				ret = dsp0VideocodecAvcDecodeCore(global, handle,
						pAu, &retryPicture, &numOfOutput,
						&numOfDecoded);
				if (ret >= 0 && numOfOutput != 0)
					ret = (int)0x80620204;
			} else {
				ret = (int)0x80620210;
			}
		}
		_unlockProcessSuspendCore();
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)decoder->pMutex);
	return ret;
}

static inline int dsp0VideocodecAvcDecodeStopInline(unsigned int handle,
		const Dsp0VideocodecArrayPicture *pArrayPicture,
		unsigned int *pNumOfOutput)
{
	Dsp0VideocodecGlobal *global;
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecDecoderCommand *command;
	unsigned int auIndex;
	int ret;

	global = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	sceKernelAssert(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	if (global->initialized != 1)
		return (int)0x8062020c;
	if (pNumOfOutput == NULL || pArrayPicture == NULL ||
	    pArrayPicture->pPicture == NULL)
		return (int)0x80620209;
	if (pArrayPicture->numOfElm > 1)
		return (int)0x80620201;
	decoder = &global->decoders[handle - 257];
	if (handle - 257 >= global->library->numOfDecoders)
		return (int)0x8062020c;
	if (decoder->state != 256)
		return (int)0x8062020c;
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0)
		return ret;
	lockFastMutex((SceKernelFastMutex *)decoder->pMutex);
	command = decoder->pCommand;
	auIndex = command->auIndex;
	command->au[auIndex].valid = 0;
	ret = dsp0VideocodecStopDecoder(command);
	if (ret >= 0) {
		decoder->status = 2;
		*pNumOfOutput = command->au[auIndex].numOfOutput > 0 ?
				command->numOfPicture : 0;
		if ((unsigned int)command->au[auIndex].numOfOutput <=
		    command->numOfPicture)
			command->au[command->auIndex].valid = decoder->auStatus;
		else
			command->au[command->auIndex].valid = 0;
		*pArrayPicture->pPicture[0] = command->pPicture;
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)decoder->pMutex);
	_unlockProcessSuspendCore();
	return ret;
}

int dsp0VideocodecAvcDecodeStop(unsigned int handle,
				const Dsp0VideocodecArrayPicture *pArrayPicture,
				unsigned int *pNumOfOutput)
{
	return dsp0VideocodecAvcDecodeStopInline(handle, pArrayPicture,
						 pNumOfOutput);
}

int dsp0VideocodecAvcDecodeFlush(unsigned int handle)
{
	Dsp0VideocodecGlobal *global;
	Dsp0VideocodecDecoder *decoder;
	Dsp0VideocodecArrayPicture arrayPicture;
	Dsp0VideocodecPicture *picture;
	Dsp0VideocodecPicture **pPicture;
	unsigned int numOfOutput;
	int ret;

	global = SceProcessmgrForDriver_EE694840(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	sceKernelAssert(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	if (global->initialized != 1)
		return (int)0x8062020c;
	decoder = &global->decoders[handle - 257];
	if (handle - 257 >= global->library->numOfDecoders)
		return (int)0x8062020c;
	if (decoder->state != 256)
		return (int)0x8062020c;
	lockFastMutex((SceKernelFastMutex *)decoder->pMutex);
	arrayPicture.numOfElm = 1;
	pPicture = &picture;
	arrayPicture.pPicture = &pPicture;
	do {
		ret = dsp0VideocodecAvcDecodeStopInline(handle, &arrayPicture,
							&numOfOutput);
		if (ret < 0)
			break;
	} while (numOfOutput != 0);
	sceKernelUnlockFastMutex((SceKernelFastMutex *)decoder->pMutex);
	return ret;
}
