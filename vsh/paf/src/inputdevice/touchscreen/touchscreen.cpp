/*
 * paf::inputdevice::touchscreen: once per frame the samples SceTouch took
 * since the last Update are read for each panel. A finger keeps the slot it
 * got when it touched down until it is lifted; the steps each slot went
 * through are queued, and every Update hands out the next step of each slot,
 * skipping moves that a later move replaces. A panel that is locked masks the
 * fingers on it until they are lifted. Each slot keeps the last points of its
 * finger, for its speed.
 */
#include <inputdevice/touchscreen/touchscreen.h>
#include <touch.h>
#include <std/list>
#include <std/string.h>
#include <std/stdlib.h>
#include <timer/timer.h>
#include <kernel/processmgr.h>
#include "../../std/math.h"

using namespace paf;
using namespace paf::inputdevice;
using namespace paf::inputdevice::touchscreen;

enum {
	/* the samples read from SceTouch at a time */
	SAMPLE_NUM = 16,
	/* the points of a finger kept for its speed */
	TRACE_NUM = 12,
	/* a gap between samples that masks the fingers on the panel (us) */
	SAMPLE_GAP = 200000,
	ERROR_INVALID_PORT = -2135994080,	/* 0x80AF5120 */
	ERROR_INVALID_REGION = -2135994079	/* 0x80AF5121 */
};

/* A port's slots. */
class DataList {
public:
	enum {
		DATA_NUM = 8
	};

	Data data[DATA_NUM];
};

/* the width of the screen's edges where a finger is corrected less (pixels) */
#define EDGE_SIZE	64.0f

/* s_region: the regions read; s_active_region: those SceTouch was told to use */
#define REGION_DEFAULT		0x1
/* SceTouch's region flag for the common dialogs */
#define REGION_COMMON_DIALOG	0x80

/* s_region_request: who asked for the default region */
#define REQUEST_DEFAULT		0x1
#define REQUEST_OWNERSHIP	0x2

/* a region of a panel, in panel dots */
typedef struct SceTouchRegion {
	SceInt16 x;
	SceInt16 y;
	SceInt16 width;
	SceInt16 height;
} SceTouchRegion;

typedef struct SceTouchRegionAttr {
	SceUInt32 reserved;
} SceTouchRegionAttr;

extern "C" {
int sceTouchGetPixelDensity(float *front, float *back);
int sceTouchSetSamplingStateExt(SceUInt32 port, SceUInt32 state);
int sceTouchPeekRegion(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs, SceUInt32 region);
int sceTouchPeekRegionExt(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs, SceUInt32 region);
int sceTouchSetRegion(SceUInt32 port, SceUInt32 id, const SceTouchRegion *region,
		      SceTouchRegionAttr attr);
int sceTouchClearRegion(SceUInt32 port, SceUInt32 id);
int sceTouchActivateRegion(SceUInt32 port, SceUInt32 id, SceUInt32 activate);
}

/* A slot of a panel: the finger SceTouch reports under id, -1 for none. */
struct Point {
	Data data;
	uint32_t index;
	int32_t id;
};

struct TracePoint {
	uint64_t timestamp;
	uint16_t x;
	uint16_t y;
};

/* The last points of a slot's finger, the latest first, and where it touched down. */
struct Trace {
	int16_t num;
	TracePoint point[TRACE_NUM];
	TracePoint start;
};

static float s_screen_width = 960.0f;
static float s_screen_height = 544.0f;
static float s_viewport_x = 0.0f;
static float s_viewport_y = 0.0f;
static float s_viewport_width = 960.0f;
static float s_viewport_height = 544.0f;
/* layout units per screen pixel, and back */
static float s_scale_x = 1.0f;
static float s_scale_y = 1.0f;
static float s_inv_scale_x = 1.0f;
static float s_inv_scale_y = 1.0f;

