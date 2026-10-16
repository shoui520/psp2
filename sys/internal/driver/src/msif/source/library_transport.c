/* Transport helpers; source-file names absent from debug records. */
#include "library.h"
#include <string.h>

int MsproalReadMediaRegisters(struct MsproalHandle *handle, int address,
	int count, void *registers)
{
	int result;

	if ((unsigned int)(count - 1) > 255)
		return -1;
	result = MsproalSetRegisterWindow(handle, address, count, 16, 15);
	if (!result)
		return MsproalReadRegisters(handle, count, registers);
	return result;
}

int MsproalResetController(struct MsproalHandle *handle)
{
	int result;

	result = MsproalResetSequencer(handle);
	if (!result)
		return MsproalResetTransferEngine(handle);
	return result;
}

int MsproalExecuteCommand(struct MsproalHandle *handle, int command, int timeout)
{
	uint8_t interrupt;
	int result;

	result = MsproalSendCommand(handle, command);
	if (result || command == 60)
		return result;
	result = MsproalWaitTransfer(handle, timeout, 0);
	if (result)
		return result == -4 ? -3 : result;
	if (handle->busMode == 1) {
		result = MsproalGetInterrupt(handle, &interrupt);
		if (result)
			return result;
	} else {
		interrupt = handle->interruptFlags;
	}
	if (interrupt & 0x41)
		return interrupt & 1 ? -105 : -106;
	return interrupt & 0xa0 ? 0 : -4;
}

int MsproalWriteMediaRegisters(struct MsproalHandle *handle, int address,
	int count, const void *registers)
{
	int result;

	if ((unsigned int)(count - 1) > 255)
		return -1;
	result = MsproalSetRegisterWindow(handle, 2, 1, address, count);
	if (!result)
		return MsproalWriteRegisters(handle, count, registers);
	return result;
}

