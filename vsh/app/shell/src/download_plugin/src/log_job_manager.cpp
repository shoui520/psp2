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
#include <plugin.h>
#include <appmgr/appmgr.h>

#include "../../esecron/include/esebase.hpp"
#include "../../esecron/include/esetask.hpp"
#include "../../esecron/include/eseparam.hpp"
#include "../../esecron/include/eseutil.hpp"
#include "../../esecron/include/esehttp.hpp"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../../shell.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "download_plugin.h"

PAF_VECTOR_INLINE_DTOR(BgdlTaskEntry)

using namespace paf;

#define LOG_EVENT_ICON_SIZE	64		/* an event's icon, 64x64 DXT3 */
#define LOG_EVENT_EXEC_MODE	0x70000		/* PLACEHOLDER: how the exec title is started */
#define LOG_EVENT_UPDATE_ICON	"vs0:app/NPXS10015/sce_sys/icon0.png"

static BgdlJobQueue *s_jobQueue;			/* (0x81544888) */
static LogJobManager *s_manager;			/* (0x8154488C) */

void Bgdl_8112661E(BgdlEvent *event, void *arg);	/* (0x8112661E) */
/* (0x81372196) the application a game's title runs as, into name (PLACEHOLDER) */
int Game_81372196(const char *titleId, char *name);
/* (0x81416908) the download plugin's error dialog of the kind (PLACEHOLDER) */
void Bgdl_81416908(int kind);

