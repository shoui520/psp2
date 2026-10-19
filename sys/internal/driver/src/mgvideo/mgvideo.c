/* Exported SceMgVideo APIs. */

#include "mgvideo_private.h"
#include <intrmgr/syscall.h>
#include <magicgate/magicgate.h>
#include <acmgr/acmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>



int SceMgVideo_C62CA5A7(int type, int flags, int options)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = createVideoContext(type, flags, options);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int SceMgVideo_D6EE7B53(int handle)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = releaseVideoContext(handle);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int waitUserVideoRecord(int handle, int index, int flags)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = waitVideoRecord(handle, index, flags);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int signalUserVideoRecord(int handle, int index)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = signalVideoRecord(handle, index);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int initializeUserVideoKeyring(int handle)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = initializeVideoKeyring(handle);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int SceMgVideo_18D553EB(int handle, MgVideoFileGroupInfo *output,
		      unsigned int size)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = enumerateVideoFileGroups(handle, output, size);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int authorizeVideoRecord(int handle, unsigned int index,
			const MgVideoUserBuffer *recordInput,
			const MgVideoUserBuffer *input)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = authorizeUserVideoRecord(handle, index,
							     recordInput, input);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int openUserVideoRecordStream(int handle, unsigned int index, int blockNumber,
			    const MgVideoUserBuffer *keyring,
			    const MgVideoUserBuffer *recordInput,
			    const MgVideoUserBuffer *input)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = openVideoRecordStream(handle, index,
							  blockNumber, keyring,
							  recordInput, input);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int closeUserVideoRecordStream(int handle)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = closeVideoRecordStream(handle);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int openUserVideoCipherChannel(int handle, const MgVideoBoxHeader *input, int size)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = openVideoCipherChannel(handle, input, size);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int closeUserVideoCipherChannel(int handle)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = closeVideoCipherChannel(handle);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int isUserVideoCipherKeySelected(int handle, unsigned int selection)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = isVideoCipherKeySelected(handle, selection);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int decryptUserVideoCipherBuffer(unsigned int handle, void *output,
			       const void *input, unsigned int size)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = decryptVideoCipherBuffer(handle, output,
							     input, size);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int decryptUserVideoCipherStream(unsigned int handle, void *output,
			       const void *input, unsigned int size,
			       unsigned int totalSize, unsigned int *processed)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = decryptVideoCipherStream(handle, output,
							     input, size, totalSize,
							     processed);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int encryptUserVideoCipherBuffer(unsigned int handle, void *output,
			       const void *input, unsigned int size,
			       uint32_t blockIndex)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = encryptVideoCipherBuffer(handle, output,
							     input, size, blockIndex);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int encryptUserVideoCipherStream(unsigned int handle, void *output,
			       const void *input, unsigned int size,
			       unsigned int totalSize, unsigned int *processed,
			       uint32_t blockIndex)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = encryptVideoCipherStream(handle, output,
							     input, size, totalSize,
							     processed, blockIndex);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int removeVideoRecordBlocks(int handle, unsigned int index,
			    const MgVideoUserBuffer *recordInput,
			    const int16_t *blockNumbers, int blockCount,
			    const MgVideoUserBuffer *input)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = removeUserVideoRecordBlocks(handle, index,
								recordInput, blockNumbers,
								blockCount, input);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int saveUserVideoKeyring(int handle)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = saveVideoKeyring(handle);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int resetUserVideoKeyring(int handle)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = resetVideoKeyring(handle);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int SceMgVideo_22D74C37(int handle, const MagicGateFileIdentity *input)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = selectVideoFileGroup(handle, input);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int queryUserVideoPlaybackInfo(int handle, void *output, unsigned int outputSize,
			      const MgVideoUserBuffer *input, int mode)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = getUserVideoPlaybackInfo(handle, output,
							     outputSize, input, mode);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int exportUserVideoEkb2Record(int handle, const MgVideoUserBuffer *output)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = copyVideoEkb2RecordToUser(handle, output);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int createUserVideoKeyring(int handle, const MgVideoUserBuffer *output,
			  MgVideoGeneratedProtectionInfo *protection,
			  const MgVideoAvOut *avOut,
			  const MgVideoKeyGenerationParameters *parameters,
			  int mode)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = generateUserVideoKeyring(handle, output,
							     protection, avOut,
							     parameters, mode);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int replaceVideoKeyring(int handle, unsigned int index, int blockNumber,
			const MgVideoUserBuffer *keyring,
			const MgVideoUserBuffer *input,
			const MgVideoUserBuffer *recordInput,
			const MgVideoUserBuffer *replacementInput)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = replaceUserVideoKeyring(handle, index,
							    blockNumber, keyring,
							    input, recordInput,
							    replacementInput);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int updateVideoPlaybackPeriod(int handle, unsigned int index, int blockNumber,
			      const MgVideoUserBuffer *keyring,
			      const MgVideoUserBuffer *input,
			      const MgVideoUserBuffer *recordInput,
			      const MgVideoUserBuffer *replacementInput,
			      const MgVideoUserBuffer *optionInput,
			      const void *extensionInput)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = updateUserVideoPlaybackPeriod(handle, index,
								 blockNumber, keyring,
								 input, recordInput,
								 replacementInput,
								 optionInput, extensionInput);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int SceMgVideoForMiniApp_3EBD4502(int type, int flags, int options)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = createVideoContext(type, flags, options);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int SceMgVideoForMiniApp_02E5F373(int handle)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = releaseVideoContext(handle);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int SceMgVideoForMiniApp_EA3A897A(int handle, MgVideoFileGroupInfo *output,
			       unsigned int size)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = enumerateVideoFileGroups(handle, output, size);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