static InitParam s_init_param;
/* dots per millimetre of each panel */
static float s_pixel_density[MAX_PORT];
static DeviceHandler s_device_handler[MAX_PORT];
static SceTouchPanelInfo s_panel_info[MAX_PORT];
/* the steps of the slots, in the order they happened, while a sample is read */
static list<Point> s_point_list[MAX_PORT];
static Point s_point[MAX_PORT][DataList::DATA_NUM];
/* the latest sample read */
static SceTouchData s_last_data[MAX_PORT];
static list<Data> s_queue[MAX_PORT][DataList::DATA_NUM];
/* s_data before the mask */
static DataList s_data_raw[MAX_PORT];
static DataList s_data_pre[MAX_PORT];
static DataList s_data[MAX_PORT];
static Trace s_trace[MAX_PORT][DataList::DATA_NUM];
static uint32_t s_mask[MAX_PORT];
static uint32_t s_mask_pre[MAX_PORT];
static uint32_t s_lock_count[MAX_PORT];
static uint32_t s_unlock_count[MAX_PORT];
static uint32_t s_region[MAX_PORT];
static uint32_t s_active_region[MAX_PORT];
/* the regions that take the fingers SceTouch hides from the layers above */
static uint32_t s_hide_upper_layer_region[MAX_PORT];
static uint32_t s_region_request[MAX_PORT];
static list<Timer> s_lock_timer[MAX_PORT];
static SceUInt64 s_idle_time[MAX_PORT];
static SceFVector4 s_edge_correction[MAX_PORT];

static bool IsReleased(const DataList *data_list)
{
	for (uint32_t i = 0; i < DataList::DATA_NUM; i++) {
		int32_t state = data_list->data[i].state;

		if (state == Data::STATE_PRESS || state == Data::STATE_MOVE)
			return false;
	}
	return true;
}

void paf::inputdevice::touchscreen::GetDeviceInfo(uint32_t port, DeviceInfo *info)
{
	if (port >= MAX_PORT)
		return;

	const SceTouchPanelInfo *panel = &s_panel_info[port];
	float width = (float)(panel->maxDispX - panel->minDispX + 1);
	int32_t height = panel->maxDispY - panel->minDispY + 1;
	float scale = s_screen_width / width;

	float left = sce_paf_floorf((float)panel->minAaX * s_screen_width / width);
	float top = sce_paf_floorf((float)panel->minAaY * s_screen_height / (float)height);
	float right = sce_paf_floorf((float)panel->maxAaX * s_screen_width / width);
	float bottom = sce_paf_floorf((float)panel->maxAaY * s_screen_height / (float)height);

	info->pixel_per_mm = s_pixel_density[port] * scale;
	info->layout_per_mm = info->pixel_per_mm * s_scale_y;
	info->active_min = math::v2(left, top);
	info->active_max = math::v2(right, bottom);
}

