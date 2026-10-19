#ifndef MARLIN_HCI_PATH_H
#define MARLIN_HCI_PATH_H

typedef struct MarlinStringList {
	struct MarlinStringList *next;
	char *string;
} MarlinStringList;

typedef struct MarlinPathInfo {
	int isFile;
	unsigned int size;
} MarlinPathInfo;

int MarlinAppendPath(unsigned int *size, char *path, const char *component);
int MarlinStringListDestroy(void *allocator, MarlinStringList *list);

/* directory.c */
int MarlinCreateDirectories(void *allocator, const char *path);
int MarlinGetStorageRoot(void *allocator, unsigned int *size, char *path);
signed char MarlinGetPathSeparator(void);

#endif
