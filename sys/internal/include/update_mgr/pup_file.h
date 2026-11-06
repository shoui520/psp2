/*
 * The PUP file's header and segment table, which update_mgr verifies and the system updater
 * (updater_core) reads. Sony's prototype names the members prefixed m_; the others and the
 * header's name are ours.
 */
#ifndef UPDATE_MGR_PUP_FILE_H
#define UPDATE_MGR_PUP_FILE_H

#include <stdint.h>

/* Complete authenticated file header; uninterpreted descriptions stay serialized. */
typedef struct PupHeader {
	uint8_t m_magic[8];
	uint8_t formatDescription[8];
	uint64_t m_format_version;
	uint64_t m_segment_num;
	uint64_t m_file_offset;
	uint64_t m_file_size;
	uint8_t signingDescription[8];
	uint8_t m_attribute[4];
	uint32_t platform;
	uint32_t formatPolicy;
	uint32_t m_support_list;
	uint32_t m_base_version;
	uint32_t m_base_buildno;
	uint8_t authenticatedExtension[48];
} PupHeader;

/* one entry of the segment table, which follows the header */
typedef struct PupSegment {
	uint64_t segment_id;
	uint64_t offset;
	uint64_t size;
	uint64_t sign_algorithm;
} PupSegment;

#endif