/* the title's row read from the app info database */
__attribute__((noinline)) int Bgdl_81123BC0(const char *titleId, LsdbAppInfoPtr *info)
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

	{
		string id(titleId);

		ret = SceLsdb_D239031A(client, &id, info);
	}
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

	LogEventJob(int type, const char *text, int arg)
		: job::JobItem("Job_EventLogDownload", NULL), m_type(type), m_arg(arg)
	{
		if (text != NULL)
			m_text = text;
	}

	virtual ~LogEventJob()
	{
	}

	virtual int Run();					/* (0x81127FEC) */
	int Bgdl_8112775E(LogJobManager *manager);

	virtual void Finish(int result)
	{
	}

	int m_type;			/* 0 removes the event, 1 writes it, 2 updates it */
	LsdbNewEventKey m_key;
	string m_text;			/* the title whose download events type 2 removes */
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
	: m_queue(NULL), m_accessor(NULL), m_object(NULL), m_clean(clean), m_mounted(true)
{
	bool mounted;

	s_manager = this;
	{
		LocalFileStat stat;

		mounted = LocalFile::Getstat("ux0:", &stat) == 0;
	}
	m_mounted = mounted;
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
		m_object = NULL;

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

/* a download's event in the log: LOGSTATUS0 or BGCPSTATUS */
static inline int logEventIsDownload(const LsdbNewEvent *event)
{
	const char *title = event->titleId.c_str();
	int download = 0;

	if (sce_paf_strcmp("LOGSTATUS0", title) == 0 || sce_paf_strcmp("BGCPSTATUS", title) == 0)
		download = 1;
	return download;
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
		if (ret != 0)
			return ret;

		vector<LsdbNewEventPtr>::iterator it;

		for (it = events.begin(); it != events.end(); ++it) {
			LsdbNewEvent *event = it->get();

			if (event->titleId.length() == 0)
				continue;

			if (logEventIsDownload(event) != 1 || it->get()->itemId.length() == 0)
				continue;

			int id = -1;

			if (it->get()->itemId.c_str() != NULL)
				id = sce_paf_atoi(it->get()->itemId.c_str());
			if (id < 0) {
				if (ids[1] == 0 || ids[1] == -2 || (SceUInt32)id > (SceUInt32)ids[1])
					ids[1] = id;
			} else {
				if (ids[0] == 0 || ids[0] == -1 || (SceUInt32)id > (SceUInt32)ids[0])
					ids[0] = id;
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
void Bgdl_81124180(common::SharedPtr<job::JobItem> job)
{
	if (s_manager != NULL && s_manager->m_queue != NULL)
		s_manager->m_queue->Bgdl_81123E3C(job);
}

/* the task's event key: its number, under BGCPSTATUS for a copy, else LOGSTATUS0 */
int Bgdl_81124232(const BgdlEvent *task, LsdbNewEventKey *key)
{
	char buf[16] = "";

	sce_paf_snprintf(buf, sizeof(buf) - 1, "%u", task->id);
	key->itemId.assign(buf, sce_paf_strlen(buf));
	if (task->param.type == 1)
		key->titleId = "BGCPSTATUS";
	else
		key->titleId = "LOGSTATUS0";
	return 0;
}

/* the bytes an image of the mode takes */
SceSize Bgdl_81124324(SceUInt32 width, SceUInt32 height, int mode)
{
	switch (mode) {
	case ImageMode_ABGR8888:
	case ImageMode_RGBA8888:
	case ImageMode_ARGB8888:
	case ImageMode_BGRA8888:
		return 4 * (height * width);
	case ImageMode_A8:
	case ImageMode_L8:
		return height * width;
	case ImageMode_DXT1:
		return ((width + 3) >> 2) * ((height + 3) >> 2) * 8 + 128;
	case ImageMode_DXT3:
	case ImageMode_DXT5:
		return ((width + 3) >> 2) * ((height + 3) >> 2) * 16 + 128;
	case ImageMode_BGR888:
	case ImageMode_RGB888:
		return 3 * (height * width);
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

/* the exec title and argument written to the event when they differ */
static inline void logEventSetExec(LsdbNewEvent *event, const string &titleId,
    const string &arg, int *fields)
{
	if (event->execArg != arg || event->execTitleId != titleId ||
	    event->execMode != LOG_EVENT_EXEC_MODE) {
		event->execArg = arg;
		event->execTitleId = titleId;
		event->execMode = LOG_EVENT_EXEC_MODE;
		*fields |= NE_FIELD_EXEC;
	}
}

/* the content id appended to a browse argument */
static inline void logEventAppendContentId(string *arg, SceUInt64 contentId)
{
	if (contentId != 0) {
		char buf[64];

		sce_paf_memset(buf, 0, sizeof(buf));
		sce_paf_snprintf(buf, sizeof(buf) - 1, "&content_id=%llu", contentId);
		*arg += buf;
	}
}

/* the task's log event made or brought up to date with what the event says of the task */
int LogJobManager::Bgdl_81124DE2(BgdlEvent *task, bool insert)
{
	int ret = 0;
	LsdbNewEvent event;

	get_time_internal();
	if ((task->type != 3 && task->type != 4) || task->param.key110)
		goto out;
	{
		SceInt64 msgType = 3;
		int fields = 0;
		bool changed = false;
		bool popup = false;
		bool isNew = false;
		bool setMsgType;

		event.msgType = 3;
		Bgdl_81124232(task, &event);
		if (insert == false) {
			ret = SceLsdb_07ADA030(m_accessor, &event);
			if (ret != 0)
				goto out;
			if (event.msgType == 80 || event.msgType == 81)
				goto out;
		}

		string title("");
		string icon("");
		string titleId("");
		string desc("");
		common::SharedPtr<MallocBuffer> dds;

		if ((event.notifyFlags & 1) == 0 && task->param.key115)
			event.notifyFlags |= 1;
		if (event.title.length() == 0 || event.iconPath.length() == 0 ||
		    event.execTitleId.length() == 0) {
			EseParam param(task->id);

			if (param.open() == 0) {
				if (event.title.length() == 0) {
					param.read(105, &title);
					param.read(203, &desc);
				}
				if (event.iconPath.length() == 0)
					param.read(106, &icon);
				if (event.execTitleId.length() == 0)
					param.read(220, &titleId);
			}
		}
		if (event.title.length() == 0 || event.iconPath.length() == 0 ||
		    event.execTitleId.length() == 0) {
			if (title.length() != 0) {
				if (event.title != title) {
					event.title = title.c_str();
					fields = NE_FIELD_TITLE;
				}
			} else if (desc.length() != 0) {
				if (event.title != desc) {
					event.title = desc.c_str();
					fields = NE_FIELD_TITLE;
				}
			}
			if (icon.length() != 0 && event.iconDataSize == 0 && event.iconPath.length() == 0 &&
			    Bgdl_81124386(icon.c_str(), &event, &dds) == 0)
				fields |= NE_FIELD_ICON;
		}
		if (event.execTitleId.length() == 0 && titleId.length() != 0) {
			if (event.execTitleId != titleId) {
				event.execTitleId = titleId.c_str();
				fields |= NE_FIELD_EXEC;
			}
		}

		if (task->param.runStatus == 0) {
			if (task->param.ready) {
				string execTitle;
				string execArg;

				switch (task->param.key107) {
				case 1:
				case 2:
				case 3:
				case 4:
					if (!("BGCPSTATUS" == event.titleId)) {
						if (task->param.key107 > 1) {
							if (task->param.key107 <= 2) {
								execTitle = "NPXS10009";
								execArg = "music:browse?category=ALL";
								logEventAppendContentId(&execArg, task->value);
							} else if (task->param.key107 <= 4) {
								execTitle = "NPXS10010";
								execArg = "video:browse?category=ALL";
								logEventAppendContentId(&execArg, task->value);
							}
						} else if (task->param.key107 != 0) {
							execTitle = "NPXS10004";
							execArg = "photo:browse?category=ALL";
						}
						logEventSetExec(&event, execTitle, execArg, &fields);
						if (event.actType != 2) {
							event.actType = 2;
							fields |= NE_FIELD_ACT_TYPE;
						}
						msgType = 80;
						isNew = true;
					} else {
						if (task->param.ready && task->param.key102)
							msgType = 81;
						else
							msgType = 2;
						if (event.actType != 10) {
							event.actType = 10;
							fields |= NE_FIELD_ACT_TYPE;
						}
						execArg = "";
						switch (task->param.key107) {
						case 6:
							execTitle = "NPXS10101";
							execArg = "ux0:bgdl/w/log.dat";
							break;
						case 3:
							execTitle = "NPXS10010";
							break;
						case 2:
							execTitle = "NPXS10009";
							break;
						case 1:
							execTitle = "NPXS10004";
							break;
						default:
							break;
						}
						logEventSetExec(&event, execTitle, execArg, &fields);
						isNew = true;
					}
					break;
				case 5:
					execTitle = "";
					execArg = "";
					logEventSetExec(&event, execTitle, execArg, &fields);
					if (event.actType != 0) {
						event.actType = 0;
						fields |= NE_FIELD_ACT_TYPE;
					}
					if (event.iconPath.length() == 0) {
						event.iconPath = LOG_EVENT_UPDATE_ICON;
						fields |= NE_FIELD_ICON_PATH;
					}
					msgType = 82;
					popup = true;
					isNew = false;
					break;
				case 6:
				case 7:
				case 8:
				case 9:
				case 12:
					isNew = true;
					if (task->param.key113) {
						int actType;

						isNew = false;
						switch (task->param.key107) {
						case 6:
							actType = 3;
							break;
						case 7:
							actType = 4;
							break;
						case 8:
							actType = 5;
							break;
						case 9:
							actType = 6;
							break;
						case 12:
							actType = 9;
							isNew = true;
							break;
						default:
							actType = 0;
							break;
						}
						msgType = 82;
						popup = true;
						if (event.actType != actType) {
							event.actType = actType;
							fields |= NE_FIELD_ACT_TYPE;
						}
					}
					break;
				default:
					isNew = false;
					break;
				}
				setMsgType = true;
			} else if (task->param.key113) {
				string execTitle;
				string execArg;
				int actType;

				switch (task->param.key107) {
				case 5:
					execTitle = "NPXS10016";
					execArg = "updater::via_downloader";
					logEventSetExec(&event, execTitle, execArg, &fields);
					actType = 2;
					if (event.actType != 2) {
						event.actType = 2;
						fields |= NE_FIELD_ACT_TYPE;
					}
					if (event.iconPath.length() == 0) {
						event.iconPath = LOG_EVENT_UPDATE_ICON;
						fields |= NE_FIELD_ICON_PATH;
					}
					isNew = true;
					break;
				case 6:
					actType = 3;
					isNew = false;
					if (event.popupNo != true) {
						event.popupNo = true;
						fields |= NE_FIELD_POPUP_NO;
					}
					break;
				case 7:
					actType = 4;
					isNew = true;
					break;
				case 8:
					actType = 5;
					isNew = true;
					break;
				case 9:
					actType = 6;
					isNew = true;
					break;
				case 12:
					actType = 9;
					isNew = true;
					break;
				default:
					actType = 0;
					break;
				}
				if (event.actType != actType) {
					event.actType = actType;
					fields |= NE_FIELD_ACT_TYPE;
				}
				msgType = 7;
				if (task->result != 0) {
					msgType = 8;
					isNew = false;
				}
				setMsgType = true;
				popup = true;
			} else {
				int result = task->result;

				if (result == 0 || result == (int)0x80103A44 || result == (int)0x80103A24 ||
				    result == (int)0x80103A1C) {
					msgType = 6;
				} else if (task->param.state == 3) {
					msgType = 0;
				} else {
					msgType = 2;
					isNew = true;
				}
				setMsgType = true;
			}
		} else if (task->param.runStatus <= 1) {
			if (task->param.state <= 2) {
				msgType = 3;
				setMsgType = true;
			} else if (task->param.state <= 3) {
				msgType = 5;
				setMsgType = true;
			} else {
				setMsgType = false;
			}
		} else if (task->param.runStatus != 3) {
			setMsgType = true;
			switch (task->param.state) {
			case 0:
			case 1:
				msgType = 3;
				break;
			case 2:
				msgType = 4;
				break;
			case 3:
				if (task->param.key107 > 2) {
					if (task->param.key107 <= 4) {
						msgType = 1;
						break;
					}
				} else if (task->param.key107 != 0) {
					msgType = 1;
					break;
				}
				setMsgType = false;
				break;
			case 4:
				setMsgType = false;
				break;
			default:
				break;
			}
		} else {
			setMsgType = false;
		}

		int state = bgdlStringToInt(event.msgArg[8]);
		int result = bgdlParseInt(event.msgArg[7]);

		if (state != bgdlGetInt((const int *)&task->param)) {
			bgdlUIntToString((const SceUInt32 *)&task->param, &event.msgArg[8]);
			bgdlStringToInt(event.msgArg[8]);
			changed = true;
		}
		if (result != task->result) {
			bgdlFormatUInt(task->result, &event.msgArg[7]);
			changed = true;
		}
		if (setMsgType == true && event.msgType != msgType) {
			event.msgType = msgType;
			fields |= NE_FIELD_MSG;
		}
		if (changed == true)
			fields |= NE_FIELD_MSG;
		if (event.newFlag != isNew) {
			event.newFlag = isNew;
			fields |= NE_FIELD_NEW_FLAG;
			if (isNew) {
				popup = true;
				if (event.popupNo != true) {
					event.popupNo = true;
					fields |= NE_FIELD_POPUP_NO;
				}
			}
		}
		if (fields == 0)
			goto out;

		int r;

		if (popup) {
			event.timeUpdated = "";
			fields |= NE_FIELD_TIME_UPDATED;
		}
		if (insert == true) {
			r = SceLsdb_315B9FD6(&event, true);
		} else {
			LsdbNewEventPtr ptr(new LsdbNewEvent(event));

			r = SceLsdb_B3252544(&ptr, 1, fields);
		}
		if (r != 0)
			goto out;
		ret = 0;
	}
out:
	get_time_internal();
	return ret;
}

/* the log brought into line with the tasks: the events of tasks no longer there removed (all of
   them when clean), the tasks whose event says another state or result listed in *changed, and
   an event made for each task the log has none for */
int LogJobManager::Bgdl_81125DD6(vector<BgdlTaskEntry> *changed)
{
	int ret;

	changed->clear();

	vector<BgdlTaskEntry> tasks;
	vector<LsdbNewEventPtr> list;

	if (m_accessor == NULL) {
		ret = -1;
	} else {
		BgdlManager *manager = bgdlManagerGet();

		if (manager == NULL)
			ret = -1;
		else
			ret = manager->ListTasks(&tasks, -1);
	}
	if (ret == 0) {
		do {
			vector<LsdbNewEventPtr> events;
			int count = 0;
			int newCount = 0;
			LsdbNewEventKey keys[64];

			ret = SceLsdb_B285FC5E(m_accessor, &events, 64, &count, &newCount, true);
			if (ret != 0)
				break;

			vector<LsdbNewEventPtr>::iterator it = events.begin();
			int removed = 0;

			if (it != events.end()) {
				do {
					LsdbNewEvent *event = it->get();

					if (event->titleId.length() != 0 && logEventIsDownload(event) == 1) {
						if (m_clean == true) {
							keys[removed++] = *it->get();
						} else if (it->get()->itemId.length() != 0) {
							int id = -1;
							SceUInt32 i;

							if (it->get()->itemId.c_str() != NULL)
								id = sce_paf_atoi(it->get()->itemId.c_str());
							for (i = 0; i < tasks.size(); i++) {
								if (id == (int)tasks[i].id)
									break;
							}
							if (i < tasks.size()) {
								BgdlTaskEntry task = tasks[i];
								int state = bgdlStringToInt(it->get()->msgArg[8]);
								int result = bgdlParseInt(it->get()->msgArg[7]);

								if (state != bgdlGetInt((const int *)&task.param) ||
								    result != task.result)
									changed->push_back(task);
							} else if (it->get()->msgType != 80 && it->get()->msgType != 82 &&
								   it->get()->msgType != 81) {
								keys[removed++] = *it->get();
							}
						} else {
							keys[removed++] = *it->get();
						}
					}
					++it;
				} while (it != events.end() && removed < 64);
				if (removed != 0) {
					ret = SceLsdb_AE04DDEB(keys, removed);
					if (ret != 0)
						break;
				}
			}
			if (m_clean == true)
				break;
			if (removed != 0 || changed->size() != 0) {
				ret = SceLsdb_B285FC5E(m_accessor, &events, 64, &count, &newCount, true);
				if (ret != 0)
					break;
			}

			int ids[64];
			int found = 0;

			for (it = events.begin(); it != events.end() && found < 64; ++it) {
				LsdbNewEvent *event = it->get();

				if (event->titleId.length() != 0 && logEventIsDownload(event) == 1) {
					int id = -1;

					if (it->get()->itemId.c_str() != NULL)
						id = sce_paf_atoi(it->get()->itemId.c_str());
					ids[found++] = id;
				}
			}

			vector<BgdlTaskEntry> missing;
			SceUInt32 i;

			for (i = 0; i < tasks.size(); i++) {
				int j;

				for (j = 0; j < found; j++) {
					if (ids[j] == (int)tasks[i].id)
						break;
				}
				if (j >= found)
					missing.push_back(tasks[i]);
			}
			for (i = 0; i < missing.size(); i++) {
				BgdlEvent event;

				event.type = 3;
				event.id = missing[i].id;
				event.result = 0;
				event.reserved = 0;
				event.value = 0;
				event.param = missing[i].param;
				event.result = missing[i].result;
				Bgdl_81124DE2(&event, true);
			}
		} while (0);
	}
	return ret;
}

/* the job that writes a task's change into the event log (PLACEHOLDER names) */
class LogTaskJob : public job::JobItem {
public:
	LogTaskJob(const BgdlEvent *event, LogJobManager *manager)
		: job::JobItem("Job_EventLogDownload", NULL)
	{
		m_event = *event;
		m_manager = manager;
	}

	virtual ~LogTaskJob()
	{
	}

	virtual int Run();					/* (0x81127092) */
	int Bgdl_811269A4(LogJobManager *manager, SceUInt64 rowid);
	int Bgdl_81127048(LogJobManager *manager, SceUInt64 *rowid);

	virtual void Finish(int result)
	{
	}

	BgdlEvent m_event;
	LogJobManager *m_manager;
};

/* starts the log: the tasks it does not have yet written, and the manager listened to */
void LogJobManager::Bgdl_81126418(DownloadPluginObject **object, const int *reserved)
{
	if (m_accessor == NULL)
		return;

	vector<BgdlTaskEntry> changed;

	Bgdl_81125DD6(&changed);
	if (object != NULL)
		m_object = object;
	for (SceUInt32 i = 0; i < changed.size(); i++) {
		BgdlEvent event;

		event.type = 3;
		event.id = changed[i].id;
		event.param = changed[i].param;
		event.result = changed[i].result;
		event.reserved = 0;
		event.value = 0;

		common::SharedPtr<job::JobItem> job(new LogTaskJob(&event, this));

		Bgdl_81124180(job);
	}

	BgdlManager *manager = bgdlManagerGet();

	if (manager != NULL)
		manager->AddListener(Bgdl_8112661E, this);
}

/* the manager's listener: every event but type 2 queued for the log */
void Bgdl_8112661E(BgdlEvent *event, void *arg)
{
	if (event->type == 2)
		return;

	common::SharedPtr<job::JobItem> job(new LogTaskJob(event, (LogJobManager *)arg));

	Bgdl_81124180(job);
}

/* the events of the title's downloads (msg_type 80 and 82) removed */
int LogJobManager::Bgdl_81126788(const char *titleId, SceInt64 reserved)
{
	int ret = 0;

	if (titleId == NULL || m_accessor == NULL)
		return ret;

	vector<LsdbNewEventPtr> events;
	int count = 0;
	int newCount = 0;
	LsdbNewEventKey keys[64];
	int removed = 0;

	ret = SceLsdb_B285FC5E(m_accessor, &events, 64, &count, &newCount, true);
	if (ret != 0)
		return ret;

	vector<LsdbNewEventPtr>::iterator it = events.begin();

	if (it != events.end()) {
		do {
			LsdbNewEvent *event = it->get();

			if (event->titleId.length() != 0) {
				if (logEventIsDownload(event) == 1 && it->get()->execTitleId.length() != 0) {
					switch (it->get()->msgType) {
					case 80:
					case 82:
						if (sce_paf_strcmp(titleId, it->get()->execTitleId.c_str()) == 0)
							keys[removed++] = *it->get();
						break;
					}
				}
			}
			++it;
		} while (it != events.end());
		if (removed != 0)
			SceLsdb_AE04DDEB(keys, removed);
	}
	return ret;
}

/* tells the plugin's object of a task's change: added, removed or, for a state, run status
   or param change, finished */
int LogTaskJob::Bgdl_811269A4(LogJobManager *manager, SceUInt64 rowid)
{
	int kind;

	if (manager == NULL || manager->m_object == NULL || *manager->m_object == NULL)
		return -1;
	switch (m_event.type) {
	case 4:
	case 3:
		if (m_event.param.runStatus != 0 || !m_event.param.key113)
			return -1;
		kind = 2;
		break;
	case 1:
		kind = 1;
		break;
	case 0:
		kind = 0;
		break;
	default:
		return -1;
	}
	if (m_event.param.key111 != 2)
		return -1;
	(*manager->m_object)->Bgdl_8112F29A(2, kind, m_event.id, rowid);
	return 0;
}

/* a removed task's event removed, unless its param keeps it */
int LogJobManager::Bgdl_81126A1C(const BgdlEvent *task)
{
	bool remove = true;

	if (task->param.runStatus == 0 && task->param.ready)
		remove = !task->param.key102;
	if (!task->param.key110 && !remove)
		return 0;

	LsdbNewEvent event;

	Bgdl_81124232(task, &event);
	SceLsdb_07ADA030(m_accessor, &event);

	int ret = SceLsdb_AE04DDEB(&event, 1);

	if (ret != 0)
		return ret;
	return 0;
}

/* an added task's event written, from its param file; rowid gets its row */
int LogJobManager::Bgdl_81126C2E(const BgdlEvent *task, SceUInt64 *rowid)
{
	string title;
	string titleId;
	string icon;
	int ret;

	{
		EseParam param(task->id);

		ret = param.open();
		if (ret != 0)
			return ret;
		param.read(105, &title);
		param.read(106, &icon);
		param.read(220, &titleId);
	}

	LsdbNewEvent event;

	Bgdl_81124232(task, &event);
	if (title.length() != 0)
		event.title = title.c_str();
	else
		event.title = "";
	if (titleId.length() != 0)
		event.execTitleId = titleId.c_str();
	else
		event.execTitleId = "";
	if (task->param.key115)
		event.notifyFlags |= 1;
	bgdlFormatUInt(0, &event.msgArg[7]);
	bgdlUIntToString((const SceUInt32 *)&task->param, &event.msgArg[8]);

	common::SharedPtr<MallocBuffer> dds;

	if (icon.length() != 0 && event.iconDataSize == 0)
		Bgdl_81124386(icon.c_str(), &event, &dds);
	event.newFlag = false;
	event.msgType = 3;
	event.popupNo = false;
	ret = SceLsdb_315B9FD6(&event, false);
	*rowid = event.rowid;
	if (ret != 0)
		return ret;
	return 0;
}

/* the task's change written into the log by its type */
int LogTaskJob::Bgdl_81127048(LogJobManager *manager, SceUInt64 *rowid)
{
	int ret = -1;

	if (manager == NULL || manager->m_accessor == NULL)
		return ret;
	switch (m_event.type) {
	case 0:
		ret = manager->Bgdl_81126C2E(&m_event, rowid);
		break;
	case 1:
		ret = manager->Bgdl_81126A1C(&m_event);
		break;
	case 2:
		ret = 0;
		break;
	case 3:
	case 4:
		ret = manager->Bgdl_81124DE2(&m_event, false);
		break;
	}
	return ret;
}

int LogTaskJob::Run()
{
	LogJobManager *manager = s_manager;

	if (manager == NULL || m_event.id == (SceUInt32)-1)
		return -1;

	SceUInt64 rowid = 0;

	Bgdl_81127048(manager, &rowid);
	Bgdl_811269A4(manager, rowid);
	return -1;
}

/* the event's exec argument, NULL when it has none */
static inline const char *logEventExecArg(const LsdbNewEvent *event)
{
	const char *arg = NULL;

	if (event->execArg.length() != 0)
		arg = event->execArg.c_str();
	return arg;
}

/* the event's title launched, by its act type (PLACEHOLDER: what the act types and states are) */
void Bgdl_811270F4(const LsdbNewEvent *event)
{
	if (event == NULL || event->execTitleId.length() == 0)
		return;
	switch (event->actType) {
	case 3: {
		int state = bgdlStringToInt(event->msgArg[8]);

		if ((state & 0xF0) == 0x60 && ((state & 0x3800000) == 0x1000000 ||
					       (state & 0x3800000) == 0x1800000)) {
			LsdbAppInfoPtr info(new LsdbAppInfo());

			if (Bgdl_81123BC0(event->execTitleId.c_str(), &info) != 0)
				shellEventServerSend(1, NULL, 0);
			else
				sceAppMgrLaunchAppByName(event->execMode | 0x20000,
							 event->execTitleId.c_str(),
							 logEventExecArg(event));
			return;
		}
		break;
	}
	case 4:
		if ((bgdlStringToInt(event->msgArg[8]) & 0x3800000) == 0x800000) {
			Bgdl_81416908(2);
		} else {
			char name[32] = "";

			if (Game_81372196(event->execTitleId.c_str(), name) == 1) {
				sceAppMgrLaunchAppByName(event->execMode | 0x20000, name,
							 logEventExecArg(event));
			} else {
				Plugin *plugin = Plugin::Find("applauncher_plugin");

				if (plugin != NULL) {
					ApplauncherInterface *iface =
						(ApplauncherInterface *)plugin->GetInterface(1);

					if (iface != NULL)
						iface->func[10]();
				}
				Bgdl_81416908(1);
			}
		}
		return;
	case 5:
	case 6: {
		int flags = event->execMode | 0x20000;
		LsdbAppInfoPtr info(new LsdbAppInfo());

		if (Bgdl_81123BC0(event->execTitleId.c_str(), &info) != 0)
			Bgdl_81416908(0);
		else
			sceAppMgrLaunchAppByName(flags, event->execTitleId.c_str(),
						 logEventExecArg(event));
		return;
	}
	default:
		break;
	}

	LsdbAppInfoPtr info(new LsdbAppInfo());

	if (Bgdl_81123BC0(event->execTitleId.c_str(), &info) != 0)
		Bgdl_81416908(0);
	else
		sceAppMgrLaunchAppByName(event->execMode, event->execTitleId.c_str(),
					 logEventExecArg(event));
}

/* the event's title shown by the applauncher (PLACEHOLDER: what its interface's 4 and 10 are) */
void Bgdl_811274B8(const LsdbNewEvent *event)
{
	Plugin *plugin = Plugin::Find("applauncher_plugin");
	ApplauncherInterface *iface = NULL;

	if (plugin != NULL)
		iface = (ApplauncherInterface *)plugin->GetInterface(1);
	if (event == NULL || event->execTitleId.length() == 0 || iface == NULL)
		return;
	if (event->actType == 4) {
		if ((bgdlStringToInt(event->msgArg[8]) & 0x3800000) == 0x800000) {
			LsdbAppInfoPtr info(new LsdbAppInfo());

			if (Bgdl_81123BC0(event->execTitleId.c_str(), &info) != 0) {
				iface->func[10]();
				Bgdl_81416908(1);
			} else {
				((void (*)(const string *, int))iface->func[4])(&event->execTitleId, 1);
			}
		} else {
			char name[32] = "";

			if (Game_81372196(event->execTitleId.c_str(), name) == 1) {
				string titleId(name);

				((void (*)(const string *, int))iface->func[4])(&titleId, 1);
			} else {
				iface->func[10]();
				Bgdl_81416908(1);
			}
		}
	} else {
		LsdbAppInfoPtr info(new LsdbAppInfo());

		if (Bgdl_81123BC0(event->execTitleId.c_str(), &info) != 0)
			Bgdl_81416908(0);
		else
			((void (*)(const string *, int))iface->func[4])(&event->execTitleId, 1);
	}
}

/* the key's event opened: its title launched or shown, by its message and act type */
int LogEventJob::Bgdl_8112775E(LogJobManager *manager)
{
	if (manager == NULL || manager->m_accessor == NULL)
		return 0;

	LsdbNewEvent event;

	event.rowid = m_key.rowid;
	if (SceLsdb_07ADA030(manager->m_accessor, &event) != 0)
		return 0;
	switch (event.msgType) {
	case 82:
		switch (event.actType) {
		case 3:
			if (event.execTitleId.length() != 0 &&
			    sce_paf_strncmp(event.execTitleId.c_str(), "PCSI00011", 10) == 0) {
				LsdbNewEvent launch(event);

				launch.execTitleId = "NPXS10016";
				launch.execArg = "console_info::psm";
				launch.execMode = LOG_EVENT_EXEC_MODE;
				Bgdl_811270F4(&launch);
				break;
			}
			if (sce_paf_strcmp(event.execTitleId.c_str(), string("PCSC80018").c_str()) == 0) {
				Bgdl_81416908(4);
				break;
			}
			/* fall through */
		case 4:
		case 5:
		case 6:
			Bgdl_811274B8(&event);
			break;
		case 9: {
			LsdbNewEvent launch(event);

			launch.execTitleId = "NPXS10016";
			launch.execArg = "custom_themes";
			launch.execMode = 0;
			Bgdl_811270F4(&launch);
			break;
		}
		}
		break;
	case 81:
		if (event.actType == 10) {
			LsdbNewEvent launch(event);

			launch.execMode = LOG_EVENT_EXEC_MODE;
			Bgdl_811270F4(&launch);
		}
		break;
	case 80:
		break;
	case 8:
		switch (event.actType) {
		case 3:
		case 9: {
			int result = 0;

			if (event.msgArg[7].length() != 0)
				result = sce_paf_atoi(event.msgArg[7].c_str());
			if (result == (int)BGDL_ERROR_ALREADY_INSTALLED && bgdlManagerGet() != NULL) {
				int id = -1;

				if (event.itemId.length() != 0)
					id = sce_paf_atoi(event.itemId.c_str());
				if (bgdlResetInstalledTask(id) == 0 && event.actType == 9)
					shellEventServerSend(1, NULL, 0);
			}
			break;
		}
		}
		break;
	case 7:
		switch (event.actType) {
		case 3:
		case 4:
		case 5:
		case 6:
		case 9:
			Bgdl_811270F4(&event);
			break;
		}
		break;
	}
	return 0;
}

int LogEventJob::Run()
{
	LogJobManager *manager = s_manager;

	if (manager == NULL)
		return -1;
	switch (m_type) {
	case 0:
		if (manager->m_accessor != NULL)
			SceLsdb_AE04DDEB(&m_key, 1);
		break;
	case 1:
		Bgdl_8112775E(manager);
		break;
	case 2:
		manager->Bgdl_81126788(m_text.c_str(), -1);
		break;
	}
	return -1;
}

/* a job of the given type queued for the text */
int Bgdl_8112803E(int type, const char *text, int arg)
{
	common::SharedPtr<job::JobItem> job(new LogEventJob(type, text, arg));

	BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A4FC(job);
	return 1;
}

/* the title's download events removed (when its tasks are) */
int Bgdl_8112816E(const char *titleId)
{
	Bgdl_8112803E(2, titleId, 0);
	return 0;
}
