/* SSLeay runtime callbacks/control with measured SSL-C adaptation patches.
 * The binding names and this wrapper's filename are ours, not Sony names.
 */
#include "runtime.h"
#include "memory.h"
#include "format.h"
#include "buffer/buffer.h"
#include "stack.h"

#define CRYPTO_NUM_LOCKS 39
#define CRYPTO_LOCK_APP 24
#define CRYPTO_num_locks sslNumLocks
#define CRYPTO_get_new_lockid sslGetNewLockId
#define CRYPTO_get_lock_name sslGetLockName
#define CRYPTO_lock sslLock
#define CRYPTO_get_locking_callback sslGetLockCallback
#define CRYPTO_get_add_lock_callback sslGetAddLockCallback
#define CRYPTO_set_locking_callback sslSetLockCallback
#define CRYPTO_set_add_lock_callback sslSetAddLockCallback
#define CRYPTO_add_lock sslAddReference
#define CRYPTO_get_id_callback sslGetThreadCallback
#define CRYPTO_set_id_callback sslSetThreadCallback
#define CRYPTO_thread_id sslThreadId
#define locking_callback sslLockCallback
#define add_lock_callback sslAddLockCallback
#define lock_names sslLockNames
#define app_locks sslAppLocks
#define id_callback sslThreadCallback
#define Free(ptr) sslFreeCallback(ptr)
#define strcmp sslCompareCString

#include "cryptlib_sce.inc"

/* The upstream memory-control commands; only this state operation is selected. */
#define CRYPTO_MEM_CHECK_OFF 0
#define CRYPTO_MEM_CHECK_ON 1
#define CRYPTO_MEM_CHECK_ENABLE 2
#define CRYPTO_MEM_CHECK_DISABLE 3
#define CRYPTO_mem_ctrl sslUpdateRuntimeFlags
#define mh_mode sslRuntimeFlags

#include "mem_ctrl_sce.inc"
