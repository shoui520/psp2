/*
 * SpkgGroupManager (spkg_group_manager.cpp): the system packages of a PUP, grouped, and the
 * update or inspection of them all. The class name is ours.
 */
#ifndef UPDATER_CORE_SPKG_GROUP_MANAGER_H
#define UPDATER_CORE_SPKG_GROUP_MANAGER_H

#include <scetypes.h>

class SpkgGroupManager {
public:
	SpkgGroupManager(int reserved);
	virtual ~SpkgGroupManager();

	void Terminate(void);
	int UpdateAllSpkg(const char *fileName);
	int InspectAllSpackage(const char *fileName);
	int GetSpackageHeader(void *header, int type);
	int GetProgress(void *progress);
	int GetEstimateProcessTime(const char *fileName, int *time);
	int CheckAcceptableSpkgType(const char *fileName, const int *types, int count, int *result);
	int GetResult(void);
	int GetProgressRate(void);

private:
	SceUInt8 m_state[348];	/* laid out with spkg_group_manager.cpp (0x160 bytes in all) */
};

#endif
