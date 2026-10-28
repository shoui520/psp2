# Sony's file layout (Sony paths from the 1.80/3.20/4.00 debug records; the
# recordless vfs/vops glue and nullrwicv_sector.c are Princess-of-Sleeping's
# estimated names), in the link order of the original.
psp2_module(os0_kd_bootimage_pfsmgr KIND kernel COMPILER gcc48 SKELETON SOURCES
	pfs_api_vfs.c
	pmi.c
	handler/pfs_vops.c
	handler/pfops.c
	handler/pfops_impl.c
	handler/icvdb.c
	handler/ac_vfs.c
	handler/access_control.c
	handler/mode_setting.c
	handler/storage_use.c
	bufmgr/file_access.c
	bufmgr/lru.c
	bufmgr/buffer_list.c
	table/roicv_db.c
	efile/roicv_sector.c
	efile/nullroicv_sector.c
	efile/rwicv_sector.c
	efile/nullrwicv_sector.c
	efile/secure_drive.c
	efile/crypt_packet.c
	efile/crypt_engine.c
	efile/crypt_engine_impl.c
	facade/facade.c
	crypt/xts.c
	crypt/xts_dmac5.c
	crypt/cbc_dmac5.c
	crypt/sha1_icv_dmac5.c
	handler/pfs_util.c
	handler/redirect_vops.c
	handler/redirectops.c
	handler/filesdb.c
	handler/ac.c
	bufmgr/heap.c
	bufmgr/page_acc.c
	bufmgr/buf_alloc.c
	tree/rwicv_db.c
	table/filetable.c
	btree/fsdb.c
	tree/filetree.c
	table/page_subtable.c
	btree/btree.c
	tree/subtree.c
	tree/merkle_tree.c
)
