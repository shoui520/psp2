/*
 * paf::ui::Environment, as far as converted C++ uses it: the widget system's
 * state for one framework.
 */
#ifndef PAF_WIDGET_CORE_ENVIRONMENT_H
#define PAF_WIDGET_CORE_ENVIRONMENT_H

namespace paf {
namespace ui {

class EnvironmentParam {
public:
	virtual ~EnvironmentParam();
};

class Environment {
public:
	virtual ~Environment();

	/* whether nothing the widgets still run keeps the framework from ending */
	bool IsTermAvailable();
};

}
}

#endif
