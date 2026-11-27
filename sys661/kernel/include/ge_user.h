/* Copyright (C) The uOFW team
   See the file COPYING for copying permission.
*/

/**
 * @author artart78
 * @version 6.60
 *
 * The ge.prx module RE'ing.
 */

#include "common_header.h"

/** @defgroup GE sceGE_Manager Module
 *
 * @{
 */

/*
 * List of commands accepted by the GE.
 */
#include "ge_cmds.h"

#define SCE_GE_MTX_BONEA 0
#define SCE_GE_MTX_BONEB 1
#define SCE_GE_MTX_BONEC 2
#define SCE_GE_MTX_BONED 3
#define SCE_GE_MTX_BONEE 4
#define SCE_GE_MTX_BONEF 5
#define SCE_GE_MTX_BONEG 6
#define SCE_GE_MTX_BONEH 7
#define SCE_GE_MTX_WORLD 8
#define SCE_GE_MTX_VIEW  9
#define SCE_GE_MTX_PROJ  10
#define SCE_GE_MTX_TGEN  11
#define SCE_GE_MTX_COUNT 12

/*
 * List of signals which can be sent using the SIGNAL command.
 */

/**
 * In SDK versions <= 0x02000010, pause the display list, call the callback with the SIGNAL argument,
 * and restart the display list in the state specified in the END instruction.
 * Otherwise, just call the callback. Resume GE execution afterwards.
 */
#define SCE_GE_SIGNAL_HANDLER_SUSPEND  0x01
/**
 * Resume GE execution, then call the signal callback with the SIGNAL argument.
 */
#define SCE_GE_SIGNAL_HANDLER_CONTINUE 0x02
/**
 * Set the current display list's status to PAUSE, its signal to the END argument and its signal data
 * to the SIGNAL command, then resume GE execution.
 */
#define SCE_GE_SIGNAL_HANDLER_PAUSE    0x03
/**
 * Set the current display list's signal to SYNC, then resume GE execution.
 */
#define SCE_GE_SIGNAL_SYNC             0x08
/**
 * Jump to the (s << 16) | (e & 0xFFFF) address, where s is SIGNAL's argument and e is END's
 */
#define SCE_GE_SIGNAL_JUMP             0x10
/**
 * Same as SCE_GE_SIGNAL_JUMP, but saving the status so we can use SCE_GE_SIGNAL_RET to return to the caller.
 */
#define SCE_GE_SIGNAL_CALL             0x11
/**
 * Return after using a SCE_GE_SIGNAL_*CALL signal.
 */
#define SCE_GE_SIGNAL_RET              0x12
/**
 * Same as SCE_GE_SIGNAL_JUMP, but where the address is relative to the current one.
 */
#define SCE_GE_SIGNAL_RJUMP            0x13
/**
 * Same as SCE_GE_SIGNAL_CALL, but where the address is relative to the current one.
 */
#define SCE_GE_SIGNAL_RCALL            0x14
/**
 * Same as SCE_GE_SIGNAL_JUMP, but where the address is relative to ORIGIN.
 */
#define SCE_GE_SIGNAL_OJUMP            0x15
/**
 * Same as SCE_GE_SIGNAL_CALL, but where the address is relative to ORIGIN.
 */
#define SCE_GE_SIGNAL_OCALL            0x16
/**
 * Run a TBP0/TBW0 pair with the same address as for SCE_GE_SIGNAL_RJUMP,
 * taking ((e >> 16) & 0xFF) for the size.
 */
#define SCE_GE_SIGNAL_RTBP0            0x20
#define SCE_GE_SIGNAL_RTBP1            0x21
#define SCE_GE_SIGNAL_RTBP2            0x22
#define SCE_GE_SIGNAL_RTBP3            0x23
#define SCE_GE_SIGNAL_RTBP4            0x24
#define SCE_GE_SIGNAL_RTBP5            0x25
#define SCE_GE_SIGNAL_RTBP6            0x26
#define SCE_GE_SIGNAL_RTBP7            0x27
/**
 * Run a TBP0/TBW0 pair with the same address as for SCE_GE_SIGNAL_OJUMP,
 * taking ((e >> 16) & 0xFF) for the size.
 */
