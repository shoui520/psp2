#include "../../../../naac_internal.h"

int32_t __f98aacInitChannelMappingFromConfiguration(NaacChannelMap *map, int32_t numChannels)
{
	if (map == NULL)
		return NAAC_ERR_INVALID_PARAM;

	sceClibMemset(map, 0, sizeof(NaacChannelMap));
	map->numChannels = numChannels;

	switch (numChannels) {
	case 1:
		map->numElements = 1;
		map->numFrontElements = 1;
		map->elements[0].elementType = NAAC_ELEM_SCE;
		map->elements[0].instanceTag = 0;
		map->elements[0].firstChannel = 0;
		map->elements[0].secondChannel = -1;
		break;
	case 2:
		map->numElements = 1;
		map->numFrontElements = 1;
		map->elements[0].elementType = NAAC_ELEM_CPE;
		map->elements[0].instanceTag = 0;
		map->elements[0].firstChannel = 0;
		map->elements[0].secondChannel = 1;
		break;
	case 3:
		map->numElements = 2;
		map->numFrontElements = 2;
		map->elements[0].elementType = NAAC_ELEM_SCE;
		map->elements[0].instanceTag = 0;
		map->elements[0].firstChannel = 0;
		map->elements[0].secondChannel = -1;
		map->elements[1].elementType = NAAC_ELEM_CPE;
		map->elements[1].instanceTag = 0;
		map->elements[1].firstChannel = 1;
		map->elements[1].secondChannel = 2;
		break;
	case 4:
		map->numElements = 3;
		map->numFrontElements = 2;
		map->numBackElements = 1;
		map->elements[0].elementType = NAAC_ELEM_SCE;
		map->elements[0].instanceTag = 0;
		map->elements[0].firstChannel = 0;
		map->elements[0].secondChannel = -1;
		map->elements[1].elementType = NAAC_ELEM_CPE;
		map->elements[1].instanceTag = 0;
		map->elements[1].firstChannel = 1;
		map->elements[1].secondChannel = 2;
		map->elements[2].elementType = NAAC_ELEM_SCE;
		map->elements[2].instanceTag = 1;
		map->elements[2].firstChannel = 3;
		map->elements[2].secondChannel = -1;
		break;
	case 5:
		map->numElements = 3;
		map->numFrontElements = 2;
		map->numBackElements = 1;
		map->elements[0].elementType = NAAC_ELEM_SCE;
		map->elements[0].instanceTag = 0;
		map->elements[0].firstChannel = 0;
		map->elements[0].secondChannel = -1;
		map->elements[1].elementType = NAAC_ELEM_CPE;
		map->elements[1].instanceTag = 0;
		map->elements[1].firstChannel = 1;
		map->elements[1].secondChannel = 2;
		map->elements[2].elementType = NAAC_ELEM_CPE;
		map->elements[2].instanceTag = 1;
		map->elements[2].firstChannel = 3;
		map->elements[2].secondChannel = 4;
		break;
	case 6:
		map->numElements = 4;
		map->numFrontElements = 2;
		map->numBackElements = 1;
		map->numLfeElements = 1;
		map->hasLfe = 1;
		map->elements[0].elementType = NAAC_ELEM_SCE;
		map->elements[0].instanceTag = 0;
		map->elements[0].firstChannel = 0;
		map->elements[0].secondChannel = -1;
		map->elements[1].elementType = NAAC_ELEM_CPE;
		map->elements[1].instanceTag = 0;
		map->elements[1].firstChannel = 1;
		map->elements[1].secondChannel = 2;
		map->elements[2].elementType = NAAC_ELEM_CPE;
		map->elements[2].instanceTag = 1;
		map->elements[2].firstChannel = 3;
		map->elements[2].secondChannel = 4;
		map->elements[3].elementType = NAAC_ELEM_LFE;
		map->elements[3].instanceTag = 0;
		map->elements[3].firstChannel = 5;
		map->elements[3].secondChannel = -1;
		break;
	case 8:
		map->numElements = 5;
		map->numFrontElements = 3;
		map->numBackElements = 1;
		map->numLfeElements = 1;
		map->hasLfe = 1;
		map->elements[0].elementType = NAAC_ELEM_SCE;
		map->elements[0].instanceTag = 0;
		map->elements[0].firstChannel = 0;
		map->elements[0].secondChannel = -1;
		map->elements[1].elementType = NAAC_ELEM_CPE;
		map->elements[1].instanceTag = 0;
		map->elements[1].firstChannel = 1;
		map->elements[1].secondChannel = 2;
		map->elements[2].elementType = NAAC_ELEM_CPE;
		map->elements[2].instanceTag = 1;
		map->elements[2].firstChannel = 3;
		map->elements[2].secondChannel = 4;
		map->elements[3].elementType = NAAC_ELEM_CPE;
		map->elements[3].instanceTag = 2;
		map->elements[3].firstChannel = 5;
		map->elements[3].secondChannel = 6;
		map->elements[4].elementType = NAAC_ELEM_LFE;
		map->elements[4].instanceTag = 0;
		map->elements[4].firstChannel = 7;
		map->elements[4].secondChannel = -1;
		break;
	default:
		return NAAC_ERR_INVALID_PARAM;
	}

	return NAAC_OK;
}
