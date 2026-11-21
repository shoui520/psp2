/*
 * log_job_manager.cpp: the event log's job manager. It keeps the "SceLogJobManagerJob" queue
 * and the new-event accessor, finds the highest ids the background downloads' log events
 * (LOGSTATUS0, BGCPSTATUS) were given, and queues the jobs that write a download's progress
 * and icon into the event log. The names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <file/localfile.h>
#include <buffer/buffer.h>
#include <image/image.h>
#include <framework.h>

#include "../../esecron/include/esebase.hpp"
#include "../../esecron/include/esetask.hpp"
#include "../../esecron/include/eseparam.hpp"
#include "../../esecron/include/eseutil.hpp"
#include "../../esecron/include/esehttp.hpp"
#include "src/livespace_db/livespace_db_internal.h"
#include "download_plugin.h"

using namespace paf;

#define LOG_EVENT_ICON_SIZE	64		/* an event's icon, 64x64 DXT3 */

static BgdlJobQueue *s_jobQueue;			/* (0x81544888) */
static LogJobManager *s_manager;			/* (0x8154488C) */

void Bgdl_8112661E(BgdlEvent *event, void *arg);	/* (0x8112661E) */

/* the title's row read from the app info database */
int Bgdl_81123BC0(const char *titleId, LsdbAppInfoPtr *info)
{
	AppInfoQClient *client = new AppInfoQClient;
	int ret;

	if (client == NULL || (client = SceLsdb_A6D8772C(client)) == NULL)
		return BGDL_ERROR_NO_MEMORY;
	ret = SceLsdb_E90B81CD(client);
	if (ret < 0) {
		operator delete(SceLsdb_E2101B03(client));
		return ret;
	}

	string id(titleId);

	ret = SceLsdb_D239031A(client, &id, info);
	if (ret < 0) {
		operator delete(SceLsdb_E2101B03(client));
		return ret;
	}
	operator delete(SceLsdb_E2101B03(client));
	return 0;
}

/* the job that writes a download's event into the event log (PLACEHOLDER names) */
class LogEventJob : public job::JobItem {
public:
	LogEventJob(int type, const LsdbNewEventKey *key, int arg)
		: job::JobItem("Job_EventLogDownload", NULL), m_type(type), m_arg(arg)
	{
		m_key = *key;
	}

	virtual ~LogEventJob()
	{
	}

	virtual int Run();					/* (0x81127FEC) */

	virtual void Finish(int result)
	{
	}

	int m_type;			/* 0 removes the event, 1 writes it, 2 updates it */
	LsdbNewEventKey m_key;
	string m_text;			/* PLACEHOLDER: not set here */
	int m_arg;
};

/* a job of the given type queued for the key's event */
int Bgdl_81123C90(int type, const LsdbNewEventKey *key, int arg)
{
	common::SharedPtr<job::JobItem> job(new LogEventJob(type, key, arg));

	BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A4FC(job);
	return 1;
}

/* the event of the row written (the interface's EventLogDownload) */
int Bgdl_81123DD0(SceInt64 rowid)
{
	LsdbNewEventKey key;

	key.rowid = rowid;
	Bgdl_81123C90(1, &key, 0);
	return 0;
}

int BgdlJobQueue::Bgdl_81123E3C(common::SharedPtr<job::JobItem> job)
{
	if (m_queue != NULL)
		return m_queue->Enqueue(job);
	return 0;
}

BgdlJobQueue *BgdlJobQueue::Bgdl_81123E48(void)
{
	return s_jobQueue;
}

LogJobManager::LogJobManager(bool clean)
	: m_queue(NULL), m_accessor(NULL), m_reserved(0), m_clean(clean), m_mounted(true)
{
	LocalFileStat stat;

	s_manager = this;
	m_mounted = LocalFile::Getstat("ux0:", &stat) == 0;
}

/* the accessor closed, the listener removed and the queue destroyed */
void LogJobManager::Bgdl_81123EF8(void)
{
	if (m_accessor != NULL) {
		SceLsdb_937405B5(m_accessor);
		if (m_accessor != NULL) {
			NewEventAccessor *accessor = m_accessor;

			SceLsdb_937405B5(accessor);
			operator delete(accessor);
		}
		m_accessor = NULL;
		m_reserved = 0;

		BgdlManager *manager = bgdlManagerGet();

		if (manager != NULL)
			manager->RemoveListener(Bgdl_8112661E, NULL);
	}
	if (m_queue != NULL) {
		BgdlJobQueue *queue = m_queue;

		if (queue->m_queue != NULL) {
			delete queue->m_queue;
			queue->m_queue = NULL;
		}
		s_jobQueue = NULL;
		operator delete(queue);
		m_queue = NULL;
	}
	m_clean = false;
}

LogJobManager::~LogJobManager()
{
	Bgdl_81123EF8();
	s_manager = NULL;
}

/* the highest ids the download events in the log were given: ids[0] the highest of 0 or more,
   ids[1] the highest below 0 */