int MsproalWaitTransferComplete(struct MsproalHandle *handle, int timeout)
{
	int result;

	if (MsproalUserWaitEventFlag(0x80001300, timeout))
		return -4;
	msproal_user_clear_flg(0x7fffacff);
	if ((int32_t)handle->interruptFlags < 0) {
		handle->registers->transferControl |= 0x8000;
		MsproalTimerStart(1);
		while (handle->registers->transferControl & 0x8000) {
			if (MsproalTimerCheck())
				return -4;
		}
		MsproalTimerStop();
		handle->registers->transferControl =
			(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
		return -12;
	}
	result = handle->interruptFlags & 0x300;
	if (result)
		return handle->interruptFlags & 0x100 ? -103 : -102;
	return result;
}

int MsproalSetHostBusMode(struct MsproalHandle *handle, int busMode)
{
	struct MsifRegisterWindow *registers = handle->registers;
	uint32_t control = registers->transferControl;

	if (busMode == 1)
		control = (control & ~0x40) | 0x80;
	else if (busMode == 5)
		control = (control & ~0x80) | 0x40;
	else
		control &= 0xff3f;
	registers->transferControl = control;
	MsproalUserSetBusClock(busMode);
	return 0;
}

int MsproalSendExtendedCommand(struct MsproalHandle *handle,
	int command, uint32_t sector, int count)
{
	union MsproalFifoData data;
	int retries = 4;
	int result;

	data.extendedCommand.count[1] = count;
	data.extendedCommand.count[0] = count >> 8;
	data.extendedCommand.sector[3] = sector;
	data.extendedCommand.command = command;
	data.extendedCommand.sector[1] = sector >> 16;
	data.extendedCommand.sector[0] = sector >> 24;
	data.extendedCommand.sector[2] = sector >> 8;
	data.extendedCommand.reserved = 0;
	do {
		handle->interruptFlags = 0;
		if (!MsproalUserPollEventFlag(0x80000000)) {
			msproal_user_clear_flg(0x7fffffff);
			return -12;
		}
		msproal_user_clear_flg(0xbfff0000);
		handle->registers->transferCommand = 0x9007;
		MsproalTimerStart(1);
		while (!(handle->registers->interruptStatus & 0x4000)) {
			if (!MsproalUserPollEventFlag(0x80000000)) {
				msproal_user_clear_flg(0x7fffffff);
				goto cancelled;
			}
			if (MsproalTimerCheck())
				return -4;
		}
		handle->registers->transferFifo = data.words[0];
		handle->registers->transferFifo = data.words[1];
		MsproalTimerStop();
		if (MsproalUserWaitEventFlag(0x80001300, 1))
			return -4;
		msproal_user_clear_flg(0x7fffacff);
		if ((int32_t)handle->interruptFlags < 0)
			goto cancelled;
		result = handle->interruptFlags & 0x300;
		if (!result)
			return result;
		if (!(handle->interruptFlags & 0x100))
			return -102;
	} while (--retries);
	return -3;

cancelled:
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalReadFifoPair(struct MsproalHandle *handle, uint32_t *data)
{
	while (!(handle->registers->interruptStatus & 0x4000)) {
		if (!MsproalUserPollEventFlag(0x80000100))
			goto interrupted;
		if (MsproalTimerCheck())
			return -4;
	}
	data[0] = handle->registers->transferFifo;
	data[1] = handle->registers->transferFifo;
	return 0;

interrupted:
	msproal_user_clear_flg(0x7fffeeff);
	if ((int32_t)handle->interruptFlags >= 0)
		return -103;
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalGetInterrupt(struct MsproalHandle *handle, uint8_t *interrupt)
{
	union MsproalFifoData data;
	int retries = 4;
	int result;

	do {
		handle->interruptFlags = 0;
		if (!MsproalUserPollEventFlag(0x80000000)) {
			msproal_user_clear_flg(0x7fffffff);
			return -12;
		}
		msproal_user_clear_flg(0xbfff0000);
		handle->registers->transferCommand = 0x7001;
		MsproalTimerStart(1);
		result = MsproalReadFifoPair(handle, data.words);
		if (result) {
			MsproalTimerStop();
			if (result != -103)
				return result;
		} else {
			*interrupt = data.bytes[0];
			MsproalTimerStop();
			if (MsproalUserWaitEventFlag(0x80001300, 1))
				return -4;
			msproal_user_clear_flg(0x7fffacff);
			if ((int32_t)handle->interruptFlags < 0)
				goto cancelled;
			result = handle->interruptFlags & 0x300;
			if (!result)
				return result;
			if (handle->interruptFlags & 0x100)
				return -103;
		}
	} while (--retries);
	return -3;

cancelled:
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalReadFifo(struct MsproalHandle *handle, int count, void *data)
{
	uint32_t *cursor = data;
	uint32_t *end = (uint32_t *)((uintptr_t)data + ((uint32_t)count & ~7u));
	uint32_t remainder[2];
	int result;

	while ((uintptr_t)cursor < (uintptr_t)end) {
		while (!(handle->registers->interruptStatus & 0x4000)) {
			if (!MsproalUserPollEventFlag(0x80000100))
				goto interrupted;
			if (MsproalTimerCheck())
				return -4;
		}
		cursor[0] = handle->registers->transferFifo;
		__builtin_prefetch(cursor + 8);
		if ((uintptr_t)end <= (uintptr_t)(cursor + 1))
			return -101;
		cursor += 2;
		cursor[-1] = handle->registers->transferFifo;
	}
	if (count & 7) {
		result = MsproalReadFifoPair(handle, remainder);
		if (result)
			return result;
		memcpy(cursor, remainder, count & 7);
	}
	return 0;

interrupted:
	msproal_user_clear_flg(0x7fffeeff);
	if ((int32_t)handle->interruptFlags >= 0)
		return -103;
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalReadLongData(struct MsproalHandle *handle, void *data)
{
	uint32_t *cursor;
	uint32_t *end = (uint32_t *)data + 138;
	int retries = 4;
	int result;

retry:
	handle->interruptFlags = 0;
	if (!MsproalUserPollEventFlag(0x80000000)) {
		msproal_user_clear_flg(0x7fffffff);
		return -12;
	}
	cursor = (uint32_t *)data + 10;
	msproal_user_clear_flg(0xbfff0000);
	handle->registers->transferCommand = 0x2200;
	MsproalTimerStart(5);
	do {
		uint32_t first;

		while (!(handle->registers->interruptStatus & 0x4000)) {
			if (!MsproalUserPollEventFlag(0x80000100))
				goto interrupted;
			if (MsproalTimerCheck())
				goto timedOut;
		}
		first = handle->registers->transferFifo;
		__builtin_prefetch(cursor);
		cursor += 2;
		cursor[-12] = first;
		cursor[-11] = handle->registers->transferFifo;
	} while (cursor != end);
	MsproalTimerStop();
	result = MsproalWaitTransferComplete(handle, 1);
	return result == -102 ? -3 : result;

interrupted:
	msproal_user_clear_flg(0x7fffeeff);
	if ((int32_t)handle->interruptFlags >= 0) {
		MsproalTimerStop();
		if (--retries)
			goto retry;
		return -3;
	}
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			goto timedOut;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	result = -12;
	goto finish;

timedOut:
	result = -4;
finish:
	MsproalTimerStop();
	return result;
}

int MsproalReadRegisters(struct MsproalHandle *handle, int count, void *registers)
{
	uint16_t command = (count & 0x7ff) | 0x4000;
	int retries = 4;
	int result;

	do {
		handle->interruptFlags = 0;
		if (!MsproalUserPollEventFlag(0x80000000)) {
			msproal_user_clear_flg(0x7fffffff);
			return -12;
		}
		msproal_user_clear_flg(0xbfff0000);
		handle->registers->transferCommand = command;
		MsproalTimerStart(5);
		result = MsproalReadFifo(handle, count, registers);
		MsproalTimerStop();
		if (result) {
			if (result != -103)
				return result;
		} else {
			if (MsproalUserWaitEventFlag(0x80001300, 1))
				return -4;
			msproal_user_clear_flg(0x7fffacff);
			if ((int32_t)handle->interruptFlags < 0)
				goto cancelled;
			result = handle->interruptFlags & 0x300;
			if (!result)
				return result;
			if (handle->interruptFlags & 0x100)
				return -103;
		}
	} while (--retries);
	return -3;

cancelled:
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalReadShortData(struct MsproalHandle *handle, int count, void *data)
{
	uint16_t command = (count & 0x7ff) | 0x3000;
	int retries = 4;
	int result;

	for (;;) {
		handle->interruptFlags = 0;
		if (!MsproalUserPollEventFlag(0x80000000)) {
			msproal_user_clear_flg(0x7fffffff);
			return -12;
		}
		msproal_user_clear_flg(0xbfff0000);
		handle->registers->transferCommand = command;
		MsproalTimerStart(1);
		result = MsproalReadFifo(handle, count, data);
		MsproalTimerStop();
		if (!result)
			break;
		if (result != -103)
			return result;
		if (!--retries)
			return -3;
	}
	result = MsproalWaitTransferComplete(handle, 1);
	if (result)
		return result == -102 ? -3 : result;
	return 0;
}

int MsproalResetTransferEngine(struct MsproalHandle *handle)
{
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return 0;
}

int MsproalSetRegisterWindow(struct MsproalHandle *handle,
	int readAddress, int readCount, int writeAddress, int writeCount)
{
	union MsproalFifoData data;
	int retries = 4;
	int result;

	data.registerWindow.writeAddress = writeAddress;
	data.registerWindow.readAddress = readAddress;
	data.registerWindow.readCount = readCount;
	data.registerWindow.writeCount = writeCount;
	data.registerWindow.reserved[0] = 0;
	data.registerWindow.reserved[1] = 0;
	data.registerWindow.reserved[2] = 0;
	data.registerWindow.reserved[3] = 0;
	do {
		handle->interruptFlags = 0;
		if (!MsproalUserPollEventFlag(0x80000000)) {
			msproal_user_clear_flg(0x7fffffff);
			return -12;
		}
		msproal_user_clear_flg(0xbfff0000);
		handle->registers->transferCommand = 0x8004;
		MsproalTimerStart(1);
		while (!(handle->registers->interruptStatus & 0x4000)) {
			if (!MsproalUserPollEventFlag(0x80000000)) {
				msproal_user_clear_flg(0x7fffffff);
				goto cancelled;
			}
			if (MsproalTimerCheck())
				return -4;
		}
		handle->registers->transferFifo = data.words[0];
		handle->registers->transferFifo = data.words[1];
		MsproalTimerStop();
		if (MsproalUserWaitEventFlag(0x80001300, 1))
			return -4;
		msproal_user_clear_flg(0x7fffacff);
		if ((int32_t)handle->interruptFlags < 0)
			goto cancelled;
		result = handle->interruptFlags & 0x300;
		if (!result)
			return result;
		if (!(handle->interruptFlags & 0x100))
			return -102;
	} while (--retries);
	return -3;

cancelled:
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalWaitTransfer(struct MsproalHandle *handle, int timeout, int mode)
{
	int result;

	if (mode == 1) {
		if (MsproalUserWaitEventFlag(0x80001300, timeout))
			return -4;
		msproal_user_clear_flg(0x7fffacff);
		if ((int32_t)handle->interruptFlags < 0) {
			handle->registers->transferControl |= 0x8000;
			MsproalTimerStart(1);
			while (handle->registers->transferControl & 0x8000) {
				if (MsproalTimerCheck())
					return -4;
			}
			MsproalTimerStop();
			handle->registers->transferControl =
				(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
			return -12;
		}
		result = handle->interruptFlags & 0x300;
		if (result)
			return handle->interruptFlags & 0x100 ? -103 : -102;
		return result;
	}
	if (mode == 2) {
		MsproalUserWaitMilliseconds(timeout);
		return 0;
	}
	if (mode != 0)
		return -1;
	if (MsproalUserWaitEventFlag(0x80002000, timeout))
		return -4;
	msproal_user_clear_flg(0x7fffdf00);
	return (int32_t)handle->interruptFlags < 0 ? -12 : 0;
}

int MsproalWriteFifoPair(struct MsproalHandle *handle, const uint32_t *data)
{
	while (!(handle->registers->interruptStatus & 0x4000)) {
		if (!MsproalUserPollEventFlag(0x80000000))
			goto cancelled;
		if (MsproalTimerCheck())
			return -4;
	}
	handle->registers->transferFifo = data[0];
	handle->registers->transferFifo = data[1];
	return 0;

cancelled:
	msproal_user_clear_flg(0x7fffffff);
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalSendCommand(struct MsproalHandle *handle, int command)
{
	union MsproalFifoData data;
	int retries = 4;
	int result;

	do {
		handle->interruptFlags = 0;
		if (!MsproalUserPollEventFlag(0x80000000)) {
			msproal_user_clear_flg(0x7fffffff);
			return -12;
		}
		msproal_user_clear_flg(0xbfff0000);
		handle->registers->transferCommand = 0xe001;
		MsproalTimerStart(1);
		data.bytes[0] = command;
		data.bytes[1] = 0;
		data.bytes[2] = 0;
		data.bytes[3] = 0;
		data.bytes[4] = 0;
		data.bytes[5] = 0;
		data.bytes[6] = 0;
		data.bytes[7] = 0;
		result = MsproalWriteFifoPair(handle, data.words);
		if (result)
			return result;
		MsproalTimerStop();
		if (MsproalUserWaitEventFlag(0x80001300, 1))
			return -4;
		msproal_user_clear_flg(0x7fffacff);
		if ((int32_t)handle->interruptFlags < 0)
			goto cancelled;
		result = handle->interruptFlags & 0x300;
		if (!result)
			return result;
		if (!(handle->interruptFlags & 0x100))
			return -102;
	} while (--retries);
	return -3;

cancelled:
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalWriteFifo(struct MsproalHandle *handle, int count, const void *data)
{
	const uint32_t *cursor = data;
	const uint32_t *end = (const uint32_t *)
		((uintptr_t)data + ((uint32_t)count & ~7u));
	union MsproalFifoData remainder;
	unsigned int tail = count & 7;

	while ((uintptr_t)cursor < (uintptr_t)end) {
		uint32_t first;

		while (!(handle->registers->interruptStatus & 0x4000)) {
			if (!MsproalUserPollEventFlag(0x80000000))
				goto cancelled;
			if (MsproalTimerCheck())
				return -4;
		}
		first = cursor[0];
		__builtin_prefetch(cursor + 10);
		cursor += 2;
		handle->registers->transferFifo = first;
		handle->registers->transferFifo = cursor[-1];
	}
	if (!tail)
		return 0;
	memcpy(remainder.bytes, cursor, tail);
	memset(remainder.bytes + tail, 0, sizeof(remainder.bytes) - tail);
	return MsproalWriteFifoPair(handle, remainder.words);

cancelled:
	msproal_user_clear_flg(0x7fffffff);
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalWriteRegisters(struct MsproalHandle *handle, int count,
	const void *registers)
{
	uint16_t command = (count & 0x7ff) | 0xb000;
	int retries = 4;
	int result;

	do {
		handle->interruptFlags = 0;
		if (!MsproalUserPollEventFlag(0x80000000)) {
			msproal_user_clear_flg(0x7fffffff);
			return -12;
		}
		msproal_user_clear_flg(0xbfff0000);
		handle->registers->transferCommand = command;
		MsproalTimerStart(10);
		result = MsproalWriteFifo(handle, count, registers);
		if (result)
			return result;
		MsproalTimerStop();
		if (MsproalUserWaitEventFlag(0x80001300, 2))
			return -4;
		msproal_user_clear_flg(0x7fffacff);
		if ((int32_t)handle->interruptFlags < 0)
			goto cancelled;
		result = handle->interruptFlags & 0x300;
		if (!result)
			return result;
		if (!(handle->interruptFlags & 0x100))
			return -102;
	} while (--retries);
	return -3;

cancelled:
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}

int MsproalWriteShortData(struct MsproalHandle *handle, int count, const void *data)
{
	uint16_t command = (count & 0x7ff) | 0xc000;
	int retries = 4;
	int result;

	do {
		handle->interruptFlags = 0;
		if (!MsproalUserPollEventFlag(0x80000000)) {
			msproal_user_clear_flg(0x7fffffff);
			return -12;
		}
		msproal_user_clear_flg(0xbfff0000);
		handle->registers->transferCommand = command;
		MsproalTimerStart(1);
		result = MsproalWriteFifo(handle, count, data);
		if (result)
			return result;
		MsproalTimerStop();
		if (MsproalUserWaitEventFlag(0x80001300, 1))
			return -4;
		msproal_user_clear_flg(0x7fffacff);
		if ((int32_t)handle->interruptFlags < 0)
			goto cancelled;
		result = handle->interruptFlags & 0x300;
		if (!result)
			return result;
		if (!(handle->interruptFlags & 0x100))
			return -102;
	} while (--retries);
	return -3;

cancelled:
	handle->registers->transferControl |= 0x8000;
	MsproalTimerStart(1);
	while (handle->registers->transferControl & 0x8000) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->transferControl =
		(handle->registers->transferControl & 0xfff8) | 0x4000 | 5;
	return -12;
}
