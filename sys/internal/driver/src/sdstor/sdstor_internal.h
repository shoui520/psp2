/* SceSdstor module-private partition and open-handle owners. */
#ifndef SDSTOR_INTERNAL_H
#define SDSTOR_INTERNAL_H

#include <stdint.h>
#include <threadmgr/fast_mutex.h>
#include <sdstor/sdstor.h>
#include <sdstor/mbr.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>

typedef struct SceSdstorSddev SceSdstorSddev;
typedef struct SceSdstorSdpart SceSdstorSdpart;

typedef struct SceSdstorMediaInfo {
	uint32_t blockSize;
	uint32_t sectorCount;
	uint32_t eraseBlockCount;
} SceSdstorMediaInfo;

typedef struct SceSdstorSdinst {
	uint8_t port;
	uint8_t kind;
	uint8_t id;
	uint8_t stat;
} SceSdstorSdinst;

typedef struct SceSdstorNameComponent {
	int count;
	const char *const *names;
} SceSdstorNameComponent;

struct SceSdstorSddev {
	struct ThreadmgrFastMutex fastmutex;
	uint8_t hport;
	uint8_t mediaType;
	uint8_t stat;
	uint8_t writeProtected;
	SceSdstorMediaInfo mediaInfo;
	void *sdif_ctx;
};

typedef struct __attribute__((packed, aligned(2))) SceSdstorPartitionAccess {
	uint32_t flags;
	uint16_t mode;
} SceSdstorPartitionAccess;

typedef struct SceSdstorPartitionEntry {
	uint32_t start_lba;
	uint32_t n_sectors;
	SceSdstorSdpart *sdpart;
	void *partition_entry;
	char numeric_name[4];
	uint32_t flags;
	uint16_t mode;
	uint16_t reserved;
} SceSdstorPartitionEntry;

struct SceSdstorSdpart {
	struct ThreadmgrFastMutex fastmutex;
	uint32_t blockSize;
	SceSdstorSddev *sddev;
	SceSdstorMediaInfo mediaInfo;
	void *mbr_ptr;
	SceSdstorPartitionEntry pentry[17];
	uint8_t stat;
	uint8_t prev_stat;
	uint8_t externalFormat;
	uint8_t reserved;
};

typedef struct SceSdstorFd {
	struct ThreadmgrFastMutex fastmutex;
	uint32_t blockSize;
	uint32_t start_lba;
	uint32_t n_sectors;
	int flags;
	SceSdstorPartitionEntry *pentry;
	SceSdstorSddev *sddev;
} SceSdstorFd;

_Static_assert(sizeof(SceSdstorMediaInfo) == 12, "SceSdstorMediaInfo");
_Static_assert(sizeof(SceSdstorSdinst) == 4, "SceSdstorSdinst");
_Static_assert(sizeof(SceSdstorSddev) == 84, "SceSdstorSddev");
_Static_assert(sizeof(SceSdstorPartitionAccess) == 6, "SceSdstorPartitionAccess");
_Static_assert(sizeof(SceSdstorMbrPartition) == 16, "SceSdstorMbrPartition");
_Static_assert(sizeof(SceSdstorMbr) == 512, "SceSdstorMbr");
_Static_assert(sizeof(SceSdstorPartitionEntry) == 28, "SceSdstorPartitionEntry");
_Static_assert(sizeof(SceSdstorSdpart) == 568, "SceSdstorSdpart");
_Static_assert(sizeof(SceSdstorFd) == 88, "SceSdstorFd");

int sddev_get_port(SceSdstorSddev *device);
SceSdstorSddev *sddev_get_object(unsigned int port);
int sddev_power_on(SceSdstorSddev *device);
int sddev_close(SceSdstorSddev *device);
int sddev_get_info(SceSdstorSddev *device, SceSdstorMediaInfo *info);
int sdpart_decode_name(const char *name, unsigned int length,
		       SceSdstorSdinst *sdinst);
unsigned int sdpart_partition_code_to_id(unsigned int code);
void sdpart_trace_sdinst(const SceSdstorSdinst *sdinst);
int sddev_read(SceSdstorSddev *device, void *buffer, uint32_t sector,
	       uint32_t sectorCount);
int sddev_write(SceSdstorSddev *device, const void *buffer, uint32_t sector,
		uint32_t sectorCount);

/* sdctrl.c */
int sdstorRegisterUsbMassInterrupt(void);
int sdctrl_initialize(void);
int sdctrl_remove(unsigned int port);
int sdctrl_insert(unsigned int port);
int sdctrl_disable_mount(unsigned int port);
int sdctrl_invalidate_partition(unsigned int port);

/* sddev.c */
int sddev_power_off(SceSdstorSddev *device);
int sddev_init(void);
int sddev_remove(SceSdstorSddev *device);
int sddev_insert(SceSdstorSddev *device);
int sddev_suspend(SceSdstorSddev *device);
int sddev_resume(SceSdstorSddev *device);
int sddev_check_removal(SceSdstorSddev *device);
int sddev_has_write_protect_area(SceSdstorSddev *device, uint32_t sector,
				 uint32_t count, uint8_t *writeProtected);
int sddev_format_rm(SceSdstorSddev *device);
int sddev_erase_sd(SceSdstorSddev *device, SceUID callback);
int sddev_is_read_write(SceSdstorSddev *device);

/* sdpart.c */
int sdpart_init(void);
int sdpart_close(unsigned int port);
int sdpart_remove(unsigned int port);
int sdpart_insert(unsigned int port);
int sdpart_suspend(unsigned int port);
int sdpart_resume_stat(unsigned int port);
int sdpart_write_sd_mbr(unsigned int port);
int sdpart_valid_internal_mbr(const SceMbr *mbr);
SceSdstorPartitionEntry *sdpart_get_object(const char *name, int length);
unsigned int sdpart_grab_mc(unsigned int port);
int sdpart_release_mc(unsigned int port);
unsigned int sdpart_get_mc_port(void);
int sdpart_grab_xmc(unsigned int port);
int sdpart_release_xmc(unsigned int port);
unsigned int sdpart_get_xmc_port(void);

/* sdstor.c */
int sdstor_initialize(void);
int sdstor_construct(SceSdstorPartitionEntry *partition, int flags, SceSdstorFd **resultHandle);
int sdstor_destruct(SceSdstorFd *handle);
int sdstor_read(SceSdstorFd *handle, int64_t offset, void *buffer, uint32_t size);
int sdstor_write(SceSdstorFd *handle, int64_t offset, const void *buffer, uint32_t size);

/* sdstor_vfs.c */
int sdstor_vfs_initialize(void);

#endif