int LogJobManager::Bgdl_81123F7A(int *ids)
{
	int ret = 0;

	ids[0] = 0;
	ids[1] = 0x80000000;
	if (m_clean != true && m_accessor != NULL) {
		vector<LsdbNewEventPtr> events;
		int count = 0;
		int newCount = 0;

		ret = SceLsdb_B285FC5E(m_accessor, &events, 64, &count, &newCount, true);
		if (ret == 0) {
			vector<LsdbNewEventPtr>::iterator it;

			for (it = events.begin(); it != events.end(); ++it) {
				LsdbNewEvent *event = it->get();

				if (event->titleId.length() == 0)
					continue;

				bool download = false;

				if (sce_paf_strcmp("LOGSTATUS0", event->titleId.c_str()) == 0 ||
				    sce_paf_strcmp("BGCPSTATUS", event->titleId.c_str()) == 0)
					download = true;
				if (download != true || event->itemId.length() == 0)
					continue;

				int id = -1;

				if (event->itemId.c_str() != NULL)
					id = sce_paf_atoi(event->itemId.c_str());
				if (id >= 0) {
					if (ids[0] == 0 || ids[0] == -1 || (SceUInt32)id > (SceUInt32)ids[0])
						ids[0] = id;
				} else {
					if (ids[1] == 0 || ids[1] == -2 || (SceUInt32)id > (SceUInt32)ids[1])
						ids[1] = id;
				}
			}
		}
	}
	return ret;
}

inline BgdlJobQueue::BgdlJobQueue() : m_queue(NULL)
{
	job::JobQueue::Option option;

	option.priority = 0x10000105;
	option.stackSize = 0x2000;
	option.numThreads = 1;
	m_queue = new job::JobQueue("SceLogJobManagerJob", &option);
	s_jobQueue = this;
}

/* the queue and the accessor made, and the ids found */
int LogJobManager::Bgdl_811240B6(int *ids)
{
	m_queue = new BgdlJobQueue;

	NewEventAccessor *accessor = NULL;
	void *mem = operator new(sizeof(NewEventAccessor));

	if (mem != NULL)
		accessor = SceLsdb_919C0934((NewEventAccessor *)mem);
	m_accessor = accessor;
	if (accessor != NULL && SceLsdb_4C137FCC(accessor) != 0) {
		if (m_accessor != NULL) {
			accessor = m_accessor;
			SceLsdb_937405B5(accessor);
			operator delete(accessor);
		}
		m_accessor = NULL;
	}
	Bgdl_81123F7A(ids);
	if (m_accessor != NULL)
		return 0;
	return -1;
}

/* a job queued on the log's queue */
void Bgdl_81124180(common::SharedPtr<job::JobItem> *job)
{
	if (s_manager != NULL && s_manager->m_queue != NULL)
		s_manager->m_queue->Bgdl_81123E3C(*job);
}

/* the bytes an image of the mode takes */
SceSize Bgdl_81124324(int width, int height, int mode)
{
	switch (mode) {
	case ImageMode_ABGR8888:
	case ImageMode_RGBA8888:
	case ImageMode_ARGB8888:
	case ImageMode_BGRA8888:
		return width * height * 4;
	case ImageMode_A8:
	case ImageMode_L8:
		return width * height;
	case ImageMode_DXT1:
		return ((width + 3) >> 2) * ((height + 3) >> 2) * 8 + 128;
	case ImageMode_DXT3:
	case ImageMode_DXT5:
		return ((width + 3) >> 2) * ((height + 3) >> 2) * 16 + 128;
	case ImageMode_BGR888:
	case ImageMode_RGB888:
		return width * height * 3;
	default:
		return 0;
	}
}

/* the icon at path made a 64x64 DXT3 DDS in *dds and given to the event */
int Bgdl_81124386(const char *path, LsdbNewEvent *event, common::SharedPtr<MallocBuffer> *dds)
{
	int ret = -1;

	if (path == NULL)
		return ret;

	image::ImageExtent extent(LOG_EVENT_ICON_SIZE, LOG_EVENT_ICON_SIZE);

	if (LocalFile::GetFileSize(path) <= 0)
		return ret;

	memory::HeapAllocator *heap = Framework::Instance()->GetDecodeHeap();
	common::SharedPtr<image::Image> img;

	if (heap != NULL)
		img = image::Image::Open(path, heap, image::ImageFormat_None, 0, 0);
	else
		img = image::Image::Open(path, NULL, image::ImageFormat_None, 0, 0);
	if (img.get() == NULL)
		return ret;

	common::SharedPtr<image::Image> rgba;
	common::SharedPtr<image::Image> dxt;

	if (img->GetFormat() == image::ImageFormat_Dds) {
		image::ImageExtent size = img->GetExtent();

		if (size.w != extent.w || size.h != extent.h || img->GetMode() != ImageMode_DXT3)
			return ret;
		dxt = img;
	} else {
		if (img->SetDecExtent(extent, image::ImageResizeType_NearestNeighbor) != true)
			return ret;
		if (img->GetMode() == ImageMode_RGBA8888)
			rgba = img;
		else
			rgba = img->ConvertMode(ImageMode_RGBA8888, image::ImageConvertModeType_Default);
		if (rgba.get() == NULL)
			return ret;
		dxt = rgba->ConvertMode(ImageMode_DXT3, image::ImageConvertModeType_Default);
		if (dxt.get() == NULL)
			return ret;
	}

	image::ImageBuffer pixels = dxt->ToBuffer(true);

	if (pixels.buff == NULL)
		return ret;
	*dds = MallocBuffer::Allocate(Bgdl_81124324(extent.w, extent.h, ImageMode_DXT3));
	if ((*dds)->GetSize() == 0)
		return ret;
	if (image::Image::SaveAsDds((*dds)->GetAddr(), (*dds)->GetSize(), pixels.buff, extent.w,
	    extent, ImageMode_DXT3, 0) == 1) {
		event->iconData = (*dds)->GetAddr();
		event->iconDataSize = (*dds)->GetSize();
		ret = 0;
	} else {
		dds->reset();
	}
	return ret;
}
