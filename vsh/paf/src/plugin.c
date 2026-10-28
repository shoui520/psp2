#include "plugin.h"

PluginInitParam2 *PluginInitParam2Construct(PluginInitParam2 *self)
{
	self->draw_priority = 150;
	self->surface_pool_size = 0;
	self->texture_load_filter = 0;
	self->graphics_flag = 128;
	return self;
}

PluginInitParam2 *PluginInitParam2ConstructBase(PluginInitParam2 *self)
{
	self->draw_priority = 150;
	self->surface_pool_size = 0;
	self->texture_load_filter = 0;
	self->graphics_flag = 128;
	return self;
}

PluginInitParam *PluginInitParamConstruct(PluginInitParam *self)
{
	PluginArgumentMapNode *head;

	PafStringInitialize(&self->name);
	PafStringInitialize(&self->caller_name);
	self->set_param_func = NULL;
	self->init_func = NULL;
	self->start_func = NULL;
	self->stop_func = NULL;
	self->exit_func = NULL;
	PafStringInitialize(&self->resource_file);
	self->resource_data = NULL;
	self->resource_data_size = 0;
	self->locale = Locale_Default;
	self->private_heap_size = 0;
	self->private_heap = NULL;
	self->decode_heap_size = 0;
	self->decode_heap = NULL;
	self->surface_pool = NULL;
	self->draw_priority = -1;
	self->arguments._Mysize = 0;
	head = ScePafStdc_FC5CD359(sizeof(*head));
	if (!head)
		ScePafStdc_D608246A();
	self->arguments._Myhead = head;
	if (head) {
		head->_Parent = head;
		head->_Left = head;
		head->_Right = head;
		head->_Color = 0;
		self->arguments._Myhead->_Isnil = 1;
	}
	PafStringInitialize(&self->module_file);
	self->module_interface_version = 0;
	self->module_option = 0;
	self->option = Option_Default;
	self->notify_callback = NULL;
	self->unload_callback = NULL;
	self->param = NULL;
	self->env_param = NULL;
	self->graphics_flag = 128;
	return self;
}

PluginInitParam *PluginInitParamConstructBase(PluginInitParam *self)
{
	return PluginInitParamConstruct(self);
}

PluginArgumentMap *PluginArgumentMapClear(PluginArgumentMap *self)
{
	PluginArgumentMapNode *head = self->_Myhead;
	PluginArgumentMapNode *node, *parent;

	if (!head)
		return self;
	node = head->_Left;
	while (!node->_Isnil) {
		if (!node->_Left->_Isnil) {
			node = node->_Left;
		} else if (!node->_Right->_Isnil) {
			node = node->_Right;
		} else {
			char *key;

			parent = node->_Parent;
			if (!parent->_Isnil) {
				if (parent->_Left == node)
					parent->_Left = head;
				else
					parent->_Right = head;
			}
			if (node->_Myval.second.str.ptr != &PafStringEmpty)
				ScePafStdc_1B77082E(node->_Myval.second.str.ptr);
			key = node->_Myval.first.ptr;
			node->_Myval.second.str.ptr = (char *)-1;
			node->_Myval.second.str.count = (size_t)-1;
			if (key != &PafStringEmpty)
				ScePafStdc_1B77082E(key);
			node->_Myval.first.ptr = (char *)-1;
			node->_Myval.first.count = (size_t)-1;
			ScePafStdc_1B77082E(node);
			head = self->_Myhead;
			node = parent;
		}
	}
	head->_Parent = head;
	head->_Left = head;
	head->_Right = head;
	self->_Mysize = 0;
	return NULL;
}
