/*
 * The update's working directory on ud0: (Sony's partition_manager.cpp), where the PUP and the
 * updater it carries are put, made if missing and emptied after the update.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <updater_core/updater_core.h>
#include "updater_log.h"
#include "partition_manager.h"

#define UPDATER_FILE_ID		0x8d6cf84d

/* error codes (the names ours) */
#define SCE_UPDATER_ERROR_REMOVE	0x80180067
#define SCE_UPDATER_ERROR_MKDIR		0x801800a7

#define UPDATE_WORKING_DIRECTORY	"ud0:/PSP2UPDATE"
#define UPDATE_WORKING_DIRECTORY_MODE	0006
#define UPDATE_UPDATER_PATH		UPDATE_WORKING_DIRECTORY "/psp2swu.self"
#define UPDATE_PUP_PATH			UPDATE_WORKING_DIRECTORY "/PSP2UPDAT.PUP"

PartitionManager::PartitionManager()
{
}

PartitionManager::~PartitionManager()
{
}

const char *PartitionManager::GetWorkingDirectory(void)
{
	return UPDATE_WORKING_DIRECTORY;
}

/* makes the working directory unless it is there */
int PartitionManager::PartitionSetUp(void)
{
	SceIoStat stat;
	int ret;

	if (sceIoGetstat(UPDATE_WORKING_DIRECTORY, &stat) == 0)
		return 0;
	ret = sceIoMkdir(UPDATE_WORKING_DIRECTORY, UPDATE_WORKING_DIRECTORY_MODE);
	if (ret < 0) {
		UPDATER_ERROR_FMT(79, "%x", ret);
		return SCE_UPDATER_ERROR_MKDIR;
	}
	return 0;
}

const char *PartitionManager::GetUpdaterPath(void)
{
	return UPDATE_UPDATER_PATH;
}

const char *PartitionManager::GetPupPath(void)
{
	return UPDATE_PUP_PATH;
}

/* removes the PUP and the updater from the working directory */
int PartitionManager::CleanUp(void)
{
	SceIoStat stat;
	int ret;

	if (sceIoGetstat(UPDATE_PUP_PATH, &stat) >= 0) {
		ret = sceIoRemove(UPDATE_PUP_PATH);
		if (ret < 0) {
			UPDATER_ERROR_FMT(113, "%s : %x", UPDATE_PUP_PATH, ret);
			return SCE_UPDATER_ERROR_REMOVE;
		}
	}
	if (sceIoGetstat(UPDATE_UPDATER_PATH, &stat) >= 0) {
		ret = sceIoRemove(UPDATE_UPDATER_PATH);
		if (ret < 0) {
			UPDATER_ERROR_FMT(132, "%s : %x", UPDATE_UPDATER_PATH, ret);
			return SCE_UPDATER_ERROR_REMOVE;
		}
	}
	return 0;
}
