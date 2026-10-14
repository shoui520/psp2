#ifndef MARLIN_HCI_HANDLES_H
#define MARLIN_HCI_HANDLES_H

/* One complete entry: every field is initialized, queried and cleared. */
typedef struct MarlinHandleEntry {
	unsigned int id;
	unsigned int type;
	unsigned int size;
	void *object;
} MarlinHandleEntry;

extern MarlinHandleEntry marlinHandles[32];

void MarlinHandlesInitialize(void);
int MarlinHandleCreate(unsigned int type, unsigned int size, void *object,
		       unsigned int *handle);
int MarlinHandleGet(unsigned int handle, unsigned int *type, unsigned int *size,
		    void **object);
int MarlinHandleRelease(unsigned int handle);
void MarlinHandlesTerminate(void);

#endif
