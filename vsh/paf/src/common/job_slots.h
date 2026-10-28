/* Indexed ownership of jobs; internal names not recovered from Sony records. */
#ifndef PAF_COMMON_JOB_SLOTS_H
#define PAF_COMMON_JOB_SLOTS_H

#include "../job/job.h"

typedef struct JobItemVector {
	JobItemSharedPtr *_data;
	size_t _size;
	size_t _capacity;
	unsigned char _allocator;
} JobItemVector;

typedef struct JobSlots JobSlots;

typedef void (*JobSlotRemovedFunction)(size_t index, JobItemSharedPtr *item, void *userdata);

struct JobSlots {
	JobItemVector m_items;
	JobSlotRemovedFunction m_on_removed;
	void *m_userdata;
};

typedef struct SlotJobItem {
	JobItem base;
	JobSlots *m_owner;
} SlotJobItem;

typedef char JobItemVectorSizeCheck[sizeof(JobItemVector) == 16 ? 1 : -1];
typedef char JobSlotsSizeCheck[sizeof(JobSlots) == 24 ? 1 : -1];
typedef char SlotJobItemSizeCheck[sizeof(SlotJobItem) == 32 ? 1 : -1];

#define SlotJobItemConstruct ScePafCommon_74C6457F
#define SlotJobItemConstructBase ScePafCommon_79C0CFDA
#define SlotJobItemRun ScePafCommon_8AA97D29
#define JobSlotsClear ScePafCommon_2D234E8A
#define JobSlotsGet ScePafCommon_CB966BC9
#define JobSlotsFind ScePafCommon_B708260B
#define JobSlotsComplete ScePafCommon_CC8F253C
#define SlotJobItemFinish ScePafCommon_CB109FEE
#define JobSlotsGrow ScePafCommon_802ADD92
#define JobSlotsCancel ScePafCommon_E531DCDD
#define JobSlotsEnqueue ScePafCommon_215536F3
#define JobSlotsCancelAll ScePafCommon_59FD41F2
#define JobSlotsContains ScePafCommon_50675CDC
#define JobSlotsFreeIndex ScePafCommon_880AA4A5
#define JobSlotsFindShared ScePafCommon_203EF891
#define JobSlotsConstruct ScePafCommon_3BCCE98B
#define JobSlotsConstructBase ScePafCommon_8199CAE6
#define JobSlotsDestroy ScePafCommon_F5E73158
#define JobSlotsDestroyBase ScePafCommon_BB73B79C

SlotJobItem *SlotJobItemConstruct(SlotJobItem *self, const char *name, const JobItemOption *option);
SlotJobItem *SlotJobItemConstructBase(SlotJobItem *self, const char *name, const JobItemOption *option);
int SlotJobItemRun(JobItem *base);
JobSlots *JobSlotsClear(JobSlots *self, size_t index);
uintptr_t JobSlotsGet(JobItemSharedPtr *result, JobSlots *self, size_t index);
int JobSlotsFind(JobSlots *self, JobItem *job);
void JobSlotsComplete(JobSlots *self, SlotJobItem *job);
void SlotJobItemFinish(JobItem *base, int result);
void JobSlotsGrow(JobSlots *self, size_t size);
JobSlots *JobSlotsCancel(JobSlots *self, size_t index);
int JobSlotsEnqueue(JobSlots *self, size_t index, JobQueue *queue, JobItemSharedPtr *item);
void JobSlotsCancelAll(JobSlots *self);
int JobSlotsContains(JobSlots *self, size_t index);
size_t JobSlotsFreeIndex(JobSlots *self);
int JobSlotsFindShared(JobSlots *self, const JobItemSharedPtr *item);
JobSlots *JobSlotsConstruct(JobSlots *self);
JobSlots *JobSlotsConstructBase(JobSlots *self);
JobSlots *JobSlotsDestroy(JobSlots *self);
JobSlots *JobSlotsDestroyBase(JobSlots *self);

JobItemVector *JobItemVectorDestroy(JobItemVector *self);
int JobItemVectorReallocate(JobItemVector *self, size_t capacity);
void JobItemVectorResize(JobItemVector *self, size_t size, const JobItemSharedPtr *value);
JobItem *SlotJobItemDestroy(JobItem *base);
void SlotJobItemDelete(JobItem *base);

#endif