int SceMgVideoForMiniApp_CF0D2A06(int handle, const MagicGateFileIdentity *input)
{
	SceUInt32 user = sceSyscallEnter();
	int status;

	if (SceSblACMgrForDriver_A27E47A7(0)) {
		status = lockVideoDriver();
		if (status >= 0) {
			status = sceKernelSetThreadAccessLevel(64);
			if (status >= 0) {
				int result = selectVideoFileGroup(handle, input);

				sceKernelSetThreadAccessLevel(status);
				unlockVideoDriver();
				sceSyscallLeave(user);
				return result;
			}
		}
	} else {
		status = (int)0x80010058;
	}
	sceSyscallLeave(user);
	return status;
}

#if FW_VERSION >= 0x03650000 && FW_VERSION < 0x04000000
int SceMgVideo_E8617A7F(int type, int flags, int options)
{
	return SceMgVideo_C62CA5A7(type, flags, options);
}

int SceMgVideo_653809E0(int handle)
{
	return SceMgVideo_D6EE7B53(handle);
}
#endif

int SceMgVideo_4C014C4D(int handle, int index, int flags)
{
	return waitUserVideoRecord(handle, index, flags);
}

int SceMgVideo_8CDC8D33(int handle, int index)
{
	return signalUserVideoRecord(handle, index);
}

int SceMgVideo_11FF1A1B(int handle)
{
	return initializeUserVideoKeyring(handle);
}

#if FW_VERSION >= 0x03650000 && FW_VERSION < 0x04000000
int SceMgVideo_5ADD5F9B(int handle, MgVideoFileGroupInfo *output,
		      unsigned int size)
{
	return SceMgVideo_18D553EB(handle, output, size);
}
#endif

int SceMgVideo_BCC6E991(int handle, unsigned int index,
		      const MgVideoUserBuffer *recordInput,
		      const MgVideoUserBuffer *input)
{
	MgVideoUserBuffer *recordCopy = 0;
	int result;

	if (recordInput) {
		recordCopy = __builtin_alloca(sizeof(*recordCopy));
		result = sceKernelCopyFromUser(recordCopy, recordInput,
					       sizeof(*recordCopy));
		if (result < 0)
			return result;
	}
	if (input) {
		MgVideoUserBuffer *inputCopy = __builtin_alloca(sizeof(*inputCopy));

		result = sceKernelCopyFromUser(inputCopy, input, sizeof(*inputCopy));
		input = inputCopy;
		if (result < 0)
			return result;
	}
	return authorizeVideoRecord(handle, index, recordCopy, input);
}

