/*
 * console_info_send_data_job.cpp: the job the sender queues (0x810F2FA4-0x810F302C). The file's and
 * the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include "console_info.h"

using namespace paf;

/* (0x810F2FA4) */
ConsoleInfoSendDataJob::ConsoleInfoSendDataJob(int (*run)(int), int (*finish)(void), int arg)
	: JobItem("SceConsoleInfoSendDataJob", NULL), m_run(run), m_finish(finish), m_arg(arg)
{
}

/* (0x810F2FD8, 0x810F2FF4) */
ConsoleInfoSendDataJob::~ConsoleInfoSendDataJob()
{
}

/* (0x810F3014) */
int ConsoleInfoSendDataJob::Run()
{
	if (m_run != NULL)
		m_run(m_arg);
	return 0;
}

/* (0x810F3022) */
void ConsoleInfoSendDataJob::Finish(int result)
{
	if (m_finish != NULL)
		m_finish();
}
