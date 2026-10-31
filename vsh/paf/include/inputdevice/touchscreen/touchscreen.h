/*
 * paf::inputdevice::touchscreen: the front and back touch panels, read
 * through SceTouch once per frame. Each finger keeps the slot it was given
 * when it touched down, so the eight slots of a port follow the same fingers
 * from frame to frame; the frames sampled since the last Update are queued
 * per slot and handed out one step at a time.
 */
#ifndef PAF_INPUTDEVICE_TOUCHSCREEN_TOUCHSCREEN_H
#define PAF_INPUTDEVICE_TOUCHSCREEN_TOUCHSCREEN_H

#include <stdint.h>
#include <math/math.h>
#include <math/rect.h>

namespace paf {
namespace inputdevice {
namespace touchscreen {

enum {
	PORT_FRONT = 0,
	PORT_BACK = 1,
	MAX_PORT = 2
};

/* the regions a layout may set on a panel, after the default one */
enum {
	MAX_REGION_NUM = 6
};

class InitParam {
public:
	enum Mode {
		Mode_Normal = 0,
		Mode_CommonDialog = 2	/* SceTouch's Ext sampling */
	};

	int32_t m_mode;
};

/* One finger's state in a slot, in screen pixels from the top left. */
class Data {
public:
	enum State {
		STATE_NONE = 0,
		STATE_PRESS = 1,
		STATE_RELEASE = 2,
		STATE_MOVE = 3,
		/* the finger was masked: it touches on, but no longer counts */
		STATE_CANCEL = 4
	};

	uint64_t timestamp;
	int32_t x;
	int32_t y;
	int32_t force;
	int32_t state;
	/* the point in layout units from the centre of the viewport, y up */
	int32_t center_orig_x;
	int32_t center_orig_y;
};

/* A panel seen from the screen. */
class DeviceInfo {
public:
	float pixel_per_mm;
	/* pixel_per_mm in layout units */
	float layout_per_mm;
	/* the corners of the active area, in screen pixels */
	math::v2 active_min;
	math::v2 active_max;
};

/* called with the port's eight slots after each Update */
typedef void (*DeviceHandler)(Data *data);

void GetDeviceInfo(uint32_t port, DeviceInfo *info);
int32_t Init(InitParam &init_param);
int32_t Activate(uint32_t port);
int32_t Deactivate(uint32_t port);
int32_t End();
void Unlock(uint32_t port);
uint32_t SetMask(uint32_t port, uint32_t mask);
uint32_t GetMask(uint32_t port);
void Lock(uint32_t port);
int32_t Update(float diff_time, const Data **front_data, const Data **back_data,
	       const Data **front_data_pre, const Data **back_data_pre);
DeviceHandler SetDeviceHandler(uint32_t port, DeviceHandler new_handler);
bool IsLocked(uint32_t port);
void SetScreenResolution(uint32_t width, uint32_t height);
void GetScreenResolution(uint32_t &width, uint32_t &height);
void SetScreenViewport(int32_t x, int32_t y, uint32_t width, uint32_t height);
void GetScreenViewport(int32_t &x, int32_t &y, uint32_t &width, uint32_t &height);
void GetEdgeCorrection(uint32_t port, float *correction);
int32_t SetRegion(uint32_t port, uint32_t region_id, const math::rect &rect, bool activate,
		  bool hide_upper_layer);
int32_t ActivateDefaultRegion(uint32_t port, bool activate);
int32_t ActivateOwnershipRegion(uint32_t port, bool activate);
int32_t GetPointFromTopleft(uint32_t port, uint32_t id, float &x, float &y);
int32_t GetPointFromCenter(uint32_t port, uint32_t id, float &x, float &y);
void GetData(uint32_t port, uint32_t id, const Data **data);
void GetDataPre(uint32_t port, uint32_t id, const Data **data);
bool IsOwnershipReleasable(uint32_t port);
/* the time in seconds with no finger on a panel */
float GetIdleTime(uint32_t port);

}
}
}

/*
 * No Sony names found for these NIDs: whether no finger is on a panel, its
 * mask ignored; a point from the centre of the viewport to the top left of
 * the screen and back; a layout rectangle as the screen sees it; a panel
 * locked for a time; a finger's speed over the last milliseconds given, in
 * screen pixels and in layout units per millisecond; where a finger touched
 * down, from the top left and from the centre.
 */
extern "C" int32_t ScePafMisc_DD2F6FFC(uint32_t port);
extern "C" void ScePafMisc_8424A840(float *center_x, float *center_y, float x, float y);
extern "C" void ScePafMisc_9C8CBB11(float *x, float *y, float center_x, float center_y);
extern "C" paf::math::rect ScePafMisc_84B49847(const paf::math::rect &rect);
extern "C" void ScePafMisc_2CF401D9(uint32_t port, float time);
extern "C" int32_t ScePafMisc_F612C512(uint32_t port, uint32_t id, uint64_t time, float *vx,
				       float *vy);
extern "C" int32_t ScePafMisc_88F4A40B(uint32_t port, uint32_t id, uint64_t time, float *vx,
				       float *vy);
extern "C" int32_t ScePafMisc_24D9EBA3(uint32_t port, uint32_t id, float *x, float *y);
extern "C" int32_t ScePafMisc_3BA14D8A(uint32_t port, uint32_t id, float *x, float *y);

#endif /* PAF_INPUTDEVICE_TOUCHSCREEN_TOUCHSCREEN_H */
