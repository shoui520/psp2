/*
 * SceIniFileProcessor: the exports. An application holds a processor object of two words (a table of functions
 * and a pointer to what the library keeps for it); its methods call the implementation's.
 */

#include "ini_internal.h"

extern "C" void ScePafStdc_F36BABE3(void *handle);	/* the module's end: runs what was registered to run at exit */

/* what the library keeps for a processor object: the implementation, the application's configuration and the
 * allocator the implementation takes its memory from */
struct IniHolder {
	IniProcessorBase *processor;
	IniConfig config;
	bool reserved;
	IniAllocatorRef allocator;
};

/* the application's processor object */
struct IniProcessorObject {
	const void *const *vtable;
	IniHolder *holder;
};

void *__dso_handle;	/* 0x81005008 */

extern "C" {

__attribute__((noinline)) static void *iniModuleStart(void)
{
	__dso_handle = &__dso_handle;
	return &__dso_handle + 1;
}

__attribute__((noinline)) static int iniModuleStop(void)
{
	ScePafStdc_F36BABE3(&__dso_handle);
	return 0;
}

int module_start(SceSize arglen, const void *argp)
{
	iniModuleStart();
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	iniModuleStop();
	return 0;
}

/* a configuration with nothing in it */
IniConfig *SceIniFileProcessor_14146AF5(IniConfig *config)
{
	config->poolSize = 0;
	config->reserved = 0;
	config->context = NULL;
	config->ops = NULL;
	return config;
}

IniConfig *SceIniFileProcessor_9F8FC03F(IniConfig *config)
{
	config->poolSize = 0;
	config->reserved = 0;
	config->context = NULL;
	config->ops = NULL;
	return config;
}

IniProcessorObject *SceIniFileProcessor_A0F71A2C(IniProcessorObject *object);
IniProcessorObject *SceIniFileProcessor_746B194F(IniProcessorObject *object);
IniProcessorObject *SceIniFileProcessor_ACEB88BD(IniProcessorObject *object);
int SceIniFileProcessor_9F57E743(IniProcessorObject *object);

/* the table of the processor object's functions: its destructors, then nothing */
extern const void *const g_iniProcessorObjectTable[4];
const void *const g_iniProcessorObjectTable[4] = {
	(const void *)SceIniFileProcessor_ACEB88BD,
	(const void *)SceIniFileProcessor_9F57E743,
	NULL,
	NULL,
};

static IniProcessorObject *iniProcessorObjectInit(IniProcessorObject *object)
{
	IniHolder *holder;
	IniHolder *made = NULL;

	object->vtable = g_iniProcessorObjectTable;
	holder = (IniHolder *)ScePafStdc_FC5CD359(sizeof(IniHolder));
	if (holder != NULL) {
		holder->processor = NULL;
		holder->config.poolSize = 0;
		holder->config.reserved = 0;
		holder->config.ops = NULL;
		holder->config.context = NULL;
		holder->reserved = false;
		iniAllocatorRefInit(&holder->allocator);
		made = holder;
	}
	object->holder = made;
	return object;
}

IniProcessorObject *SceIniFileProcessor_A0F71A2C(IniProcessorObject *object)
{
	return iniProcessorObjectInit(object);
}

IniProcessorObject *SceIniFileProcessor_746B194F(IniProcessorObject *object)
{
	return iniProcessorObjectInit(object);
}

/* ends the implementation (when there is one) and takes the object's words back */
IniProcessorObject *SceIniFileProcessor_ACEB88BD(IniProcessorObject *object)
{
	IniHolder *holder = object->holder;

	object->vtable = g_iniProcessorObjectTable;
	if (holder != NULL) {
		if (holder->processor != NULL) {
			holder->processor->clear();
			holder->processor->~IniProcessorBase();
			iniFree(&holder->allocator, holder->processor);
			holder->processor = NULL;
			holder = object->holder;
		}
		if (holder != NULL) {
			iniAllocatorRefRelease(&holder->allocator);
			ScePafStdc_1B77082E(holder);
		}
	}
	return object;
}

int SceIniFileProcessor_9F57E743(IniProcessorObject *object)
{
	SceIniFileProcessor_ACEB88BD(object);
	ScePafStdc_1B77082E(object);
	return 0;
}

IniProcessorObject *SceIniFileProcessor_7169CE28(IniProcessorObject *object)
{
	return SceIniFileProcessor_ACEB88BD(object);
}


/* gives the object its implementation: the allocator the configuration asks for, and a processor on it */
int SceIniFileProcessor_0975D396(IniProcessorObject *object, const IniConfig *config)
{
	IniHolder *holder = object->holder;
	int result;
	void *memory;

	if (holder == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY;
	holder->config.poolSize = config->poolSize;
	holder->config.ops = config->ops;
	holder->config.reserved = config->reserved;
	holder->config.context = config->context;
	holder = object->holder;
	if (holder->config.ops != NULL && holder->config.ops->alloc != NULL && holder->config.ops->free != NULL)
		result = iniAllocatorRefSet(&holder->allocator, &holder->config);
	else
		result = iniAllocatorRefCreatePool(&holder->allocator, holder->config.poolSize);
	if (result == 0) {
		memory = iniAlloc(&holder->allocator, sizeof(IniProcessor));
		IniProcessor *processor = NULL;

		if (memory != NULL)
			processor = new (memory) IniProcessor(&holder->allocator, &holder->config);
		object->holder->processor = processor;
		return processor->setConfig(&holder->config);
	}
	return result;
}

/* ends the implementation and gives its memory back, keeping the object */
void *SceIniFileProcessor_2BEF7391(IniProcessorObject *object)
{
	IniHolder *holder = object->holder;

	if (holder != NULL && holder->processor != NULL) {
		holder->processor->clear();
		holder->processor->~IniProcessorBase();
		iniFree(&holder->allocator, holder->processor);
		holder->processor = NULL;
		return NULL;
	}
	return object;
}

int SceIniFileProcessor_51B791E8(IniProcessorObject *object, const char *path, const char *mode, int flags)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->open(path, mode, flags);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_FD8DE2F8(IniProcessorObject *object, const char *path)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->create(path);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_B48C756B(IniProcessorObject *object)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->rewind();
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_A51840C7(IniProcessorObject *object)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->flush();
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_EDFAD6B4(IniProcessorObject *object)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->close();
	return 0;
}

int SceIniFileProcessor_7F22CED1(IniProcessorObject *object, const char *key, char *buf, SceSize size, SceSize offset)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->get(key, buf, size, offset);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_D5C7B3EE(IniProcessorObject *object, char *key, char *value, SceSize size)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->next(key, value, size);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_DBC5F9A8(IniProcessorObject *object, const char *key, const char *value)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->set(key, value);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_703E1BAE(IniProcessorObject *object, const char *key, const char *value)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->add(key, value);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_C2B3A41C(IniProcessorObject *object, const char *key)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->remove(key);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_109B4E83(IniProcessorObject *object, void **buf, int *length)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->text(buf, length);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_B785FE67(IniProcessorObject *object, const char *buf, SceSize size)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->parse(buf, size);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int SceIniFileProcessor_655D31C6(IniProcessorObject *object)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->freeText();
	return 0;
}

int SceIniFileProcessor_48A6BDCB(IniProcessorObject *object)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->lastError();
	return 0;
}

SceSize SceIniFileProcessor_D7648B61(IniProcessorObject *object)
{
	IniProcessorBase *processor = object->holder->processor;

	if (processor != NULL)
		return processor->count();
	return 0;
}

}