int SceMgVideo_E9367823(int handle, unsigned int index, int blockNumber,
		      const MgVideoStreamOpenArguments *inputArguments)
{
	MgVideoStreamOpenArguments arguments;
	const MgVideoUserBuffer *keyring, *recordInput, *input;
	int result;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	keyring = arguments.keyring;
	recordInput = arguments.recordInput;
	input = arguments.input;
	if (keyring) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, keyring, sizeof(*copy));
		keyring = copy;
		if (result < 0)
			return result;
	}
	if (recordInput) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, recordInput, sizeof(*copy));
		recordInput = copy;
		if (result < 0)
			return result;
	}
	if (input) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, input, sizeof(*copy));
		input = copy;
		if (result < 0)
			return result;
	}
	return openUserVideoRecordStream(handle, index, blockNumber, keyring,
					 recordInput, input);
}

int SceMgVideo_8A803FB4(int handle)
{
	return closeUserVideoRecordStream(handle);
}

int SceMgVideo_3F3E3A8E(int handle, const MgVideoBoxHeader *input, int size,
		      const MgVideoCipherOpenArguments *inputArguments)
{
	MgVideoCipherOpenArguments arguments;
	int result;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	if (input) {
		void *copy;

		if (arguments.copySize > 1024)
			return (int)0x8002710b;
		copy = __builtin_alloca(1024);
		result = sceKernelCopyFromUser(copy, input, arguments.copySize);
		if (result < 0)
			return result;
		input = copy;
	}
	return openUserVideoCipherChannel(handle, input, size);
}

int SceMgVideo_553C65CB(int handle)
{
	return closeUserVideoCipherChannel(handle);
}

int SceMgVideo_CD12E1AC(int handle, unsigned int selection)
{
	return isUserVideoCipherKeySelected(handle, selection);
}

int SceMgVideo_5A62101E(unsigned int handle, void *output,
		      const void *input, unsigned int size)
{
	return decryptUserVideoCipherBuffer(handle, output, input, size);
}

int SceMgVideo_1F9409D1(unsigned int handle, void *output, const void *input,
		      const MgVideoCipherStreamArguments *inputArguments)
{
	MgVideoCipherStreamArguments arguments;
	unsigned int size, totalSize, *processed;
	int result;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	size = arguments.size;
	totalSize = arguments.totalSize;
	processed = arguments.processed;
	if (processed) {
		unsigned int *copy = __builtin_alloca(sizeof(*copy));
		int cipherResult;

		*copy = 0;
		result = sceKernelCopyFromUser(copy, processed, sizeof(*copy));
		if (result < 0)
			return result;
		cipherResult = decryptUserVideoCipherStream(handle, output, input,
							  size, totalSize, copy);
		result = sceKernelCopyToUser(processed, copy, sizeof(*copy));
		if (result < 0)
			return result;
		return cipherResult;
	}
	return decryptUserVideoCipherStream(handle, output, input, size, totalSize, 0);
}

int SceMgVideo_ECB4B61C(unsigned int handle, void *output, const void *input,
		      const MgVideoCipherBufferArguments *inputArguments)
{
	MgVideoCipherBufferArguments arguments;
	int result;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	return encryptUserVideoCipherBuffer(handle, output, input, arguments.size,
					    arguments.blockIndex);
}

int SceMgVideo_A4B68884(unsigned int handle, void *output, const void *input,
		      const MgVideoCipherEncryptArguments *inputArguments)
{
	MgVideoCipherEncryptArguments arguments;
	unsigned int size, totalSize, *processed;
	uint32_t blockIndex;
	int result;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	size = arguments.size;
	totalSize = arguments.totalSize;
	processed = arguments.processed;
	blockIndex = arguments.blockIndex;
	if (processed) {
		unsigned int *copy = __builtin_alloca(sizeof(*copy));
		int cipherResult;

		*copy = 0;
		result = sceKernelCopyFromUser(copy, processed, sizeof(*copy));
		if (result < 0)
			return result;
		cipherResult = encryptUserVideoCipherStream(handle, output, input,
						 size, totalSize, copy, blockIndex);
		result = sceKernelCopyToUser(processed, copy, sizeof(*copy));
		if (result < 0)
			return result;
		return cipherResult;
	}
	return encryptUserVideoCipherStream(handle, output, input, size, totalSize,
					    0, blockIndex);
}

