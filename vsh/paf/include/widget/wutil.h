/*
 * paf::ui::wutil, as far as converted C++ uses it. Names after ScePaf-RE.
 */
#ifndef PAF_WIDGET_WUTIL_H
#define PAF_WIDGET_WUTIL_H

namespace paf {

class Plugin;

namespace ui {

/* told when a plugin starts, before its own start function */
class MessageDialogListener {
public:
	virtual ~MessageDialogListener();
	virtual void PluginOnStart(Plugin *plugin);
};

namespace wutil {

MessageDialogListener *GetMessageDialogListener();

}
}
}

#endif
