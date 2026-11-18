/*
 * SceIniFileProcessor: the exports. An application holds an IniFileProcessor of two words (a table of functions
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

}

/* a configuration with nothing in it */
IniConfig::IniConfig()
{
	poolSize = 0;
	reserved = 0;
	context = NULL;
	ops = NULL;
}

IniFileProcessor::IniFileProcessor()
{
	IniHolder *holder = (IniHolder *)ScePafStdc_FC5CD359(sizeof(IniHolder));
	IniHolder *made = NULL;

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
	m_holder = made;
}

/* ends the implementation (when there is one) and gives the holder back */
IniFileProcessor::~IniFileProcessor()
{
	IniHolder *holder = m_holder;

	if (holder != NULL) {
		if (holder->processor != NULL) {
			holder->processor->clear();
			holder->processor->~IniProcessorBase();
			iniFree(&holder->allocator, holder->processor);
			holder->processor = NULL;
			holder = m_holder;
		}
		if (holder != NULL) {
			iniAllocatorRefRelease(&holder->allocator);
			ScePafStdc_1B77082E(holder);
		}
	}
}

/* gives the object its implementation: the allocator the configuration asks for, and a processor on it */
int IniFileProcessor::initialize(const IniConfig *config)
{
	IniHolder *holder = m_holder;
	int result;
	void *memory;

	if (holder == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY;
	holder->config.poolSize = config->poolSize;
	holder->config.ops = config->ops;
	holder->config.reserved = config->reserved;
	holder->config.context = config->context;
	holder = m_holder;
	if (holder->config.ops != NULL && holder->config.ops->alloc != NULL && holder->config.ops->free != NULL)
		result = iniAllocatorRefSet(&holder->allocator, &holder->config);
	else
		result = iniAllocatorRefCreatePool(&holder->allocator, holder->config.poolSize);
	if (result == 0) {
		memory = iniAlloc(&holder->allocator, sizeof(IniProcessor));
		IniProcessor *processor = NULL;

		if (memory != NULL)
			processor = new (memory) IniProcessor(&holder->allocator, &holder->config);
		m_holder->processor = processor;
		return processor->setConfig(&holder->config);
	}
	return result;
}

/* ends the implementation and gives its memory back, keeping the object */
void *IniFileProcessor::terminate()
{
	IniHolder *holder = m_holder;

	if (holder != NULL && holder->processor != NULL) {
		holder->processor->clear();
		holder->processor->~IniProcessorBase();
		iniFree(&holder->allocator, holder->processor);
		holder->processor = NULL;
		return NULL;
	}
	return this;
}

int IniFileProcessor::open(const char *path, const char *mode, int flags)
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->open(path, mode, flags);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::create(const char *path, const char *mode, int flags)
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->create(path);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::rewind()
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->rewind();
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::flush()
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->flush();
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::close()
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->close();
	return 0;
}

int IniFileProcessor::get(const char *key, char *buf, SceSize size, SceSize offset)
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->get(key, buf, size, offset);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::next(char *key, char *value, SceSize size)
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->next(key, value, size);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::set(const char *key, const char *value)
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->set(key, value);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::add(const char *key, const char *value)
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->add(key, value);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::remove(const char *key, SceSize offset)
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->remove(key);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::serialize(void **buf, int *length)
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->text(buf, length);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::deserialize(const char *buf, SceSize size)
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->parse(buf, size);
	return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
}

int IniFileProcessor::freeText()
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->freeText();
	return 0;
}

int IniFileProcessor::lastError()
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->lastError();
	return 0;
}

SceSize IniFileProcessor::count()
{
	IniProcessorBase *processor = m_holder->processor;

	if (processor != NULL)
		return processor->count();
	return 0;
}
