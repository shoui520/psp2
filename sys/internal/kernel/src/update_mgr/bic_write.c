#include <psp2kern/kernel/sysclib.h>
#include "spkg_peripherals.h"
#include "bic_update.h"
#include <syscon/syscon.h>

int UpdateUtilUpdateNodeStatus(unsigned int nodeType,
		const UpdateUtilNodeStatus *status);

int BicUtilWriteFirmware(const void *buffer, uint64_t size, uint32_t version,
		unsigned int nodeType, UpdateUtilNodeStatus *status)
{
	BicUpdateCommand command;
	BicUpdateToken token;
	uint8_t compareData[98];
	const uint8_t *line;
	uint32_t totalSize = size;
	uint32_t consumed, remaining, position;
	uint32_t tokenCount, tokenLength;
	uint32_t cmdNo, cmdOff, rest, unit_size;
	int result, value;

	(void)version;
	if (!updateUseBatteryService)
		return (int)0x800f0202;
	result = ksceSysconBatteryStartBLMode();
	if (result != (int)0x80250222 && result)
		return (int)0x800f0205;
	if (totalSize) {
		line = buffer;
		if (!line)
			return (int)0x800f0216;
		remaining = totalSize;
		consumed = 0;
		cmdNo = 1;
		for (;;) {
			memset(&command, 0, sizeof(command));
			if (line[0] == 'X')
				command.type = 2;
			else if (line[0] == 'C')
				command.type = 1;
			else if (line[0] != 'W')
				return (int)0x800f0224;
			if (line[1] != ':')
				return (int)0x800f0224;
			position = 2;
			if (remaining > 2) {
				tokenCount = 0;
				tokenLength = 0;
				for (;;) {
					uint8_t character = line[position];

					if (character == ' ') {
						if (tokenLength) {
							if (command.type == 2) {
								command.delayMilliseconds =
									BicUtilStringToInt(token.text, 10);
							} else if (command.type) {
								value = BicUtilStringToInt(token.text, 16);
								if (!tokenCount) {
									command.compareAddress = value;
									tokenCount = 1;
								} else if (tokenCount == 1) {
									command.compareOffset = value;
									tokenCount = 2;
								} else {
									command.data[tokenCount - 2] = value;
									command.size = tokenCount - 1;
									tokenCount++;
								}
							} else {
								command.data[tokenCount++] =
									BicUtilStringToInt(token.text, 16);
								command.size = tokenCount;
							}
						}
						token.cleared = 0;
						tokenLength = 0;
					} else if (look_ctype_table(character) & 0x44) {
						position++;
						token.text[tokenLength++] = character;
						if (position == remaining)
							break;
						continue;
					} else if (character == '\n' && tokenLength) {
						if (command.type == 2) {
							command.delayMilliseconds =
								BicUtilStringToInt(token.text, 10);
						} else if (command.type) {
							value = BicUtilStringToInt(token.text, 16);
							if (!tokenCount)
								command.compareAddress = value;
							else if (tokenCount == 1)
								command.compareOffset = value;
							else {
								command.data[tokenCount - 2] = value;
								command.size = tokenCount - 1;
							}
						} else {
							command.data[tokenCount] =
								BicUtilStringToInt(token.text, 16);
							command.size = tokenCount + 1;
						}
						position++;
						break;
					}
					if (++position == remaining)
						break;
				}
			}
			if (command.type == 1) {
				memset(compareData, 0, sizeof(compareData));
				if (ksceSysconBatteryReadBLCommand(cmdNo, command.compareAddress,
						command.compareOffset, compareData, command.size) ||
				    memcmp(compareData, command.data, command.size))
					return (int)0x800f0205;
				cmdNo++;
			} else if (command.type == 2) {
				cmdNo++;
				sceKernelDelayThread(1000 * command.delayMilliseconds);
			} else if (!command.type) {
				rest = command.size;
				for (cmdOff = 0; rest; cmdOff += unit_size) {
					unit_size = rest >= 16 ? 16 : rest;
					if (ksceSysconBatterySetBLCommand(cmdNo, cmdOff,
							&command.data[cmdOff], unit_size))
						return (int)0x800f0205;
					rest -= unit_size;
				}
				if (ksceSysconBatteryExecBLCommand(cmdNo))
					return (int)0x800f0205;
				cmdNo++;
			}
			consumed += position;
			status->progress = 3;
			status->written_rates = 100 * consumed / totalSize;
			status->seq_no++;
			UpdateUtilUpdateNodeStatus(nodeType, status);
			if (totalSize <= consumed)
				break;
			line = (const uint8_t *)buffer + consumed;
			remaining = totalSize - consumed;
			if (!line || !remaining)
				return (int)0x800f0216;
		}
	}
	status->progress = 3;
	status->written_rates = 100;
	status->seq_no++;
	UpdateUtilUpdateNodeStatus(nodeType, status);
	if (ksceSysconBatteryStopBLMode())
		return (int)0x800f0205;
	BicUtilWaitCookieStatus();
	if (ksceSysconBatterySWReset())
		return (int)0x800f0205;
	BicUtilWaitCookieStatus();
	return 0;
}
