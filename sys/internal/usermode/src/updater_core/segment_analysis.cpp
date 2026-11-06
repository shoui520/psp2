/*
 * The segment analysis behind sceUpdaterIsSegmentAvailable (the file name is ours): each call
 * gets the next piece of a PUP being downloaded. The first piece holds the header; the
 * segment table's entries are read across pieces until the one of the segment waited for,
 * and the segment is available once the bytes seen reach its end.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <update_mgr/pup_file.h>
#include <updater_core/updater_core.h>
#include "updater_log.h"
#include "segment_analysis.h"

#define UPDATER_FILE_ID		0xf211a21f

SegmentAnalysis::SegmentAnalysis()
{
	SceSize i;

	m_segmentId = 0;
	m_processed = 0;
	m_segmentOffset = 0;
	m_segmentSize = 0;
	m_headerRead = 0;
	m_entryLength = 0;
	m_found = 0;
	m_entryIndex = 0;
	for (i = 0; i < sizeof(m_header); i++)
		((SceUInt8 *)&m_header)[i] = 0;
	for (i = 0; i < sizeof(m_entry); i++)
		((SceUInt8 *)&m_entry)[i] = 0;
}

SegmentAnalysis::~SegmentAnalysis()
{
}

void SegmentAnalysis::Initialize(SceUInt64 segmentId)
{
	m_segmentId = segmentId;
}

/*
 * Reads the segment table's entries in the rest of a piece (*remaining bytes from its end),
 * the first completing the one a piece before ended inside, until the segment's entry: then
 * its place is kept and *found set. A piece that ends inside an entry keeps what it has of it.
 */
int SegmentAnalysis::findSegment(SceUpdaterSegmentData *data, SceUInt64 *remaining, bool *found)
{
	const SceUInt8 *src = (const SceUInt8 *)data->data + (data->size - *remaining);

	*found = false;
	if (m_entryLength == 0) {
		if (*remaining < sizeof(m_entry)) {
			sceClibMemcpy(&m_entry, src, *remaining);
			m_entryLength = *remaining;
			*found = false;
			m_processed += *remaining;
			return 0;
		}
		sceClibMemcpy(&m_entry, src, sizeof(m_entry));
		*remaining -= sizeof(m_entry);
		m_processed += sizeof(m_entry);
		src += sizeof(m_entry);
	} else {
		if (*remaining + m_entryLength < sizeof(m_entry)) {
			sceClibMemcpy((SceUInt8 *)&m_entry + m_entryLength, src, *remaining);
			m_entryLength += *remaining;
			*found = false;
			m_processed += *remaining;
			return 0;
		}
		sceClibMemcpy((SceUInt8 *)&m_entry + m_entryLength, src,
			      sizeof(m_entry) - m_entryLength);
		*remaining = *remaining + m_entryLength - sizeof(m_entry);
		m_processed += sizeof(m_entry) - m_entryLength;
		src += sizeof(m_entry) - m_entryLength;
	}

	for (;;) {
		m_entryIndex++;
		if (m_entry.segment_id == m_segmentId) {
			m_segmentOffset = m_entry.offset;
			m_segmentSize = m_entry.size;
			*found = true;
			return 0;
		}
		if (m_entryIndex >= m_header.m_segment_num) {
			UPDATER_ERROR_FMT(212, "0x%llx", m_segmentId);
			*found = false;
			return SCE_UPDATER_ERROR_SEGMENT_NOT_FOUND;
		}
		if (*remaining < sizeof(m_entry)) {
			sceClibMemcpy(&m_entry, src, *remaining);
			m_entryLength = *remaining;
			*found = false;
			return 0;
		}
		*remaining -= sizeof(m_entry);
		if (*remaining == 0) {
			*found = false;
			m_entryLength = 0;
			return 0;
		}
		sceClibMemcpy(&m_entry, src, sizeof(m_entry));
		m_processed += sizeof(m_entry);
		src += sizeof(m_entry);
	}
}

int SegmentAnalysis::IsSegmentAvailable(SceUpdaterSegmentData *data, int *result)
{
	SceUInt64 remaining = data->size;
	bool found;
	int ret = 0;

	*result = SCE_UPDATER_SEGMENT_NOT_AVAILABLE;
	if (m_headerRead == 0) {
		if (remaining < sizeof(m_header)) {
			UPDATER_ERROR_FMT(84, "%lld", remaining);
			return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
		}
		sceClibMemcpy(&m_header, data->data, sizeof(m_header));
		remaining -= sizeof(m_header);
		m_processed = sizeof(m_header);
		m_headerRead = 1;
		if (data->size <= sizeof(m_header))
			return ret;
	}
	if (m_found == 0) {
		ret = findSegment(data, &remaining, &found);
		if (ret != 0)
			return ret;
		if (!found)
			return ret;
		m_found = 1;
	}
	m_processed += (SceInt32)remaining;
	if (m_segmentOffset + m_segmentSize <= m_processed)
		*result = SCE_UPDATER_SEGMENT_AVAILABLE;
	return ret;
}
