/*
 * shell_cron.cpp: the shell's cron, started and stopped (0x810FB5D4-0x810FB68C): the one
 * instance, and the interface's constructor and destructor. The class's code is
 * shell_cron_impl.cpp. The file's and the functions' names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>

#include "shell.h"
#include "shell_cron.h"
#include "shell_cron_impl.h"

/* (0x81544780) the cron, once started */
static ShellCron *s_instance = NULL;

/* (0x81559678) */
static ShellCronImpl s_cron;

/* (0x810FB5D4) */
int shellCronStart(void)
{
	int ret;

	if (s_instance != NULL)
		return SHELL_CRON_ERROR_ALREADY_STARTED;
	ret = s_cron.Start();
	if (ret == 0)
		s_instance = &s_cron;
	return ret;
}

/* (0x810FB612) */
int shellCronStop(void)
{
	if (s_instance == NULL)
		return 0;
	return s_cron.Stop();
}

/* (0x810FB632) */
ShellCron *shellCronGetInstance(void)
{
	return s_instance;
}

/* (0x810FB640) */
ShellCron::ShellCron()
{
}

/* (0x810FB640, 0x810FB650) */
ShellCron::~ShellCron()
{
}

