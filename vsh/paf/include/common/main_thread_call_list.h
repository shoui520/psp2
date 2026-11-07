/*
 * paf::common::MainThreadCallList: functions called once on the main thread, at its next timer
 * tick (src/common/main_thread_call_list.c). Names from the mangled exports
 * (_ZN3paf6common18MainThreadCallList8RegisterEPFvPvES2_, ...10UnregisterEPFvPvES2_).
 */
#ifndef PAF_COMMON_MAIN_THREAD_CALL_LIST_H
#define PAF_COMMON_MAIN_THREAD_CALL_LIST_H

namespace paf {
namespace common {

class MainThreadCallList {
public:
	typedef void (*Function)(void *userdata);

	/* the function called with userdata (export 6735B503) */
	static void Register(Function func, void *userdata);
	/* the registered function and userdata taken off again (export 4A8115D5) */
	static void Unregister(Function func, void *userdata);
};

}
}

#endif
