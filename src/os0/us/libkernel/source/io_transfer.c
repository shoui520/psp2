#include <sce/libkernel_backends.h>
#include <psp2common/kernel/iofilemgr.h>

struct IoSeekOptions {
	SceOff offset;
	int whence;
};

struct IoSeekAsyncOptions {
	SceOff offset;
	int whence;
	void *result;
	SceUInt reserved[2];
};

struct IoControlOptions {
	SceSize inputSize;
	void *output;
	SceSize outputSize;
	SceUInt reserved;
};

struct IoDeviceControlOptions {
	SceSize inputSize;
	void *output;
	SceSize outputSize;
	SceUInt reserved[3];
};

struct IoControlAsyncOptions {
	SceSize inputSize;
	void *output;
	SceSize outputSize;
	void *result;
	SceUInt reserved[2];
};

struct IoPositionOptions {
	SceOff offset;
	SceUInt reserved[2];
};

struct IoPositionAsyncOptions {
	SceOff offset;
	void *result;
};


SceOff sceIoLseek(SceUID id, SceOff offset, int whence)
{
	struct IoSeekOptions options = { offset, whence };

	return _sceIoLseek(id, &options);
}

int sceIoLseekAsync(SceUID id, SceOff offset, int whence, void *result)
{
	struct IoSeekAsyncOptions options;

	options.offset = offset;
	options.whence = whence;
	options.result = result;

	return _sceIoLseekAsync(id, &options);
}

int sceIoIoctl(SceUID id, unsigned int command, const void *input,
		SceSize inputSize, void *output, SceSize outputSize)
{
	struct IoControlOptions options;

	options.inputSize = inputSize;
	options.output = output;
	options.outputSize = outputSize;

	return _sceIoIoctl(id, command, input, &options);
}

int sceIoDevctl(const char *device, unsigned int command, const void *input,
		SceSize inputSize, void *output, SceSize outputSize)
{
	struct IoDeviceControlOptions options;

	options.inputSize = inputSize;
	options.output = output;
	options.outputSize = outputSize;

	return _sceIoDevctl(device, command, input, &options);
}

int sceIoIoctlAsync(SceUID id, unsigned int command, const void *input,
		SceSize inputSize, void *output, SceSize outputSize, void *result)
{
	struct IoControlAsyncOptions options;

	options.inputSize = inputSize;
	options.output = output;
	options.outputSize = outputSize;
	options.result = result;

	return _sceIoIoctlAsync(id, command, input, &options);
}

int sceIoDevctlAsync(const char *device, unsigned int command, const void *input,
		SceSize inputSize, void *output, SceSize outputSize, void *result)
{
	struct IoControlAsyncOptions options;

	options.inputSize = inputSize;
	options.output = output;
	options.outputSize = outputSize;
	options.result = result;

	return _sceIoDevctlAsync(device, command, input, &options);
}

int sceIoPread(SceUID id, void *buffer, SceSize size, SceOff offset)
{
	struct IoPositionOptions options;

	options.offset = offset;

	return _sceIoPread(id, buffer, size, &options);
}

int sceIoPwrite(SceUID id, const void *buffer, SceSize size, SceOff offset)
{
	struct IoPositionOptions options;

	options.offset = offset;

	return _sceIoPwrite(id, buffer, size, &options);
}

int sceIoPreadAsync(SceUID id, void *buffer, SceSize size, SceOff offset,
		void *result)
{
	struct IoPositionAsyncOptions options = { offset, result };

	return _sceIoPreadAsync(id, buffer, size, &options);
}

int sceIoPwriteAsync(SceUID id, const void *buffer, SceSize size, SceOff offset,
		void *result)
{
	struct IoPositionAsyncOptions options = { offset, result };

	return _sceIoPwriteAsync(id, buffer, size, &options);
}

int sceIoDread(SceUID id, SceIoDirent *entry)
{
	return _sceIoDread(id, entry);
}

int sceIoGetstatByFd(SceUID id, SceIoStat *status)
{
	return _sceIoGetstatByFd(id, status);
}

int sceIoChstatByFd(SceUID id, const SceIoStat *status, int bits)
{
	return _sceIoChstatByFd(id, status, bits);
}

int sceIoCompleteMultiple(const void *requests, int count, int *completed)
{
	return _sceIoCompleteMultiple(requests, count, completed);
}
