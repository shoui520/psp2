/*
 * paf::Module: the modules loaded, by path. A Module of a path already
 * loaded shares its ModuleImpl and counts a reference; the last reference
 * let go unloads the module.
 */
#include <module/module.h>
#include <std/string.h>
#include <thread/thread.h>
#include <kernel/processmgr.h>

using namespace paf;

static SceUInt64 s_last_time = 0;
static map<string, ModuleImpl *> s_module_map;
static thread::Mutex s_mutex("ScePafModuleMutex", 0);

void Module::SetInterface(int32_t version, void *interface)
{
	impl->interface_table[version] = interface;
}

void *Module::GetInterface(int32_t version) const
{
	map<int32_t, void *>::iterator it = impl->interface_table.find(version);

	if (it == impl->interface_table.end())
		return NULL;
	return it->second;
}

int32_t Module::GetStatus()
{
	return impl->result;
}

SceUInt64 ScePafMisc_A8133C0A(void)
{
	return s_last_time;
}

Module::Module(const char *name, const char *funcname, int32_t option,
	       const ExtraOption *extra_option)
{
	s_mutex.Lock();
	s_last_time = sceKernelGetProcessTimeWide();

	map<string, ModuleImpl *>::iterator it = s_module_map.find(string(name));

	if (it != s_module_map.end()) {
		impl = it->second;
		impl->ref_count++;
		s_mutex.Unlock();
	} else {
		impl = new ModuleImpl(name, option, extra_option);
		s_module_map[string(name)] = impl;
		s_mutex.Unlock();
		impl->Start(this, funcname);
	}
}

Module::~Module()
{
	s_mutex.Lock();
	impl->ref_count--;
	s_last_time = sceKernelGetProcessTimeWide();
	if (impl->ref_count != 0) {
		s_mutex.Unlock();
		return;
	}
	s_module_map.erase(impl->name);
	s_mutex.Unlock();
	delete impl;
}
