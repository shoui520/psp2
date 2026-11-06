/* PartitionManager (partition_manager.cpp): the update's working partition. The class name is ours. */
#ifndef UPDATER_CORE_PARTITION_MANAGER_H
#define UPDATER_CORE_PARTITION_MANAGER_H

class PartitionManager {
public:
	PartitionManager();
	~PartitionManager();

	const char *GetWorkingDirectory(void);
	int PartitionSetUp(void);
	int CleanUp(void);
	const char *GetPupPath(void);
};

#endif
