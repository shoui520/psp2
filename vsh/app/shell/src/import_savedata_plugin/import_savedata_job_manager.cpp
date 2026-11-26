/*
 * import_savedata_job_manager.cpp: singleton job queue manager for
 * import_savedata_plugin (0x81396638..0x813968FB).
 */
#include "import_savedata_internal.h"

static ImportSaveDataJobManager *s_jobManagerInstance = NULL;	/* 0x81548D68 */

/* 0x81396638 */
ImportSaveDataJobManager::ImportSaveDataJobManager()
	: m_queue(NULL)
{
}

/* 0x8139689E / 0x813968EA */
ImportSaveDataJobManager::~ImportSaveDataJobManager()
{
	if (m_queue != NULL) {
		m_queue->CancelAllItems();
		delete m_queue;
		m_queue = NULL;
		m_activeJobs.clear();
	}
}

/* 0x81396676 */
ImportSaveDataJobManager *ImportSaveDataJobManager::Create(void)
{
	if (s_jobManagerInstance == NULL)
		s_jobManagerInstance = new ImportSaveDataJobManager();
	return s_jobManagerInstance;
}

/* 0x813966A2 */
void ImportSaveDataJobManager::Destroy(void)
{
	if (s_jobManagerInstance != NULL) {
		delete s_jobManagerInstance;
		s_jobManagerInstance = NULL;
	}
}

/* 0x813966C8 */
ImportSaveDataJobManager *ImportSaveDataJobManager::GetInstance(void)
{
	return s_jobManagerInstance;
}

/* 0x813966D4 */
void ImportSaveDataJobManager::CreateQueue(void)
{
	paf::job::JobQueue::Option opt;
	opt.priority = 0x10000100;
	opt.stackSize = 0x4000;
	opt.numThreads = 1;
	m_queue = new paf::job::JobQueue("SceImportSaveDataJobQueue", &opt);
}

/* 0x81396732 */
void ImportSaveDataJobManager::RemoveJob(SceInt32 key, bool cancelJob)
{
	paf::map<SceInt32, paf::job::JobItem *>::iterator it = m_activeJobs.find(key);
	if (it != m_activeJobs.end()) {
		if (cancelJob && it->second != NULL)
			it->second->Cancel();
		m_activeJobs.erase(it);
	}
}

/* 0x813967A8 */
void ImportSaveDataJobManager::EnqueueJob(SceInt32 key, paf::job::JobItem *job)
{
	if (m_queue == NULL)
		return;

	paf::common::SharedPtr<paf::job::JobItem> jobPtr(job);
	m_queue->Enqueue(jobPtr);
	RemoveJob(key, true);
	m_activeJobs.insert(paf::pair<SceInt32, paf::job::JobItem *>(key, job));
}

/* 0x81396882 */
bool ImportSaveDataJobManager::CancelAll(void)
{
	if (m_queue == NULL)
		return false;

	m_queue->CancelAllItems();
	m_activeJobs.clear();
	return true;
}
