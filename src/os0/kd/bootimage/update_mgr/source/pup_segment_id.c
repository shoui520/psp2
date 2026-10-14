#include "update_internal.h"

int PupUtilVerifyPupSegmentById(SceUID fd, uint64_t seg_id,
		int need_header_verify, int load_sm, int resident_sm)
{
	PupSegment *segment;
	uint64_t index, count;
	int result;

	if (fd < 0)
		goto invalid;
	if (need_header_verify) {
		result = PupUtilVerifyPupHeader(fd, load_sm, 1);
		if (result)
			goto cleanup;
	}
	if (!pupFileBuffer)
		goto invalid;
	count = pupFileHeader.m_segment_num;
	segment = (PupSegment *)(pupFileBuffer + 1);
	index = 0;
	while (index != count) {
		if (segment->segment_id == seg_id)
			goto verify;
		segment++;
		index++;
	}
	result = (int)0x800f0202;
	goto cleanup;
verify:
	if (need_header_verify)
		result = PupUtilVerifyPupSegment(fd, index, need_header_verify, 0, 1);
	else
		result = PupUtilVerifyPupSegment(fd, index, need_header_verify, 1, 1);
cleanup:
	if (resident_sm != 1 && pupSmId != -1) {
		UpdateUtilStopSm(&pupSmId);
		pupSmId = -1;
	}
	return result;
invalid:
	result = (int)0x800f0216;
	goto cleanup;
}
