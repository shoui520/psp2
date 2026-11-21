/*
 * download_plugin.h: the resident download_plugin (download_plugin.cpp), the background
 * downloads' front end, and the objects it makes and destroys. Its name for each is ours
 * (PLACEHOLDER) until their own files are written.
 */
#ifndef _DOWNLOAD_PLUGIN_H
#define _DOWNLOAD_PLUGIN_H

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>

/* the table given as the plugin's GetInterface(1) (PLACEHOLDER names) */
struct DownloadPluginInterface {
	int (*Notify)(void (*func)(void *), void *arg);		/* (0x811237C4) */
	int (*SetPermission)(void (*func)(void *), void *arg);	/* (0x811238C8) */
	int (*Bgdl_811239D4)(void);
	int (*Bgdl_811239DC)(void);
	int (*Terminate)(void);					/* (0x811239EC) */
	int (*Bgdl_811239E4)(void);
	int (*EventLogDownload)(SceInt64 rowid);		/* (0x81123DD0) */
	int (*Bgdl_81123ACA)(SceUInt32 taskId);
	int (*Reserved)(void);					/* (0x81409E2C) returns 0 */
};

/* the job a call through the interface queues (PLACEHOLDER names) */
class DownloadPluginApiJob : public paf::job::JobItem {
public:
	DownloadPluginApiJob(int type, void (*func)(void *), void *arg)
		: paf::job::JobItem("Job_DownloadPluginApi", NULL), m_type(type), m_reserved(0),
		  m_func(func), m_arg(arg)
	{
	}

	virtual ~DownloadPluginApiJob()				/* (0x8141678C) */
	{
	}

	virtual int Run();					/* (0x814167C8) */

	virtual void Finish(int result)				/* (0x814098DC) */
	{
	}

	int m_type;			/* 0 notify, 1 set the permission */
	int m_reserved;
	void (*m_func)(void *);		/* called when it is done */
	void *m_arg;
};

/* (0x8112F27C) a third object the plugin makes, 12 bytes (PLACEHOLDER) */
class DownloadPluginObject {
public:
	__attribute__((noinline)) DownloadPluginObject()
	{
		sce_paf_memset(this, 0, sizeof(*this));
	}

	__attribute__((noinline)) ~DownloadPluginObject()
	{
		sce_paf_memset(this, 0, sizeof(*this));
	}

	void Bgdl_8112F270(void);
	void Bgdl_8112F28C(void);

	int m_words[3];			/* PLACEHOLDER: not read here */
};

class BgdlJobQueue;

struct NewEventAccessor;

/* log_job_manager.cpp's (0x81123E54..): the event log's job manager, whose queue is
   "SceLogJobManagerJob" (PLACEHOLDER names) */
class LogJobManager {
public:
	LogJobManager(bool clean);					/* (0x81123E54) */
	~LogJobManager();						/* (0x81123F62) */
	void Bgdl_81123EF8(void);					/* stops it */
	int Bgdl_81123F7A(int *ids);
	int Bgdl_811240B6(int *ids);					/* starts it */
	void Bgdl_81126418(DownloadPluginObject **object, const int *reserved);

	BgdlJobQueue *m_queue;
	NewEventAccessor *m_accessor;	/* the event log's new events */
	int m_reserved;
	bool m_clean;
	bool m_mounted;			/* ux0: was there when it was made */
};

/* system_update_svc.cpp's service (PLACEHOLDER names) */
class SystemUpdateSvc {
public:
	static void Bgdl_8112CD1A(void);
	static void Bgdl_8112CDB2(void);
	static void Bgdl_8112D38C(void);
	static void Bgdl_8112D42A(void);
	void Bgdl_8112D508(paf::Plugin *plugin, DownloadPluginObject *object);
	void Bgdl_8112D7F0(void);
	void Bgdl_8112DA56(void);
	int Bgdl_81413CB0(bool flag);			/* folded with an inline "return 1" */
};

extern SystemUpdateSvc *g_systemUpdateSvc;		/* (0x81544898) PLACEHOLDER name */

int Bgdl_8112BB36(void);
int Bgdl_8112B9A2(void);
int Bgdl_8112BCEC(void);
int Bgdl_81123DD0(SceInt64 rowid);			/* log_job_manager.cpp's */

paf::Plugin *downloadPluginGet(void);			/* (0x811234F4) PLACEHOLDER name */

#endif