#define SCE_GE_SIGNAL_OTBP0            0x28
#define SCE_GE_SIGNAL_OTBP1            0x29
#define SCE_GE_SIGNAL_OTBP2            0x2A
#define SCE_GE_SIGNAL_OTBP3            0x2B
#define SCE_GE_SIGNAL_OTBP4            0x2C
#define SCE_GE_SIGNAL_OTBP5            0x2D
#define SCE_GE_SIGNAL_OTBP6            0x2E
#define SCE_GE_SIGNAL_OTBP7            0x2F
/**
 * Same as SCE_GE_SIGNAL_RTBP0, for a CBP/CBW pair.
 */
#define SCE_GE_SIGNAL_RCBP             0x30
/**
 * Same as SCE_GE_SIGNAL_OTBP0, for a CBP/CBW pair.
 */
#define SCE_GE_SIGNAL_OCBP             0x38
/**
 * If deci2p operations are defined, break here and run the SCE_DECI2OP_GE_BREAK operation
 * until it resumes operation.
 */
#define SCE_GE_SIGNAL_BREAK1           0xF0
/**
 * Same as SCE_GE_SIGNAL_BREAK1, but break only if the breakpoint counter is -1 or equal
 * to the defined value (ie, we reached the breakpoint the number of times specified when
 * the breakpoint was created).
 */
#define SCE_GE_SIGNAL_BREAK2           0xFF

/** Structure storing a stack (for CALL/RET) */
typedef struct
{
    /** The stack buffer */
    unsigned int stack[8];
} SceGeStack;

/** Structure storing a GE context */
typedef struct
{
    /** The context buffer */
    unsigned int ctx[512];
} SceGeContext;

/** Typedef for a GE callback */
typedef void (*SceGeCallback)(int id, void *arg);

/** Structure to hold the callback data */
typedef struct
{
    /** GE callback for the signal interrupt */
    SceGeCallback signal_func;
    /** GE callback argument for signal interrupt */
    void *signal_arg;
    /** GE callback for the finish interrupt */
    SceGeCallback finish_func;
    /** GE callback argument for finish interrupt */
    void *finish_arg;
} SceGeCallbackData;

/** List of arguments when enqueueing a list */
typedef struct
{
    /** Size of the structure (16) */
    u32 size;
    /** Pointer to a context */
    SceGeContext *ctx;
    /** Number of stacks to use */
    u32 numStacks;
    /** Pointer to the stacks (unused) */
    SceGeStack *stacks;
} SceGeListArgs;

/** State of a display list, returned by sceGeListSync() and sceGeDrawSync() */
typedef enum
{
    /** The list has been completed */
    SCE_GE_LIST_COMPLETED,
    /** The list is queued but not executed yet */
    SCE_GE_LIST_QUEUED,
    /** The list is currently being executed */
    SCE_GE_LIST_DRAWING,
    /** The list was stopped because it encountered stall address */
    SCE_GE_LIST_STALLING,
    /** The list is paused because of a signal */
    SCE_GE_LIST_PAUSED
} SceGeListState;

/**
 * Updates the stall address.
 *
 * @param dlId The ID of the display list whose stall address will be modified
 *
 * @return Zero on success, otherwise less than zero.
 */
int sceGeListUpdateStallAddr(int dlId, void *stall);

/**
 * Gets a command (?).
 *
 * @param cmdOff The command ID.
 *
 * @return The command on success, otherwise less than zero.
 */
int sceGeGetCmd(u32 cmdOff);

/**
 * Gets a matrix.
 *
 * @param id The matrix ID (0 - 11)
 * @param mtx A buffer to store the matrix.
 *
 * @return Zero on success, otherwise less than zero.
 */
int sceGeGetMtx(int id, int *mtx);

/**
 * Saves the GE context of the current display list into a structure.
 *
 * @param ctx The structure to save the GE context in.
 *
 * @return Zero on success, otherwise less than zero.
 */