int SceMgVideo_0F3F58E3(int handle, unsigned int index,
		      const MgVideoUserBuffer *recordInput,
		      const MgVideoBlockRemoveArguments *inputArguments)
{
	MgVideoBlockRemoveArguments arguments;
	const int16_t *blockNumbers;
	const MgVideoUserBuffer *input;
	int blockCount, result;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	blockNumbers = arguments.blockNumbers;
	blockCount = arguments.blockCount;
	input = arguments.input;
	if (recordInput) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, recordInput, sizeof(*copy));
		recordInput = copy;
		if (result < 0)
			return result;
	}
	if (blockNumbers) {
		int16_t *copy;

		if (arguments.copySize > 1024)
			return (int)0x8002710b;
		copy = __builtin_alloca(1024);
		result = sceKernelCopyFromUser(copy, blockNumbers, arguments.copySize);
		blockNumbers = copy;
		if (result < 0)
			return result;
	}
	if (input) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, input, sizeof(*copy));
		input = copy;
		if (result < 0)
			return result;
	}
	return removeVideoRecordBlocks(handle, index, recordInput, blockNumbers,
				       blockCount, input);
}

int SceMgVideo_F46B72E6(int handle)
{
	return saveUserVideoKeyring(handle);
}

int SceMgVideo_A271BBED(int handle)
{
	return resetUserVideoKeyring(handle);
}

#if FW_VERSION >= 0x03650000 && FW_VERSION < 0x04000000
int SceMgVideo_5B57B197(int handle, const MagicGateFileIdentity *input)
{
	return SceMgVideo_22D74C37(handle, input);
}
#endif

int SceMgVideo_0F6359B1(int handle, void *output, unsigned int outputSize,
		      const MgVideoPlaybackQueryArguments *inputArguments)
{
	MgVideoPlaybackQueryArguments arguments;
	const MgVideoUserBuffer *input;
	void *copy = 0;
	int mode, result, queryResult;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	input = arguments.input;
	mode = arguments.mode;
	if (output) {
		if (arguments.copySize > 64)
			return (int)0x8002710b;
		copy = __builtin_alloca(64);
		memset(copy, 0, 64);
	}
	if (input) {
		MgVideoUserBuffer *inputCopy = __builtin_alloca(sizeof(*inputCopy));

		result = sceKernelCopyFromUser(inputCopy, input, sizeof(*inputCopy));
		input = inputCopy;
		if (result < 0)
			return result;
	}
	queryResult = queryUserVideoPlaybackInfo(handle, copy, outputSize, input, mode);
	if (output) {
		result = sceKernelCopyToUser(output, copy, arguments.copySize);
		if (result < 0)
			return result;
	}
	return queryResult;
}

int SceMgVideo_FBF16BC5(int handle, const MgVideoUserBuffer *output)
{
	if (output) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));
		int result;

		result = sceKernelCopyFromUser(copy, output, sizeof(*copy));
		if (result < 0)
			return result;
		output = copy;
	}
	return exportUserVideoEkb2Record(handle, output);
}

