#include "plugin.h"

static const char pluginArgumentEmpty[] = "";

PluginInitParam2 *PluginInitParam2Construct(PluginInitParam2 *self)
{
	self->draw_priority = 150;
	self->surface_pool_size = 0;
	self->texture_load_filter = 0;
	self->graphics_flag = 128;
	return self;
}

int32_t PluginArgumentToInt(const PluginArgument *self)
{
	if (self->type == ArgumentType_Int)
		return self->i;
	if (self->type == ArgumentType_Float)
		return (int32_t)self->f;
	return 0;
}

float PluginArgumentToFloat(const PluginArgument *self)
{
	if (self->type == ArgumentType_Int)
		return (float)self->i;
	if (self->type == ArgumentType_Float)
		return self->f;
	return 0.0f;
}

PafString *PluginArgumentToString(PafString *result, const PluginArgument *self)
{
	extern size_t sce_paf_strlen(const char *text);

	if (self->type == ArgumentType_String) {
		PafStringInitialize(result);
		return PafStringCopy(result, &self->str);
	}
	PafStringInitialize(result);
	return PafStringAssign(result, pluginArgumentEmpty,
			       sce_paf_strlen(pluginArgumentEmpty));
}

PluginInitParam2 *PluginInitParam2ConstructBase(PluginInitParam2 *self)
{
	self->draw_priority = 150;
	self->surface_pool_size = 0;
	self->texture_load_filter = 0;
	self->graphics_flag = 128;
	return self;
}

void PluginStop(Plugin *self)
{
	if (self->stop_func)
		self->stop_func(self);
}

PluginPageOpenParam *PluginPageOpenParamConstruct(PluginPageOpenParam *self)
{
	self->fade = 0;
	self->draw_layer = 0;
	self->draw_mask = 0;
	self->overwrite_draw_priority = -1;
	self->option = PageOption_None;
	self->event_handler_table = NULL;
	self->fade_time_ms = 200.0f;
	self->transition_type = TransitionType_None;
	self->timer = NULL;
	self->env_param = NULL;
	self->graphics_flag = 128;
	return self;
}

void PluginStart(Plugin *self)
{
	if (self->rscobj && (self->option & Option_AutoOpen)) {
		CxmlElement table, page;

		ResourceObjGetTableElement(&table, self->rscobj, TableType_Page);
		if (table.doc && table.offset >= 0) {
			CxmlElementGetFirstChild(&page, &table);
			if (page.doc && page.offset >= 0) {
				PluginPageOpenParam param;

				PluginPageOpenParamConstruct(&param);
				PluginPageOpen(self, &page, &param);
				if (self->option & Option_AutoActivate)
					PluginPageActivate(self, &page);
				if (param.timer)
					param.timer->vtable->Delete(param.timer);
			}
		}
	}
	if (self->start_func)
		self->start_func(self);
}

PluginPageOpenParam *PluginPageOpenParamConstructBase(PluginPageOpenParam *self)
{
	self->fade = 0;
	self->draw_layer = 0;
	self->draw_mask = 0;
	self->overwrite_draw_priority = -1;
	self->option = PageOption_None;
	self->event_handler_table = NULL;
	self->fade_time_ms = 200.0f;
	self->transition_type = TransitionType_None;
	self->timer = NULL;
	self->env_param = NULL;
	self->graphics_flag = 128;
	return self;
}

void PluginGetArg(PluginArgument *result, const Plugin *self,
		  const char *key, unsigned char *exist)
{
	extern size_t sce_paf_strlen(const char *text);
	PluginArgumentMapNode *found;
	PafString name;
	size_t len = 0;

	PafStringInitialize(&name);
	if (key)
		len = sce_paf_strlen(key);
	PafStringAssign(&name, key, len);
	PluginArgumentMapFind(&found, &self->arguments, &name);
	PafStringDestroy(&name);
	if (found == self->arguments._Myhead) {
		if (exist)
			*exist = 0;
		result->type = ArgumentType_Int;
		result->i = 0;
		PafStringInitialize(&result->str);
	} else {
		const PluginArgument *argument = &found->_Myval.second;

		if (exist)
			*exist = 1;
		result->type = argument->type;
		result->i = argument->i;
		PafStringInitialize(&result->str);
		PafStringCopy(&result->str, &argument->str);
	}
}

void PluginNotify(const Plugin *self, int32_t status)
{
	PluginNotifyNode *head = self->notify_callback_list.root_node;
	PluginNotifyNode *node = head ? head->next : NULL;

	while (node != head) {
		if (node->val) {
			node->val(status, self->param);
			head = self->notify_callback_list.root_node;
		}
		if (node)
			node = node->next;
	}
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

void PluginArgumentMapFind(PluginArgumentMapNode **result,
			   const PluginArgumentMap *self, const PafString *key)
{
	extern int sce_paf_strcmp(const char *left, const char *right);
	PluginArgumentMapNode *node = NULL;

	if (self->_Myhead) {
		node = self->_Myhead->_Parent;
		while (!node->_Isnil) {
			const char *left = key->ptr;
			const char *right = node->_Myval.first.ptr;

			if ((unsigned char)*left < (unsigned char)*right ||
			    sce_paf_strcmp(left, right) < 0) {
				node = node->_Left;
			} else {
				left = node->_Myval.first.ptr;
				right = key->ptr;
				if ((unsigned char)*left >= (unsigned char)*right &&
				    sce_paf_strcmp(left, right) >= 0)
					break;
				node = node->_Right;
			}
		}
	}
	*result = node;
}
