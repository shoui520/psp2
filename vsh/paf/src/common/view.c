#include "view.h"

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