int sceGeSaveContext(SceGeContext *ctx);

/**
 * Restores a context from a structure.
 *
 * @param ctx The structure to load the GE context from.
 *
 * @return Zero on success, otherwise less than zero.
 */
int sceGeRestoreContext(SceGeContext *ctx);

/**
 * Gets the EDRAM address.
 *
 * @return 0x04000000.
 */
int sceGeEdramGetAddr();

/**
 * Sets the EDRAM address translation.
 *
 * @param arg The memory width (0, 0x200, 0x400, 0x800 or 0x1000).
 *
 * @return The previous memory width.
 */
int sceGeEdramSetAddrTranslation(int arg);

/**
 * Gets the EDRAM size, set with sceGeEdramSetSize().
 *
 * @return The EDRAM size.
 */
int sceGeEdramGetSize();

/**
 * Dequeues a list.
 *
 * @param dlId The ID of the display list to dequeue.
 *
 * @return Zero on success, otherwise less than zero.
 */
int sceGeListDeQueue(int dlId);

/**
 * Peeks a list state, or waits for it to be completed.
 *
 * @param dlId The ID of the display list to check.
 * @param mode 0 to wait for the display list to be completed, or 1 to check its current state.
 *
 * @return On success, SCE_GE_LIST_COMPLETED if mode is 0, one of the values of SceGeListState if mode is 1, and otherwise, less than zero.
 */
SceGeListState sceGeListSync(int dlId, int mode);

/**
 * Peeks a drawing state, or waits for the drawing to be completed.
 *
 * @param syncType 0 to wait for the drawing to be completed, or to check its current state.
 *
 * @return On success, SCE_GE_LIST_COMPLETED if mode is 0, SCE_GE_LIST_STALLING, SCE_GE_LIST_DRAWING or SCE_GE_LIST_COMPLETED if mode is 1, and otherwise, less than zero.
 */
SceGeListState sceGeDrawSync(int syncType);

/**
 * Stop the GE drawing.
 *
 * @param resetQueue If not set to 0, the display list queue will be emptied.
 * @param arg1 Unused pointer.
 *
 * @return The stopped display list ID on success, otherwise less than zero.
 */
int sceGeBreak(u32 resetQueue, void *arg1);

/**
 * Continues the queue execution after a sceGeBreak().
 *
 * @return Zero on success, otherwise less than zero.
 */
int sceGeContinue();

/**
 * Sets GE finish/signal callbacks.
 *
 * @param cb The GE callbacks parameters.
 *
 * @return The callbacks id on success, otherwise less than zero.
 */
int sceGeSetCallback(SceGeCallbackData *cb);

/**
 * Gets a stack from the current display list, using its ID.
 *
 * @param stackId The stack ID.
 * @param stack A pointer where the stack will be stored.
 *
 * @return The current stack of the display list on success, otherwise less than zero.
 */
int sceGeGetStack(int stackId, SceGeStack *stack);

/**
 * Enqueues a display list at the end of the queue.
 *
 * @param list A pointer to the list of commands.
 * @param stall The address where the display list will stall.
 * @param cbid The callback ID, returned by sceGeSetCallback(), of the callbacks to be used.
 * @param arg A structure storing arguments about the list.
 *
 * @return The display list ID on success, otherwise less than zero.
 */
int sceGeListEnQueue(void *list, void *stall, int cbid, SceGeListArgs *arg);

/**
 * Enqueues a display list as the next display list that will be executed.
 *
 * @param list A pointer to the list of commands.
 * @param stall The address where the display list will stall.
 * @param cbid The callback ID, returned by sceGeSetCallback(), of the callbacks to be used.
 * @param arg A structure storing arguments about the list.
 *
 * @return The display list ID on success, otherwise less than zero.
 */
int sceGeListEnQueueHead(void *list, void *stall, int cbid, SceGeListArgs *arg);

/**
 * Unsets GE callbacks.
 *
 * @param cbId The ID of the callbacks to unset.
 *
 * @return Zero on success, otherwise less than zero.
 */
int sceGeUnsetCallback(int cbId);

/** @} */

