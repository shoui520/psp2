/*
 * The widget factories: for each widget type the functions that make the
 * widget and its parameters, which the Framework registers with Plugin by the
 * type's name. A table's name is its type's TypeName(), set when libpaf is
 * loaded.
 */
#ifndef PAF_WIDGET_FACTORY_H
#define PAF_WIDGET_FACTORY_H

namespace paf {

class VersatileParam;

namespace graph {
class DrawObj;
}

namespace ui {

class Widget;
class WidgetParam;

class FactoryFuncTable {
public:
	const char *m_name;
	Widget *(*m_factory_widget)(Widget *parent);
	WidgetParam *(*m_factory_param)();
	VersatileParam *(*m_factory_layout)();
	/* the type's WidgetFocusParam */
	VersatileParam *(*m_factory_resource_param)();
	VersatileParam *(*m_factory_style)();
	VersatileParam *(*m_factory_handler)();
	VersatileParam *(*m_factory_sound)();
};

/* a draw object's factory, by its parameter's ParamName() */
class FactoryFuncDrawObj {
public:
	const char *m_name;
	graph::DrawObj *(*m_factory_drawobj)();
	VersatileParam *(*m_factory_param)();
};

}
}

extern paf::ui::FactoryFuncTable g_factoryGroup;
extern paf::ui::FactoryFuncTable g_factoryPlane;
extern paf::ui::FactoryFuncTable g_factoryButtonBase;
extern paf::ui::FactoryFuncTable g_factoryButton;
extern paf::ui::FactoryFuncTable g_factoryPlaneButton;
extern paf::ui::FactoryFuncTable g_factoryImageButton;
extern paf::ui::FactoryFuncTable g_factoryRadioButton;
extern paf::ui::FactoryFuncTable g_factoryCornerButton;
extern paf::ui::FactoryFuncTable g_factoryRadioBox;
extern paf::ui::FactoryFuncTable g_factoryCheckBox;
extern paf::ui::FactoryFuncTable g_factorySupplier;
extern paf::ui::FactoryFuncTable g_factoryText;
extern paf::ui::FactoryFuncTable g_factoryTextBox;
extern paf::ui::FactoryFuncTable g_factoryScrollViewBase;
extern paf::ui::FactoryFuncTable g_factoryScrollView;
extern paf::ui::FactoryFuncTable g_factoryListView;
extern paf::ui::FactoryFuncTable g_factoryListItem;
extern paf::ui::FactoryFuncTable g_factoryBox;
extern paf::ui::FactoryFuncTable g_factoryDialog;
extern paf::ui::FactoryFuncTable g_factoryScrollBar;
extern paf::ui::FactoryFuncTable g_factoryScrollBarTouch;
extern paf::ui::FactoryFuncTable g_factoryScrollBar2d;
extern paf::ui::FactoryFuncTable g_factorySlideBar;
extern paf::ui::FactoryFuncTable g_factoryImageSlideBar;
extern paf::ui::FactoryFuncTable g_factoryProgressBar;
extern paf::ui::FactoryFuncTable g_factoryProgressBarTouch;
extern paf::ui::FactoryFuncTable g_factoryFileList;
extern paf::ui::FactoryFuncTable g_factorySlidingDrawer;
extern paf::ui::FactoryFuncTable g_factoryRichText;
extern paf::ui::FactoryFuncTable g_factoryDirectBar;
extern paf::ui::FactoryFuncTable g_factoryBusyIndicator;
/* the factories libpaf does not export, named as Sony named the exported ones */
extern paf::ui::FactoryFuncTable g_factorySpeechBalloon;
extern paf::ui::FactoryFuncTable g_factoryNumSpin;
extern paf::ui::FactoryFuncTable g_factoryCompositeDrawer;
extern paf::ui::FactoryFuncTable g_factoryCompositeButton;
extern paf::ui::FactoryFuncTable g_factoryAppIconSimple;
extern paf::ui::FactoryFuncDrawObj g_factoryPlaneObj;
extern paf::ui::FactoryFuncDrawObj g_factoryTextObj;

#endif