int SceMgVideo_ACD50568(int handle, const MgVideoUserBuffer *output,
		      MgVideoGeneratedProtectionInfo *protectionOutput,
		      const MgVideoKeyringCreateArguments *inputArguments)
{
	MgVideoKeyringCreateArguments arguments;
	const MgVideoAvOut *avOut;
	const MgVideoKeyGenerationParameters *parameters;
	MgVideoGeneratedProtectionInfo *protection = 0;
	int mode, result, keyringResult;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	avOut = arguments.avOut;
	parameters = arguments.parameters;
	mode = arguments.mode;
	if (output) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, output, sizeof(*copy));
		output = copy;
		if (result < 0)
			return result;
	}
	if (protectionOutput) {
		protection = __builtin_alloca(sizeof(*protection));
		memset(protection, 0, sizeof(*protection));
	}
	if (avOut) {
		MgVideoAvOut *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, avOut, sizeof(*copy));
		avOut = copy;
		if (result < 0)
			return result;
	}
	if (parameters) {
		MgVideoKeyGenerationParameters *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, parameters, sizeof(*copy));
		parameters = copy;
		if (result < 0)
			return result;
	}
	keyringResult = createUserVideoKeyring(handle, output, protection, avOut,
					      parameters, mode);
	if (protectionOutput) {
		result = sceKernelCopyToUser(protectionOutput, protection,
					     sizeof(*protection));
		if (result < 0)
			return result;
	}
	return keyringResult;
}

int SceMgVideo_74907E1E(int handle, unsigned int index, int blockNumber,
		      const MgVideoKeyringReplaceArguments *inputArguments)
{
	MgVideoKeyringReplaceArguments arguments;
	const MgVideoUserBuffer *keyring, *input, *recordInput, *replacementInput;
	int result;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	keyring = arguments.keyring;
	input = arguments.input;
	recordInput = arguments.recordInput;
	replacementInput = arguments.replacementInput;
	if (keyring) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, keyring, sizeof(*copy));
		keyring = copy;
		if (result < 0)
			return result;
	}
	if (input) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, input, sizeof(*copy));
		input = copy;
		if (result < 0)
			return result;
	}
	if (recordInput) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, recordInput, sizeof(*copy));
		recordInput = copy;
		if (result < 0)
			return result;
	}
	if (replacementInput) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, replacementInput, sizeof(*copy));
		replacementInput = copy;
		if (result < 0)
			return result;
	}
	return replaceVideoKeyring(handle, index, blockNumber, keyring, input,
				   recordInput, replacementInput);
}

int SceMgVideo_03F7BC67(int handle, unsigned int index, int blockNumber,
		      const MgVideoPlaybackUpdateArguments *inputArguments)
{
	MgVideoPlaybackUpdateArguments arguments;
	const MgVideoUserBuffer *keyring, *input, *recordInput, *replacementInput;
	const MgVideoUserBuffer *optionInput;
	const void *extensionInput;
	int result;

	result = sceKernelCopyFromUser(&arguments, inputArguments, sizeof(arguments));
	if (result < 0)
		return result;
	keyring = arguments.keyring;
	input = arguments.input;
	recordInput = arguments.recordInput;
	replacementInput = arguments.replacementInput;
	optionInput = arguments.optionInput;
	extensionInput = arguments.extensionInput;
	if (keyring) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, keyring, sizeof(*copy));
		keyring = copy;
		if (result < 0)
			return result;
	}
	if (input) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, input, sizeof(*copy));
		input = copy;
		if (result < 0)
			return result;
	}
	if (recordInput) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, recordInput, sizeof(*copy));
		recordInput = copy;
		if (result < 0)
			return result;
	}
	if (replacementInput) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, replacementInput, sizeof(*copy));
		replacementInput = copy;
		if (result < 0)
			return result;
	}
	if (optionInput) {
		MgVideoUserBuffer *copy = __builtin_alloca(sizeof(*copy));

		result = sceKernelCopyFromUser(copy, optionInput, sizeof(*copy));
		optionInput = copy;
		if (result < 0)
			return result;
	}
	if (extensionInput) {
		void *copy = __builtin_alloca(16);

		result = sceKernelCopyFromUser(copy, extensionInput, 16);
		extensionInput = copy;
		if (result < 0)
			return result;
	}
	return updateVideoPlaybackPeriod(handle, index, blockNumber, keyring, input,
					 recordInput, replacementInput, optionInput,
					 extensionInput);
}

#if FW_VERSION < 0x03650000 || FW_VERSION >= 0x04000000
int SceMgVideoDummy_29119904(void)
{
	return (int)0x8050c001;
}
#endif
