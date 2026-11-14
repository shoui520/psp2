/*
 * xnotify.c: the condition variables to notify when a thread exits
 * (notify_all_at_thread_exit), in blocks of NITEMS
 */
#include <stdlib.h>

#include "xthrpsp2.h"

#define NITEMS	20

typedef struct {	/* a condition variable to notify, and its mutex */
	_Thrd_t id;
	_Mtx_t *mtx;
	_Cnd_t *cnd;
	int *res;
} _At_thread_exit_data;

typedef struct _At_thread_exit_block {
	_At_thread_exit_data data[NITEMS];
	int num_used;
	struct _At_thread_exit_block *next;
} _At_thread_exit_block;

static _Once_flag mtx_once = _ONCE_FLAG_INIT;
static _Mtx_t mtx;
static _At_thread_exit_block _Thread_exit_data;

static void init_mtx(void);

int _Cnd_register_at_thread_exit(_Cnd_t *cnd, _Mtx_t *mtx_arg, int *p)
{
	_At_thread_exit_block *block = &_Thread_exit_data;

	_Call_once(&mtx_once, init_mtx);
	if (_Mtx_lock(&mtx) != _Thrd_success)
		return _Thrd_error;
	while (block != NULL) {
		if (block->num_used == NITEMS) {	/* full: the next block */
			if (block->next == NULL)
				block->next = calloc(1, sizeof (_At_thread_exit_block));
			block = block->next;
		} else {	/* a free slot */
			int i;

			for (i = 0; i < NITEMS; ++i) {
				if (block->data[i].mtx == NULL) {
					block->data[i].id = _Thrd_current();
					block->data[i].mtx = mtx_arg;
					block->data[i].cnd = cnd;
					block->data[i].res = p;
					++block->num_used;
					break;
				}
			}
			block = NULL;
		}
	}
	return _Mtx_unlock(&mtx);
}

int _Cnd_unregister_at_thread_exit(_Mtx_t *mtx_arg)
{
	_At_thread_exit_block *block = &_Thread_exit_data;

	_Call_once(&mtx_once, init_mtx);
	if (_Mtx_lock(&mtx) != _Thrd_success)
		return _Thrd_error;
	while (block != NULL) {
		int i;

		for (i = 0; block->num_used != 0 && i < NITEMS; ++i) {
			if (block->data[i].mtx == mtx_arg) {	/* clear the entry */
				block->data[i].mtx = NULL;
				--block->num_used;
			}
		}
		block = block->next;
	}
	return _Mtx_unlock(&mtx);
}

int _Cnd_do_broadcast_at_thread_exit(void)
{
	_At_thread_exit_block *block = &_Thread_exit_data;
	_Thrd_t current;

	_Call_once(&mtx_once, init_mtx);
	if (_Mtx_lock(&mtx) != _Thrd_success)
		return _Thrd_error;
	current = _Thrd_current();
	while (block != NULL) {
		int i;

		for (i = 0; block->num_used != 0 && i < NITEMS; ++i) {
			if (block->data[i].mtx != NULL && _Thrd_equal(block->data[i].id, current)) {
				if (block->data[i].res != NULL)
					*block->data[i].res = 1;
				_Mtx_unlock(block->data[i].mtx);
				_Cnd_broadcast(block->data[i].cnd);
				block->data[i].mtx = NULL;
				--block->num_used;
			}
		}
		block = block->next;
	}
	return _Mtx_unlock(&mtx);
}

static void init_mtx(void)
{
	if (_Mtx_init(&mtx, _Mtx_plain) != _Thrd_success)
		abort();
}
