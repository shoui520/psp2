# SceUlt (vs0:sys/external/libult.suprx), built by Sony with SNC 3.57 in C++: the user-level
# threads (ulthreads on fibers, run by worker threads) and their synchronization objects
# (mutex, condition variable, semaphore, reader-writer lock, queue) over non-blocking
# queues, at Sony's paths (internal/usermode/src/ult/src in the SDK's build paths).
enable_language(CXX)
psp2_module(vs0_sys_external_libult KIND user COMPILER snc PARTIAL SOURCES
	src/AbstractThread.cpp
	src/Cinterface.cpp
	src/CinterfaceInternal.cpp
	src/KernelThread.cpp
	src/prx_start_stop.c
	src/Queue.cpp
	src/ThreadFactory.cpp
	src/ult_util.cpp
	src/Ulthread.cpp
	src/WorkerThread.cpp
)
