#include "view.h"
#include "../plugin.h"

extern void ScePafStdc_1B77082E(void *ptr);

static const struct {
	int offset_to_top;
	const void *type_info;
	ViewControllerVTable methods;
} viewControllerClass = {
	0, NULL, {
		ViewControllerDestroy, ViewControllerDelete,
		ViewControllerPluginOnInit, ViewControllerPluginOnStart,
		ViewControllerPluginOnStop, ViewControllerPluginOnExit
	}
};

static const struct {
	int offset_to_top;
	const void *type_info;
	ViewVTable methods;
} viewClass = {
	0, NULL, { ViewDestroy, ViewDelete }
};

static const char viewCallerName[] = "__main__";

View *ViewConstruct(View *self, const PafString *pluginName,
		    const PafString *resourcePath, ViewController *controller)
{
	extern size_t sce_paf_strlen(const char *text);
	PluginInitParam param;

	self->vtable = &viewClass.methods;
	self->m_controller = controller;
	PluginInitParamConstruct(&param);
	PafStringCopy(&param.name, pluginName);
	PafStringCopy(&param.resource_file, resourcePath);
	PafStringAssign(&param.caller_name, viewCallerName,
			sce_paf_strlen(viewCallerName));
	param.init_func = ViewPluginOnInit;
	param.start_func = ViewPluginOnStart;
	param.param = controller;
	param.stop_func = ViewPluginOnStop;
	param.exit_func = ViewPluginOnExit;
	PluginLoadAsync(&param, NULL, 0);
	PafStringDestroy(&param.module_file);
	PluginArgumentMapClear(&param.arguments);
	if (param.arguments._Myhead)
		ScePafStdc_1B77082E(param.arguments._Myhead);
	param.arguments._Myhead = (PluginArgumentMapNode *)-1;
	param.arguments._Mysize = (size_t)-1;
	PafStringDestroy(&param.resource_file);
	PafStringDestroy(&param.caller_name);
	PafStringDestroy(&param.name);
	return self;
}

View *ViewConstructBase(View *self, const PafString *pluginName,
			const PafString *resourcePath, ViewController *controller)
{
	return ViewConstruct(self, pluginName, resourcePath, controller);
}

View *ViewDestroy(View *self)
{
	self->vtable = &viewClass.methods;
	return self;
}

void ViewDelete(View *self)
{
	self->vtable = &viewClass.methods;
	ScePafStdc_1B77082E(self);
}

View *ViewDestroyBase(View *self)
{
	self->vtable = &viewClass.methods;
	return self;
}

void ViewControllerPluginOnInit(ViewController *self, Plugin *plugin)
{
}

void ViewControllerPluginOnStart(ViewController *self, Plugin *plugin)
{
}

void ViewControllerPluginOnStop(ViewController *self, Plugin *plugin)
{
}

void ViewControllerPluginOnExit(ViewController *self, Plugin *plugin)
{
}

void ViewPluginOnInit(Plugin *plugin)
{
	if (plugin) {
		ViewController *controller = plugin->param;

		if (controller)
			controller->vtable->PluginOnInit(controller, plugin);
	}
}

void ViewPluginOnStop(Plugin *plugin)
{
	if (plugin) {
		ViewController *controller = plugin->param;

		if (controller)
			controller->vtable->PluginOnStop(controller, plugin);
	}
}

void ViewPluginOnExit(Plugin *plugin)
{
	if (plugin) {
		ViewController *controller = plugin->param;

		if (controller)
			controller->vtable->PluginOnExit(controller, plugin);
	}
}

ViewController *ViewControllerDestroy(ViewController *self)
{
	self->vtable = &viewControllerClass.methods;
	return self;
}

void ViewControllerDelete(ViewController *self)
{
	self->vtable = &viewControllerClass.methods;
	ScePafStdc_1B77082E(self);
}