int32_t paf::inputdevice::touchscreen::Init(InitParam &init_param)
{
	static const float front_correction[4] = { 2.2805016f, 9.12200642f, 0.0f, -1.1402508f };
	static const float back_correction[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	DeviceInfo info;
	int32_t ret;

	s_init_param = init_param;
	sce_paf_memset(s_data_raw, 0, sizeof(s_data_raw));
	sce_paf_memset(s_data_pre, 0, sizeof(s_data_pre));
	sce_paf_memset(s_data, 0, sizeof(s_data));
	sce_paf_memset(s_trace, 0, sizeof(s_trace));
	sce_paf_memset(s_last_data, 0, sizeof(s_last_data));
	for (uint32_t port = 0; port < MAX_PORT; port++) {
		s_pixel_density[port] = 0.0f;
		s_mask[port] = 0;
		s_mask_pre[port] = 0;
		s_lock_count[port] = 0;
		s_active_region[port] = 0;
		s_hide_upper_layer_region[port] = 0;
		s_region_request[port] = 0;
		s_region[port] = REGION_DEFAULT;
		s_idle_time[port] = 0;
	}

	ret = sceTouchGetPixelDensity(&s_pixel_density[PORT_FRONT], &s_pixel_density[PORT_BACK]);
	if (ret != 0)
		return ret;
	for (uint32_t port = 0; port < MAX_PORT; port++) {
		for (Point *point = s_point[port]; point != s_point[port] + DataList::DATA_NUM; point++)
			point->id = -1;
		ret = sceTouchGetPanelInfo(port, &s_panel_info[port]);
		if (ret != 0)
			return ret;
		ret = sceTouchActivateRegion(port, 0, 1);
		if (ret < 0)
			return ret;
		if (s_init_param.m_mode == InitParam::Mode_CommonDialog)
			s_region[port] |= REGION_COMMON_DIALOG;
	}

	GetDeviceInfo(PORT_FRONT, &info);
	s_edge_correction[PORT_FRONT].x = front_correction[0] * info.pixel_per_mm;
	s_edge_correction[PORT_FRONT].y = front_correction[1] * info.pixel_per_mm;
	s_edge_correction[PORT_FRONT].z = front_correction[2] * info.pixel_per_mm;
	s_edge_correction[PORT_FRONT].w = front_correction[3] * info.pixel_per_mm;
	GetDeviceInfo(PORT_BACK, &info);
	s_edge_correction[PORT_BACK].x = back_correction[0] * info.pixel_per_mm;
	s_edge_correction[PORT_BACK].y = back_correction[1] * info.pixel_per_mm;
	s_edge_correction[PORT_BACK].z = back_correction[2] * info.pixel_per_mm;
	s_edge_correction[PORT_BACK].w = back_correction[3] * info.pixel_per_mm;
	return ret;
}

int32_t paf::inputdevice::touchscreen::Activate(uint32_t port)
{
	int32_t ret;

	if (port >= MAX_PORT)
		return ERROR_INVALID_PORT;
	if (s_init_param.m_mode == InitParam::Mode_CommonDialog)
		return sceTouchSetSamplingStateExt(port, SCE_TOUCH_SAMPLING_STATE_START);
	ret = sceTouchSetSamplingState(port, SCE_TOUCH_SAMPLING_STATE_START);
	if (port == PORT_BACK && ret == 0)
		return sceTouchEnableTouchForce(PORT_BACK);
	return ret;
}

int32_t paf::inputdevice::touchscreen::Deactivate(uint32_t port)
{
	if (port >= MAX_PORT)
		return ERROR_INVALID_PORT;
	if (s_init_param.m_mode == InitParam::Mode_CommonDialog)
		return sceTouchSetSamplingStateExt(port, SCE_TOUCH_SAMPLING_STATE_STOP);
	if (port == PORT_BACK)
		sceTouchDisableTouchForce(PORT_BACK);
	return sceTouchSetSamplingState(port, SCE_TOUCH_SAMPLING_STATE_STOP);
}

int32_t paf::inputdevice::touchscreen::End()
{
	for (uint32_t port = 0; port < MAX_PORT; port++) {
		if (s_init_param.m_mode == InitParam::Mode_CommonDialog)
			s_region[port] ^= REGION_COMMON_DIALOG;
	}
	return 0;
}

void paf::inputdevice::touchscreen::Unlock(uint32_t port)
{
	if (port < MAX_PORT) {
		if (s_lock_count[port] > s_unlock_count[port])
			s_unlock_count[port]++;
	}
}

int32_t ScePafMisc_DD2F6FFC(uint32_t port)
{
	if (port >= MAX_PORT)
		return 1;
	return IsReleased(&s_data_raw[port]);
}

uint32_t paf::inputdevice::touchscreen::SetMask(uint32_t port, uint32_t mask)
{
	if (port >= MAX_PORT)
		return 0;
	s_mask[port] |= mask;
	return s_mask[port];
}

void ScePafMisc_8424A840(float *center_x, float *center_y, float x, float y)
{
	*center_x = (x - s_screen_width * 0.5f) * s_scale_x;
	*center_y = (s_screen_height * 0.5f - y) * s_scale_y;
}

uint32_t paf::inputdevice::touchscreen::GetMask(uint32_t port)
{
	if (port >= MAX_PORT)
		return 0;
	return s_mask[port];
}

void paf::inputdevice::touchscreen::Lock(uint32_t port)
{
	if (port < MAX_PORT)
		s_lock_count[port]++;
}

int32_t paf::inputdevice::touchscreen::Update(float diff_time, const Data **front_data,
					      const Data **back_data, const Data **front_data_pre,
					      const Data **back_data_pre)
{
	for (uint32_t port = 0; port < MAX_PORT; port++) {
		if (s_lock_timer[port].size() != 0) {
			list<Timer>::iterator it = s_lock_timer[port].begin();

			while (it != s_lock_timer[port].end()) {
				if (it->m_state == Timer::State_Finished) {
					it = s_lock_timer[port].erase(it);
					Unlock(port);
				} else {
					it->DoTick();
					++it;
				}
			}
		}
	}

	if (front_data != NULL)
		*front_data = s_data[PORT_FRONT].data;
	if (back_data != NULL)
		*back_data = s_data[PORT_BACK].data;
	if (front_data_pre != NULL)
		*front_data_pre = s_data_pre[PORT_FRONT].data;
	if (back_data_pre != NULL)
		*back_data_pre = s_data_pre[PORT_BACK].data;
	sce_paf_memcpy(s_data_pre, s_data, sizeof(s_data_pre));

	for (uint32_t port = 0; port < MAX_PORT; port++) {
		SceTouchData sample;
		SceTouchData samples[SAMPLE_NUM];
		int32_t ret;

		sce_paf_memset(&sample, 0, sizeof(sample));
		if (s_init_param.m_mode == InitParam::Mode_CommonDialog)
			ret = sceTouchPeekRegionExt(port, samples, SAMPLE_NUM, s_region[port]);
		else
			ret = sceTouchPeekRegion(port, samples, SAMPLE_NUM, s_region[port]);
		if (ret < 0)
			return ret;
		if (ret <= 0)
			continue;

		/* the samples newer than the last one read, else that one again */
		uint64_t last = s_last_data[port].timeStamp;
		uint64_t latest = samples[ret - 1].timeStamp;
		int32_t old = ret;

		do {
			if (last >= samples[old - 1].timeStamp)
				break;
			old--;
		} while (old > 0);

		const SceTouchData *next = &samples[old - 1];
		int32_t num = 1;

		if (ret - old > 0) {
			num = ret - old;
			next = &samples[old];
		}
		if ((int64_t)(latest - last) >= SAMPLE_GAP) {
			Lock(port);
			Unlock(port);
		}
		s_last_data[port].timeStamp = latest;

		while (num > 0) {
			sample = *next++;
			num--;

			uint32_t report_num = sample.reportNum;
			Point *begin = s_point[port];
			Point *end = s_point[port] + DataList::DATA_NUM;
			Point *point;

			if (report_num != 0) {
				/* the fingers lifted */
				for (point = begin; point != end; point++) {
					if (point->id < 0)
						continue;

					uint32_t i;

					for (i = 0; i < report_num; i++) {
						if (point->id == sample.report[i].id)
							break;
					}
					if (i >= report_num) {
						point->data.state = Data::STATE_RELEASE;
						s_point_list[port].push_back(*point);
						point->id = -1;
					}
				}

				for (uint32_t i = 0; i < report_num; i++) {
					const SceTouchReport *report = &sample.report[i];
					uint32_t id = report->id;

					if (s_hide_upper_layer_region[port] == 0 && report->info != 0) {
						if ((GetMask(port) & (1 << i)) == 0)
							SetMask(port, 1 << i);
					}

					for (point = begin; point != end; point++) {
						if (point->id == id)
							break;
					}
					if (point == end) {
						for (point = begin; point != end; point++) {
							if (point->id < 0)
								break;
						}
						if (point == end)
							continue;
						point->data.state = Data::STATE_PRESS;

						uint32_t index = 0;

						for (Point *p = begin; p != point; p++)
							index++;
						point->index = index;
						point->id = id;
					} else {
						point->data.state = Data::STATE_MOVE;
					}

					const SceTouchPanelInfo *panel = &s_panel_info[port];
					int32_t width = panel->maxDispX - panel->minDispX + 1;
					int32_t height = panel->maxDispY - panel->minDispY + 1;

					if (width > 0 && height > 0) {
						float x = (float)report->x * s_screen_width / (float)width;
						float y = (float)report->y * s_screen_height / (float)height;
						const SceFVector4 *correction = &s_edge_correction[PORT_FRONT];

						if (port == PORT_BACK)
							correction = &s_edge_correction[PORT_BACK];

						float correction_x = correction->z;
						float correction_y = correction->w;

						/* less of the correction towards the screen's edges */
						if (correction_x != 0.0f) {
							if (x >= s_screen_width - EDGE_SIZE)
								x += (1.0f - (x - (s_screen_width - EDGE_SIZE)) /
									      EDGE_SIZE) * correction_x;
							else if (x <= EDGE_SIZE)
								x += x / EDGE_SIZE * correction_x;
							else
								x += correction_x;
						}
						if (correction_y != 0.0f) {
							if (y >= s_screen_height - EDGE_SIZE)
								y += (1.0f - (y - (s_screen_height - EDGE_SIZE)) /
									      EDGE_SIZE) * correction_y;
							else if (y <= EDGE_SIZE)
								y += y / EDGE_SIZE * correction_y;
							else
								y += correction_y;
						}

						float center_x;
						float center_y;

						ScePafMisc_8424A840(&center_x, &center_y, x, y);
						point->data.x = (int32_t)x;
						point->data.y = (int32_t)y;
						point->data.center_orig_x = (int32_t)center_x;
						point->data.center_orig_y = (int32_t)center_y;
					}
					point->data.force = report->force;
					s_point_list[port].push_back(*point);
				}
			} else {
				for (point = begin; point != end; point++) {
					if (point->id >= 0) {
						point->data.state = Data::STATE_RELEASE;
						s_point_list[port].push_back(*point);
						point->id = -1;
					}
				}
			}

			/* queue the steps on their slots */
			while (s_point_list[port].size() != 0) {
				Point step = s_point_list[port].front();

				s_point_list[port].pop_front();
				if (step.index < DataList::DATA_NUM) {
					Data queued = step.data;

					queued.timestamp = sample.timeStamp;
					s_queue[port][step.index].push_back(queued);
				}
			}
		}
	}

	for (uint32_t port = 0; port < MAX_PORT; port++) {
		DataList *data_list = &s_data[port];

		sce_paf_memset(data_list, 0, sizeof(*data_list));
		for (uint32_t i = 0; i < DataList::DATA_NUM; i++) {
			Data *slot = &data_list->data[i];
			list<Data> *queue = &s_queue[port][i];

			slot->state = Data::STATE_NONE;
			if (queue->size() != 0) {
				*slot = queue->front();
				queue->pop_front();
				/* the first step that happened, then the last of a run of moves */
				while (slot->state == Data::STATE_NONE && queue->size() != 0) {
					*slot = queue->front();
					queue->pop_front();
				}
				while (slot->state == Data::STATE_MOVE && queue->size() != 0) {
					*slot = queue->front();
					queue->pop_front();
				}
				if (slot->state != Data::STATE_NONE)
					continue;
			}
			slot->timestamp = s_last_data[port].timeStamp;
		}
		sce_paf_memcpy(&s_data_raw[port], data_list, sizeof(*data_list));

		uint32_t lock_count = s_lock_count[port];
		uint32_t mask = s_mask[port];
		uint32_t unlock_count = s_unlock_count[port];

		/* a lock masks the fingers on the panel */
		if (lock_count != 0) {
			uint32_t touched = 0;

			for (uint32_t i = 0; i < DataList::DATA_NUM; i++) {
				int32_t state = data_list->data[i].state;

				if (state == Data::STATE_PRESS || state == Data::STATE_MOVE ||
				    state == Data::STATE_RELEASE)
					touched |= 1 << i;
			}
			if (port < MAX_PORT) {
				mask |= touched;
				s_mask[port] = mask;
			}
		}
		if (unlock_count != 0) {
			s_lock_count[port] = lock_count - unlock_count;
			s_unlock_count[port] = 0;
		}
		if (mask != 0) {
			uint32_t mask_pre = s_mask_pre[port];

			/* a masked finger is cancelled once, then reads as none until lifted */
			for (uint32_t i = 0; i < DataList::DATA_NUM; i++) {
				Data *slot = &data_list->data[i];
				uint32_t bit = 1 << i;
				uint32_t masked = bit & mask;

				if ((slot->state == Data::STATE_RELEASE || slot->state == Data::STATE_NONE) &&
				    masked != 0) {
					mask &= ~bit;
					s_mask[port] = mask;
				}
				if (masked != 0) {
					if ((bit & mask_pre) != 0)
						slot->state = Data::STATE_NONE;
					else
						slot->state = Data::STATE_CANCEL;
				}
			}
			s_mask_pre[port] = mask;
		}

		for (uint32_t i = 0; i < DataList::DATA_NUM; i++) {
			const Data *slot = &data_list->data[i];
			Trace *trace = &s_trace[port][i];
			int32_t state = slot->state;

			if (state != Data::STATE_PRESS && state != Data::STATE_MOVE &&
			    state != Data::STATE_RELEASE)
				continue;

			int32_t x = slot->x;
			int32_t y = slot->y;
			uint64_t timestamp = slot->timestamp;

			if (state == Data::STATE_PRESS) {
				trace->num = 0;
				trace->start.timestamp = timestamp;
				trace->start.x = x;
				trace->start.y = y;
			}
			if (state != Data::STATE_RELEASE || trace->num <= 1) {
				for (int32_t j = TRACE_NUM - 2; j >= 0; j--)
					trace->point[j + 1] = trace->point[j];
				trace->point[0].timestamp = timestamp;
				trace->point[0].x = x;
				trace->point[0].y = y;
				if (trace->num < TRACE_NUM)
					trace->num++;
			}
		}

		if (ScePafMisc_DD2F6FFC(port)) {
			if (s_idle_time[port] == 0)
				s_idle_time[port] = sceKernelGetProcessTimeWide();
		} else {
			s_idle_time[port] = 0;
		}
		if (s_device_handler[port] != NULL)
			s_device_handler[port](data_list->data);
	}
	return 0;
}

DeviceHandler paf::inputdevice::touchscreen::SetDeviceHandler(uint32_t port,
							      DeviceHandler new_handler)
{
	if (port >= MAX_PORT)
		return NULL;

	DeviceHandler old_handler = s_device_handler[port];

	s_device_handler[port] = new_handler;
	return old_handler;
}

bool paf::inputdevice::touchscreen::IsLocked(uint32_t port)
{
	return port < MAX_PORT && s_lock_count[port] > s_unlock_count[port];
}

void ScePafMisc_2CF401D9(uint32_t port, float time)
{
	if (port < MAX_PORT) {
		Timer timer;

		timer.InitTimer(time, 0, 0, -1, NULL);
		s_lock_timer[port].push_back(timer);
		Lock(port);
	}
}

void paf::inputdevice::touchscreen::SetScreenResolution(uint32_t width, uint32_t height)
{
	s_screen_width = (float)width;
	s_screen_height = (float)height;
	s_scale_x = s_viewport_width / ((float)width - s_viewport_x * 2.0f);
	s_scale_y = s_viewport_height / ((float)height - s_viewport_y * 2.0f);
	s_inv_scale_x = 1.0f / s_scale_x;
	s_inv_scale_y = 1.0f / s_scale_y;
}

void paf::inputdevice::touchscreen::GetScreenResolution(uint32_t &width, uint32_t &height)
{
	width = (uint32_t)s_screen_width;
	height = (uint32_t)s_screen_height;
}

void paf::inputdevice::touchscreen::SetScreenViewport(int32_t x, int32_t y, uint32_t width,
						      uint32_t height)
{
	s_viewport_x = (float)x;
	s_viewport_y = (float)y;
	s_viewport_width = (float)width;
	s_viewport_height = (float)height;
	s_scale_x = (float)width / (s_screen_width - (float)x * 2.0f);
	s_scale_y = (float)height / (s_screen_height - (float)y * 2.0f);
	s_inv_scale_x = 1.0f / s_scale_x;
	s_inv_scale_y = 1.0f / s_scale_y;
}

void paf::inputdevice::touchscreen::GetScreenViewport(int32_t &x, int32_t &y, uint32_t &width,
						      uint32_t &height)
{
	x = (int32_t)s_viewport_x;
	y = (int32_t)s_viewport_y;
	width = (uint32_t)s_viewport_width;
	height = (uint32_t)s_viewport_height;
}

/* The panel's correction in layout units, y up. */
void paf::inputdevice::touchscreen::GetEdgeCorrection(uint32_t port, float *correction)
{
	if (correction == NULL)
		return;

	float scale = s_scale_y;
	SceFVector4 *vector = (SceFVector4 *)correction;

	*vector = port == PORT_BACK ? s_edge_correction[PORT_BACK] : s_edge_correction[PORT_FRONT];
	vector->x = vector->x * scale;
	vector->y = vector->y * scale;
	vector->z = vector->z * scale;
	vector->w = -(vector->w * scale);
}

/* Region region_id + 1 of the panel set to a layout rectangle, or cleared when it has no area. */
static int32_t UpdateRegion(uint32_t port, uint32_t region_id, const math::rect &rect)
{
	if (port >= MAX_PORT)
		return ERROR_INVALID_PORT;
	if (region_id >= MAX_REGION_NUM)
		return ERROR_INVALID_REGION;

	uint32_t id = region_id + 1;
	SceTouchRegionAttr attr;
	int32_t ret;

	sce_paf_memset(&attr, 0, sizeof(attr));

	const SceTouchPanelInfo *panel = &s_panel_info[port];
	int32_t panel_width = panel->maxDispX - panel->minDispX + 1;
	int32_t panel_height = panel->maxDispY - panel->minDispY + 1;
	float x = (rect.left() + s_screen_width * 0.5f) * (float)panel_width / s_screen_width;
	float y = (s_screen_height * 0.5f - rect.top()) * (float)panel_height / s_screen_height;
	float width = rect.width() * (float)panel_width / s_screen_width;
	float height = rect.height() * (float)panel_height / s_screen_height;
	math::rect area;
	float area_width = (float)(panel->maxAaX - panel->minAaX + 1);
	float area_height = (float)(panel->maxAaY - panel->minAaY + 1);

	area.set_left((float)panel->minAaX);
	area.set_right((float)panel->minAaX + area_width);
	area.set_bottom((float)panel->minAaY);
	area.set_top((float)panel->minAaY + area_height);

	math::rect region(x + width * 0.5f, y + height * 0.5f, width, height);
	SceTouchRegion touch_region;

	region = ScePafMisc_B2795FE2(area, region);
	touch_region.x = (int32_t)region.left();
	touch_region.y = (int32_t)region.bottom();
	touch_region.width = (int32_t)region.width();
	touch_region.height = (int32_t)region.height();
	if (touch_region.width > 0 && touch_region.height > 0) {
		ret = sceTouchSetRegion(port, id, &touch_region, attr);
		if (ret == 0)
			s_region[port] |= 1 << id;
	} else {
		ret = sceTouchClearRegion(port, id);
		if (ret == 0)
			s_region[port] |= 1 << id;
	}
	return ret;
}

int32_t paf::inputdevice::touchscreen::SetRegion(uint32_t port, uint32_t region_id,
						 const math::rect &rect, bool activate,
						 bool hide_upper_layer)
{
	if (port >= MAX_PORT)
		return ERROR_INVALID_PORT;
	if (region_id >= MAX_REGION_NUM)
		return ERROR_INVALID_REGION;

	uint32_t id = region_id + 1;
	SceUInt32 region_activate;
	int32_t ret;

	/* the default region takes the whole panel, and the common dialogs have their own */
	if ((s_active_region[port] & REGION_DEFAULT) != 0 ||
	    s_init_param.m_mode == InitParam::Mode_CommonDialog)
		activate = false;
	region_activate = 0;
	if (activate) {
		region_activate = 1;
		ret = UpdateRegion(port, region_id, rect);
		if (ret < 0)
			return ret;
	}
	ret = sceTouchActivateRegion(port, id, region_activate);
	if (ret == 0) {
		uint32_t bit = 1 << id;

		if (activate)
			s_active_region[port] |= bit;
		else
			s_active_region[port] &= ~bit;
		if (hide_upper_layer)
			s_hide_upper_layer_region[port] |= bit;
		else
			s_hide_upper_layer_region[port] &= ~bit;
	}
	return ret;
}

/* The default region is active while the default or the ownership asks for it. */
static int32_t RequestDefaultRegion(uint32_t port, uint32_t request, bool activate)
{
	if (port >= MAX_PORT)
		return ERROR_INVALID_PORT;

	int32_t ret;

	if (activate)
		s_region_request[port] |= request;
	else
		s_region_request[port] &= ~request;

	bool active = (s_region_request[port] & (REQUEST_DEFAULT | REQUEST_OWNERSHIP)) != 0;

	ret = sceTouchActivateRegion(port, 0, active);
	if (ret == 0) {
		if (active)
			s_active_region[port] |= REGION_DEFAULT;
		else
			s_active_region[port] &= ~REGION_DEFAULT;
	}
	return ret;
}

int32_t paf::inputdevice::touchscreen::ActivateDefaultRegion(uint32_t port, bool activate)
{
	return RequestDefaultRegion(port, REQUEST_DEFAULT, activate);
}

int32_t paf::inputdevice::touchscreen::ActivateOwnershipRegion(uint32_t port, bool activate)
{
	return RequestDefaultRegion(port, REQUEST_OWNERSHIP, activate);
}

math::rect ScePafMisc_84B49847(const math::rect &rect)
{
	math::rect result(rect);

	float center_x = result.center_x() * s_inv_scale_x + s_screen_width * 0.5f;
	float center_y = s_screen_height * 0.5f - result.center_y() * s_inv_scale_y;

	result.set_center(center_x, center_y);
	result.set_size(result.width() * s_inv_scale_x, result.height() * s_inv_scale_y);
	return result;
}

void ScePafMisc_9C8CBB11(float *x, float *y, float center_x, float center_y)
{
	*x = s_inv_scale_x * center_x + s_screen_width * 0.5f;
	*y = s_screen_height * 0.5f - center_y * s_inv_scale_y;
}

int32_t paf::inputdevice::touchscreen::GetPointFromTopleft(uint32_t port, uint32_t id, float &x,
							   float &y)
{
	if (port >= MAX_PORT || id >= DataList::DATA_NUM)
		return -1;

	const Data *slot = &s_data[port].data[id];

	x = (float)slot->x;
	y = (float)slot->y;
	return 0;
}

int32_t paf::inputdevice::touchscreen::GetPointFromCenter(uint32_t port, uint32_t id, float &x,
							  float &y)
{
	if (port >= MAX_PORT || id >= DataList::DATA_NUM)
		return -1;

	const Data *slot = &s_data[port].data[id];

	x = (float)slot->center_orig_x;
	y = (float)slot->center_orig_y;
	return 0;
}

void paf::inputdevice::touchscreen::GetData(uint32_t port, uint32_t id, const Data **data)
{
	if (port >= MAX_PORT || id >= DataList::DATA_NUM) {
		*data = NULL;
		return;
	}
	*data = &s_data[port].data[id];
}

void paf::inputdevice::touchscreen::GetDataPre(uint32_t port, uint32_t id, const Data **data)
{
	if (port >= MAX_PORT || id >= DataList::DATA_NUM) {
		*data = NULL;
		return;
	}
	*data = &s_data_pre[port].data[id];
}

int32_t ScePafMisc_F612C512(uint32_t port, uint32_t id, uint64_t time, float *vx, float *vy)
{
	uint64_t span = time * 1000;
	int32_t ret = 0;

	if (port >= MAX_PORT || id >= DataList::DATA_NUM)
		return -1;

	const Trace *trace = &s_trace[port][id];
	uint64_t latest = trace->point[0].timestamp;
	uint32_t x = trace->point[0].x;
	uint32_t y = trace->point[0].y;
	int32_t num = trace->num;

	*vx = 0.0f;
	*vy = 0.0f;
	if (num <= 1)
		return ret;

	/* the point closest to span before the latest */
	const TracePoint *point;

	if (latest - trace->point[num - 1].timestamp < span) {
		point = &trace->point[num - 1];
	} else {
		uint64_t prev = 0;
		int32_t index;

		for (int32_t i = 1; i < num; i++) {
			uint64_t elapsed = latest - trace->point[i].timestamp;

			if (span < elapsed) {
				index = i;
				if (elapsed - span > prev && prev != 0)
					index = i - 1;
				break;
			}
			prev = elapsed;
			index = i;
		}
		point = &trace->point[index];
	}

	uint64_t timestamp = point->timestamp;
	uint32_t point_x = point->x;
	uint32_t point_y = point->y;
	uint64_t msec = (latest - timestamp) / 1000;

	if (msec > 0) {
		float dx = (float)x - (float)point_x;
		float dy = (float)y - (float)point_y;

		*vx = dx / (float)msec;
		*vy = dy / (float)msec;
	}
	return ret;
}

int32_t ScePafMisc_88F4A40B(uint32_t port, uint32_t id, uint64_t time, float *vx, float *vy)
{
	float x;
	float y;
	int32_t ret;

	ret = ScePafMisc_F612C512(port, id, time, &x, &y);
	*vx = s_scale_x * x;
	*vy = -(s_scale_y * y);
	return ret;
}

int32_t ScePafMisc_24D9EBA3(uint32_t port, uint32_t id, float *x, float *y)
{
	if (port >= MAX_PORT || id >= DataList::DATA_NUM)
		return -1;

	const Trace *trace = &s_trace[port][id];

	if (trace->num <= 0) {
		*y = 0.0f;
		*x = 0.0f;
	} else {
		*x = (float)trace->start.x;
		*y = (float)trace->start.y;
	}
	return 0;
}

int32_t ScePafMisc_3BA14D8A(uint32_t port, uint32_t id, float *x, float *y)
{
	if (port >= MAX_PORT || id >= DataList::DATA_NUM)
		return -1;

	const Trace *trace = &s_trace[port][id];

	if (trace->num <= 0) {
		*y = 0.0f;
		*x = 0.0f;
	} else {
		ScePafMisc_8424A840(x, y, (float)trace->start.x, (float)trace->start.y);
	}
	return 0;
}

bool paf::inputdevice::touchscreen::IsOwnershipReleasable(uint32_t port)
{
	if (port >= MAX_PORT)
		return true;
	if (port == PORT_BACK)
		return true;
	return IsReleased(&s_data[port]);
}

float paf::inputdevice::touchscreen::GetIdleTime(uint32_t port)
{
	if (ScePafMisc_DD2F6FFC(port) && s_idle_time[port] != 0) {
		SceUInt64 now = sceKernelGetProcessTimeWide();

		if (now > s_idle_time[port])
			return (float)(now - s_idle_time[port]) / 1000.0f;
	}
	return 0.0f;
}
