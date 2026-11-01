/*
 * paf::ui::wutil, as far as converted C++ uses it: the widget system's setup
 * and the listeners it hands out. Names after ScePaf-RE.
 */
#ifndef PAF_WIDGET_WUTIL_H
#define PAF_WIDGET_WUTIL_H

#include <stdint.h>
#include <graphics/gutil.h>

namespace paf {

class Plugin;

namespace ui {

class Environment;

/* told when a plugin starts, before its own start function */
class MessageDialogListener {
public:
	virtual ~MessageDialogListener();
	virtual void PluginOnStart(Plugin *plugin);
};

namespace wutil {

/* what the widget system is made with; Init keeps the listeners for the getters */
class InitParam {
public:
	InitParam();

	gutil::WinHandle whandle;
	void *default_shader_addr;
	int32_t default_shader_size;
	void *surf_dev_mem_addr;
	int32_t surf_dev_mem_size;
	void *surf_def_pool_addr;
	int32_t surf_def_pool_size;
	int32_t surf_def_pool_device_type;
	/* the screen, and the size and field of view the camera projects with */
	int32_t screen_width;
	int32_t screen_height;
	int32_t view_width;
	int32_t view_height;
	float fov;
	int32_t projection_option;
	uint32_t input_device_mode;
	void *input_handler_pad;
	void *input_handler_touch;
	void *input_handler_touch_rear;
	void *input_handler_motion_sensor;
	void *edit_listener;
	void *web_edit_listener;
	void *message_dialog_listener;
	void *listener_A55215E5;
	void *webview_listener;
	void *listener_E638AA1F;
};

int32_t Init(const InitParam &param);
void Term();
void DevTerm();
void DevInit(const gutil::DevInitParam &param);
void SetEnv(Environment *env);

MessageDialogListener *GetMessageDialogListener();

namespace debug {

gutil::DevInitParam DevInit(uint32_t width, uint32_t height);
void DevTerm();

}
}
}
}

extern "C" {
/* the window, the screen size and the field of view, from the window */
paf::ui::wutil::InitParam *ScePafWidget_5245A0F2(paf::ui::wutil::InitParam *param,
						 const paf::gutil::WinHandle *whandle);
}

#endif
