/*
 * paf::ui::wutil, as far as converted C++ uses it: the widget system's setup
 * and the listeners it hands out. Names after ScePaf-RE.
 */
#ifndef PAF_WIDGET_WUTIL_H
#define PAF_WIDGET_WUTIL_H

#include <stdint.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface.h>
#include <std/string>
#include <math/ngp/v4.h>

namespace paf {

class Plugin;

namespace ui {

class Environment;
class Handler;
class TextBoxEditListener;
class TextBoxEditMenuListener;

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
	/* the text set up with the graphics when not NULL */
	const graph::TextInitParam *text_param;
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
int32_t Term();
void DevTerm();
int32_t DevInit(const gutil::DevInitParam &param);
void SetEnv(Environment *env);

/* what the Environment is sent: the event is put in its queue for the next frame */
void PostTouchScreenEvent(Environment *env, int32_t type, int32_t state, int32_t x, int32_t y,
			  int32_t z, int32_t touch_id);
void PostKeycodeEvent(Environment *env, int32_t type, int32_t port, int32_t keydev,
		      int32_t keycode, int32_t modify, int32_t repeat, int32_t mode);
void PostStateEvent(Environment *env, int32_t type, Handler *target);

/* the listeners Init was given, or made (the edit listener) */
TextBoxEditListener *GetEditListener();
void *GetWebEditListener();
MessageDialogListener *GetMessageDialogListener();
void *GetWebViewListener();

namespace debug {

gutil::DevInitParam DevInit(uint32_t width, uint32_t height);
void DevTerm();
/* gives back the memory a program without the Framework had */
int32_t Term(InitParam &param);
int32_t vec4_printf(char *str, const math::v4 &data);
/* the file loaded as a surface of the default surface pool (0, or an error) */
int32_t LoadGimTexture(const string &path, intrusive_ptr<graph::Surface> &surface);
int32_t LoadPngTexture(const string &path, intrusive_ptr<graph::Surface> &surface);

}
}
}
}

extern "C" {
/* the window, the screen size and the field of view, from the window */
paf::ui::wutil::InitParam *ScePafWidget_5245A0F2(paf::ui::wutil::InitParam *param,
						 const paf::gutil::WinHandle *whandle);
/* what a program without the Framework is made with, from its window (the text set up with the
   graphics is the one it keeps) */
paf::ui::wutil::InitParam ScePafWidget_63D72B1C(const paf::gutil::WinHandle *whandle);
/* the edit menu listener, and the two listeners the names of which are not known */
paf::ui::TextBoxEditMenuListener *ScePafWidget_1EE9D37E(void);
void *ScePafWidget_A55215E5(void);
void *ScePafWidget_E638AA1F(void);
/* the frame begun and ended on the root screen, or in the Environment set with SetEnv; the
   input devices updated when asked to */
int32_t ScePafWidget_095117B9(bool update_input);
}

#endif
