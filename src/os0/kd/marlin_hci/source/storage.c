#include "storage.h"

MarlinBuffer *marlinStoredData;

int MarlinGetStoredSections(int selector, MarlinBuffer **first,
			    MarlinBuffer **second)
{
	int result = 0;
	MarlinBuffer *firstSection = 0;
	MarlinBuffer *secondSection = 0;

	if (marlinStoredData == 0) {
		result = MarlinLoadStoredData(0, &marlinStoredData);
		if (result != 0) {
			result = result != 1 ? result : 0x23110005;
			goto out;
		}
	}
	result = MarlinExtractStoredSections(marlinStoredData, selector,
					   &firstSection, &secondSection);
	if (result == 0) {
		if (first != 0) {
			*first = firstSection;
			firstSection = 0;
		}
		if (second != 0) {
			*second = secondSection;
			secondSection = 0;
		}
	}
out:
	MarlinBufferDestroy(0, firstSection);
	MarlinBufferDestroy(0, secondSection);
	return result;
}
